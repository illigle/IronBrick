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

#include "AvsDecUtility.h"

alignas(16) const int16_t s_RndCol[8] = {4, 4, 4, 4, 4, 4, 4, 4};
alignas(16) const int16_t s_RndRow[8] = {64, 64, 64, 64, 64, 64, 64, 64};

// 一维 DCT 变换, t1, t3, t5, t7 为临时寄存器
// 结果依次为 x0, x2, x4, x6, x1, x3, x5, x7
#define AVS_IDCT_1D(x0, x1, x2, x3, x4, x5, x6, x7, t1, t3, t5, t7) \
    t1 = _mm_adds_epi16( x1, x1 );   /* s1*2 */             \
    t7 = _mm_adds_epi16( x7, x7 );   /* s7*2 */             \
    t3 = _mm_adds_epi16( x3, x3 );   /* s3*2 */             \
    t5 = _mm_adds_epi16( x5, x5 );   /* s5*2 */             \
    x1 = _mm_adds_epi16( x1, t1 );   /* s1*3 */             \
    x7 = _mm_adds_epi16( x7, t7 );   /* s7*3 */             \
    x3 = _mm_adds_epi16( x3, t3 );   /* s3*3 */             \
    x5 = _mm_adds_epi16( x5, t5 );   /* s5*3 */             \
    t1 = _mm_adds_epi16( t1, x7 );   /* s1*2 + s7*3 */      \
    t5 = _mm_adds_epi16( t5, x3 );   /* s5*2 + s3*3 */      \
    x1 = _mm_subs_epi16( x1, t7 );   /* s1*3 - s7*2 */      \
    x5 = _mm_subs_epi16( x5, t3 );   /* s5*3 - s3*2 */      \
    t7 = _mm_subs_epi16( x1, t1 );   /* s1*1 - s7*5 */      \
    t3 = _mm_subs_epi16( x5, t5 );   /* s5*1 - s3*5 */      \
    x7 = _mm_adds_epi16( x1, t1 );   /* s1*5 + s7*1 */      \
    x3 = _mm_adds_epi16( x5, t5 );   /* s5*5 + s3*1 */      \
    x7 = _mm_adds_epi16( x7, t5 );  \
    t7 = _mm_adds_epi16( t7, x5 );  \
    x7 = _mm_adds_epi16( x7, x7 );  \
    t7 = _mm_adds_epi16( t7, t7 );  \
    t5 = _mm_adds_epi16( t5, x7 );   /* o0 = s1*10 + s3*9 + s5*6 + s7*2 */  \
    t7 = _mm_adds_epi16( t7, x5 );   /* o3 = s1*2 - s3*6 + s5*9 - s7*10 */  \
    t3 = _mm_adds_epi16( t3, t1 );  \
    x3 = _mm_subs_epi16( x1, x3 );  \
    t3 = _mm_adds_epi16( t3, t3 );  \
    x3 = _mm_adds_epi16( x3, x3 );  \
    t3 = _mm_adds_epi16( t3, t1 );   /* o2 = s1*6 - s2*10 + s5*2 + s7*9 */  \
    t1 = _mm_adds_epi16( x3, x1 );   /* o1 = s1*9 - s3*2 - s5*10 - s7*6 */  \
    x2 = _mm_adds_epi16( x2, x2 );  \
    x6 = _mm_adds_epi16( x6, x6 );  \
    x1 = _mm_slli_epi16( x2, 2 );   \
    x5 = _mm_slli_epi16( x6, 2 );   \
    x1 = _mm_adds_epi16( x1, x2 );   /* s2*10 */    \
    x5 = _mm_adds_epi16( x5, x6 );   /* s6*10 */    \
    x6 = _mm_adds_epi16( x6, x6 );   /* s6*4 */     \
    x2 = _mm_adds_epi16( x2, x2 );   /* s2*4 */     \
    x1 = _mm_adds_epi16( x1, x6 );   /* s2*10 + s6*4 */     \
    x5 = _mm_subs_epi16( x5, x2 );   /* s6*10 - s2*4 */     \
    x2 = _mm_subs_epi16( x0, x4 );  \
    x0 = _mm_adds_epi16( x0, x4 );  \
    x2 = _mm_slli_epi16( x2, 3 );    /* (s0 - s4)*8 */      \
    x0 = _mm_slli_epi16( x0, 3 );    /* (s0 + s4)*8 */      \
    x4 = _mm_adds_epi16( x2, x5 );   /* e2 */               \
    x2 = _mm_subs_epi16( x2, x5 );   /* e1 */               \
    x6 = _mm_subs_epi16( x0, x1 );   /* e3 */               \
    x0 = _mm_adds_epi16( x0, x1 );   /* e0 */               \
    x1 = _mm_subs_epi16( x6, t7 );   /* r4 = e3 - o3 */     \
    x3 = _mm_subs_epi16( x4, t3 );   /* r5 = e2 - o2 */     \
    x5 = _mm_subs_epi16( x2, t1 );   /* r6 = e1 - o1 */     \
    x7 = _mm_subs_epi16( x0, t5 );   /* r7 = e0 - o0 */     \
    x0 = _mm_adds_epi16( x0, t5 );   /* r0 = e0 + o0 */     \
    x2 = _mm_adds_epi16( x2, t1 );   /* r1 = e1 + o1 */     \
    x4 = _mm_adds_epi16( x4, t3 );   /* r2 = e2 + o2 */     \
    x6 = _mm_adds_epi16( x6, t7 );   /* r3 = e3 + o3 */

