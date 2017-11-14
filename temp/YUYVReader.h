#ifndef _YUYVREADER_H_
#define _YUYVREADER_H_

#include "YUVReader.h"

class YUYVReader
{
public:
    YUYVReader();
    ~YUYVReader();

    bool    OpenFile( const char* filename, int width, int height );
    void    Close();

    int     GetFrameCount() const       { return m_FrameCnt; }
    int     GetFrameWidth() const       { return m_FrameWidth; }
    int     GetFrameHeight() const      { return m_FrameHeight; }
    int     GetFrameSize() const        { return m_FrameSize; }     // raw frame size
    int     GetChromaType() const       { return YUV_ChromaType_422; }

    // 读取一帧原始格式的数据, pDst 大小至少为 GetFrameSize()
    bool    GetFrameRaw( int idx, uint8_t* pDst );

    // 读取一帧亮度数据, pDst 大小至少为 GetFrameWidth() * GetFrameHeight()
    bool    GetFrameRawLuma( int idx, uint8_t* pDst );

    // 读取一帧数据, 并将其转化为 YUV 4:2:0 格式
    bool    GetFrameYUVP420( int idx, YUVFrame* pFrame );

    // 读取一帧数据, 并将其转化为 YUV 4:2:2 格式
    bool    GetFrameYUVP422( int idx, YUVFrame* pFrame );

    // 读取一帧数据, 并将其转化为 YUV 4:4:4 格式
    bool    GetFrameYUVP444( int idx, YUVFrame* pFrame );

    // 读取一帧数据, 并将其转化为 BGRA 格式,  
    // pGBRA 需 16 字节对其, 大小至少为 dstPitch * GetFrameHeight(), dstpitch 需 >= GetFrameWidth() * 4 且为 16 的倍数
    bool    GetFrameBGRA( int idx, uint8_t* pGBRA, int dstPitch );

private:
    int         m_FrameWidth;
    int         m_FrameHeight;
    int         m_FrameSize;
    int         m_FrameCnt;
    uint8_t*    m_pLine;
    FILE*       m_pFile;
};

#endif