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

#ifndef _IRONBRICK_MEMUTILITY_H_
#define _IRONBRICK_MEMUTILITY_H_

#if defined(_MSC_VER) || defined(__linux__)
#include <malloc.h>
#elif defined(__MACH__)
#include <malloc/malloc.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <new>
#include <memory>
#include <type_traits>
#include "IrkCommon.h"

#ifdef _MSC_VER
extern "C" void __cdecl __stosd(unsigned long*, unsigned long, size_t);
extern "C" void __cdecl __stosw(unsigned short*, unsigned short, size_t);
extern "C" void __cdecl __stosb(unsigned char*, unsigned char, size_t);
#pragma intrinsic( __stosd, __stosw, __stosb )
#pragma intrinsic( memcpy, memset, strlen, strcmp )
#endif

#ifndef MAX
#define MAX(x, y)   ((x) > (y) ? (x) : (y))
#endif
#ifndef MIN
#define MIN(x, y)   ((x) < (y) ? (x) : (y))
#endif

namespace irk {

//======================================================================================================================
// similar to c11 aligned_alloc, but throw bad_alloc if allocation failed

#ifdef _WIN32

__forceinline void* aligned_malloc(size_t size, size_t alignment)
{
    assert(size > 0);
    assert((alignment & (alignment - 1)) == 0);    // power of 2
    void* pbuf = ::_aligned_malloc(size, alignment);
    if (!pbuf)
        throw std::bad_alloc();
    return pbuf;
}

__forceinline void aligned_free(void* ptr)
{
    ::_aligned_free(ptr);
}

#else

IRK_FCINLINE void* aligned_malloc(size_t size, size_t alignment)
{
    assert(size > 0);
    assert((alignment & (alignment - 1)) == 0);   // power of 2
    if (alignment < sizeof(void*))
        alignment = sizeof(void*);
    void* pbuf = nullptr;
    if (::posix_memalign(&pbuf, alignment, size) != 0)
        throw std::bad_alloc();
    return pbuf;
}

IRK_FCINLINE void aligned_free(void* ptr)
{
    ::free(ptr);
}

#endif

// similar to malloc, but throw bad_alloc if allocation failed
IRK_FCINLINE void* checked_malloc(size_t size)
{
    void* pbuf = ::malloc(size);
    if (!pbuf)
        throw std::bad_alloc();
    return pbuf;
}

// similar to calloc, but throw bad_alloc if allocation failed
IRK_FCINLINE void* checked_calloc(size_t num, size_t size)
{
    void* pbuf = ::calloc(num, size);
    if (!pbuf)
        throw std::bad_alloc();
    return pbuf;
}

// similar to realloc, but throw bad_alloc if allocation failed
IRK_FCINLINE void* checked_realloc(void* ptr, size_t size)
{
    void* pbuf = ::realloc(ptr, size);
    if (!pbuf && size > 0)
        throw std::bad_alloc();
    return pbuf;
}

//======================================================================================================================
// simple utility to make sure memory allocated be freed, similar to std::unique_ptr

class AlignedBuf : IrkNocopy
{
public:
    explicit AlignedBuf(void* pbuf = nullptr) : m_Buf(pbuf) {}  // manage existing buffer, take ownership
    explicit AlignedBuf(size_t size, size_t alignment)            // create new buffer
    {
        m_Buf = aligned_malloc(size, alignment);
    }
    ~AlignedBuf()
    {
        aligned_free(m_Buf);
    }
    AlignedBuf(AlignedBuf&& other) noexcept   // move constructor
    {
        m_Buf = other.m_Buf;
        other.m_Buf = nullptr;
    }
    AlignedBuf& operator=(AlignedBuf&& other) noexcept    // move assignment
    {
        if (this != &other)
        {
            this->rebind(other.m_Buf);
            other.m_Buf = nullptr;
        }
        return *this;
    }

    void* alloc(size_t size, size_t alignment)  // create new buffer
    {
        if (m_Buf)     // free old buffer
        {
            aligned_free(m_Buf);
            m_Buf = nullptr;
        }
        m_Buf = aligned_malloc(size, alignment);
        return m_Buf;
    }
    void* buffer()          // view underlying buffer, but still own it
    {
        return m_Buf;
    }
    void* detach()          // hand out buffer's ownership
    {
        void* pbuf = m_Buf;
        m_Buf = nullptr;
        return pbuf;
    }
    void rebind(void* pbuf = nullptr) // manage another buffer
    {
        if (m_Buf != pbuf)
        {
            aligned_free(m_Buf);
            m_Buf = pbuf;
        }
    }
private:
    void* m_Buf;
};

class MallocedBuf : IrkNocopy
{
public:
    explicit MallocedBuf(void* pbuf = nullptr) : m_Buf(pbuf) {} // manage existing buffer, take ownership
    explicit MallocedBuf(size_t size) // create new buffer
    {
        m_Buf = checked_malloc(size);
    }
    ~MallocedBuf()
    {
        ::free(m_Buf);
    }
    MallocedBuf(MallocedBuf&& other) noexcept // move constructor
    {
        m_Buf = other.m_Buf;
        other.m_Buf = nullptr;
    }
    MallocedBuf& operator=(MallocedBuf&& other) noexcept  // move assignment
    {
        if (this != &other)
        {
            this->rebind(other.m_Buf);
            other.m_Buf = nullptr;
        }
        return *this;
    }

