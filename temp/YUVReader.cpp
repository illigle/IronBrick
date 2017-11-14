#include "stdafx.h"
#include "YUVReader.h"
#include <assert.h>
#include <malloc.h>
#include <string.h>
#include <intrin.h>

#ifdef _MSC_VER
#pragma intrinsic( memcmp, memcpy, memset )
#endif

// #define CHECK_CVT_TIME  1

/*
// ITU-R 709 标准 :
const float Kr_709 = 0.2125f;
const float Kb_709 = 0.0721f;
const float Kg_709 = 0.7154f;

// 未经量化处理的标准公式
static void YCbCr_RGB_709_Ref( float Y, float Cb, float Cr, float* pR, float* pG, float* pB )
{
    *pR = Y + 2.f * (1.f - Kr_709) * Cr;
    *pG = Y - 2.f * Kr_709 * (1.f - Kr_709) * Cr / Kg_709 - 2.f * Kb_709 * (1.f - Kb_709) * Cb / Kg_709;
    *pB = Y + 2.f * (1.f - Kb_709) * Cb;
}
*/

/* ITU-R 709 标准, 量化的公式
*  输入 Y : [16 235], Cb Cr : [16 240], alpha : [0 255]
*  输出 [0 255]
*/
static int YCbCr_BGR_709( int Y, int Cb, int Cr )
{
    /*
    #define RGB_CLAMP(x) ((x) > 0.f ? ((x) < 255.f ? (uint32_t)(x) : 255) : 0)
    float fB = 1.164f * (Y - 16) + 2.114f * (Cb - 128);
    float fG = 1.164f * (Y - 16) - 0.534f * (Cr - 128) - 0.213f * (Cb - 128);    
    float fR = 1.164f * (Y - 16) + 1.793f * (Cr - 128);
    uint32_t uG = RGB_CLAMP( fG );
    uint32_t uB = RGB_CLAMP( fB );
    uint32_t uR = RGB_CLAMP( fR );
    return (uR << 16) | (uG << 8) | uB; */

    ALIGNAS(16) float bgra[4];
    bgra[0] = 1.164f * (Y - 16) + 2.114f * (Cb - 128);
    bgra[1] = 1.164f * (Y - 16) - 0.534f * (Cr - 128) - 0.213f * (Cb - 128);
    bgra[2] = 1.164f * (Y - 16) + 1.793f * (Cr - 128);
    bgra[3] = 0.f;

    __m128i val = _mm_cvtps_epi32( *(__m128*)bgra );
    val = _mm_packs_epi32( val, val );
    val = _mm_packus_epi16( val, val );
    return _mm_cvtsi128_si32( val );
}

// 转化成整数运算( 系数 * 1024 )
static int YCbCr_BGR_709_int32( int Y, int Cb, int Cr )
{
    Y = 1192 * (Y - 16) + 512;      // 512 for round
    Cb -= 128;
    Cr -= 128;

    ALIGNAS(16) int bgra[4] = { Y + 2165 * Cb, Y - 547 * Cr - 218 * Cb, Y + 1836 * Cr, 0 };
    __m128i val = _mm_load_si128( (__m128i*)bgra );
    val = _mm_srai_epi32( val, 10 );
    val = _mm_packs_epi32( val, val );
    val = _mm_packus_epi16( val, val );
    return _mm_cvtsi128_si32( val );
}

/* ITU-R 601 标准, 量化的公式
*  输入 Y : [16 235], Cb Cr : [16 240], alpha : [0 255]
*  输出 [0 255]
*/
static int YCbCr_BGR_601( int Y, int Cb, int Cr )
{
    ALIGNAS(16) float bgra[4];
    bgra[0] = 1.164f * (Y - 16) + 2.017f * (Cb - 128);
    bgra[1] = 1.164f * (Y - 16) - 0.813f * (Cr - 128) - 0.392f * (Cb - 128);    
    bgra[2] = 1.164f * (Y - 16) + 1.596f * (Cr - 128);
    bgra[3] = 0.f;

    __m128i val = _mm_cvttps_epi32( *(__m128*)bgra );
    val = _mm_packs_epi32( val, val );
    val = _mm_packus_epi16( val, val );
    return _mm_cvtsi128_si32( val );
}

// 转化成整数运算( 系数 * 1024 )
static int YCbCr_BGR_601_int32( int Y, int Cb, int Cr )
{
    Y = 1192 * (Y - 16) + 512;      // 512 for round
    Cb -= 128;
    Cr -= 128;

    ALIGNAS(16) int bgra[4] = { Y + 2065 * Cb, Y - 832 * Cr - 401 * Cb, Y + 1634 * Cr, 0 };
    __m128i val = _mm_load_si128( (__m128i*)bgra );
    val = _mm_srai_epi32( val, 10 );
    val = _mm_packs_epi32( val, val );
    val = _mm_packus_epi16( val, val );
    return _mm_cvtsi128_si32( val );
}

typedef int (*YCbCr_BGR_Func)( int, int, int );

ALIGNAS(16) static const uint8_t s_bgraShuffle[16] = 
{
    0, 4, 8, 0x80, 1, 5, 9, 0x80, 2, 6, 10, 0x80, 3, 7, 11, 0x80,
};

ALIGNAS(16) static const float s_Yfactors[4]    = { 1.164f, 1.164f, 1.164f, 1.164f };
ALIGNAS(16) static const float s_UfactorsHD[4]  = { 0.213f, 0.213f, 0.213f, 0.213f };
ALIGNAS(16) static const float s_UScaleHD[4]    = { 9.92488f, 9.92488f, 9.92488f, 9.92488f };  // 2.114 / 0.213 = 9.92488
ALIGNAS(16) static const float s_VfactorsHD[4]  = { 0.534f, 0.534f, 0.534f, 0.534f };
ALIGNAS(16) static const float s_VScaleHD[4]    = { 3.35768f, 3.35768f, 3.35768f, 3.35768f };  // 1.793 / 0.534 = 3.35768
ALIGNAS(16) static const float s_UfactorsSD[4]  = { 0.392f, 0.392f, 0.392f, 0.392f };
ALIGNAS(16) static const float s_UScaleSD[4]    = { 5.14540f, 5.14540f, 5.14540f, 5.14540f };  // 2.017 / 0.392 = 5.14540
ALIGNAS(16) static const float s_VfactorsSD[4]  = { 0.813f, 0.813f, 0.813f, 0.813f };
ALIGNAS(16) static const float s_VScaleSD[4]    = { 1.96310f, 1.96310f, 1.96310f, 1.96310f };  // 1.793 / 0.813 = 1.96310

// 16 位整数运算(系数 * 64)
ALIGNAS(16) static const int16_t s_YCoeff[8]    = { 74, 74, 74, 74, 74, 74, 74, 74 };
ALIGNAS(16) static const int16_t s_UCoeffHD[8]  = { 135, 135, 135, 135, 135, 135, 135, 135 };
ALIGNAS(16) static const int16_t s_UCoeffHDm[8] = { -14, -14, -14, -14, -14, -14, -14, -14 };
ALIGNAS(16) static const int16_t s_VCoeffHD[8]  = { 115, 115, 115, 115, 115, 115, 115, 115 };
ALIGNAS(16) static const int16_t s_VCoeffHDm[8] = { -34, -34, -34, -34, -34, -34, -34, -34 };
ALIGNAS(16) static const int16_t s_UCoeffSD[8]  = { 129, 129, 129, 129, 129, 129, 129, 129 };
ALIGNAS(16) static const int16_t s_UCoeffSDm[8] = { -25, -25, -25, -25, -25, -25, -25, -25 };
ALIGNAS(16) static const int16_t s_VCoeffSD[8]  = { 102, 102, 102, 102, 102, 102, 102, 102 };
ALIGNAS(16) static const int16_t s_VCoeffSDm[8] = { -52, -52, -52, -52, -52, -52, -52, -52 };
ALIGNAS(16) static const int16_t s_Round32[8]   = { 32, 32, 32, 32, 32, 32, 32, 32 };
ALIGNAS(16) static const int16_t s_Minus16[8]   = { -16, -16, -16, -16, -16, -16, -16, -16 };
ALIGNAS(16) static const int16_t s_Minus128[8]  = { -128, -128, -128, -128, -128, -128, -128, -128 };

union XMM
{
    __m128i i128;
    __m128  f128;
    __m128d d128;
};

