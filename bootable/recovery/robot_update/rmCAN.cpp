/*************************************************************************
	> File Name: rmCAN.c
	> Author: 
	> Mail:  
	> Created Time: 2016年03月10日 星期四 17时03分15秒
 ************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/time.h>

#include <vector>
#include <map>
#include <list>
#include <algorithm>

#include "flexcan.h"
#include "rmCAN.h"
#include "robot_common_data.h"

#define TAG "RMCAN: "

const uint8_t TimeOutId = 127;

static int sock_fd = -1;
static uint8_t g_PacketsId = 0;
static bool g_flag = true;

static pthread_t g_recvThread = 0;
static pthread_mutex_t list_mutex;

// incomplete packets storge
typedef std::vector<struct flexcan_packet> UDT_VEC_FLEXCAN;          /* multiple packet vector */
typedef std::map<uint8_t, UDT_VEC_FLEXCAN> UDT_MAP_FLEXCAN;          /* <packet_id, vector> */
typedef std::map<uint8_t, UDT_MAP_FLEXCAN> UDT_MAP_INT_FLEXCAN;      /* <src_id, map> */
typedef std::list<struct CANPACKET*>       UDT_LIST_CANPACKET;       /* receive list */

UDT_MAP_INT_FLEXCAN InCompletePackets;
UDT_LIST_CANPACKET  RecvList;

static void debugInfo(CANPACKET *packet)
{
    //printf(TAG "packet address=0x%08x\n", packet);
    //printf(TAG "packet->priority=%d\n", packet->priority);
    //printf(TAG "packet->src_id=0x%02x\n", packet->src_id);
    //printf(TAG "packet->dst_id=0x%02x\n", packet->dst_id);
    printf(TAG "packet->type=0x%02x\n", ((uint8_t *)packet->p_data)[0]);
    printf(TAG "packet->len=%d\n", packet->len);
}


static CANPACKET *_getCanPacket(int priority, int src_id, int dst_id, const void *pdata, int len)
{
    CANPACKET *packet = NULL;

    void *ptr = malloc(len);
    if (!ptr)
    {
        printf(TAG "Malloc request failed!(%s)\n", strerror(errno));
        return NULL;
    }

    memcpy(ptr, pdata, len);
    
    packet = new CANPACKET;
    packet->priority = priority;
    packet->src_id   = src_id;
    packet->dst_id   = dst_id;
    packet->p_data   = ptr;
    packet->len      = len;

    //printf(TAG "Origin data:\n");
    //debugInfo(packet);

    return packet;
}

static int PacketCompare(const struct flexcan_packet &lpacket,
                        const struct flexcan_packet &rpacket)
{
    return (lpacket.packet_SN < rpacket.packet_SN);
}


static CANPACKET *getCanPacket(const struct flexcan_packet *can_data)
{
    CANPACKET *packet =  _getCanPacket(
                            can_data->priority, 
                            can_data->s_id,
                            can_data->d_id, 
                            can_data->data,
                            can_data->len);
    
    return packet;
}


static CANPACKET *getCanPacket(UDT_VEC_FLEXCAN packets)
{
    std::sort(packets.begin(), packets.end(), PacketCompare);

    uint8_t data[CAN_DATA_TOTAL_SIZE];
    uint32_t len = 0;
    memset(data, 0, CAN_DATA_TOTAL_SIZE);

    UDT_VEC_FLEXCAN::iterator iter; 
    for (iter = packets.begin(); iter != packets.end(); iter++)
    {
        memcpy(data + len, (*iter).data, (*iter).len);
        len += (*iter).len;
    }

    CANPACKET *packet = _getCanPacket(
                            packets.at(0).priority,
                            packets.at(0).s_id,
                            packets.at(0).d_id,
                            data, len);

    return packet;
}

static bool isRecvListEmpty()
{
    bool isEmpty;

    pthread_mutex_lock(&list_mutex);
    isEmpty = RecvList.empty();
    pthread_mutex_unlock(&list_mutex);

    return isEmpty;
}

static void pushToRecvList(CANPACKET *packet)
{
    pthread_mutex_lock(&list_mutex);
    RecvList.push_back(packet);
    pthread_mutex_unlock(&list_mutex);
}

static void popFromRecvList(CANPACKET **packet)
{
    CANPACKET *packet_local = NULL;
    UDT_LIST_CANPACKET::iterator i;

    pthread_mutex_lock(&list_mutex);
    if (!RecvList.empty())
    {
        i = RecvList.begin();
        packet_local = *i;

        RecvList.erase(i);
    }
    pthread_mutex_unlock(&list_mutex);
    
    *packet = packet_local;
}

static int _sendToCan(const struct flexcan_packet *packet)
{
    int ret = flexcan_send(packet);
    while (ret < 0 && errno == ENOBUFS)
    {
        usleep(3 * 1000);  // sleep 3ms, again send
        ret = flexcan_send(packet);
    }
    return ret;
}

