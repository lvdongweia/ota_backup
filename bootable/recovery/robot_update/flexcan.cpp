/*************************************************************************
	> File Name: flexcan.c
	> Author: 
	> Mail:  
	> Created Time: 2016年03月10日 星期四 14时58分44秒
 ************************************************************************/

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <net/if.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/can.h>

#include "flexcan.h"

#define TAG "FLEX_CAN: "


#define CAN_D_SMOD_OFFSET       0
#define CAN_S_SMOD_OFFSET       4
#define CAN_SUM_OFFSET          8
#define CAN_D_SYS_OFFSET        14
#define CAN_S_SYS_OFFSET        18
#define CAN_PRIORITY_OFFSET     26

/* send */
#define CAN_DATA2ID_PR_MASK     0x7  /* priority   */
#define CAN_DATA2ID_SYS_MASK    0xF0 /* system id  */
#define CAN_DATA2ID_SMOD_MASK   0X0F /* module id  */
#define CAN_DATA2ID_SUM_MASK    0xFF /* packet sum */


#define CAN_DATA2ID_PRIORITY(p) ((uint32_t)((p) & CAN_DATA2ID_PR_MASK) << CAN_PRIORITY_OFFSET)
#define CAN_DATA2ID_S_SYS(s)    ((uint32_t)((s) & CAN_DATA2ID_SYS_MASK) << CAN_S_SYS_OFFSET)
#define CAN_DATA2ID_D_SYS(d)    ((uint32_t)((d) & CAN_DATA2ID_SYS_MASK) << CAN_D_SYS_OFFSET)
#define CAN_DATA2ID_S_SMOD(s)   ((uint32_t)((s) & CAN_DATA2ID_SMOD_MASK) << CAN_S_SMOD_OFFSET)
#define CAN_DATA2ID_D_SMOD(d)   ((uint32_t)((d) & CAN_DATA2ID_SMOD_MASK))
#define CAN_DATA2ID_SUM(sum)    ((uint32_t)((sum) & CAN_DATA2ID_SUM_MASK) << CAN_SUM_OFFSET)


/* receive */
#define CAN_ID2DATA_PR_MASK     0x1C000000
#define CAN_ID2DATA_S_SYS_MASK  0x03C00000
#define CAN_ID2DATA_D_SYS_MASK  0x003C0000
#define CAN_ID2DATA_S_SMOD_MASK 0x000000F0
#define CAN_ID2DATA_D_SMOD_MASK 0x0000000F
#define CAN_ID2DATA_SUM_MASK    0x0000FF00

#define CAN_ID2DATA_PRIORITY(p) (uint8_t)(((p) & CAN_ID2DATA_PR_MASK) >> CAN_PRIORITY_OFFSET)
#define CAN_ID2DATA_S_SYS(s)    (uint8_t)(((s) & CAN_ID2DATA_S_SYS_MASK) >> CAN_S_SYS_OFFSET)
#define CAN_ID2DATA_D_SYS(d)    (uint8_t)(((d) & CAN_ID2DATA_D_SYS_MASK) >> CAN_D_SYS_OFFSET)
#define CAN_ID2DATA_S_SMOD(s)   (uint8_t)(((s) & CAN_ID2DATA_S_SMOD_MASK) >> CAN_S_SMOD_OFFSET)
#define CAN_ID2DATA_D_SMOD(d)   (uint8_t)(((d) & CAN_ID2DATA_D_SMOD_MASK))
#define CAN_ID2DATA_SUM(m)      (uint8_t)(((m) & CAN_ID2DATA_SUM_MASK) >> CAN_SUM_OFFSET)
#define CAN_ID2DATA_S_MODULE(s) (CAN_ID2DATA_S_SYS(s) | CAN_ID2DATA_S_SMOD(s))
#define CAN_ID2DATA_D_MODULE(d) (CAN_ID2DATA_D_SYS(d) | CAN_ID2DATA_D_SMOD(d))


/* can socket fd */
static int sockfd = -1;

int flexcan_device_open()
{
    struct ifreq ifr;
    struct sockaddr_can addr;

    sockfd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (sockfd < 0) 
    {
        printf(TAG "Can socket open error.(%s)\n", strerror(errno));
        return -1;
    }

    strcpy(ifr.ifr_name, ROBOT_CAN_INTERFACE);
    if (ioctl(sockfd, SIOCGIFINDEX, &ifr))
    {
        printf(TAG "Can ioctl SIOCGIFINDEX error.(%s)\n", strerror(errno));
        close(sockfd);
        return -1;
    }

    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        printf(TAG "Can bind error.(%s)\n", strerror(errno));
        close(sockfd);
        return -1;
    }

    return sockfd;
}