void ConvertYUV420_BGRA( YUVFrame* pFrame, uint8_t* pDst, int dstPitch )
{
    DASSERT( (dstPitch & 15) == 0 );
#ifdef CHECK_CVT_TIME
    LARGE_INTEGER fre, t1, t2;
    QueryPerformanceFrequency( &fre );
    QueryPerformanceCounter( &t1 );
#endif

    uint8_t* yrow = pFrame->planes[0];
    uint8_t* urow = pFrame->planes[1];
    uint8_t* vrow = pFrame->planes[2];
    const int yWidth8 = pFrame->width[0] >> 3;
    const int yHeight = pFrame->height[0];
    const int yPitch = pFrame->pitch[0];
    const int uvWidth = pFrame->width[1];

    // setup transform factors
    __m128  yfactor, ufactor, vfactor, uscale, vscale;
    __m128i bgrShuffle16 = _mm_load_si128( (__m128i*)s_bgraShuffle );
    YCbCr_BGR_Func Yuv2Bgr = NULL;

    if( yHeight >= 720 )   // HD
    {
        yfactor = _mm_load_ps( s_Yfactors );
        ufactor = _mm_load_ps( s_UfactorsHD );
        vfactor = _mm_load_ps( s_VfactorsHD );
        uscale = _mm_load_ps( s_UScaleHD );
        vscale = _mm_load_ps( s_VScaleHD );
        Yuv2Bgr = &YCbCr_BGR_709;
    }
    else                    // SD
    {
        yfactor = _mm_load_ps( s_Yfactors );
        ufactor = _mm_load_ps( s_UfactorsSD );
        vfactor = _mm_load_ps( s_VfactorsSD );
        uscale = _mm_load_ps( s_UScaleSD );
        vscale = _mm_load_ps( s_VScaleSD );
        Yuv2Bgr = &YCbCr_BGR_601;
    }

    // 转化 4:2:0, 4:2:2 到 4:4:4 取决于当初的采样算法,
    // 由于人眼对颜色不敏感, 简化起见, 垂直方向不进行滤波, 水平方向取平均
    XMM yxm1, yxm2, yxm3, yxm4;
    XMM uxm1, uxm2, uxm3, vxm1, vxm2, vxm3;

    for( int i = 0; i < yHeight; i += 2 )
    {
        uint8_t* dst = pDst + i * dstPitch;

        // 设置边界点, 方便滤波
        uint8_t uEdge = urow[uvWidth];
        uint8_t vEdge = vrow[uvWidth];
        urow[uvWidth] = urow[uvWidth-1];
        vrow[uvWidth] = vrow[uvWidth-1];

        // 使用 sse4, 每次处理 8 个点
        int j = 0;
        for( ; j < yWidth8; j++ )
        {
            yxm1.i128 = _mm_loadl_epi64( (__m128i*)(yrow + j*8) );
            yxm3.i128 = _mm_loadl_epi64( (__m128i*)(yrow + j*8 + yPitch) );
            uxm1.i128 = _mm_loadl_epi64( (__m128i*)(urow + j*4) );
            vxm1.i128 = _mm_loadl_epi64( (__m128i*)(vrow + j*4) );

            // UV 水平方向不存在的点用左右点的均值替代
            uxm2.i128 = _mm_srli_si128( uxm1.i128, 1 );
            vxm2.i128 = _mm_srli_si128( vxm1.i128, 1 );
            uxm2.i128 = _mm_avg_epu8( uxm2.i128, uxm1.i128 );
            vxm2.i128 = _mm_avg_epu8( vxm2.i128, vxm1.i128 );
            uxm1.i128 = _mm_unpacklo_epi8( uxm1.i128, uxm2.i128 );
            vxm1.i128 = _mm_unpacklo_epi8( vxm1.i128, vxm2.i128 );
            yxm2.i128 = _mm_shuffle_epi32( yxm1.i128, 0x55 );
            yxm4.i128 = _mm_shuffle_epi32( yxm3.i128, 0x55 );
            uxm2.i128 = _mm_shuffle_epi32( uxm1.i128, 0x55 );
            vxm2.i128 = _mm_shuffle_epi32( vxm1.i128, 0x55 );

            yxm1.i128 = _mm_cvtepu8_epi32( yxm1.i128 );
            yxm2.i128 = _mm_cvtepu8_epi32( yxm2.i128 );
            yxm3.i128 = _mm_cvtepu8_epi32( yxm3.i128 );
            yxm4.i128 = _mm_cvtepu8_epi32( yxm4.i128 );
            uxm1.i128 = _mm_cvtepu8_epi32( uxm1.i128 );
            uxm2.i128 = _mm_cvtepu8_epi32( uxm2.i128 );
            vxm1.i128 = _mm_cvtepu8_epi32( vxm1.i128 );
            vxm2.i128 = _mm_cvtepu8_epi32( vxm2.i128 );
            __m128i xm0 = _mm_cvtsi32_si128( 16 );
            xm0 = _mm_shuffle_epi32( xm0, 0 );
            yxm1.i128 = _mm_sub_epi32( yxm1.i128, xm0 );
            yxm2.i128 = _mm_sub_epi32( yxm2.i128, xm0 );
            yxm3.i128 = _mm_sub_epi32( yxm3.i128, xm0 );
            yxm4.i128 = _mm_sub_epi32( yxm4.i128, xm0 );
            xm0 = _mm_slli_epi32( xm0, 3 );                 // 128
            uxm1.i128 = _mm_sub_epi32( uxm1.i128, xm0 );
            uxm2.i128 = _mm_sub_epi32( uxm2.i128, xm0 );
            vxm1.i128 = _mm_sub_epi32( vxm1.i128, xm0 );
            vxm2.i128 = _mm_sub_epi32( vxm2.i128, xm0 );

            // cvt to float, mul factor
            yxm1.f128 = _mm_cvtepi32_ps( yxm1.i128 );
            yxm2.f128 = _mm_cvtepi32_ps( yxm2.i128 );
            yxm3.f128 = _mm_cvtepi32_ps( yxm3.i128 );
            yxm4.f128 = _mm_cvtepi32_ps( yxm4.i128 );
            uxm1.f128 = _mm_cvtepi32_ps( uxm1.i128 );
            uxm2.f128 = _mm_cvtepi32_ps( uxm2.i128 );
            vxm1.f128 = _mm_cvtepi32_ps( vxm1.i128 );
            vxm2.f128 = _mm_cvtepi32_ps( vxm2.i128 );
            yxm1.f128 = _mm_mul_ps( yxm1.f128, yfactor );
            yxm2.f128 = _mm_mul_ps( yxm2.f128, yfactor );
            yxm3.f128 = _mm_mul_ps( yxm3.f128, yfactor );
            yxm4.f128 = _mm_mul_ps( yxm4.f128, yfactor );
            uxm1.f128 = _mm_mul_ps( uxm1.f128, ufactor );
            uxm2.f128 = _mm_mul_ps( uxm2.f128, ufactor );
            vxm1.f128 = _mm_mul_ps( vxm1.f128, vfactor );
            vxm2.f128 = _mm_mul_ps( vxm2.f128, vfactor );

            // green
            XMM gxm1, gxm3;
            gxm1.f128 = _mm_sub_ps( yxm1.f128, uxm1.f128 );
            gxm1.f128 = _mm_sub_ps( gxm1.f128, vxm1.f128 );
            gxm3.f128 = _mm_sub_ps( yxm3.f128, uxm1.f128 );
            gxm3.f128 = _mm_sub_ps( gxm3.f128, vxm1.f128 );
            // blue
            uxm1.f128 = _mm_mul_ps( uxm1.f128, uscale );
            uxm3.f128 = _mm_add_ps( uxm1.f128, yxm3.f128 );
            uxm1.f128 = _mm_add_ps( uxm1.f128, yxm1.f128 );
            // red
            vxm1.f128 = _mm_mul_ps( vxm1.f128, vscale );
            vxm3.f128 = _mm_add_ps( vxm1.f128, yxm3.f128 );
            vxm1.f128 = _mm_add_ps( vxm1.f128, yxm1.f128 );
            // convert to int
            gxm1.i128 = _mm_cvtps_epi32( gxm1.f128 );
            gxm3.i128 = _mm_cvtps_epi32( gxm3.f128 );
            uxm1.i128 = _mm_cvtps_epi32( uxm1.f128 );
            uxm3.i128 = _mm_cvtps_epi32( uxm3.f128 );
            vxm1.i128 = _mm_cvtps_epi32( vxm1.f128 );
            vxm3.i128 = _mm_cvtps_epi32( vxm3.f128 );
            uxm1.i128 = _mm_packs_epi32( uxm1.i128, gxm1.i128 );
            uxm3.i128 = _mm_packs_epi32( uxm3.i128, gxm3.i128 );
            vxm1.i128 = _mm_packs_epi32( vxm1.i128, vxm1.i128 );
            vxm3.i128 = _mm_packs_epi32( vxm3.i128, vxm3.i128 );
            uxm1.i128 = _mm_packus_epi16( uxm1.i128, vxm1.i128 );
            uxm3.i128 = _mm_packus_epi16( uxm3.i128, vxm3.i128 );
            uxm1.i128 = _mm_shuffle_epi8( uxm1.i128, bgrShuffle16 );
            uxm3.i128 = _mm_shuffle_epi8( uxm3.i128, bgrShuffle16 );

            // output
            _mm_store_si128( (__m128i*)dst, uxm1.i128 );
            _mm_store_si128( (__m128i*)(dst + dstPitch), uxm3.i128 );

            // green
            gxm1.f128 = _mm_sub_ps( yxm2.f128, uxm2.f128 );
            gxm1.f128 = _mm_sub_ps( gxm1.f128, vxm2.f128 );
            gxm3.f128 = _mm_sub_ps( yxm4.f128, uxm2.f128 );
            gxm3.f128 = _mm_sub_ps( gxm3.f128, vxm2.f128 );
            // blue
            uxm2.f128 = _mm_mul_ps( uxm2.f128, uscale );
            uxm3.f128 = _mm_add_ps( uxm2.f128, yxm4.f128 );
            uxm2.f128 = _mm_add_ps( uxm2.f128, yxm2.f128 );
            // red
            vxm2.f128 = _mm_mul_ps( vxm2.f128, vscale );
            vxm3.f128 = _mm_add_ps( vxm2.f128, yxm4.f128 );
            vxm2.f128 = _mm_add_ps( vxm2.f128, yxm2.f128 );
            // convert to int
            gxm1.i128 = _mm_cvtps_epi32( gxm1.f128 );
            gxm3.i128 = _mm_cvtps_epi32( gxm3.f128 );
            uxm2.i128 = _mm_cvtps_epi32( uxm2.f128 );
            uxm3.i128 = _mm_cvtps_epi32( uxm3.f128 );
            vxm2.i128 = _mm_cvtps_epi32( vxm2.f128 );
            vxm3.i128 = _mm_cvtps_epi32( vxm3.f128 );
            uxm2.i128 = _mm_packs_epi32( uxm2.i128, gxm1.i128 );
            uxm3.i128 = _mm_packs_epi32( uxm3.i128, gxm3.i128 );
            vxm2.i128 = _mm_packs_epi32( vxm2.i128, vxm2.i128 );
            vxm3.i128 = _mm_packs_epi32( vxm3.i128, vxm3.i128 );
            uxm2.i128 = _mm_packus_epi16( uxm2.i128, vxm2.i128 );
            uxm3.i128 = _mm_packus_epi16( uxm3.i128, vxm3.i128 );
            uxm2.i128 = _mm_shuffle_epi8( uxm2.i128, bgrShuffle16 );
            uxm3.i128 = _mm_shuffle_epi8( uxm3.i128, bgrShuffle16 );

            // output
            _mm_store_si128( (__m128i*)(dst + 16), uxm2.i128 );
            _mm_store_si128( (__m128i*)(dst + dstPitch + 16), uxm3.i128 );
            dst += 32;
        }
        for( j = j * 4; j < uvWidth; j++ )
        {
            uint8_t u2 = (urow[j] + urow[j+1] + 1) >> 1;
            uint8_t v2 = (vrow[j] + vrow[j+1] + 1) >> 1;

            *(int*)dst = Yuv2Bgr( yrow[2*j], urow[j], vrow[j] );
            *(int*)(dst + 4) = Yuv2Bgr( yrow[2*j+1], u2, v2 );
            *(int*)(dst + dstPitch) = Yuv2Bgr( yrow[2*j+yPitch], urow[j], vrow[j] );
            *(int*)(dst + dstPitch + 4) = Yuv2Bgr( yrow[2*j+yPitch+1], u2, v2 );
            dst += 8;
        }

        urow[uvWidth] = uEdge;
        vrow[uvWidth] = vEdge;
        yrow += yPitch * 2;
        urow += pFrame->pitch[1];
        vrow += pFrame->pitch[2];
    }

#ifdef CHECK_CVT_TIME
    QueryPerformanceCounter( &t2 );
    dprintf( "yuv420 to rgb %dx%d : %0.3f ms\n", pFrame->width[0], yHeight, 1000.0 * (t2.QuadPart-t1.QuadPart) / (double)fre.QuadPart );
#endif
}

