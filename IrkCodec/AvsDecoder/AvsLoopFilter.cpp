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

// 参考帧索引是否不同
#define REFIDX_DIFF( s, d ) (((s)[0] ^ (d)[0]) | ((s)[1] ^ (d)[1]))

// 计算宏块环路滤波强度
uint32_t calc_BS_P16x16( int refIdx, MvUnion curMv, const MbContext* leftMb, const MbContext* topMb )
{
    uint32_t lfBS = 0;

    if( leftMb->ipMode[0] >= 0 )    // 左边为帧内预测宏块
    {
        lfBS |= 0xA;
    }
    else
    {
        if( refIdx != leftMb->refIdxs[0][0] )   // 参考图像不同
        {
            lfBS |= 0x1;
        }
        else
        {
            int dmv = abs( curMv.x - leftMb->mvs[0][0].x );
            dmv |= abs( curMv.y - leftMb->mvs[0][0].y );
            lfBS |= ((3 - dmv) >> 31) & 0x1;
        }
        if( refIdx != leftMb->refIdxs[1][0] )   // 参考图像不同
        {
            lfBS |= 0x4;
        }
        else
        {
            int dmv = abs( curMv.x - leftMb->mvs[1][0].x );
            dmv |= abs( curMv.y - leftMb->mvs[1][0].y );
            lfBS |= ((3 - dmv) >> 31) & 0x4;
        }
    }

    if( topMb->ipMode[0] >= 0 )    // 上边为帧内预测宏块
    {
        lfBS |= 0xA00;
    }
    else
    {
        if( refIdx != topMb->refIdxs[0][0] )    // 参考图像不同
        {
            lfBS |= 0x100;
        }
        else
        {
            int dmv = abs( curMv.x - topMb->mvs[0][0].x );
            dmv |= abs( curMv.y - topMb->mvs[0][0].y );
            lfBS |= ((3 - dmv) >> 31) & 0x100;
        }
        if( refIdx != topMb->refIdxs[1][0] )    // 参考图像不同
        {
            lfBS |= 0x400;
        }
        else
        {
            int dmv = abs( curMv.x - topMb->mvs[1][0].x );
            dmv |= abs( curMv.y - topMb->mvs[1][0].y );
            lfBS |= ((3 - dmv) >> 31) & 0x400;
        }
    }

    return lfBS;
}

// 计算宏块环路滤波强度
uint32_t calc_BS_P16x8( const int8_t refIdxs[2], const MvUnion curMvs[2], 
                                const MbContext* leftMb, const MbContext* topMb )
{
    uint32_t lfBS = 0;

    if( leftMb->ipMode[0] >= 0 )    // 左边为帧内预测宏块
    {
        lfBS |= 0xA;
    }
    else
    {
        if( refIdxs[0] != leftMb->refIdxs[0][0] )   // 参考图像不同
        {
            lfBS |= 0x1;
        }
        else
        {
            int dmv = abs( curMvs[0].x - leftMb->mvs[0][0].x );
            dmv |= abs( curMvs[0].y - leftMb->mvs[0][0].y );
            lfBS |= ((3 - dmv) >> 31) & 0x1;
        }
        if( refIdxs[1] != leftMb->refIdxs[1][0] )   // 参考图像不同
        {
            lfBS |= 0x4;
        }
        else
        {
            int dmv = abs( curMvs[1].x - leftMb->mvs[1][0].x );
            dmv |= abs( curMvs[1].y - leftMb->mvs[1][0].y );
            lfBS |= ((3 - dmv) >> 31) & 0x4;
        }
    }

    if( topMb->ipMode[0] >= 0 )    // 上边为帧内预测宏块
    {
        lfBS |= 0xA00;
    }
    else
    {
        if( refIdxs[0] != topMb->refIdxs[0][0] )    // 参考图像不同
        {
            lfBS |= 0x100;
        }
        else
        {
            int dmv = abs( curMvs[0].x - topMb->mvs[0][0].x );
            dmv |= abs( curMvs[0].y - topMb->mvs[0][0].y );
            lfBS |= ((3 - dmv) >> 31) & 0x100;
        }
        if( refIdxs[0] != topMb->refIdxs[1][0] )    // 参考图像不同
        {
            lfBS |= 0x400;
        }
        else
        {
            int dmv = abs( curMvs[0].x - topMb->mvs[1][0].x );
            dmv |= abs( curMvs[0].y - topMb->mvs[1][0].y );
            lfBS |= ((3 - dmv) >> 31) & 0x400;
        }
    }

    if( refIdxs[0] != refIdxs[1] )
    {
        lfBS |= 0x5000;
    }
    else
    {
        int dmv = abs( curMvs[0].x - curMvs[1].x );
        dmv |= abs( curMvs[0].y - curMvs[1].y );
        lfBS |= ((3 - dmv) >> 31) & 0x5000;
    }

    return lfBS;
}

// 计算宏块环路滤波强度
uint32_t calc_BS_P8x16( const int8_t refIdxs[2], const MvUnion curMvs[2], 
                                const MbContext* leftMb, const MbContext* topMb )
{
    uint32_t lfBS = 0;

    if( leftMb->ipMode[0] >= 0 )    // 左边为帧内预测宏块
    {
        lfBS |= 0xA;
    }
    else
    {
        if( refIdxs[0] != leftMb->refIdxs[0][0] )   // 参考图像不同
        {
            lfBS |= 0x1;
        }
        else
        {
            int dmv = abs( curMvs[0].x - leftMb->mvs[0][0].x );
            dmv |= abs( curMvs[0].y - leftMb->mvs[0][0].y );
            lfBS |= ((3 - dmv) >> 31) & 0x1;
        }
        if( refIdxs[0] != leftMb->refIdxs[1][0] )   // 参考图像不同
        {
            lfBS |= 0x4;
        }
        else
        {
            int dmv = abs( curMvs[0].x - leftMb->mvs[1][0].x );
            dmv |= abs( curMvs[0].y - leftMb->mvs[1][0].y );
            lfBS |= ((3 - dmv) >> 31) & 0x4;
        }
    }

    if( refIdxs[0] != refIdxs[1] )
    {
        lfBS |= 0x50;
    }
    else
    {
        int dmv = abs( curMvs[0].x - curMvs[1].x );
        dmv |= abs( curMvs[0].y - curMvs[1].y );
        lfBS |= ((3 - dmv) >> 31) & 0x50;
    }

    if( topMb->ipMode[0] >= 0 )    // 上边为帧内预测宏块
    {
        lfBS |= 0xA00;
    }
    else
    {
        if( refIdxs[0] != topMb->refIdxs[0][0] )    // 参考图像不同
        {
            lfBS |= 0x100;
        }
        else
        {
            int dmv = abs( curMvs[0].x - topMb->mvs[0][0].x );
            dmv |= abs( curMvs[0].y - topMb->mvs[0][0].y );
            lfBS |= ((3 - dmv) >> 31) & 0x100;
        }
        if( refIdxs[1] != topMb->refIdxs[1][0] )    // 参考图像不同
        {
            lfBS |= 0x400;
        }
        else
        {
            int dmv = abs( curMvs[1].x - topMb->mvs[1][0].x );
            dmv |= abs( curMvs[1].y - topMb->mvs[1][0].y );
            lfBS |= ((3 - dmv) >> 31) & 0x400;
        }
    }

    return lfBS;
}

// 计算宏块环路滤波强度
uint32_t calc_BS_P8x8( const int8_t refIdxs[4], const MvUnion curMvs[4], 
                                const MbContext* leftMb, const MbContext* topMb )
{
    uint32_t lfBS = 0;

    if( leftMb->ipMode[0] >= 0 )    // 左边为帧内预测宏块
    {
        lfBS |= 0xA;
    }
    else
    {
        if( refIdxs[0] != leftMb->refIdxs[0][0] )   // 参考图像不同
        {
            lfBS |= 0x1;
        }
        else
        {
            int dmv = abs( curMvs[0].x - leftMb->mvs[0][0].x );
            dmv |= abs( curMvs[0].y - leftMb->mvs[0][0].y );
            lfBS |= ((3 - dmv) >> 31) & 0x1;
        }
        if( refIdxs[2] != leftMb->refIdxs[1][0] )   // 参考图像不同
        {
            lfBS |= 0x4;
        }
        else
        {
            int dmv = abs( curMvs[2].x - leftMb->mvs[1][0].x );
            dmv |= abs( curMvs[2].y - leftMb->mvs[1][0].y );
            lfBS |= ((3 - dmv) >> 31) & 0x4;
        }
    }

    if( refIdxs[0] != refIdxs[1] )
    {
        lfBS |= 0x10;
    }
    else
    {
        int dmv = abs( curMvs[0].x - curMvs[1].x );
        dmv |= abs( curMvs[0].y - curMvs[1].y );
        lfBS |= ((3 - dmv) >> 31) & 0x10;
    }
    if( refIdxs[2] != refIdxs[3] )
    {
        lfBS |= 0x40;
    }
    else
    {
        int dmv = abs( curMvs[2].x - curMvs[3].x );
        dmv |= abs( curMvs[2].y - curMvs[3].y );
        lfBS |= ((3 - dmv) >> 31) & 0x40;
    }

    if( topMb->ipMode[0] >= 0 )    // 上边为帧内预测宏块
    {
        lfBS |= 0xA00;
    }
    else
    {
        if( refIdxs[0] != topMb->refIdxs[0][0] )    // 参考图像不同
        {
            lfBS |= 0x100;
        }
        else
        {
            int dmv = abs( curMvs[0].x - topMb->mvs[0][0].x );
            dmv |= abs( curMvs[0].y - topMb->mvs[0][0].y );
            lfBS |= ((3 - dmv) >> 31) & 0x100;
        }
        if( refIdxs[1] != topMb->refIdxs[1][0] )    // 参考图像不同
        {
            lfBS |= 0x400;
        }
        else
        {
            int dmv = abs( curMvs[1].x - topMb->mvs[1][0].x );
            dmv |= abs( curMvs[1].y - topMb->mvs[1][0].y );
            lfBS |= ((3 - dmv) >> 31) & 0x400;
        }
    }

    if( refIdxs[0] != refIdxs[2] )
    {
        lfBS |= 0x1000;
    }
    else
    {
        int dmv = abs( curMvs[0].x - curMvs[2].x );
        dmv |= abs( curMvs[0].y - curMvs[2].y );
        lfBS |= ((3 - dmv) >> 31) & 0x1000;
    }
    if( refIdxs[1] != refIdxs[3] )
    {
        lfBS |= 0x4000;
    }
    else
    {
        int dmv = abs( curMvs[1].x - curMvs[3].x );
        dmv |= abs( curMvs[1].y - curMvs[3].y );
        lfBS |= ((3 - dmv) >> 31) & 0x4000;
    }

    return lfBS;
}

// 计算宏块环路滤波强度
uint32_t calc_BS_B16x16( const int8_t refIdxs[2], const MvUnion curMvs[2], 
                            const MbContext* leftMb, const MbContext* topMb )
{
    uint32_t lfBS = 0;
    __m128i xm3 = _mm_set1_epi32( 0x30003 );
    __m128i xm0 = _mm_loadl_epi64( (__m128i*)curMvs );
    xm0 = _mm_unpacklo_epi64( xm0, xm0 );
    
    if( leftMb->ipMode[0] >= 0 )    // 左边为帧内预测宏块
    {
        lfBS |= 0xA;
    }
    else
    {
        __m128i xm1 = _mm_loadu_si128( (__m128i*)(leftMb->mvs) );
        xm1 = _mm_subs_epi16( xm1, xm0 );
        xm1 = _mm_abs_epi16( xm1 );
        xm1 = _mm_cmpgt_epi16( xm1, xm3 );
        int mvMsk = _mm_movemask_epi8( xm1 );

        if( REFIDX_DIFF( refIdxs, leftMb->refIdxs[0] ) | (mvMsk & 0xFF) )
        {
            lfBS |= 0x1;
        }
        if( REFIDX_DIFF( refIdxs, leftMb->refIdxs[1] ) | (mvMsk & 0xFF00) )
        {
            lfBS |= 0x4;
        }
    }

    if( topMb->ipMode[0] >= 0 )    // 上边为帧内预测宏块
    {
        lfBS |= 0xA00;
    }
    else
    {
        __m128i xm1 = _mm_loadu_si128( (__m128i*)(topMb->mvs) );
        xm1 = _mm_subs_epi16( xm1, xm0 );
        xm1 = _mm_abs_epi16( xm1 );
        xm1 = _mm_cmpgt_epi16( xm1, xm3 );
        int mvMsk = _mm_movemask_epi8( xm1 );

        if( REFIDX_DIFF( refIdxs, topMb->refIdxs[0] ) | (mvMsk & 0xFF) )
        {
            lfBS |= 0x100;
        }
        if( REFIDX_DIFF( refIdxs, topMb->refIdxs[1] ) | (mvMsk & 0xFF00) )
        {
            lfBS |= 0x400;
        }
    }

    return lfBS;
}

// 计算宏块环路滤波强度
uint32_t calc_BS_B16x8( const int8_t refIdxs[2][2], const MvUnion curMvs[2][2], 
                            const MbContext* leftMb, const MbContext* topMb )
{
    uint32_t lfBS = 0;
    __m128i xm3 = _mm_set1_epi32( 0x30003 );
    __m128i xm0 = _mm_loadu_si128( (__m128i*)curMvs );
    
    if( leftMb->ipMode[0] >= 0 )    // 左边为帧内预测宏块
    {
        lfBS |= 0xA;
    }
    else
    {
        __m128i xm1 = _mm_loadu_si128( (__m128i*)(leftMb->mvs) );
        xm1 = _mm_subs_epi16( xm1, xm0 );
        xm1 = _mm_abs_epi16( xm1 );
        xm1 = _mm_cmpgt_epi16( xm1, xm3 );
        int mvMsk = _mm_movemask_epi8( xm1 );

        if( REFIDX_DIFF( refIdxs[0], leftMb->refIdxs[0] ) | (mvMsk & 0xFF) )
        {
            lfBS |= 0x1;
        }
        if( REFIDX_DIFF( refIdxs[1], leftMb->refIdxs[1] ) | (mvMsk & 0xFF00) )
        {
            lfBS |= 0x4;
        }
    }

    if( topMb->ipMode[0] >= 0 )    // 上边为帧内预测宏块
    {
        lfBS |= 0xA00;
    }
    else
    {
        __m128i xm1 = _mm_loadu_si128( (__m128i*)(topMb->mvs) );
        xm0 = _mm_unpacklo_epi64( xm0, xm0 );
        xm1 = _mm_subs_epi16( xm1, xm0 );
        xm1 = _mm_abs_epi16( xm1 );
        xm1 = _mm_cmpgt_epi16( xm1, xm3 );
        int mvMsk = _mm_movemask_epi8( xm1 );

        if( REFIDX_DIFF( refIdxs[0], topMb->refIdxs[0] ) | (mvMsk & 0xFF) )
        {
            lfBS |= 0x100;
        }
        if( REFIDX_DIFF( refIdxs[0], topMb->refIdxs[1] ) | (mvMsk & 0xFF00) )
        {
            lfBS |= 0x400;
        }
    }

    if( REFIDX_DIFF( refIdxs[0], refIdxs[1] ) )
    {
        lfBS |= 0x5000;
    }
    else
    {
        int dmv = abs( curMvs[0][0].x - curMvs[1][0].x );
        dmv |= abs( curMvs[0][0].y - curMvs[1][0].y );
        dmv |= abs( curMvs[0][1].x - curMvs[1][1].x );
        dmv |= abs( curMvs[0][1].y - curMvs[1][1].y );
        lfBS |= ((3 - dmv) >> 31) & 0x5000;
    }

    return lfBS;
}

