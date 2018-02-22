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

#ifndef _IRONBRICK_ATOMIC_H_
#define _IRONBRICK_ATOMIC_H_

/* NOTE:
*  In C++11 standard operands in atomic functions must be the type of atomic<T>.
*  This header defines atomic operations on fundamental types.
*/

#include "IrkCommon.h"

#ifdef _MSC_VER

extern "C"
{
    long _InterlockedExchange(long volatile*, long);
    long _InterlockedExchangeAdd(long volatile*, long);
    long _InterlockedIncrement(long volatile*);
    long _InterlockedDecrement(long volatile*);
    long _InterlockedAnd(long volatile*, long);
    long _InterlockedOr(long volatile*, long);
    long _InterlockedXor(long volatile*, long);
    long _InterlockedCompareExchange(long volatile*, long, long);
    __int64 _InterlockedCompareExchange64(__int64 volatile*, __int64, __int64);
    void* _InterlockedCompareExchangePointer(void* volatile*, void*, void*);
    void _ReadWriteBarrier(void);
    void _mm_mfence(void);
    void _mm_pause(void);
}
#pragma intrinsic( _InterlockedExchange )
#pragma intrinsic( _InterlockedExchangeAdd )
#pragma intrinsic( _InterlockedAnd )
#pragma intrinsic( _InterlockedOr )
#pragma intrinsic( _InterlockedXor )
#pragma intrinsic( _InterlockedIncrement )
#pragma intrinsic( _InterlockedDecrement )
#pragma intrinsic( _InterlockedCompareExchange )
#pragma intrinsic( _InterlockedCompareExchange64 )
#pragma intrinsic( _InterlockedCompareExchangePointer )
#pragma intrinsic( _ReadWriteBarrier )
#pragma intrinsic( _mm_pause )
#pragma intrinsic( _mm_mfence )

#ifdef _M_X64

extern "C"
{
    __int64 _InterlockedExchange64(__int64 volatile*, __int64);
    __int64 _InterlockedExchangeAdd64(__int64 volatile*, __int64);
    __int64 _InterlockedIncrement64(__int64 volatile*);
    __int64 _InterlockedDecrement64(__int64 volatile*);
    __int64 _InterlockedAnd64(__int64 volatile*, __int64);
    __int64 _InterlockedOr64(__int64 volatile*, __int64);
    __int64 _InterlockedXor64(__int64 volatile*, __int64);
}
#pragma intrinsic( _InterlockedExchange64 )
#pragma intrinsic( _InterlockedExchangeAdd64 )
#pragma intrinsic( _InterlockedIncrement64 )
#pragma intrinsic( _InterlockedDecrement64 )
#pragma intrinsic( _InterlockedAnd64 )
#pragma intrinsic( _InterlockedOr64 )
#pragma intrinsic( _InterlockedXor64 )

#endif  // #ifdef _M_X64

// full fence
#define atomic_full_fence()         do{ _ReadWriteBarrier(); _mm_mfence(); }while(0)
// compiler fence
#define atomic_compiler_fence()     _ReadWriteBarrier()
// CPU pause
#define atomic_cpu_pause()          _mm_pause()