void ConvertYUV422_BGRA( YUVFrame* pFrame, uint8_t* pDst, int dstPitch )
{
    DASSERT( (dstPitch & 15) == 0 );
#ifdef CHECK_CVT_TIME
    LARGE_INTEGER fre, t1, t2;
    QueryPerformanceFrequency( &fre );
    QueryPerformanceCounter( &t1 );
#endif

    uint8_t* yrow = pFrame->planes[0];
    uint8_t* urow = pFrame->planes[1];
    uint8_t* vrow = pFrame->planes[2];
    const int yWidth8 = pFrame->width[0] >> 3;
    const int yHeight = pFrame->height[0];
    const int uvWidth = pFrame->width[1];

    // setup transform factors
    __m128  yfactor, ufactor, vfactor, uscale, vscale;
    __m128i bgrShuffle16 = _mm_load_si128( (__m128i*)s_bgraShuffle );
    YCbCr_BGR_Func Yuv2Bgr = NULL;

    if( yHeight >= 720 )   // HD
    {
        yfactor = _mm_load_ps( s_Yfactors );
        ufactor = _mm_load_ps( s_UfactorsHD );
        vfactor = _mm_load_ps( s_VfactorsHD );
        uscale = _mm_load_ps( s_UScaleHD );
        vscale = _mm_load_ps( s_VScaleHD );
        Yuv2Bgr = &YCbCr_BGR_709;
    }
    else                    // SD
    {
        yfactor = _mm_load_ps( s_Yfactors );
        ufactor = _mm_load_ps( s_UfactorsSD );
        vfactor = _mm_load_ps( s_VfactorsSD );
        uscale = _mm_load_ps( s_UScaleSD );
        vscale = _mm_load_ps( s_VScaleSD );
        Yuv2Bgr = &YCbCr_BGR_601;
    }

    // 转化 4:2:0, 4:2:2 到 4:4:4 取决于当初的采样算法,
    // 由于人眼对颜色不敏感, 简化起见, 垂直方向不进行滤波, 水平方向取平均
    XMM yxm1, yxm2, uxm1, uxm2, vxm1, vxm2;

    for( int i = 0; i < yHeight; i++ )
    {
        uint8_t* dst = pDst + i * dstPitch;

        // 设置边界点, 方便滤波
        uint8_t uEdge = urow[uvWidth];
        uint8_t vEdge = vrow[uvWidth];
        urow[uvWidth] = urow[uvWidth-1];
        vrow[uvWidth] = vrow[uvWidth-1];

        // 使用 sse4, 每次处理 8 个点
        int j = 0;
        for( ; j < yWidth8; j++ )
        {
            yxm1.i128 = _mm_loadl_epi64( (__m128i*)(yrow + j*8) );
            uxm1.i128 = _mm_loadl_epi64( (__m128i*)(urow + j*4) );
            vxm1.i128 = _mm_loadl_epi64( (__m128i*)(vrow + j*4) );

            // UV 水平方向不存在的点用左右点的均值替代
            uxm2.i128 = _mm_srli_si128( uxm1.i128, 1 );
            vxm2.i128 = _mm_srli_si128( vxm1.i128, 1 );
            uxm2.i128 = _mm_avg_epu8( uxm2.i128, uxm1.i128 );
            vxm2.i128 = _mm_avg_epu8( vxm2.i128, vxm1.i128 );
            uxm1.i128 = _mm_unpacklo_epi8( uxm1.i128, uxm2.i128 );
            vxm1.i128 = _mm_unpacklo_epi8( vxm1.i128, vxm2.i128 );
            yxm2.i128 = _mm_shuffle_epi32( yxm1.i128, 0x55 );
            uxm2.i128 = _mm_shuffle_epi32( uxm1.i128, 0x55 );
            vxm2.i128 = _mm_shuffle_epi32( vxm1.i128, 0x55 );

            yxm1.i128 = _mm_cvtepu8_epi32( yxm1.i128 );
            yxm2.i128 = _mm_cvtepu8_epi32( yxm2.i128 );
            uxm1.i128 = _mm_cvtepu8_epi32( uxm1.i128 );
            uxm2.i128 = _mm_cvtepu8_epi32( uxm2.i128 );
            vxm1.i128 = _mm_cvtepu8_epi32( vxm1.i128 );
            vxm2.i128 = _mm_cvtepu8_epi32( vxm2.i128 );
            __m128i xm0 = _mm_cvtsi32_si128( 16 );
            xm0 = _mm_shuffle_epi32( xm0, 0 );
            yxm1.i128 = _mm_sub_epi32( yxm1.i128, xm0 );
            yxm2.i128 = _mm_sub_epi32( yxm2.i128, xm0 );
            xm0 = _mm_slli_epi32( xm0, 3 );                 // 128
            uxm1.i128 = _mm_sub_epi32( uxm1.i128, xm0 );
            uxm2.i128 = _mm_sub_epi32( uxm2.i128, xm0 );
            vxm1.i128 = _mm_sub_epi32( vxm1.i128, xm0 );
            vxm2.i128 = _mm_sub_epi32( vxm2.i128, xm0 );

            // cvt to float, mul factor
            yxm1.f128 = _mm_cvtepi32_ps( yxm1.i128 );
            yxm2.f128 = _mm_cvtepi32_ps( yxm2.i128 );
            uxm1.f128 = _mm_cvtepi32_ps( uxm1.i128 );
            uxm2.f128 = _mm_cvtepi32_ps( uxm2.i128 );
            vxm1.f128 = _mm_cvtepi32_ps( vxm1.i128 );
            vxm2.f128 = _mm_cvtepi32_ps( vxm2.i128 );
            yxm1.f128 = _mm_mul_ps( yxm1.f128, yfactor );
            yxm2.f128 = _mm_mul_ps( yxm2.f128, yfactor );
            uxm1.f128 = _mm_mul_ps( uxm1.f128, ufactor );
            uxm2.f128 = _mm_mul_ps( uxm2.f128, ufactor );
            vxm1.f128 = _mm_mul_ps( vxm1.f128, vfactor );
            vxm2.f128 = _mm_mul_ps( vxm2.f128, vfactor );

            // green
            XMM gxm1;
            gxm1.f128 = _mm_sub_ps( yxm1.f128, uxm1.f128 );
            gxm1.f128 = _mm_sub_ps( gxm1.f128, vxm1.f128 );
            // blue
            uxm1.f128 = _mm_mul_ps( uxm1.f128, uscale );
            uxm1.f128 = _mm_add_ps( uxm1.f128, yxm1.f128 );
            // red
            vxm1.f128 = _mm_mul_ps( vxm1.f128, vscale );
            vxm1.f128 = _mm_add_ps( vxm1.f128, yxm1.f128 );
            // convert to int
            gxm1.i128 = _mm_cvtps_epi32( gxm1.f128 );
            uxm1.i128 = _mm_cvtps_epi32( uxm1.f128 );
            vxm1.i128 = _mm_cvtps_epi32( vxm1.f128 );
            uxm1.i128 = _mm_packs_epi32( uxm1.i128, gxm1.i128 );
            vxm1.i128 = _mm_packs_epi32( vxm1.i128, vxm1.i128 );
            uxm1.i128 = _mm_packus_epi16( uxm1.i128, vxm1.i128 );
            uxm1.i128 = _mm_shuffle_epi8( uxm1.i128, bgrShuffle16 );

            // output
            _mm_store_si128( (__m128i*)dst, uxm1.i128 );

            // green
            gxm1.f128 = _mm_sub_ps( yxm2.f128, uxm2.f128 );
            gxm1.f128 = _mm_sub_ps( gxm1.f128, vxm2.f128 );
            // blue
            uxm2.f128 = _mm_mul_ps( uxm2.f128, uscale );
            uxm2.f128 = _mm_add_ps( uxm2.f128, yxm2.f128 );
            // red
            vxm2.f128 = _mm_mul_ps( vxm2.f128, vscale );
            vxm2.f128 = _mm_add_ps( vxm2.f128, yxm2.f128 );
            // convert to int
            gxm1.i128 = _mm_cvtps_epi32( gxm1.f128 );
            uxm2.i128 = _mm_cvtps_epi32( uxm2.f128 );
            vxm2.i128 = _mm_cvtps_epi32( vxm2.f128 );
            uxm2.i128 = _mm_packs_epi32( uxm2.i128, gxm1.i128 );
            vxm2.i128 = _mm_packs_epi32( vxm2.i128, vxm2.i128 );
            uxm2.i128 = _mm_packus_epi16( uxm2.i128, vxm2.i128 );
            uxm2.i128 = _mm_shuffle_epi8( uxm2.i128, bgrShuffle16 );

            // output
            _mm_store_si128( (__m128i*)(dst + 16), uxm2.i128 );
            dst += 32;
        }
        for( j = j * 4; j < uvWidth; j++ )
        {
            uint8_t u2 = (urow[j] + urow[j+1] + 1) >> 1;
            uint8_t v2 = (vrow[j] + vrow[j+1] + 1) >> 1;

            *(int*)dst = Yuv2Bgr( yrow[2*j], urow[j], vrow[j] );
            *(int*)(dst + 4) = Yuv2Bgr( yrow[2*j+1], u2, v2 );
            dst += 8;
        }

        urow[uvWidth] = uEdge;
        vrow[uvWidth] = vEdge;
        yrow += pFrame->pitch[0];
        urow += pFrame->pitch[1];
        vrow += pFrame->pitch[2];
    }

#ifdef CHECK_CVT_TIME
    QueryPerformanceCounter( &t2 );
    dprintf( "yuv422 to rgb %dx%d : %0.3f ms\n", pFrame->width[0], yHeight, 1000.0 * (t2.QuadPart-t1.QuadPart) / (double)fre.QuadPart );
#endif
}

