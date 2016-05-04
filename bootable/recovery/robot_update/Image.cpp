/*************************************************************************
    > File Name: Image.cpp
    > Author: 
    > Mail:  
    > Created Time: 2015年11月23日 星期一 14时31分41秒
 ************************************************************************/
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>

#include <vector>
#include <algorithm>
using namespace std;

#include "Image.h"

#undef LOG_TAG
#define LOG_TAG "IMAGE: "

namespace android
{

vector<PIMAGEELEMENT> m_pElements;

CImage::CImage()
{
    m_DataSize = 0;
}

CImage::~CImage()
{
    ClearVector();
}

void CImage::ClearVector()
{
    vector<PIMAGEELEMENT>::iterator it = m_pElements.begin();

    for (; it != m_pElements.end(); it++)
    {
        PIMAGEELEMENT pElement = (PIMAGEELEMENT)*it;
        if (NULL != pElement->data)
        {
            free(pElement->data);
        }
        delete pElement;
    }

    m_pElements.clear();
}

int CImage::GetNbElements()
{
    return m_pElements.size();
}

bool CImage::LoadHex(const char *path, int addr_bytes)
{
    IMAGEELEMENT element, *pPrevElement;
    FILE *fp;
    bool last_byte = false;
    bool bRet = true;
    bool bConcatenate = false;
    unsigned int  sum_var,
                  address,
                  target_address,
                  base_address=0,
                  extended_address=0,
                  checksum;

    unsigned int byte_count, separator, character;
    char colon;
    int lineno = 0;

    m_DataSize = 0;
    ClearVector();

    if (false == CheckFileExt(path))
    {
        printf(LOG_TAG "Could not identify the file name suffix\n");
        return false;
    }

    fp = fopen(path, "r");
    if (NULL == fp)
    {
        printf(LOG_TAG "File open error.(%s)\n", strerror(errno));
        return false;
    }

    // read file
    do 
    {
        sum_var = 0;
        fscanf(fp, "%1c", &colon);

        if (feof(fp))
        {
            last_byte = true;
        }
        else if (':' == colon)
        {
            lineno++;

            fscanf(fp, "%2x", &byte_count);
            sum_var += byte_count;
            fscanf(fp, "%4x", &address);
            fscanf(fp, "%2x", &separator);

            if (0x00 == separator)
            {
                m_DataSize += byte_count;

                target_address = (extended_address << 16) + (base_address << 4) + address;
                sum_var = sum_var + (address >> 8) + (address % 256);

                element.address = target_address;
                element.length  = byte_count;
                element.data    = (uint8_t *)malloc(byte_count);

                for (int i = 0; i < (int)byte_count; i++)
                {
                    fscanf(fp, "%2x", &character);
                    sum_var += character;
                    element.data[i] = (uint8_t)character;
                }

                sum_var += separator;
                fscanf(fp, "%2x", &checksum);
                sum_var = (sum_var % 256);

                if (((checksum + sum_var) % 256) != 0)
                {
                    free(element.data);
                    printf(LOG_TAG "line %i: Bad hexadecimal checksum-00!\n", lineno);
                    bRet = false;
                    break;
                }

                bConcatenate = false;
                if (0 != GetNbElements())
                {
                    pPrevElement = (PIMAGEELEMENT)m_pElements.back();
                    if ((pPrevElement->address + pPrevElement->length / addr_bytes) == element.address)
                        bConcatenate = true;
                    else
                        bConcatenate = false;
                }

                if (false == bConcatenate)
                {
                    //printf("New Address=0x%08x\n", element.address);
                    AddImageElement(&element);
                }
                else
                {
                    pPrevElement->data = (uint8_t *)realloc(pPrevElement->data, pPrevElement->length + element.length);
                    memcpy(pPrevElement->data + pPrevElement->length, element.data, element.length);
                    pPrevElement->length += element.length;
                }

                free(element.data);
            }
            else if (0x01 == separator)
            {
                sum_var = sum_var + (address >> 8) + (address % 256) + separator;
                sum_var = (sum_var % 256);
                fscanf(fp, "%2x", &checksum);
                if (((checksum + sum_var) % 256) != 0)
                {
                    printf(LOG_TAG "line %i: Bad hexadecimal checksum-01!\n", lineno);
                    bRet = false;
                    break;
                }
                else
                {
                    last_byte = true;
                }
            }
            else if (0x02 == separator)
            {
                fscanf(fp, "%4x", &base_address);
                fscanf(fp, "%2x", &checksum);
                sum_var = sum_var + (address >> 8) + (address % 256) + separator \
                          + (base_address >> 8) + (base_address % 256);
                sum_var = (sum_var % 256);
                if (((checksum + sum_var) % 256) != 0)
                {
                    printf(LOG_TAG "line %i: Bad hexadecimal checksum-02!\n", lineno);
                    bRet = false;
                    break;
                }
            }
            else if (0x03 == separator)
            {
                sum_var += sum_var + (address >> 8) + (address % 256) + separator;
                for (int i = 0; i < (int)byte_count; i++)
                {
                    fscanf(fp, "%2x", &character);
                    sum_var += character;
                }
                fscanf(fp, "%2x", &checksum);
                sum_var = (sum_var % 256);
                if (((checksum + sum_var) % 256) != 0)
                {
                    printf(LOG_TAG "line %i: Bad hexadecimal checksum-03!\n", lineno);
                    bRet = false;
                    break;
                } 
            }
            else if (0x04 == separator)
            {
                fscanf(fp, "%4x", &extended_address);
                fscanf(fp, "%2x", &checksum);
                sum_var = sum_var + (address >> 8) + (address % 256) + separator \
                          + (extended_address >> 8) + (extended_address % 256);
                sum_var = (sum_var % 256);
                if (((checksum + sum_var) % 256) != 0)
                {
                    printf(LOG_TAG "line %i: Bad hexadecimal checksum-04!\n", lineno);
                    bRet = false;
                    break;
                }
            }
            else if (0x05 == separator)
            {
                sum_var = sum_var + (address >> 8) + (address % 256) + separator;
                for (int i = 0; i < (int)byte_count; i++)
                {
                    fscanf(fp, "%2x", &character);
                    sum_var += character;
                }
                fscanf(fp, "%2x", &checksum);
                sum_var = (sum_var % 256);
                if (((checksum + sum_var) % 256) != 0)
                {
                    printf(LOG_TAG "line %i: Bad hexadecimal checksum-05!\n", lineno);
                    bRet = false;
                    break;
                }
            }
            else
            {
                printf(LOG_TAG "line %i: Unknow data type!\n", lineno);
                bRet = false;
                break;
            }
        }
        else if (('\r' == colon) || ('\n' == colon))
        {
            if (feof(fp))
                last_byte = true;
        }
        else if (' ' == colon)
        {
            if (feof(fp))
                last_byte = true;
        }
        else
        {
            printf(LOG_TAG "line %i: Not hex format!\n", lineno);
            bRet = false;
            break;
        }
    } while(false == last_byte);

    fclose(fp);

    if (true == bRet)
    {
       CompactElements();
    }

    return bRet;
}

PIMAGEELEMENT CImage::GetElement(int rank)
{
    int sum = GetNbElements();
    if (0 > sum || (rank > sum - 1))
        return NULL;


    return m_pElements.at(rank);
}


bool CImage::CheckFileExt(const char *path)
{
    char Drive[3], Dir[256], Fname[256], Ext[256];
    char *ptr;

    m_pElements.clear();

    _splitpath(path, Drive, Dir, Fname, Ext);
    ptr = _strupr(Ext);
    strcpy(Ext, ptr);

    if (strcmp(Ext, ".HEX") == 0) 
        return true;
    else
        return false;
}


void CImage::_splitpath(const char *path, char *drive, char *dir, char *fname, char *ext)
{
    char *p_whole_name;

    drive[0] = '\0';
    if (NULL == path)
    {
        dir[0] = '\0';
        fname[0] = '\0';
        ext[0] = '\0';
        return;
    }

    if ('/' == path[strlen(path)])
    {
        strcpy(dir, path);
        fname[0] = '\0';
        ext[0] = '\0';
        return;
    }

    p_whole_name = strrchr(path, '/');
    if (NULL != p_whole_name)
    {
        p_whole_name++;
        _split_whole_name(p_whole_name, fname, ext);
    }
    else
    {
        _split_whole_name(path, fname, ext);
        dir[0] = '\0';
    }
}

void CImage::_split_whole_name(const char *whole_name, char *fname, char *ext)
{
    char *p_ext;

    p_ext = strrchr(whole_name, '.');
    if (NULL != p_ext)
    {
        strcpy(ext, p_ext);
        snprintf(fname, p_ext - whole_name + 1, "%s", whole_name);
    }
    else
    {
        ext[0] = '\0';
        strcpy(fname, whole_name);
    }
}

char* CImage::_strupr(char *str)
{
    char *origin = str;
    for (; *str != '\0'; str++)
    {
        *str = toupper(*str);
    }

    return origin;
}

void CImage::AddImageElement(PIMAGEELEMENT element)
{
    PIMAGEELEMENT pNewElement;

    pNewElement = new IMAGEELEMENT;
    pNewElement->address = element->address;
    pNewElement->length  = element->length;
    pNewElement->data    = (uint8_t *)malloc(element->length);

    memcpy(pNewElement->data, element->data, element->length);

    m_pElements.push_back(pNewElement);
}


void CImage::CompactElements()
{
    vector<PIMAGEELEMENT>::iterator it;

    // printf("vector size: %d\n", m_pElements.size());
    sort(m_pElements.begin(), m_pElements.end(), CompareElement);

    for (it = m_pElements.begin(); it != m_pElements.end() - 1; it++)
    {
        PIMAGEELEMENT pElement1, pElement2;
        pElement1 = (PIMAGEELEMENT)(*it);
        pElement2 = (PIMAGEELEMENT)(*(it+1));

        if (pElement1->address + pElement1->length/2 == pElement2->address)
        {
            pElement1->data = (uint8_t *)realloc(pElement1->data, pElement1->length + pElement2->length);
            memcpy(pElement1->data + pElement1->length, pElement2->data, pElement2->length);
            pElement1->length += pElement2->length;
            free(pElement2->data);

            m_pElements.erase(it+1);

            delete pElement2;
        }
    }
}

bool CImage::CompareElement(PIMAGEELEMENT pElem1, PIMAGEELEMENT pElem2)
{
    return pElem1->address < pElem2->address;
}

};