// 计算宏块环路滤波强度
uint32_t calc_BS_B8x16( const int8_t refIdxs[2][2], const MvUnion curMvs[2][2], 
                            const MbContext* leftMb, const MbContext* topMb )
{
    uint32_t lfBS = 0;
    __m128i xm3 = _mm_set1_epi32( 0x30003 );
    
    if( leftMb->ipMode[0] >= 0 )    // 左边为帧内预测宏块
    {
        lfBS |= 0xA;
    }
    else
    {
        __m128i xm0 = _mm_loadu_si128( (__m128i*)curMvs );
        __m128i xm1 = _mm_loadu_si128( (__m128i*)(leftMb->mvs) );
        xm0 = _mm_unpacklo_epi64( xm0, xm0 );
        xm1 = _mm_subs_epi16( xm1, xm0 );
        xm1 = _mm_abs_epi16( xm1 );
        xm1 = _mm_cmpgt_epi16( xm1, xm3 );
        int mvMsk = _mm_movemask_epi8( xm1 );

        if( REFIDX_DIFF( refIdxs[0], leftMb->refIdxs[0] ) | (mvMsk & 0xFF) )
        {
            lfBS |= 0x1;
        }
        if( REFIDX_DIFF( refIdxs[0], leftMb->refIdxs[1] ) | (mvMsk & 0xFF00) )
        {
            lfBS |= 0x4;
        }
    }

    if( REFIDX_DIFF( refIdxs[0], refIdxs[1] ) )
    {
        lfBS |= 0x50;
    }
    else
    {
        int dmv = abs( curMvs[0][0].x - curMvs[1][0].x );
        dmv |= abs( curMvs[0][0].y - curMvs[1][0].y );
        dmv |= abs( curMvs[0][1].x - curMvs[1][1].x );
        dmv |= abs( curMvs[0][1].y - curMvs[1][1].y );
        lfBS |= ((3 - dmv) >> 31) & 0x50;
    }

    if( topMb->ipMode[0] >= 0 )    // 上边为帧内预测宏块
    {
        lfBS |= 0xA00;
    }
    else
    {
        __m128i xm0 = _mm_loadu_si128( (__m128i*)curMvs );
        __m128i xm1 = _mm_loadu_si128( (__m128i*)(topMb->mvs) );
        xm1 = _mm_subs_epi16( xm1, xm0 );
        xm1 = _mm_abs_epi16( xm1 );
        xm1 = _mm_cmpgt_epi16( xm1, xm3 );
        int mvMsk = _mm_movemask_epi8( xm1 );

        if( REFIDX_DIFF( refIdxs[0], topMb->refIdxs[0] ) | (mvMsk & 0xFF) )
        {
            lfBS |= 0x100;
        }
        if( REFIDX_DIFF( refIdxs[1], topMb->refIdxs[1] ) | (mvMsk & 0xFF00) )
        {
            lfBS |= 0x400;
        }
    }

    return lfBS;
}

// 计算宏块环路滤波强度
uint32_t calc_BS_B8x8( const int8_t refIdxs[4][2], const MvUnion curMvs[4][2], 
                            const MbContext* leftMb, const MbContext* topMb )
{
    uint32_t lfBS = 0;
    uint32_t mvMsk = 0;
    __m128i xm3 = _mm_set1_epi32( 0x30003 );
    __m128i xm2 = _mm_loadu_si128( (__m128i*)(curMvs[0]) );     // 0, 1
    __m128i xm4 = _mm_loadu_si128( (__m128i*)(curMvs[2]) );     // 2, 3
    __m128i xm0 = _mm_unpacklo_epi64( xm2, xm4 );               // 0, 2

    // 垂直边界 1
    if( leftMb->ipMode[0] >= 0 )    // 左边为帧内预测宏块
    {
        lfBS |= 0xA;
    }
    else
    {
        __m128i xm1 = _mm_loadu_si128( (__m128i*)(leftMb->mvs) );
        xm1 = _mm_subs_epi16( xm1, xm0 );
        xm1 = _mm_abs_epi16( xm1 );
        xm1 = _mm_cmpgt_epi16( xm1, xm3 );
        mvMsk = _mm_movemask_epi8( xm1 );

        if( REFIDX_DIFF(refIdxs[0], leftMb->refIdxs[0]) | (mvMsk & 0xFF) )
        {
            lfBS |= 0x1;
        }
        if( REFIDX_DIFF(refIdxs[2], leftMb->refIdxs[1]) | (mvMsk & 0xFF00) )
        {
            lfBS |= 0x4;
        }
    }

    // 垂直边界 2
    __m128i xm1 = _mm_unpackhi_epi64( xm2, xm4 );   // 1, 3
    xm1 = _mm_subs_epi16( xm1, xm0 );
    xm1 = _mm_abs_epi16( xm1 );
    xm1 = _mm_cmpgt_epi16( xm1, xm3 );
    mvMsk = _mm_movemask_epi8( xm1 );
    if( REFIDX_DIFF(refIdxs[0], refIdxs[1]) | (mvMsk & 0xFF) )
    {
        lfBS |= 0x10;
    }
    if( REFIDX_DIFF(refIdxs[2], refIdxs[3]) | (mvMsk & 0xFF00) )
    {
        lfBS |= 0x40;
    }

    // 水平边界 1
    if( topMb->ipMode[0] >= 0 )    // 上边为帧内预测宏块
    {
        lfBS |= 0xA00;
    }
    else
    {
        __m128i xm1 = _mm_loadu_si128( (__m128i*)(topMb->mvs) );
        xm1 = _mm_subs_epi16( xm1, xm2 );
        xm1 = _mm_abs_epi16( xm1 );
        xm1 = _mm_cmpgt_epi16( xm1, xm3 );
        mvMsk = _mm_movemask_epi8( xm1 );

        if( REFIDX_DIFF(refIdxs[0], topMb->refIdxs[0]) | (mvMsk & 0xFF) ) 
        {
            lfBS |= 0x100;
        }
        if( REFIDX_DIFF(refIdxs[1], topMb->refIdxs[1]) | (mvMsk & 0xFF00) )
        {
            lfBS |= 0x400;
        }
    }

    // 水平边界2
    xm2 = _mm_subs_epi16( xm2, xm4 );
    xm2 = _mm_abs_epi16( xm2 );
    xm2 = _mm_cmpgt_epi16( xm2, xm3 );
    mvMsk = _mm_movemask_epi8( xm2 );
    if( REFIDX_DIFF(refIdxs[0], refIdxs[2]) | (mvMsk & 0xFF) )
    {
        lfBS |= 0x1000;
    }
    if( REFIDX_DIFF(refIdxs[1], refIdxs[3]) | (mvMsk & 0xFF00) )
    {
        lfBS |= 0x4000;
    }

    return lfBS;
}

//======================================================================================================================
alignas(16) const int16_t s_Add2x8[8] = { 2, 2, 2, 2, 2, 2, 2, 2 };
alignas(16) const int16_t s_Add4x8[8] = { 4, 4, 4, 4, 4, 4, 4, 4 };

// GCC 无法生成最优的指令
#if defined(_MSC_VER) && defined(NDEBUG)
#define loadzx_epu8_epi16( addr ) _mm_cvtepu8_epi16( *(__m128i*)(addr) )
#else
#define loadzx_epu8_epi16( addr ) _mm_cvtepu8_epi16( _mm_loadl_epi64( (__m128i*)(addr) ) )
#endif

static void LF_luma_ver_bs2_sse4( uint8_t* data, int pitch, int16_t alpha, int16_t beta )
{
    __m128i p2, p1, p0, q0, q1, q2;
    __m128i m0, m1, m2, m3;
    __m128i x0, x1;
    __m128i add2 = _mm_load_si128( (__m128i*)s_Add2x8 );
    const int pitch2 = pitch + pitch;
    const int pitch3 = pitch + pitch2;

    // 读取并转置
    uint8_t* src = data - 4;
    m0 = _mm_loadl_epi64( (__m128i*)(src) );
    x0 = _mm_loadl_epi64( (__m128i*)(src + pitch) );
    m1 = _mm_loadl_epi64( (__m128i*)(src + pitch2) );
    x1 = _mm_loadl_epi64( (__m128i*)(src + pitch3) );
    m0 = _mm_unpacklo_epi8( m0, x0 );
    m1 = _mm_unpacklo_epi8( m1, x1 );
    src += pitch * 4;
    m2 = _mm_loadl_epi64( (__m128i*)(src) );
    x0 = _mm_loadl_epi64( (__m128i*)(src + pitch) );
    m3 = _mm_loadl_epi64( (__m128i*)(src + pitch2) );
    x1 = _mm_loadl_epi64( (__m128i*)(src + pitch3) );
    m2 = _mm_unpacklo_epi8( m2, x0 );
    m3 = _mm_unpacklo_epi8( m3, x1 );
    q0 = _mm_unpackhi_epi16( m0, m1 );
    q1 = _mm_unpackhi_epi16( m2, m3 );
    m0 = _mm_unpacklo_epi16( m0, m1 );
    m2 = _mm_unpacklo_epi16( m2, m3 );
    q2 = _mm_unpackhi_epi32( q0, q1 );
    p1 = _mm_unpackhi_epi32( m0, m2 );
    q0 = _mm_unpacklo_epi32( q0, q1 );
    m0 = _mm_unpacklo_epi32( m0, m2 );
    q1 = _mm_srli_si128( q0, 8 );
    p0 = _mm_srli_si128( p1, 8 );
    p2 = _mm_srli_si128( m0, 8 );

    // 转化为 int16
    q0 = _mm_cvtepu8_epi16( q0 );
    q1 = _mm_cvtepu8_epi16( q1 );
    q2 = _mm_cvtepu8_epi16( q2 );
    p0 = _mm_cvtepu8_epi16( p0 );
    p1 = _mm_cvtepu8_epi16( p1 );
    p2 = _mm_cvtepu8_epi16( p2 );

    // 计算 mask
    m0 = _mm_set1_epi16( alpha );
    m2 = _mm_set1_epi16( beta );
    x0 = _mm_sub_epi16( p0, q0 );
    m3 = _mm_srai_epi16( m0, 2 );
    x0 = _mm_abs_epi16( x0 );           // abs(p0 - q0)
    m3 = _mm_add_epi16( m3, add2 );     // (alpha >> 2) + 2
    m0 = _mm_cmpgt_epi16( m0, x0 );     // abs(p0 - q0) < alpha
    m3 = _mm_cmpgt_epi16( m3, x0 );     // abs(p0 - q0) < (alpha >> 2) + 2
    x0 = _mm_sub_epi16( p1, p0 );
    x1 = _mm_sub_epi16( q1, q0 );
    x0 = _mm_abs_epi16( x0 );           // abs(p1 - p0)
    x1 = _mm_abs_epi16( x1 );           // abs(q1 - q0)
    x0 = _mm_cmpgt_epi16( m2, x0 );     // abs(p1 - p0) < beta
    x1 = _mm_cmpgt_epi16( m2, x1 );     // abs(q1 - q0) < beta
    m0 = _mm_and_si128( m0, x0 );
    m0 = _mm_and_si128( m0, x1 );       // abs(p0 - q0) < alpha && abs(p1 - p0) < beta && abs(q1 - q0) < beta
    if( _mm_testz_si128( m0, m0 ) )
        return;

    p2 = _mm_sub_epi16( p2, p0 );
    q2 = _mm_sub_epi16( q2, q0 );
    p2 = _mm_abs_epi16( p2 );           // abs(p2 - p0)
    q2 = _mm_abs_epi16( q2 );           // abs(q2 - q0)
    m1 = _mm_cmpgt_epi16( m2, p2 );     // abs(p2 - p0) < beta
    m2 = _mm_cmpgt_epi16( m2, q2 );     // abs(q2 - q0) < beta
    m1 = _mm_and_si128( m1, m3 );       // abs(p2 - p0) < beta && abs(p0 - q0) < (alpha >> 2) + 2
    m2 = _mm_and_si128( m2, m3 );       // abs(q2 - q0) < beta && abs(p0 - q0) < (alpha >> 2) + 2

    m3 = _mm_add_epi16( p0, q0 );
    m3 = _mm_add_epi16( m3, add2 );     // p0 + q0 + 2
    x0 = _mm_add_epi16( p1, p0 );
    x1 = _mm_add_epi16( p1, p1 );
    x0 = _mm_add_epi16( x0, m3 );       // p1 + 2*p0 + q0 + 2
    x1 = _mm_add_epi16( x1, m3 );       // 2*p1 + p0 + q0 + 2
    x0 = _mm_srai_epi16( x0, 2 );
    x1 = _mm_srai_epi16( x1, 2 );
    x0 = _mm_blendv_epi8( x1, x0, m1 ); // select p0
    m1 = _mm_and_si128( m1, m0 );
    p0 = _mm_blendv_epi8( p0, x0, m0 );
    p1 = _mm_blendv_epi8( p1, x1, m1 );
    x0 = _mm_add_epi16( q1, q0 ); 
    x1 = _mm_add_epi16( q1, q1 );
    x0 = _mm_add_epi16( x0, m3 );       // q1 + 2*q0 + p0 + 2
    x1 = _mm_add_epi16( x1, m3 );       // 2*q1 + q0 + p0 + 2
    x0 = _mm_srai_epi16( x0, 2 );
    x1 = _mm_srai_epi16( x1, 2 );
    x0 = _mm_blendv_epi8( x1, x0, m2 ); // select q0
    m2 = _mm_and_si128( m2, m0 );
    q0 = _mm_blendv_epi8( q0, x0, m0 );
    q1 = _mm_blendv_epi8( q1, x1, m2 );

    // 转化为 uint8
    p1 = _mm_packus_epi16( p1, p1 );
    p0 = _mm_packus_epi16( p0, p0 ); 
    q0 = _mm_packus_epi16( q0, q0 );
    q1 = _mm_packus_epi16( q1, q1 );

    // 转置
    p1 = _mm_unpacklo_epi8( p1, p0 );
    q0 = _mm_unpacklo_epi8( q0, q1 );
    p0 = _mm_unpackhi_epi16( p1, q0 );
    p1 = _mm_unpacklo_epi16( p1, q0 );

    // 输出
    uint8_t* dst = data - 2;
#ifdef _MSC_VER
    *(int32_as*)dst = _mm_cvtsi128_si32( p1 );
    p1 = _mm_srli_si128( p1, 4 );
    *(int32_as*)(dst + pitch) = _mm_cvtsi128_si32( p1 );
    p1 = _mm_srli_si128( p1, 4 );
    *(int32_as*)(dst + pitch2) = _mm_cvtsi128_si32( p1 );
    p1 = _mm_srli_si128( p1, 4 );
    *(int32_as*)(dst + pitch3) = _mm_cvtsi128_si32( p1 );
    dst += pitch * 4;
    *(int32_as*)dst = _mm_cvtsi128_si32( p0 );
    p0 = _mm_srli_si128( p0, 4 );
    *(int32_as*)(dst + pitch) = _mm_cvtsi128_si32( p0 );
    p0 = _mm_srli_si128( p0, 4 );
    *(int32_as*)(dst + pitch2) = _mm_cvtsi128_si32( p0 );
    p0 = _mm_srli_si128( p0, 4 );
    *(int32_as*)(dst + pitch3) = _mm_cvtsi128_si32( p0 );
#else
    alignas(16) uint8_t buf[32];  
    _mm_store_si128( (__m128i*)buf, p1 );
    _mm_store_si128( (__m128i*)(buf + 16), p0 );
    *(int32_as*)dst = *(int32_as*)buf;
    *(int32_as*)(dst + pitch) = *(int32_as*)(buf + 4);
    *(int32_as*)(dst + pitch2) = *(int32_as*)(buf + 8);
    *(int32_as*)(dst + pitch3) = *(int32_as*)(buf + 12);
    dst += pitch * 4;  
    *(int32_as*)dst = *(int32_as*)(buf + 16);
    *(int32_as*)(dst + pitch) = *(int32_as*)(buf + 20);
    *(int32_as*)(dst + pitch2) = *(int32_as*)(buf + 24);
    *(int32_as*)(dst + pitch3) = *(int32_as*)(buf + 28);
#endif
}