//======================================================================================================================
namespace irk {

__forceinline int atomic_load(const volatile int* pData)
{
    int value = *pData;
    atomic_compiler_fence();
    return value;
}

__forceinline int64_t atomic_load(const volatile int64_t* pData)
{
#ifdef _M_X64
    int64_t value = *pData;
    atomic_compiler_fence();
    return value;
#else
    return _InterlockedCompareExchange64((volatile int64_t*)pData, 0, 0);
#endif
}

__forceinline void* atomic_load(void* volatile* pPtr)
{
    void* ptr = *pPtr;
    atomic_compiler_fence();
    return ptr;
}


/*************************************************************************************************
MSDN :
When using __asm to write assembly language in C/C++ functions,
you don't need to preservethe EAX, EBX, ECX, EDX, ESI, or EDI registers.
However, ebx may be used by the compiler to keep track of aligned variables, so we have to save it.
warning C4731: frame pointer register 'ebx' modified by assembly code.
Since we have save 'ebx', so we can disable this warnig
**************************************************************************************************/
#pragma warning( push )
#pragma warning( disable : 4731 )

__forceinline void atomic_store(volatile int* pData, int value)
{
    _InterlockedExchange((long volatile*)pData, value);
}

__forceinline void atomic_store(volatile int64_t* pData, int64_t value)
{
#ifdef _M_X64
    _InterlockedExchange64(pData, value);
#else
    int lowv = (int)(value & 0xFFFFFFFF);
    int highv = (int)(value >> 32);
    __asm
    {
        push edi
        push ebx
        mov edi, pData
        mov ebx, lowv
        mov ecx, highv
        mov eax, dword ptr[edi]
        mov edx, dword ptr[edi + 4]
        atomic_asm_loop_flag__:
        lock cmpxchg8b qword ptr[edi]
            jnz atomic_asm_loop_flag__
            pop ebx
            pop edi
    }
    _ReadWriteBarrier();
#endif
}

__forceinline void atomic_store(void* volatile *pPtr, void* ptr)
{
#ifdef _M_X64
    _InterlockedExchange64((int64_t volatile*)pPtr, (int64_t)ptr);
#else
    _InterlockedExchange((long volatile*)pPtr, (long)ptr);
#endif
}

__forceinline int atomic_exchange(volatile int* pData, int value)
{
    return _InterlockedExchange((long volatile*)pData, value);
}

__forceinline int64_t atomic_exchange(volatile int64_t* pData, int64_t value)
{
#ifdef _M_X64
    return _InterlockedExchange64(pData, value);
#else
    int lowv = (int)(value & 0xFFFFFFFF);
    int highv = (int)(value >> 32);
    __asm
    {
        push edi
        push ebx
        mov edi, pData
        mov ebx, lowv
        mov ecx, highv
        mov eax, dword ptr[edi]
        mov edx, dword ptr[edi + 4]
        atomic_asm_loop_flag__:
        lock cmpxchg8b qword ptr[edi]
            jnz atomic_asm_loop_flag__
            pop ebx
            pop edi
    }
    _ReadWriteBarrier();
#endif
}

__forceinline void* atomic_exchange(void* volatile* pPtr, void* ptr)
{
#ifdef _M_X64
    return (void*)_InterlockedExchange64((volatile __int64*)pPtr, (__int64)ptr);
#else
    return (void*)_InterlockedExchange((volatile long*)pPtr, (long)ptr);
#endif
}

__forceinline bool atomic_compare_exchange(volatile int* pData, int* pOldVal, int newVal)
{
    int oldVal = *pOldVal;      // must copy to local variable
    int curVal = _InterlockedCompareExchange((volatile long*)pData, newVal, oldVal);
    if (curVal != oldVal)      // failed
    {
        *pOldVal = curVal;
        return false;
    }
    return true;
}
__forceinline int atomic_compare_swap(volatile int* pData, int oldVal, int newVal)
{
    // return current value
    return _InterlockedCompareExchange((volatile long*)pData, newVal, oldVal);
}

__forceinline bool atomic_compare_exchange(volatile int64_t* pData, int64_t* pOldVal, int64_t newVal)
{
    int64_t oldVal = *pOldVal;  // must copy to local variable
    int64_t curVal = _InterlockedCompareExchange64(pData, newVal, oldVal);
    if (curVal != oldVal)      // failed
    {
        *pOldVal = curVal;
        return false;
    }
    return true;
}
__forceinline int64_t atomic_compare_swap(volatile int64_t* pData, int64_t oldVal, int newVal)
{
    // return current value
    return _InterlockedCompareExchange64(pData, newVal, oldVal);
}

__forceinline bool atomic_compare_exchange(void* volatile* pPtr, void** pOldPtr, void* newPtr)
{
    void* oldptr = *pOldPtr;    // must copy to local variable
    void* curptr = _InterlockedCompareExchangePointer(pPtr, newPtr, oldptr);
    if (oldptr != curptr)
    {
        *pOldPtr = curptr;
        return false;
    }
    return true;
}
__forceinline void* atomic_compare_swap(void* volatile* pPtr, void* oldPtr, void* newPtr)
{
    // return current value
    return _InterlockedCompareExchangePointer(pPtr, newPtr, oldPtr);
}

__forceinline int atomic_fetch_add(int volatile* pData, int addend)
{
    return _InterlockedExchangeAdd((long volatile*)pData, addend);
}

__forceinline int64_t atomic_fetch_add(int64_t volatile* pData, int64_t addend)
{
#ifdef _M_X64
    return _InterlockedExchangeAdd64(pData, addend);
#else
    int lowv = (int)(addend & 0xFFFFFFFF);
    int highv = (int)(addend >> 32);
    __asm
    {
        push edi
        push ebx
        mov edi, pData
        mov eax, dword ptr[edi]
        mov edx, dword ptr[edi + 4]
        atomic_asm_loop_flag__:
        mov ebx, lowv
            mov ecx, highv
            add ebx, eax
            adc ecx, edx
            lock cmpxchg8b qword ptr[edi]
            jnz atomic_asm_loop_flag__
            pop ebx
            pop edi
    }
    _ReadWriteBarrier();
#endif
}

__forceinline int atomic_fetch_sub(int volatile* pData, int subtrahend)
{
    return _InterlockedExchangeAdd((long volatile*)pData, -subtrahend);
}

__forceinline int64_t atomic_fetch_sub(int64_t volatile* pData, int64_t subtrahend)
{
    return atomic_fetch_add(pData, -subtrahend);
}

__forceinline int atomic_fetch_and(int volatile* pData, int mask)
{
    return _InterlockedAnd((long volatile*)pData, mask);
}

__forceinline int64_t atomic_fetch_and(int64_t volatile* pData, int64_t mask)
{
#ifdef _M_X64
    return _InterlockedAnd64(pData, mask);
#else
    int lowv = (int)(mask & 0xFFFFFFFF);
    int highv = (int)(mask >> 32);
    __asm
    {
        push edi
        push ebx
        mov edi, pData
        mov eax, dword ptr[edi]
        mov edx, dword ptr[edi + 4]
        atomic_asm_loop_flag__:
        mov ebx, lowv
            mov ecx, highv
            and ebx, eax
            and ecx, edx
            lock cmpxchg8b qword ptr[edi]
            jnz atomic_asm_loop_flag__
            pop ebx
            pop edi
    }
    _ReadWriteBarrier();
#endif
}

__forceinline int atomic_fetch_or(int volatile* pData, int mask)
{
    return _InterlockedOr((long volatile*)pData, mask);
}

__forceinline int64_t atomic_fetch_or(int64_t volatile* pData, int64_t mask)
{
#ifdef _M_X64
    return _InterlockedOr64(pData, mask);
#else
    int lowv = (int)(mask & 0xFFFFFFFF);
    int highv = (int)(mask >> 32);
    __asm
    {
        push edi
        push ebx
        mov edi, pData
        mov eax, dword ptr[edi]
        mov edx, dword ptr[edi + 4]
        atomic_asm_loop_flag__:
        mov ebx, lowv
            mov ecx, highv
            or ebx, eax
            or ecx, edx
            lock cmpxchg8b qword ptr[edi]
            jnz atomic_asm_loop_flag__
            pop ebx
            pop edi
    }
    _ReadWriteBarrier();
#endif
}

__forceinline int atomic_fetch_xor(int volatile* pData, int mask)
{
    return _InterlockedXor((long volatile*)pData, mask);
}

__forceinline int64_t atomic_fetch_xor(int64_t volatile* pData, int64_t mask)
{
#ifdef _M_X64
    return _InterlockedXor64(pData, mask);
#else
    int lowv = (int)(mask & 0xFFFFFFFF);
    int highv = (int)(mask >> 32);
    __asm
    {
        push edi
        push ebx
        mov edi, pData
        mov eax, dword ptr[edi]
        mov edx, dword ptr[edi + 4]
        atomic_asm_loop_flag__:
        mov ebx, lowv
            mov ecx, highv
            xor ebx, eax
            xor ecx, edx
            lock cmpxchg8b qword ptr[edi]
            jnz atomic_asm_loop_flag__
            pop ebx
            pop edi
    }
    _ReadWriteBarrier();
#endif
}

__forceinline int atomic_inc(int volatile* pData)
{
    return _InterlockedIncrement((long volatile*)pData);
}

__forceinline int64_t atomic_inc(int64_t volatile* pData)
{
#ifdef _M_X64
    return _InterlockedIncrement64(pData);
#else
    return atomic_fetch_add(pData, 1) + 1;
#endif
}

__forceinline int atomic_dec(int volatile* pData)
{
    return _InterlockedDecrement((long volatile*)pData);
}

__forceinline int64_t atomic_dec(int64_t volatile* pData)
{
#ifdef _M_X64
    return _InterlockedDecrement64(pData);
#else
    return atomic_fetch_add(pData, -1) - 1;
#endif
}

__forceinline uint8_t atomic_bit_fetch_set(int volatile* pData, int bit)
{
#if defined(_M_X64)
    long mask = (1 << bit);
    long old = _InterlockedOr((long volatile*)pData, mask);
    return (uint8_t)(old >> bit) & 1;
#else
    __asm
    {
        mov eax, bit
        mov edx, pData
        lock bts dword ptr[edx], eax
        setc al
    }
    _ReadWriteBarrier();
#endif
}

__forceinline uint8_t atomic_bit_fetch_set(int64_t volatile* pData, int bit)
{
    int64_t mask = (1LL << bit);
    int64_t old = atomic_fetch_or(pData, mask);
    return (uint8_t)(old >> bit) & 1;
}

__forceinline uint8_t atomic_bit_fetch_reset(int volatile* pData, int bit)
{
#if defined(_M_X64)
    long mask = (1 << bit);
    long old = _InterlockedAnd((long volatile*)pData, ~mask);
    return (uint8_t)(old >> bit) & 1;
#else
    __asm
    {
        mov eax, bit;
        mov edx, pData;
        lock btr dword ptr[edx], eax;
        setc al;
    }
    _ReadWriteBarrier();
#endif
}

__forceinline uint8_t atomic_bit_fetch_reset(int64_t volatile* pData, int bit)
{
    int64_t mask = (1LL << bit);
    int64_t old = atomic_fetch_and(pData, ~mask);
    return (uint8_t)(old >> bit) & 1;
}

__forceinline uint8_t atomic_bit_fetch_complement(int volatile* pData, int bit)
{
#if defined(_M_X64)
    long mask = (1 << bit);
    long old = _InterlockedXor((long volatile*)pData, mask);
    return (uint8_t)(old >> bit) & 1;
#else
    __asm
    {
        mov eax, bit
        mov edx, pData
        lock btc dword ptr[edx], eax
        setc al
    }
    _ReadWriteBarrier();
#endif
}

__forceinline uint8_t atomic_bit_fetch_complement(int64_t volatile* pData, int bit)
{
    int64_t mask = (1LL << bit);
    int64_t old = atomic_fetch_xor(pData, mask);
    return (uint8_t)(old >> bit) & 1;
}

#pragma warning( pop )
} // namespace irk

