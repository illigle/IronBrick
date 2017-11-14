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

/* �� YUV ��ת��Ϊ BGRA ��ʽ, 
*  ���Ϊ 4:2:0 ���� 4:2:2 ��ʽ, pFrame ���� plane ��ָ����ڴ�Ӧ����� 4 ���ֽ�,
*  pGBRA �� 16 �ֽڶ���, ��ָ����ڴ��С����Ϊ dstPitch * pFrame->pitch[0], 
*  dstpitch �� >= pFrame->width[0] * 4 ��Ϊ 16 �ı��� 
*/
extern void ConvertYUV420_BGRA( YUVFrame* pFrame, uint8_t* pGBRA, int dstPitch );
extern void ConvertYUV422_BGRA( YUVFrame* pFrame, uint8_t* pGBRA, int dstPitch );
extern void ConvertYUV444_BGRA( YUVFrame* pFrame, uint8_t* pGBRA, int dstPitch );

// ʹ�� 16 λ��������, ����ٶ�, ��������
extern void FastConvertYUV420_BGRA( YUVFrame* pFrame, uint8_t* pGBRA, int dstPitch );
extern void FastConvertYUV422_BGRA( YUVFrame* pFrame, uint8_t* pGBRA, int dstPitch );
extern void FastConvertYUV444_BGRA( YUVFrame* pFrame, uint8_t* pGBRA, int dstPitch );

// ��ȡ YUV ����, ���ԭ������Ŀ�����ݸ�ʽ��ƥ��, �Զ�ת��
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