static void LF_chroma_ver_bs2_sse4( uint8_t* data, int pitch, int16_t alpha, int16_t beta )
{
    __m128i p2, p1, p0, q0, q1, q2;
    __m128i m0, m1, m2, m3;
    __m128i x0, x1;
    __m128i add2 = _mm_load_si128( (__m128i*)s_Add2x8 );
    const int pitch2 = pitch + pitch;
    const int pitch3 = pitch + pitch2;

    // 读取并转置
    uint8_t* src = data - 4;
    m0 = _mm_loadl_epi64( (__m128i*)(src) );
    x0 = _mm_loadl_epi64( (__m128i*)(src + pitch) );
    m1 = _mm_loadl_epi64( (__m128i*)(src + pitch2) );
    x1 = _mm_loadl_epi64( (__m128i*)(src + pitch3) );
    m0 = _mm_unpacklo_epi8( m0, x0 );
    m1 = _mm_unpacklo_epi8( m1, x1 );
    src += pitch * 4;
    m2 = _mm_loadl_epi64( (__m128i*)(src) );
    x0 = _mm_loadl_epi64( (__m128i*)(src + pitch) );
    m3 = _mm_loadl_epi64( (__m128i*)(src + pitch2) );
    x1 = _mm_loadl_epi64( (__m128i*)(src + pitch3) );
    m2 = _mm_unpacklo_epi8( m2, x0 );
    m3 = _mm_unpacklo_epi8( m3, x1 );
    q0 = _mm_unpackhi_epi16( m0, m1 );
    q1 = _mm_unpackhi_epi16( m2, m3 );
    m0 = _mm_unpacklo_epi16( m0, m1 );
    m2 = _mm_unpacklo_epi16( m2, m3 );
    q2 = _mm_unpackhi_epi32( q0, q1 );
    p1 = _mm_unpackhi_epi32( m0, m2 );
    q0 = _mm_unpacklo_epi32( q0, q1 );
    m0 = _mm_unpacklo_epi32( m0, m2 );
    q1 = _mm_srli_si128( q0, 8 );
    p0 = _mm_srli_si128( p1, 8 );
    p2 = _mm_srli_si128( m0, 8 );

    // 转化为 int16
    q0 = _mm_cvtepu8_epi16( q0 );
    q1 = _mm_cvtepu8_epi16( q1 );
    q2 = _mm_cvtepu8_epi16( q2 );
    p0 = _mm_cvtepu8_epi16( p0 );
    p1 = _mm_cvtepu8_epi16( p1 );
    p2 = _mm_cvtepu8_epi16( p2 );

    // 计算 mask
    m0 = _mm_set1_epi16( alpha );
    m2 = _mm_set1_epi16( beta );
    x0 = _mm_sub_epi16( p0, q0 );
    m3 = _mm_srai_epi16( m0, 2 );
    x0 = _mm_abs_epi16( x0 );           // abs(p0 - q0)
    m3 = _mm_add_epi16( m3, add2 );     // (alpha >> 2) + 2
    m0 = _mm_cmpgt_epi16( m0, x0 );     // abs(p0 - q0) < alpha
    m3 = _mm_cmpgt_epi16( m3, x0 );     // abs(p0 - q0) < (alpha >> 2) + 2
    x0 = _mm_sub_epi16( p1, p0 );
    x1 = _mm_sub_epi16( q1, q0 );
    x0 = _mm_abs_epi16( x0 );           // abs(p1 - p0)
    x1 = _mm_abs_epi16( x1 );           // abs(q1 - q0)
    x0 = _mm_cmpgt_epi16( m2, x0 );     // abs(p1 - p0) < beta
    x1 = _mm_cmpgt_epi16( m2, x1 );     // abs(q1 - q0) < beta
    m0 = _mm_and_si128( m0, x0 );
    m0 = _mm_and_si128( m0, x1 );       // abs(p0 - q0) < alpha && abs(p1 - p0) < beta && abs(q1 - q0) < beta
    if( _mm_testz_si128( m0, m0 ) )
        return;

    p2 = _mm_sub_epi16( p2, p0 );
    q2 = _mm_sub_epi16( q2, q0 );
    p2 = _mm_abs_epi16( p2 );           // abs(p2 - p0)
    q2 = _mm_abs_epi16( q2 );           // abs(q2 - q0)
    m1 = _mm_cmpgt_epi16( m2, p2 );     // abs(p2 - p0) < beta
    m2 = _mm_cmpgt_epi16( m2, q2 );     // abs(q2 - q0) < beta
    m1 = _mm_and_si128( m1, m3 );       // abs(p2 - p0) < beta && abs(p0 - q0) < (alpha >> 2) + 2
    m2 = _mm_and_si128( m2, m3 );       // abs(q2 - q0) < beta && abs(p0 - q0) < (alpha >> 2) + 2

    m3 = _mm_add_epi16( p0, q0 );
    m3 = _mm_add_epi16( m3, add2 );         // p0 + q0 + 2
    x0 = _mm_add_epi16( p1, p0 );
    x1 = _mm_add_epi16( p1, p1 );
    x0 = _mm_add_epi16( x0, m3 );           // p1 + 2*p0 + q0 + 2
    x1 = _mm_add_epi16( x1, m3 );           // 2*p1 + p0 + q0 + 2
    x1 = _mm_blendv_epi8( x1, x0, m1 );     // select p0
    x1 = _mm_srai_epi16( x1, 2 );
    p0 = _mm_blendv_epi8( p0, x1, m0 );
    x0 = _mm_add_epi16( q1, q0 ); 
    x1 = _mm_add_epi16( q1, q1 );
    x0 = _mm_add_epi16( x0, m3 );           // q1 + 2*q0 + p0 + 2
    x1 = _mm_add_epi16( x1, m3 );           // 2*q1 + q0 + p0 + 2
    x1 = _mm_blendv_epi8( x1, x0, m2 );     // select q0
    x1 = _mm_srai_epi16( x1, 2 );
    q0 = _mm_blendv_epi8( q0, x1, m0 );

    p0 = _mm_packus_epi16( p0, p0 ); 
    q0 = _mm_packus_epi16( q0, q0 );
    p0 = _mm_unpacklo_epi8( p0, q0 );

    uint8_t* dst = data - 1;
    alignas(16) uint8_t buf[16];
    _mm_store_si128( (__m128i*)buf, p0 );
    *(uint16_as*)dst = *(uint16_as*)buf;
    *(uint16_as*)(dst + pitch) = *(uint16_as*)(buf + 2);
    *(uint16_as*)(dst + pitch2) = *(uint16_as*)(buf + 4);
    *(uint16_as*)(dst + pitch3) = *(uint16_as*)(buf + 6);
    dst += pitch * 4;
    *(uint16_as*)dst = *(uint16_as*)(buf + 8);
    *(uint16_as*)(dst + pitch) = *(uint16_as*)(buf + 10);
    *(uint16_as*)(dst + pitch2) = *(uint16_as*)(buf + 12);
    *(uint16_as*)(dst + pitch3) = *(uint16_as*)(buf + 14);
}

static void LF_luma_hor_bs2_sse4( uint8_t* data, int pitch, int16_t alpha, int16_t beta )
{
    __m128i p1, p0, q0, q1;
    __m128i m0, m1, m2, m3;
    __m128i x0, x1;
    __m128i add2 = _mm_load_si128( (__m128i*)s_Add2x8 );

    p1 = loadzx_epu8_epi16( data - pitch*2 );
    p0 = loadzx_epu8_epi16( data - pitch);
    q0 = loadzx_epu8_epi16( data );
    q1 = loadzx_epu8_epi16( data + pitch );
    m0 = _mm_set1_epi16( alpha );
    m2 = _mm_set1_epi16( beta );

    x0 = _mm_sub_epi16( p0, q0 );
    m3 = _mm_srai_epi16( m0, 2 );
    x0 = _mm_abs_epi16( x0 );           // abs(p0 - q0)
    m3 = _mm_add_epi16( m3, add2 );     // (alpha >> 2) + 2
    m0 = _mm_cmpgt_epi16( m0, x0 );     // abs(p0 - q0) < alpha
    m3 = _mm_cmpgt_epi16( m3, x0 );     // abs(p0 - q0) < (alpha >> 2) + 2
    x0 = _mm_sub_epi16( p1, p0 );
    x1 = _mm_sub_epi16( q1, q0 );
    x0 = _mm_abs_epi16( x0 );           // abs(p1 - p0)
    x1 = _mm_abs_epi16( x1 );           // abs(q1 - q0)
    x0 = _mm_cmpgt_epi16( m2, x0 );     // abs(p1 - p0) < beta
    x1 = _mm_cmpgt_epi16( m2, x1 );     // abs(q1 - q0) < beta
    m0 = _mm_and_si128( m0, x0 );
    m0 = _mm_and_si128( m0, x1 );       // abs(p0 - q0) < alpha && abs(p1 - p0) < beta && abs(q1 - q0) < beta
    if( _mm_testz_si128( m0, m0 ) )
        return;

    x0 = loadzx_epu8_epi16( data - pitch*3 );   // p2
    x1 = loadzx_epu8_epi16( data + pitch*2 );   // q2
    x0 = _mm_sub_epi16( x0, p0 );
    x1 = _mm_sub_epi16( x1, q0 );
    x0 = _mm_abs_epi16( x0 );                   // abs(p2 - p0)
    x1 = _mm_abs_epi16( x1 );                   // abs(q2 - q0)
    m1 = _mm_cmpgt_epi16( m2, x0 );             // abs(p2 - p0) < beta
    m2 = _mm_cmpgt_epi16( m2, x1 );             // abs(q2 - q0) < beta
    m1 = _mm_and_si128( m1, m3 );               // abs(p2 - p0) < beta && abs(p0 - q0) < (alpha >> 2) + 2
    m2 = _mm_and_si128( m2, m3 );               // abs(q2 - q0) < beta && abs(p0 - q0) < (alpha >> 2) + 2

    m3 = _mm_add_epi16( p0, q0 );
    m3 = _mm_add_epi16( m3, add2 );         // p0 + q0 + 2
    x0 = _mm_add_epi16( p1, p0 );
    x1 = _mm_add_epi16( p1, p1 );
    x0 = _mm_add_epi16( x0, m3 );           // p1 + 2*p0 + q0 + 2
    x1 = _mm_add_epi16( x1, m3 );           // 2*p1 + p0 + q0 + 2
    x0 = _mm_srai_epi16( x0, 2 );
    x1 = _mm_srai_epi16( x1, 2 );
    x0 = _mm_blendv_epi8( x1, x0, m1 );     // select p0
    m1 = _mm_and_si128( m1, m0 );
    p0 = _mm_blendv_epi8( p0, x0, m0 );
    p1 = _mm_blendv_epi8( p1, x1, m1 );
    p0 = _mm_packus_epi16( p0, p0 );
    p1 = _mm_packus_epi16( p1, p1 );
    _mm_storel_epi64( (__m128i*)(data - pitch), p0 );       // save p0
    _mm_storel_epi64( (__m128i*)(data - pitch*2), p1 );     // save p1  
    x0 = _mm_add_epi16( q1, q0 ); 
    x1 = _mm_add_epi16( q1, q1 );
    x0 = _mm_add_epi16( x0, m3 );           // q1 + 2*q0 + p0 + 2
    x1 = _mm_add_epi16( x1, m3 );           // 2*q1 + q0 + p0 + 2
    x0 = _mm_srai_epi16( x0, 2 );
    x1 = _mm_srai_epi16( x1, 2 );
    x0 = _mm_blendv_epi8( x1, x0, m2 );     // select q0
    m2 = _mm_and_si128( m2, m0 );
    q0 = _mm_blendv_epi8( q0, x0, m0 );
    q1 = _mm_blendv_epi8( q1, x1, m2 );
    q0 = _mm_packus_epi16( q0, q0 );
    q1 = _mm_packus_epi16( q1, q1 );
    _mm_storel_epi64( (__m128i*)(data), q0 );           // save q0
    _mm_storel_epi64( (__m128i*)(data + pitch), q1 );   // save q1  
}