void ConvertYUV444_BGRA( YUVFrame* pFrame, uint8_t* pDst, int dstPitch )
{
    DASSERT( (dstPitch & 15) == 0 );
#ifdef CHECK_CVT_TIME
    LARGE_INTEGER fre, t1, t2;
    QueryPerformanceFrequency( &fre );
    QueryPerformanceCounter( &t1 );
#endif

    uint8_t* yrow = pFrame->planes[0];
    uint8_t* urow = pFrame->planes[1];
    uint8_t* vrow = pFrame->planes[2];
    const int width = pFrame->width[0];
    const int width8 = pFrame->width[0] >> 3;
    const int height = pFrame->height[0];
    const int pitch = pFrame->pitch[0];

    // setup transform factors
    __m128  yfactor, ufactor, vfactor, uscale, vscale;
    __m128i bgrShuffle16 = _mm_load_si128( (__m128i*)s_bgraShuffle );
    YCbCr_BGR_Func Yuv2Bgr = NULL;

    if( height >= 720 )   // HD
    {
        yfactor = _mm_load_ps( s_Yfactors );
        ufactor = _mm_load_ps( s_UfactorsHD );
        vfactor = _mm_load_ps( s_VfactorsHD );
        uscale = _mm_load_ps( s_UScaleHD );
        vscale = _mm_load_ps( s_VScaleHD );
        Yuv2Bgr = &YCbCr_BGR_709;
    }
    else                    // SD
    {
        yfactor = _mm_load_ps( s_Yfactors );
        ufactor = _mm_load_ps( s_UfactorsSD );
        vfactor = _mm_load_ps( s_VfactorsSD );
        uscale = _mm_load_ps( s_UScaleSD );
        vscale = _mm_load_ps( s_VScaleSD );
        Yuv2Bgr = &YCbCr_BGR_601;
    }

    XMM yxm1, yxm2, uxm1, uxm2, vxm1, vxm2;

    for( int i = 0; i < height; i++ )
    {
        uint8_t* dst = pDst + i * dstPitch;

        // 使用 sse4, 每次处理 8 个点
        int j = 0;
        for( ; j < width8; j++ )
        {
            __m128i xm0 = _mm_cvtsi32_si128( 16 );
            xm0 = _mm_shuffle_epi32( xm0, 0 );
            yxm1.i128 = _mm_loadl_epi64( (__m128i*)(yrow + j*8) );
            uxm1.i128 = _mm_loadl_epi64( (__m128i*)(urow + j*8) );
            vxm1.i128 = _mm_loadl_epi64( (__m128i*)(vrow + j*8) );
            yxm2.i128 = _mm_shuffle_epi32( yxm1.i128, 0x55 );
            uxm2.i128 = _mm_shuffle_epi32( uxm1.i128, 0x55 );
            vxm2.i128 = _mm_shuffle_epi32( vxm1.i128, 0x55 );
            yxm1.i128 = _mm_cvtepu8_epi32( yxm1.i128 );
            yxm2.i128 = _mm_cvtepu8_epi32( yxm2.i128 );
            uxm1.i128 = _mm_cvtepu8_epi32( uxm1.i128 );
            uxm2.i128 = _mm_cvtepu8_epi32( uxm2.i128 );
            vxm1.i128 = _mm_cvtepu8_epi32( vxm1.i128 );
            vxm2.i128 = _mm_cvtepu8_epi32( vxm2.i128 );
            yxm1.i128 = _mm_sub_epi32( yxm1.i128, xm0 );
            yxm2.i128 = _mm_sub_epi32( yxm2.i128, xm0 );
            xm0 = _mm_slli_epi32( xm0, 3 );                 // 128
            uxm1.i128 = _mm_sub_epi32( uxm1.i128, xm0 );
            uxm2.i128 = _mm_sub_epi32( uxm2.i128, xm0 );
            vxm1.i128 = _mm_sub_epi32( vxm1.i128, xm0 );
            vxm2.i128 = _mm_sub_epi32( vxm2.i128, xm0 );

            // cvt to float, mul factor
            yxm1.f128 = _mm_cvtepi32_ps( yxm1.i128 );
            yxm2.f128 = _mm_cvtepi32_ps( yxm2.i128 );
            uxm1.f128 = _mm_cvtepi32_ps( uxm1.i128 );
            uxm2.f128 = _mm_cvtepi32_ps( uxm2.i128 );
            vxm1.f128 = _mm_cvtepi32_ps( vxm1.i128 );
            vxm2.f128 = _mm_cvtepi32_ps( vxm2.i128 );
            yxm1.f128 = _mm_mul_ps( yxm1.f128, yfactor );
            yxm2.f128 = _mm_mul_ps( yxm2.f128, yfactor );
            uxm1.f128 = _mm_mul_ps( uxm1.f128, ufactor );
            uxm2.f128 = _mm_mul_ps( uxm2.f128, ufactor );
            vxm1.f128 = _mm_mul_ps( vxm1.f128, vfactor );
            vxm2.f128 = _mm_mul_ps( vxm2.f128, vfactor );

            // green
            XMM gxm1;
            gxm1.f128 = _mm_sub_ps( yxm1.f128, uxm1.f128 );
            gxm1.f128 = _mm_sub_ps( gxm1.f128, vxm1.f128 );
            // blue
            uxm1.f128 = _mm_mul_ps( uxm1.f128, uscale );
            uxm1.f128 = _mm_add_ps( uxm1.f128, yxm1.f128 );
            // red
            vxm1.f128 = _mm_mul_ps( vxm1.f128, vscale );
            vxm1.f128 = _mm_add_ps( vxm1.f128, yxm1.f128 );
            // convert to int
            gxm1.i128 = _mm_cvtps_epi32( gxm1.f128 );
            uxm1.i128 = _mm_cvtps_epi32( uxm1.f128 );
            vxm1.i128 = _mm_cvtps_epi32( vxm1.f128 );
            uxm1.i128 = _mm_packs_epi32( uxm1.i128, gxm1.i128 );
            vxm1.i128 = _mm_packs_epi32( vxm1.i128, vxm1.i128 );
            uxm1.i128 = _mm_packus_epi16( uxm1.i128, vxm1.i128 );
            uxm1.i128 = _mm_shuffle_epi8( uxm1.i128, bgrShuffle16 );

            // output
            _mm_store_si128( (__m128i*)dst, uxm1.i128 );

            // green
            gxm1.f128 = _mm_sub_ps( yxm2.f128, uxm2.f128 );
            gxm1.f128 = _mm_sub_ps( gxm1.f128, vxm2.f128 );
            // blue
            uxm2.f128 = _mm_mul_ps( uxm2.f128, uscale );
            uxm2.f128 = _mm_add_ps( uxm2.f128, yxm2.f128 );
            // red
            vxm2.f128 = _mm_mul_ps( vxm2.f128, vscale );
            vxm2.f128 = _mm_add_ps( vxm2.f128, yxm2.f128 );
            // convert to int
            gxm1.i128 = _mm_cvtps_epi32( gxm1.f128 );
            uxm2.i128 = _mm_cvtps_epi32( uxm2.f128 );
            vxm2.i128 = _mm_cvtps_epi32( vxm2.f128 );
            uxm2.i128 = _mm_packs_epi32( uxm2.i128, gxm1.i128 );
            vxm2.i128 = _mm_packs_epi32( vxm2.i128, vxm2.i128 );
            uxm2.i128 = _mm_packus_epi16( uxm2.i128, vxm2.i128 );
            uxm2.i128 = _mm_shuffle_epi8( uxm2.i128, bgrShuffle16 );

            // output
            _mm_store_si128( (__m128i*)(dst + 16), uxm2.i128 );
            dst += 32;
        }
        for( j = j * 8; j < width; j++ )
        {
            *(int*)dst = Yuv2Bgr( yrow[j], urow[j], vrow[j] );
            dst += 4;
        }

        yrow += pFrame->pitch[0];
        urow += pFrame->pitch[1];
        vrow += pFrame->pitch[2];
    }

#ifdef CHECK_CVT_TIME
    QueryPerformanceCounter( &t2 );
    dprintf( "yuv444 to rgb %dx%d : %0.3f ms\n", pFrame->width[0], height, 1000.0 * (t2.QuadPart-t1.QuadPart) / (double)fre.QuadPart );
#endif
}

