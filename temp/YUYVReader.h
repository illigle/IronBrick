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

    // ��ȡһ֡ԭʼ��ʽ������, pDst ��С����Ϊ GetFrameSize()
    bool    GetFrameRaw( int idx, uint8_t* pDst );

    // ��ȡһ֡��������, pDst ��С����Ϊ GetFrameWidth() * GetFrameHeight()
    bool    GetFrameRawLuma( int idx, uint8_t* pDst );

    // ��ȡһ֡����, ������ת��Ϊ YUV 4:2:0 ��ʽ
    bool    GetFrameYUVP420( int idx, YUVFrame* pFrame );

    // ��ȡһ֡����, ������ת��Ϊ YUV 4:2:2 ��ʽ
    bool    GetFrameYUVP422( int idx, YUVFrame* pFrame );

    // ��ȡһ֡����, ������ת��Ϊ YUV 4:4:4 ��ʽ
    bool    GetFrameYUVP444( int idx, YUVFrame* pFrame );

    // ��ȡһ֡����, ������ת��Ϊ BGRA ��ʽ,  
    // pGBRA �� 16 �ֽڶ���, ��С����Ϊ dstPitch * GetFrameHeight(), dstpitch �� >= GetFrameWidth() * 4 ��Ϊ 16 �ı���
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