static void LF_chroma_hor_bs2_sse4( uint8_t* data, int pitch, int16_t alpha, int16_t beta )
{
    __m128i p1, p0, q0, q1;
    __m128i m0, m1, m2, m3;
    __m128i x0, x1;
    __m128i add2 = _mm_load_si128( (__m128i*)s_Add2x8 );

    p1 = loadzx_epu8_epi16( data - pitch*2 );
    p0 = loadzx_epu8_epi16( data - pitch);
    q0 = loadzx_epu8_epi16( data );
    q1 = loadzx_epu8_epi16( data + pitch );
    m0 = _mm_set1_epi16( alpha );
    m2 = _mm_set1_epi16( beta );

    x0 = _mm_sub_epi16( p0, q0 );
    m3 = _mm_srai_epi16( m0, 2 );
    x0 = _mm_abs_epi16( x0 );                       // abs(p0 - q0)
    m3 = _mm_add_epi16( m3, add2 );                 // (alpha >> 2) + 2
    m0 = _mm_cmpgt_epi16( m0, x0 );                 // abs(p0 - q0) < alpha
    m3 = _mm_cmpgt_epi16( m3, x0 );                 // abs(p0 - q0) < (alpha >> 2) + 2
    x0 = _mm_abs_epi16( _mm_sub_epi16( p1, p0 ) );  // abs(p1 - p0)
    x1 = _mm_abs_epi16( _mm_sub_epi16( q1, q0 ) );  // abs(q1 - q0)
    x0 = _mm_cmpgt_epi16( m2, x0 );                 // abs(p1 - p0) < beta
    x1 = _mm_cmpgt_epi16( m2, x1 );                 // abs(q1 - q0) < beta
    m0 = _mm_and_si128( m0, x0 );
    m0 = _mm_and_si128( m0, x1 );                   // abs(p0 - q0) < alpha && abs(p1 - p0) < beta && abs(q1 - q0) < beta
    if( _mm_testz_si128( m0, m0 ) )
        return;

    x0 = loadzx_epu8_epi16( data - pitch*3 );       // p2
    x1 = loadzx_epu8_epi16( data + pitch*2 );       // q2
    x0 = _mm_abs_epi16( _mm_sub_epi16( x0, p0 ) );  // abs(p2 - p0)
    x1 = _mm_abs_epi16( _mm_sub_epi16( x1, q0 ) );  // abs(q2 - q0)
    m1 = _mm_cmpgt_epi16( m2, x0 );                 // abs(p2 - p0) < beta
    m2 = _mm_cmpgt_epi16( m2, x1 );                 // abs(q2 - q0) < beta
    m1 = _mm_and_si128( m1, m3 );                   // abs(p2 - p0) < beta && abs(p0 - q0) < (alpha >> 2) + 2
    m2 = _mm_and_si128( m2, m3 );                   // abs(q2 - q0) < beta && abs(p0 - q0) < (alpha >> 2) + 2

    m3 = _mm_add_epi16( p0, q0 );
    m3 = _mm_add_epi16( m3, add2 );         // p0 + q0 + 2
    x0 = _mm_add_epi16( p1, p0 );
    p1 = _mm_add_epi16( p1, p1 );
    x0 = _mm_add_epi16( x0, m3 );           // p1 + 2*p0 + q0 + 2
    p1 = _mm_add_epi16( p1, m3 );           // 2*p1 + p0 + q0 + 2 
    p1 = _mm_blendv_epi8( p1, x0, m1 );
    p1 = _mm_srai_epi16( p1, 2 );
    p0 = _mm_blendv_epi8( p0, p1, m0 );
    x0 = _mm_add_epi16( q1, q0 ); 
    q1 = _mm_add_epi16( q1, q1 );
    x0 = _mm_add_epi16( x0, m3 );           // q1 + 2*q0 + p0 + 2
    q1 = _mm_add_epi16( q1, m3 );           // 2*q1 + q0 + p0 + 2
    q1 = _mm_blendv_epi8( q1, x0, m2 );
    q1 = _mm_srai_epi16( q1, 2 );
    q0 = _mm_blendv_epi8( q0, q1, m0 );

    p0 = _mm_packus_epi16( p0, p0 );
    q0 = _mm_packus_epi16( q0, q0 );
    _mm_storel_epi64( (__m128i*)(data - pitch), p0 );   // save p0
    _mm_storel_epi64( (__m128i*)data, q0 );             // save q0
}

static void LF_luma_ver_bs1_sse4( uint8_t* data, int pitch, int16_t alpha, int16_t beta, int16_t tc )
{
    __m128i p2, p1, p0, q0, q1, q2, p20, q20;
    __m128i m0, m1, x0, x1, c0, c1;
    __m128i add4 = _mm_load_si128( (__m128i*)s_Add4x8 );
    const int pitch2 = pitch + pitch;
    const int pitch3 = pitch + pitch2;

    // 读取并转置
    uint8_t* src = data - 4;
    m0 = _mm_loadl_epi64( (__m128i*)(src) );
    x0 = _mm_loadl_epi64( (__m128i*)(src + pitch) );
    m1 = _mm_loadl_epi64( (__m128i*)(src + pitch2) );
    x1 = _mm_loadl_epi64( (__m128i*)(src + pitch3) );
    m0 = _mm_unpacklo_epi8( m0, x0 );
    m1 = _mm_unpacklo_epi8( m1, x1 );
    src += pitch * 4;
    q2 = _mm_loadl_epi64( (__m128i*)(src) );
    x0 = _mm_loadl_epi64( (__m128i*)(src + pitch) );
    p2 = _mm_loadl_epi64( (__m128i*)(src + pitch2) );
    x1 = _mm_loadl_epi64( (__m128i*)(src + pitch3) );
    q2 = _mm_unpacklo_epi8( q2, x0 );
    p2 = _mm_unpacklo_epi8( p2, x1 );
    q0 = _mm_unpackhi_epi16( m0, m1 );
    m0 = _mm_unpacklo_epi16( m0, m1 );
    q1 = _mm_unpackhi_epi16( q2, p2 );   
    q2 = _mm_unpacklo_epi16( q2, p2 );
    p1 = _mm_unpackhi_epi32( m0, q2 );
    m0 = _mm_unpacklo_epi32( m0, q2 );
    q2 = _mm_unpackhi_epi32( q0, q1 );
    q0 = _mm_unpacklo_epi32( q0, q1 );
    q1 = _mm_srli_si128( q0, 8 );
    p0 = _mm_srli_si128( p1, 8 );
    p2 = _mm_srli_si128( m0, 8 );

    // 转化为 int16
    q0 = _mm_cvtepu8_epi16( q0 );
    q1 = _mm_cvtepu8_epi16( q1 );
    q2 = _mm_cvtepu8_epi16( q2 );
    p0 = _mm_cvtepu8_epi16( p0 );
    p1 = _mm_cvtepu8_epi16( p1 );
    p2 = _mm_cvtepu8_epi16( p2 );

    // 计算 mask
    m0 = _mm_set1_epi16( alpha );
    m1 = _mm_set1_epi16( beta );
    x0 = _mm_abs_epi16( _mm_sub_epi16( p0, q0 ) );
    m0 = _mm_cmpgt_epi16( m0, x0 );     // abs(p0 - q0) < alpha
    x0 = _mm_sub_epi16( p1, p0 );
    x1 = _mm_sub_epi16( q1, q0 );
    x0 = _mm_abs_epi16( x0 );           // abs(p1 - p0)
    x1 = _mm_abs_epi16( x1 );           // abs(q1 - q0)
    x0 = _mm_cmpgt_epi16( m1, x0 );     // abs(p1 - p0) < beta
    x1 = _mm_cmpgt_epi16( m1, x1 );     // abs(q1 - q0) < beta
    m0 = _mm_and_si128( m0, x0 );
    m0 = _mm_and_si128( m0, x1 );       // abs(p0 - q0) < alpha && abs(p1 - p0) < beta && abs(q1 - q0) < beta
    if( _mm_testz_si128( m0, m0 ) )
        return;

    p20 = _mm_sub_epi16( p2, p0 );
    q20 = _mm_sub_epi16( q2, q0 );
    p20 = _mm_abs_epi16( p20 );         // abs(p2 - p0)
    q20 = _mm_abs_epi16( q20 );         // abs(q2 - q0)

    c0 = _mm_set1_epi16( tc );
    c0 = _mm_and_si128( c0, m0 );       // tc     
    c1 = _mm_sign_epi16( c0, m0 );      // -tc

    x0 = _mm_sub_epi16( q0, p0 );       // q0 - p0
    x1 = _mm_sub_epi16( p1, q1 );       // p1 - q1
    x1 = _mm_add_epi16( x1, x0 );       // (p1 - q1) + (q0 - p0)
    x0 = _mm_add_epi16( x0, x0 );       // (q0 - p0) * 2
    x1 = _mm_add_epi16( x1, x0 );       // (q0 - p0) * 3 + (p1 - q1)
    x1 = _mm_add_epi16( x1, add4 );     // (q0 - p0) * 3 + (p1 - q1) + 4
    x1 = _mm_srai_epi16( x1, 3 );   
    x1 = _mm_min_epi16( x1, c0 );
    x1 = _mm_max_epi16( x1, c1 );       // delta
    p0 = _mm_add_epi16( p0, x1 );       // + delta
    q0 = _mm_sub_epi16( q0, x1 );       // - delta
    p0 = _mm_packus_epi16( p0, p0 );    // clip 0~255
    q0 = _mm_packus_epi16( q0, q0 );    // clip 0~255
    p0 = _mm_cvtepu8_epi16( p0 );       
    q0 = _mm_cvtepu8_epi16( q0 );

    m0 = _mm_cmpgt_epi16( m1, p20 );    // abs(p2 - p0) < beta
    x1 = _mm_sub_epi16( p0, p1 );       // P0 - p1
    p2 = _mm_sub_epi16( p2, q0 );       // p2 - Q0
    p2 = _mm_add_epi16( p2, x1 );       // (P0 - p1) + (p2 - Q0)
    x1 = _mm_add_epi16( x1, x1 );       // (P0 - p1) * 2
    x1 = _mm_add_epi16( x1, p2 );
    x1 = _mm_add_epi16( x1, add4 );     // (P0 - p1) * 3 + (p2 - Q0) + 4
    x1 = _mm_srai_epi16( x1, 3 );
    x1 = _mm_min_epi16( x1, c0 );
    x1 = _mm_max_epi16( x1, c1 );
    x1 = _mm_and_si128( x1, m0 );
    p1 = _mm_add_epi16( p1, x1 );       // p1 + delta

    m1 = _mm_cmpgt_epi16( m1, q20 );    // abs(q2 - q0) < beta
    x1 = _mm_sub_epi16( q1, q0 );       // q1 - Q0
    x0 = _mm_sub_epi16( p0, q2 );       // P0 - q2
    x0 = _mm_add_epi16( x0, x1 );       // (q1 - Q0) + (q2 - P0)
    x1 = _mm_add_epi16( x1, x1 );       // (q1 - Q0) * 2
    x1 = _mm_add_epi16( x1, x0 );
    x1 = _mm_add_epi16( x1, add4 );     // (q1 - Q0) * 3 + (P0 - q2) + 4
    x1 = _mm_srai_epi16( x1, 3 );
    x1 = _mm_min_epi16( x1, c0 );
    x1 = _mm_max_epi16( x1, c1 );
    x1 = _mm_and_si128( x1, m1 );
    q1 = _mm_sub_epi16( q1, x1 );       // q1 - delta

    // 转化为 uint8
    p1 = _mm_packus_epi16( p1, p1 );
    p0 = _mm_packus_epi16( p0, p0 );
    q0 = _mm_packus_epi16( q0, q0 );
    q1 = _mm_packus_epi16( q1, q1 );

    // 转置
    p1 = _mm_unpacklo_epi8( p1, p0 );
    q0 = _mm_unpacklo_epi8( q0, q1 );
    p0 = _mm_unpackhi_epi16( p1, q0 );
    p1 = _mm_unpacklo_epi16( p1, q0 );

    // 输出
    uint8_t* dst = data - 2;
#ifdef _MSC_VER
    *(int32_as*)dst = _mm_cvtsi128_si32( p1 );
    p1 = _mm_srli_si128( p1, 4 );
    *(int32_as*)(dst + pitch) = _mm_cvtsi128_si32( p1 );
    p1 = _mm_srli_si128( p1, 4 );
    *(int32_as*)(dst + pitch2) = _mm_cvtsi128_si32( p1 );
    p1 = _mm_srli_si128( p1, 4 );
    *(int32_as*)(dst + pitch3) = _mm_cvtsi128_si32( p1 );
    dst += pitch * 4;
    *(int32_as*)dst = _mm_cvtsi128_si32( p0 );
    p0 = _mm_srli_si128( p0, 4 );
    *(int32_as*)(dst + pitch) = _mm_cvtsi128_si32( p0 );
    p0 = _mm_srli_si128( p0, 4 );
    *(int32_as*)(dst + pitch2) = _mm_cvtsi128_si32( p0 );
    p0 = _mm_srli_si128( p0, 4 );
    *(int32_as*)(dst + pitch3) = _mm_cvtsi128_si32( p0 );
#else
    alignas(16) uint8_t buf[32];  
    _mm_store_si128( (__m128i*)buf, p1 );
    _mm_store_si128( (__m128i*)(buf + 16), p0 );
    *(int32_as*)dst = *(int32_as*)buf;
    *(int32_as*)(dst + pitch) = *(int32_as*)(buf + 4);
    *(int32_as*)(dst + pitch2) = *(int32_as*)(buf + 8);
    *(int32_as*)(dst + pitch3) = *(int32_as*)(buf + 12);
    dst += pitch * 4;  
    *(int32_as*)dst = *(int32_as*)(buf + 16);
    *(int32_as*)(dst + pitch) = *(int32_as*)(buf + 20);
    *(int32_as*)(dst + pitch2) = *(int32_as*)(buf + 24);
    *(int32_as*)(dst + pitch3) = *(int32_as*)(buf + 28);
#endif
}