//=========================================================================================================================================
void FastConvertYUV420_BGRA( YUVFrame* pFrame, uint8_t* pDst, int dstPitch )
{
    DASSERT( (dstPitch & 15) == 0 );
#ifdef CHECK_CVT_TIME
    LARGE_INTEGER fre, t1, t2;
    QueryPerformanceFrequency( &fre );
    QueryPerformanceCounter( &t1 );
#endif

    uint8_t* yrow = pFrame->planes[0];
    uint8_t* urow = pFrame->planes[1];
    uint8_t* vrow = pFrame->planes[2];
    const int width8 = pFrame->width[0] >> 3;
    const int height = pFrame->height[0];
    const int pitch = pFrame->pitch[0];
    const int uvWidth = pFrame->width[1];

    // setup transform factors
    __m128i ycoeff, ucoeff, ucoeffm, vcoeff, vcoeffm;
    YCbCr_BGR_Func Yuv2Bgr = NULL;

    if( height >= 720 )   // HD
    {
        ycoeff = _mm_load_si128( (__m128i*)s_YCoeff );
        ucoeff = _mm_load_si128( (__m128i*)s_UCoeffHD );
        ucoeffm = _mm_load_si128( (__m128i*)s_UCoeffHDm );
        vcoeff = _mm_load_si128( (__m128i*)s_VCoeffHD );
        vcoeffm = _mm_load_si128( (__m128i*)s_VCoeffHDm );
        Yuv2Bgr = &YCbCr_BGR_709_int32;
    }
    else                    // SD
    {
        ycoeff = _mm_load_si128( (__m128i*)s_YCoeff );
        ucoeff = _mm_load_si128( (__m128i*)s_UCoeffSD );
        ucoeffm = _mm_load_si128( (__m128i*)s_UCoeffSDm );
        vcoeff = _mm_load_si128( (__m128i*)s_VCoeffSD );
        vcoeffm = _mm_load_si128( (__m128i*)s_VCoeffSDm );
        Yuv2Bgr = &YCbCr_BGR_709_int32;
    }

    __m128i yxm, uxm, vxm, rxm, bxm, xmb, xmr;
    __m128i minus16 = _mm_load_si128( (__m128i*)s_Minus16 );
    __m128i minus128 = _mm_load_si128( (__m128i*)s_Minus128 );
    __m128i round32 = _mm_load_si128( (__m128i*)s_Round32 );

    for( int i = 0; i < height; i += 2 )
    {
        uint8_t* dst = pDst + i * dstPitch;

        // 设置边界点, 方便滤波
        uint8_t uEdge = urow[uvWidth];
        uint8_t vEdge = vrow[uvWidth];
        urow[uvWidth] = urow[uvWidth-1];
        vrow[uvWidth] = vrow[uvWidth-1];

        int j = 0;
        for( ; j < width8; j++ )
        {
            __m128i zero = _mm_setzero_si128();
            yxm = _mm_loadl_epi64( (__m128i*)(yrow + j*8) );
            uxm = _mm_loadl_epi64( (__m128i*)(urow + j*4) );
            vxm = _mm_loadl_epi64( (__m128i*)(vrow + j*4) );
            yxm = _mm_unpacklo_epi8( yxm, zero );
            uxm = _mm_unpacklo_epi8( uxm, zero );
            vxm = _mm_unpacklo_epi8( vxm, zero );
            bxm = _mm_srli_si128( uxm, 2 );         // UV 水平方向不存在的点用左右点的均值替代
            rxm = _mm_srli_si128( vxm, 2 );
            bxm = _mm_avg_epu16( bxm, uxm );
            rxm = _mm_avg_epu16( rxm, vxm );
            uxm = _mm_unpacklo_epi16( uxm, bxm );
            vxm = _mm_unpacklo_epi16( vxm, rxm );
            yxm = _mm_add_epi16( yxm, minus16 );
            uxm = _mm_add_epi16( uxm, minus128 );
            vxm = _mm_add_epi16( vxm, minus128 );
            yxm = _mm_mullo_epi16( yxm, ycoeff );
            yxm = _mm_add_epi16( yxm, round32 );
            bxm = _mm_mullo_epi16( uxm, ucoeff );
            rxm = _mm_mullo_epi16( vxm, vcoeff );
            uxm = _mm_mullo_epi16( uxm, ucoeffm );
            vxm = _mm_mullo_epi16( vxm, vcoeffm );
            xmb = _mm_adds_epi16( yxm, bxm );
            xmr = _mm_adds_epi16( yxm, rxm );
            yxm = _mm_adds_epi16( yxm, uxm );
            yxm = _mm_adds_epi16( yxm, vxm );
            xmb = _mm_srai_epi16( xmb, 6 );             // blue           
            xmr = _mm_srai_epi16( xmr, 6 );             // red
            yxm = _mm_srai_epi16( yxm, 6 );             // green
            xmb = _mm_packus_epi16( xmb, xmb );
            xmr = _mm_packus_epi16( xmr, xmr );
            yxm = _mm_packus_epi16( yxm, yxm );
            xmb = _mm_unpacklo_epi8( xmb, yxm );        // g b g b ...
            xmr = _mm_unpacklo_epi8( xmr, zero );       // 0 r 0 r ...
            yxm = _mm_unpackhi_epi16( xmb, xmr );
            xmb = _mm_unpacklo_epi16( xmb, xmr );

            // output
            _mm_store_si128( (__m128i*)(dst), xmb );
            _mm_store_si128( (__m128i*)(dst + 16), yxm );

            // 2nd row
            yxm = _mm_loadl_epi64( (__m128i*)(yrow + j*8 + pitch) );
            yxm = _mm_unpacklo_epi8( yxm, zero );
            yxm = _mm_add_epi16( yxm, minus16 );
            yxm = _mm_mullo_epi16( yxm, ycoeff );
            yxm = _mm_add_epi16( yxm, round32 );
            bxm = _mm_adds_epi16( bxm, yxm );
            rxm = _mm_adds_epi16( rxm, yxm );
            yxm = _mm_adds_epi16( yxm, uxm );
            yxm = _mm_adds_epi16( yxm, vxm );
            bxm = _mm_srai_epi16( bxm, 6 );             // blue           
            rxm = _mm_srai_epi16( rxm, 6 );             // red
            yxm = _mm_srai_epi16( yxm, 6 );             // green
            bxm = _mm_packus_epi16( bxm, bxm );
            rxm = _mm_packus_epi16( rxm, rxm );
            yxm = _mm_packus_epi16( yxm, yxm );
            bxm = _mm_unpacklo_epi8( bxm, yxm );        // g b g b ...
            rxm = _mm_unpacklo_epi8( rxm, zero );       // 0 r 0 r ...
            yxm = _mm_unpackhi_epi16( bxm, rxm );
            bxm = _mm_unpacklo_epi16( bxm, rxm );

            // output
            _mm_store_si128( (__m128i*)(dst + dstPitch), bxm );
            _mm_store_si128( (__m128i*)(dst + dstPitch + 16), yxm );
            dst += 32;
        }
        for( j = j * 4; j < uvWidth; j++ )
        {
            uint8_t u2 = (urow[j] + urow[j+1] + 1) >> 1;
            uint8_t v2 = (vrow[j] + vrow[j+1] + 1) >> 1;

            *(int*)dst = Yuv2Bgr( yrow[2*j], urow[j], vrow[j] );
            *(int*)(dst + 4) = Yuv2Bgr( yrow[2*j+1], u2, v2 );
            *(int*)(dst + dstPitch) = Yuv2Bgr( yrow[2*j+pitch], urow[j], vrow[j] );
            *(int*)(dst + dstPitch + 4) = Yuv2Bgr( yrow[2*j+pitch+1], u2, v2 );
            dst += 8;
        }

        urow[uvWidth] = uEdge;
        vrow[uvWidth] = vEdge;

        yrow += pitch * 2;
        urow += pFrame->pitch[1];
        vrow += pFrame->pitch[2];
    }

#ifdef CHECK_CVT_TIME
    QueryPerformanceCounter( &t2 );
    dprintf( "yuv420 to rgb %dx%d : %0.3f ms\n", pFrame->width[0], height, 1000.0 * (t2.QuadPart-t1.QuadPart) / (double)fre.QuadPart );
#endif
}

void FastConvertYUV422_BGRA( YUVFrame* pFrame, uint8_t* pDst, int dstPitch )
{
    DASSERT( (dstPitch & 15) == 0 );
#ifdef CHECK_CVT_TIME
    LARGE_INTEGER fre, t1, t2;
    QueryPerformanceFrequency( &fre );
    QueryPerformanceCounter( &t1 );
#endif

    uint8_t* yrow = pFrame->planes[0];
    uint8_t* urow = pFrame->planes[1];
    uint8_t* vrow = pFrame->planes[2];
    const int width8 = pFrame->width[0] >> 3;
    const int height = pFrame->height[0];
    const int uvWidth = pFrame->width[1];

    // setup transform factors
    __m128i ycoeff, ucoeff, ucoeffm, vcoeff, vcoeffm;
    YCbCr_BGR_Func Yuv2Bgr = NULL;

    if( height >= 720 )   // HD
    {
        ycoeff = _mm_load_si128( (__m128i*)s_YCoeff );
        ucoeff = _mm_load_si128( (__m128i*)s_UCoeffHD );
        ucoeffm = _mm_load_si128( (__m128i*)s_UCoeffHDm );
        vcoeff = _mm_load_si128( (__m128i*)s_VCoeffHD );
        vcoeffm = _mm_load_si128( (__m128i*)s_VCoeffHDm );
        Yuv2Bgr = &YCbCr_BGR_709_int32;
    }
    else                    // SD
    {
        ycoeff = _mm_load_si128( (__m128i*)s_YCoeff );
        ucoeff = _mm_load_si128( (__m128i*)s_UCoeffSD );
        ucoeffm = _mm_load_si128( (__m128i*)s_UCoeffSDm );
        vcoeff = _mm_load_si128( (__m128i*)s_VCoeffSD );
        vcoeffm = _mm_load_si128( (__m128i*)s_VCoeffSDm );
        Yuv2Bgr = &YCbCr_BGR_709_int32;
    }

    __m128i yxm, uxm, vxm, rxm, bxm;
    __m128i minus128 = _mm_load_si128( (__m128i*)s_Minus128 );

    for( int i = 0; i < height; i++ )
    {
        uint8_t* dst = pDst + i * dstPitch;

        // 设置边界点, 方便滤波
        uint8_t uEdge = urow[uvWidth];
        uint8_t vEdge = vrow[uvWidth];
        urow[uvWidth] = urow[uvWidth-1];
        vrow[uvWidth] = vrow[uvWidth-1];

        int j = 0;
        for( ; j < width8; j++ )
        {
            __m128i zero = _mm_setzero_si128();
            yxm = _mm_loadl_epi64( (__m128i*)(yrow + j*8) );
            uxm = _mm_loadl_epi64( (__m128i*)(urow + j*4) );
            vxm = _mm_loadl_epi64( (__m128i*)(vrow + j*4) );
            yxm = _mm_unpacklo_epi8( yxm, zero );
            uxm = _mm_unpacklo_epi8( uxm, zero );
            vxm = _mm_unpacklo_epi8( vxm, zero );
            bxm = _mm_srli_si128( uxm, 2 );         // UV 水平方向不存在的点用左右点的均值替代
            rxm = _mm_srli_si128( vxm, 2 );
            bxm = _mm_avg_epu16( bxm, uxm );
            rxm = _mm_avg_epu16( rxm, vxm );
            uxm = _mm_unpacklo_epi16( uxm, bxm );
            vxm = _mm_unpacklo_epi16( vxm, rxm );
            yxm = _mm_add_epi16( yxm, *(__m128i*)s_Minus16 );
            uxm = _mm_add_epi16( uxm, minus128 );
            vxm = _mm_add_epi16( vxm, minus128 );
            yxm = _mm_mullo_epi16( yxm, ycoeff );
            yxm = _mm_add_epi16( yxm, *(__m128i*)s_Round32 );

            bxm = _mm_mullo_epi16( uxm, ucoeff );
            rxm = _mm_mullo_epi16( vxm, vcoeff );
            uxm = _mm_mullo_epi16( uxm, ucoeffm );
            vxm = _mm_mullo_epi16( vxm, vcoeffm );
            bxm = _mm_adds_epi16( bxm, yxm );
            rxm = _mm_adds_epi16( rxm, yxm );
            yxm = _mm_adds_epi16( yxm, uxm );
            yxm = _mm_adds_epi16( yxm, vxm );
            bxm = _mm_srai_epi16( bxm, 6 );             // blue           
            rxm = _mm_srai_epi16( rxm, 6 );             // red
            yxm = _mm_srai_epi16( yxm, 6 );             // green
            bxm = _mm_packus_epi16( bxm, bxm );
            rxm = _mm_packus_epi16( rxm, rxm );
            yxm = _mm_packus_epi16( yxm, yxm );
            bxm = _mm_unpacklo_epi8( bxm, yxm );        // g b g b ...
            rxm = _mm_unpacklo_epi8( rxm, zero );       // 0 r 0 r ...
            yxm = _mm_unpackhi_epi16( bxm, rxm );
            bxm = _mm_unpacklo_epi16( bxm, rxm );

            // output
            _mm_store_si128( (__m128i*)(dst), bxm );
            _mm_store_si128( (__m128i*)(dst + 16), yxm );
            dst += 32;
        }
        for( j = j * 4; j < uvWidth; j++ )
        {
            uint8_t u2 = (urow[j] + urow[j+1] + 1) >> 1;
            uint8_t v2 = (vrow[j] + vrow[j+1] + 1) >> 1;

            *(int*)dst = Yuv2Bgr( yrow[2*j], urow[j], vrow[j] );
            *(int*)(dst + 4) = Yuv2Bgr( yrow[2*j+1], u2, v2 );
            dst += 8;
        }

        urow[uvWidth] = uEdge;
        vrow[uvWidth] = vEdge;

        yrow += pFrame->pitch[0];
        urow += pFrame->pitch[1];
        vrow += pFrame->pitch[2];
    }

#ifdef CHECK_CVT_TIME
    QueryPerformanceCounter( &t2 );
    dprintf( "yuv422 to rgb %dx%d : %0.3f ms\n", pFrame->width[0], height, 1000.0 * (t2.QuadPart-t1.QuadPart) / (double)fre.QuadPart );
#endif
}

