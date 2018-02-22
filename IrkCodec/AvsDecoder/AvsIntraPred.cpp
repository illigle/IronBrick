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
#include <assert.h>
#include "AvsIntraPred.h"

#if defined(__GNUC__) || defined(__clang__)

// for strict aliasing
typedef int16_t __attribute__((may_alias))  int16_as;
typedef int32_t __attribute__((may_alias))  int32_as;
typedef int64_t __attribute__((may_alias))  int64_as;
typedef uint16_t __attribute__((may_alias)) uint16_as;
typedef uint32_t __attribute__((may_alias)) uint32_as;
typedef uint64_t __attribute__((may_alias)) uint64_as;

#else

typedef int16_t     int16_as;
typedef int32_t     int32_as;
typedef int64_t     int64_as;
typedef uint16_t    uint16_as;
typedef uint32_t    uint32_as;
typedef uint64_t    uint64_as;

#endif

namespace irk_avs_dec {

/*
* GCC 对 sse intrinsic 的支持欠佳,  _mm_loadl_epi64 和 _mm_storel_epi64 会生成许多多余的指令
*/

// 垂直预测
void intra_pred_ver(uint8_t* dst, int pitch, NBUsable)
{
    uint64_t val = *(uint64_as*)(dst - pitch);
    *(uint64_as*)(dst + pitch) = *(uint64_as*)dst = val;
    dst += pitch * 2;
    *(uint64_as*)(dst + pitch) = *(uint64_as*)dst = val;
    dst += pitch * 2;
    *(uint64_as*)(dst + pitch) = *(uint64_as*)dst = val;
    dst += pitch * 2;
    *(uint64_as*)(dst + pitch) = *(uint64_as*)dst = val;
}

// 水平预测
void intra_pred_hor(uint8_t* dst, int pitch, NBUsable)
{
    for (int i = 0; i < 8; i++)
    {
        *(uint32_as*)(dst + 4) = *(uint32_as*)dst = dst[-1] * 0x01010101u;
        dst += pitch;
    }
}

#define LOAD_LEFT_EDGE_X10( dst, src, pitch ) \
    (dst)[0] = (src)[0-pitch];      \
    (dst)[1] = (src)[0];            \
    (dst)[2] = (src)[pitch];        \
    (dst)[3] = (src)[pitch*2];      \
    (dst)[4] = (src)[pitch*3];      \
    (dst)[5] = (src)[pitch*4];      \
    (dst)[6] = (src)[pitch*5];      \
    (dst)[7] = (src)[pitch*6];      \
    (dst)[8] = (src)[pitch*7];      \
    (dst)[9] = (src)[pitch*8];

// DC 预测
void intra_pred_dc(uint8_t* dst, int pitch, NBUsable usable)
{
    __m128i xm0, xm1, xm2, xm3, xm4, xm5, xm6;
    const int8_t* flags = usable.flags;

    if ((flags[0] & flags[2]))  // 上边界和左边界都可用
    {
        alignas(16) uint8_t buf[16];
        const uint8_t* left = dst - 1;
        const uint8_t* top = dst - pitch;
        LOAD_LEFT_EDGE_X10(buf, left, pitch);

        xm6 = _mm_setzero_si128();
        xm6 = _mm_cmpeq_epi16(xm6, xm6);            // -1
        xm0 = _mm_loadu_si128((__m128i*)(top - 1)); // 边界有填充, 无需担心越界
        xm3 = _mm_loadu_si128((__m128i*)buf);
        xm1 = _mm_srli_si128(xm0, 1);               // top[0...7]
        xm4 = _mm_srli_si128(xm3, 1);               // left[0...7]
        xm0 = _mm_cvtepu8_epi16(xm0);
        xm1 = _mm_cvtepu8_epi16(xm1);
        xm3 = _mm_cvtepu8_epi16(xm3);
        xm4 = _mm_cvtepu8_epi16(xm4);
        xm2 = _mm_srli_si128(xm1, 2);
        xm5 = _mm_srli_si128(xm4, 2);
        xm1 = _mm_sub_epi16(xm1, xm6);              // + 1
        xm4 = _mm_sub_epi16(xm4, xm6);              // + 1
        xm2 = _mm_insert_epi16(xm2, top[7 + flags[1]], 7);
        xm5 = _mm_insert_epi16(xm5, buf[8 + flags[3]], 7);
        xm1 = _mm_add_epi16(xm1, xm1);
        xm4 = _mm_add_epi16(xm4, xm4);
        xm3 = _mm_add_epi16(xm3, xm5);
        xm0 = _mm_add_epi16(xm0, xm2);
        xm3 = _mm_add_epi16(xm3, xm4);
        xm0 = _mm_add_epi16(xm0, xm1);
        xm3 = _mm_srai_epi16(xm3, 2);               // left
        xm0 = _mm_srai_epi16(xm0, 2);               // top 
        xm4 = _mm_unpacklo_epi16(xm3, xm3);         // 0 0 1 1 2 2 3 3
        xm3 = _mm_unpackhi_epi16(xm3, xm3);         // 4 4 5 5 6 6 7 7

        xm1 = _mm_shuffle_epi32(xm4, 0);
        xm2 = _mm_shuffle_epi32(xm4, 0x55);
        xm1 = _mm_add_epi16(xm1, xm0);
        xm2 = _mm_add_epi16(xm2, xm0);
        xm1 = _mm_srai_epi16(xm1, 1);
        xm2 = _mm_srai_epi16(xm2, 1);
        xm1 = _mm_packus_epi16(xm1, xm1);
        xm2 = _mm_packus_epi16(xm2, xm2);
        _mm_storel_epi64((__m128i*)dst, xm1);
        _mm_storel_epi64((__m128i*)(dst + pitch), xm2);
        dst += pitch * 2;
        xm1 = _mm_shuffle_epi32(xm4, 0xAA);
        xm2 = _mm_shuffle_epi32(xm4, 0xFF);
        xm1 = _mm_add_epi16(xm1, xm0);
        xm2 = _mm_add_epi16(xm2, xm0);
        xm1 = _mm_srai_epi16(xm1, 1);
        xm2 = _mm_srai_epi16(xm2, 1);
        xm1 = _mm_packus_epi16(xm1, xm1);
        xm2 = _mm_packus_epi16(xm2, xm2);
        _mm_storel_epi64((__m128i*)dst, xm1);
        _mm_storel_epi64((__m128i*)(dst + pitch), xm2);
        dst += pitch * 2;
        xm1 = _mm_shuffle_epi32(xm3, 0);
        xm2 = _mm_shuffle_epi32(xm3, 0x55);
        xm1 = _mm_add_epi16(xm1, xm0);
        xm2 = _mm_add_epi16(xm2, xm0);
        xm1 = _mm_srai_epi16(xm1, 1);
        xm2 = _mm_srai_epi16(xm2, 1);
        xm1 = _mm_packus_epi16(xm1, xm1);
        xm2 = _mm_packus_epi16(xm2, xm2);
        _mm_storel_epi64((__m128i*)dst, xm1);
        _mm_storel_epi64((__m128i*)(dst + pitch), xm2);
        dst += pitch * 2;
        xm1 = _mm_shuffle_epi32(xm3, 0xAA);
        xm2 = _mm_shuffle_epi32(xm3, 0xFF);
        xm1 = _mm_add_epi16(xm1, xm0);
        xm2 = _mm_add_epi16(xm2, xm0);
        xm1 = _mm_srai_epi16(xm1, 1);
        xm2 = _mm_srai_epi16(xm2, 1);
        xm1 = _mm_packus_epi16(xm1, xm1);
        xm2 = _mm_packus_epi16(xm2, xm2);
        _mm_storel_epi64((__m128i*)dst, xm1);
        _mm_storel_epi64((__m128i*)(dst + pitch), xm2);
    }
    else if (flags[0])  // 上边界可用
    {
        const uint8_t* top = dst - pitch;
        xm1 = _mm_loadl_epi64((__m128i*)top);
        xm3 = _mm_setzero_si128();                  // 0
        xm1 = _mm_unpacklo_epi8(xm1, xm3);
        xm3 = _mm_cmpeq_epi16(xm3, xm3);            // -1
        xm0 = _mm_slli_si128(xm1, 2);
        xm2 = _mm_srli_si128(xm1, 2);
        xm1 = _mm_sub_epi16(xm1, xm3);              // + 1
        xm0 = _mm_insert_epi16(xm0, top[0 - flags[2]], 0);
        xm2 = _mm_insert_epi16(xm2, top[7 + flags[1]], 7);
        xm1 = _mm_add_epi16(xm1, xm1);
        xm0 = _mm_add_epi16(xm0, xm2);
        xm0 = _mm_add_epi16(xm0, xm1);
        xm0 = _mm_srai_epi16(xm0, 2);
        xm0 = _mm_packus_epi16(xm0, xm0);

        _mm_storel_epi64((__m128i*)dst, xm0);
        _mm_storel_epi64((__m128i*)(dst + pitch), xm0);
        dst += pitch * 2;
        _mm_storel_epi64((__m128i*)dst, xm0);
        _mm_storel_epi64((__m128i*)(dst + pitch), xm0);
        dst += pitch * 2;
        _mm_storel_epi64((__m128i*)dst, xm0);
        _mm_storel_epi64((__m128i*)(dst + pitch), xm0);
        dst += pitch * 2;
        _mm_storel_epi64((__m128i*)dst, xm0);
        _mm_storel_epi64((__m128i*)(dst + pitch), xm0);
    }
    else if (flags[2])  // 左边界可用
    {
        alignas(16) uint8_t buf[16];
        const uint8_t* left = dst - 1;
        LOAD_LEFT_EDGE_X10(buf, left, pitch);
        xm1 = _mm_loadl_epi64((__m128i*)(buf + 1));
        xm3 = _mm_setzero_si128();              // 0
        xm1 = _mm_unpacklo_epi8(xm1, xm3);
        xm3 = _mm_cmpeq_epi16(xm3, xm3);        // -1
        xm0 = _mm_slli_si128(xm1, 2);
        xm2 = _mm_srli_si128(xm1, 2);
        xm1 = _mm_sub_epi16(xm1, xm3);          // + 1
        xm0 = _mm_insert_epi16(xm0, buf[1 - flags[0]], 0);
        xm2 = _mm_insert_epi16(xm2, buf[8 + flags[3]], 7);
        xm1 = _mm_add_epi16(xm1, xm1);
        xm0 = _mm_add_epi16(xm0, xm2);
        xm0 = _mm_add_epi16(xm0, xm1);
        xm0 = _mm_srai_epi16(xm0, 2);
        xm0 = _mm_packus_epi16(xm0, xm0);

        xm0 = _mm_unpacklo_epi8(xm0, xm0);
        xm1 = _mm_unpacklo_epi16(xm0, xm0);
        xm0 = _mm_unpackhi_epi16(xm0, xm0);
        _mm_storel_epi64((__m128i*)(dst), _mm_shuffle_epi32(xm1, 0));
        _mm_storel_epi64((__m128i*)(dst + pitch), _mm_shuffle_epi32(xm1, 0x55));
        _mm_storel_epi64((__m128i*)(dst + pitch * 2), _mm_shuffle_epi32(xm1, 0xAA));
        _mm_storel_epi64((__m128i*)(dst + pitch * 3), _mm_shuffle_epi32(xm1, 0xFF));
        dst += pitch * 4;
        _mm_storel_epi64((__m128i*)(dst), _mm_shuffle_epi32(xm0, 0));
        _mm_storel_epi64((__m128i*)(dst + pitch), _mm_shuffle_epi32(xm0, 0x55));
        _mm_storel_epi64((__m128i*)(dst + pitch * 2), _mm_shuffle_epi32(xm0, 0xAA));
        _mm_storel_epi64((__m128i*)(dst + pitch * 3), _mm_shuffle_epi32(xm0, 0xFF));
    }
    else    // 边界都不可用, 赋值为 128
    {
        *(uint64_as*)(dst + pitch) = *(uint64_as*)dst = 0x8080808080808080ULL;
        dst += pitch * 2;
        *(uint64_as*)(dst + pitch) = *(uint64_as*)dst = 0x8080808080808080ULL;
        dst += pitch * 2;
        *(uint64_as*)(dst + pitch) = *(uint64_as*)dst = 0x8080808080808080ULL;
        dst += pitch * 2;
        *(uint64_as*)(dst + pitch) = *(uint64_as*)dst = 0x8080808080808080ULL;
    }
}

#define LOAD_LEFT_EDGE_X8( dst, src, pitch ) \
    (dst)[0] = (src)[0];            \
    (dst)[1] = (src)[pitch];        \
    (dst)[2] = (src)[pitch*2];      \
    (dst)[3] = (src)[pitch*3];      \
    (dst)[4] = (src)[pitch*4];      \
    (dst)[5] = (src)[pitch*5];      \
    (dst)[6] = (src)[pitch*6];      \
    (dst)[7] = (src)[pitch*7];

// down-left 预测
void intra_pred_downleft(uint8_t* dst, int pitch, NBUsable usable)
{
    __m128i xm0, xm1, xm2, xm3, xm4;
    __m128i msk = _mm_setzero_si128();                      // 0 x 16
    msk = _mm_sub_epi8(msk, _mm_cmpeq_epi8(msk, msk));      // 1 x 16

    const uint8_t* top = dst - pitch;
    xm0 = _mm_loadu_si128((__m128i*)top);       // 边界有填充, 不会越界
    if (usable.flags[1] == 0)                   // 右上角 8x8 块不可用
    {
        xm4 = _mm_cvtsi32_si128(top[7] * 0x01010101);
        xm4 = _mm_unpacklo_epi32(xm4, xm4);
        xm0 = _mm_unpacklo_epi64(xm0, xm4);
        xm1 = _mm_srli_si128(xm0, 1);
        xm2 = _mm_srli_si128(xm0, 2);
        xm2 = _mm_unpacklo_epi64(xm2, xm4);
    }
    else
    {
        xm1 = _mm_srli_si128(xm0, 1);
        xm2 = _mm_srli_si128(xm0, 2);
        xm2 = _mm_insert_epi16(xm2, top[15], 7);
    }
    xm4 = _mm_xor_si128(xm0, xm2);
    xm0 = _mm_avg_epu8(xm0, xm2);
    xm2 = _mm_xor_si128(xm0, xm1);
    xm0 = _mm_avg_epu8(xm0, xm1);
    xm4 = _mm_and_si128(xm4, xm2);
    xm4 = _mm_and_si128(xm4, msk);
    xm0 = _mm_subs_epu8(xm0, xm4);          // (r[i] + 2*r[i+1] + r[i+2] + 2) >> 2, i = 1...16

    const uint8_t* left = dst - 1;
    alignas(16) uint8_t buf[16];
    LOAD_LEFT_EDGE_X8(buf, left, pitch);
    xm1 = _mm_loadl_epi64((__m128i*)buf);
    if (usable.flags[3] == 0)               // 左下 8x8 块不可用
    {
        xm4 = _mm_cvtsi32_si128(buf[7] * 0x01010101);
        xm4 = _mm_unpacklo_epi32(xm4, xm4);
        xm1 = _mm_unpacklo_epi64(xm1, xm4);
        xm2 = _mm_srli_si128(xm1, 1);
        xm3 = _mm_srli_si128(xm1, 2);
        xm3 = _mm_unpacklo_epi64(xm3, xm4);
    }
    else    // 左下 8x8 块可用
    {
        left += pitch * 8;
        LOAD_LEFT_EDGE_X8(buf, left, pitch);
        xm4 = _mm_loadl_epi64((__m128i*)buf);
        xm1 = _mm_unpacklo_epi64(xm1, xm4);
        xm2 = _mm_srli_si128(xm1, 1);
        xm3 = _mm_srli_si128(xm1, 2);
        xm3 = _mm_insert_epi16(xm3, buf[7], 7);
    }
    xm4 = _mm_xor_si128(xm1, xm3);
    xm1 = _mm_avg_epu8(xm1, xm3);
    xm3 = _mm_xor_si128(xm1, xm2);
    xm1 = _mm_avg_epu8(xm1, xm2);
    xm4 = _mm_and_si128(xm4, xm3);
    xm4 = _mm_and_si128(xm4, msk);
    xm1 = _mm_subs_epu8(xm1, xm4);          // (c[i] + 2*c[i+1] + c[i+2] + 2) >> 2, i = 1...16

    xm4 = _mm_xor_si128(xm0, xm1);
    xm0 = _mm_avg_epu8(xm0, xm1);
    xm4 = _mm_and_si128(xm4, msk);
    xm0 = _mm_subs_epu8(xm0, xm4);

    // GCC 对 sse intrinsic 的支持欠佳, 没有生成最优指令
#ifndef __x86_64__
    _mm_storel_epi64((__m128i*)dst, xm0);
    xm0 = _mm_srli_si128(xm0, 1);
    _mm_storel_epi64((__m128i*)(dst + pitch), xm0);
    dst += pitch * 2;
    xm0 = _mm_srli_si128(xm0, 1);
    _mm_storel_epi64((__m128i*)dst, xm0);
    xm0 = _mm_srli_si128(xm0, 1);
    _mm_storel_epi64((__m128i*)(dst + pitch), xm0);
    dst += pitch * 2;
    xm0 = _mm_srli_si128(xm0, 1);
    _mm_storel_epi64((__m128i*)dst, xm0);
    xm0 = _mm_srli_si128(xm0, 1);
    _mm_storel_epi64((__m128i*)(dst + pitch), xm0);
    dst += pitch * 2;
    xm0 = _mm_srli_si128(xm0, 1);
    _mm_storel_epi64((__m128i*)dst, xm0);
    xm0 = _mm_srli_si128(xm0, 1);
    _mm_storel_epi64((__m128i*)(dst + pitch), xm0);
#else
    _mm_store_si128((__m128i*)buf, xm0);
    *(uint64_as*)dst = *(uint64_as*)buf;
    *(uint64_as*)(dst + pitch) = *(uint64_as*)(buf + 1);
    dst += pitch * 2;
    *(uint64_as*)dst = *(uint64_as*)(buf + 2);
    *(uint64_as*)(dst + pitch) = *(uint64_as*)(buf + 3);
    dst += pitch * 2;
    *(uint64_as*)dst = *(uint64_as*)(buf + 4);
    *(uint64_as*)(dst + pitch) = *(uint64_as*)(buf + 5);
    dst += pitch * 2;
    *(uint64_as*)dst = *(uint64_as*)(buf + 6);
    *(uint64_as*)(dst + pitch) = *(uint64_as*)(buf + 7);
#endif
}

// down-right 预测
void intra_pred_downright(uint8_t* dst, int pitch, NBUsable)
{
    const uint8_t* left = dst - 1;
    const uint8_t* top = dst - pitch;
    alignas(16) uint8_t buf[16];
    buf[0] = left[pitch * 7];
    buf[1] = left[pitch * 6];
    buf[2] = left[pitch * 5];
    buf[3] = left[pitch * 4];
    buf[4] = left[pitch * 3];
    buf[5] = left[pitch * 2];
    buf[6] = left[pitch];
    buf[7] = left[0];
    *(uint64_as*)(buf + 8) = *(uint64_as*)(top - 1);

    __m128i xm0, xm1, xm2, xm4;
    __m128i msk = _mm_setzero_si128();                      // 0 x 16
    msk = _mm_sub_epi8(msk, _mm_cmpeq_epi8(msk, msk));      // 1 x 16

    xm0 = _mm_load_si128((__m128i*)buf);
    xm1 = _mm_srli_si128(xm0, 1);
    xm2 = _mm_srli_si128(xm0, 2);
    xm2 = _mm_insert_epi16(xm2, top[7], 7);

    xm4 = _mm_xor_si128(xm0, xm2);
    xm0 = _mm_avg_epu8(xm0, xm2);
    xm2 = _mm_xor_si128(xm0, xm1);
    xm0 = _mm_avg_epu8(xm0, xm1);
    xm4 = _mm_and_si128(xm4, xm2);
    xm4 = _mm_and_si128(xm4, msk);
    xm0 = _mm_subs_epu8(xm0, xm4);

    // GCC 对 sse intrinsic 的支持欠佳, 没有生成最优指令
#ifndef __x86_64__
    _mm_storel_epi64((__m128i*)(dst + 7 * pitch), xm0);
    xm0 = _mm_srli_si128(xm0, 1);
    _mm_storel_epi64((__m128i*)(dst + 6 * pitch), xm0);
    xm0 = _mm_srli_si128(xm0, 1);
    _mm_storel_epi64((__m128i*)(dst + 5 * pitch), xm0);
    xm0 = _mm_srli_si128(xm0, 1);
    _mm_storel_epi64((__m128i*)(dst + 4 * pitch), xm0);
    xm0 = _mm_srli_si128(xm0, 1);
    _mm_storel_epi64((__m128i*)(dst + 3 * pitch), xm0);
    xm0 = _mm_srli_si128(xm0, 1);
    _mm_storel_epi64((__m128i*)(dst + 2 * pitch), xm0);
    xm0 = _mm_srli_si128(xm0, 1);
    _mm_storel_epi64((__m128i*)(dst + pitch), xm0);
    xm0 = _mm_srli_si128(xm0, 1);
    _mm_storel_epi64((__m128i*)dst, xm0);
#else
    _mm_store_si128((__m128i*)buf, xm0);
    *(uint64_as*)(dst) = *(uint64_as*)(buf + 7);
    *(uint64_as*)(dst + pitch) = *(uint64_as*)(buf + 6);
    *(uint64_as*)(dst + pitch * 2) = *(uint64_as*)(buf + 5);
    *(uint64_as*)(dst + pitch * 3) = *(uint64_as*)(buf + 4);
    dst += pitch * 4;
    *(uint64_as*)(dst) = *(uint64_as*)(buf + 3);
    *(uint64_as*)(dst + pitch) = *(uint64_as*)(buf + 2);
    *(uint64_as*)(dst + pitch * 2) = *(uint64_as*)(buf + 1);
    *(uint64_as*)(dst + pitch * 3) = *(uint64_as*)(buf);
#endif
}

// 色差分量 plane 预测
void intra_pred_plane(uint8_t* dst, int pitch, NBUsable)
{
    alignas(16) static int16_t s_HorFactor[8] = {-3, -2, -1, 0, 1, 2, 3, 4};

    const uint8_t* left = dst - 1;
    const uint8_t* top = dst - pitch;
    int iv = (left[pitch * 4] - left[pitch * 2]) + (left[pitch * 5] - left[pitch]) * 2 +
        (left[pitch * 6] - left[0]) * 3 + (left[pitch * 7] - left[0 - pitch]) * 4;
    int ia = ((top[7] + left[pitch * 7]) << 4) + 16;
    int ih = (top[4] - top[2]) + (top[5] - top[1]) * 2 +
        (top[6] - top[0]) * 3 + (top[7] - top[-1]) * 4;

    __m128i xia, xib, xic, xc3;
    iv = (iv * 17 + 16) >> 5;
    ih = (ih * 17 + 16) >> 5;
    xia = _mm_set1_epi16(ia);
    xib = _mm_set1_epi16(ih);
    xic = _mm_set1_epi16(iv);
    xib = _mm_mullo_epi16(xib, *(__m128i*)s_HorFactor);
    xc3 = _mm_add_epi16(xic, xic);
    xia = _mm_add_epi16(xia, xib);
    xc3 = _mm_add_epi16(xc3, xic);        // xic * 3
    xia = _mm_sub_epi16(xia, xc3);

    xib = _mm_srai_epi16(xia, 5);
    xia = _mm_add_epi16(xia, xic);
    _mm_storel_epi64((__m128i*)dst, _mm_packus_epi16(xib, xib));
    dst += pitch;
    xib = _mm_srai_epi16(xia, 5);
    xia = _mm_add_epi16(xia, xic);
    _mm_storel_epi64((__m128i*)dst, _mm_packus_epi16(xib, xib));
    dst += pitch;
    xib = _mm_srai_epi16(xia, 5);
    xia = _mm_add_epi16(xia, xic);
    _mm_storel_epi64((__m128i*)dst, _mm_packus_epi16(xib, xib));
    dst += pitch;
    xib = _mm_srai_epi16(xia, 5);
    xia = _mm_add_epi16(xia, xic);
    _mm_storel_epi64((__m128i*)dst, _mm_packus_epi16(xib, xib));
    dst += pitch;
    xib = _mm_srai_epi16(xia, 5);
    xia = _mm_add_epi16(xia, xic);
    _mm_storel_epi64((__m128i*)dst, _mm_packus_epi16(xib, xib));
    dst += pitch;
    xib = _mm_srai_epi16(xia, 5);
    xia = _mm_add_epi16(xia, xic);
    _mm_storel_epi64((__m128i*)dst, _mm_packus_epi16(xib, xib));
    dst += pitch;
    xib = _mm_srai_epi16(xia, 5);
    xia = _mm_add_epi16(xia, xic);
    _mm_storel_epi64((__m128i*)dst, _mm_packus_epi16(xib, xib));
    dst += pitch;
    xia = _mm_srai_epi16(xia, 5);
    _mm_storel_epi64((__m128i*)dst, _mm_packus_epi16(xia, xia));
}

}   // namespace irk_avs_dec