static void LF_chroma_ver_bs1_sse4( uint8_t* data, int pitch, int16_t alpha, int16_t beta, int16_t tc[2] )
{
    __m128i p1, p0, q0, q1;
    __m128i m0, m1, x0, x1;
    const int pitch2 = pitch + pitch;
    const int pitch3 = pitch + pitch2;

    // 读取并转置
    uint8_t* src = data - 2;
    p1 = _mm_cvtsi32_si128( *(int32_t*)src );
    x0 = _mm_cvtsi32_si128( *(int32_t*)(src + pitch) );
    p0 = _mm_cvtsi32_si128( *(int32_t*)(src + pitch2) );
    x1 = _mm_cvtsi32_si128( *(int32_t*)(src + pitch3) );
    p1 = _mm_unpacklo_epi8( p1, x0 );
    p0 = _mm_unpacklo_epi8( p0, x1 );
    p1 = _mm_unpacklo_epi16( p1, p0 );
    src += pitch * 4;
    m0 = _mm_cvtsi32_si128( *(int32_t*)src );
    x0 = _mm_cvtsi32_si128( *(int32_t*)(src + pitch) );
    m1 = _mm_cvtsi32_si128( *(int32_t*)(src + pitch2) );
    x1 = _mm_cvtsi32_si128( *(int32_t*)(src + pitch3) );
    m0 = _mm_unpacklo_epi8( m0, x0 );
    m1 = _mm_unpacklo_epi8( m1, x1 );
    m0 = _mm_unpacklo_epi16( m0, m1 );
    q0 = _mm_unpackhi_epi32( p1, m0 );
    p1 = _mm_unpacklo_epi32( p1, m0 );
    q1 = _mm_srli_si128( q0, 8 );
    p0 = _mm_srli_si128( p1, 8 );

    // 转化为 int16
    q0 = _mm_cvtepu8_epi16( q0 );
    q1 = _mm_cvtepu8_epi16( q1 );
    p0 = _mm_cvtepu8_epi16( p0 );
    p1 = _mm_cvtepu8_epi16( p1 );

    // 计算 mask
    m0 = _mm_set1_epi16( alpha );
    m1 = _mm_set1_epi16( beta );
    x0 = _mm_abs_epi16( _mm_sub_epi16( p0, q0 ) );
    m0 = _mm_cmpgt_epi16( m0, x0 );     // abs(p0 - q0) < alpha
    x0 = _mm_sub_epi16( p1, p0 );
    x1 = _mm_sub_epi16( q1, q0 );
    x0 = _mm_abs_epi16( x0 );           // abs(p1 - p0)
    x1 = _mm_abs_epi16( x1 );           // abs(q1 - q0)
    x0 = _mm_cmpgt_epi16( m1, x0 );     // abs(p1 - p0) < beta
    m1 = _mm_cmpgt_epi16( m1, x1 );     // abs(q1 - q0) < beta
    m0 = _mm_and_si128( m0, x0 );
    m0 = _mm_and_si128( m0, m1 );       // abs(p0 - q0) < alpha && abs(p1 - p0) < beta && abs(q1 - q0) < beta
    if( _mm_testz_si128( m0, m0 ) )
        return;

    x0 = _mm_sub_epi16( q0, p0 );
    p1 = _mm_sub_epi16( p1, q1 );
    x0 = _mm_sub_epi16( x0, m0 );       // (q0 - p0) + 1
    p1 = _mm_sub_epi16( p1, m0 );       // (p1 - q1) + 1
    p1 = _mm_add_epi16( p1, x0 );       // (q0 - p0) + (p1 - q1) + 2
    x0 = _mm_add_epi16( x0, x0 );       // (q0 - p0) * 2 + 2
    p1 = _mm_add_epi16( p1, x0 );       // (q0 - p0) * 3 + (p1 - q1) + 4
    p1 = _mm_srai_epi16( p1, 3 );
    x0 = _mm_cvtsi32_si128( tc[0] );
    x1 = _mm_cvtsi32_si128( tc[1] );
    x0 = _mm_unpacklo_epi16( x0, x0 ); 
    x1 = _mm_unpacklo_epi16( x1, x1 );
    x0 = _mm_unpacklo_epi32( x0, x0 );
    x1 = _mm_unpacklo_epi32( x1, x1 );
    x0 = _mm_unpacklo_epi64( x0, x1 );  // tc
    x1 = _mm_sign_epi16( x0, m0 );      // -tc
    p1 = _mm_min_epi16( p1, x0 );
    p1 = _mm_max_epi16( p1, x1 );       // delta
    p1 = _mm_and_si128( p1, m0 );
    p0 = _mm_add_epi16( p0, p1 );       // + delta
    q0 = _mm_sub_epi16( q0, p1 );       // - delta
    p0 = _mm_packus_epi16( p0, p0 );
    q0 = _mm_packus_epi16( q0, q0 );
    p0 = _mm_unpacklo_epi8( p0, q0 );

    alignas(16) uint8_t buf[16];
    _mm_store_si128( (__m128i*)buf, p0 );
    uint8_t* dst = data - 1;
    *(uint16_as*)dst = *(uint16_as*)buf;
    *(uint16_as*)(dst + pitch) = *(uint16_as*)(buf + 2);
    *(uint16_as*)(dst + pitch2) = *(uint16_as*)(buf + 4);
    *(uint16_as*)(dst + pitch3) = *(uint16_as*)(buf + 6);
    dst += pitch * 4;
    *(uint16_as*)dst = *(uint16_as*)(buf + 8);
    *(uint16_as*)(dst + pitch) = *(uint16_as*)(buf + 10);
    *(uint16_as*)(dst + pitch2) = *(uint16_as*)(buf + 12);
    *(uint16_as*)(dst + pitch3) = *(uint16_as*)(buf + 14);
}

static void LF_luma_hor_bs1_sse4( uint8_t* data, int pitch, int16_t alpha, int16_t beta, int16_t tc )
{
    __m128i p1, p0, q0, q1, p20, q20;
    __m128i m0, m1, x0, x1, c0, c1;
    __m128i add4 = _mm_load_si128( (__m128i*)s_Add4x8 );

    p1 = loadzx_epu8_epi16( data - pitch*2 );
    p0 = loadzx_epu8_epi16( data - pitch);
    q0 = loadzx_epu8_epi16( data );
    q1 = loadzx_epu8_epi16( data + pitch );

    // 计算 mask
    m0 = _mm_set1_epi16( alpha );
    m1 = _mm_set1_epi16( beta );
    x0 = _mm_abs_epi16( _mm_sub_epi16( p0, q0 ) );
    m0 = _mm_cmpgt_epi16( m0, x0 );     // abs(p0 - q0) < alpha
    x0 = _mm_sub_epi16( p1, p0 );
    x1 = _mm_sub_epi16( q1, q0 );
    x0 = _mm_abs_epi16( x0 );           // abs(p1 - p0)
    x1 = _mm_abs_epi16( x1 );           // abs(q1 - q0)
    x0 = _mm_cmpgt_epi16( m1, x0 );     // abs(p1 - p0) < beta
    x1 = _mm_cmpgt_epi16( m1, x1 );     // abs(q1 - q0) < beta
    m0 = _mm_and_si128( m0, x0 );
    m0 = _mm_and_si128( m0, x1 );       // abs(p0 - q0) < alpha && abs(p1 - p0) < beta && abs(q1 - q0) < beta
    if( _mm_testz_si128( m0, m0 ) )
        return;

    c0 = _mm_set1_epi16( tc );
    c0 = _mm_and_si128( c0, m0 );       // tc     
    c1 = _mm_sign_epi16( c0, m0 );      // -tc
    p20 = p0;
    q20 = q0;

    x0 = _mm_sub_epi16( q0, p0 );       // q0 - p0
    x1 = _mm_sub_epi16( p1, q1 );       // p1 - q1
    x1 = _mm_add_epi16( x1, x0 );       // (p1 - q1) + (q0 - p0)
    x0 = _mm_add_epi16( x0, x0 );       // (q0 - p0) * 2
    x1 = _mm_add_epi16( x1, x0 );       // (q0 - p0) * 3 + (p1 - q1)
    x1 = _mm_add_epi16( x1, add4 );     // (q0 - p0) * 3 + (p1 - q1) + 4
    x1 = _mm_srai_epi16( x1, 3 );   
    x1 = _mm_min_epi16( x1, c0 );
    x1 = _mm_max_epi16( x1, c1 );       // delta
    p0 = _mm_add_epi16( p0, x1 );       // + delta
    q0 = _mm_sub_epi16( q0, x1 );       // - delta
    p0 = _mm_packus_epi16( p0, p0 );    // clip 0~255
    q0 = _mm_packus_epi16( q0, q0 );    // clip 0~255
    p0 = _mm_cvtepu8_epi16( p0 );       
    q0 = _mm_cvtepu8_epi16( q0 );

    x0 = loadzx_epu8_epi16( data - pitch*3 );   // p2
    p20 = _mm_sub_epi16( p20, x0 );
    p20 = _mm_abs_epi16( p20 );
    m0 = _mm_cmpgt_epi16( m1, p20 );            // abs(p2 - p0) < beta
    x1 = _mm_sub_epi16( p0, p1 );               // P0 - p1
    x0 = _mm_sub_epi16( x0, q0 );               // p2 - Q0
    x0 = _mm_add_epi16( x0, x1 );               // (P0 - p1) + (p2 - Q0)
    x1 = _mm_add_epi16( x1, x1 );               // (P0 - p1) * 2
    x1 = _mm_add_epi16( x1, x0 );
    x1 = _mm_add_epi16( x1, add4 );             // (P0 - p1) * 3 + (p2 - Q0) + 4
    x1 = _mm_srai_epi16( x1, 3 );
    x1 = _mm_min_epi16( x1, c0 );
    x1 = _mm_max_epi16( x1, c1 );
    x1 = _mm_and_si128( x1, m0 );
    p1 = _mm_add_epi16( p1, x1 );               // p1 + delta

    x0 = loadzx_epu8_epi16( data + pitch*2 );   // q2
    q20 = _mm_sub_epi16( q20, x0 );
    q20 = _mm_abs_epi16( q20 );
    m1 = _mm_cmpgt_epi16( m1, q20 );            // abs(q2 - q0) < beta
    x1 = _mm_sub_epi16( q1, q0 );               // q1 - Q0
    x0 = _mm_sub_epi16( p0, x0 );               // P0 - q2
    x0 = _mm_add_epi16( x0, x1 );               // (P0 - q2) + (q1 - Q0)
    x1 = _mm_add_epi16( x1, x1 );               // (q1 - Q0) * 2
    x1 = _mm_add_epi16( x1, x0 );
    x1 = _mm_add_epi16( x1, add4 );             // (q1 - Q0) * 3 + (P0 - q2) + 4
    x1 = _mm_srai_epi16( x1, 3 );
    x1 = _mm_min_epi16( x1, c0 );
    x1 = _mm_max_epi16( x1, c1 );
    x1 = _mm_and_si128( x1, m1 );
    q1 = _mm_sub_epi16( q1, x1 );               // q1 - delta

    p1 = _mm_packus_epi16( p1, p1 );
    p0 = _mm_packus_epi16( p0, p0 );
    q0 = _mm_packus_epi16( q0, q0 );
    q1 = _mm_packus_epi16( q1, q1 );
    _mm_storel_epi64( (__m128i*)(data - pitch*2), p1 );     // save p1
    _mm_storel_epi64( (__m128i*)(data - pitch), p0 );       // save p0
    _mm_storel_epi64( (__m128i*)(data), q0 );               // save q0
    _mm_storel_epi64( (__m128i*)(data + pitch), q1 );       // save q1 
}

// 色度分量水平滤波, 8 个像素, 
static void LF_chroma_hor_bs1_sse4( uint8_t* data, int pitch, int16_t alpha, int16_t beta, int16_t tc[2] )
{
    __m128i p1, p0, q0, q1;
    __m128i m0, m1, x0, x1;

    p1 = loadzx_epu8_epi16( data - pitch*2 );
    p0 = loadzx_epu8_epi16( data - pitch);
    q0 = loadzx_epu8_epi16( data );
    q1 = loadzx_epu8_epi16( data + pitch );

    // 计算 mask
    m0 = _mm_set1_epi16( alpha );
    m1 = _mm_set1_epi16( beta );
    x0 = _mm_abs_epi16( _mm_sub_epi16( p0, q0 ) );
    m0 = _mm_cmpgt_epi16( m0, x0 );     // abs(p0 - q0) < alpha
    x0 = _mm_sub_epi16( p1, p0 );
    x1 = _mm_sub_epi16( q1, q0 );
    x0 = _mm_abs_epi16( x0 );           // abs(p1 - p0)
    x1 = _mm_abs_epi16( x1 );           // abs(q1 - q0)
    x0 = _mm_cmpgt_epi16( m1, x0 );     // abs(p1 - p0) < beta
    m1 = _mm_cmpgt_epi16( m1, x1 );     // abs(q1 - q0) < beta
    m0 = _mm_and_si128( m0, x0 );
    m0 = _mm_and_si128( m0, m1 );       // abs(p0 - q0) < alpha && abs(p1 - p0) < beta && abs(q1 - q0) < beta
    if( _mm_testz_si128( m0, m0 ) )
        return;

    x0 = _mm_sub_epi16( q0, p0 );       // q0 - p0
    p1 = _mm_sub_epi16( p1, q1 );       // p1 - q1
    x0 = _mm_sub_epi16( x0, m0 );       // (q0 - p0) + 1
    p1 = _mm_sub_epi16( p1, m0 );       // (p1 - q1) + 1
    p1 = _mm_add_epi16( p1, x0 );       // (p1 - q1) + (q0 - p0) + 2
    x0 = _mm_add_epi16( x0, x0 );       // (q0 - p0) * 2 + 2
    p1 = _mm_add_epi16( p1, x0 );       // (q0 - p0) * 3 + (p1 - q1) + 4
    p1 = _mm_srai_epi16( p1, 3 );
    x0 = _mm_cvtsi32_si128( tc[0] );
    x1 = _mm_cvtsi32_si128( tc[1] );
    x0 = _mm_unpacklo_epi16( x0, x0 ); 
    x1 = _mm_unpacklo_epi16( x1, x1 );
    x0 = _mm_unpacklo_epi32( x0, x0 );
    x1 = _mm_unpacklo_epi32( x1, x1 );
    x0 = _mm_unpacklo_epi64( x0, x1 );  // tc
    x1 = _mm_sign_epi16( x0, m0 );      // -tc
    p1 = _mm_min_epi16( p1, x0 );
    p1 = _mm_max_epi16( p1, x1 );       // delta
    p1 = _mm_and_si128( p1, m0 );
    p0 = _mm_add_epi16( p0, p1 );       // + delta
    q0 = _mm_sub_epi16( q0, p1 );       // - delta
    p0 = _mm_packus_epi16( p0, p0 );
    q0 = _mm_packus_epi16( q0, q0 );
    _mm_storel_epi64( (__m128i*)(data - pitch), p0 );   // save P0
    _mm_storel_epi64( (__m128i*)data, q0 );             // save Q0
}