void FastConvertYUV444_BGRA( YUVFrame* pFrame, uint8_t* pDst, int dstPitch )
{
    DASSERT( (dstPitch & 15) == 0 );
#ifdef CHECK_CVT_TIME
    LARGE_INTEGER fre, t1, t2;
    QueryPerformanceFrequency( &fre );
    QueryPerformanceCounter( &t1 );
#endif

    uint8_t* yrow = pFrame->planes[0];
    uint8_t* urow = pFrame->planes[1];
    uint8_t* vrow = pFrame->planes[2];
    const int width = pFrame->width[0];
    const int width8 = pFrame->width[0] >> 3;
    const int height = pFrame->height[0];

    // setup transform factors
    __m128i ycoeff, ucoeff, ucoeffm, vcoeff, vcoeffm;
    YCbCr_BGR_Func Yuv2Bgr = NULL;

    if( height >= 720 )   // HD
    {
        ycoeff = _mm_load_si128( (__m128i*)s_YCoeff );
        ucoeff = _mm_load_si128( (__m128i*)s_UCoeffHD );
        ucoeffm = _mm_load_si128( (__m128i*)s_UCoeffHDm );
        vcoeff = _mm_load_si128( (__m128i*)s_VCoeffHD );
        vcoeffm = _mm_load_si128( (__m128i*)s_VCoeffHDm );
        Yuv2Bgr = &YCbCr_BGR_709_int32;
    }
    else                    // SD
    {
        ycoeff = _mm_load_si128( (__m128i*)s_YCoeff );
        ucoeff = _mm_load_si128( (__m128i*)s_UCoeffSD );
        ucoeffm = _mm_load_si128( (__m128i*)s_UCoeffSDm );
        vcoeff = _mm_load_si128( (__m128i*)s_VCoeffSD );
        vcoeffm = _mm_load_si128( (__m128i*)s_VCoeffSDm );
        Yuv2Bgr = &YCbCr_BGR_709_int32;
    }

    __m128i yxm, uxm, vxm, rxm, bxm;
    __m128i minus128 = _mm_load_si128( (__m128i*)s_Minus128 );

    for( int i = 0; i < height; i++ )
    {
        uint8_t* dst = pDst + i * dstPitch;

        int j = 0;
        for( ; j < width8; j++ )
        {
            __m128i zero = _mm_setzero_si128();
            yxm = _mm_loadl_epi64( (__m128i*)(yrow + j*8) );
            uxm = _mm_loadl_epi64( (__m128i*)(urow + j*8) );
            vxm = _mm_loadl_epi64( (__m128i*)(vrow + j*8) );
            yxm = _mm_unpacklo_epi8( yxm, zero );
            uxm = _mm_unpacklo_epi8( uxm, zero );
            vxm = _mm_unpacklo_epi8( vxm, zero );
            yxm = _mm_add_epi16( yxm, *(__m128i*)s_Minus16 );
            uxm = _mm_add_epi16( uxm, minus128 );
            vxm = _mm_add_epi16( vxm, minus128 );
            yxm = _mm_mullo_epi16( yxm, ycoeff );
            yxm = _mm_add_epi16( yxm, *(__m128i*)s_Round32 );

            bxm = _mm_mullo_epi16( uxm, ucoeff );
            rxm = _mm_mullo_epi16( vxm, vcoeff );
            uxm = _mm_mullo_epi16( uxm, ucoeffm );
            vxm = _mm_mullo_epi16( vxm, vcoeffm );
            bxm = _mm_adds_epi16( bxm, yxm );
            rxm = _mm_adds_epi16( rxm, yxm );
            yxm = _mm_adds_epi16( yxm, uxm );
            yxm = _mm_adds_epi16( yxm, vxm );
            bxm = _mm_srai_epi16( bxm, 6 );             // blue           
            rxm = _mm_srai_epi16( rxm, 6 );             // red
            yxm = _mm_srai_epi16( yxm, 6 );             // green
            bxm = _mm_packus_epi16( bxm, bxm );
            rxm = _mm_packus_epi16( rxm, rxm );
            yxm = _mm_packus_epi16( yxm, yxm );
            bxm = _mm_unpacklo_epi8( bxm, yxm );        // g b g b ...
            rxm = _mm_unpacklo_epi8( rxm, zero );       // 0 r 0 r ...
            yxm = _mm_unpackhi_epi16( bxm, rxm );
            bxm = _mm_unpacklo_epi16( bxm, rxm );

            // output
            _mm_store_si128( (__m128i*)(dst), bxm );
            _mm_store_si128( (__m128i*)(dst + 16), yxm );
            dst += 32;
        }
        for( j = j * 8; j < width; j++ )
        {
            *(int*)dst = Yuv2Bgr( yrow[j], urow[j], vrow[j] );
            dst += 4;
        }

        yrow += pFrame->pitch[0];
        urow += pFrame->pitch[1];
        vrow += pFrame->pitch[2];
    }

#ifdef CHECK_CVT_TIME
    QueryPerformanceCounter( &t2 );
    dprintf( "yuv444 to rgb %dx%d : %0.3f ms\n", pFrame->width[0], height, 1000.0 * (t2.QuadPart-t1.QuadPart) / (double)fre.QuadPart );
#endif
}

//==========================================================================================================================================
static ALIGNAS(16) int16_t s_TempBuf[4096*2160];

static void Cvt2Bits10_Bit8( const int16_t* src, uint8_t* dst, int width )
{
    int w8 = width - 8;
    int i = 0;
    for( ; i <= w8; i += 8 )
    {
        __m128i xm0 = _mm_load_si128( (__m128i*)(src + i) );
        xm0 = _mm_srli_epi16( xm0, 2 );
        xm0 = _mm_packus_epi16( xm0, xm0 );
        _mm_storel_epi64( (__m128i*)(dst + i), xm0 );
    }
    for( ; i < width; i++ )
    {
        dst[i] = src[i] >> 2;
    }
}

static void Cvt2Bits12_Bit8( const int16_t* src, uint8_t* dst, int width )
{
    int w8 = width - 8;
    int i = 0;
    for( ; i <= w8; i += 8 )
    {
        __m128i xm0 = _mm_load_si128( (__m128i*)(src + i) );
        xm0 = _mm_srli_epi16( xm0, 4 );
        xm0 = _mm_packus_epi16( xm0, xm0 );
        _mm_storel_epi64( (__m128i*)(dst + i), xm0 );
    }
    for( ; i < width; i++ )
    {
        dst[i] = src[i] >> 4;
    }
}

static int ReadCvtUint8( uint8_t* dst, int width, FILE* fp, int bitDepth )
{
    DASSERT( (bitDepth == 8 || bitDepth == 10 || bitDepth == 12) );
    if( width > 4096 * 2160 )
        return 0;

    if( bitDepth == 8 )
    {
        return (int)fread( dst, 1, width, fp );
    }
    else if( bitDepth == 10 )
    {
        int rdcnt = (int)fread( s_TempBuf, 2, width, fp );
        Cvt2Bits10_Bit8( s_TempBuf, dst, rdcnt );
        return rdcnt;
    }
    else if( bitDepth == 12 )
    {
        int rdcnt = (int)fread( s_TempBuf, 2, width, fp );
        Cvt2Bits12_Bit8( s_TempBuf, dst, rdcnt );
        return rdcnt;
    }
    return 0;
}

//==========================================================================================================================================
ALIGNAS(16) static const uint8_t s_YUV420Mask[16] = { 0xFF, 0, 0xFF, 0, 0xFF, 0, 0xFF, 0, 0xFF, 0, 0xFF, 0, 0xFF, 0, 0xFF, 0 };

