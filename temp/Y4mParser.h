#ifndef _Y4MPARSER_H_
#define _Y4MPARSER_H_

#include "YUVReader.h"

class Y4mParser
{
public:
    Y4mParser();
    ~Y4mParser();

    bool    OpenFile( const char* filename );
    void    Close();

    int     GetFrameWidth() const               { return m_FrameWidth; }
    int     GetFrameHeight() const              { return m_FrameHeight; }
    int     GetFrameSize() const                { return m_FrameSize; }     // raw frame size
    int     GetFrameCount() const               { return m_FrameCnt; }
    void    GetFrameRate( int* num, int* den )  { *num = m_FrameRateNum; *den = m_FrameRateDen; }
    int     GetFrameStruct() const              { return m_FrameStruct; }   // YUV_Progressive, YUV_Top_Field_First, YUV_Bot_Field_First
    int     GetChromaType() const               { return m_ChromaType; }    // YUV_ChromaType_420, YUV_ChromaType_422, YUV_ChromaType_444

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
    int         ParseField( const char* field, int size );
    int         m_FrameWidth;
    int         m_FrameHeight;
    int         m_FrameSize;
    int         m_ChromaType;
    int         m_YSize;
    int         m_FrameRateNum;
    int         m_FrameRateDen;
    int         m_FrameStruct;
    int         m_FrameOffset;
    int         m_FrameCnt;
    uint8_t*    m_pYPlane;
    uint8_t*    m_pUPlane;
    uint8_t*    m_pVPlane;
    FILE*       m_pFile;
};

#endif