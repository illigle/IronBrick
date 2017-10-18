/*
* This Source Code Form is subject to the terms of the Mozilla Public License Version 2.0.
* If a copy of the MPL was not distributed with this file, 
* You can obtain one at http://mozilla.org/MPL/2.0/.

* Covered Software is provided on an "as is" basis, 
* without warranty of any kind, either expressed, implied, or statutory,
* that the Covered Software is free of defects, merchantable, 
* fit for a particular purpose or non-infringing.
 
* Copyright (c) Wei Dongliang <illigle@163.com>.
*/

#include <smmintrin.h>      // SSE-4
#include "AvsDecoder.h"

namespace irk_avs_dec {

static inline MvUnion mv_scale( MvUnion src, int scale )
{
    MvUnion mv;
    mv.x = static_cast<int16_t>((src.x * scale + (src.x >> 31) + 256) >> 9);
    mv.y = static_cast<int16_t>((src.y * scale + (src.y >> 31) + 256) >> 9);
    return mv;
}

// 返回 x, y, z 的中间值
static inline int media3( int x, int y, int z )
{
    __m128i xm = _mm_cvtsi32_si128( x );
    __m128i ym = _mm_cvtsi32_si128( y );
    __m128i zm = _mm_cvtsi32_si128( z );
    __m128i xymax = _mm_max_epi32( xm, ym );
    xm = _mm_min_epi32( xm, ym );
    zm = _mm_min_epi32( zm, xymax );
    zm = _mm_max_epi32( zm, xm );
    return _mm_cvtsi128_si32( zm );
}

// 运动矢量预测
MvUnion get_mv_pred( const MvInfo abcMVS[3], int refDist )
{
    static const uint8_t s_MVRefIdx[8] = {7, 7, 7, 2, 7, 1, 0, 0};

    int msk = (abcMVS[0].refIdx >> 31) & 1;
    msk |= (abcMVS[1].refIdx >> 31) & 2;
    msk |= (abcMVS[2].refIdx >> 31) & 4;
    int idx = s_MVRefIdx[msk];
    if( idx <= 2 )                  // 只有一个参考索引不为 -1, 或者全为 -1
        return abcMVS[idx].mv;

    // 根据参考帧距离进行缩放
    MvUnion mvA = mv_scale( abcMVS[0].mv, abcMVS[0].denDist * refDist );
    MvUnion mvB = mv_scale( abcMVS[1].mv, abcMVS[1].denDist * refDist );
    MvUnion mvC = mv_scale( abcMVS[2].mv, abcMVS[2].denDist * refDist );
    int distAB = abs( mvA.x - mvB.x ) + abs( mvA.y - mvB.y );
    int distBC = abs( mvC.x - mvB.x ) + abs( mvC.y - mvB.y );
    int distAC = abs( mvA.x - mvC.x ) + abs( mvA.y - mvC.y );
    int medABC = media3( distAB, distBC, distAC );
    if( medABC == distAB )
        return mvC;
    if( medABC == distBC )
        return mvA;
    return mvB;
}

// 针对周边 block 至少两个参考帧索引 >= 0 的情形
MvUnion get_mv_pred2( const MvInfo abcMVS[3], int refDist )
{
    // 根据参考帧距离进行缩放
    MvUnion mvA = mv_scale( abcMVS[0].mv, abcMVS[0].denDist * refDist );
    MvUnion mvB = mv_scale( abcMVS[1].mv, abcMVS[1].denDist * refDist );
    MvUnion mvC = mv_scale( abcMVS[2].mv, abcMVS[2].denDist * refDist );
    int distAB = abs( mvA.x - mvB.x ) + abs( mvA.y - mvB.y );
    int distBC = abs( mvC.x - mvB.x ) + abs( mvC.y - mvB.y );
    int distAC = abs( mvA.x - mvC.x ) + abs( mvA.y - mvC.y );
    int medABC = media3( distAB, distBC, distAC );
    if( medABC == distAB )
        return mvC;
    if( medABC == distBC )
        return mvA;
    return mvB;
}

//======================================================================================================================
// AVS 标准允许指向参考帧有效范围外, 此时需填充

struct McExtRect
{
    int         x;
    int         y;
    int         height;
    int         picWidth;
    int         picHeight;
    uint8_t*    buf;
};

static void MC_extend_32( const uint8_t* src, int pitch, int x, int height, int maxX, uint8_t* dst )
{
    // 边界有填充, 且 x86 支持非对齐读写
    if( x < 0 )                     // left
    {
        for( int i = 0; i < height; i++ )
        {
            __m128i xm0 = _mm_set1_epi8( src[0] );
            _mm_store_si128( (__m128i*)dst, xm0 );
            _mm_store_si128( (__m128i*)(dst + 16), xm0 );
            for( int j = (-x & ~3); j < 24; j += 4 )
                *(uint32_as*)(dst + j) =  *(uint32_as*)(src + j + x);

            dst += 32;
            src += pitch;
        }
    }
    else if( maxX - x < 24 )        // right
    {
        int xx = maxX - x;
        int x4 = (xx + 3) & ~3;     // mulitple of 4 >= xx
        src += maxX;
        for( int i = 0; i < height; ++i )
        {
            __m128i xm0 = _mm_set1_epi8( src[-1] );
            _mm_store_si128( (__m128i*)dst, xm0 );
            _mm_store_si128( (__m128i*)(dst + 16), xm0 );
            for( int j = 0; j < x4; j += 4 )
                *(uint32_as*)(dst + j ) = *(uint32_as*)(src + j - xx);

            dst += 32;
            src += pitch;
        }
    }
    else    // media
    {
        src += x;
        for( int i = 0; i < height; i++ )
        {
            __m128i xm0 = _mm_loadu_si128( (__m128i*)src );          
            __m128i xm1 = _mm_loadu_si128( (__m128i*)(src + 16) );
            _mm_store_si128( (__m128i*)dst, xm0 );
            _mm_store_si128( (__m128i*)(dst + 16), xm1 );
            dst += 32;
            src += pitch;
        }
    }
}

static void MC_extend_16( const uint8_t* src, int pitch, int x, int height, int maxX, uint8_t* dst )
{
    // 边界有填充, 且 x86 支持非对齐读写
    if( x < 0 )                 // left
    {
        for( int i = 0; i < height; i++ )
        {
            __m128i xm0 = _mm_set1_epi8( src[0] );
            _mm_store_si128( (__m128i*)dst, xm0 );
            for( int j = (-x & ~3); j < 16; j += 4 )
                *(uint32_as*)(dst + j) =  *(uint32_as*)(src + j + x);

            dst += 16;
            src += pitch;
        }
    }
    else if( maxX - x < 16 )        // right
    {
        int xx = maxX - x;
        int x4 = (xx + 3) & ~3;     // mulitple of 4 >= xx
        src += maxX;
        for( int i = 0; i < height; ++i )
        {
            __m128i xm0 = _mm_set1_epi8( src[-1] );
            _mm_store_si128( (__m128i*)dst, xm0 );
            for( int j = 0; j < x4; j += 4 )
                *(uint32_as*)(dst + j ) = *(uint32_as*)(src + j - xx);

            dst += 16;
            src += pitch;
        }
    }
    else    // media
    {
        src += x;
        for( int i = 0; i < height; i++ )
        {
            __m128i xm0 = _mm_loadu_si128( (__m128i*)src );
            _mm_store_si128( (__m128i*)dst, xm0 );
            dst += 16;
            src += pitch;
        }
    }
}

static void MC_extend_8( const uint8_t* src, int pitch, int x, int height, int maxX, uint8_t* dst )
{
    if( x < -8 )        // left
    {
        for( int i = 0; i < height; i++ )
        {
            uint32_t d4 = src[0] * 0x01010101u;
            *(uint32_as*)dst = d4;
            *(uint32_as*)(dst + 4) = d4;
            dst += 8;
            src += pitch;
        }
    }
    else if( x > maxX ) // right
    {
        src += maxX - 1;
        for( int i = 0; i < height; i++ )
        {
            uint32_t d4 = src[0] * 0x01010101u;
            *(uint32_as*)dst = d4;
            *(uint32_as*)(dst + 4) = d4;
            dst += 8;
            src += pitch;
        }
    }
    else
    {
        src += x;
        for( int i = 0; i < height; i++ )
        {
            *(uint32_as*)dst = *(uint32_as*)src;
            *(uint32_as*)(dst + 4) = *(uint32_as*)(src + 4);
            dst += 8;
            src += pitch;
        }
    }
}

// 得到 32xN 大小的参考图像, 如果超出原始图像范围, 用边界点填充, 结果的行宽为 32
static void get_ref_data_32xN( const uint8_t* src, int pitch, const McExtRect& rc )
{
    uint8_t* dst = rc.buf;
    assert( ((uintptr_t)dst & 15) == 0 );

    if( rc.y < 0 )  // 上边界
    {
        int yy = -rc.y;
        if( yy >= rc.height )
            yy = rc.height - 1;

        uint8_t* dst2 = dst + yy * 32;
        MC_extend_32( src, pitch, rc.x, rc.height - yy, rc.picWidth, dst2 );

        // above top scan line
        __m128i xm0 = _mm_load_si128( (__m128i*)dst2 );
        __m128i xm1 = _mm_load_si128( (__m128i*)(dst2 + 16) );
        for( int i = 0; i < yy; i++ )
        {
            _mm_store_si128( (__m128i*)dst, xm0 );
            _mm_store_si128( (__m128i*)(dst + 16), xm1 );
            dst += 32;
        }
    }
    else if( rc.picHeight - rc.y < rc.height )  // 下边界
    {
        int yy = rc.picHeight - rc.y;
        if( yy < 1 )
            yy = 1;

        src += (rc.picHeight - yy) * pitch;
        MC_extend_32( src, pitch, rc.x, yy, rc.picWidth, dst );

        // below bottom scan line
        dst += yy * 32;
        __m128i xm0 = _mm_load_si128( (__m128i*)(dst - 32) );
        __m128i xm1 = _mm_load_si128( (__m128i*)(dst - 16) );
        for( int i = yy; i < rc.height; i++ )
        {
            _mm_store_si128( (__m128i*)dst, xm0 );
            _mm_store_si128( (__m128i*)(dst + 16), xm1 );
            dst += 32;
        }
    }
    else
    {
        src += rc.y * pitch;
        MC_extend_32( src, pitch, rc.x, rc.height, rc.picWidth, dst );
    }
}

// 得到 16xN 大小的参考图像, 如果超出原始图像范围, 用边界点填充, 结果的行宽为 16
static void get_ref_data_16xN( const uint8_t* src, int pitch, const McExtRect& rc )
{
    uint8_t* dst = rc.buf;
    assert( ((uintptr_t)dst & 15) == 0 );

    if( rc.y < 0 )      // 上边界
    {
        int yy = -rc.y;
        if( yy >= rc.height )
            yy = rc.height - 1;

        uint8_t* dst2 = dst + yy * 16;
        MC_extend_16( src, pitch, rc.x, rc.height - yy, rc.picWidth, dst2 );

        // above top scan line
        __m128i xm0 = _mm_load_si128( (__m128i*)dst2 );
        for( int i = 0; i < yy; i++ )
        {
            _mm_store_si128( (__m128i*)dst, xm0 );
            dst += 16;
        }
    }
    else if( rc.picHeight - rc.y < rc.height )  // 下边界
    {
        int yy = rc.picHeight - rc.y;
        if( yy < 1 )
            yy = 1;

        src += (rc.picHeight - yy) * pitch;
        MC_extend_16( src, pitch, rc.x, yy, rc.picWidth, dst );

        // below bottom scan line
        dst += yy * 16;
        __m128i xm0 = _mm_load_si128( (__m128i*)(dst - 16) );
        for( int i = yy; i < rc.height; i++ )
        {
            _mm_store_si128( (__m128i*)dst, xm0 );
            dst += 16;
        }
    }
    else
    {
        src += rc.y * pitch;
        MC_extend_16( src, pitch, rc.x, rc.height, rc.picWidth, dst );
    }
}

// 得到 8xN 大小的参考图像, 如果超出原始图像范围, 用边界点填充, 结果的行宽为 8
static void get_ref_data_8xN( const uint8_t* src, int pitch, const McExtRect& rc )
{
    assert( ((uintptr_t)rc.buf & 15) == 0 );
    uint8_t* dst = rc.buf;

    if( rc.y < 0 )          // 上边界
    {
        int yy = -rc.y;
        if( yy >= rc.height )
            yy = rc.height - 1;

        uint8_t* dst2 = dst + yy * 8;
        MC_extend_8( src, pitch, rc.x, rc.height - yy, rc.picWidth, dst2 );

        // above top scan line
        __m128i xm0 = _mm_loadl_epi64( (__m128i*)dst2 );
        for( int i = 0; i < yy; i++ )
        {
            _mm_storel_epi64( (__m128i*)dst, xm0 );
            dst += 8;
        }
    }
    else if( rc.picHeight - rc.y < rc.height )     // 下边界
    {
        int yy = rc.picHeight - rc.y;
        if( yy < 1 )
            yy = 1;

        src += (rc.picHeight - yy) * pitch;
        MC_extend_8( src, pitch, rc.x, yy, rc.picWidth, dst );

        // below bottom scan line
        dst += yy * 8;
        __m128i xm0 = _mm_loadl_epi64( (__m128i*)(dst - 8) );
        for( int i = yy; i < rc.height; i++ )
        {
            _mm_storel_epi64( (__m128i*)dst, xm0 );
            dst += 8;
        }
    }
    else
    {
        src += rc.y * pitch;
        MC_extend_8( src, pitch, rc.x, rc.height, rc.picWidth, dst );
    }
}

//======================================================================================================================

// 整数点简单复制, 16x16, 16x8
static void MC_copy_16xN( const uint8_t* src, int srcPitch, uint8_t* dst, int dstPitch, int N )
{
    assert( N == 16 || N == 8 );
    __m128i xm0, xm1;
    while( 1 )
    {
        xm0 = _mm_loadu_si128( (__m128i*)src );
        xm1 = _mm_loadu_si128( (__m128i*)(src + srcPitch) );
        _mm_store_si128( (__m128i*)dst, xm0 );
        _mm_store_si128( (__m128i*)(dst + dstPitch), xm1 );
        src += srcPitch * 2;
        dst += dstPitch * 2;
        xm0 = _mm_loadu_si128( (__m128i*)src );
        xm1 = _mm_loadu_si128( (__m128i*)(src + srcPitch) );
        _mm_store_si128( (__m128i*)dst, xm0 );
        _mm_store_si128( (__m128i*)(dst + dstPitch), xm1 );
        src += srcPitch * 2;
        dst += dstPitch * 2;
        xm0 = _mm_loadu_si128( (__m128i*)src );
        xm1 = _mm_loadu_si128( (__m128i*)(src + srcPitch) );
        _mm_store_si128( (__m128i*)dst, xm0 );
        _mm_store_si128( (__m128i*)(dst + dstPitch), xm1 );
        src += srcPitch * 2;
        dst += dstPitch * 2;
        xm0 = _mm_loadu_si128( (__m128i*)src );
        xm1 = _mm_loadu_si128( (__m128i*)(src + srcPitch) );
        _mm_store_si128( (__m128i*)dst, xm0 );
        _mm_store_si128( (__m128i*)(dst + dstPitch), xm1 );

        if( (N -= 8) == 0 )
            break;
        src += srcPitch * 2;
        dst += dstPitch * 2;
    }
}

// 整数点简单复制, 8x16, 8x8, 8x4
static void MC_copy_8xN( const uint8_t* src, int srcPitch, uint8_t* dst, int dstPitch, int N )
{
    assert( (N & 3) == 0 );
    __m128i xm0, xm1;
    while( 1 )
    {
        xm0 = _mm_loadl_epi64( (__m128i*)src );
        xm1 = _mm_loadl_epi64( (__m128i*)(src + srcPitch) );
        _mm_storel_epi64( (__m128i*)dst, xm0 );
        _mm_storel_epi64( (__m128i*)(dst + dstPitch), xm1 );
        src += srcPitch * 2;
        dst += dstPitch * 2;
        xm0 = _mm_loadl_epi64( (__m128i*)src );
        xm1 = _mm_loadl_epi64( (__m128i*)(src + srcPitch) );
        _mm_storel_epi64( (__m128i*)dst, xm0 );
        _mm_storel_epi64( (__m128i*)(dst + dstPitch), xm1 );

        if( (N -= 4) == 0 )
            break;
        src += srcPitch * 2;
        dst += dstPitch * 2;
    }
}

// 整数点简单复制, 4x8
static inline void MC_copy_4x8( const uint8_t* src, int srcPitch, uint8_t* dst, int dstPitch )
{
    *(uint32_as*)dst = *(uint32_as*)src;
    *(uint32_as*)(dst + dstPitch)   = *(uint32_as*)(src + srcPitch);
    *(uint32_as*)(dst + dstPitch*2) = *(uint32_as*)(src + srcPitch*2);
    *(uint32_as*)(dst + dstPitch*3) = *(uint32_as*)(src + srcPitch*3);
    src += srcPitch * 4;
    dst += dstPitch * 4;
    *(uint32_as*)dst = *(uint32_as*)src;
    *(uint32_as*)(dst + dstPitch)   = *(uint32_as*)(src + srcPitch);
    *(uint32_as*)(dst + dstPitch*2) = *(uint32_as*)(src + srcPitch*2);
    *(uint32_as*)(dst + dstPitch*3) = *(uint32_as*)(src + srcPitch*3);
}

// 整数点简单复制, 4x4
static inline void MC_copy_4x4( const uint8_t* src, int srcPitch, uint8_t* dst, int dstPitch )
{
    *(uint32_as*)dst = *(uint32_as*)src;
    *(uint32_as*)(dst + dstPitch)   = *(uint32_as*)(src + srcPitch);
    *(uint32_as*)(dst + dstPitch*2) = *(uint32_as*)(src + srcPitch*2);
    *(uint32_as*)(dst + dstPitch*3) = *(uint32_as*)(src + srcPitch*3);
}

//======================================================================================================================
// 亮度分量帧间预测

// GCC 无法生成最优的指令
#if defined(_MSC_VER) && defined(NDEBUG)
#define loadzx_epu8_epi16( addr ) _mm_cvtepu8_epi16( *(__m128i*)(addr) )
#else
#define loadzx_epu8_epi16( addr ) _mm_cvtepu8_epi16( _mm_loadl_epi64( (__m128i*)(addr) ) )
#endif

// 半像素点水平滤波 [-1, 5, 5 -1]
// x0 输入, y0 输出, x1, x2, x3 为临时内存, zero 为 0
#define HP_HOR_FILTER( x0, x1, x2, x3, y0 ) \
    x1 = _mm_srli_si128( x0, 1 );           \
    x2 = _mm_srli_si128( x0, 2 );           \
    x3 = _mm_srli_si128( x0, 3 );           \
    x0 = _mm_cvtepu8_epi16( x0 );           \
    x1 = _mm_cvtepu8_epi16( x1 );           \
    x2 = _mm_cvtepu8_epi16( x2 );           \
    x3 = _mm_cvtepu8_epi16( x3 );           \
    x0 = _mm_adds_epi16( x0, x3 );          \
    x1 = _mm_adds_epi16( x1, x2 );          \
    y0 = _mm_slli_epi16( x1, 2 );           \
    x1 = _mm_subs_epi16( x1, x0 );          \
    y0 = _mm_adds_epi16( y0, x1 );

// 半像素点垂直滤波 [-1, 5, 5 -1]
// x0~x3 为输入, y0 为输出, t0, t1 为临时寄存器
#define HP_VER_FILTER( x0, x1, x2, x3, t0, t1, y0 ) \
    t0 = _mm_adds_epi16( x0, x3 );      \
    t1 = _mm_adds_epi16( x1, x2 );      \
    y0 = _mm_subs_epi16( t1, t0 );      \
    t1 = _mm_slli_epi16( t1, 2 );       \
    y0 = _mm_adds_epi16( y0, t1 );

// 半像素点水平滤波
static void MC_luma_8xN_x2y0( const uint8_t* src, int srcPitch, uint8_t* dst, int dstPitch, int N )
{
    __m128i xm0, xm1, xm2, xm3, ym0;
    __m128i rnd = _mm_set1_epi16( 4 );

    src -= 1;
    for( int i = 0; i < N; i++ )
    {
        xm0 = _mm_loadu_si128( (__m128i*)src );
        HP_HOR_FILTER( xm0, xm1, xm2, xm3, ym0 );
        ym0 = _mm_adds_epi16( ym0, rnd );
        ym0 = _mm_srai_epi16( ym0, 3 );
        ym0 = _mm_packus_epi16( ym0, ym0 );
        _mm_storel_epi64( (__m128i*)dst, ym0 );
        src += srcPitch;
        dst += dstPitch;
    }
}

// 半像素点垂直滤波
static void MC_luma_8xN_x0y2( const uint8_t* src, int srcPitch, uint8_t* dst, int dstPitch, int N )
{
    assert( N == 16 || N == 8 );
    __m128i xm0, xm1, xm2, xm3, tm0, tm1, ym0;
    __m128i rnd = _mm_set1_epi16( 4 );

    xm0 = loadzx_epu8_epi16( src - srcPitch );
    xm1 = loadzx_epu8_epi16( src );
    xm2 = loadzx_epu8_epi16( src + srcPitch );
    src += srcPitch * 2;

    for( int i = 0; i < N; i += 4 )
    {
        // row 0
        xm3 = loadzx_epu8_epi16( src );
        HP_VER_FILTER( xm0, xm1, xm2, xm3, tm0, tm1, ym0 );
        ym0 = _mm_adds_epi16( ym0, rnd );
        ym0 = _mm_srai_epi16( ym0, 3 );
        ym0 = _mm_packus_epi16( ym0, ym0 );
        _mm_storel_epi64( (__m128i*)dst, ym0 );
        src += srcPitch;
        dst += dstPitch;

        // row 1
        xm0 = loadzx_epu8_epi16( src );
        HP_VER_FILTER( xm1, xm2, xm3, xm0, tm0, tm1, ym0 );
        ym0 = _mm_adds_epi16( ym0, rnd );
        ym0 = _mm_srai_epi16( ym0, 3 );
        ym0 = _mm_packus_epi16( ym0, ym0 );
        _mm_storel_epi64( (__m128i*)dst, ym0 );
        src += srcPitch;
        dst += dstPitch;

        // row 2
        xm1 = loadzx_epu8_epi16( src );
        HP_VER_FILTER( xm2, xm3, xm0, xm1, tm0, tm1, ym0 );
        ym0 = _mm_adds_epi16( ym0, rnd );
        ym0 = _mm_srai_epi16( ym0, 3 );
        ym0 = _mm_packus_epi16( ym0, ym0 );
        _mm_storel_epi64( (__m128i*)dst, ym0 );
        src += srcPitch;
        dst += dstPitch;

        // row 3
        xm2 = loadzx_epu8_epi16( src );
        HP_VER_FILTER( xm3, xm0, xm1, xm2, tm0, tm1, ym0 );
        ym0 = _mm_adds_epi16( ym0, rnd );
        ym0 = _mm_srai_epi16( ym0, 3 );
        ym0 = _mm_packus_epi16( ym0, ym0 );
        _mm_storel_epi64( (__m128i*)dst, ym0 );
        src += srcPitch;
        dst += dstPitch;
    }
}

// 半像素点中间值
static void MC_luma_8xN_x2y2( const uint8_t* src, int srcPitch, uint8_t* dst, int dstPitch, int N )
{
    assert( N == 16 || N == 8 );
    __m128i xm0, xm1, xm2, xm3, ym0, ym1, ym2, ym3;
    __m128i rnd = _mm_set1_epi16( 32 );

    src -= 1;
    xm0 = _mm_loadu_si128( (__m128i*)(src - srcPitch) );
    HP_HOR_FILTER( xm0, xm1, xm2, xm3, ym0 );
    xm0 = _mm_loadu_si128( (__m128i*)src );
    HP_HOR_FILTER( xm0, xm1, xm2, xm3, ym1 );
    xm0 = _mm_loadu_si128( (__m128i*)(src + srcPitch) );
    HP_HOR_FILTER( xm0, xm1, xm2, xm3, ym2 );
    src += srcPitch * 2;

    for( int i = 0; i < N; i += 4 )
    {
        // row 0
        xm0 = _mm_loadu_si128( (__m128i*)src );
        HP_HOR_FILTER( xm0, xm1, xm2, xm3, ym3 );               // 水平滤波
        HP_VER_FILTER( ym0, ym1, ym2, ym3, xm0, xm1, xm2 );     // 垂直滤波
        xm2 = _mm_adds_epi16( xm2, rnd );
        xm2 = _mm_srai_epi16( xm2, 6 );
        xm2 = _mm_packus_epi16( xm2, xm2 );
        _mm_storel_epi64( (__m128i*)dst, xm2 );
        src += srcPitch;
        dst += dstPitch;

        // row 1
        xm0 = _mm_loadu_si128( (__m128i*)src );
        HP_HOR_FILTER( xm0, xm1, xm2, xm3, ym0 );               // 水平滤波
        HP_VER_FILTER( ym1, ym2, ym3, ym0, xm0, xm1, xm2 );     // 垂直滤波
        xm2 = _mm_adds_epi16( xm2, rnd );
        xm2 = _mm_srai_epi16( xm2, 6 );
        xm2 = _mm_packus_epi16( xm2, xm2 );
        _mm_storel_epi64( (__m128i*)dst, xm2 );
        src += srcPitch;
        dst += dstPitch;

        // row 2
        xm0 = _mm_loadu_si128( (__m128i*)src );
        HP_HOR_FILTER( xm0, xm1, xm2, xm3, ym1 );               // 水平滤波
        HP_VER_FILTER( ym2, ym3, ym0, ym1, xm0, xm1, xm2 );     // 垂直滤波
        xm2 = _mm_adds_epi16( xm2, rnd );
        xm2 = _mm_srai_epi16( xm2, 6 );
        xm2 = _mm_packus_epi16( xm2, xm2 );
        _mm_storel_epi64( (__m128i*)dst, xm2 );
        src += srcPitch;
        dst += dstPitch;

        // row 3
        xm0 = _mm_loadu_si128( (__m128i*)src );
        HP_HOR_FILTER( xm0, xm1, xm2, xm3, ym2 );               // 水平滤波
        HP_VER_FILTER( ym3, ym0, ym1, ym2, xm0, xm1, xm2 );     // 垂直滤波
        xm2 = _mm_adds_epi16( xm2, rnd );
        xm2 = _mm_srai_epi16( xm2, 6 );
        xm2 = _mm_packus_epi16( xm2, xm2 );
        _mm_storel_epi64( (__m128i*)dst, xm2 );
        src += srcPitch;
        dst += dstPitch;
    }
}

// (0.25, 0.25), (0.25, 0.75), (0.75, 0.25), (0.75, 0.75)
// src2: 对应边角整数样本点
static void MC_luma_8xN_x1y1( const uint8_t* src, int srcPitch, uint8_t* dst, int dstPitch, int N, const uint8_t* src2 )
{
    assert( N == 16 || N == 8 );
    __m128i xm0, xm1, xm2, xm3, ym0, ym1, ym2, ym3;
    __m128i rnd = _mm_set1_epi16( 64 );

    src -= 1;
    xm0 = _mm_loadu_si128( (__m128i*)(src - srcPitch) );
    HP_HOR_FILTER( xm0, xm1, xm2, xm3, ym0 );
    xm0 = _mm_loadu_si128( (__m128i*)src );
    HP_HOR_FILTER( xm0, xm1, xm2, xm3, ym1 );
    xm0 = _mm_loadu_si128( (__m128i*)(src + srcPitch) );
    HP_HOR_FILTER( xm0, xm1, xm2, xm3, ym2 );
    src += srcPitch * 2;

    for( int i = 0; i < N; i += 4 )
    {
        // row 0
        xm0 = _mm_loadu_si128( (__m128i*)src );
        HP_HOR_FILTER( xm0, xm1, xm2, xm3, ym3 );               // 水平滤波
        HP_VER_FILTER( ym0, ym1, ym2, ym3, xm0, xm1, xm2 );     // 垂直滤波
        xm0 = loadzx_epu8_epi16( src2 );
        xm2 = _mm_adds_epi16( xm2, rnd );
        xm0 = _mm_slli_epi16( xm0, 6 );
        xm2 = _mm_adds_epi16( xm2, xm0 );
        xm2 = _mm_srai_epi16( xm2, 7 );
        xm2 = _mm_packus_epi16( xm2, xm2 );
        _mm_storel_epi64( (__m128i*)dst, xm2 );
        src += srcPitch;
        dst += dstPitch;
        src2 += srcPitch;

        // row 1
        xm0 = _mm_loadu_si128( (__m128i*)src );
        HP_HOR_FILTER( xm0, xm1, xm2, xm3, ym0 );               // 水平滤波
        HP_VER_FILTER( ym1, ym2, ym3, ym0, xm0, xm1, xm2 );     // 垂直滤波
        xm0 = loadzx_epu8_epi16( src2 );
        xm2 = _mm_adds_epi16( xm2, rnd );
        xm0 = _mm_slli_epi16( xm0, 6 );
        xm2 = _mm_adds_epi16( xm2, xm0 );
        xm2 = _mm_srai_epi16( xm2, 7 );
        xm2 = _mm_packus_epi16( xm2, xm2 );
        _mm_storel_epi64( (__m128i*)dst, xm2 );
        src += srcPitch;
        dst += dstPitch;
        src2 += srcPitch;

        // row 2
        xm0 = _mm_loadu_si128( (__m128i*)src );
        HP_HOR_FILTER( xm0, xm1, xm2, xm3, ym1 );               // 水平滤波
        HP_VER_FILTER( ym2, ym3, ym0, ym1, xm0, xm1, xm2 );     // 垂直滤波
        xm0 = loadzx_epu8_epi16( src2 );
        xm2 = _mm_adds_epi16( xm2, rnd );
        xm0 = _mm_slli_epi16( xm0, 6 );
        xm2 = _mm_adds_epi16( xm2, xm0 );
        xm2 = _mm_srai_epi16( xm2, 7 );
        xm2 = _mm_packus_epi16( xm2, xm2 );
        _mm_storel_epi64( (__m128i*)dst, xm2 );
        src += srcPitch;
        dst += dstPitch;
        src2 += srcPitch;

        // row 3
        xm0 = _mm_loadu_si128( (__m128i*)src );
        HP_HOR_FILTER( xm0, xm1, xm2, xm3, ym2 );               // 水平滤波
        HP_VER_FILTER( ym3, ym0, ym1, ym2, xm0, xm1, xm2 );     // 垂直滤波
        xm0 = loadzx_epu8_epi16( src2 );
        xm2 = _mm_adds_epi16( xm2, rnd );
        xm0 = _mm_slli_epi16( xm0, 6 );
        xm2 = _mm_adds_epi16( xm2, xm0 );
        xm2 = _mm_srai_epi16( xm2, 7 );
        xm2 = _mm_packus_epi16( xm2, xm2 );
        _mm_storel_epi64( (__m128i*)dst, xm2 );
        src += srcPitch;
        dst += dstPitch;
        src2 += srcPitch;
    }
}

// 1/4 像素水平滤波, [-1,-2,96,42,-7]
// x0 ~ x4 输入, y0 输出
#define QP_HOR_FILTER( x0, x1, x2, x3, x4, y0 ) \
    y0 = _mm_slli_epi16( x2, 6 );  /* x2*64 */  \
    x2 = _mm_slli_epi16( x2, 5 );  /* x2*32 */  \
    x1 = _mm_adds_epi16( x1, x1 );              \
    y0 = _mm_adds_epi16( y0, x2 ); /* x2*96 */  \
    x1 = _mm_add_epi16( x1, x0 );  /* x1*2+x0*/ \
    x2 = _mm_slli_epi16( x4, 3 );  /* x4*8 */   \
    y0 = _mm_subs_epi16( y0, x1 );              \
    x3 = _mm_adds_epi16( x3, x3 );  /* x3*2 */  \
    y0 = _mm_subs_epi16( y0, x2 );              \
    x0 = _mm_slli_epi16( x3, 2 );   /* x3*8 */  \
    y0 = _mm_adds_epi16( y0, x4 );              \
    x3 = _mm_adds_epi16( x3, x0 );  /* x3*10 */ \
    x0 = _mm_slli_epi16( x0, 2 );   /* x3*32 */ \
    y0 = _mm_adds_epi16( y0, x3 );              \
    y0 = _mm_adds_epi16( y0, x0 );
    
// 1/4 像素垂直滤波, [-1,-2,96,42,-7]
// x0 ~ x4 输入, y0 输出, t0, t1 为临时寄存器
#define QP_VER_FILTER( x0, x1, x2, x3, x4, t0, t1, y0 ) \
    y0 = _mm_slli_epi16( x2, 6 );  /* x2*64 */  \
    t0 = _mm_slli_epi16( x2, 5 );  /* x2*32 */  \
    t1 = _mm_adds_epi16( x1, x1 );              \
    y0 = _mm_adds_epi16( y0, t0 ); /* x2*96 */  \
    t1 = _mm_add_epi16( t1, x0 );  /* x1*2+x0*/ \
    t0 = _mm_slli_epi16( x4, 3 );  /* x4*8 */   \
    y0 = _mm_subs_epi16( y0, t1 );              \
    t1 = _mm_adds_epi16( x3, x3 );  /* x3*2 */  \
    y0 = _mm_subs_epi16( y0, t0 );              \
    t0 = _mm_slli_epi16( t1, 2 );   /* x3*8 */  \
    y0 = _mm_adds_epi16( y0, x4 );              \
    t1 = _mm_adds_epi16( t1, t0 );  /* x3*10 */ \
    t0 = _mm_slli_epi16( t0, 2 );   /* x3*32 */ \
    y0 = _mm_adds_epi16( y0, t1 );              \
    y0 = _mm_adds_epi16( y0, t0 );

// (0.25, 0)
static void MC_luma_8xN_x1y0( const uint8_t* src, int srcPitch, uint8_t* dst, int dstPitch, int N )
{
    __m128i xm0, xm1, xm2, xm3, xm4, ym0;
    __m128i zero = _mm_setzero_si128();
    __m128i rnd = _mm_set1_epi16( 64 );

    src -= 2;
    for( int i = 0; i < N; i++ )
    {
        xm0 = _mm_loadu_si128( (__m128i*)src );
        xm4 = _mm_unpackhi_epi8( xm0, zero );
        xm0 = _mm_unpacklo_epi8( xm0, zero );
        xm1 = _mm_alignr_epi8( xm4, xm0, 2 );
        xm2 = _mm_alignr_epi8( xm4, xm0, 4 );
        xm3 = _mm_alignr_epi8( xm4, xm0, 6 );
        xm4 = _mm_alignr_epi8( xm4, xm0, 8 );

        QP_HOR_FILTER( xm0, xm1, xm2, xm3, xm4, ym0 );
        ym0 = _mm_adds_epi16( ym0, rnd );
        ym0 = _mm_srai_epi16( ym0, 7 );
        ym0 = _mm_packus_epi16( ym0, ym0 );
        _mm_storel_epi64( (__m128i*)dst, ym0 );

        src += srcPitch;
        dst += dstPitch;
    }
}

// (0.75, 0)
static void MC_luma_8xN_x3y0( const uint8_t* src, int srcPitch, uint8_t* dst, int dstPitch, int N )
{
    __m128i xm0, xm1, xm2, xm3, xm4, ym0;
    __m128i zero = _mm_setzero_si128();
    __m128i rnd = _mm_set1_epi16( 64 );

    src -= 1;
    for( int i = 0; i < N; i++ )
    {
        xm0 = _mm_loadu_si128( (__m128i*)src );
        xm4 = _mm_unpackhi_epi8( xm0, zero );
        xm0 = _mm_unpacklo_epi8( xm0, zero );
        xm1 = _mm_alignr_epi8( xm4, xm0, 2 );
        xm2 = _mm_alignr_epi8( xm4, xm0, 4 );
        xm3 = _mm_alignr_epi8( xm4, xm0, 6 );
        xm4 = _mm_alignr_epi8( xm4, xm0, 8 );

        QP_HOR_FILTER( xm4, xm3, xm2, xm1, xm0, ym0 );
        ym0 = _mm_adds_epi16( ym0, rnd );
        ym0 = _mm_srai_epi16( ym0, 7 );
        ym0 = _mm_packus_epi16( ym0, ym0 );
        _mm_storel_epi64( (__m128i*)dst, ym0 );

        src += srcPitch;
        dst += dstPitch;
    }
}

// (0, 0.25)
static void MC_luma_8xN_x0y1( const uint8_t* src, int srcPitch, uint8_t* dst, int dstPitch, int N )
{
    assert( N == 8 || N == 16 );
    __m128i xm0, xm1, xm2, xm3, xm4, tm0, tm1, ym0;
    __m128i rnd = _mm_set1_epi16( 64 );

    xm0 = loadzx_epu8_epi16( src - srcPitch*2 );
    xm1 = loadzx_epu8_epi16( src - srcPitch );
    xm2 = loadzx_epu8_epi16( src );
    xm3 = loadzx_epu8_epi16( src + srcPitch );
    src += srcPitch * 2;

    while( 1 )
    {      
        xm4 = loadzx_epu8_epi16( src );
        QP_VER_FILTER( xm0, xm1, xm2, xm3, xm4, tm0, tm1, ym0 );
        ym0 = _mm_adds_epi16( ym0, rnd );
        ym0 = _mm_srai_epi16( ym0, 7 );
        ym0 = _mm_packus_epi16( ym0, ym0 );
        _mm_storel_epi64( (__m128i*)dst, ym0 );
        src += srcPitch;
        dst += dstPitch;

        xm0 = loadzx_epu8_epi16( src );
        QP_VER_FILTER( xm1, xm2, xm3, xm4, xm0, tm0, tm1, ym0 );
        ym0 = _mm_adds_epi16( ym0, rnd );
        ym0 = _mm_srai_epi16( ym0, 7 );
        ym0 = _mm_packus_epi16( ym0, ym0 );
        _mm_storel_epi64( (__m128i*)dst, ym0 );
        src += srcPitch;
        dst += dstPitch;

        xm1 = loadzx_epu8_epi16( src );
        QP_VER_FILTER( xm2, xm3, xm4, xm0, xm1, tm0, tm1, ym0 );
        ym0 = _mm_adds_epi16( ym0, rnd );
        ym0 = _mm_srai_epi16( ym0, 7 );
        ym0 = _mm_packus_epi16( ym0, ym0 );
        _mm_storel_epi64( (__m128i*)dst, ym0 );
        src += srcPitch;
        dst += dstPitch;

        xm2 = loadzx_epu8_epi16( src );
        QP_VER_FILTER( xm3, xm4, xm0, xm1, xm2, tm0, tm1, ym0 );
        ym0 = _mm_adds_epi16( ym0, rnd );
        ym0 = _mm_srai_epi16( ym0, 7 );
        ym0 = _mm_packus_epi16( ym0, ym0 );
        _mm_storel_epi64( (__m128i*)dst, ym0 );

        if( (N -= 4) == 0 )
            break;

        src += srcPitch;
        dst += dstPitch;
        xm3 = xm2;
        xm2 = xm1;
        xm1 = xm0;
        xm0 = xm4;
    }
}

// (0, 0.75)
static void MC_luma_8xN_x0y3( const uint8_t* src, int srcPitch, uint8_t* dst, int dstPitch, int N )
{
    assert( N == 8 || N == 16 );
    __m128i xm0, xm1, xm2, xm3, xm4, tm0, tm1, ym0;
    __m128i rnd = _mm_set1_epi16( 64 );

    xm0 = loadzx_epu8_epi16( src - srcPitch );
    xm1 = loadzx_epu8_epi16( src );
    xm2 = loadzx_epu8_epi16( src + srcPitch );
    xm3 = loadzx_epu8_epi16( src + srcPitch*2 );
    src += srcPitch * 3;

    while( 1 )
    {      
        xm4 = loadzx_epu8_epi16( src );
        QP_VER_FILTER( xm4, xm3, xm2, xm1, xm0, tm0, tm1, ym0 );
        ym0 = _mm_adds_epi16( ym0, rnd );
        ym0 = _mm_srai_epi16( ym0, 7 );
        ym0 = _mm_packus_epi16( ym0, ym0 );
        _mm_storel_epi64( (__m128i*)dst, ym0 );
        src += srcPitch;
        dst += dstPitch;

        xm0 = loadzx_epu8_epi16( src );
        QP_VER_FILTER( xm0, xm4, xm3, xm2, xm1, tm0, tm1, ym0 );
        ym0 = _mm_adds_epi16( ym0, rnd );
        ym0 = _mm_srai_epi16( ym0, 7 );
        ym0 = _mm_packus_epi16( ym0, ym0 );
        _mm_storel_epi64( (__m128i*)dst, ym0 );
        src += srcPitch;
        dst += dstPitch;

        xm1 = loadzx_epu8_epi16( src );
        QP_VER_FILTER( xm1, xm0, xm4, xm3, xm2, tm0, tm1, ym0 );
        ym0 = _mm_adds_epi16( ym0, rnd );
        ym0 = _mm_srai_epi16( ym0, 7 );
        ym0 = _mm_packus_epi16( ym0, ym0 );
        _mm_storel_epi64( (__m128i*)dst, ym0 );
        src += srcPitch;
        dst += dstPitch;

        xm2 = loadzx_epu8_epi16( src );
        QP_VER_FILTER( xm2, xm1, xm0, xm4, xm3, tm0, tm1, ym0 );
        ym0 = _mm_adds_epi16( ym0, rnd );
        ym0 = _mm_srai_epi16( ym0, 7 );
        ym0 = _mm_packus_epi16( ym0, ym0 );
        _mm_storel_epi64( (__m128i*)dst, ym0 );
       
        if( (N -= 4) == 0 )
            break;

        src += srcPitch;
        dst += dstPitch;
        xm3 = xm2;
        xm2 = xm1;
        xm1 = xm0;
        xm0 = xm4;
    }
}

// 半像素点水平滤波, 存储中间值, 目标行宽为 16 个像素
static void MC_luma_16xN_hor_temp( const uint8_t* src, int srcPitch, int16_t* dst, int N )
{
    assert( N == 12 || N == 20 );
    __m128i xm0, xm1, xm2, xm3, ym0;
    src -= 1;
    for( int i = 0; i < N; i++ )
    {
        xm0 = _mm_loadu_si128( (__m128i*)src );
        HP_HOR_FILTER( xm0, xm1, xm2, xm3, ym0 );
        _mm_store_si128( (__m128i*)dst, ym0 );

        xm0 = _mm_loadu_si128( (__m128i*)(src + 8) );
        HP_HOR_FILTER( xm0, xm1, xm2, xm3, ym0 );
        _mm_store_si128( (__m128i*)(dst + 8), ym0 );

        src += srcPitch;
        dst += 16;
    }
}

// 半像素点水平滤波, 存储中间值, 目标行宽为 16 个像素
static void MC_luma_8xN_hor_temp( const uint8_t* src, int srcPitch, int16_t* dst, int N )
{
    assert( N == 12 || N == 20 );
    __m128i xm0, xm1, xm2, xm3, ym0;
    src -= 1;
    for( int i = 0; i < N; i++ )
    {
        xm0 = _mm_loadu_si128( (__m128i*)src );
        HP_HOR_FILTER( xm0, xm1, xm2, xm3, ym0 );
        _mm_store_si128( (__m128i*)dst, ym0 );

        src += srcPitch;
        dst += 16;
    }
}

// 半像素点垂直滤波, 存储中间值, 目标行宽为 24 个像素
static void MC_luma_24xN_ver_temp( const uint8_t* src, int srcPitch, int16_t* dst, int N )
{
    assert( N == 8 || N == 16 );
    __m128i xm0, xm1, xm2, xm3;
    __m128i ym0, ym1, ym2, ym3;
    __m128i zm0, zm1, zm2, zm3, sm0;

    xm0 = loadzx_epu8_epi16( src - srcPitch );
    ym0 = loadzx_epu8_epi16( src - srcPitch + 8 );
    zm0 = loadzx_epu8_epi16( src - srcPitch + 16 );
    xm1 = loadzx_epu8_epi16( src );
    ym1 = loadzx_epu8_epi16( src + 8 );
    zm1 = loadzx_epu8_epi16( src + 16 );
    xm2 = loadzx_epu8_epi16( src + srcPitch );
    ym2 = loadzx_epu8_epi16( src + srcPitch + 8 );
    zm2 = loadzx_epu8_epi16( src + srcPitch + 16 );
    src += srcPitch * 2;

    for( int i = 0; i < N; i += 4 )
    {
        // row 0
        xm3 = loadzx_epu8_epi16( src );
        HP_VER_FILTER( xm0, xm1, xm2, xm3, ym3, zm3, sm0 );
        _mm_store_si128( (__m128i*)(dst + 0), sm0 );
        ym3 = loadzx_epu8_epi16( src + 8 );
        HP_VER_FILTER( ym0, ym1, ym2, ym3, xm0, zm3, sm0 );
        _mm_store_si128( (__m128i*)(dst + 8), sm0 );
        zm3 = loadzx_epu8_epi16( src + 16 );
        HP_VER_FILTER( zm0, zm1, zm2, zm3, xm0, ym0, sm0 );
        _mm_store_si128( (__m128i*)(dst + 16), sm0 );
        src += srcPitch;

        // row 1
        xm0 = loadzx_epu8_epi16( src );
        HP_VER_FILTER( xm1, xm2, xm3, xm0, ym0, zm0, sm0 );
        _mm_store_si128( (__m128i*)(dst + 24), sm0 );
        ym0 = loadzx_epu8_epi16( src + 8 );
        HP_VER_FILTER( ym1, ym2, ym3, ym0, xm1, zm0, sm0 );
        _mm_store_si128( (__m128i*)(dst + 32), sm0 );
        zm0 = loadzx_epu8_epi16( src + 16 );
        HP_VER_FILTER( zm1, zm2, zm3, zm0, xm1, ym1, sm0 );
        _mm_store_si128( (__m128i*)(dst + 40), sm0 );
        src += srcPitch;

        // row 2
        xm1 = loadzx_epu8_epi16( src );
        HP_VER_FILTER( xm2, xm3, xm0, xm1, ym1, zm1, sm0 );
        _mm_store_si128( (__m128i*)(dst + 48), sm0 );
        ym1 = loadzx_epu8_epi16( src + 8 );
        HP_VER_FILTER( ym2, ym3, ym0, ym1, xm2, zm1, sm0 );
        _mm_store_si128( (__m128i*)(dst + 56), sm0 );
        zm1 = loadzx_epu8_epi16( src + 16 );
        HP_VER_FILTER( zm2, zm3, zm0, zm1, xm2, ym2, sm0 );
        _mm_store_si128( (__m128i*)(dst + 64), sm0 );
        src += srcPitch;

        // row 3
        xm2 = loadzx_epu8_epi16( src );
        HP_VER_FILTER( xm3, xm0, xm1, xm2, ym2, zm2, sm0 );
        _mm_store_si128( (__m128i*)(dst + 72), sm0 );
        ym2 = loadzx_epu8_epi16( src + 8 );
        HP_VER_FILTER( ym3, ym0, ym1, ym2, xm3, zm2, sm0 );
        _mm_store_si128( (__m128i*)(dst + 80), sm0 );
        zm2 = loadzx_epu8_epi16( src + 16 );
        HP_VER_FILTER( zm3, zm0, zm1, zm2, xm3, ym3, sm0 );
        _mm_store_si128( (__m128i*)(dst + 88), sm0 );
        src += srcPitch;

        dst += 24 * 4;
    }
}

// 半像素点垂直滤波, 存储中间值, 目标行宽为 24 个像素
static void MC_luma_16xN_ver_temp( const uint8_t* src, int srcPitch, int16_t* dst, int N )
{
    assert( N == 8 || N == 16 );
    __m128i xm0, xm1, xm2, xm3;
    __m128i ym0, ym1, ym2, ym3;
    __m128i tm0, tm1, sm0, zero = _mm_setzero_si128();

    xm0 = _mm_loadu_si128( (__m128i*)(src - srcPitch) );
    xm1 = _mm_loadu_si128( (__m128i*)src );
    xm2 = _mm_loadu_si128( (__m128i*)(src + srcPitch) );
    src += srcPitch * 2;
    ym0 = _mm_unpackhi_epi8( xm0, zero );
    ym1 = _mm_unpackhi_epi8( xm1, zero );
    ym2 = _mm_unpackhi_epi8( xm2, zero );
    xm0 = _mm_unpacklo_epi8( xm0, zero );
    xm1 = _mm_unpacklo_epi8( xm1, zero );
    xm2 = _mm_unpacklo_epi8( xm2, zero );
 
    for( int i = 0; i < N; i += 4 )
    {
        // row 0
        xm3 = _mm_loadu_si128( (__m128i*)src );
        ym3 = _mm_unpackhi_epi8( xm3, zero );
        xm3 = _mm_unpacklo_epi8( xm3, zero );
        HP_VER_FILTER( xm0, xm1, xm2, xm3, tm0, tm1, sm0 );
        _mm_store_si128( (__m128i*)dst, sm0 );
        HP_VER_FILTER( ym0, ym1, ym2, ym3, tm0, tm1, sm0 );
        _mm_store_si128( (__m128i*)(dst + 8), sm0 );
        src += srcPitch;

        // row 1
        xm0 = _mm_loadu_si128( (__m128i*)src );
        ym0 = _mm_unpackhi_epi8( xm0, zero );
        xm0 = _mm_unpacklo_epi8( xm0, zero );
        HP_VER_FILTER( xm1, xm2, xm3, xm0, tm0, tm1, sm0 );
        _mm_store_si128( (__m128i*)(dst + 24), sm0 );
        HP_VER_FILTER( ym1, ym2, ym3, ym0, tm0, tm1, sm0 );
        _mm_store_si128( (__m128i*)(dst + 32), sm0 );
        src += srcPitch;

        // row 2
        xm1 = _mm_loadu_si128( (__m128i*)src );
        ym1 = _mm_unpackhi_epi8( xm1, zero );
        xm1 = _mm_unpacklo_epi8( xm1, zero );
        HP_VER_FILTER( xm2, xm3, xm0, xm1, tm0, tm1, sm0 );
        _mm_store_si128( (__m128i*)(dst + 48), sm0 );
        HP_VER_FILTER( ym2, ym3, ym0, ym1, tm0, tm1, sm0 );
        _mm_store_si128( (__m128i*)(dst + 56), sm0 );
        src += srcPitch;

        // row 3
        xm2 = _mm_loadu_si128( (__m128i*)src );
        ym2 = _mm_unpackhi_epi8( xm2, zero );
        xm2 = _mm_unpacklo_epi8( xm2, zero );
        HP_VER_FILTER( xm3, xm0, xm1, xm2, tm0, tm1, sm0 );
        _mm_store_si128( (__m128i*)(dst + 72), sm0 );
        HP_VER_FILTER( ym3, ym0, ym1, ym2, tm0, tm1, sm0 );
        _mm_store_si128( (__m128i*)(dst + 80), sm0 );
        src += srcPitch;

        dst += 96;
    }
}

// (0.25, 0.5), 输入数据行宽为 24 像素
static void MC_luma_8xN_x1y2( const int16_t* src, uint8_t* dst, int dstPitch, int N )
{
    assert( ((uintptr_t)src & 15) == 0 );
    __m128i xm0, xm1, xm2, xm3, xm4, ym0;
    __m128i tm0, tm1, tm2, tm3, tm4, ym1;
    __m128i rnd = _mm_set1_epi16( 32 );
    __m128i msk = _mm_set1_epi16( 15 );

    for( int i = 0; i < N; i++ )
    {
        xm0 = _mm_load_si128( (__m128i*)src );
        xm4 = _mm_load_si128( (__m128i*)(src + 8) );
        xm1 = _mm_alignr_epi8( xm4, xm0, 2 );
        xm2 = _mm_alignr_epi8( xm4, xm0, 4 );
        xm3 = _mm_alignr_epi8( xm4, xm0, 6 );
        xm4 = _mm_alignr_epi8( xm4, xm0, 8 );

        // 由于输入数据最大可能为 255*8, 后续运算可能溢出, 分步处理
        tm0 = _mm_and_si128( xm0, msk );
        tm1 = _mm_and_si128( xm1, msk );
        tm2 = _mm_and_si128( xm2, msk );
        tm3 = _mm_and_si128( xm3, msk );
        tm4 = _mm_and_si128( xm4, msk );
        xm0 = _mm_srai_epi16( xm0, 4 );
        xm1 = _mm_srai_epi16( xm1, 4 );
        xm2 = _mm_srai_epi16( xm2, 4 );
        xm3 = _mm_srai_epi16( xm3, 4 );
        xm4 = _mm_srai_epi16( xm4, 4 );

        QP_HOR_FILTER( tm0, tm1, tm2, tm3, tm4, ym1 );
        QP_HOR_FILTER( xm0, xm1, xm2, xm3, xm4, ym0 );  
        ym1 = _mm_srai_epi16( ym1, 4 );
        ym0 = _mm_adds_epi16( ym0, ym1 );
        ym0 = _mm_adds_epi16( ym0, rnd );
        ym0 = _mm_srai_epi16( ym0, 6 );
        ym0 = _mm_packus_epi16( ym0, ym0 );
        _mm_storel_epi64( (__m128i*)dst, ym0 );

        src += 24;
        dst += dstPitch;
    }
}

// (0.75, 0.5), 输入数据行宽为 24 像素
static void MC_luma_8xN_x3y2( const int16_t* src, uint8_t* dst, int dstPitch, int N )
{
    assert( ((uintptr_t)src & 15) == 0 );
    __m128i xm0, xm1, xm2, xm3, xm4, ym0;
    __m128i tm0, tm1, tm2, tm3, tm4, ym1;
    __m128i rnd = _mm_set1_epi16( 32 );
    __m128i msk = _mm_set1_epi16( 15 );

    for( int i = 0; i < N; i++ )
    {
        xm0 = _mm_load_si128( (__m128i*)src );
        xm4 = _mm_load_si128( (__m128i*)(src + 8) );
        xm1 = _mm_alignr_epi8( xm4, xm0, 2 );
        xm2 = _mm_alignr_epi8( xm4, xm0, 4 );
        xm3 = _mm_alignr_epi8( xm4, xm0, 6 );
        xm4 = _mm_alignr_epi8( xm4, xm0, 8 );

        // 由于输入数据最大可能为 255*8, 后续运算可能溢出, 分步处理
        tm0 = _mm_and_si128( xm0, msk );
        tm1 = _mm_and_si128( xm1, msk );
        tm2 = _mm_and_si128( xm2, msk );
        tm3 = _mm_and_si128( xm3, msk );
        tm4 = _mm_and_si128( xm4, msk );
        xm0 = _mm_srai_epi16( xm0, 4 );
        xm1 = _mm_srai_epi16( xm1, 4 );
        xm2 = _mm_srai_epi16( xm2, 4 );
        xm3 = _mm_srai_epi16( xm3, 4 );
        xm4 = _mm_srai_epi16( xm4, 4 );

        QP_HOR_FILTER( tm4, tm3, tm2, tm1, tm0, ym1 );
        QP_HOR_FILTER( xm4, xm3, xm2, xm1, xm0, ym0 );
        ym1 = _mm_srai_epi16( ym1, 4 );
        ym0 = _mm_adds_epi16( ym0, ym1 );
        ym0 = _mm_adds_epi16( ym0, rnd );
        ym0 = _mm_srai_epi16( ym0, 6 );
        ym0 = _mm_packus_epi16( ym0, ym0 );
        _mm_storel_epi64( (__m128i*)dst, ym0 );

        src += 24;
        dst += dstPitch;
    }
}

// (0.5, 0.25), 输入数据行宽为 16 像素
static void MC_luma_8xN_x2y1( const int16_t* src, uint8_t* dst, int dstPitch, int N )
{
    assert( ((uintptr_t)src & 15) == 0 );
    assert( N == 8 || N == 16 );
    __m128i xm0, xm1, xm2, xm3, xm4, ym0;
    __m128i tm0, tm1, tm2, tm3, tm4, ym1;
    __m128i em0, em1;                           // 临时寄存器
    __m128i rnd = _mm_set1_epi16( 32 );
    __m128i msk = _mm_set1_epi16( 15 );

    xm0 = _mm_load_si128( (__m128i*)src );
    xm1 = _mm_load_si128( (__m128i*)(src + 16) );
    xm2 = _mm_load_si128( (__m128i*)(src + 32) );
    xm3 = _mm_load_si128( (__m128i*)(src + 48) );
    src += 64;

    // 由于输入数据最大可能为 255*8, 后续运算可能溢出, 分步处理
    tm0 = _mm_and_si128( xm0, msk );
    tm1 = _mm_and_si128( xm1, msk );
    tm2 = _mm_and_si128( xm2, msk );
    tm3 = _mm_and_si128( xm3, msk ); 
    xm0 = _mm_srai_epi16( xm0, 4 );
    xm1 = _mm_srai_epi16( xm1, 4 );
    xm2 = _mm_srai_epi16( xm2, 4 );
    xm3 = _mm_srai_epi16( xm3, 4 );
    
    while( 1 )
    {      
        xm4 = _mm_load_si128( (__m128i*)src );
        tm4 = _mm_and_si128( xm4, msk );
        xm4 = _mm_srai_epi16( xm4, 4 );
        QP_VER_FILTER( tm0, tm1, tm2, tm3, tm4, em0, em1, ym1 );
        QP_VER_FILTER( xm0, xm1, xm2, xm3, xm4, em0, em1, ym0 );
        ym1 = _mm_srai_epi16( ym1, 4 );
        ym0 = _mm_adds_epi16( ym0, ym1 );
        ym0 = _mm_adds_epi16( ym0, rnd );
        ym0 = _mm_srai_epi16( ym0, 6 );
        ym0 = _mm_packus_epi16( ym0, ym0 );
        _mm_storel_epi64( (__m128i*)(dst), ym0 );

        xm0 = _mm_load_si128( (__m128i*)(src + 16) );
        tm0 = _mm_and_si128( xm0, msk );
        xm0 = _mm_srai_epi16( xm0, 4 );
        QP_VER_FILTER( tm1, tm2, tm3, tm4, tm0, em0, em1, ym1 );
        QP_VER_FILTER( xm1, xm2, xm3, xm4, xm0, em0, em1, ym0 );
        ym1 = _mm_srai_epi16( ym1, 4 );
        ym0 = _mm_adds_epi16( ym0, ym1 );
        ym0 = _mm_adds_epi16( ym0, rnd );
        ym0 = _mm_srai_epi16( ym0, 6 );
        ym0 = _mm_packus_epi16( ym0, ym0 );
        _mm_storel_epi64( (__m128i*)(dst + dstPitch), ym0 );

        xm1 = _mm_load_si128( (__m128i*)(src + 32) );
        tm1 = _mm_and_si128( xm1, msk );
        xm1 = _mm_srai_epi16( xm1, 4 );
        QP_VER_FILTER( tm2, tm3, tm4, tm0, tm1, em0, em1, ym1 );
        QP_VER_FILTER( xm2, xm3, xm4, xm0, xm1, em0, em1, ym0 );
        ym1 = _mm_srai_epi16( ym1, 4 );
        ym0 = _mm_adds_epi16( ym0, ym1 );
        ym0 = _mm_adds_epi16( ym0, rnd );
        ym0 = _mm_srai_epi16( ym0, 6 );
        ym0 = _mm_packus_epi16( ym0, ym0 );
        _mm_storel_epi64( (__m128i*)(dst + dstPitch*2), ym0 );

        xm2 = _mm_load_si128( (__m128i*)(src + 48) );
        tm2 = _mm_and_si128( xm2, msk );
        xm2 = _mm_srai_epi16( xm2, 4 );
        QP_VER_FILTER( tm3, tm4, tm0, tm1, tm2, em0, em1, ym1 );
        QP_VER_FILTER( xm3, xm4, xm0, xm1, xm2, em0, em1, ym0 );
        ym1 = _mm_srai_epi16( ym1, 4 );
        ym0 = _mm_adds_epi16( ym0, ym1 );
        ym0 = _mm_adds_epi16( ym0, rnd );
        ym0 = _mm_srai_epi16( ym0, 6 );
        ym0 = _mm_packus_epi16( ym0, ym0 );
        _mm_storel_epi64( (__m128i*)(dst + dstPitch*3), ym0 );

        if( (N -= 4) == 0 )
            break;

        src += 64;
        dst += dstPitch * 4;
        tm3 = tm2;
        tm2 = tm1;
        tm1 = tm0;
        tm0 = tm4;
        xm3 = xm2;
        xm2 = xm1;
        xm1 = xm0;
        xm0 = xm4;
    }
}

// (0.5, 0.75), 输入数据行宽为 16 像素
static void MC_luma_8xN_x2y3( const int16_t* src, uint8_t* dst, int dstPitch, int N )
{
    assert( ((uintptr_t)src & 15) == 0 );
    assert( N == 8 || N == 16 );
    __m128i xm0, xm1, xm2, xm3, xm4, ym0;
    __m128i tm0, tm1, tm2, tm3, tm4, ym1;
    __m128i em0, em1;                           // 临时寄存器
    __m128i rnd = _mm_set1_epi16( 32 );
    __m128i msk = _mm_set1_epi16( 15 );

    xm0 = _mm_load_si128( (__m128i*)src );
    xm1 = _mm_load_si128( (__m128i*)(src + 16) );
    xm2 = _mm_load_si128( (__m128i*)(src + 32) );
    xm3 = _mm_load_si128( (__m128i*)(src + 48) );
    src += 64;

    // 由于输入数据最大可能为 255*8, 后续运算可能溢出, 分步处理
    tm0 = _mm_and_si128( xm0, msk );
    tm1 = _mm_and_si128( xm1, msk );
    tm2 = _mm_and_si128( xm2, msk );
    tm3 = _mm_and_si128( xm3, msk ); 
    xm0 = _mm_srai_epi16( xm0, 4 );
    xm1 = _mm_srai_epi16( xm1, 4 );
    xm2 = _mm_srai_epi16( xm2, 4 );
    xm3 = _mm_srai_epi16( xm3, 4 );

    while( 1 )
    {      
        xm4 = _mm_load_si128( (__m128i*)src );
        tm4 = _mm_and_si128( xm4, msk );
        xm4 = _mm_srai_epi16( xm4, 4 );
        QP_VER_FILTER( tm4, tm3, tm2, tm1, tm0, em0, em1, ym1 );
        QP_VER_FILTER( xm4, xm3, xm2, xm1, xm0, em0, em1, ym0 );
        ym1 = _mm_srai_epi16( ym1, 4 );
        ym0 = _mm_adds_epi16( ym0, ym1 );
        ym0 = _mm_adds_epi16( ym0, rnd );
        ym0 = _mm_srai_epi16( ym0, 6 );
        ym0 = _mm_packus_epi16( ym0, ym0 );
        _mm_storel_epi64( (__m128i*)(dst), ym0 );

        xm0 = _mm_load_si128( (__m128i*)(src + 16) );
        tm0 = _mm_and_si128( xm0, msk );
        xm0 = _mm_srai_epi16( xm0, 4 );
        QP_VER_FILTER( tm0, tm4, tm3, tm2, tm1, em0, em1, ym1 );
        QP_VER_FILTER( xm0, xm4, xm3, xm2, xm1, em0, em1, ym0 );
        ym1 = _mm_srai_epi16( ym1, 4 );
        ym0 = _mm_adds_epi16( ym0, ym1 );
        ym0 = _mm_adds_epi16( ym0, rnd );
        ym0 = _mm_srai_epi16( ym0, 6 );
        ym0 = _mm_packus_epi16( ym0, ym0 );
        _mm_storel_epi64( (__m128i*)(dst + dstPitch), ym0 );

        xm1 = _mm_load_si128( (__m128i*)(src + 32) );
        tm1 = _mm_and_si128( xm1, msk );
        xm1 = _mm_srai_epi16( xm1, 4 );
        QP_VER_FILTER( tm1, tm0, tm4, tm3, tm2, em0, em1, ym1 );
        QP_VER_FILTER( xm1, xm0, xm4, xm3, xm2, em0, em1, ym0 );
        ym1 = _mm_srai_epi16( ym1, 4 );
        ym0 = _mm_adds_epi16( ym0, ym1 );
        ym0 = _mm_adds_epi16( ym0, rnd );
        ym0 = _mm_srai_epi16( ym0, 6 );
        ym0 = _mm_packus_epi16( ym0, ym0 );
        _mm_storel_epi64( (__m128i*)(dst + dstPitch*2), ym0 );

        xm2 = _mm_load_si128( (__m128i*)(src + 48) );
        tm2 = _mm_and_si128( xm2, msk );
        xm2 = _mm_srai_epi16( xm2, 4 );
        QP_VER_FILTER( tm2, tm1, tm0, tm4, tm3, em0, em1, ym1 );
        QP_VER_FILTER( xm2, xm1, xm0, xm4, xm3, em0, em1, ym0 );
        ym1 = _mm_srai_epi16( ym1, 4 );
        ym0 = _mm_adds_epi16( ym0, ym1 );
        ym0 = _mm_adds_epi16( ym0, rnd );
        ym0 = _mm_srai_epi16( ym0, 6 );
        ym0 = _mm_packus_epi16( ym0, ym0 );
        _mm_storel_epi64( (__m128i*)(dst + dstPitch*3), ym0 );

        if( (N -= 4) == 0 )
            break;

        src += 64;
        dst += dstPitch * 4;
        tm3 = tm2;
        tm2 = tm1;
        tm1 = tm0;
        tm0 = tm4;
        xm3 = xm2;
        xm2 = xm1;
        xm1 = xm0;
        xm0 = xm4;
    }
}

//======================================================================================================================

// 判断 rc1 是否包含在 rc2 中
static inline bool in_rect_of( const Rect* rc1, const Rect* rc2 )
{
    __m128i xm0 = _mm_loadu_si128( (__m128i*)rc1 );
    __m128i xm1 = _mm_loadu_si128( (__m128i*)rc2 );
    const int mask = _mm_movemask_epi8( _mm_cmpgt_epi32( xm0, xm1 ) );
    return mask == 0xFF;
}

// 检查参考帧数据是否已解码, 未解码等待
static inline void check_ref_data( FrmDecContext* ctx, DecFrame* refFrame, int y )
{
    assert( refFrame->decState );

    if( ctx->curFrame == refFrame ) // 同一帧的第二场, 第一场数据总是已解码
    {
        assert( ctx->fieldIdx == 1 );
        return;
    }

    int refLine = y * (2 - ctx->frameCoding);   // 可能存在帧场自适应编码, 全部转化为帧的刻度
    if( refLine <= refFrame->decState->m_lineReady )
    {
        return;
    }

    // 等待参考帧, +32 是为了让参考帧多解码一些
    ctx->refRequest.m_refLine = refLine + 32;
    refFrame->decState->wait_request( &ctx->refRequest );
}

// 16x16 亮度分量帧间预测
void luma_inter_pred_16x16( FrmDecContext* ctx, const RefPicture* refpic, uint8_t* dst, int dstPitch, int x, int y )
{
    const int xInt = x >> 2;
    const int yInt = y >> 2;         
    const int xy = ((y & 3) << 2) | (x & 3); 
    int srcPitch = ctx->picPitch[0];
    const uint8_t* src = nullptr;

    // 检查参考帧数据是否已解码, 未可用则等待
    check_ref_data( ctx, refpic->pframe, yInt + 19 );

    Rect rc1 = {xInt - 2, yInt - 2, xInt + 19, yInt + 19};
    if( in_rect_of( &rc1, &ctx->refRcLuma ) )
    {
        // 在参考帧实际图像范围内
        src = refpic->plane[0] + yInt * srcPitch + xInt;
    }
    else
    {
        // 对参考帧边界进行扩展, 得到参考数据
        McExtRect refRect;
        refRect.x = xInt - 2;
        refRect.y = yInt - 2;
        refRect.height = 16 + 5;
        refRect.picWidth = ctx->picWidth;
        refRect.picHeight = ctx->picHeight;
        refRect.buf = ctx->tempBuf;
        get_ref_data_32xN( refpic->plane[0], srcPitch, refRect );
        srcPitch = 32;
        src = refRect.buf + 32 * 2 + 2;
    }

    int16_t* tmpBuf = (int16_t*)(ctx->tempBuf + 768);
    switch( xy )
    {
    case 0:             // 0, 0
        MC_copy_16xN( src, srcPitch, dst, dstPitch, 16 );
        break;
    case 1:             // 0.25, 0
        MC_luma_8xN_x1y0( src,     srcPitch, dst,     dstPitch, 16 );
        MC_luma_8xN_x1y0( src + 8, srcPitch, dst + 8, dstPitch, 16 );
        break;
    case 2:             // 0.5, 0
        MC_luma_8xN_x2y0( src,     srcPitch, dst,     dstPitch, 16 );
        MC_luma_8xN_x2y0( src + 8, srcPitch, dst + 8, dstPitch, 16 );
        break;
    case 3:             // 0.75, 0
        MC_luma_8xN_x3y0( src,     srcPitch, dst,     dstPitch, 16 );
        MC_luma_8xN_x3y0( src + 8, srcPitch, dst + 8, dstPitch, 16 );
        break;
    case 4:             // 0, 0.25
        MC_luma_8xN_x0y1( src,     srcPitch, dst,     dstPitch, 16 );
        MC_luma_8xN_x0y1( src + 8, srcPitch, dst + 8, dstPitch, 16 );
        break;
    case 5:             // 0.25, 0.25
        MC_luma_8xN_x1y1( src,     srcPitch, dst,     dstPitch, 16, src );
        MC_luma_8xN_x1y1( src + 8, srcPitch, dst + 8, dstPitch, 16, src + 8 );
        break;
    case 6:             // 0.5, 0.25
        MC_luma_16xN_hor_temp( src - srcPitch*2, srcPitch, tmpBuf, 20 );
        MC_luma_8xN_x2y1( tmpBuf,     dst,     dstPitch, 16 );
        MC_luma_8xN_x2y1( tmpBuf + 8, dst + 8, dstPitch, 16 );
        break;
    case 7:             // 0.75, 0.25
        MC_luma_8xN_x1y1( src,     srcPitch, dst,     dstPitch, 16, src + 1 );
        MC_luma_8xN_x1y1( src + 8, srcPitch, dst + 8, dstPitch, 16, src + 9 );
        break;  
    case 8:             // 0, 0.5
        MC_luma_8xN_x0y2( src,     srcPitch, dst,     dstPitch, 16 );
        MC_luma_8xN_x0y2( src + 8, srcPitch, dst + 8, dstPitch, 16 );
        break;
    case 9:             // 0.25, 0.5
        MC_luma_24xN_ver_temp( src - 2, srcPitch, tmpBuf, 16 );
        MC_luma_8xN_x1y2( tmpBuf,     dst,     dstPitch, 16 );
        MC_luma_8xN_x1y2( tmpBuf + 8, dst + 8, dstPitch, 16 );
        break;
    case 10:            // 0.5, 0.5
        MC_luma_8xN_x2y2( src,     srcPitch, dst,     dstPitch, 16 );
        MC_luma_8xN_x2y2( src + 8, srcPitch, dst + 8, dstPitch, 16 );
        break;
    case 11:            // 0.75, 0.5
        MC_luma_24xN_ver_temp( src - 1, srcPitch, tmpBuf, 16 );
        MC_luma_8xN_x3y2( tmpBuf,     dst,     dstPitch, 16 );
        MC_luma_8xN_x3y2( tmpBuf + 8, dst + 8, dstPitch, 16 );
        break;
    case 12:            // 0, 0.75
        MC_luma_8xN_x0y3( src,     srcPitch, dst,     dstPitch, 16 );
        MC_luma_8xN_x0y3( src + 8, srcPitch, dst + 8, dstPitch, 16 );
        break;
    case 13:            // 0.25, 0.75
        MC_luma_8xN_x1y1( src,     srcPitch, dst,     dstPitch, 16, src + srcPitch );
        MC_luma_8xN_x1y1( src + 8, srcPitch, dst + 8, dstPitch, 16, src + srcPitch + 8 );
        break;
    case 14:            // 0.5, 0.75
        MC_luma_16xN_hor_temp( src - srcPitch, srcPitch, tmpBuf, 20 );
        MC_luma_8xN_x2y3( tmpBuf,     dst,     dstPitch, 16 );
        MC_luma_8xN_x2y3( tmpBuf + 8, dst + 8, dstPitch, 16 );
        break;
    case 15:            // 0.75, 0.75
        MC_luma_8xN_x1y1( src,     srcPitch, dst,     dstPitch, 16, src + srcPitch + 1 );
        MC_luma_8xN_x1y1( src + 8, srcPitch, dst + 8, dstPitch, 16, src + srcPitch + 9 );
        break;
    default:
        assert(0);
        break;
    }
}

// 16x8 亮度分量帧间预测
void luma_inter_pred_16x8( FrmDecContext* ctx, const RefPicture* refpic, uint8_t* dst, int dstPitch, int x, int y )
{
    const int xInt = x >> 2;
    const int yInt = y >> 2;         
    const int xy = ((y & 3) << 2) | (x & 3);
    int srcPitch = ctx->picPitch[0];
    const uint8_t* src = nullptr;

    // 检查参考帧数据是否已解码, 未可用则等待
    check_ref_data( ctx, refpic->pframe, yInt + 11 );

    Rect rc1 = {xInt - 2, yInt - 2, xInt + 19, yInt + 11};
    if( in_rect_of( &rc1, &ctx->refRcLuma ) )
    {
        // 在参考帧实际图像范围内
        src = refpic->plane[0] + yInt * srcPitch + xInt;
    }
    else
    {
        // 对参考帧边界进行扩展, 得到参考数据
        McExtRect refRect;
        refRect.x = xInt - 2;
        refRect.y = yInt - 2;
        refRect.height = 8 + 5;
        refRect.picWidth = ctx->picWidth;
        refRect.picHeight = ctx->picHeight;
        refRect.buf = ctx->tempBuf;
        get_ref_data_32xN( refpic->plane[0], srcPitch, refRect );
        srcPitch = 32;
        src = refRect.buf + 32 * 2 + 2;     
    }

    int16_t* tmpBuf = (int16_t*)(ctx->tempBuf + 768);
    switch( xy )
    {
    case 0:             // 0, 0
        MC_copy_16xN( src, srcPitch, dst, dstPitch, 8 );
        break;
    case 1:             // 0.25, 0
        MC_luma_8xN_x1y0( src,     srcPitch, dst,     dstPitch, 8 );
        MC_luma_8xN_x1y0( src + 8, srcPitch, dst + 8, dstPitch, 8 );
        break;
    case 2:             // 0.5, 0
        MC_luma_8xN_x2y0( src,     srcPitch, dst,     dstPitch, 8 );
        MC_luma_8xN_x2y0( src + 8, srcPitch, dst + 8, dstPitch, 8 );
        break;
    case 3:             // 0.75, 0
        MC_luma_8xN_x3y0( src,     srcPitch, dst,     dstPitch, 8 );
        MC_luma_8xN_x3y0( src + 8, srcPitch, dst + 8, dstPitch, 8 );
        break;
    case 4:             // 0, 0.25
        MC_luma_8xN_x0y1( src,     srcPitch, dst,     dstPitch, 8 );
        MC_luma_8xN_x0y1( src + 8, srcPitch, dst + 8, dstPitch, 8 );
        break;
    case 5:             // 0.25, 0.25
        MC_luma_8xN_x1y1( src,     srcPitch, dst,     dstPitch, 8, src );
        MC_luma_8xN_x1y1( src + 8, srcPitch, dst + 8, dstPitch, 8, src + 8 );
        break;
    case 6:             // 0.5, 0.25
        MC_luma_16xN_hor_temp( src - srcPitch*2, srcPitch, tmpBuf, 12 );
        MC_luma_8xN_x2y1( tmpBuf,     dst,     dstPitch, 8 );
        MC_luma_8xN_x2y1( tmpBuf + 8, dst + 8, dstPitch, 8 );
        break;
    case 7:             // 0.75, 0.25
        MC_luma_8xN_x1y1( src,     srcPitch, dst,     dstPitch, 8, src + 1 );
        MC_luma_8xN_x1y1( src + 8, srcPitch, dst + 8, dstPitch, 8, src + 9 );
        break;  
    case 8:             // 0, 0.5
        MC_luma_8xN_x0y2( src,     srcPitch, dst,     dstPitch, 8 );
        MC_luma_8xN_x0y2( src + 8, srcPitch, dst + 8, dstPitch, 8 );
        break;
    case 9:             // 0.25, 0.5
        MC_luma_24xN_ver_temp( src - 2, srcPitch, tmpBuf, 8 );
        MC_luma_8xN_x1y2( tmpBuf,     dst,     dstPitch, 8 );
        MC_luma_8xN_x1y2( tmpBuf + 8, dst + 8, dstPitch, 8 );
        break;
    case 10:            // 0.5, 0.5
        MC_luma_8xN_x2y2( src,     srcPitch, dst,     dstPitch, 8 );
        MC_luma_8xN_x2y2( src + 8, srcPitch, dst + 8, dstPitch, 8 );
        break;
    case 11:            // 0.75, 0.5
        MC_luma_24xN_ver_temp( src - 1, srcPitch, tmpBuf, 8 );
        MC_luma_8xN_x3y2( tmpBuf,     dst,     dstPitch, 8 );
        MC_luma_8xN_x3y2( tmpBuf + 8, dst + 8, dstPitch, 8 );
        break;
    case 12:            // 0, 0.75
        MC_luma_8xN_x0y3( src,     srcPitch, dst,     dstPitch, 8 );
        MC_luma_8xN_x0y3( src + 8, srcPitch, dst + 8, dstPitch, 8 );
        break;
    case 13:            // 0.25, 0.75
        MC_luma_8xN_x1y1( src,     srcPitch, dst,     dstPitch, 8, src + srcPitch );
        MC_luma_8xN_x1y1( src + 8, srcPitch, dst + 8, dstPitch, 8, src + srcPitch + 8 );
        break;
    case 14:            // 0.5, 0.75
        MC_luma_16xN_hor_temp( src - srcPitch, srcPitch, tmpBuf, 12 );
        MC_luma_8xN_x2y3( tmpBuf,     dst,     dstPitch, 8 );
        MC_luma_8xN_x2y3( tmpBuf + 8, dst + 8, dstPitch, 8 );
        break;
    case 15:            // 0.75, 0.75
        MC_luma_8xN_x1y1( src,     srcPitch, dst,     dstPitch, 8, src + srcPitch + 1 );
        MC_luma_8xN_x1y1( src + 8, srcPitch, dst + 8, dstPitch, 8, src + srcPitch + 9 );
        break;
    default:
        assert(0);
        break;
    }
}

// 8x16 亮度分量帧间预测
void luma_inter_pred_8x16( FrmDecContext* ctx, const RefPicture* refpic, uint8_t* dst, int dstPitch, int x, int y )
{
    const int xInt = x >> 2;
    const int yInt = y >> 2;         
    const int xy = ((y & 3) << 2) | (x & 3);
    int srcPitch = ctx->picPitch[0];
    const uint8_t* src = nullptr;

    // 检查参考帧数据是否已解码, 未可用则等待
    check_ref_data( ctx, refpic->pframe, yInt + 19 );

    Rect rc1 = {xInt - 2, yInt - 2, xInt + 11, yInt + 19};
    if( in_rect_of( &rc1, &ctx->refRcLuma ) )
    {
        // 在参考帧实际图像范围内
        src = refpic->plane[0] + yInt * srcPitch + xInt;
    }
    else
    {
        // 对参考帧边界进行扩展, 得到参考数据
        McExtRect refRect;
        refRect.x = xInt - 2;
        refRect.y = yInt - 2;
        refRect.height = 16 + 5;
        refRect.picWidth = ctx->picWidth;
        refRect.picHeight = ctx->picHeight;
        refRect.buf = ctx->tempBuf;
        get_ref_data_16xN( refpic->plane[0], srcPitch, refRect );
        srcPitch = 16;
        src = refRect.buf + 16 * 2 + 2;    
    }

    int16_t* tmpBuf = (int16_t*)(ctx->tempBuf + 768);
    switch( xy )
    {
    case 0:             // 0, 0
        MC_copy_8xN( src, srcPitch, dst, dstPitch, 16 );
        break;
    case 1:             // 0.25, 0
        MC_luma_8xN_x1y0( src, srcPitch, dst, dstPitch, 16 );
        break;
    case 2:             // 0.5, 0
        MC_luma_8xN_x2y0( src, srcPitch, dst, dstPitch, 16 );
        break;
    case 3:             // 0.75, 0
        MC_luma_8xN_x3y0( src, srcPitch, dst, dstPitch, 16 );
        break;
    case 4:             // 0, 0.25
        MC_luma_8xN_x0y1( src, srcPitch, dst, dstPitch, 16 );
        break;
    case 5:             // 0.25, 0.25
        MC_luma_8xN_x1y1( src, srcPitch, dst, dstPitch, 16, src );
        break;
    case 6:             // 0.5, 0.25
        MC_luma_8xN_hor_temp( src - srcPitch*2, srcPitch, tmpBuf, 20 );
        MC_luma_8xN_x2y1( tmpBuf, dst, dstPitch, 16 );
        break;
    case 7:             // 0.75, 0.25
        MC_luma_8xN_x1y1( src, srcPitch, dst, dstPitch, 16, src + 1 );
        break;  
    case 8:             // 0, 0.5
        MC_luma_8xN_x0y2( src, srcPitch, dst, dstPitch, 16 );
        break;
    case 9:             // 0.25, 0.5
        MC_luma_16xN_ver_temp( src - 2, srcPitch, tmpBuf, 16 );
        MC_luma_8xN_x1y2( tmpBuf, dst, dstPitch, 16 );
        break;
    case 10:            // 0.5, 0.5
        MC_luma_8xN_x2y2( src, srcPitch, dst, dstPitch, 16 );
        break;
    case 11:            // 0.75, 0.5
        MC_luma_16xN_ver_temp( src - 1, srcPitch, tmpBuf, 16 );
        MC_luma_8xN_x3y2( tmpBuf, dst, dstPitch, 16 );
        break;
    case 12:            // 0, 0.75
        MC_luma_8xN_x0y3( src, srcPitch, dst, dstPitch, 16 );
        break;
    case 13:            // 0.25, 0.75
        MC_luma_8xN_x1y1( src, srcPitch, dst, dstPitch, 16, src + srcPitch );
        break;
    case 14:            // 0.5, 0.75
        MC_luma_8xN_hor_temp( src - srcPitch, srcPitch, tmpBuf, 20 );
        MC_luma_8xN_x2y3( tmpBuf, dst, dstPitch, 16 );
        break;
    case 15:            // 0.75, 0.75
        MC_luma_8xN_x1y1( src, srcPitch, dst, dstPitch, 16, src + srcPitch + 1 );
        break;
    default:
        assert(0);
        break;
    }
}

// 8x8 亮度分量帧间预测
void luma_inter_pred_8x8( FrmDecContext* ctx, const RefPicture* refpic, uint8_t* dst, int dstPitch, int x, int y )
{
    const int xInt = x >> 2;
    const int yInt = y >> 2;         
    const int xy = ((y & 3) << 2) | (x & 3);
    int srcPitch = ctx->picPitch[0];
    const uint8_t* src = nullptr;

    // 检查参考帧数据是否已解码, 未可用则等待
    check_ref_data( ctx, refpic->pframe, yInt + 11 );

    Rect rc1 = {xInt - 2, yInt - 2, xInt + 11, yInt + 11};
    if( in_rect_of( &rc1, &ctx->refRcLuma ) )
    {
        // 在参考帧实际图像范围内
        src = refpic->plane[0] + yInt * srcPitch + xInt;
    }
    else
    {
        // 对参考帧边界进行扩展, 得到参考数据
        McExtRect refRect;
        refRect.x = xInt - 2;
        refRect.y = yInt - 2;
        refRect.height = 8 + 5;
        refRect.picWidth = ctx->picWidth;
        refRect.picHeight = ctx->picHeight;
        refRect.buf = ctx->tempBuf;
        get_ref_data_16xN( refpic->plane[0], srcPitch, refRect );
        srcPitch = 16;
        src = refRect.buf + 16 * 2 + 2;    
    }

    int16_t* tmpBuf = (int16_t*)(ctx->tempBuf + 768);
    switch( xy )
    {
    case 0:             // 0, 0
        MC_copy_8xN( src, srcPitch, dst, dstPitch, 8 );
        break;
    case 1:             // 0.25, 0
        MC_luma_8xN_x1y0( src, srcPitch, dst, dstPitch, 8 );
        break;
    case 2:             // 0.5, 0
        MC_luma_8xN_x2y0( src, srcPitch, dst, dstPitch, 8 );
        break;
    case 3:             // 0.75, 0
        MC_luma_8xN_x3y0( src, srcPitch, dst, dstPitch, 8 );
        break;
    case 4:             // 0, 0.25
        MC_luma_8xN_x0y1( src, srcPitch, dst, dstPitch, 8 );
        break;
    case 5:             // 0.25, 0.25
        MC_luma_8xN_x1y1( src, srcPitch, dst, dstPitch, 8, src );
        break;
    case 6:             // 0.5, 0.25
        MC_luma_8xN_hor_temp( src - srcPitch*2, srcPitch, tmpBuf, 12 );
        MC_luma_8xN_x2y1( tmpBuf, dst, dstPitch, 8 );
        break;
    case 7:             // 0.75, 0.25
        MC_luma_8xN_x1y1( src, srcPitch, dst, dstPitch, 8, src + 1 );
        break;  
    case 8:             // 0, 0.5
        MC_luma_8xN_x0y2( src, srcPitch, dst, dstPitch, 8 );
        break;
    case 9:             // 0.25, 0.5
        MC_luma_16xN_ver_temp( src - 2, srcPitch, tmpBuf, 8 );
        MC_luma_8xN_x1y2( tmpBuf, dst, dstPitch, 8 );
        break;
    case 10:            // 0.5, 0.5
        MC_luma_8xN_x2y2( src, srcPitch, dst, dstPitch, 8 );
        break;
    case 11:            // 0.75, 0.5
        MC_luma_16xN_ver_temp( src - 1, srcPitch, tmpBuf, 8 );
        MC_luma_8xN_x3y2( tmpBuf, dst, dstPitch, 8 );
        break;
    case 12:            // 0, 0.75
        MC_luma_8xN_x0y3( src, srcPitch, dst, dstPitch, 8 );
        break;
    case 13:            // 0.25, 0.75
        MC_luma_8xN_x1y1( src, srcPitch, dst, dstPitch, 8, src + srcPitch );
        break;
    case 14:            // 0.5, 0.75
        MC_luma_8xN_hor_temp( src - srcPitch, srcPitch, tmpBuf, 12 );
        MC_luma_8xN_x2y3( tmpBuf, dst, dstPitch, 8 );
        break;
    case 15:            // 0.75, 0.75
        MC_luma_8xN_x1y1( src, srcPitch, dst, dstPitch, 8, src + srcPitch + 1 );
        break;
    default:
        assert(0);
        break;
    }
}

//======================================================================================================================
// 色差分量帧间预测

// 横向滤波, 输入地址为 src, 输出为 vm0, tm1 为临时变量, dmx 为滤波系数
#define CHROMA_MC_HOR8( src, vm0 ) \
    vm0 = _mm_loadu_si128( (__m128i*)(src) );   \
    tm1 = _mm_srli_si128( vm0, 1 );             \
    vm0 = _mm_cvtepu8_epi16( vm0 );             \
    tm1 = _mm_cvtepu8_epi16( tm1 );             \
    tm1 = _mm_sub_epi16( tm1, vm0 );            \
    vm0 = _mm_slli_epi16( vm0, 3 );             \
    tm1 = _mm_mullo_epi16( tm1, dmx );          \
    vm0 = _mm_add_epi16( vm0, tm1 );

// 横向滤波, 输入 vm0, vm1, add, shift, 处理后 vm1, add, shift 不变
// 输出 vm0, tm1 为临时变量, dmy 为滤波系数
#define CHROMA_MC_VER( vm0, vm1, add, shift ) \
    tm1 = _mm_sub_epi16( vm1, vm0 );    \
    vm0 = _mm_slli_epi16( vm0, 3 );     \
    tm1 = _mm_mullo_epi16( tm1, dmy );  \
    vm0 = _mm_add_epi16( vm0, tm1 );    \
    vm0 = _mm_add_epi16( vm0, add );    \
    vm0 = _mm_srai_epi16( vm0, shift ); \
    vm0 = _mm_packus_epi16( vm0, vm0 );

// 8x16, 8x8, 8x4
static void MC_chroma_8xN_bilinear( const uint8_t* src, int srcPitch, uint8_t* dst, int dstPitch, int dx, int dy, int N )
{
    assert( (N & 3) == 0 );
    __m128i c32 = _mm_set1_epi16( 32 );
    __m128i xm0, xm1, dmx, dmy, tm1;

    dmx = _mm_cvtsi32_si128( dx );
    dmy = _mm_cvtsi32_si128( dy );
    dmx = _mm_unpacklo_epi16( dmx, dmx );
    dmy = _mm_unpacklo_epi16( dmy, dmy );
    dmx = _mm_shuffle_epi32( dmx, 0 );
    dmy = _mm_shuffle_epi32( dmy, 0 );

    CHROMA_MC_HOR8( src, xm0 );
    while( 1 )
    {
        CHROMA_MC_HOR8( src + srcPitch, xm1 );
        CHROMA_MC_VER( xm0, xm1, c32, 6 );
        _mm_storel_epi64( (__m128i*)dst, xm0 );

        src += srcPitch * 2;
        CHROMA_MC_HOR8( src, xm0 );
        CHROMA_MC_VER( xm1, xm0, c32, 6 );
        _mm_storel_epi64( (__m128i*)(dst + dstPitch), xm1 );

        dst += dstPitch * 2;
        CHROMA_MC_HOR8( src + srcPitch, xm1 );
        CHROMA_MC_VER( xm0, xm1, c32, 6 );
        _mm_storel_epi64( (__m128i*)dst, xm0 );

        src += srcPitch * 2;
        CHROMA_MC_HOR8( src, xm0 );
        CHROMA_MC_VER( xm1, xm0, c32, 6 );
        _mm_storel_epi64( (__m128i*)(dst + dstPitch), xm1 );

        if( (N -= 4) == 0 )
            break;
        dst += dstPitch * 2;
    }
}

// 针对垂直方向无需滤波的情况( dy == 0 )
static void MC_chroma_8xN_hor( const uint8_t* src, int srcPitch, uint8_t* dst, int dstPitch, int dx, int dy, int N )
{
    assert( (N & 3) == 0 && dy == 0 );
    __m128i c4 = _mm_set1_epi16( 4 );
    __m128i xm0, dmx, tm1;
    dmx = _mm_cvtsi32_si128( dx );
    dmx = _mm_unpacklo_epi16( dmx, dmx );
    dmx = _mm_shuffle_epi32( dmx, 0 );

    while( 1 )
    {
        CHROMA_MC_HOR8( src, xm0 );
        xm0 = _mm_add_epi16( xm0, c4 );
        xm0 = _mm_srai_epi16( xm0, 3 );
        xm0 = _mm_packus_epi16( xm0, xm0 );
        _mm_storel_epi64( (__m128i*)dst, xm0 );

        CHROMA_MC_HOR8( src + srcPitch, xm0 );
        xm0 = _mm_add_epi16( xm0, c4 );
        xm0 = _mm_srai_epi16( xm0, 3 );
        xm0 = _mm_packus_epi16( xm0, xm0 );
        _mm_storel_epi64( (__m128i*)(dst + dstPitch), xm0 );

        src += srcPitch * 2;
        dst += dstPitch * 2;
        CHROMA_MC_HOR8( src, xm0 );
        xm0 = _mm_add_epi16( xm0, c4 );
        xm0 = _mm_srai_epi16( xm0, 3 );
        xm0 = _mm_packus_epi16( xm0, xm0 );
        _mm_storel_epi64( (__m128i*)dst, xm0 );

        CHROMA_MC_HOR8( src + srcPitch, xm0 );
        xm0 = _mm_add_epi16( xm0, c4 );
        xm0 = _mm_srai_epi16( xm0, 3 );
        xm0 = _mm_packus_epi16( xm0, xm0 );
        _mm_storel_epi64( (__m128i*)(dst + dstPitch), xm0 );

        if( (N -= 4) == 0 )
            break;
        src += srcPitch * 2;
        dst += dstPitch * 2;
    }
}

#undef CHROMA_MC_HOR8

// 针对水平方向无需滤波的情况( dx == 0 )
static void MC_chroma_8xN_ver( const uint8_t* src, int srcPitch, uint8_t* dst, int dstPitch, int dx, int dy, int N )
{
    assert( (N & 3) == 0 && dx == 0 );
    __m128i c4 = _mm_set1_epi16( 4 );
    __m128i xm0, xm1, dmy, tm1;
    dmy = _mm_cvtsi32_si128( dy );
    dmy = _mm_unpacklo_epi16( dmy, dmy );
    dmy = _mm_shuffle_epi32( dmy, 0 );

    xm0 = loadzx_epu8_epi16( src );
    while( 1 )
    {
        xm1 = loadzx_epu8_epi16( src + srcPitch );
        CHROMA_MC_VER( xm0, xm1, c4, 3 );
        _mm_storel_epi64( (__m128i*)dst, xm0 );

        src += srcPitch * 2;
        xm0 = loadzx_epu8_epi16( src );
        CHROMA_MC_VER( xm1, xm0, c4, 3 );
        _mm_storel_epi64( (__m128i*)(dst + dstPitch), xm1 );

        dst += dstPitch * 2;
        xm1 = loadzx_epu8_epi16( src + srcPitch );
        CHROMA_MC_VER( xm0, xm1, c4, 3 );
        _mm_storel_epi64( (__m128i*)dst, xm0 );

        src += srcPitch * 2;
        xm0 = loadzx_epu8_epi16( src );
        CHROMA_MC_VER( xm1, xm0, c4, 3 );
        _mm_storel_epi64( (__m128i*)(dst + dstPitch), xm1 );

        if( (N -= 4) == 0 )
            break;
        dst += dstPitch * 2;
    }
}

#define CHROMA_MC_HOR4( src, vm0 )              \
    vm0 = _mm_loadl_epi64( (__m128i*)(src) );   \
    vm0 = _mm_cvtepu8_epi16( vm0 );             \
    tm1 = _mm_mullo_epi16( vm0, dmx );          \
    vm0 = _mm_slli_epi16( vm0, 3 );             \
    vm0 = _mm_sub_epi16( vm0, tm1 );            \
    tm1 = _mm_srli_si128( tm1, 2 );             \
    vm0 = _mm_add_epi16( vm0, tm1 );

// 4x8, 4x4
static void MC_chroma_4xN_bilinear( const uint8_t* src, int srcPitch, uint8_t* dst, int dstPitch, int dx, int dy, int N )
{
    assert( N == 4 || N == 8 );
    __m128i c32 = _mm_set1_epi16( 32 );
    __m128i xm0, xm1, dmx, dmy, tm1;
    dmx = _mm_cvtsi32_si128( dx );
    dmy = _mm_cvtsi32_si128( dy );
    dmx = _mm_unpacklo_epi16( dmx, dmx );
    dmy = _mm_unpacklo_epi16( dmy, dmy );
    dmx = _mm_shuffle_epi32( dmx, 0 );
    dmy = _mm_shuffle_epi32( dmy, 0 );

    CHROMA_MC_HOR4( src, xm0 );
    CHROMA_MC_HOR4( src + srcPitch, xm1 );
    CHROMA_MC_VER( xm0, xm1, c32, 6 );
    *(int*)dst = _mm_cvtsi128_si32( xm0 );

    src += srcPitch * 2;
    CHROMA_MC_HOR4( src, xm0 );
    CHROMA_MC_VER( xm1, xm0, c32, 6 );
    *(int*)(dst + dstPitch) = _mm_cvtsi128_si32( xm1 );

    dst += dstPitch * 2;
    CHROMA_MC_HOR4( src + srcPitch, xm1 );
    CHROMA_MC_VER( xm0, xm1, c32, 6 );
    *(int*)dst = _mm_cvtsi128_si32( xm0 );

    src += srcPitch * 2;
    CHROMA_MC_HOR4( src, xm0 );
    CHROMA_MC_VER( xm1, xm0, c32, 6 );
    *(int*)(dst + dstPitch) = _mm_cvtsi128_si32( xm1 );

    if( N == 4 )    // 4 x 4
        return;

    dst += dstPitch * 2;
    CHROMA_MC_HOR4( src + srcPitch, xm1 );
    CHROMA_MC_VER( xm0, xm1, c32, 6 );
    *(int*)dst = _mm_cvtsi128_si32( xm0 );

    src += srcPitch * 2;
    CHROMA_MC_HOR4( src, xm0 );
    CHROMA_MC_VER( xm1, xm0, c32, 6 );
    *(int*)(dst + dstPitch) = _mm_cvtsi128_si32( xm1 );

    dst += dstPitch * 2;
    CHROMA_MC_HOR4( src + srcPitch, xm1 );
    CHROMA_MC_VER( xm0, xm1, c32, 6 );
    *(int*)dst = _mm_cvtsi128_si32( xm0 );

    src += srcPitch * 2;
    CHROMA_MC_HOR4( src, xm0 );
    CHROMA_MC_VER( xm1, xm0, c32, 6 );
    *(int*)(dst + dstPitch) = _mm_cvtsi128_si32( xm1 );
}

// 针对垂直方向无需滤波的情况( dy == 0 )
static void MC_chroma_4xN_hor( const uint8_t* src, int srcPitch, uint8_t* dst, int dstPitch, int dx, int dy, int N )
{
    assert( N == 4 || N == 8 );
    assert( dy == 0 );
    __m128i c4 = _mm_set1_epi16( 4 );
    __m128i xm0, dmx, tm1;
    dmx = _mm_cvtsi32_si128( dx );
    dmx = _mm_unpacklo_epi16( dmx, dmx );
    dmx = _mm_shuffle_epi32( dmx, 0 );

    CHROMA_MC_HOR4( src, xm0 );
    xm0 = _mm_add_epi16( xm0, c4 );
    xm0 = _mm_srai_epi16( xm0, 3 );
    xm0 = _mm_packus_epi16( xm0, xm0 );
    *(int*)dst = _mm_cvtsi128_si32( xm0 );

    CHROMA_MC_HOR4( src + srcPitch, xm0 );
    xm0 = _mm_add_epi16( xm0, c4 );
    xm0 = _mm_srai_epi16( xm0, 3 );
    xm0 = _mm_packus_epi16( xm0, xm0 );
    *(int*)(dst + dstPitch) = _mm_cvtsi128_si32( xm0 );

    src += srcPitch * 2;
    dst += dstPitch * 2;
    CHROMA_MC_HOR4( src, xm0 );
    xm0 = _mm_add_epi16( xm0, c4 );
    xm0 = _mm_srai_epi16( xm0, 3 );
    xm0 = _mm_packus_epi16( xm0, xm0 );
    *(int*)dst = _mm_cvtsi128_si32( xm0 );

    CHROMA_MC_HOR4( src + srcPitch, xm0 );
    xm0 = _mm_add_epi16( xm0, c4 );
    xm0 = _mm_srai_epi16( xm0, 3 );
    xm0 = _mm_packus_epi16( xm0, xm0 );
    *(int*)(dst + dstPitch) = _mm_cvtsi128_si32( xm0 );

    if( N == 4 )    // 4 x4
        return;

    src += srcPitch * 2;
    dst += dstPitch * 2;
    CHROMA_MC_HOR4( src, xm0 );
    xm0 = _mm_add_epi16( xm0, c4 );
    xm0 = _mm_srai_epi16( xm0, 3 );
    xm0 = _mm_packus_epi16( xm0, xm0 );
    *(int*)dst = _mm_cvtsi128_si32( xm0 );

    CHROMA_MC_HOR4( src + srcPitch, xm0 );
    xm0 = _mm_add_epi16( xm0, c4 );
    xm0 = _mm_srai_epi16( xm0, 3 );
    xm0 = _mm_packus_epi16( xm0, xm0 );
    *(int*)(dst + dstPitch) = _mm_cvtsi128_si32( xm0 );

    src += srcPitch * 2;
    dst += dstPitch * 2;
    CHROMA_MC_HOR4( src, xm0 );
    xm0 = _mm_add_epi16( xm0, c4 );
    xm0 = _mm_srai_epi16( xm0, 3 );
    xm0 = _mm_packus_epi16( xm0, xm0 );
    *(int*)dst = _mm_cvtsi128_si32( xm0 );

    CHROMA_MC_HOR4( src + srcPitch, xm0 );
    xm0 = _mm_add_epi16( xm0, c4 );
    xm0 = _mm_srai_epi16( xm0, 3 );
    xm0 = _mm_packus_epi16( xm0, xm0 );
    *(int*)(dst + dstPitch) = _mm_cvtsi128_si32( xm0 );
}

// 针对水平方向无需滤波的情况( dx == 0 )
static void MC_chroma_4xN_ver( const uint8_t* src, int srcPitch, uint8_t* dst, int dstPitch, int dx, int dy, int N )
{
    assert( N == 4 || N == 8 );
    assert( dx == 0 );
    __m128i c4 = _mm_set1_epi16( 4 );
    __m128i xm0, xm1, dmy, tm1;
    dmy = _mm_cvtsi32_si128( dy );
    dmy = _mm_unpacklo_epi16( dmy, dmy );
    dmy = _mm_shuffle_epi32( dmy, 0 );

    xm0 = loadzx_epu8_epi16( src );
    xm1 = loadzx_epu8_epi16( src + srcPitch );
    CHROMA_MC_VER( xm0, xm1, c4, 3 );
    *(int*)dst = _mm_cvtsi128_si32( xm0 );

    src += srcPitch * 2;
    xm0 = loadzx_epu8_epi16( src );
    CHROMA_MC_VER( xm1, xm0, c4, 3 );
    *(int*)(dst + dstPitch) = _mm_cvtsi128_si32( xm1 );

    dst += dstPitch * 2;
    xm1 = loadzx_epu8_epi16( src + srcPitch );
    CHROMA_MC_VER( xm0, xm1, c4, 3 );
    *(int*)dst = _mm_cvtsi128_si32( xm0 );

    src += srcPitch * 2;
    xm0 = loadzx_epu8_epi16( src );
    CHROMA_MC_VER( xm1, xm0, c4, 3 );
    *(int*)(dst + dstPitch) = _mm_cvtsi128_si32( xm1 );

    if( N == 4 )
        return;

    dst += dstPitch * 2;
    xm1 = loadzx_epu8_epi16( src + srcPitch );
    CHROMA_MC_VER( xm0, xm1, c4, 3 );
    *(int*)dst = _mm_cvtsi128_si32( xm0 );

    src += srcPitch * 2;
    xm0 = loadzx_epu8_epi16( src );
    CHROMA_MC_VER( xm1, xm0, c4, 3 );
    *(int*)(dst + dstPitch) = _mm_cvtsi128_si32( xm1 );

    dst += dstPitch * 2;
    xm1 = loadzx_epu8_epi16( src + srcPitch );
    CHROMA_MC_VER( xm0, xm1, c4, 3 );
    *(int*)dst = _mm_cvtsi128_si32( xm0 );

    src += srcPitch * 2;
    xm0 = loadzx_epu8_epi16( src );
    CHROMA_MC_VER( xm1, xm0, c4, 3 );
    *(int*)(dst + dstPitch) = _mm_cvtsi128_si32( xm1 );
}

//=======================================================================================================================

// 8x8 色差分量帧间预测
void chroma_inter_pred_8x8( FrmDecContext* ctx, const RefPicture* refpic, 
                            uint8_t* dstCb, uint8_t* dstCr, int dstPitch, int x, int y )
{
    const int xInt = x >> 3;
    const int yInt = y >> 3;
    const int dx = x & 7;
    const int dy = y & 7;
    int srcPitch = ctx->picPitch[1];
    const uint8_t* srcCb = nullptr;
    const uint8_t* srcCr = nullptr;

    Rect rc1 = {xInt, yInt, xInt + 9, yInt + 9};
    if( in_rect_of( &rc1, &ctx->refRcCbcr ) )
    {
        // 在参考帧实际图像范围内
        srcCb = refpic->plane[1] + yInt * srcPitch + xInt;
        srcCr = refpic->plane[2] + yInt * srcPitch + xInt;
    }
    else
    {     
        // 对参考帧边界进行扩展, 得到参考数据
        McExtRect refRect;
        refRect.x = xInt;
        refRect.y = yInt;
        refRect.height = 9;
        refRect.picWidth = ctx->chromaWidth;
        refRect.picHeight = ctx->chromaHeight;
        refRect.buf = ctx->tempBuf;
        get_ref_data_16xN( refpic->plane[1], srcPitch, refRect );
        refRect.buf = ctx->tempBuf + 16 * 16;
        get_ref_data_16xN( refpic->plane[2], srcPitch, refRect );
        srcPitch = 16;
        srcCb = ctx->tempBuf;
        srcCr = ctx->tempBuf + 16 * 16;
    }

    const int idx = (dy==0) * 2 + (dx==0);
    switch( idx )
    {
    case 0:
        MC_chroma_8xN_bilinear( srcCb, srcPitch, dstCb, dstPitch, dx, dy, 8 );
        MC_chroma_8xN_bilinear( srcCr, srcPitch, dstCr, dstPitch, dx, dy, 8 );
        break;
    case 1:         // dx == 0
        MC_chroma_8xN_ver( srcCb, srcPitch, dstCb, dstPitch, dx, dy, 8 );
        MC_chroma_8xN_ver( srcCr, srcPitch, dstCr, dstPitch, dx, dy, 8 );
        break;
    case 2:         // dy == 0
        MC_chroma_8xN_hor( srcCb, srcPitch, dstCb, dstPitch, dx, dy, 8 );
        MC_chroma_8xN_hor( srcCr, srcPitch, dstCr, dstPitch, dx, dy, 8 );
        break;
    case 3:
        MC_copy_8xN( srcCb, srcPitch, dstCb, dstPitch, 8 );
        MC_copy_8xN( srcCr, srcPitch, dstCr, dstPitch, 8 );
        break;
    default:
        assert(0);
        break;
    }
}

// 8x4 色差分量帧间预测
void chroma_inter_pred_8x4( FrmDecContext* ctx, const RefPicture* refpic, 
                            uint8_t* dstCb, uint8_t* dstCr, int dstPitch, int x, int y )
{
    const int xInt = x >> 3;
    const int yInt = y >> 3;
    const int dx = x & 7;
    const int dy = y & 7;
    int srcPitch = ctx->picPitch[1];
    const uint8_t* srcCb = nullptr;
    const uint8_t* srcCr = nullptr;

    Rect rc1 = {xInt, yInt, xInt + 9, yInt + 5};
    if( in_rect_of( &rc1, &ctx->refRcCbcr ) )
    {
        // 在参考帧实际图像范围内
        srcCb = refpic->plane[1] + yInt * srcPitch + xInt;
        srcCr = refpic->plane[2] + yInt * srcPitch + xInt;
    }
    else
    {
        // 对参考帧边界进行扩展, 得到参考数据
        McExtRect refRect;
        refRect.x = xInt;
        refRect.y = yInt;
        refRect.height = 5;
        refRect.picWidth = ctx->chromaWidth;
        refRect.picHeight = ctx->chromaHeight;
        refRect.buf = ctx->tempBuf;
        get_ref_data_16xN( refpic->plane[1], srcPitch, refRect );
        refRect.buf = ctx->tempBuf + 8 * 16;
        get_ref_data_16xN( refpic->plane[2], srcPitch, refRect );
        srcPitch = 16;
        srcCb = ctx->tempBuf;
        srcCr = ctx->tempBuf + 8 * 16;
    }

    const int idx = (dy==0) * 2 + (dx==0);
    switch( idx )
    {
    case 0:
        MC_chroma_8xN_bilinear( srcCb, srcPitch, dstCb, dstPitch, dx, dy, 4 );
        MC_chroma_8xN_bilinear( srcCr, srcPitch, dstCr, dstPitch, dx, dy, 4 );
        break;
    case 1:         // dx == 0
        MC_chroma_8xN_ver( srcCb, srcPitch, dstCb, dstPitch, dx, dy, 4 );
        MC_chroma_8xN_ver( srcCr, srcPitch, dstCr, dstPitch, dx, dy, 4 );
        break;
    case 2:         // dy == 0
        MC_chroma_8xN_hor( srcCb, srcPitch, dstCb, dstPitch, dx, dy, 4 );
        MC_chroma_8xN_hor( srcCr, srcPitch, dstCr, dstPitch, dx, dy, 4 );
        break;
    case 3:
        MC_copy_8xN( srcCb, srcPitch, dstCb, dstPitch, 4 );
        MC_copy_8xN( srcCr, srcPitch, dstCr, dstPitch, 4 );
        break;
    default:
        assert(0);
        break;
    }
}

// 4x8 色差分量帧间预测
void chroma_inter_pred_4x8( FrmDecContext* ctx, const RefPicture* refpic, 
                            uint8_t* dstCb, uint8_t* dstCr, int dstPitch, int x, int y )
{
    const int xInt = x >> 3;
    const int yInt = y >> 3;
    const int dx = x & 7;
    const int dy = y & 7;
    int srcPitch = ctx->picPitch[1];
    const uint8_t* srcCb = nullptr;
    const uint8_t* srcCr = nullptr;

    Rect rc1 = {xInt, yInt, xInt + 5, yInt + 9};
    if( in_rect_of( &rc1, &ctx->refRcCbcr ) )
    {
        // 在参考帧实际图像范围内
        srcCb = refpic->plane[1] + yInt * srcPitch + xInt;
        srcCr = refpic->plane[2] + yInt * srcPitch + xInt;
    }
    else
    {
        // 对参考帧边界进行扩展, 得到参考数据
        McExtRect refRect;
        refRect.x = xInt;
        refRect.y = yInt;
        refRect.height = 9;
        refRect.picWidth = ctx->chromaWidth;
        refRect.picHeight = ctx->chromaHeight;
        refRect.buf = ctx->tempBuf;
        get_ref_data_8xN( refpic->plane[1], srcPitch, refRect );
        refRect.buf = ctx->tempBuf + 8 * 16;
        get_ref_data_8xN( refpic->plane[2], srcPitch, refRect );
        srcPitch = 8;
        srcCb = ctx->tempBuf;
        srcCr = ctx->tempBuf + 8 * 16;
    }

    const int idx = (dy==0) * 2 + (dx==0);
    switch( idx )
    {
    case 0:
        MC_chroma_4xN_bilinear( srcCb, srcPitch, dstCb, dstPitch, dx, dy, 8 );
        MC_chroma_4xN_bilinear( srcCr, srcPitch, dstCr, dstPitch, dx, dy, 8 );
        break;
    case 1:         // dx == 0
        MC_chroma_4xN_ver( srcCb, srcPitch, dstCb, dstPitch, dx, dy, 8 );
        MC_chroma_4xN_ver( srcCr, srcPitch, dstCr, dstPitch, dx, dy, 8 );
        break;
    case 2:         // dy == 0
        MC_chroma_4xN_hor( srcCb, srcPitch, dstCb, dstPitch, dx, dy, 8 );
        MC_chroma_4xN_hor( srcCr, srcPitch, dstCr, dstPitch, dx, dy, 8 );
        break;
    case 3:
        MC_copy_4x8( srcCb, srcPitch, dstCb, dstPitch );
        MC_copy_4x8( srcCr, srcPitch, dstCr, dstPitch );
        break;
    default:
        assert(0);
        break;
    }
}

// 4x4 色差分量帧间预测
void chroma_inter_pred_4x4( FrmDecContext* ctx, const RefPicture* refpic, 
                            uint8_t* dstCb, uint8_t* dstCr, int dstPitch, int x, int y )
{
    const int xInt = x >> 3;
    const int yInt = y >> 3;
    const int dx = x & 7;
    const int dy = y & 7;
    int srcPitch = ctx->picPitch[1];
    const uint8_t* srcCb = nullptr;
    const uint8_t* srcCr = nullptr;

    Rect rc1 = {xInt, yInt, xInt + 5, yInt + 5};
    if( in_rect_of( &rc1, &ctx->refRcCbcr ) )
    {
        // 在参考帧实际图像范围内
        srcCb = refpic->plane[1] + yInt * srcPitch + xInt;
        srcCr = refpic->plane[2] + yInt * srcPitch + xInt;
    }
    else
    {
        // 对参考帧边界进行扩展, 得到参考数据
        McExtRect refRect;        
        refRect.x = xInt;
        refRect.y = yInt;
        refRect.height = 5;
        refRect.picWidth = ctx->chromaWidth;
        refRect.picHeight = ctx->chromaHeight;
        refRect.buf = ctx->tempBuf;
        get_ref_data_8xN( refpic->plane[1], srcPitch, refRect );
        refRect.buf = ctx->tempBuf + 8 * 8;
        get_ref_data_8xN( refpic->plane[2], srcPitch, refRect );
        srcPitch = 8;
        srcCb = ctx->tempBuf;
        srcCr = ctx->tempBuf + 8 * 8;
    }

    const int idx = (dy==0) * 2 + (dx==0);
    switch( idx )
    {
    case 0:
        MC_chroma_4xN_bilinear( srcCb, srcPitch, dstCb, dstPitch, dx, dy, 4 );
        MC_chroma_4xN_bilinear( srcCr, srcPitch, dstCr, dstPitch, dx, dy, 4 );
        break;
    case 1:         // dx == 0
        MC_chroma_4xN_ver( srcCb, srcPitch, dstCb, dstPitch, dx, dy, 4 );
        MC_chroma_4xN_ver( srcCr, srcPitch, dstCr, dstPitch, dx, dy, 4 );
        break;
    case 2:         // dy == 0
        MC_chroma_4xN_hor( srcCb, srcPitch, dstCb, dstPitch, dx, dy, 4 );
        MC_chroma_4xN_hor( srcCr, srcPitch, dstCr, dstPitch, dx, dy, 4 );
        break;
    case 3:
        MC_copy_4x4( srcCb, srcPitch, dstCb, dstPitch );
        MC_copy_4x4( srcCr, srcPitch, dstCr, dstPitch );
        break;
    default:
        assert(0);
        break;
    }
}

//======================================================================================================================
// 加权预测

void weight_pred_16xN( uint8_t* dst, int pitch, int scale, int delta, int N )
{
    assert( ((uintptr_t)dst & 15) == 0 );
    assert( (pitch & 15) == 0 );
    __m128i smm = _mm_set1_epi16( scale );
    __m128i dmm = _mm_set1_epi16( delta );
    __m128i amm = _mm_set1_epi16( 16 );
    __m128i zero = _mm_setzero_si128();
    __m128i xm0, xm1;
    for( int i = 0; i < N; i++ )
    {
        xm0 = _mm_load_si128( (__m128i*)dst );
        xm1 = _mm_unpackhi_epi8( xm0, zero );
        xm0 = _mm_unpacklo_epi8( xm0, zero );
        xm1 = _mm_mullo_epi16( xm1, smm );
        xm0 = _mm_mullo_epi16( xm0, smm );
        xm1 = _mm_adds_epi16( xm1, amm );
        xm0 = _mm_adds_epi16( xm0, amm );
        xm1 = _mm_srai_epi16( xm1, 5 );
        xm0 = _mm_srai_epi16( xm0, 5 );
        xm1 = _mm_adds_epi16( xm1, dmm );
        xm0 = _mm_adds_epi16( xm0, dmm );
        xm0 = _mm_packus_epi16( xm0, xm1 );
        _mm_store_si128( (__m128i*)dst, xm0 );
        dst += pitch;
    }
}

void weight_pred_8xN( uint8_t* dst, int pitch, int scale, int delta, int N )
{
    assert( (N & 3) == 0 );
    __m128i smm = _mm_set1_epi16( scale );
    __m128i dmm = _mm_set1_epi16( delta );
    __m128i amm = _mm_set1_epi16( 16 );
    __m128i zero = _mm_setzero_si128();
    __m128i xm0, xm1;
    for( int i = 0; i < N; i += 2 )
    {
        xm0 = _mm_loadl_epi64( (__m128i*)dst );
        xm1 = _mm_loadl_epi64( (__m128i*)(dst + pitch) );
        xm0 = _mm_unpacklo_epi8( xm0, zero );
        xm1 = _mm_unpacklo_epi8( xm1, zero );     
        xm0 = _mm_mullo_epi16( xm0, smm );
        xm1 = _mm_mullo_epi16( xm1, smm ); 
        xm0 = _mm_adds_epi16( xm0, amm );
        xm1 = _mm_adds_epi16( xm1, amm );
        xm0 = _mm_srai_epi16( xm0, 5 );
        xm1 = _mm_srai_epi16( xm1, 5 );
        xm0 = _mm_adds_epi16( xm0, dmm );
        xm1 = _mm_adds_epi16( xm1, dmm );
        xm0 = _mm_packus_epi16( xm0, xm0 );
        xm1 = _mm_packus_epi16( xm1, xm1 );
        _mm_storel_epi64( (__m128i*)dst, xm0 );
        _mm_storel_epi64( (__m128i*)(dst + pitch), xm1 );
        dst += pitch * 2;
    }
}

void weight_pred_4xN( uint8_t* dst, int pitch, int scale, int delta, int N )
{
    assert( (N & 3) == 0 );
    __m128i smm = _mm_set1_epi16( scale );
    __m128i dmm = _mm_set1_epi16( delta );
    __m128i amm = _mm_set1_epi16( 16 );
    __m128i zero = _mm_setzero_si128();
    __m128i xm0, xm1, xm2, xm3;
    for( int i = 0; i < N; i += 4 )
    {
        xm0 = _mm_cvtsi32_si128( *(int32_as*)dst );
        xm1 = _mm_cvtsi32_si128( *(int32_as*)(dst + pitch) );
        xm2 = _mm_cvtsi32_si128( *(int32_as*)(dst + pitch*2) );
        xm3 = _mm_cvtsi32_si128( *(int32_as*)(dst + pitch*3) );
        xm0 = _mm_unpacklo_epi32( xm0, xm1 );
        xm2 = _mm_unpacklo_epi32( xm2, xm3 );
        xm0 = _mm_unpacklo_epi8( xm0, zero );
        xm2 = _mm_unpacklo_epi8( xm2, zero );     
        xm0 = _mm_mullo_epi16( xm0, smm );
        xm2 = _mm_mullo_epi16( xm2, smm ); 
        xm0 = _mm_adds_epi16( xm0, amm );
        xm2 = _mm_adds_epi16( xm2, amm );
        xm0 = _mm_srai_epi16( xm0, 5 );
        xm2 = _mm_srai_epi16( xm2, 5 );
        xm0 = _mm_adds_epi16( xm0, dmm );
        xm2 = _mm_adds_epi16( xm2, dmm );
        xm0 = _mm_packus_epi16( xm0, xm2 );
        *(int32_as*)dst = _mm_cvtsi128_si32( xm0 );
        xm0 = _mm_srli_si128( xm0, 4 );
        *(int32_as*)(dst + pitch) = _mm_cvtsi128_si32( xm0 );
        xm0 = _mm_srli_si128( xm0, 4 );
        *(int32_as*)(dst + pitch*2) = _mm_cvtsi128_si32( xm0 );
        xm0 = _mm_srli_si128( xm0, 4 );
        *(int32_as*)(dst + pitch*3) = _mm_cvtsi128_si32( xm0 );
        dst += pitch * 4;
    }
}

//======================================================================================================================

// 取平均, 16x16, 16x8
void MC_avg_16xN( const uint8_t* src, int srcPitch, uint8_t* dst, int dstPitch, int N )
{
    assert( ((uintptr_t)src & 15)==0 && ((uintptr_t)dst & 15)==0 );
    assert( (srcPitch & 15)==0 && (dstPitch & 15)==0 );
    assert( (N & 7) == 0 );
    __m128i xm0, xm1, xm2, xm3;

#define XMM_AVG_16X2    \
    xm0 = _mm_load_si128( (__m128i*)src ); \
    xm2 = _mm_load_si128( (__m128i*)(src + srcPitch) ); \
    xm1 = _mm_load_si128( (__m128i*)dst ); \
    xm3 = _mm_load_si128( (__m128i*)(dst + dstPitch) ); \
    xm0 = _mm_avg_epu8( xm0, xm1 ); \
    xm2 = _mm_avg_epu8( xm2, xm3 ); \
    _mm_store_si128( (__m128i*)dst, xm0 ); \
    _mm_store_si128( (__m128i*)(dst + dstPitch), xm2 );

    while( 1 )
    {
        XMM_AVG_16X2;
        src += srcPitch * 2;
        dst += dstPitch * 2;
        XMM_AVG_16X2;
        src += srcPitch * 2;
        dst += dstPitch * 2;
        XMM_AVG_16X2;
        src += srcPitch * 2;
        dst += dstPitch * 2;
        XMM_AVG_16X2;

        if( (N -= 8) == 0 )
            break;
        src += srcPitch * 2;
        dst += dstPitch * 2;
    }

#undef XMM_AVG_16X2
}

// 取平均, 8x16, 8x8, 8x4
void MC_avg_8xN( const uint8_t* src, int srcPitch, uint8_t* dst, int dstPitch, int N )
{
    assert( (N & 3) == 0 );
    __m128i xm0, xm1, xm2, xm3;
    while( 1 )
    {
        xm0 = _mm_loadl_epi64( (__m128i*)src );
        xm2 = _mm_loadl_epi64( (__m128i*)(src + srcPitch) );
        xm1 = _mm_loadl_epi64( (__m128i*)dst );
        xm3 = _mm_loadl_epi64( (__m128i*)(dst + dstPitch) );
        xm0 = _mm_avg_epu8( xm0, xm1 );
        xm2 = _mm_avg_epu8( xm2, xm3 );
        _mm_storel_epi64( (__m128i*)dst, xm0 );
        _mm_storel_epi64( (__m128i*)(dst + dstPitch), xm2 );
        src += srcPitch * 2;
        dst += dstPitch * 2;
        xm0 = _mm_loadl_epi64( (__m128i*)src );
        xm2 = _mm_loadl_epi64( (__m128i*)(src + srcPitch) );
        xm1 = _mm_loadl_epi64( (__m128i*)dst );
        xm3 = _mm_loadl_epi64( (__m128i*)(dst + dstPitch) );
        xm0 = _mm_avg_epu8( xm0, xm1 );
        xm2 = _mm_avg_epu8( xm2, xm3 );
        _mm_storel_epi64( (__m128i*)dst, xm0 );
        _mm_storel_epi64( (__m128i*)(dst + dstPitch), xm2 );

        if( (N -= 4) == 0 )
            break;
        src += srcPitch * 2;
        dst += dstPitch * 2;
    }
}

// 取平均, 4x8, 4x4
void MC_avg_4xN( const uint8_t* src, int srcPitch, uint8_t* dst, int dstPitch, int N )
{
    assert( (N & 3) == 0 );
    __m128i xm0, xm1, xm2, xm3;
    while( 1 )
    {
        xm0 = _mm_cvtsi32_si128( *(int32_as*)src );
        xm2 = _mm_cvtsi32_si128( *(int32_as*)(src + srcPitch) );
        xm1 = _mm_cvtsi32_si128( *(int32_as*)dst );
        xm3 = _mm_cvtsi32_si128( *(int32_as*)(dst + dstPitch) );
        xm0 = _mm_avg_epu8( xm0, xm1 );
        xm2 = _mm_avg_epu8( xm2, xm3 );
        *(int32_as*)dst = _mm_cvtsi128_si32( xm0 );
        *(int32_as*)(dst + dstPitch) = _mm_cvtsi128_si32( xm2 );
        src += srcPitch * 2;
        dst += dstPitch * 2;
        xm0 = _mm_cvtsi32_si128( *(int32_as*)src );
        xm2 = _mm_cvtsi32_si128( *(int32_as*)(src + srcPitch) );
        xm1 = _mm_cvtsi32_si128( *(int32_as*)dst );
        xm3 = _mm_cvtsi32_si128( *(int32_as*)(dst + dstPitch) );
        xm0 = _mm_avg_epu8( xm0, xm1 );
        xm2 = _mm_avg_epu8( xm2, xm3 );
        *(int32_as*)dst = _mm_cvtsi128_si32( xm0 );
        *(int32_as*)(dst + dstPitch) = _mm_cvtsi128_si32( xm2 );

        if( (N -= 4) == 0 )
            break;
        src += srcPitch * 2;
        dst += dstPitch * 2;
    }
}

}   // namespace irk_avs_dec