// 8x8 转置, t1 为临时寄存器
// 结果依次为: x0, x1, x5, x4, x7, x2, x6, x3
#define TRANSPOSE_8x8(x0, x1, x2, x3, x4, x5, x6, x7, t1) \
    t1 = x7;    \
    x7 = _mm_unpackhi_epi16( x0, x1 );  \
    x0 = _mm_unpacklo_epi16( x0, x1 );  \
    x1 = _mm_unpackhi_epi16( x2, x3 );  \
    x2 = _mm_unpacklo_epi16( x2, x3 );  \
    x3 = _mm_unpackhi_epi16( x4, x5 );  \
    x4 = _mm_unpacklo_epi16( x4, x5 );  \
    x5 = _mm_unpackhi_epi16( x6, t1 );  \
    x6 = _mm_unpacklo_epi16( x6, t1 );  \
    t1 = x5;    \
    x5 = _mm_unpackhi_epi32( x0, x2 );  \
    x0 = _mm_unpacklo_epi32( x0, x2 );  \
    x2 = _mm_unpackhi_epi32( x4, x6 );  \
    x4 = _mm_unpacklo_epi32( x4, x6 );  \
    x6 = _mm_unpackhi_epi32( x7, x1 );  \
    x7 = _mm_unpacklo_epi32( x7, x1 );  \
    x1 = _mm_unpackhi_epi32( x3, t1 );  \
    x3 = _mm_unpacklo_epi32( x3, t1 );  \
    t1 = x1;    \
    x1 = _mm_unpackhi_epi64( x0, x4 );  \
    x0 = _mm_unpacklo_epi64( x0, x4 );  \
    x4 = _mm_unpackhi_epi64( x5, x2 );  \
    x5 = _mm_unpacklo_epi64( x5, x2 );  \
    x2 = _mm_unpackhi_epi64( x7, x3 );  \
    x7 = _mm_unpacklo_epi64( x7, x3 );  \
    x3 = _mm_unpackhi_epi64( x6, t1 );  \
    x6 = _mm_unpacklo_epi64( x6, t1 );

// (x + rnd) >> shf
#define ROUND_SHIFT(x0, x1, x2, x3, x4, x5, x6, x7, rnd, shf) \
    x0 = _mm_adds_epi16( x0, rnd ); \
    x1 = _mm_adds_epi16( x1, rnd ); \
    x2 = _mm_adds_epi16( x2, rnd ); \
    x3 = _mm_adds_epi16( x3, rnd ); \
    x0 = _mm_srai_epi16( x0, shf ); \
    x1 = _mm_srai_epi16( x1, shf ); \
    x2 = _mm_srai_epi16( x2, shf ); \
    x3 = _mm_srai_epi16( x3, shf ); \
    x4 = _mm_adds_epi16( x4, rnd ); \
    x5 = _mm_adds_epi16( x5, rnd ); \
    x6 = _mm_adds_epi16( x6, rnd ); \
    x7 = _mm_adds_epi16( x7, rnd ); \
    x4 = _mm_srai_epi16( x4, shf ); \
    x5 = _mm_srai_epi16( x5, shf ); \
    x6 = _mm_srai_epi16( x6, shf ); \
    x7 = _mm_srai_epi16( x7, shf );

