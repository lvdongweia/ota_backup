/*************************************************************************
    > File Name: Image.h
    > Author: 
    > Mail:  
    > Created Time: 2015年11月23日 星期一 14时31分46秒
 ************************************************************************/

#ifndef IMAGE_H
#define IMAGE_H

namespace android
{
#define SINGLE_BYTE 1
#define DOUBLE_BYTE 2

typedef struct 
{  
    long address;
    long length;
    uint8_t *data;
}IMAGEELEMENT, *PIMAGEELEMENT;

class CImage
{
public: 
    CImage();
    ~CImage();

    bool LoadHex(const char *path, int addr_bytes);
    //int GetNbElements() {  return m_pElements.size(); }
    int GetNbElements();
    PIMAGEELEMENT GetElement(int rank);
    long GetDataSize() { return m_DataSize; }

private:
    long m_DataSize;
    char m_FilePath[512];
    //vector<PIMAGEELEMENT> m_pElements;

    void ClearVector();
    bool CheckFileExt(const char *path);
    void _splitpath(const char *path, char *drive, char *dir, char *fname, char *ext);
    void _split_whole_name(const char *whole_name, char *fname, char *ext);
    char* _strupr(char *str);
    void AddImageElement(PIMAGEELEMENT element);
    void CompactElements();
    static bool CompareElement(PIMAGEELEMENT pElem1, PIMAGEELEMENT pElem2);
};

}; // android namespace

#endif