//======================================================================================================================
#elif defined(__GNUC__)  || defined(__clang__)

// full fence
#define atomic_full_fence()         __asm__ __volatile__("mfence": : :"memory")
// compiler fence
#define atomic_compiler_fence()     __asm__ __volatile__("": : :"memory")
// CPU pause
#define atomic_cpu_pause()          __asm__ __volatile__("pause")

namespace irk {

inline __attribute__((always_inline))
int atomic_load(const volatile int* pData)
{
    int value = *pData;
    atomic_compiler_fence();
    return value;
}

inline __attribute__((always_inline))
int64_t atomic_load(const volatile int64_t* pData)
{
#ifdef __x86_64__
    int64_t value = *pData;
    atomic_compiler_fence();
    return value;
#else
    return __sync_val_compare_and_swap((int64_t*)pData, 0, 0);
#endif
}

inline __attribute__((always_inline))
void* atomic_load(void* volatile* pPtr)
{
    void* ptr = *pPtr;
    atomic_compiler_fence();
    return ptr;
}

inline __attribute__((always_inline))
void atomic_store(volatile int* pData, int value)
{
    __sync_synchronize();
    *pData = value;
}

inline __attribute__((always_inline))
void atomic_store(volatile int64_t* pData, int64_t value)
{
#ifdef __x86_64__
    __sync_synchronize();
    *pData = value;
#else
    int64_t curVal = *pData;
    while (!__sync_bool_compare_and_swap(pData, curVal, value))
        curVal = *pData;
#endif
}

inline __attribute__((always_inline))
void atomic_store(void* volatile *pPtr, void* ptr)
{
    __sync_synchronize();
    *pPtr = ptr;
}

inline __attribute__((always_inline))
int atomic_exchange(volatile int* pData, int value)
{
    int oldVal;
    __asm__ __volatile__
    (
        "lock\n\t xchg %0, %1"
        : "=r"(oldVal), "=m"(*pData)
        : "0"(value), "m"(*pData)
        : "memory"
    );
    return oldVal;
}

inline __attribute__((always_inline))
int64_t atomic_exchange(volatile int64_t* pData, int64_t value)
{
    int64_t oldVal;
#ifdef __x86_64__
    __asm__ __volatile__
    (
        "lock\n\t xchg %0, %1"
        : "=r"(oldVal), "=m"(*pData)
        : "0"(value), "m"(*pData)
        : "memory"
    );
#else
    oldVal = *pData;
    while (!__sync_bool_compare_and_swap(pData, oldVal, value))
        oldVal = *pData;
#endif
    return oldVal;
}

inline __attribute__((always_inline))
void* atomic_exchange(void* volatile* pPtr, void* ptr)
{
#ifdef __x86_64__
    return (void*)atomic_exchange((volatile int64_t*)pPtr, (int64_t)ptr);
#else
    return (void*)atomic_exchange((volatile int*)pPtr, (int)ptr);
#endif
}

inline __attribute__((always_inline))
bool atomic_compare_exchange(volatile int* pData, int* pOldVal, int newVal)
{
    int oldVal = *pOldVal;      // must copy to local variable
    int curVal = __sync_val_compare_and_swap(pData, oldVal, newVal);
    if (curVal != oldVal)      // failed
    {
        *pOldVal = curVal;
        return false;
    }
    return true;
}
inline __attribute__((always_inline))
int atomic_compare_swap(volatile int* pData, int oldVal, int newVal)
{
    return __sync_val_compare_and_swap(pData, oldVal, newVal);
}

inline __attribute__((always_inline))
bool atomic_compare_exchange(volatile int64_t* pData, int64_t* pOldVal, int64_t newVal)
{
    int64_t oldVal = *pOldVal;  // must copy to local variable
    int64_t curVal = __sync_val_compare_and_swap(pData, oldVal, newVal);
    if (curVal != oldVal)      // failed
    {
        *pOldVal = curVal;
        return false;
    }
    return true;
}
inline __attribute__((always_inline))
int64_t atomic_compare_swap(volatile int64_t* pData, int64_t oldVal, int newVal)
{
    return __sync_val_compare_and_swap(pData, oldVal, newVal);
}

inline __attribute__((always_inline))
bool atomic_compare_exchange(void* volatile* pPtr, void** pOldPtr, void* newPtr)
{
#ifdef __x86_64__
    return atomic_compare_exchange((volatile int64_t*)pPtr, (int64_t*)pOldPtr, (int64_t)newPtr);
#else
    return atomic_compare_exchange((volatile int*)pPtr, (int*)pOldPtr, (int)newPtr);
#endif
}
inline __attribute__((always_inline))
void* atomic_compare_swap(void* volatile* pPtr, void* oldPtr, void* newPtr)
{
#ifdef __x86_64__
    return (void*)atomic_compare_swap((volatile int64_t*)pPtr, (int64_t)oldPtr, (int64_t)newPtr);
#else
    return (void*)atomic_compare_swap((volatile int*)pPtr, (int)oldPtr, (int)newPtr);
#endif
}

inline __attribute__((always_inline))
int atomic_fetch_add(int volatile* pData, int addend)
{
    return __sync_fetch_and_add(pData, addend);
}

inline __attribute__((always_inline))
int64_t atomic_fetch_add(int64_t volatile* pData, int64_t addend)
{
    return __sync_fetch_and_add(pData, addend);
}

inline __attribute__((always_inline))
int atomic_fetch_sub(int volatile* pData, int addend)
{
    return __sync_fetch_and_sub(pData, addend);
}

inline __attribute__((always_inline))
int64_t atomic_fetch_sub(int64_t volatile* pData, int64_t addend)
{
    return __sync_fetch_and_sub(pData, addend);
}

inline __attribute__((always_inline))
int atomic_fetch_and(int volatile* pData, int mask)
{
    return __sync_fetch_and_and(pData, mask);
}

inline __attribute__((always_inline))
int64_t atomic_fetch_and(int64_t volatile* pData, int64_t mask)
{
    return __sync_fetch_and_and(pData, mask);
}

inline __attribute__((always_inline))
int atomic_fetch_or(int volatile* pData, int mask)
{
    return __sync_fetch_and_or(pData, mask);
}

inline __attribute__((always_inline))
int64_t atomic_fetch_or(int64_t volatile* pData, int64_t mask)
{
    return __sync_fetch_and_or(pData, mask);
}

inline __attribute__((always_inline))
int atomic_fetch_xor(int volatile* pData, int mask)
{
    return __sync_fetch_and_xor(pData, mask);
}

inline __attribute__((always_inline))
int64_t atomic_fetch_xor(int64_t volatile* pData, int64_t mask)
{
    return __sync_fetch_and_xor(pData, mask);
}

inline __attribute__((always_inline))
int atomic_inc(int volatile* pData)
{
    return __sync_add_and_fetch(pData, 1);
}

inline __attribute__((always_inline))
int64_t atomic_inc(int64_t volatile* pData)
{
    return __sync_add_and_fetch(pData, 1);
}

inline __attribute__((always_inline))
int atomic_dec(int volatile* pData)
{
    return __sync_sub_and_fetch(pData, 1);
}

inline __attribute__((always_inline))
int64_t atomic_dec(int64_t volatile* pData)
{
    return __sync_sub_and_fetch(pData, 1);
}

inline __attribute__((always_inline))
uint8_t atomic_bit_fetch_set(int volatile* pData, int bit)
{
    uint8_t res;
    __asm__ __volatile__
    (
        "lock\n\t bts %2, %1\n\t"
        "setc %0"
        : "=q"(res), "+m"(*pData)
        : "r"(bit)
        : "memory", "cc"
    );
    return res;
}

inline __attribute__((always_inline))
uint8_t atomic_bit_fetch_set(int64_t volatile* pData, int bit)
{
#ifdef __x86_64__
    uint8_t res;
    __asm__ __volatile__
    (
        "lock\n\t bts %2, %1\n\t"
        "setc %0"
        : "=q"(res), "+m"(*pData)
        : "r"(bit)
        : "memory", "cc"
    );
    return res;
#else
    int64_t mask = (1LL << bit);
    int64_t old = atomic_fetch_or(pData, mask);
    return (uint8_t)(old >> bit) & 1;
#endif
}

inline __attribute__((always_inline))
uint8_t atomic_bit_fetch_reset(int volatile* pData, int bit)
{
    uint8_t res;
    __asm__ __volatile__
    (
        "lock\n\t btr %2, %1\n\t"
        "setc %0"
        : "=q"(res), "+m"(*pData)
        : "r"(bit)
        : "memory", "cc"
    );
    return res;
}

inline __attribute__((always_inline))
uint8_t atomic_bit_fetch_reset(int64_t volatile* pData, int bit)
{
#ifdef __x86_64__
    uint8_t res;
    __asm__ __volatile__
    (
        "lock\n\t btr %2, %1\n\t"
        "setc %0"
        : "=q"(res), "+m"(*pData)
        : "r"(bit)
        : "memory", "cc"
    );
    return res;
#else
    int64_t mask = (1LL << bit);
    int64_t old = atomic_fetch_and(pData, ~mask);
    return (uint8_t)(old >> bit) & 1;
#endif
}

inline __attribute__((always_inline))
uint8_t atomic_bit_fetch_complement(int volatile* pData, int bit)
{
    uint8_t res;
    __asm__ __volatile__
    (
        "lock\n\t btc %2, %1\n\t"
        "setc %0"
        : "=q"(res), "+m"(*pData)
        : "r"(bit)
        : "memory", "cc"
    );
    return res;
}

inline __attribute__((always_inline))
uint8_t atomic_bit_fetch_complement(int64_t volatile* pData, int bit)
{
#ifdef __x86_64__
    uint8_t res;
    __asm__ __volatile__
    (
        "lock\n\t btc %2, %1\n\t"
        "setc %0"
        : "=q"(res), "+m"(*pData)
        : "r"(bit)
        : "memory", "cc"
    );
    return res;
#else
    int64_t mask = (1LL << bit);
    int64_t old = atomic_fetch_xor(pData, mask);
    return (uint8_t)(old >> bit) & 1;
#endif
}

} // namespace irk

#else
#error unsupport compiler
#endif

#endif
