/*************************************************************************
	> File Name: robot_upgrade.h
	> Author: 
	> Mail:  
	> Created Time: 2016年03月11日 星期五 14时15分43秒
 ************************************************************************/

#ifndef _ROBOT_UPGRADE_H
#define _ROBOT_UPGRADE_H

#include "robot_common_data.h"

#ifdef __cplusplus
extern "C" {
#endif


struct UPGRADE_MODULE
{
    uint8_t module;
    char    *file;
};

int robotSubSysUpdate(int count, UPGRADE_MODULE *m);


#ifdef __cplusplus
}
#endif



#endif /* _ROBOT_UPGRADE_H */