//======================================================================================================================
static const uint8_t s_AlphaTab[64+16] = 
{
    0,  0,  0,  0,  0,  0,  1,  1,  1,  1,  1,  2,  2,  2,  3,  3,
    4,  4,  5,  5,  6,  7,  8,  9,  10, 11, 12, 13, 15, 16, 18, 20,
    22, 24, 26, 28, 30, 33, 33, 35, 35, 36, 37, 37, 39, 39, 42, 44,
    46, 48, 50, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
};

static const uint8_t s_BetaTab[64+16] = 
{
    0,  0,  0,  0,  0,  0,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,
    2,  2,  3,  3,  3,  3,  4,  4,  4,  4,  5,  5,  5,  5,  6,  6,
    6,  7,  7,  7,  8,  8,  8,  9,  9,  10, 10, 11, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 23, 24, 24, 25, 25, 26, 27,
    27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 
};

static const uint8_t s_TcTab[64+16] = 
{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2,
    2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4,
    5, 5, 5, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
};

// 针对 I 帧
void loop_filterI_sse4( FrmDecContext* ctx, int my )
{
    const MbContext* mbCtx = ctx->topMbBuf[(my & 1)^1] + 1;
    const int mbCnt = ctx->mbColCnt;
    const int alphaOffset = ctx->picHdr.alpha_c_offset;
    const int betaOffset = ctx->picHdr.beta_offset;

    // Luma
    int pitch = ctx->picPitch[0];  
    uint8_t* dst = ctx->picPlane[0] + my * pitch * 16;
    for( int i = 0; i < mbCnt; i++ )
    {
        const int curQp = mbCtx[i].curQp;

        // 垂直边界 1
        int avQp = (mbCtx[i].leftQp + curQp + 1) >> 1;
        int idxA = avQp + alphaOffset;
        int idxB = avQp + betaOffset;
        if( idxA >= 6 && idxB >= 6 )
        {
            int16_t alpha = s_AlphaTab[idxA];
            int16_t beta = s_BetaTab[idxB];
            LF_luma_ver_bs2_sse4( dst,           pitch, alpha, beta );
            LF_luma_ver_bs2_sse4( dst + 8*pitch, pitch, alpha, beta );
        }

        // 垂直边界 2
        idxA = curQp + alphaOffset;
        idxB = curQp + betaOffset;
        if( idxA >= 6 && idxB >= 6 )
        {
            int16_t alpha = s_AlphaTab[idxA];
            int16_t beta = s_BetaTab[idxB];
            LF_luma_ver_bs2_sse4( dst + 8,           pitch, alpha, beta );
            LF_luma_ver_bs2_sse4( dst + 8 + 8*pitch, pitch, alpha, beta );
        }

        // 水平边界 1
        avQp = (mbCtx[i].topQp + curQp + 1) >> 1;
        idxA = avQp + alphaOffset;
        idxB = avQp + betaOffset;
        if( idxA >= 6 && idxB >= 6 )
        {
            int16_t alpha = s_AlphaTab[idxA];
            int16_t beta = s_BetaTab[idxB];
            LF_luma_hor_bs2_sse4( dst,     pitch, alpha, beta );
            LF_luma_hor_bs2_sse4( dst + 8, pitch, alpha, beta );
        }

        // 水平边界 2
        idxA = curQp + alphaOffset;
        idxB = curQp + betaOffset;
        if( idxA >= 6 && idxB >= 6 )
        {
            int16_t alpha = s_AlphaTab[idxA];
            int16_t beta = s_BetaTab[idxB];
            LF_luma_hor_bs2_sse4( dst + 8*pitch,     pitch, alpha, beta );
            LF_luma_hor_bs2_sse4( dst + 8*pitch + 8, pitch, alpha, beta );
        }

        dst += 16;
    }

    // Cb
    bool bTopLine = (mbCtx[0].topQp >= 0);
    int qpDelta = 2 * ctx->picHdr.chroma_quant_delta_cb + 1; 
    pitch = ctx->picPitch[1];   
    dst = ctx->picPlane[1] + my * pitch * 8;
    if( bTopLine )
    {
        int cbQp = mbCtx[0].curQp + qpDelta;
        int avQp = (mbCtx[0].topQp + cbQp) >> 1;
        int idxA = g_ChromaQp[avQp] + alphaOffset;
        int idxB = g_ChromaQp[avQp] + betaOffset;
        if( idxA >= 6 && idxB >= 6 )
        {
            LF_chroma_hor_bs2_sse4( dst, pitch, s_AlphaTab[idxA], s_BetaTab[idxB] );
        }
    }
    for( int i = 1; i < mbCnt; i++ )
    {
        const int cbQp = mbCtx[i].curQp + qpDelta;
        dst += 8;

        // 垂直边界
        int avQp = (mbCtx[i].leftQp + cbQp) >> 1;
        int idxA = g_ChromaQp[avQp] + alphaOffset;
        int idxB = g_ChromaQp[avQp] + betaOffset;
        if( idxA >= 6 && idxB >= 6 )
        {
            LF_chroma_ver_bs2_sse4( dst, pitch, s_AlphaTab[idxA], s_BetaTab[idxB] );
        }

        // 水平边界
        if( bTopLine )
        {
            int avQp = (mbCtx[i].topQp + cbQp) >> 1;
            int idxA = g_ChromaQp[avQp] + alphaOffset;
            int idxB = g_ChromaQp[avQp] + betaOffset;
            if( idxA >= 6 && idxB >= 6 )
            {
                LF_chroma_hor_bs2_sse4( dst, pitch, s_AlphaTab[idxA], s_BetaTab[idxB] );
            }
        }
    }

    // Cr
    qpDelta = 2 * ctx->picHdr.chroma_quant_delta_cr + 1;
    pitch = ctx->picPitch[2];
    dst = ctx->picPlane[2] + my * pitch * 8;
    if( bTopLine )
    {
        int crQp = mbCtx[0].curQp + qpDelta;
        int avQp = (mbCtx[0].topQp + crQp) >> 1;
        int idxA = g_ChromaQp[avQp] + alphaOffset;
        int idxB = g_ChromaQp[avQp] + betaOffset;
        if( idxA >= 6 && idxB >= 6 )
        {
            LF_chroma_hor_bs2_sse4( dst, pitch, s_AlphaTab[idxA], s_BetaTab[idxB] );
        }
    }
    for( int i = 1; i < mbCnt; i++ )
    {
        const int crQp = mbCtx[i].curQp + qpDelta;
        dst += 8;

        // 垂直边界
        int avQp = (mbCtx[i].leftQp + crQp) >> 1;
        int idxA = g_ChromaQp[avQp] + alphaOffset;
        int idxB = g_ChromaQp[avQp] + betaOffset;
        if( idxA >= 6 && idxB >= 6 )
        {
            LF_chroma_ver_bs2_sse4( dst, pitch, s_AlphaTab[idxA], s_BetaTab[idxB] );
        }

        // 水平边界
        if( bTopLine )
        {
            int avQp = (mbCtx[i].topQp + crQp) >> 1;
            int idxA = g_ChromaQp[avQp] + alphaOffset;
            int idxB = g_ChromaQp[avQp] + betaOffset;
            if( idxA >= 6 && idxB >= 6 )
            {
                LF_chroma_hor_bs2_sse4( dst, pitch, s_AlphaTab[idxA], s_BetaTab[idxB] );
            }
        }
    }
}

// 针对 P 帧和 B 帧
void loop_filterPB_sse4( FrmDecContext* ctx, int my )
{
    const MbContext* mbCtx = ctx->topMbBuf[(my & 1)^1] + 1;
    const int mbCnt = ctx->mbColCnt;
    const int alphaOffset = ctx->picHdr.alpha_c_offset;
    const int betaOffset = ctx->picHdr.beta_offset;

    // Luma
    int pitch = ctx->picPitch[0];  
    uint8_t* dst = ctx->picPlane[0] + my * pitch * 16;
    for( int i = 0; i < mbCnt; i++ )
    {
        const int curQp = mbCtx[i].curQp;
        const uint32_t bs = mbCtx[i].lfBS;

        // 垂直边界 1
        int avQp = (mbCtx[i].leftQp + curQp + 1) >> 1;
        int idxA = avQp + alphaOffset;
        int idxB = avQp + betaOffset;
        if( idxA >= 6 && idxB >= 6 )
        {
            int16_t alpha = s_AlphaTab[idxA];
            int16_t beta = s_BetaTab[idxB];
            if( bs & 0xA )      // bs == 2
            {
                LF_luma_ver_bs2_sse4( dst, pitch, alpha, beta );
                LF_luma_ver_bs2_sse4( dst + 8*pitch, pitch, alpha, beta );
            }
            else if( idxA >= 16 )
            {
                if( bs & 0x1 )
                    LF_luma_ver_bs1_sse4( dst, pitch, alpha, beta, s_TcTab[idxA] );
                if( bs & 0x4 )
                    LF_luma_ver_bs1_sse4( dst + 8*pitch, pitch, alpha, beta, s_TcTab[idxA] );
            }
        }

        // 垂直边界 2
        idxA = curQp + alphaOffset;
        idxB = curQp + betaOffset;
        if( idxA >= 6 && idxB >= 6 )
        {
            int16_t alpha = s_AlphaTab[idxA];
            int16_t beta = s_BetaTab[idxB];
            if( bs & 0xA0 )      // bs == 2
            {
                LF_luma_ver_bs2_sse4( dst + 8, pitch, alpha, beta );
                LF_luma_ver_bs2_sse4( dst + 8 + 8*pitch, pitch, alpha, beta );
            }
            else if( idxA >= 16 )
            {
                if( bs & 0x10 )
                    LF_luma_ver_bs1_sse4( dst + 8, pitch, alpha, beta, s_TcTab[idxA] );
                if( bs & 0x40 )
                    LF_luma_ver_bs1_sse4( dst + 8 + 8*pitch, pitch, alpha, beta, s_TcTab[idxA] );
            }
        }

        // 水平边界 1
        avQp = (mbCtx[i].topQp + curQp + 1) >> 1;
        idxA = avQp + alphaOffset;
        idxB = avQp + betaOffset;
        if( idxA >= 6 && idxB >= 6 )
        {
            int16_t alpha = s_AlphaTab[idxA];
            int16_t beta = s_BetaTab[idxB];
            if( bs & 0xA00 )        // bs == 2
            {   
                LF_luma_hor_bs2_sse4( dst, pitch, alpha, beta );
                LF_luma_hor_bs2_sse4( dst + 8, pitch, alpha, beta );
            }
            else if( idxA >= 16 )
            {
                if( bs & 0x100 )
                    LF_luma_hor_bs1_sse4( dst, pitch, alpha, beta, s_TcTab[idxA] );
                if( bs & 0x400 )
                    LF_luma_hor_bs1_sse4( dst + 8, pitch, alpha, beta, s_TcTab[idxA] );
            }
        }

        // 水平边界 2
        idxA = curQp + alphaOffset;
        idxB = curQp + betaOffset;
        if( idxA >= 6 && idxB >= 6 )
        {
            int16_t alpha = s_AlphaTab[idxA];
            int16_t beta = s_BetaTab[idxB];
            if( bs & 0xA000 )   // bs == 2
            {   
                LF_luma_hor_bs2_sse4( dst + 8*pitch,     pitch, alpha, beta );
                LF_luma_hor_bs2_sse4( dst + 8*pitch + 8, pitch, alpha, beta );
            }
            else if( idxA >= 16 )
            {
                if( bs & 0x1000 )
                    LF_luma_hor_bs1_sse4( dst + 8*pitch, pitch, alpha, beta, s_TcTab[idxA] );
                if( bs & 0x4000 )
                    LF_luma_hor_bs1_sse4( dst + 8*pitch + 8, pitch, alpha, beta, s_TcTab[idxA] );
            }
        }

        dst += 16;
    }

    // Cb
    bool bTopLine = (mbCtx[0].topQp >= 0);
    int qpDelta = 2 * ctx->picHdr.chroma_quant_delta_cb + 1; 
    pitch = ctx->picPitch[1];   
    dst = ctx->picPlane[1] + my * pitch * 8;
    for( int i = 0; i < mbCnt; i++ )
    {
        const int cbQp = mbCtx[i].curQp + qpDelta;
        const uint32_t bs = mbCtx[i].lfBS;

        // 垂直边界
        if( i > 0 )
        {
            int avQp = (mbCtx[i].leftQp + cbQp) >> 1;
            int idxA = g_ChromaQp[avQp] + alphaOffset;
            int idxB = g_ChromaQp[avQp] + betaOffset;
            if( idxA >= 6 && idxB >= 6 )
            {
                if( bs & 0xA )
                {
                    LF_chroma_ver_bs2_sse4( dst, pitch, s_AlphaTab[idxA], s_BetaTab[idxB] );
                }
                else if( idxA >= 16 && (bs & 0x5) )
                {
                    int16_t tc[2];
                    tc[0] = s_TcTab[idxA] * (bs & 1);
                    tc[1] = s_TcTab[idxA] * ((bs >> 2) & 1);
                    LF_chroma_ver_bs1_sse4( dst, pitch, s_AlphaTab[idxA], s_BetaTab[idxB], tc );
                }
            }
        }

        // 水平边界
        if( bTopLine )
        {
            int avQp = (mbCtx[i].topQp + cbQp) >> 1;
            int idxA = g_ChromaQp[avQp] + alphaOffset;
            int idxB = g_ChromaQp[avQp] + betaOffset;
            if( idxA >= 6 && idxB >= 6 )
            {
                if( bs & 0xA00 )
                {
                    LF_chroma_hor_bs2_sse4( dst, pitch, s_AlphaTab[idxA], s_BetaTab[idxB] );
                }
                else if( idxA >= 16 && (bs & 0x500) )
                {
                    int16_t tc[2];
                    tc[0] = s_TcTab[idxA] * ((bs >> 8) & 1);
                    tc[1] = s_TcTab[idxA] * ((bs >> 10) & 1);
                    LF_chroma_hor_bs1_sse4( dst, pitch, s_AlphaTab[idxA], s_BetaTab[idxB], tc );
                }
            }
        }

        dst += 8;
    }

    // Cr
    qpDelta = 2 * ctx->picHdr.chroma_quant_delta_cr + 1;
    pitch = ctx->picPitch[2];
    dst = ctx->picPlane[2] + my * pitch * 8;
    for( int i = 0; i < mbCnt; i++ )
    {
        const int crQp = mbCtx[i].curQp + qpDelta;
        const uint32_t bs = mbCtx[i].lfBS;

        // 垂直边界
        if( i > 0 )
        {
            int avQp = (mbCtx[i].leftQp + crQp) >> 1;
            int idxA = g_ChromaQp[avQp] + alphaOffset;
            int idxB = g_ChromaQp[avQp] + betaOffset;
            if( idxA >= 6 && idxB >= 6 )
            {
                if( bs & 0xA )
                {
                    LF_chroma_ver_bs2_sse4( dst, pitch, s_AlphaTab[idxA], s_BetaTab[idxB] );
                }
                else if( idxA >= 16 && (bs & 0x5) )
                {
                    int16_t tc[2];
                    tc[0] = s_TcTab[idxA] * (bs & 1);
                    tc[1] = s_TcTab[idxA] * ((bs >> 2) & 1);
                    LF_chroma_ver_bs1_sse4( dst, pitch, s_AlphaTab[idxA], s_BetaTab[idxB], tc );
                }
            }
        }

        // 水平边界
        if( bTopLine )
        {
            int avQp = (mbCtx[i].topQp + crQp) >> 1;
            int idxA = g_ChromaQp[avQp] + alphaOffset;
            int idxB = g_ChromaQp[avQp] + betaOffset;
            if( idxA >= 6 && idxB >= 6 )
            {
                if( bs & 0xA00 )
                {
                    LF_chroma_hor_bs2_sse4( dst, pitch, s_AlphaTab[idxA], s_BetaTab[idxB] );
                }
                else if( idxA >= 16 && (bs & 0x500) )
                {
                    int16_t tc[2];
                    tc[0] = s_TcTab[idxA] * ((bs >> 8) & 1);
                    tc[1] = s_TcTab[idxA] * ((bs >> 10) & 1);
                    LF_chroma_hor_bs1_sse4( dst, pitch, s_AlphaTab[idxA], s_BetaTab[idxB], tc );
                }
            }
        }

        dst += 8;
    }
}

