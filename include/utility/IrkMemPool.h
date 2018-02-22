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

#ifndef _IRONBRICK_MEMPOOL_H_
#define _IRONBRICK_MEMPOOL_H_

#include "IrkMemUtility.h"
#include "IrkContract.h"

namespace irk {

// A light-weight memory pool used to allocate many small and fixed-sized items(slots)
// All memory will be released when memory slots is destroyed
class MemSlots : IrkNocopy
{
public:
    MemSlots();
    MemSlots(size_t itemSize, size_t maxCnt, size_t alignment = sizeof(void*));
    ~MemSlots() { this->clear(); }

    // initialize the memory slots, can be called many times
    // @param itemSize  The size of one item
    // @param maxCnt    The expected maximum item count
    // @param alignment Item alignment shall not greater than 16
    void init(size_t itemSize, size_t maxCnt, size_t alignment = sizeof(void*));

    // allocate an item
    void* alloc();
    void* alloc(size_t size, size_t alignment)
    {
        irk_expect(size <= m_SlotSize && alignment <= m_Alignment);
        (void)size;
        (void)alignment;
        return this->alloc();
    }

    // deallocate an item
    void dealloc(void* ptr);
    void dealloc(void* ptr, size_t size)
    {
        (void)size;
        this->dealloc(ptr);
    }

    // free all memory
    void clear();

    // get the actual slot size
    size_t slot_size() const { return m_SlotSize; }

private:
    char*   m_pChunk;
    char*   m_pFreeList;
    size_t  m_Used;
    size_t  m_Alignment;
    size_t  m_SlotSize;
    size_t  m_ChunkSize;
};

// A light-weight memory pool used to allocate many small and variable-sized items
// Memory item will NOT be freed individually but released all at once when memory chuncks are destroyed
class MemChunks : IrkNocopy
{
public:
    MemChunks();
    explicit MemChunks(size_t chunkSize);
    ~MemChunks() { this->clear(); }

    // initialize the memory chuck, can be called many times
    // @param chunkSize The size of internal memory chunk
    // @param largeSize Should less than chunkSize / 2, item greater than largeSize will be allocated by new
    void    init(size_t chunkSize, size_t largeSize = 0);

    // allocate a memory item, alignment shall not greater than 16
    void*   alloc(size_t size, size_t alignment = sizeof(void*));

    // NOTE: internal memory chunks will not be freed
    void    dealloc(void*, size_t);

    // free all memory
    void    clear();

private:
    size_t  m_ChunkSize;
    size_t  m_LargeSize;
    char*   m_pChunk;
    char*   m_pFree;
    char*   m_pLargeBlock;      // extra large buffer list
};

// A special light-weight memory pool used to allocate several small and variable-sized items
// @Warning User is responsible for deallocating every item allocated!
// Memory item is sliced from an initial buffer(usually a stack array), 
// if initial buffer is exhausted, allocation is forward to malloc
class MemArena : IrkNocopy
{
public:
    // @param buff  External initial buffer, if NULL, an initial buffer will be allocated
    // @param size  Initial buffer's size
    MemArena(void* buff, size_t size);
    MemArena() : m_pBuff(nullptr), m_pFree(nullptr), m_BufSize(0), m_AllocCnt(0) {}
    ~MemArena();

    // initialize the memory arena, can be called many times
    // @param buff  External initial buffer, if NULL, an initial buffer will be allocated 
    // @param size  Initial buffer's size
    void init(void* buff, size_t size);

    // allocate a memory item, alignment shall not greater than 16
    void* alloc(size_t size, size_t alignment = sizeof(void*));

    // deallocate memory item
    void dealloc(void* ptr, size_t size);

private:
    char*       m_pBuff;
    char*       m_pFree;
    size_t      m_BufSize;
    uint32_t    m_AllocCnt;
};

// A heavy-weight memory pool used to allocate many small(< 256 bytes) and variable-sized items
// All memory will be released when memory pool is destroyed
class MemPool : IrkNocopy
{
public:
    MemPool();
    explicit MemPool(size_t chunkSize);
    ~MemPool();

    // initialize the memory pool, can be called many times
    // @param chunkSize The initial size of the memory pool
    void init(size_t chunkSize);

    // allocate a memory item, alignment shall not greater than 16
    void* alloc(size_t size, size_t alignment = sizeof(void*));

    // deallocate memory item
    void dealloc(void* ptr, size_t size);

    // free all memory
    void clear();

    // real buffer capacity for the requested size
    size_t bucket_size(size_t size) const;

private:
    static constexpr int kNumBucket = 10;
    char*       m_Buckets[kNumBucket];  // bucket free list
    char*       m_pLargeBlock;          // extra large buffer list
    MemChunks   m_Chunk;
#ifndef NDEBUG
    uint32_t    m_AllocCnt;             // for debug
#endif
};

} // namespace irk

// create object from memory pool
template<class Ty, class Pool, typename ...Args>
inline Ty* irk_new(Pool& pool, Args&& ...args)
{
    void* mem = pool.alloc(sizeof(Ty), alignof(Ty));
    return ::new(mem) Ty(std::forward<Args>(args)...);
}

// delete object from memory pool
// WARNING: Ty must be the real type, not the base type
template<class Ty, class Pool>
inline void irk_delete(Pool& pool, Ty* ptr)
{
    if (ptr)
    {
        ptr->~Ty();
        pool.dealloc(ptr, sizeof(Ty));
    }
}

#endif