    void* alloc(size_t size)      // create new buffer
    {
        if (m_Buf)
        {
#ifdef _MSC_VER
            if (_msize(m_Buf) >= size)
                return m_Buf;
#elif defined(__linux__)
            if (malloc_usable_size(m_Buf) >= size)
                return m_Buf;
#elif defined(__MACH__)
            if (malloc_size(m_Buf) >= size)
                return m_Buf;
#endif
            ::free(m_Buf);        // free old buffer
            m_Buf = nullptr;
        }
        m_Buf = checked_malloc(size);
        return m_Buf;
    }
    void* calloc(size_t num, size_t size)    // create new buffer, and memset to 0
    {
        if (m_Buf)     // free old buffer
        {
            ::free(m_Buf);
            m_Buf = nullptr;
        }
        m_Buf = checked_calloc(num, size);
        return m_Buf;
    }
    void* realloc(size_t size)    // realloc buffer
    {
        m_Buf = checked_realloc(m_Buf, size);
        return m_Buf;
    }
    void* buffer()                  // view underlying buffer, but still own it
    {
        return m_Buf;
    }
    void* detach()                  // hand out buffer's ownership
    {
        void* pbuf = m_Buf;
        m_Buf = nullptr;
        return pbuf;
    }
    void rebind(void* pbuf = nullptr) // manage another buffer
    {
        if (m_Buf != pbuf)
        {
            ::free(m_Buf);
            m_Buf = pbuf;
        }
    }
private:
    void* m_Buf;
};

//======================================================================================================================

template<typename Ty, typename Dx>
inline auto make_unique_resource(Ty* obj, Dx&& deleter)
{
    return std::unique_ptr<Ty, std::decay_t<Dx>>(obj, std::forward<Dx>(deleter));
}

// the same as std::make_unique<Ty[]>( count )
template<class Ty>
inline auto make_unique_array(size_t count)
{
    return std::unique_ptr<Ty[]>(new Ty[count]{});
}

// this version will leave POD array uninitialized
template<class Ty>
inline auto make_unique_raw_array(size_t count)
{
    return std::unique_ptr<Ty[]>(new Ty[count]);
}

template<typename Ty, typename Dx>
inline auto make_shared_resource(Ty* obj, Dx&& deleter)
{
    return std::shared_ptr<Ty>(obj, std::forward<Dx>(deleter));
}

// std::make_shared for array
template<class Ty>
inline auto make_shared_array(size_t count)
{
    return std::shared_ptr<Ty>(new Ty[count]{}, std::default_delete<Ty[]>());
}

// this version will leave POD array uninitialized
template<class Ty>
inline auto make_shared_raw_array(size_t count)
{
    return std::shared_ptr<Ty>(new Ty[count], std::default_delete<Ty[]>());
}

//======================================================================================================================
// extended memset

#if defined(_MSC_VER)

// count : number of doublewords to write
__forceinline void memset32(void* dst, uint32_t value, size_t count)
{
    __stosd((unsigned long*)dst, value, count);
}

// count : number of words to write
__forceinline void memset16(void* dst, uint16_t value, size_t count)
{
    __stosw((unsigned short*)dst, value, count);
}

// secure zero memory
__forceinline void secure_bzeros(void* dst, size_t size)
{
    volatile uint8_t* vptr = (volatile uint8_t*)dst;
    __stosb((uint8_t*)vptr, 0, size);
}

#elif defined(__GNUC__) || defined(__clang__)

// NOTE: GCC assure destination flag is cleared befor inline assembly

// count : number of doublewords to write
IRK_FCINLINE void memset32(void* dst, uint32_t value, size_t count)
{
    uint32_t* dd = (uint32_t*)dst;
    size_t cc = count;
    __asm__ __volatile__("rep; stosl \n\t"
        : "+c"(cc), "+D"(dd)
        : "a"(value)
        : "memory");
}

// count : number of words to write
IRK_FCINLINE void memset16(void* dst, uint16_t value, size_t count)
{
    uint16_t* dd = (uint16_t*)dst;
    size_t cc = count;
    __asm__ __volatile__("rep; stosw \n\t"
        : "+c"(cc), "+D"(dd)
        : "a"((uint32_t)(value))
        : "memory");
}

// secure zero memory
IRK_FCINLINE void secure_bzeros(void* dst, size_t size)
{
    uint8_t* dd = (uint8_t*)(dst);
    size_t cc = size;
    __asm__ __volatile__("rep; stosb \n\t"
        : "+c"(cc), "+D"(dd) : "a"(0)
        : "memory");
}

#else

// count : number of doublewords to write
inline void memset32(void* dst, uint32_t value, size_t count)
{
    uint32_t* dd = (uint32_t*)dst;
    for (size_t i = 0; i < count; i++)
        dd[i] = value;
}

// count : number of words to write
inline void memset16(void* dst, uint16_t value, size_t count)
{
    uint16_t* dd = (uint16_t*)dst;
    for (size_t i = 0; i < count; i++)
        dd[i] = value;
}

// secure zero memory
inline void secure_bzeros(void* dst, size_t size)
{
    volatile uint8_t* vptr = (volatile uint8_t*)dst;
    for (size_t i = 0; i < size; i++)
        vptr[i] = 0;
}

#endif

// memset trivial object to zeros
template<typename T>
IRK_FCINLINE void bzeros(T* obj)
{
    static_assert(std::is_trivially_copyable<T>::value, "requires trivially copyable type");
    static_assert(sizeof(T) > 0, "requires complete type");
    ::memset(obj, 0, sizeof(T));
}

// get item count of the array
template<typename T, size_t N>
constexpr size_t countof(const volatile T(&)[N])
{
    return N;
}

}   // namespace irk
#endif