static int _recvFromCan(struct flexcan_packet *packet)
{
    int ret = flexcan_recv(packet);
    if (ret != 0)
    {
        printf(TAG "Recv flexcan packet error.\n");
        return -1;
    }

    return 0;
}

static int sendToCan(const struct CANPACKET *packet)
{
    if (packet == NULL)
        return -1;

    int ret = -1;
    struct flexcan_packet can_data;
    memset(&can_data, 0, sizeof(struct flexcan_packet));

    /* common data */
    can_data.priority = packet->priority;
    can_data.s_id     = packet->src_id;
    can_data.d_id     = packet->dst_id;

    if (packet->len <= CAN_DATA_MAX_LEN)
    {
        can_data.len = packet->len;
        memcpy(can_data.data, packet->p_data, packet->len);
        ret = _sendToCan(&can_data);

        /*
         * memset:
         * can_data.packet_id = 0;
         * can_data.packet_SN = 0;
         * can_data.sum       = 0;
         */
    }
    else
    {
        can_data.packet_id = g_PacketsId;
        g_PacketsId++;

        uint8_t num = (packet->len % CAN_SUB_PACKET_SIZE == 0)
            ? (packet->len / CAN_SUB_PACKET_SIZE)
            : (packet->len / CAN_SUB_PACKET_SIZE + 1);
        can_data.sum = num -1;  /* 0 ~ num-1 */

        uint8_t *p = (uint8_t *)(packet->p_data);
        can_data.len = CAN_SUB_PACKET_SIZE;

        /* 0 ~ num-2 */
        int i;
        for (i = 0; i < num - 1; i++)
        {
            can_data.packet_SN = i;
            memcpy(can_data.data, p, CAN_SUB_PACKET_SIZE);

            ret = _sendToCan(&can_data);
            if (ret != 0) goto send_err;
            p += CAN_SUB_PACKET_SIZE;
            memset(can_data.data, 0 , CAN_SUB_PACKET_SIZE);
        }

        /* send lastest sub-packet */
        uint32_t left = packet->len - (num - 1) * CAN_SUB_PACKET_SIZE;
        can_data.len = left;
        can_data.packet_SN = num - 1;
        memcpy(can_data.data, p, left);

        ret = _sendToCan(&can_data);
    }


send_err:
    
    return ret;
}

static void removePacketId(UDT_MAP_FLEXCAN &packets, UDT_MAP_FLEXCAN::iterator itt)
{
    itt->second.clear();
    packets.erase(itt);
}

static int recvFromCan()
{
    struct flexcan_packet can_data;
    memset(&can_data, 0, sizeof(flexcan_packet));

    int ret = _recvFromCan(&can_data);
    if (0 != ret)
        return -1;

    // data to upgrade module or sysctrl module
    if (can_data.d_id != RM_SYSCTRL_ID 
        && can_data.d_id != RM_UPGRADE_ID)
        return -1;

    CANPACKET *packet = NULL;
    /* incomplete packet */
    if (can_data.sum != 0)
    {
        /* need to construct a packet */
        /* <src_id, keyVector> */
        UDT_MAP_INT_FLEXCAN::iterator it = InCompletePackets.find(can_data.s_id);
        if (it != InCompletePackets.end())
        {
            /* <packet_id, vector> */
            UDT_MAP_FLEXCAN::iterator itt= it->second.find(can_data.packet_id);
            if (itt != it->second.end())
            {
                itt->second.push_back(can_data);
                size_t num = itt->second.size();

                if ((can_data.sum + 1) <= (uint8_t)num)
                {
                    packet = getCanPacket(itt->second);
                    removePacketId(it->second, itt);
                    goto complete;
                }
            }
            else
            {
                /* delete old packet_id(timeout id)
                 * Note :Not a goo solution, May lose data
                 */
                uint8_t old_packet_id = can_data.packet_id +TimeOutId;
                UDT_MAP_FLEXCAN::iterator itt_old = it->second.find(old_packet_id);
                if (itt_old != it->second.end())
                    removePacketId(it->second, itt_old);

                /* add new packet_id */
                UDT_VEC_FLEXCAN packets;
                packets.push_back(can_data);

                /* map<packet_id, vector> */
                it->second.insert(UDT_MAP_FLEXCAN::value_type(can_data.packet_id, packets));
            }
        }
        else
        {
            /* vector<> */
            UDT_VEC_FLEXCAN packets;
            packets.push_back(can_data);

            /* map<packet_id, vector> */
            UDT_MAP_FLEXCAN packets_id;
            packets_id.insert(UDT_MAP_FLEXCAN::value_type(can_data.packet_id, packets));

            /* map<src_id, map<>> */
            InCompletePackets.insert(
                    UDT_MAP_INT_FLEXCAN::value_type(can_data.s_id, packets_id));
        }

        return 0;
    }

    /* complete packet */
    packet = getCanPacket(&can_data); 

complete:
    if (packet != NULL)
    {
        pushToRecvList(packet);
    }

    return 0;
}