bool ReadYUV420( FILE* fp, int width, int height, int chroma, int bitDepth, YUVFrame* pFrame )
{
    int i = 0, j = 0;
    int uvWidth = width >> 1, uvHeight = height >> 1;

    uint8_t* yrow = pFrame->planes[0];
    uint8_t* urow = pFrame->planes[1];
    uint8_t* vrow = pFrame->planes[2];

    // read Y plane
    for( i = 0; i < height; i++ )
    {
        if( ReadCvtUint8( yrow, width, fp, bitDepth ) != width )
            return false;
        yrow += pFrame->pitch[0];
    }

    if( chroma == YUV_ChromaType_420 )
    {
        for( i = 0; i < uvHeight; i++ )
        {
            if( ReadCvtUint8( urow, uvWidth, fp, bitDepth ) != uvWidth )
                return false;
            urow += pFrame->pitch[1];
        }

        for( i = 0; i < uvHeight; i++ )
        {
            if( ReadCvtUint8( vrow, uvWidth, fp, bitDepth ) != uvWidth )
                return false;
            vrow += pFrame->pitch[2];
        }
    }
    else if( chroma == YUV_ChromaType_422 )
    {
        uint8_t* src1 = (uint8_t*)s_TempBuf;
        uint8_t* src2 = src1 + ((uvWidth + 15) & ~15);

        // 相邻两行取平均
        for( i = 0; i < uvHeight; i++ )
        {
            if( ReadCvtUint8( src1, uvWidth, fp, bitDepth ) != uvWidth )
                return false;
            if( ReadCvtUint8( src2, uvWidth, fp, bitDepth ) != uvWidth )
                return false;

            for( j = 0; j <= uvWidth - 16; j += 16 )
            {
                __m128i xm0 = _mm_load_si128( (__m128i*)(src1 + j) );
                __m128i xm1 = _mm_load_si128( (__m128i*)(src2 + j) );
                xm0 = _mm_avg_epu8( xm0, xm1 );
                _mm_storeu_si128( (__m128i*)(urow + j), xm0 );
            }
            if( j <= uvWidth - 8 )
            {
                __m128i xm0 = _mm_loadl_epi64( (__m128i*)(src1 + j) );
                __m128i xm1 = _mm_loadl_epi64( (__m128i*)(src2 + j) );
                xm0 = _mm_avg_epu8( xm0, xm1 );
                _mm_storel_epi64( (__m128i*)(urow + j), xm0 );
                j += 8;
            }
            for( ; j < uvWidth; j++ )
            {
                urow[j] = (src1[j] + src2[j] + 1) >> 1;
            }

            urow += pFrame->pitch[1];
        }

        for( i = 0; i < uvHeight; i++ )
        {
            if( ReadCvtUint8( src1, uvWidth, fp, bitDepth ) != uvWidth )
                return false;
            if( ReadCvtUint8( src2, uvWidth, fp, bitDepth ) != uvWidth )
                return false;

            for( j = 0; j <= uvWidth - 16; j += 16 )
            {
                __m128i xm0 = _mm_load_si128( (__m128i*)(src1 + j) );
                __m128i xm1 = _mm_load_si128( (__m128i*)(src2 + j) );
                xm0 = _mm_avg_epu8( xm0, xm1 );
                _mm_storeu_si128( (__m128i*)(vrow + j), xm0 );
            }
            if( j <= uvWidth - 8 )
            {
                __m128i xm0 = _mm_loadl_epi64( (__m128i*)(src1 + j) );
                __m128i xm1 = _mm_loadl_epi64( (__m128i*)(src2 + j) );
                xm0 = _mm_avg_epu8( xm0, xm1 );
                _mm_storel_epi64( (__m128i*)(vrow + j), xm0 );
                j += 8;
            }
            for( ; j < uvWidth; j++ )
            {
                vrow[j] = (src1[j] + src2[j] + 1) >> 1;
            }

            vrow += pFrame->pitch[2];
        }
    }
    else
    {
        uint8_t* src1 = (uint8_t*)s_TempBuf;
        uint8_t* src2 = src1 + ((width + 15) & ~15);
        __m128i mask16 = _mm_load_si128( (__m128i*)s_YUV420Mask );

        // 相邻两行取平均, 抽取偶数点
        for( i = 0; i < uvHeight; i++ )
        {
            if( ReadCvtUint8( src1, width, fp, bitDepth ) != width )
                return false;
            if( ReadCvtUint8( src2, width, fp, bitDepth ) != width )
                return false;

            for( j = 0; j <= uvWidth - 8; j += 8 )
            {
                __m128i xm0 = _mm_load_si128( (__m128i*)(src1 + 2*j) );
                __m128i xm1 = _mm_load_si128( (__m128i*)(src2 + 2*j) );
                xm0 = _mm_avg_epu8( xm0, xm1 );
                xm0 = _mm_and_si128( xm0, mask16 );
                xm0 = _mm_packus_epi16( xm0, xm0 );
                _mm_storel_epi64( (__m128i*)(urow + j), xm0 );
            }
            for( ; j < uvWidth; j++ )
            {
                urow[j] = (src1[2*j] + src2[2*j] + 1) >> 1;
            }

            urow += pFrame->pitch[1];
        }

        for( i = 0; i < uvHeight; i++ )
        {
            if( ReadCvtUint8( src1, width, fp, bitDepth ) != width )
                return false;
            if( ReadCvtUint8( src2, width, fp, bitDepth ) != width )
                return false;

            for( j = 0; j <= uvWidth - 8; j += 8 )
            {
                __m128i xm0 = _mm_load_si128( (__m128i*)(src1 + 2*j) );
                __m128i xm1 = _mm_load_si128( (__m128i*)(src2 + 2*j) );
                xm0 = _mm_avg_epu8( xm0, xm1 );
                xm0 = _mm_and_si128( xm0, mask16 );
                xm0 = _mm_packus_epi16( xm0, xm0 );
                _mm_storel_epi64( (__m128i*)(vrow + j), xm0 );
            }
            for( ; j < uvWidth; j++ )
            {
                vrow[j] = (src1[2*j] + src2[2*j] + 1) >> 1;
            }

            vrow += pFrame->pitch[2];
        }
    }

    pFrame->width[0] = width;
    pFrame->width[1] = uvWidth;
    pFrame->width[2] = uvWidth;
    pFrame->height[0] = height;
    pFrame->height[1] = uvHeight;
    pFrame->height[2] = uvHeight;

    return true;
}

bool ReadYUV422( FILE* fp, int width, int height, int chroma, int bitDepth, YUVFrame* pFrame )
{
    int i = 0, j = 0, uvWidth = width >> 1;

    uint8_t* yrow = pFrame->planes[0];
    uint8_t* urow = pFrame->planes[1];
    uint8_t* vrow = pFrame->planes[2];

    // read Y plane
    for( i = 0; i < height; i++ )
    {
        if( ReadCvtUint8( yrow, width, fp, bitDepth ) != width )
            return false;
        yrow += pFrame->pitch[0];
    }

    if( chroma == YUV_ChromaType_420 )
    {
        // 奇数行复制
        int pitch = pFrame->pitch[1];
        for( i = 0; i < height; i += 2 )
        {
            if( ReadCvtUint8( urow, uvWidth, fp, bitDepth ) != uvWidth )
                return false;
            
            memcpy( urow + pitch, urow, uvWidth );
            urow += pitch * 2;
        }

        pitch = pFrame->pitch[2];
        for( i = 0; i < height; i += 2 )
        {
            if( ReadCvtUint8( vrow, uvWidth, fp, bitDepth ) != uvWidth )
                return false;

            memcpy( vrow + pitch, vrow, uvWidth );
            vrow += pitch * 2;
        }
    }
    else if( chroma == YUV_ChromaType_422 )
    {
        for( i = 0; i < height; i++ )
        {
            if( ReadCvtUint8( urow, uvWidth, fp, bitDepth ) != uvWidth )
                return false;
            urow += pFrame->pitch[1];
        }

        for( i = 0; i < height; i++ )
        {
            if( ReadCvtUint8( vrow, uvWidth, fp, bitDepth ) != uvWidth )
                return false;
            vrow += pFrame->pitch[2];
        }
    }
    else
    {
        uint8_t* src = (uint8_t*)s_TempBuf;
        __m128i mask16 = _mm_load_si128( (__m128i*)s_YUV420Mask );

        // 抽取偶数点
        for( i = 0; i < height; i++ )
        {
            if( ReadCvtUint8( src, width, fp, bitDepth ) != width )
                return false;

            for( j = 0; j <= uvWidth - 8; j += 8 )
            {
                __m128i xm0 = _mm_load_si128( (__m128i*)(src + 2*j) );
                xm0 = _mm_and_si128( xm0, mask16 );
                xm0 = _mm_packus_epi16( xm0, xm0 );
                _mm_storel_epi64( (__m128i*)(urow + j), xm0 );
            }
            for( ; j < uvWidth; j++ )
            {
                urow[j] = src[2*j];
            }

            urow += pFrame->pitch[1];
        }
  
        for( i = 0; i < height; i++ )
        {
            if( ReadCvtUint8( src, width, fp, bitDepth ) != width )
                return false;

            for( j = 0; j <= uvWidth - 8; j += 8 )
            {
                __m128i xm0 = _mm_load_si128( (__m128i*)(src + 2*j) );
                xm0 = _mm_and_si128( xm0, mask16 );
                xm0 = _mm_packus_epi16( xm0, xm0 );
                _mm_storel_epi64( (__m128i*)(vrow + j), xm0 );
            }
            for( ; j < uvWidth; j++ )
            {
                vrow[j] = src[2*j];
            }

            vrow += pFrame->pitch[2];
        }
    }

    pFrame->width[0] = width;
    pFrame->width[1] = uvWidth;
    pFrame->width[2] = uvWidth;
    pFrame->height[0] = height;
    pFrame->height[1] = height;
    pFrame->height[2] = height;

    return true;
}

bool ReadYUV444( FILE* fp, int width, int height, int chroma, int bitDepth, YUVFrame* pFrame )
{
    int i = 0, j = 0;

    uint8_t* yrow = pFrame->planes[0];
    uint8_t* urow = pFrame->planes[1];
    uint8_t* vrow = pFrame->planes[2];

    // read Y plane
    for( i = 0; i < height; i++ )
    {
        if( ReadCvtUint8( yrow, width, fp, bitDepth ) != width )
            return false;
        yrow += pFrame->pitch[0];
    }

    // 转化 4:2:0, 4:2:2 到 4:4:4 取决于当初的采样算法,
    // 由于人眼对颜色不敏感, 简化起见, 垂直方向不进行滤波, 水平方向取平均
    if( chroma == YUV_ChromaType_420 )
    {
        const int uvWidth = width >> 1;
        int pitch = pFrame->pitch[1];
        uint8_t* src = (uint8_t*)s_TempBuf;

        for( i = 0; i < height; i += 2 )
        {
            if( ReadCvtUint8( src, uvWidth, fp, bitDepth ) != uvWidth )
                return false;
            src[uvWidth] = src[uvWidth-1];

            for( j = 0; j <= uvWidth - 8; j += 8 )
            {
                __m128i xm0 = _mm_loadl_epi64( (__m128i*)(src + j) );
                __m128i xm1 = _mm_loadl_epi64( (__m128i*)(src + j + 1) );
                xm1 = _mm_avg_epu8( xm1, xm0 );
                xm0 = _mm_unpacklo_epi8( xm0, xm1 );
                _mm_storeu_si128( (__m128i*)(urow + 2*j), xm0 );
                _mm_storeu_si128( (__m128i*)(urow + 2*j + pitch), xm0 );
            }
            for( ; j < uvWidth; j++ )
            {
                uint8_t u2 = (src[j] + src[j+1] + 1) >> 1;
                urow[2*j] = src[j];
                urow[2*j+1] = u2;
                urow[2*j+pitch] = src[j];
                urow[2*j+pitch+1] = u2;
            }

            urow += pitch * 2;
        }

        pitch = pFrame->pitch[2];
        for( i = 0; i < height; i += 2 )
        {
            if( ReadCvtUint8( src, uvWidth, fp, bitDepth ) != uvWidth )
                return false;
            src[uvWidth] = src[uvWidth-1];

            for( j = 0; j <= uvWidth - 8; j += 8 )
            {
                __m128i xm0 = _mm_loadl_epi64( (__m128i*)(src + j) );
                __m128i xm1 = _mm_loadl_epi64( (__m128i*)(src + j + 1) );
                xm1 = _mm_avg_epu8( xm1, xm0 );
                xm0 = _mm_unpacklo_epi8( xm0, xm1 );
                _mm_storeu_si128( (__m128i*)(vrow + 2*j), xm0 );
                _mm_storeu_si128( (__m128i*)(vrow + 2*j + pitch), xm0 );
            }
            for( ; j < uvWidth; j++ )
            {
                uint8_t v2 = (src[j] + src[j+1] + 1) >> 1;
                vrow[2*j] = src[j];
                vrow[2*j+1] = v2;
                vrow[2*j+pitch] = src[j];
                vrow[2*j+pitch+1] = v2;
            }

            vrow += pitch * 2;
        }
    }
    else if( chroma == YUV_ChromaType_422 )
    {
        const int uvWidth = width >> 1;
        uint8_t* src = (uint8_t*)s_TempBuf;

        for( i = 0; i < height; i++ )
        {
            if( ReadCvtUint8( src, uvWidth, fp, bitDepth ) != uvWidth )
                return false;
            src[uvWidth] = src[uvWidth-1];

            for( j = 0; j <= uvWidth - 8; j += 8 )
            {
                __m128i xm0 = _mm_loadl_epi64( (__m128i*)(src + j) );
                __m128i xm1 = _mm_loadl_epi64( (__m128i*)(src + j + 1) );
                xm1 = _mm_avg_epu8( xm1, xm0 );
                xm0 = _mm_unpacklo_epi8( xm0, xm1 );
                _mm_storeu_si128( (__m128i*)(urow + 2*j), xm0 );
            }
            for( ; j < uvWidth; j++ )
            {
                urow[2*j] = src[j];
                urow[2*j+1] = (src[j] + src[j+1] + 1) >> 1;
            }

            urow += pFrame->pitch[1];
        }

        for( i = 0; i < height; i++ )
        {
            if( ReadCvtUint8( src, uvWidth, fp, bitDepth ) != uvWidth )
                return false;
            src[uvWidth] = src[uvWidth-1];

            for( j = 0; j <= uvWidth - 8; j += 8 )
            {
                __m128i xm0 = _mm_loadl_epi64( (__m128i*)(src + j) );
                __m128i xm1 = _mm_loadl_epi64( (__m128i*)(src + j + 1) );
                xm1 = _mm_avg_epu8( xm1, xm0 );
                xm0 = _mm_unpacklo_epi8( xm0, xm1 );
                _mm_storeu_si128( (__m128i*)(vrow + 2*j), xm0 );
            }
            for( ; j < uvWidth; j++ )
            {
                vrow[2*j] = src[j];
                vrow[2*j+1] = (src[j] + src[j+1] + 1) >> 1;
            }

            vrow += pFrame->pitch[2];
        }
    }
    else
    {
        for( i = 0; i < height; i++ )
        {
            if( ReadCvtUint8( urow, width, fp, bitDepth ) != width )
                return false;
            urow += pFrame->pitch[1];
        }

        for( i = 0; i < height; i++ )
        {
            if( ReadCvtUint8( vrow, width, fp, bitDepth ) != width )
                return false;
            vrow += pFrame->pitch[2];
        }
    }

    pFrame->width[0] = width;
    pFrame->width[1] = width;
    pFrame->width[2] = width;
    pFrame->height[0] = height;
    pFrame->height[1] = height;
    pFrame->height[2] = height;

    return true;
}

