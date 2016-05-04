/*************************************************************************
	> File Name: rmCAN.h
	> Author: 
	> Mail:  
	> Created Time: 2016年03月10日 星期四 17时03分23秒
 ************************************************************************/

#ifndef _RM_CAN_H
#define _RM_CAN_H

#ifdef __cplusplus
extern "C" {
#endif

struct CANPACKET
{
    uint8_t priority;
    uint8_t src_id;
    uint8_t dst_id;
    void    *p_data;
    uint32_t len;
};

int openCANDevice();
void closeCANDevice();

int sendCANData(uint8_t s_id, uint8_t d_id, void *pdata, int len);
int readCANData(uint8_t exp_s_id, uint8_t exp_msg_type,
                void *pdata, int len, int timeout);


#ifdef __cplusplus
}
#endif

#endif /* _RM_CAN_H */
