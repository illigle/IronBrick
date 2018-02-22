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

#include "IrkMemPool.h"

#undef NEXT_PTR
#undef ALIGNUP_PTR
#undef ROUND_UP
#define NEXT_PTR(p)         (*(char**)(p))
#define ALIGNUP_PTR(p,a)    ((char*)(((uintptr_t)(p)+(a)-1) & ~((a)-1)))
#define ROUND_UP(x, a)      (((x)+(a)-1) & ~((a)-1))

namespace irk {

MemSlots::MemSlots()
    : m_pChunk(nullptr), m_pFreeList(nullptr), m_Used(SIZE_MAX),
    m_Alignment(sizeof(void*)), m_SlotSize(0), m_ChunkSize(0)
{}

MemSlots::MemSlots(size_t itemSize, size_t maxCnt, size_t alignment)
{
    irk_expect(itemSize > 0 && maxCnt > 1);             // require rational parameters
    irk_expect((alignment & (alignment - 1)) == 0);     // alignment shall be power of 2
    irk_expect(alignment > 0 && alignment <= 16);       // max aligment is 16
    m_pChunk = nullptr;
    m_pFreeList = nullptr;
    m_Used = SIZE_MAX;
    m_Alignment = MAX(alignment, sizeof(void*));
    m_SlotSize = ROUND_UP(itemSize, m_Alignment);
    m_ChunkSize = ROUND_UP(sizeof(char*), m_Alignment) + m_SlotSize * maxCnt;
}

void MemSlots::init(size_t itemSize, size_t maxCnt, size_t alignment)
{
    irk_expect(itemSize > 0 && maxCnt > 1);             // require rational parameters
    irk_expect((alignment & (alignment - 1)) == 0);     // alignment shall be power of 2
    irk_expect(alignment > 0 && alignment <= 16);       // max aligment is 16

    this->clear();  // make this function recallable

    m_Alignment = MAX(alignment, sizeof(void*));
    m_SlotSize = ROUND_UP(itemSize, m_Alignment);
    m_ChunkSize = ROUND_UP(sizeof(char*), m_Alignment) + m_SlotSize * maxCnt;
}

void* MemSlots::alloc()
{
    irk_expect(m_SlotSize > 0);   // init() should have been called

    void* pout = m_pFreeList;
    if (pout)
    {
        // allocate from freed list
        m_pFreeList = NEXT_PTR(pout);
    }
    else if (m_Used < m_ChunkSize)
    {
        // allocate from current chunk
        pout = m_pChunk + m_Used;
        m_Used += m_SlotSize;
    }
    else
    {
        // allocate new chunk
        char* pchunk = (char*)aligned_malloc(m_ChunkSize, m_Alignment);
        NEXT_PTR(pchunk) = m_pChunk;
        m_pChunk = pchunk;

        size_t linkSize = ROUND_UP(sizeof(char*), m_Alignment);
        m_Used = linkSize + m_SlotSize;
        pout = pchunk + linkSize;
    }

    irk_ensure(((uintptr_t)pout & (m_Alignment - 1)) == 0);
    return pout;
}

void MemSlots::dealloc(void* ptr)
{
    // add to free list
    NEXT_PTR(ptr) = m_pFreeList;
    m_pFreeList = (char*)ptr;
}

void MemSlots::clear()
{
    // free all chunks
    char* pcur = m_pChunk;
    while (pcur)
    {
        char* pnxt = NEXT_PTR(pcur);
        aligned_free(pcur);
        pcur = pnxt;
    }
    m_pChunk = nullptr;
    m_pFreeList = nullptr;
    m_Used = SIZE_MAX;
}

//======================================================================================================================
MemChunks::MemChunks()
    : m_ChunkSize(0), m_LargeSize(0), m_pChunk(nullptr), m_pFree(nullptr), m_pLargeBlock(nullptr)
{}

MemChunks::MemChunks(size_t chunkSize)
{
    // make rational parameters
    if (chunkSize < 512)
        chunkSize = 512;

    m_ChunkSize = chunkSize + sizeof(char*);
    m_LargeSize = (chunkSize >> 1);
    m_pChunk = new char[m_ChunkSize];
    NEXT_PTR(m_pChunk) = nullptr;
    m_pFree = m_pChunk + sizeof(char*);
    m_pLargeBlock = nullptr;
}

void MemChunks::init(size_t chunkSize, size_t largeSize)
{
    // free all memory
    this->clear();

    // make rational parameters
    if (chunkSize < 512)
        chunkSize = 512;
    if (largeSize == 0 || largeSize > (chunkSize >> 1))
        largeSize = chunkSize >> 1;

    m_ChunkSize = chunkSize + sizeof(char*);
    m_LargeSize = largeSize;
    m_pChunk = new char[m_ChunkSize];
    NEXT_PTR(m_pChunk) = nullptr;
    m_pFree = m_pChunk + sizeof(char*);
}

void* MemChunks::alloc(size_t size, size_t alignment)
{
    irk_expect(m_ChunkSize > 0 && m_ChunkSize > m_LargeSize * 2);
    irk_expect((alignment & (alignment - 1)) == 0);     // alignment shall be power of 2
    irk_expect(alignment > 0 && alignment <= 16);       // max aligment is 16

    char* pout = nullptr;
    if (size <= m_LargeSize)   // slice from a chunk
    {
        if (!m_pChunk)     // no chunk available
        {
            m_pChunk = new char[m_ChunkSize];
            NEXT_PTR(m_pChunk) = nullptr;
            m_pFree = m_pChunk + sizeof(char*);
        }

        // try to slice a memory from the chunk
        pout = ALIGNUP_PTR(m_pFree, alignment);
        m_pFree = pout + size;

        if (m_pFree > m_pChunk + m_ChunkSize)   // chunk exhausted, create new chunk
        {
            char* pchunk = new char[m_ChunkSize];
            pout = ALIGNUP_PTR(pchunk + sizeof(char*), alignment);

            NEXT_PTR(pchunk) = m_pChunk;
            m_pChunk = pchunk;
            m_pFree = pout + size;
            irk_ensure(m_pFree < m_pChunk + m_ChunkSize);
        }
    }
    else
    {
        // large memory fall-back to std new
        char* pBlock = new char[size + sizeof(char*) + alignment];
        NEXT_PTR(pBlock) = m_pLargeBlock;
        m_pLargeBlock = pBlock;
        pout = ALIGNUP_PTR(pBlock + sizeof(char*), alignment);
    }

    return pout;
}

void MemChunks::dealloc(void* ptr, size_t size)
{
    char* pbuf = (char*)ptr;
    if (pbuf > m_pChunk && pbuf < m_pFree && (pbuf + size) == m_pFree)
        m_pFree = pbuf;
}

void MemChunks::clear()
{
    // free all chunks allocated
    char* pcur = m_pChunk;
    while (pcur)
    {
        char* pnxt = NEXT_PTR(pcur);
        delete[] pcur;
        pcur = pnxt;
    }
    m_pChunk = nullptr;
    m_pFree = nullptr;

    // free all large memory allocated
    pcur = m_pLargeBlock;
    while (pcur)
    {
        char* pnxt = NEXT_PTR(pcur);
        delete[] pcur;
        pcur = pnxt;
    }
    m_pLargeBlock = nullptr;
}

//======================================================================================================================
MemArena::MemArena(void* buff, size_t size)
{
    irk_expect(size > 0);
    if (buff)                       // use external initial buffer
    {
        m_pBuff = (char*)buff;
        m_AllocCnt = 0;
    }
    else
    {
        m_pBuff = new char[size];   // use internal initial buffer
        m_AllocCnt = 1;             // initial buffer owner tag
    }
    m_pFree = m_pBuff;
    m_BufSize = size;
}

MemArena::~MemArena()
{
    assert(m_AllocCnt <= 1);        // all items shall be freed
    if (m_AllocCnt & 1)             // free internal initial buffer
    {
        delete[] m_pBuff;
    }
}

void MemArena::init(void* buff, size_t size)
{
    assert(m_AllocCnt <= 1);        // all items shall be freed
    if (m_AllocCnt & 1)             // free internal initial buffer
    {
        delete[] m_pBuff;
        m_pBuff = nullptr;
    }

    irk_expect(size > 0);
    if (buff)                       // use external initial buffer
    {
        m_pBuff = (char*)buff;
        m_AllocCnt = 0;
    }
    else
    {
        m_pBuff = new char[size];   // use internal initial buffer
        m_AllocCnt = 1;             // initial buffer owner tag
    }
    m_pFree = m_pBuff;
    m_BufSize = size;
}

void* MemArena::alloc(size_t size, size_t alignment)
{
    irk_expect((alignment & (alignment - 1)) == 0); // alignment shall be power of 2
    irk_expect(alignment > 0 && alignment <= 16);   // max aligment is 16
    irk_expect(m_pBuff && m_pFree);

    char* pbuf = ALIGNUP_PTR(m_pFree, alignment);
    if (pbuf + size <= m_pBuff + m_BufSize)         // slice from initial buffer
    {
        m_pFree = pbuf + size;
    }
    else
    {
        // fall-back to malloc
        pbuf = (char*)aligned_malloc(size, alignment);
    }

    m_AllocCnt += 2;
    return pbuf;
}

void MemArena::dealloc(void* ptr, size_t size)
{
    char* pbuf = (char*)ptr;
    if (pbuf >= m_pBuff && pbuf < m_pFree)  // sliced from initial buffer
    {
        if ((pbuf + size) == m_pFree)       // last allocated item wants to be freed
            m_pFree = pbuf;
    }
    else
    {
        aligned_free(ptr);
    }

    irk_expect(m_AllocCnt >= 2);
    m_AllocCnt -= 2;
}

//======================================================================================================================
#undef PREV_PTR
#define PREV_PTR(p) (*((char**)(p) + 1))

static constexpr int kDLinkSize = 16;

// bucket size
static constexpr uint32_t s_BucketSize[10] =
{
    8, 16, 32, 48, 64, 96, 128, 160, 192, 256
};

// bucket index by size / 8
static constexpr int s_BucketIdx[33] =
{
    0,
    0, 1, 2, 2, 3, 3, 4, 4,
    5, 5, 5, 5, 6, 6, 6, 6,
    7, 7, 7, 7, 8, 8, 8, 8,
    9, 9, 9, 9, 9, 9, 9, 9,
};

// get bucket for the item
static inline int GetBucketIdx(size_t size)
{
    size_t size8 = (size + 7) & ~7;         // roundup to 8 bytes
    return (size8 <= 256) ? s_BucketIdx[size8 >> 3] : INT32_MAX;
}

MemPool::MemPool()
{
    memset(m_Buckets, 0, sizeof(m_Buckets));    // empty free list
    m_pLargeBlock = nullptr;
#ifndef NDEBUG
    m_AllocCnt = 0;
#endif
}

MemPool::MemPool(size_t chunkSize)
{
    if (chunkSize < 4096)   // This is a heavy-weight memory pool !
        chunkSize = 4096;

    memset(m_Buckets, 0, sizeof(m_Buckets));    // empty free list
    m_Chunk.init(chunkSize);
    m_pLargeBlock = nullptr;
#ifndef NDEBUG
    m_AllocCnt = 0;
#endif
}

MemPool::~MemPool()
{
    this->clear();
}

// real buffer capacity for the requested size
size_t MemPool::bucket_size(size_t size) const
{
    if (size <= 256) // small object
    {
        int idx = GetBucketIdx(size);
        return s_BucketSize[idx];
    }
    return size;
}

void MemPool::init(size_t chunkSize)
{
    this->clear();

    if (chunkSize < 4096)   // This is a heavy-weight memory pool !
        chunkSize = 4096;
    m_Chunk.init(chunkSize);
}

void* MemPool::alloc(size_t size, size_t alignment)
{
    irk_expect((alignment & (alignment - 1)) == 0); // power of 2
    irk_expect(alignment > 0 && alignment <= 16);   // max alignment is 16
    if (size < alignment)
        size = alignment;
    void* pout = nullptr;

    if (size <= 256)            // small object
    {
        const int idx = GetBucketIdx(size);
        const size_t bucketSize = s_BucketSize[idx];
        irk_ensure(idx < kNumBucket && size <= bucketSize);

        if (m_Buckets[idx])     // allocate from free list
        {
            pout = m_Buckets[idx];
            m_Buckets[idx] = NEXT_PTR(pout);
        }
        else
        {
            // try to steal from bigger bucket
            int k = GetBucketIdx(bucketSize * 2);
            if (k < kNumBucket && m_Buckets[k])
            {
                pout = m_Buckets[k];
                m_Buckets[k] = NEXT_PTR(pout);
                m_Buckets[idx] = (char*)pout + bucketSize;  // only one half used
                NEXT_PTR(m_Buckets[idx]) = nullptr;
            }
            else
            {
                // allocate from memory chunk, internal alignment is 8 or 16
                if (bucketSize == 8)
                {
                    irk_ensure(alignment <= 8);
                    pout = m_Chunk.alloc(bucketSize, 8);
                }
                else
                {
                    pout = m_Chunk.alloc(bucketSize, 16);
                }
            }
        }
        irk_ensure(((uintptr_t)pout & (alignment - 1)) == 0);
    }
    else    // big memory allocate by new
    {
        char* pbuf = (char*)aligned_malloc(size + kDLinkSize, kDLinkSize);

        // double link
        NEXT_PTR(pbuf) = m_pLargeBlock;
        PREV_PTR(pbuf) = nullptr;
        if (m_pLargeBlock)
            PREV_PTR(m_pLargeBlock) = pbuf;
        m_pLargeBlock = pbuf;

        pout = pbuf + kDLinkSize;
        irk_ensure(((uintptr_t)pout & (alignment - 1)) == 0);
    }

#ifndef NDEBUG
    m_AllocCnt++;
#endif
    return pout;
}

void MemPool::dealloc(void* ptr, size_t size)
{
    int idx = GetBucketIdx(size);
    if (idx < kNumBucket)   // small object
    {
        NEXT_PTR(ptr) = m_Buckets[idx];
        m_Buckets[idx] = (char*)ptr;
    }
    else
    {
        // remove from large memory list
        char* pbuf = (char*)ptr - kDLinkSize;
        char* pNext = NEXT_PTR(pbuf);
        char* pPrev = PREV_PTR(pbuf);
        if (pNext)
            PREV_PTR(pNext) = pPrev;
        if (pPrev)
            NEXT_PTR(pPrev) = pNext;
        else
            m_pLargeBlock = pNext;

        aligned_free(pbuf);
    }

#ifndef NDEBUG
    m_AllocCnt--;
#endif
}

void MemPool::clear()
{
    // clear free lists
    memset(m_Buckets, 0, sizeof(m_Buckets));

    // clear chunks
    m_Chunk.clear();

    // free all large memory allocated
    char* pcur = m_pLargeBlock;
    while (pcur)
    {
        char* pnxt = NEXT_PTR(pcur);
        aligned_free(pcur);
        pcur = pnxt;
    }
    m_pLargeBlock = nullptr;

#ifndef NDEBUG
    m_AllocCnt = 0;
#endif
}

} // namespace irk
