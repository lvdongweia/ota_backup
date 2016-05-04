/*************************************************************************
	> File Name: flexcan.h
	> Author: 
	> Mail:  
	> Created Time: 2016年03月10日 星期四 14时58分38秒
 ************************************************************************/

#ifndef _FLEXCAN_H
#define _FLEXCAN_H

#ifdef __cplusplus
extern "C" {
#endif

#define ROBOT_CAN_INTERFACE "can0"

#define PACKET_MAX_NUM          256
#define CAN_DATA_MAX_LEN        8
#define CAN_SUB_PACKET_OFFSET   2   /* 0: id; 1: SN */
#define CAN_SUB_PACKET_SIZE     (CAN_DATA_MAX_LEN - CAN_SUB_PACKET_OFFSET)
#define CAN_DATA_TOTAL_SIZE     (CAN_SUB_PACKET_SIZE * PACKET_MAX_NUM)


/*
 * Note: if packet will send and is incomplete,
 * @len <= CAN_DATA_MAX_LEN - 2
 * data[0] = packet_id;
 * data[1] = packet_no;
 *
 * .sum == 0:
 * packet is complete.
 * .sum > 0:
 * packet is incomplete.
 */
struct flexcan_packet
{
    uint8_t packet_id;              /* uniquely identifies a entry */
    uint8_t priority;
    uint8_t s_id;
    uint8_t d_id;
    uint8_t data[CAN_DATA_MAX_LEN];
    uint32_t len;                   /* real data len <= CAN_DATA_MAX_LEN */
    uint8_t packet_SN;              /* packets number */
    uint8_t sum;                    /* starting from 0 */
};



int flexcan_device_open();
int flexcan_device_close();
int flexcan_send(const struct flexcan_packet *packet);
int flexcan_recv(struct flexcan_packet *packet);


#ifdef __cplusplus
}
#endif

#endif /*_FLEXCAN_H*/
