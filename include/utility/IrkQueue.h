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

#ifndef _IRONBRICK_QUEUE_H_
#define _IRONBRICK_QUEUE_H_

#include "IrkMemUtility.h"

namespace irk {

#define QNEXT_BLOCK( pb ) (*(T**)((char*)(pb) + m_blkSize))

// FIFO queue implemented like std::deque, designed for small trivial object
template<typename T>
class Queue : IrkNocopy
{
public:
    typedef T value_type;
    static_assert(std::is_trivially_copyable<T>::value, "only support trivially copyable type");

    // maxCnt: the expected max item count in the queue
    explicit Queue(unsigned maxCnt);
    ~Queue();
    void        push_back(const T& value);      // NOTE: will make existed iterator invalid
    bool        pop_front(T* pValue);           // NOTE: will make existed iterator invalid
    bool        pop_front();                    // NOTE: will make existed iterator invalid
    T&          front();
    const T&    front() const;
    T&          back();
    const T&    back() const;
    T&          operator[](unsigned idx);       // NOTE: O(N) search, consider using iterator
    const T&    operator[](unsigned idx) const;
    void        clear();
    unsigned    size() const { return m_totalCnt; }
    bool        empty() const { return m_totalCnt == 0; }

    // forward iterator
    class Iterator
    {
    public:
        bool operator==(const Iterator& other) const
        {
            return other.m_blk == m_blk && other.m_idx == m_idx;
        }
        bool operator!=(const Iterator& other) const
        {
            return other.m_blk != m_blk || other.m_idx != m_idx;
        }
        Iterator& operator++()
        {
            if (++m_idx >= m_cntPerBlk && m_blk)
            {
                size_t bsize = (sizeof(T) * m_cntPerBlk + sizeof(T*) - 1) & ~(sizeof(T*) - 1);
                m_blk = *(T**)((char*)(m_blk)+bsize);
                m_idx = 0;
            }
            return *this;
        }
        T& operator*()              { return m_blk[m_idx]; }
        const T& operator*() const  { return m_blk[m_idx]; }
        T* operator->()             { return m_blk + m_idx; }
        const T* operator->() const { return m_blk + m_idx; }
    private:
        friend class Queue<T>;
        Iterator(T* pblk, unsigned idx, unsigned cntPerBlk)
            : m_blk(pblk), m_idx(idx), m_cntPerBlk(cntPerBlk) {}
        T*          m_blk;          // current block
        unsigned    m_idx;          // current position in current block
        unsigned    m_cntPerBlk;    // max item count per block
    };
    Iterator begin()
    {
        assert(m_rIdx < m_cntPerBlk);
        return Iterator(m_rBlock, m_rIdx, m_cntPerBlk);
    }
    Iterator end()
    {
        if (m_wIdx < m_cntPerBlk)
            return Iterator(m_wBlock, m_wIdx, m_cntPerBlk);
        else
            return Iterator(QNEXT_BLOCK(m_wBlock), 0, m_cntPerBlk);
    }

