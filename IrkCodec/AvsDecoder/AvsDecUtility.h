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

#ifndef _AVS_DECUTILITY_H_
#define _AVS_DECUTILITY_H_

#include <emmintrin.h>  // SSE-2
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef _MSC_VER
#pragma intrinsic( abs )
#elif defined(__GNUC__) || defined(__clang__)
#define abs __builtin_abs
#endif

#ifndef MIN
#define MIN(x, y) ((x) <= (y) ? (x) : (y))
#endif
#ifndef MAX
#define MAX(x, y) ((x) >= (y) ? (x) : (y))
#endif

// 进位到 16 和 32 的整数倍, 方便调用 SSE/AVX 指令
#define XmmPitch(x)     (((x)+15) & ~15)
#define YmmPitch(x)     (((x)+31) & ~31)

#if defined(_MSC_VER) && !defined(_M_X64)

__forceinline int clip3(int val, int minv, int maxv)
{
    __asm
    {
        mov     eax, val;
        mov     ecx, minv;
        cmp     eax, ecx;
        cmovl   eax, ecx;
        mov     ecx, maxv;
        cmp     eax, ecx;
        cmovg   eax, ecx;
    }
}

#elif defined(__GNUC__) || defined(__clang__)

inline int clip3(int val, int minv, int maxv)
{
    int result;
    __asm__ __volatile__
    (
        "cmp %2, %0 \n\t"
        "cmovl %2, %0 \n\t"
        "cmp %3, %0 \n\t"
        "cmovg %3, %0 \n\t"
        : "=r"(result)
        : "0"(val), "r"(minv), "r"(maxv)
    );
    return result;
}

#else

inline int clip3(int val, int minv, int maxv)
{
    if (val < minv)
        return minv;
    else if (val > maxv)
        return maxv;
    return val;
}

#endif

inline void zero_aligned(void* dst, uint32_t size)
{
    assert(((uintptr_t)dst & 15) == 0);
    assert((size & 15) == 0);
    size >>= 4;
    __m128i* d16 = (__m128i*)dst;
    __m128i xmz = _mm_setzero_si128();
    for (uint32_t i = 0; i < size; i++)
        _mm_store_si128((d16 + i), xmz);
}

inline void copy_aligned(void* dst, const void* src, uint32_t size)
{
    assert(((uintptr_t)dst & 15) == 0 && ((uintptr_t)src & 15) == 0);
    assert((size & 15) == 0);
    size >>= 4;
    __m128i* d16 = (__m128i*)dst;
    __m128i* s16 = (__m128i*)src;
    for (uint32_t i = 0; i < size; i++)
    {
        _mm_store_si128(d16 + i, _mm_load_si128(s16 + i));
    }
}

inline void zero_block8x8(int16_t* dst)
{
    assert(((uintptr_t)dst & 15) == 0);
    __m128i xmz = _mm_setzero_si128();
    _mm_store_si128((__m128i*)(dst + 8 * 0), xmz);
    _mm_store_si128((__m128i*)(dst + 8 * 1), xmz);
    _mm_store_si128((__m128i*)(dst + 8 * 2), xmz);
    _mm_store_si128((__m128i*)(dst + 8 * 3), xmz);
    _mm_store_si128((__m128i*)(dst + 8 * 4), xmz);
    _mm_store_si128((__m128i*)(dst + 8 * 5), xmz);
    _mm_store_si128((__m128i*)(dst + 8 * 6), xmz);
    _mm_store_si128((__m128i*)(dst + 8 * 7), xmz);
}

#endif