//======================================================================================================================
// 标准 C 实现, 用作参考

#define P2  data[-3]
#define P1  data[-2]
#define P0  data[-1]
#define Q0  data[0]
#define Q1  data[1]
#define Q2  data[2]

static void LF_luma_ver_bs2_c( uint8_t* data, int pitch, int16_t alpha, int16_t beta )
{
    const int a2 = (alpha >> 2) + 2;
    for( int i = 0; i < 8; i++ )
    {
        int pq0Abs = abs( P0 - Q0 );
        if( pq0Abs < alpha && abs(P1 - P0) < beta && abs(Q1 - Q0) < beta )
        {
            int pq2 = P0 + Q0 + 2;
            if( abs(P2 - P0) < beta && pq0Abs < a2 )
            {
                P0 = (uint8_t)((P1 + P0 + pq2) >> 2);
                P1 = (uint8_t)((P1 + P1 + pq2) >> 2);
            }
            else
            {
                P0 = (uint8_t)((P1 + P1 + pq2) >> 2);
            }
            if( abs(Q2 - Q0) < beta && pq0Abs < a2 )
            {
                Q0 = (uint8_t)((Q1 + Q0 + pq2) >> 2);
                Q1 = (uint8_t)((Q1 + Q1 + pq2) >> 2);
            }
            else
            {
                Q0 = (uint8_t)((Q1 + Q1 + pq2) >> 2);
            }
        }
        data += pitch;
    }
}

static void LF_chroma_ver_bs2_c( uint8_t* data, int pitch, int16_t alpha, int16_t beta )
{
    const int a2 = (alpha >> 2) + 2;
    for( int i = 0; i < 8; i++ )
    {
        int pq0Abs = abs( P0 - Q0 );
        if( pq0Abs < alpha && abs(P1 - P0) < beta && abs(Q1 - Q0) < beta )
        {
            int pq2 = P0 + Q0 + 2;
            if( abs(P2 - P0) < beta && pq0Abs < a2 )
            {
                P0 = (uint8_t)((P1 + P0 + pq2) >> 2);
            }
            else
            {
                P0 = (uint8_t)((P1 + P1 + pq2) >> 2);
            }
            if( abs(Q2 - Q0) < beta && pq0Abs < a2 )
            {
                Q0 = (uint8_t)((Q1 + Q0 + pq2) >> 2);
            }
            else
            {
                Q0 = (uint8_t)((Q1 + Q1 + pq2) >> 2);
            }
        }
        data += pitch;
    }
}

static void LF_luma_ver_bs1_c( uint8_t* data, int pitch, int16_t alpha, int16_t beta, int16_t tc )
{
    for( int i = 0; i < 8; i++ )
    {
        if( abs(P0 - Q0) < alpha && abs(P1 - P0) < beta && abs(Q1 - Q0) < beta )
        {
            int ap = abs( P2 - P0 );
            int aq = abs( Q2 - Q0 );
            int delta = ((Q0 - P0) * 3 + (P1 - Q1) + 4) >> 3;
            delta = clip3( delta, -tc, tc );
            P0 = (uint8_t)clip3( P0 + delta, 0, 255 );
            Q0 = (uint8_t)clip3( Q0 - delta, 0, 255 );
            if( ap < beta )
            {
                delta = ((P0 - P1) * 3 + (P2 - Q0) + 4) >> 3;
                delta = clip3( delta, -tc, tc );
                P1 = (uint8_t)clip3( P1 + delta, 0, 255 );
            }
            if( aq < beta )
            {
                delta = ((Q1 - Q0) * 3 + (P0 - Q2) + 4) >> 3;
                delta = clip3( delta, -tc, tc );
                Q1 = (uint8_t)clip3( Q1 - delta, 0, 255 );
            }
        }
        data += pitch;
    }
}

static void LF_chroma_ver_bs1_c( uint8_t* data, int pitch, int16_t alpha, int16_t beta, int16_t tc[2] )
{
    for( int i = 0; i < 4; i++ )
    {
        if( abs(P0 - Q0) < alpha && abs(P1 - P0) < beta && abs(Q1 - Q0) < beta )
        {
            int delta = ((Q0 - P0) * 3 + (P1 - Q1) + 4) >> 3;
            delta = clip3( delta, -tc[0], tc[0] );
            P0 = (uint8_t)clip3( P0 + delta, 0, 255 );
            Q0 = (uint8_t)clip3( Q0 - delta, 0, 255 );
        }
        data += pitch;
    }
    for( int i = 0; i < 4; i++ )
    {
        if( abs(P0 - Q0) < alpha && abs(P1 - P0) < beta && abs(Q1 - Q0) < beta )
        {
            int delta = ((Q0 - P0) * 3 + (P1 - Q1) + 4) >> 3;
            delta = clip3( delta, -tc[1], tc[1] );
            P0 = (uint8_t)clip3( P0 + delta, 0, 255 );
            Q0 = (uint8_t)clip3( Q0 - delta, 0, 255 );
        }
        data += pitch;
    }
}

#undef P2
#undef P1
#undef P0
#undef Q0
#undef Q1
#undef Q2

#define P2  data[-pitch3]
#define P1  data[-pitch2]
#define P0  data[-pitch]
#define Q0  data[0]
#define Q1  data[pitch]
#define Q2  data[pitch2]

static void LF_luma_hor_bs2_c( uint8_t* data, int pitch, int16_t alpha, int16_t beta )
{
    const int pitch2 = pitch + pitch;
    const int pitch3 = pitch + pitch2;
    const int a2 = (alpha >> 2) + 2;
    for( int i = 0; i < 8; i++ )
    {
        int pq0Abs = abs( P0 - Q0 );
        if( pq0Abs < alpha && abs(P1 - P0) < beta && abs(Q1 - Q0) < beta )
        {
            int pq2 = P0 + Q0 + 2;
            if( abs(P2 - P0) < beta && pq0Abs < a2 )
            {
                P0 = (uint8_t)((P1 + P0 + pq2) >> 2);
                P1 = (uint8_t)((P1 + P1 + pq2) >> 2);
            }
            else
            {
                P0 = (uint8_t)((P1 + P1 + pq2) >> 2);
            }
            if( abs(Q2 - Q0) < beta && pq0Abs < a2 )
            {
                Q0 = (uint8_t)((Q1 + Q0 + pq2) >> 2);
                Q1 = (uint8_t)((Q1 + Q1 + pq2) >> 2);
            }
            else
            {
                Q0 = (uint8_t)((Q1 + Q1 + pq2) >> 2);
            }
        }
        data += 1;
    }
}

static void LF_chroma_hor_bs2_c( uint8_t* data, int pitch, int16_t alpha, int16_t beta )
{
    const int pitch2 = pitch + pitch;
    const int pitch3 = pitch + pitch2;
    const int a2 = (alpha >> 2) + 2;
    for( int i = 0; i < 8; i++ )
    {
        int pq0Abs = abs( P0 - Q0 );
        if( pq0Abs < alpha && abs(P1 - P0) < beta && abs(Q1 - Q0) < beta )
        {
            int pq2 = P0 + Q0 + 2;
            if( abs(P2 - P0) < beta && pq0Abs < a2 )
            {
                P0 = (uint8_t)((P1 + P0 + pq2) >> 2);
            }
            else
            {
                P0 = (uint8_t)((P1 + P1 + pq2) >> 2);
            }
            if( abs(Q2 - Q0) < beta && pq0Abs < a2 )
            {
                Q0 = (uint8_t)((Q1 + Q0 + pq2) >> 2);
            }
            else
            {
                Q0 = (uint8_t)((Q1 + Q1 + pq2) >> 2);
            }
        }
        data += 1;
    }
}

static void LF_luma_hor_bs1_c( uint8_t* data, int pitch, int16_t alpha, int16_t beta, int16_t tc )
{
    const int pitch2 = pitch + pitch;
    const int pitch3 = pitch + pitch2;
    for( int i = 0; i < 8; i++ )
    {
        if( abs(P0 - Q0) < alpha && abs(P1 - P0) < beta && abs(Q1 - Q0) < beta )
        {
            int ap = abs( P2 - P0 );
            int aq = abs( Q2 - Q0 );
            int delta = ((Q0 - P0) * 3 + (P1 - Q1) + 4) >> 3;
            delta = clip3( delta, -tc, tc );
            P0 = (uint8_t)clip3( P0 + delta, 0, 255 );
            Q0 = (uint8_t)clip3( Q0 - delta, 0, 255 );
            if( ap < beta )
            {
                delta = ((P0 - P1) * 3 + (P2 - Q0) + 4) >> 3;
                delta = clip3( delta, -tc, tc );
                P1 = (uint8_t)clip3( P1 + delta, 0, 255 );
            }
            if( aq < beta )
            {
                delta = ((Q1 - Q0) * 3 + (P0 - Q2) + 4) >> 3;
                delta = clip3( delta, -tc, tc );
                Q1 = (uint8_t)clip3( Q1 - delta, 0, 255 );
            }
        }
        data += 1;
    }
}

static void LF_chroma_hor_bs1_c( uint8_t* data, int pitch, int16_t alpha, int16_t beta, int16_t tc[2] )
{
    const int pitch2 = pitch + pitch;
    const int pitch3 = pitch + pitch2;
    for( int i = 0; i < 4; i++ )
    {
        if( abs(P0 - Q0) < alpha && abs(P1 - P0) < beta && abs(Q1 - Q0) < beta )
        {
            int delta = ((Q0 - P0) * 3 + (P1 - Q1) + 4) >> 3;
            delta = clip3( delta, -tc[0], tc[0] );
            P0 = (uint8_t)clip3( P0 + delta, 0, 255 );
            Q0 = (uint8_t)clip3( Q0 - delta, 0, 255 );
        }
        data += 1;
    }
    for( int i = 0; i < 4; i++ )
    {
        if( abs(P0 - Q0) < alpha && abs(P1 - P0) < beta && abs(Q1 - Q0) < beta )
        {
            int delta = ((Q0 - P0) * 3 + (P1 - Q1) + 4) >> 3;
            delta = clip3( delta, -tc[1], tc[1] );
            P0 = (uint8_t)clip3( P0 + delta, 0, 255 );
            Q0 = (uint8_t)clip3( Q0 - delta, 0, 255 );
        }
        data += 1;
    }
}

#undef P2
#undef P1
#undef P0
#undef Q0
#undef Q1
#undef Q2