    // const forward iterator
    class CIterator
    {
    public:
        bool operator==(const CIterator& other) const
        {
            return other.m_blk == m_blk && other.m_idx == m_idx;
        }
        bool operator!=(const CIterator& other) const
        {
            return other.m_blk != m_blk || other.m_idx != m_idx;
        }
        CIterator& operator++()
        {
            if (++m_idx >= m_cntPerBlk && m_blk)
            {
                size_t bsize = (sizeof(T) * m_cntPerBlk + sizeof(T*) - 1) & ~(sizeof(T*) - 1);
                m_blk = *(T**)((char*)(m_blk)+bsize);
                m_idx = 0;
            }
            return *this;
        }
        const T& operator*()        { return m_blk[m_idx]; }
        const T& operator*() const  { return m_blk[m_idx]; }
        const T* operator->()       { return m_blk + m_idx; }
        const T* operator->() const { return m_blk + m_idx; }
    private:
        friend class Queue<T>;
        CIterator(const T* pblk, unsigned idx, unsigned cntPerBlk)
            : m_blk(pblk), m_idx(idx), m_cntPerBlk(cntPerBlk) {}
        const T*    m_blk;          // current block
        unsigned    m_idx;          // current position in current block
        unsigned    m_cntPerBlk;    // max item count per block
    };
    CIterator begin() const
    {
        assert(m_rIdx < m_cntPerBlk);
        return CIterator(m_rBlock, m_rIdx, m_cntPerBlk);
    }
    CIterator end() const
    {
        if (m_wIdx < m_cntPerBlk)
            return CIterator(m_wBlock, m_wIdx, m_cntPerBlk);
        else
            return CIterator(QNEXT_BLOCK(m_wBlock), 0, m_cntPerBlk);
    }
private:
    void goto_next_blk()    // forward to next read block
    {
        T* nextBlk = QNEXT_BLOCK(m_rBlock);
        assert(nextBlk);
        if (m_xBlock == nullptr)    // reserve this block if no empty block left
            m_xBlock = m_rBlock;
        else
            aligned_free(m_rBlock);
        m_rBlock = nextBlk;
        m_rIdx = 0;
    }
    const unsigned  m_cntPerBlk;        // max item count per block
    unsigned        m_totalCnt;         // total item count
    size_t          m_blkSize;          // block size in bytes
    T*              m_wBlock;           // current write block
    unsigned        m_wIdx;             // current write position in the write block
    T*              m_rBlock;           // current read block
    unsigned        m_rIdx;             // current read position in the read block
    T*              m_xBlock;           // empty block to be reused
};

template<typename T>
Queue<T>::Queue(unsigned maxCnt) : m_cntPerBlk(maxCnt), m_totalCnt(0), m_rBlock(nullptr), m_xBlock(nullptr)
{
    assert(m_cntPerBlk > 0);
    m_blkSize = (sizeof(T) * m_cntPerBlk + sizeof(T*) - 1) & ~(sizeof(T*) - 1);
    m_wBlock = (T*)aligned_malloc(m_blkSize + sizeof(T*), MAX(alignof(T), alignof(T*)));
    QNEXT_BLOCK(m_wBlock) = nullptr;
    m_wIdx = 0;
    m_rBlock = m_wBlock;
    m_rIdx = 0;
}

template<typename T>
Queue<T>::~Queue()
{
    T* pcur = m_rBlock;
    while (pcur)
    {
        T* pnxt = QNEXT_BLOCK(pcur);
        aligned_free(pcur);
        pcur = pnxt;
    }
    if (m_xBlock)
    {
        aligned_free(m_xBlock);
    }
}

template<typename T>
inline void Queue<T>::push_back(const T& value)
{
    if (m_wIdx == m_cntPerBlk) // new block needed
    {
        if (m_xBlock)      // reuse reserved block
        {
            QNEXT_BLOCK(m_xBlock) = nullptr;
            QNEXT_BLOCK(m_wBlock) = m_xBlock;
            m_wBlock = m_xBlock;
            m_xBlock = nullptr;
        }
        else
        {
            T* pblock = (T*)aligned_malloc(m_blkSize + sizeof(T*), MAX(alignof(T), alignof(T*)));
            QNEXT_BLOCK(pblock) = nullptr;
            QNEXT_BLOCK(m_wBlock) = pblock;
            m_wBlock = pblock;
        }
        m_wIdx = 0;
    }
    m_wBlock[m_wIdx] = value;
    m_wIdx++;
    m_totalCnt++;
}

template<typename T>
inline bool Queue<T>::pop_front(T* pout)
{
    if (m_totalCnt == 0)
        return false;

    *pout = m_rBlock[m_rIdx];

    if (--m_totalCnt == 0)
    {
        assert(m_rBlock == m_wBlock);
        m_wIdx = 0;
        m_rIdx = 0;
    }
    else
    {
        if (++m_rIdx >= m_cntPerBlk)
            this->goto_next_blk();
    }
    return true;
}

template<typename T>
inline bool Queue<T>::pop_front()
{
    if (m_totalCnt == 0)
        return false;

    if (--m_totalCnt == 0)
    {
        assert(m_rBlock == m_wBlock);
        m_wIdx = 0;
        m_rIdx = 0;
    }
    else
    {
        if (++m_rIdx >= m_cntPerBlk)
            this->goto_next_blk();
    }
    return true;
}

template<typename T>
inline T& Queue<T>::front()
{
    assert(m_totalCnt > 0 && m_rIdx < m_cntPerBlk);
    return m_rBlock[m_rIdx];
}

template<typename T>
inline const T& Queue<T>::front() const
{
    assert(m_totalCnt > 0 && m_rIdx < m_cntPerBlk);
    return m_rBlock[m_rIdx];
}

template<typename T>
inline T& Queue<T>::back()
{
    assert(m_totalCnt > 0 && m_wIdx <= m_cntPerBlk);
    return m_wBlock[m_wIdx - 1];
}

template<typename T>
inline const T& Queue<T>::back() const
{
    assert(m_totalCnt > 0 && m_wIdx <= m_cntPerBlk);
    return m_wBlock[m_wIdx - 1];
}

template<typename T>
T& Queue<T>::operator[](unsigned idx)
{
    assert(idx < m_totalCnt);
    T* pblk = m_rBlock;
    unsigned k = m_rIdx + idx;
    while (k >= m_cntPerBlk)   // goto next block
    {
        pblk = QNEXT_BLOCK(pblk);
        assert(pblk);
        k -= m_cntPerBlk;
    }
    return pblk[k];
}

template<typename T>
const T& Queue<T>::operator[](unsigned idx) const
{
    assert(idx < m_totalCnt);
    T* pblk = m_rBlock;
    unsigned k = m_rIdx + idx;
    while (k >= m_cntPerBlk)   // goto next block
    {
        pblk = QNEXT_BLOCK(pblk);
        assert(pblk);
        k -= m_cntPerBlk;
    }
    return pblk[k];
}

template<typename T>
void Queue<T>::clear()
{
    // delete all blocks except the first one
    T* pcur = QNEXT_BLOCK(m_rBlock);
    while (pcur)
    {
        T* pnxt = QNEXT_BLOCK(pcur);
        aligned_free(pcur);
        pcur = pnxt;
    }
    QNEXT_BLOCK(m_rBlock) = nullptr;
    m_wBlock = m_rBlock;
    m_wIdx = 0;
    m_rIdx = 0;
    m_totalCnt = 0;

    if (m_xBlock)
    {
        aligned_free(m_xBlock);
        m_xBlock = nullptr;
    }
}

#undef QNEXT_BLOCK

}   // namespace irk
#endif