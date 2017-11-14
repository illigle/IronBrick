#ifndef _YUVREADER_H_
#define _YUVREADER_H_

#include "CommonDefine.h"
#include <stdio.h>

struct YUVFrame
{
    uint8_t*    planes[4];
    int         width[4];
    int         height[4];
    int         pitch[4];
};

/* 将 YUV 其转化为 BGRA 格式, 
*  如果为 4:2:0 或者 4:2:2 格式, pFrame 各个 plane 所指向的内存应多分配 4 个字节,
*  pGBRA 需 16 字节对其, 所指向的内存大小至少为 dstPitch * pFrame->pitch[0], 
*  dstpitch 需 >= pFrame->width[0] * 4 且为 16 的倍数 
*/
extern void ConvertYUV420_BGRA( YUVFrame* pFrame, uint8_t* pGBRA, int dstPitch );
extern void ConvertYUV422_BGRA( YUVFrame* pFrame, uint8_t* pGBRA, int dstPitch );
extern void ConvertYUV444_BGRA( YUVFrame* pFrame, uint8_t* pGBRA, int dstPitch );

// 使用 16 位整数运算, 提高速度, 牺牲精度
extern void FastConvertYUV420_BGRA( YUVFrame* pFrame, uint8_t* pGBRA, int dstPitch );
extern void FastConvertYUV422_BGRA( YUVFrame* pFrame, uint8_t* pGBRA, int dstPitch );
extern void FastConvertYUV444_BGRA( YUVFrame* pFrame, uint8_t* pGBRA, int dstPitch );

// 读取 YUV 数据, 如果原数据与目标数据格式不匹配, 自动转化
extern bool ReadYUV420( FILE* fp, int width, int height, int chroma, int bitDepth, YUVFrame* pFrame );
extern bool ReadYUV422( FILE* fp, int width, int height, int chroma, int bitDepth, YUVFrame* pFrame );
extern bool ReadYUV444( FILE* fp, int width, int height, int chroma, int bitDepth, YUVFrame* pFrame );

//===================================================================================================================================
enum YUVChromaType
{
    YUV_ChromaType_Luma,
    YUV_ChromaType_420,
    YUV_ChromaType_422,
    YUV_ChromaType_444,
};

enum YUVFrameStruct
{
    YUV_Progressive,
    YUV_Top_Field_First,
    YUV_Bot_Field_First,
};

class YUVReader
{
public:
    YUVReader();
    ~YUVReader();

    bool    OpenFile( const char* filename, int width, int height, int chroma, int bitDepth = 8 );
    void    Close();

    int     GetFrameCount() const       { return m_FrameCnt; }
    int     GetFrameWidth() const       { return m_FrameWidth; }
    int     GetFrameHeight() const      { return m_FrameHeight; }
    int     GetFrameSize() const        { return m_FrameSize; }     // raw frame size
    int     GetChromaType() const       { return m_ChromaType; }    // YUV_ChromaType_420, YUV_ChromaType_422, YUV_ChromaType_444

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
    int         m_ChromaType;
    int         m_BitDepth;
    int         m_FrameSize;
    int         m_YSize;
    int         m_FrameCnt;
    uint8_t*    m_pYPlane;
    uint8_t*    m_pUPlane;
    uint8_t*    m_pVPlane;
    FILE*       m_pFile;
};

#endif