namespace irk_avs_dec {

// src: 列主序存储的 DCT 系数
void IDCT_8x8_add_sse2( const int16_t src[64], uint8_t* dst, int dstPitch )
{
    assert( ((uintptr_t)src & 15) == 0 );
    __m128i tm1, tm3, tm5, tm7;
    __m128i xm0 = _mm_load_si128( (__m128i*)(src + 8*0) );
    __m128i xm1 = _mm_load_si128( (__m128i*)(src + 8*1) );
    __m128i xm2 = _mm_load_si128( (__m128i*)(src + 8*2) );
    __m128i xm3 = _mm_load_si128( (__m128i*)(src + 8*3) );
    __m128i xm4 = _mm_load_si128( (__m128i*)(src + 8*4) );
    __m128i xm5 = _mm_load_si128( (__m128i*)(src + 8*5) );
    __m128i xm6 = _mm_load_si128( (__m128i*)(src + 8*6) );
    __m128i xm7 = _mm_load_si128( (__m128i*)(src + 8*7) );

    // 列变换, 结果在 xm0, xm2, xm4, xm6, xm1, xm3, xm5, xm7
    AVS_IDCT_1D( xm0, xm1, xm2, xm3, xm4, xm5, xm6, xm7, tm1, tm3, tm5, tm7 );

    // (x + 4) >> 3
    tm5 = _mm_load_si128( (__m128i*)s_RndCol );
    ROUND_SHIFT( xm0, xm2, xm4, xm6, xm1, xm3, xm5, xm7, tm5, 3 );

    // 8x8 转置, 结果在 xm0, xm2, xm3, xm1, xm7, xm4, xm5, xm6
    TRANSPOSE_8x8( xm0, xm2, xm4, xm6, xm1, xm3, xm5, xm7, tm5 );

    // 行变换, 结果在 xm0, xm3, xm7, xm5, xm2, xm1, xm4, xm6
    AVS_IDCT_1D( xm0, xm2, xm3, xm1, xm7, xm4, xm5, xm6, tm1, tm3, tm5, tm7 );

    // (x + 64) >> 7
    tm5 = _mm_load_si128( (__m128i*)s_RndRow );
    ROUND_SHIFT( xm0, xm3, xm7, xm5, xm2, xm1, xm4, xm6, tm5, 7 );

    // 与预测值相加
    tm5 = _mm_setzero_si128();  
    tm1 = _mm_loadl_epi64( (__m128i*)dst );
    tm3 = _mm_loadl_epi64( (__m128i*)(dst + dstPitch) );
    tm1 = _mm_unpacklo_epi8( tm1, tm5 );
    tm3 = _mm_unpacklo_epi8( tm3, tm5 );
    tm1 = _mm_adds_epi16( tm1, xm0 );
    tm3 = _mm_adds_epi16( tm3, xm3 );
    tm1 = _mm_packus_epi16( tm1, tm1 );
    tm3 = _mm_packus_epi16( tm3, tm3 );
    _mm_storel_epi64( (__m128i*)dst, tm1 );
    _mm_storel_epi64( (__m128i*)(dst + dstPitch), tm3 );
    dst += dstPitch * 2;
    tm1 = _mm_loadl_epi64( (__m128i*)dst );
    tm3 = _mm_loadl_epi64( (__m128i*)(dst + dstPitch) );
    tm1 = _mm_unpacklo_epi8( tm1, tm5 );
    tm3 = _mm_unpacklo_epi8( tm3, tm5 );
    tm1 = _mm_adds_epi16( tm1, xm7 );
    tm3 = _mm_adds_epi16( tm3, xm5 );
    tm1 = _mm_packus_epi16( tm1, tm1 );
    tm3 = _mm_packus_epi16( tm3, tm3 );
    _mm_storel_epi64( (__m128i*)dst, tm1 );
    _mm_storel_epi64( (__m128i*)(dst + dstPitch), tm3 );
    dst += dstPitch * 2;
    tm1 = _mm_loadl_epi64( (__m128i*)dst );
    tm3 = _mm_loadl_epi64( (__m128i*)(dst + dstPitch) );
    tm1 = _mm_unpacklo_epi8( tm1, tm5 );
    tm3 = _mm_unpacklo_epi8( tm3, tm5 );
    tm1 = _mm_adds_epi16( tm1, xm2 );
    tm3 = _mm_adds_epi16( tm3, xm1 );
    tm1 = _mm_packus_epi16( tm1, tm1 );
    tm3 = _mm_packus_epi16( tm3, tm3 );
    _mm_storel_epi64( (__m128i*)dst, tm1 );
    _mm_storel_epi64( (__m128i*)(dst + dstPitch), tm3 );
    dst += dstPitch * 2;
    tm1 = _mm_loadl_epi64( (__m128i*)dst );
    tm3 = _mm_loadl_epi64( (__m128i*)(dst + dstPitch) );
    tm1 = _mm_unpacklo_epi8( tm1, tm5 );
    tm3 = _mm_unpacklo_epi8( tm3, tm5 );
    tm1 = _mm_adds_epi16( tm1, xm4 );
    tm3 = _mm_adds_epi16( tm3, xm6 );
    tm1 = _mm_packus_epi16( tm1, tm1 );
    tm3 = _mm_packus_epi16( tm3, tm3 );
    _mm_storel_epi64( (__m128i*)dst, tm1 );
    _mm_storel_epi64( (__m128i*)(dst + dstPitch), tm3 );
}

//======================================================================================================================
// 标准 C 实现, 用作参考

// 转置矩阵
static const int16_t s_TMatrix[64] = 
{
    8,  10,  10,   9,   8,   6,   4,   2,
    8,   9,   4,  -2,  -8, -10, -10,  -6,
    8,   6,  -4, -10,  -8,   2,  10,   9,
    8,   2, -10,  -6,   8,   9,  -4, -10,
    8,  -2, -10,   6,   8,  -9,  -4,  10,
    8,  -6,  -4,  10,  -8,  -2,  10,  -9,
    8,  -9,   4,   2,  -8,  10, -10,   6,
    8, -10,  10,  -9,   8,  -6,   4,  -2,
};

void IDCT_8x8_add_c( const int16_t src[64], uint8_t* dst, int dstPitch )
{
    int16_t temp[64];

    // 列变换, 输入为列序, 输出也为列序
    for( int j = 0; j < 8; j++ )
    {
        const int16_t* tmCol = s_TMatrix + 8 * j;
        for( int i = 0; i < 8; i++ )
        {
            int sum = 0;
            sum += src[i+8*0] * tmCol[0] + src[i+8*1] * tmCol[1];
            sum += src[i+8*2] * tmCol[2] + src[i+8*3] * tmCol[3];
            sum += src[i+8*4] * tmCol[4] + src[i+8*5] * tmCol[5];
            sum += src[i+8*6] * tmCol[6] + src[i+8*7] * tmCol[7];       
            temp[i+j*8] = (int16_t)clip3( (sum + 4) >> 3, -32768, 32767 );
        }
    }
 
    // 行变换
    for( int i = 0; i < 8; i++ )
    {
        uint8_t* dstRow = dst + dstPitch * i;
        const int16_t* tmRow = s_TMatrix + 8 * i;
        for( int j = 0; j < 8; j++ )
        {
            int sum = 0;
            sum += tmRow[0] * temp[8*j+0] + tmRow[1] * temp[8*j+1];
            sum += tmRow[2] * temp[8*j+2] + tmRow[3] * temp[8*j+3];
            sum += tmRow[4] * temp[8*j+4] + tmRow[5] * temp[8*j+5];
            sum += tmRow[6] * temp[8*j+6] + tmRow[7] * temp[8*j+7];
            sum = (int16_t)clip3( (sum + 64) >> 7, -32768, 32767 );
            dstRow[j] =  (uint8_t)clip3( dstRow[j] + sum, 0, 255 );
        }
    }
}

}   // namespace irk_avs_dec
