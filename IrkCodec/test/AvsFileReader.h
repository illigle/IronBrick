#ifndef _AVS_FILEREADER_H_
#define _AVS_FILEREADER_H_

#include "IrkVector.h"

// AVS 裸流读取器
class AvsFileReader
{
public:
    AvsFileReader();
    ~AvsFileReader();

    bool open(const char* filename);
    void close();
    int  seek(int64_t offset);

    // 读取一帧数据, 返回的指针无需释放, 在下一次调用和关闭前有效
    const uint8_t* get_picture(size_t* psize);

private:
    typedef irk::Vector<uint8_t> DataVec;
    DataVec m_DataBuf[2];
    int     m_BufIdx;
    FILE*   m_pFile;
};

#endif
