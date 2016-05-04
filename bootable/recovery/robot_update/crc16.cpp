
#include <stdio.h>
#include "crc16.h" 

uint16_t crc16(const uint8_t *data, int len)
{
    uint16_t CRC = 0xFFFF;
    uint8_t j, TMP = 0;
    int i;

    for (i = 0; i < len; i++)
    {
        CRC ^= data[i];
        for (j = 0; j < 8; j++)
        {
            TMP = CRC & 0x0001;
            CRC = CRC >> 1;
            if (TMP)
                CRC = CRC ^ 0xA001;
        }
    }

    return CRC; 
}