int flexcan_device_close()
{
    if (sockfd > 0)
    {
        close(sockfd);
        sockfd = -1;
    }

    return 0;
}

static int __flexcan_send(struct can_frame *frame)
{
    /* use extended frame */
    frame->can_id &= CAN_EFF_MASK;
    frame->can_id |= CAN_EFF_FLAG;

    int ret = write(sockfd, frame, sizeof(struct can_frame));
    if (ret == -1)
    {
        printf(TAG "Write error.(%s)\n", strerror(errno));
        return -1;
    }

    return 0;
}

static int __flexcan_recv(struct can_frame *frame)
{
    int ret = read(sockfd, frame, sizeof(struct can_frame));
    if (ret < 0)
    {
        printf(TAG "Read error.(%s)\n", strerror(errno));
        return -1;
    }
    else if (ret != sizeof(struct can_frame))
    {
        printf(TAG "Read incomplete can frame.\n");
        return -1;
    }
    else if (frame->can_id & CAN_ERR_FLAG)
    {
        printf(TAG "Error can frame.\n");
        return -1;
    }

    return 0;
}


int flexcan_send(const struct flexcan_packet *packet)
{
    if (!packet || packet->len <= 0 || packet->len > CAN_DATA_MAX_LEN)
    {
        printf(TAG "Packet invalid!(addr:%p, len:%d)\n", packet, (!packet ? 0 : packet->len));
        return -1;
    }

    int ret = -1;
    struct can_frame frame;
    memset(&frame, 0, sizeof(struct can_frame));

    frame.can_id = CAN_DATA2ID_PRIORITY(packet->priority)
        | CAN_DATA2ID_S_SYS(packet->s_id)
        | CAN_DATA2ID_D_SYS(packet->d_id)
        | CAN_DATA2ID_S_SMOD(packet->s_id)
        | CAN_DATA2ID_D_SMOD(packet->d_id);

    /* complete packet */
    if (packet->sum == 0)
    {
        frame.can_dlc = packet->len;
        memcpy(frame.data, packet->data, packet->len);
        ret = __flexcan_send(&frame);
    }
    else
    {
        /* incomplete packet */
        if (packet->len > CAN_SUB_PACKET_SIZE)
        {
            printf(TAG "Packet is incomplete, but data length > %d", CAN_DATA_MAX_LEN - 2);
            return -1;
        }

        /* starting from 0(sum + 1) */
        frame.can_id |= CAN_DATA2ID_SUM(packet->sum);

        memset(&frame.data, 0, sizeof(frame.data));
        frame.data[0] = packet->packet_id;
        frame.data[1] = packet->packet_SN;
        memcpy(frame.data + CAN_SUB_PACKET_OFFSET, packet->data, packet->len);
        frame.can_dlc = packet->len + CAN_SUB_PACKET_OFFSET;
        
        ret = __flexcan_send(&frame);
    }

    return ret;
}

int flexcan_recv(struct flexcan_packet *packet)
{
    struct can_frame frame;
    memset(&frame, 0, sizeof(struct can_frame));

    int ret = __flexcan_recv(&frame);
    if (ret != 0)
        return -1;

    /* get frame real id */
    if (!(frame.can_id & CAN_EFF_FLAG))
    {
        printf(TAG "CAN frame is not extended frame, can_id 0x%08x", frame.can_id);
        return -1;
    }

    uint32_t extID = frame.can_id & CAN_EFF_MASK;
    /* parse framd id */
    packet->priority = CAN_ID2DATA_PRIORITY(extID);  /* priority */
    packet->s_id     = CAN_ID2DATA_S_MODULE(extID);  /* src module id */
    packet->d_id     = CAN_ID2DATA_D_MODULE(extID);  /* dst module id */

    uint32_t sum = CAN_ID2DATA_SUM(extID);
    packet->sum = sum;

    /* just one can packet */
    if (0 == sum)
    {
        memcpy(packet->data, frame.data, frame.can_dlc);

        packet->len = frame.can_dlc;
        packet->packet_id = 0;
        packet->packet_SN = 0;
        return 0;
    }
    
    /* more packets, incomplete packet */
    if (frame.can_dlc < (CAN_SUB_PACKET_OFFSET + 1))
    {
        printf("CAN frame sub-packet length error: %d", frame.can_dlc);
        return -1;
    }

    memcpy(packet->data, frame.data + CAN_SUB_PACKET_OFFSET,
            frame.can_dlc - CAN_SUB_PACKET_OFFSET);

    packet->len = frame.can_dlc - CAN_SUB_PACKET_OFFSET;
    packet->packet_id = frame.data[0];
    packet->packet_SN = frame.data[1];

    return 0;
}