//=====================================================================================================================================
#include <io.h>

YUVReader::YUVReader()
{
    m_FrameWidth = 0;
    m_FrameHeight = 0;
    m_FrameSize = 0;
    m_ChromaType = YUV_ChromaType_420;
    m_YSize = 0;
    m_FrameCnt = 0;
    m_pYPlane = NULL;
    m_pUPlane = NULL;
    m_pVPlane = NULL;
    m_pFile = NULL;
}

YUVReader::~YUVReader()
{
    if( m_pFile )
        fclose( m_pFile );

    if( m_pYPlane )
        _aligned_free( m_pYPlane );
    if( m_pUPlane )
        _aligned_free( m_pUPlane );
    if( m_pVPlane )
        _aligned_free( m_pVPlane );
}

bool YUVReader::OpenFile( const char* filename, int width, int height, int chroma, int bitDepth )
{
    if( width <= 0 || height <= 0 || chroma < YUV_ChromaType_420 || chroma > YUV_ChromaType_444 )
        return false;
    if( bitDepth != 8 && bitDepth != 10 && bitDepth != 12 )
        return false;

    if( m_pFile )
        Close();

    FILE* fp = fopen( filename, "rb" );
    if( !fp )
        return false;

    int64_t filesize = _filelengthi64( _fileno( fp ) );
    if( filesize <= 0 )
    {
        fclose( fp );
        return false;
    }

    m_FrameWidth = width;
    m_FrameHeight = height;
    m_ChromaType = chroma;
    m_BitDepth = bitDepth;

    m_YSize = width * height;
    if( chroma == YUV_ChromaType_420 )
        m_FrameSize = m_YSize * 3 / 2;
    else if( chroma == YUV_ChromaType_422 )
        m_FrameSize = m_YSize * 2;
    else
        m_FrameSize = m_YSize * 3;
    
    if( bitDepth != 8 )
        m_FrameSize *= 2;

    m_FrameCnt = (int)(filesize / m_FrameSize);
    m_pFile = fp;

    return true;
}

void YUVReader::Close()
{
    if( m_pFile )
    {
        m_FrameWidth = 0;
        m_FrameHeight = 0;
        m_ChromaType = YUV_ChromaType_420;
        m_FrameCnt = 0;
        m_FrameSize = 0;
        m_YSize = 0;
        fclose( m_pFile );
        m_pFile = NULL;
    }

    if( m_pYPlane )
    {
        _aligned_free( m_pYPlane );
        m_pYPlane = NULL;
    }
    if( m_pUPlane )
    {
        _aligned_free( m_pUPlane );
        m_pUPlane = NULL;
    }
    if( m_pVPlane )
    {
        _aligned_free( m_pVPlane );
        m_pVPlane = NULL;
    }
}

bool YUVReader::GetFrameRaw( int idx, uint8_t* pDst )
{
    if( m_pFile && idx >= 0 && idx < m_FrameCnt )
    {
        int64_t offset = (int64_t)idx * m_FrameSize;
        _fseeki64( m_pFile, offset, SEEK_SET );
        if( fread( pDst, 1, m_FrameSize, m_pFile ) == m_FrameSize )
            return true;
    }
    return false;
}

bool YUVReader::GetFrameRawLuma( int idx, uint8_t* pDst )
{
    if( m_pFile && idx >= 0 && idx < m_FrameCnt )
    {
        int64_t offset = (int64_t)idx * m_FrameSize;
        _fseeki64( m_pFile, offset, SEEK_SET );
        if( ReadCvtUint8( pDst, m_YSize, m_pFile, m_BitDepth ) == m_YSize )
            return true;
    }
    return false;
}

bool YUVReader::GetFrameYUVP420( int idx, YUVFrame* pFrame )
{
    if( !m_pFile || idx < 0 || idx >= m_FrameCnt )
        return false;

    int64_t offset = (int64_t)idx * m_FrameSize;
    _fseeki64( m_pFile, offset, SEEK_SET );

    return ReadYUV420( m_pFile, m_FrameWidth, m_FrameHeight, m_ChromaType, m_BitDepth, pFrame );
}

bool YUVReader::GetFrameYUVP422( int idx, YUVFrame* pFrame )
{
    if( !m_pFile || idx < 0 || idx >= m_FrameCnt )
        return false;

    int64_t offset = (int64_t)idx * m_FrameSize;
    _fseeki64( m_pFile, offset, SEEK_SET );

    return ReadYUV422( m_pFile, m_FrameWidth, m_FrameHeight, m_ChromaType, m_BitDepth, pFrame );
}

bool YUVReader::GetFrameYUVP444( int idx, YUVFrame* pFrame )
{
    if( !m_pFile || idx < 0 || idx >= m_FrameCnt )
        return false;

    int64_t offset = (int64_t)idx * m_FrameSize;
    _fseeki64( m_pFile, offset, SEEK_SET );

    return ReadYUV444( m_pFile, m_FrameWidth, m_FrameHeight, m_ChromaType, m_BitDepth, pFrame );
}

bool YUVReader::GetFrameBGRA( int idx, uint8_t* pDst, int dstPitch )
{
    if( !m_pFile || idx < 0 || idx >= m_FrameCnt )
        return false;

    // read raw frame
    int uvSize = m_YSize;
    if( m_ChromaType == YUV_ChromaType_420 )
        uvSize >>= 2;
    else if( m_ChromaType == YUV_ChromaType_422 )
        uvSize >>= 1;

    if( !m_pYPlane )
    {
        m_pYPlane = (uint8_t*)_aligned_malloc( m_YSize + 16, 16 );
        m_pUPlane = (uint8_t*)_aligned_malloc( uvSize + 16, 16 );
        m_pVPlane = (uint8_t*)_aligned_malloc( uvSize + 16, 16 );
    }

    int64_t offset = (int64_t)idx * m_FrameSize;
    _fseeki64( m_pFile, offset, SEEK_SET );
    if( fread( m_pYPlane, 1, m_YSize, m_pFile ) != m_YSize )
        return false;
    if( fread( m_pUPlane, 1, uvSize, m_pFile ) != uvSize )
        return false;
    if( fread( m_pVPlane, 1, uvSize, m_pFile ) != uvSize )
        return false;

    YUVFrame frame;
    frame.planes[0] = m_pYPlane;
    frame.planes[1] = m_pUPlane;
    frame.planes[2] = m_pVPlane;

    // convert YUV to BGR
    if( m_ChromaType == YUV_ChromaType_420 )
    {
        frame.width[0] = m_FrameWidth;
        frame.width[1] = m_FrameWidth >> 1;
        frame.width[2] = m_FrameWidth >> 1;
        frame.height[0] = m_FrameHeight;
        frame.height[1] = m_FrameHeight >> 1;
        frame.height[2] = m_FrameHeight >> 1;
        frame.pitch[0] = frame.width[0];
        frame.pitch[1] = frame.width[1];
        frame.pitch[2] = frame.width[2];

        FastConvertYUV420_BGRA( &frame, pDst, dstPitch );
    }
    else if( m_ChromaType == YUV_ChromaType_422 )
    {
        frame.width[0] = m_FrameWidth;
        frame.width[1] = m_FrameWidth >> 1;
        frame.width[2] = m_FrameWidth >> 1;
        frame.height[0] = m_FrameHeight;
        frame.height[1] = m_FrameHeight;
        frame.height[2] = m_FrameHeight;
        frame.pitch[0] = frame.width[0];
        frame.pitch[1] = frame.width[1];
        frame.pitch[2] = frame.width[2];

        FastConvertYUV422_BGRA( &frame, pDst, dstPitch );
    }
    else
    {
        frame.width[0] = m_FrameWidth;
        frame.width[1] = m_FrameWidth;
        frame.width[2] = m_FrameWidth;
        frame.height[0] = m_FrameHeight;
        frame.height[1] = m_FrameHeight;
        frame.height[2] = m_FrameHeight;
        frame.pitch[0] = frame.width[0];
        frame.pitch[1] = frame.width[1];
        frame.pitch[2] = frame.width[2];

        FastConvertYUV444_BGRA( &frame, pDst, dstPitch );
    }

    return true;
}