// 针对 I 帧
void loop_filterI_c( FrmDecContext* ctx, int my )
{
    const MbContext* mbCtx = ctx->topMbBuf[(my & 1)^1] + 1;
    const int mbCnt = ctx->mbColCnt;
    const int alphaOffset = ctx->picHdr.alpha_c_offset;
    const int betaOffset = ctx->picHdr.beta_offset;

    // Luma
    int pitch = ctx->picPitch[0];  
    uint8_t* dst = ctx->picPlane[0] + my * pitch * 16;
    for( int i = 0; i < mbCnt; i++ )
    {
        const int curQp = mbCtx[i].curQp;

        // 垂直边界 1
        int avQp = (mbCtx[i].leftQp + curQp + 1) >> 1;
        int idxA = avQp + alphaOffset;
        int idxB = avQp + betaOffset;
        if( idxA >= 6 && idxB >= 6 )
        {
            int16_t alpha = s_AlphaTab[idxA];
            int16_t beta = s_BetaTab[idxB];
            LF_luma_ver_bs2_c( dst,           pitch, alpha, beta );
            LF_luma_ver_bs2_c( dst + 8*pitch, pitch, alpha, beta );
        }

        // 垂直边界 2
        idxA = curQp + alphaOffset;
        idxB = curQp + betaOffset;
        if( idxA >= 6 && idxB >= 6 )
        {
            int16_t alpha = s_AlphaTab[idxA];
            int16_t beta = s_BetaTab[idxB];
            LF_luma_ver_bs2_c( dst + 8,           pitch, alpha, beta );
            LF_luma_ver_bs2_c( dst + 8 + 8*pitch, pitch, alpha, beta );
        }

        // 水平边界 1
        avQp = (mbCtx[i].topQp + curQp + 1) >> 1;
        idxA = avQp + alphaOffset;
        idxB = avQp + betaOffset;
        if( idxA >= 6 && idxB >= 6 )
        {
            int16_t alpha = s_AlphaTab[idxA];
            int16_t beta = s_BetaTab[idxB];
            LF_luma_hor_bs2_c( dst,     pitch, alpha, beta );
            LF_luma_hor_bs2_c( dst + 8, pitch, alpha, beta );
        }

        // 水平边界 2
        idxA = curQp + alphaOffset;
        idxB = curQp + betaOffset;
        if( idxA >= 6 && idxB >= 6 )
        {
            int16_t alpha = s_AlphaTab[idxA];
            int16_t beta = s_BetaTab[idxB];
            LF_luma_hor_bs2_c( dst + 8*pitch,     pitch, alpha, beta );
            LF_luma_hor_bs2_c( dst + 8*pitch + 8, pitch, alpha, beta );
        }

        dst += 16;
    }

    // Cb
    bool bTopLine = (mbCtx[0].topQp >= 0);
    int qpDelta = 2 * ctx->picHdr.chroma_quant_delta_cb + 1; 
    pitch = ctx->picPitch[1];   
    dst = ctx->picPlane[1] + my * pitch * 8;
    if( bTopLine )
    {
        int cbQp = mbCtx[0].curQp + qpDelta;
        int avQp = (mbCtx[0].topQp + cbQp) >> 1;
        int idxA = g_ChromaQp[avQp] + alphaOffset;
        int idxB = g_ChromaQp[avQp] + betaOffset;
        if( idxA >= 6 && idxB >= 6 )
        {
            LF_chroma_hor_bs2_c( dst, pitch, s_AlphaTab[idxA], s_BetaTab[idxB] );
        }
    }
    for( int i = 1; i < mbCnt; i++ )
    {
        const int cbQp = mbCtx[i].curQp + qpDelta;
        dst += 8;

        // 垂直边界
        int avQp = (mbCtx[i].leftQp + cbQp) >> 1;
        int idxA = g_ChromaQp[avQp] + alphaOffset;
        int idxB = g_ChromaQp[avQp] + betaOffset;
        if( idxA >= 6 && idxB >= 6 )
        {
            LF_chroma_ver_bs2_c( dst, pitch, s_AlphaTab[idxA], s_BetaTab[idxB] );
        }

        // 水平边界
        if( bTopLine )
        {
            int avQp = (mbCtx[i].topQp + cbQp) >> 1;
            int idxA = g_ChromaQp[avQp] + alphaOffset;
            int idxB = g_ChromaQp[avQp] + betaOffset;
            if( idxA >= 6 && idxB >= 6 )
            {
                LF_chroma_hor_bs2_c( dst, pitch, s_AlphaTab[idxA], s_BetaTab[idxB] );
            }
        }
    }

    // Cr
    qpDelta = 2 * ctx->picHdr.chroma_quant_delta_cr + 1;
    pitch = ctx->picPitch[2];
    dst = ctx->picPlane[2] + my * pitch * 8;
    if( bTopLine )
    {
        int crQp = mbCtx[0].curQp + qpDelta;
        int avQp = (mbCtx[0].topQp + crQp) >> 1;
        int idxA = g_ChromaQp[avQp] + alphaOffset;
        int idxB = g_ChromaQp[avQp] + betaOffset;
        if( idxA >= 6 && idxB >= 6 )
        {
            LF_chroma_hor_bs2_c( dst, pitch, s_AlphaTab[idxA], s_BetaTab[idxB] );
        }
    }
    for( int i = 1; i < mbCnt; i++ )
    {
        const int crQp = mbCtx[i].curQp + qpDelta;
        dst += 8;

        // 垂直边界
        int avQp = (mbCtx[i].leftQp + crQp) >> 1;
        int idxA = g_ChromaQp[avQp] + alphaOffset;
        int idxB = g_ChromaQp[avQp] + betaOffset;
        if( idxA >= 6 && idxB >= 6 )
        {
            LF_chroma_ver_bs2_c( dst, pitch, s_AlphaTab[idxA], s_BetaTab[idxB] );
        }

        // 水平边界
        if( bTopLine )
        {
            int avQp = (mbCtx[i].topQp + crQp) >> 1;
            int idxA = g_ChromaQp[avQp] + alphaOffset;
            int idxB = g_ChromaQp[avQp] + betaOffset;
            if( idxA >= 6 && idxB >= 6 )
            {
                LF_chroma_hor_bs2_c( dst, pitch, s_AlphaTab[idxA], s_BetaTab[idxB] );
            }
        }
    }
}

// 针对 P 帧和 B 帧
void loop_filterPB_c( FrmDecContext* ctx, int my )
{
    const MbContext* mbCtx = ctx->topMbBuf[(my & 1)^1] + 1;
    const int mbCnt = ctx->mbColCnt;
    const int alphaOffset = ctx->picHdr.alpha_c_offset;
    const int betaOffset = ctx->picHdr.beta_offset;

    // Luma
    int pitch = ctx->picPitch[0];  
    uint8_t* dst = ctx->picPlane[0] + my * pitch * 16;
    for( int i = 0; i < mbCnt; i++ )
    {
        const int curQp = mbCtx[i].curQp;
        const uint32_t bs = mbCtx[i].lfBS;

        // 垂直边界 1
        int avQp = (mbCtx[i].leftQp + curQp + 1) >> 1;
        int idxA = avQp + alphaOffset;
        int idxB = avQp + betaOffset;
        if( idxA >= 6 && idxB >= 6 )
        {
            int16_t alpha = s_AlphaTab[idxA];
            int16_t beta = s_BetaTab[idxB];
            if( bs & 0xA )      // bs == 2
            {
                LF_luma_ver_bs2_c( dst, pitch, alpha, beta );
                LF_luma_ver_bs2_c( dst + 8*pitch, pitch, alpha, beta );
            }
            else if( idxA >= 16 )
            {
                if( bs & 0x1 )
                    LF_luma_ver_bs1_c( dst, pitch, alpha, beta, s_TcTab[idxA] );
                if( bs & 0x4 )
                    LF_luma_ver_bs1_c( dst + 8*pitch, pitch, alpha, beta, s_TcTab[idxA] );
            }
        }

        // 垂直边界 2
        idxA = curQp + alphaOffset;
        idxB = curQp + betaOffset;
        if( idxA >= 6 && idxB >= 6 )
        {
            int16_t alpha = s_AlphaTab[idxA];
            int16_t beta = s_BetaTab[idxB];
            if( bs & 0xA0 )      // bs == 2
            {
                LF_luma_ver_bs2_c( dst + 8, pitch, alpha, beta );
                LF_luma_ver_bs2_c( dst + 8 + 8*pitch, pitch, alpha, beta );
            }
            else if( idxA >= 16 )
            {
                if( bs & 0x10 )
                    LF_luma_ver_bs1_c( dst + 8, pitch, alpha, beta, s_TcTab[idxA] );
                if( bs & 0x40 )
                    LF_luma_ver_bs1_c( dst + 8 + 8*pitch, pitch, alpha, beta, s_TcTab[idxA] );
            }
        }

        // 水平边界 1
        avQp = (mbCtx[i].topQp + curQp + 1) >> 1;
        idxA = avQp + alphaOffset;
        idxB = avQp + betaOffset;
        if( idxA >= 6 && idxB >= 6 )
        {
            int16_t alpha = s_AlphaTab[idxA];
            int16_t beta = s_BetaTab[idxB];
            if( bs & 0xA00 )        // bs == 2
            {   
                LF_luma_hor_bs2_c( dst, pitch, alpha, beta );
                LF_luma_hor_bs2_c( dst + 8, pitch, alpha, beta );
            }
            else if( idxA >= 16 )
            {
                if( bs & 0x100 )
                    LF_luma_hor_bs1_c( dst, pitch, alpha, beta, s_TcTab[idxA] );
                if( bs & 0x400 )
                    LF_luma_hor_bs1_c( dst + 8, pitch, alpha, beta, s_TcTab[idxA] );
            }
        }

        // 水平边界 2
        idxA = curQp + alphaOffset;
        idxB = curQp + betaOffset;
        if( idxA >= 6 && idxB >= 6 )
        {
            int16_t alpha = s_AlphaTab[idxA];
            int16_t beta = s_BetaTab[idxB];
            if( bs & 0xA000 )   // bs == 2
            {   
                LF_luma_hor_bs2_c( dst + 8*pitch,     pitch, alpha, beta );
                LF_luma_hor_bs2_c( dst + 8*pitch + 8, pitch, alpha, beta );
            }
            else if( idxA >= 16 )
            {
                if( bs & 0x1000 )
                    LF_luma_hor_bs1_c( dst + 8*pitch, pitch, alpha, beta, s_TcTab[idxA] );
                if( bs & 0x4000 )
                    LF_luma_hor_bs1_c( dst + 8*pitch + 8, pitch, alpha, beta, s_TcTab[idxA] );
            }
        }

        dst += 16;
    }

    // Cb
    bool bTopLine = (mbCtx[0].topQp >= 0);
    int qpDelta = 2 * ctx->picHdr.chroma_quant_delta_cb + 1; 
    pitch = ctx->picPitch[1];   
    dst = ctx->picPlane[1] + my * pitch * 8;
    for( int i = 0; i < mbCnt; i++ )
    {
        const int cbQp = mbCtx[i].curQp + qpDelta;
        const uint32_t bs = mbCtx[i].lfBS;

        // 垂直边界
        if( i > 0 )
        {
            int avQp = (mbCtx[i].leftQp + cbQp) >> 1;
            int idxA = g_ChromaQp[avQp] + alphaOffset;
            int idxB = g_ChromaQp[avQp] + betaOffset;
            if( idxA >= 6 && idxB >= 6 )
            {
                if( bs & 0xA )
                {
                    LF_chroma_ver_bs2_c( dst, pitch, s_AlphaTab[idxA], s_BetaTab[idxB] );
                }
                else if( idxA >= 16 && (bs & 0x5) )
                {
                    int16_t tc[2];
                    tc[0] = s_TcTab[idxA] * (bs & 1);
                    tc[1] = s_TcTab[idxA] * ((bs >> 2) & 1);
                    LF_chroma_ver_bs1_c( dst, pitch, s_AlphaTab[idxA], s_BetaTab[idxB], tc );
                }
            }
        }

        // 水平边界
        if( bTopLine )
        {
            int avQp = (mbCtx[i].topQp + cbQp) >> 1;
            int idxA = g_ChromaQp[avQp] + alphaOffset;
            int idxB = g_ChromaQp[avQp] + betaOffset;
            if( idxA >= 6 && idxB >= 6 )
            {
                if( bs & 0xA00 )
                {
                    LF_chroma_hor_bs2_c( dst, pitch, s_AlphaTab[idxA], s_BetaTab[idxB] );
                }
                else if( idxA >= 16 && (bs & 0x500) )
                {
                    int16_t tc[2];
                    tc[0] = s_TcTab[idxA] * ((bs >> 8) & 1);
                    tc[1] = s_TcTab[idxA] * ((bs >> 10) & 1);
                    LF_chroma_hor_bs1_c( dst, pitch, s_AlphaTab[idxA], s_BetaTab[idxB], tc );
                }
            }
        }

        dst += 8;
    }

    // Cr
    qpDelta = 2 * ctx->picHdr.chroma_quant_delta_cr + 1;
    pitch = ctx->picPitch[2];
    dst = ctx->picPlane[2] + my * pitch * 8;
    for( int i = 0; i < mbCnt; i++ )
    {
        const int crQp = mbCtx[i].curQp + qpDelta;
        const uint32_t bs = mbCtx[i].lfBS;

        // 垂直边界
        if( i > 0 )
        {
            int avQp = (mbCtx[i].leftQp + crQp) >> 1;
            int idxA = g_ChromaQp[avQp] + alphaOffset;
            int idxB = g_ChromaQp[avQp] + betaOffset;
            if( idxA >= 6 && idxB >= 6 )
            {
                if( bs & 0xA )
                {
                    LF_chroma_ver_bs2_c( dst, pitch, s_AlphaTab[idxA], s_BetaTab[idxB] );
                }
                else if( idxA >= 16 && (bs & 0x5) )
                {
                    int16_t tc[2];
                    tc[0] = s_TcTab[idxA] * (bs & 1);
                    tc[1] = s_TcTab[idxA] * ((bs >> 2) & 1);
                    LF_chroma_ver_bs1_c( dst, pitch, s_AlphaTab[idxA], s_BetaTab[idxB], tc );
                }
            }
        }

        // 水平边界
        if( bTopLine )
        {
            int avQp = (mbCtx[i].topQp + crQp) >> 1;
            int idxA = g_ChromaQp[avQp] + alphaOffset;
            int idxB = g_ChromaQp[avQp] + betaOffset;
            if( idxA >= 6 && idxB >= 6 )
            {
                if( bs & 0xA00 )
                {
                    LF_chroma_hor_bs2_c( dst, pitch, s_AlphaTab[idxA], s_BetaTab[idxB] );
                }
                else if( idxA >= 16 && (bs & 0x500) )
                {
                    int16_t tc[2];
                    tc[0] = s_TcTab[idxA] * ((bs >> 8) & 1);
                    tc[1] = s_TcTab[idxA] * ((bs >> 10) & 1);
                    LF_chroma_hor_bs1_c( dst, pitch, s_AlphaTab[idxA], s_BetaTab[idxB], tc );
                }
            }
        }

        dst += 8;
    }
}

}   // namespace irk_avs_dec