/* Thread to receive can data */
static void *thread_recv(void *arg)
{
    int ret;
    fd_set rfd;

    g_flag = true;
    while (g_flag)
    {
        struct timeval timeout = {0, 10 * 1000};
        FD_ZERO(&rfd);

        FD_SET(sock_fd, &rfd);
        ret = select(sock_fd + 1, &rfd, NULL, NULL, &timeout);
        if (ret == 0)
            continue;
        else
        {
            if (ret > 0)
            {
                if (FD_ISSET(sock_fd, &rfd))
                {
                    recvFromCan();
                }
                else
                {
                    printf(TAG "Select success but no read fd available.\n");
                }
            }
            else
            {
                printf(TAG "Socket select error.(%s)\n", strerror(errno));
            }
        }
    }

    pthread_exit(0);
    return (void*)NULL;
}


int sendCANData(uint8_t s_id, uint8_t d_id, void *pdata, int len)
{
    if (sock_fd == -1)
    {
        printf(TAG "(Send)Socket can is closed.\n");
        return -1;
    }

    int count = 10;
    int ret = -1;
    struct CANPACKET packet;
    memset(&packet, 0, sizeof(struct CANPACKET));

    packet.priority = 3;
    packet.src_id   = s_id;
    packet.dst_id   = d_id;
    packet.p_data   = pdata;
    packet.len      = len;
    
    while (count-- > 0)
    {
        fd_set wfd;
        struct timeval timeout = {0, 10 * 1000};
        FD_ZERO(&wfd);

        FD_SET(sock_fd, &wfd);
        ret = select(sock_fd + 1, NULL, &wfd, NULL, &timeout);
        if (ret == 0) 
            continue;
        else
        {
            if(ret > 0)
            {
                if (FD_ISSET(sock_fd, &wfd))
                {
                    // printf(TAG "Send can data:\n");
                    // debugInfo(&packet);
                    ret = sendToCan(&packet);
                }
                else
                {
                    printf(TAG "Select success but not write fd available.\n");
                    ret = -1;
                }
            }
            else
            {
                printf(TAG "Socket select error.(%s)\n", strerror(errno));
                ret = -1;
            }
            break;
        }
    }

    return ret;
}

int readCANData(uint8_t exp_s_id, uint8_t exp_msg_type, void *pdata, int len, int timeout)
{
    if (sock_fd == -1)
    {
        printf(TAG "(Recv)Socket can is closed.\n");
        return -1;
    }

    if (timeout < 0)
    {
        printf(TAG "Timeout must >=0\n");
        return -1;
    }
  
    int ret = -1;
    time_t start, end;
    bool isWaiting = true;
    start = time(NULL);

    do
    {
        CANPACKET *packet = NULL;
        if (!isRecvListEmpty())
        {
            popFromRecvList(&packet);
            //printf("Recv can data:\n");
            //debugInfo(packet); 

            if (packet)
            {
                if (packet->src_id == exp_s_id
                    && ((uint8_t*)(packet->p_data))[0] == exp_msg_type)
                {
                    if (pdata != NULL)
                        memcpy(pdata, packet->p_data, (len < packet->len ? len : packet->len));
    
                    ret = packet->len;

                    free(packet->p_data);
                    delete packet;
                    break; // receive data wanted and jump out
                }

                // delete garbage data
                free(packet->p_data);
                delete packet;
            }
        }
        usleep(3 * 1000); // wait 3ms
       
        if (timeout > 0)
        {
            end = time(NULL);
            if (timeout >= (int)difftime(end, start))
                isWaiting = true;
            else
            {
                isWaiting = false;
                printf(TAG "Recv data timeiout!\n");
            }
        }
    }while (isWaiting);
    
    return ret;
}

int openCANDevice()
{
    sock_fd = flexcan_device_open();
    if (sock_fd == -1)
        return -1;

    // create can receive thread
    int ret = pthread_create(&g_recvThread, NULL, thread_recv, NULL);
    if (0 != ret)
    {
        printf(TAG "Create receive thread failed.(%s)", strerror(errno));
        return -1;
    }

    InCompletePackets.clear();

    // init mutex
    pthread_mutex_init(&list_mutex, NULL);

    return 0;
}

void closeCANDevice()
{
    sock_fd = -1;
    flexcan_device_close();

    // wait receive thread exit
    g_flag = false;
    pthread_join(g_recvThread, NULL);
    pthread_mutex_destroy(&list_mutex);
}

