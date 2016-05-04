/*************************************************************************
	> File Name: robot_upgrade.c
	> Author: 
	> Mail:  
	> Created Time: 2016年03月11日 星期五 14时14分40秒
 ************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "ui.h"
#include "screen_ui.h"
#include "roots.h"

#include "robot_update_message.h"

#include "robot_update/rmCAN.h"
#include "robot_update/crc16.h"
#include "robot_update/Image.h"
#include "robotUpdate.h"

using namespace android;

#define TAG "ROBOT_UPDATE: "

#define DATA_PACKET_LENGTH 500
#define CMD_TIMEOUT        1 
#define DATA_TIMEOUT       30
#define RESEND_COUNT       2 

extern RecoveryUI* ui;

static uint8_t g_curModule     = -1;
static uint32_t g_DataTotal    = 0;
static uint32_t g_DataSended   = 0;

static const int COMMON_SYSCTRL_SUBSYS_STATE_QUERY     = 24; //0x18
static const int COMMON_SYSCTRL_SUBSYS_STATE_QUERY_REP = 25; //0x19 
static const int COMMON_SYSCTRL_UPGRADE_CMD          = 32; //0x20
static const int COMMON_SYSCTRL_UPGRADE_CMD_REP      = 33; //0x21
static const int COMMON_SYSCTRL_EXIT_UPGRADE_CMD     = 34; //0x22
static const int COMMON_SYSCTRL_EXIT_UPGRADE_CMD_REP = 35; //0x23


static const struct ModuleDescription_{
    uint8_t module;
    char    name[32];
    int     update_time;
} ModuleDesp[] = 
{
    {RC_UPGRADE_ID,      "rc",   100},
    {RB_L_ARM_UPGRADE_ID,"rb-l", 60},
    {RB_R_ARM_UPGRADE_ID,"rb-r", 60},
    {RB_BODY_UPGRADE_ID, "rb-b", 60},
    {RF_UPGRADE_ID,      "rf",   10},
    {RP_UPGRADE_ID,      "rp",   60},
};

static const char *getModuleName(uint8_t submodule)
{
    for (uint8_t i = 0; i < sizeof(ModuleDesp)/sizeof(struct ModuleDescription_); i++)
    {
        if (submodule == ModuleDesp[i].module)
            return ModuleDesp[i].name;
    }
    return NULL;
}

static int getUpdateTime(uint8_t submodule)
{
    for (uint8_t i = 0; i < sizeof(ModuleDesp)/sizeof(struct ModuleDescription_); i++)
    {
        if (submodule == ModuleDesp[i].module)
            return ModuleDesp[i].update_time;
    }

    return 0;
}

static void downloadProgress(uint32_t cur, uint32_t tol)
{
    int prog = (cur * 100) / tol;
    printf("\rDownloading file ...(%d%%, %d/%d)", prog, (int)cur, (int)tol);

    fflush(stdout);
}

static int sendStartFlag()
{
    int ret = -1;
    uint8_t len = 0;
    uint8_t data[2] = {0};
    uint8_t ret_data[3] = {0};
    int count = RESEND_COUNT;

    data[0] = ROBOT_UPGRADE_START_CMD;
    data[1] = len = 2;

    while (count-- > 0)
    {
        ret = sendCANData(RM_UPGRADE_ID, g_curModule, data, len);
        if (0 != ret)
            continue;

        memset(ret_data, 0, sizeof(ret_data));
        ret = readCANData(g_curModule, ROBOT_UPGRADE_START_RESP,
                ret_data, sizeof(ret_data), CMD_TIMEOUT);
        if (0 < ret)
        {
            // deal response
            ret = 0;
            break;
        }
    }

    return ret;
}

static int sendDataAddress(const uint32_t addr)
{
    int ret = -1;
    uint8_t len = 0;
    uint8_t data[6] = {0};
    uint8_t ret_data[3] = {0};
    int count = RESEND_COUNT;

    data[0] = ROBOT_UPGRADE_ADDR_CMD;
    data[1] = len = 6;
    data[2] = addr >> 24;
    data[3] = (addr >> 16) & 0xFF;
    data[4] = (addr >> 8) & 0xFF;
    data[5] = addr & 0xFF;

    while (count-- > 0)
    {
        ret = sendCANData(RM_UPGRADE_ID, g_curModule, data, len);
        if (0 != ret)
            continue;

        memset(ret_data, 0, sizeof(ret_data));
        ret = readCANData(g_curModule, ROBOT_UPGRADE_ADDR_RESP, 
                ret_data, sizeof(ret_data), CMD_TIMEOUT);
        if (0 < ret)
        {
            // deal response
            ret = 0;
            break;
        }
    }

    return ret;
}

static int sendDataCRC(const uint16_t crc)
{
    int ret = -1;
    int len;
    uint8_t data[4] = {0};
    uint8_t ret_data[3] = {0};
    int count = RESEND_COUNT;

    data[0] = ROBOT_UPGRADE_CRC_CMD;
    data[1] = len = 4;
    data[2] = crc >> 8;
    data[3] = crc & 0xFF;

    while (count-- > 0)
    {
        ret = sendCANData(RM_UPGRADE_ID, g_curModule, data, len);
        if (0 != ret)
            continue;

        memset(ret_data, 0, sizeof(ret_data));
        ret = readCANData(g_curModule, ROBOT_UPGRADE_CRC_RESP,
                ret_data, sizeof(ret_data), DATA_TIMEOUT);
        if (0 < ret)
        {
            // deal response
            if (0x00 == ret_data[2])
                ret = 0;
            else
            {
                printf(TAG "crc check error.\n");
                ret = -1;
            }

            break;
        }
    }

    return ret;
}

static int sendFirmwareData(const uint8_t *buf, uint32_t len)
{
    int ret = -1;
    int length = len + 3;
    int count  = RESEND_COUNT;
    int dataCount = RESEND_COUNT;
    uint8_t data[DATA_PACKET_LENGTH + 3] = {0};
    uint8_t ret_data[3] = {0};

    // create crc16 checksum
    uint16_t crc_checksum = crc16(buf, len);
    //printf(TAG "Checksum=0x%04x\n", crc_checksum);

    data[0] = ROBOT_UPGRADE_DATA_CMD;
    data[1] = length >> 8;
    data[2] = length & 0xFF;
    memcpy(data + 3, buf, len);

    while (count-- > 0)
    {
        while (dataCount-- > 0)
        {
            ret = sendCANData(RM_UPGRADE_ID, g_curModule, data, length);
            if (0 != ret)
                continue;

            memset(ret_data, 0, sizeof(ret_data));
            ret = readCANData(g_curModule, ROBOT_UPGRADE_DATA_RESP, ret_data, 
                    sizeof(ret_data), DATA_TIMEOUT);
            if (0 < ret)
            {
                // deal response
                ret = 0;
                break;
            }
        }

        if (0 == ret)
        {
            ret = sendDataCRC(crc_checksum);
            if (0 == ret)
            {
                g_DataSended += len;
                downloadProgress(g_DataSended, g_DataTotal);

                break;
            }
        }
    }

    return ret;
}

static int sendFirmware(CImage *dataImage)
{
    int ret = -1;
    uint8_t buf[DATA_PACKET_LENGTH];
  
    g_DataSended   = 0;
    g_DataTotal    = dataImage->GetDataSize();

    downloadProgress(g_DataSended, g_DataTotal);
   
    for (int i = 0; i < dataImage->GetNbElements(); i++)
    {
        PIMAGEELEMENT pElement = dataImage->GetElement(i);
        if (NULL != pElement)
        {
            // send address
            ret = sendDataAddress(pElement->address);
            if (0 != ret)
            {
                printf(TAG "Send data address error.\n");
                return -1;
            }

            // send data
            if (pElement->length <= DATA_PACKET_LENGTH)
            {
                memset(buf, 0, sizeof(buf));
                memcpy(buf, pElement->data, pElement->length);

                // send to can
                ret = sendFirmwareData(buf, pElement->length);
                if (0 != ret)
                    return -1;
            }
            else
            {
                int num = (pElement->length % DATA_PACKET_LENGTH == 0)
                            ? (pElement->length / DATA_PACKET_LENGTH)
                            : (pElement->length / DATA_PACKET_LENGTH + 1);

                uint32_t sended = 0;
                for (int j = 0; j < num - 1; j++)
                {
                    memset(buf, 0, sizeof(buf));
                    memcpy(buf, pElement->data + sended, DATA_PACKET_LENGTH);

                    // send to can
                    sended += DATA_PACKET_LENGTH;
                    ret = sendFirmwareData(buf, DATA_PACKET_LENGTH);
                    if (0 != ret)
                        return -1;
                }

                uint32_t left = pElement->length - (num - 1) * DATA_PACKET_LENGTH;
                memset(buf, 0, sizeof(buf));
                memcpy(buf, pElement->data + sended, left);

                // send to can
                ret = sendFirmwareData(buf, left);
                if (0 != ret)
                    return -1;
            }
        }
    }
   
    return 0;
}

static int sendEndFlag()
{
    int ret = -1;
    uint8_t len = 0;
    uint8_t data[2] = {0};
    uint8_t ret_data[3] = {0};
    int count = RESEND_COUNT;

    data[0] = ROBOT_UPGRADE_END_CMD;
    data[1] = len = 2;

    while (count-- > 0)
    {
        ret = sendCANData(RM_UPGRADE_ID, g_curModule, data, len);
        if (0 != ret)
            continue;

        memset(ret_data, 0, sizeof(ret_data));
        ret = readCANData(g_curModule, ROBOT_UPGRADE_END_RESP, 
                ret_data, sizeof(ret_data), CMD_TIMEOUT);
        if (0 < ret)
        {
            // deal response
            ret = 0;
            break;
        }
    }

    return ret;
}

static int upgrade(uint8_t sub_module, const char *file)
{
    int ret;
    int addr_bytes;
    CImage dataImage;
    time_t tSt, tEd;

    tSt = time(NULL);

    ui->Print(TAG "***Update %s board...***\n", getModuleName(sub_module));
    ui->ShowProgress(0.1, getUpdateTime(sub_module));

    if (RP_UPGRADE_ID == sub_module || RF_UPGRADE_ID == sub_module)
        addr_bytes = SINGLE_BYTE;
    else
        addr_bytes = DOUBLE_BYTE;

    // set upgrade module
    g_curModule = sub_module;

    // load hex file
    if (!dataImage.LoadHex(file, addr_bytes))
    {
        printf(TAG "Load hex file error.(%s)\n", file);
        goto fail;
    }

    // send start flag
    ret = sendStartFlag();
    if (0 != ret)
    {
        printf(TAG "Send start flag error.\n");
        goto fail;
    }

    // send file
    ret = sendFirmware(&dataImage);
    if (0 != ret)
    {
        printf(TAG "Send firmware error.\n");
        goto fail;
    }

    // send end flag
    ret = sendEndFlag();
    if (0 != ret)
    {
        printf(TAG "Send end flag error.\n");
        goto fail;
    }

    tEd = time(NULL);
    printf("\n" TAG "***upgrade use %.fs***\n", difftime(tEd, tSt));

    return 0;

fail:
    ui->Print("Update %s board failed\n", getModuleName(sub_module));

    return -1;
}

static int enter_upgrade_mode(uint8_t module)
{
    int ret = -1;
    uint8_t buf[2] = {0};
    uint8_t ret_data[3] = {0};
    int count = RESEND_COUNT;

    buf[0] = COMMON_SYSCTRL_UPGRADE_CMD;
    buf[1] = 2;

    while (count-- > 0)
    {
        ret = sendCANData(RM_SYSCTRL_ID, module, buf, 2);
        if (0 != ret)
            continue;

        memset(ret_data, 0, sizeof(ret_data));
        ret = readCANData(module, COMMON_SYSCTRL_UPGRADE_CMD_REP, 
                ret_data, sizeof(ret_data), CMD_TIMEOUT);
        if (0 < ret)
        {
            // deal response
            ret = 0;
            break;
        }
    }

    return ret;
}

static int exit_upgrade_mode(uint8_t module)
{
    int ret = -1;
    uint8_t buf[2] = {0};
    uint8_t ret_data[3] = {0};
    int count = RESEND_COUNT;

    buf[0] = COMMON_SYSCTRL_EXIT_UPGRADE_CMD;
    buf[1] = 2;

    while (count-- > 0)
    {
        ret = sendCANData(RM_SYSCTRL_ID, module, buf, 2);
        if (0 != ret)
            continue;

        memset(ret_data, 0, sizeof(ret_data));
        ret = readCANData(module, COMMON_SYSCTRL_EXIT_UPGRADE_CMD_REP,
                ret_data, sizeof(ret_data), CMD_TIMEOUT);
        if (0 < ret)
        {
            // deal response
            ret = 0;
            break;
        }
    }

    return ret;
}

static int is_upgrade_mode(uint8_t module)
{
    int ret = -1;
    uint8_t buf[2] = {0};
    uint8_t ret_data[4] = {0};
    uint8_t mode;

    buf[0] = COMMON_SYSCTRL_SUBSYS_STATE_QUERY;
    buf[1] = 2;

    ret = sendCANData(RM_SYSCTRL_ID, module, buf, 2);
    if (0 == ret)
    {
        ret = readCANData(module, COMMON_SYSCTRL_SUBSYS_STATE_QUERY_REP,
                ret_data, sizeof(ret_data), CMD_TIMEOUT);
        if (0 < ret)
        {
            // deal response
            mode = ret_data[2];
            if (mode == 0x0B)
                return 0;
            else
                return -1;
        }
    }

    return -1;
}

static int allEnterUpgradeMode() 
{
    int ret = -1;
    // make sure all subsystem enter upgrade mode
    // rc
    ret = enter_upgrade_mode(RC_SYS_CTRL_ID);
    if (0 != ret) printf(TAG "Rc enter upgrade mode failed.\n");

    // rp
    ret = enter_upgrade_mode(RP_SYSCTRL_ID);
    if (0 != ret) printf(TAG "Rp enter upgrade mode failed.\n");

    // rf
    ret = enter_upgrade_mode(RF_SYSCTRL_ID);
    if (0 != ret) printf(TAG "Rf enter upgrade mode failed.\n");

    // rb_r
    ret = enter_upgrade_mode(RB_R_ARM_SYS_CTRL_ID);
    if (0 != ret) printf(TAG "Rb right enter upgrade mode failed.\n");

    // rb_l
    ret = enter_upgrade_mode(RB_L_ARM_SYS_CTRL_ID);
    if (0 != ret) printf(TAG "Rb left enter upgrade mode failed.\n");

    // rb_b
    ret = enter_upgrade_mode(RB_BODY_SYS_CTRL_ID);
    if (0 != ret) printf(TAG "Rb body enter upgrade mode failed.\n");

    return 0;
}

static void allExitUpgradeMode()
{
    int ret = -1;
    // all subsystem exit upgrade mode
    // rc
    ret = exit_upgrade_mode(RC_SYS_CTRL_ID);
    if (0 != ret) printf(TAG "Rc exit upgrade mode failed.\n");

    // rp
    ret = exit_upgrade_mode(RP_SYSCTRL_ID);
    if (0 != ret) printf(TAG "Rp exit upgrade mode failed.\n");

    // rf
    ret = exit_upgrade_mode(RF_SYSCTRL_ID);
    if (0 != ret) printf(TAG "Rf exit upgrade mode failed.\n");

    // rb_r
    ret = exit_upgrade_mode(RB_R_ARM_SYS_CTRL_ID);
    if (0 != ret) printf(TAG "Rb right exit upgrade mode failed.\n");

    // rb_l
    ret = exit_upgrade_mode(RB_L_ARM_SYS_CTRL_ID);
    if (0 != ret) printf(TAG "Rb left exit upgrade mode failed.\n");

    // rb_b
    ret = exit_upgrade_mode(RB_BODY_SYS_CTRL_ID);
    if (0 != ret) printf(TAG "Rb body exit upgrade mode failed.\n");

}


int robotSubSysUpdate(int count, UPGRADE_MODULE *m)
{
    int ret = -1;
    if (ensure_path_mounted("/mnt/internal_sd") != 0)
    {
       printf(TAG "Can't mount /mnt/internal_sd.\n");
       return -1;
    }

    //ui->SetProgressType(RecoveryUI::DETERMINATE);
    //ui->SetBackground(RecoveryUI::INSTALLING_UPDATE);

    // open can
    ret = openCANDevice();
    if (ret != 0)
    {
        printf(TAG "Open can device error.\n");
        return -1;
    }

    // enter upgrade mode
    allEnterUpgradeMode();

    // start upgrade
    for (int i = 0; i < count; i++)
    {
        upgrade(m->module, m->file);
        m++;
    }

    // exit upgrade mode
    allExitUpgradeMode();

    // close can
    closeCANDevice();
    return ret;
}

