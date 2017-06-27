/*
* This Source Code Form is subject to the terms of the Mozilla Public License Version 2.0.
* If a copy of the MPL was not distributed with this file, 
* You can obtain one at http://mozilla.org/MPL/2.0/.

* Covered Software is provided on an ¡°as is¡± basis, 
* without warranty of any kind, either expressed, implied, or statutory,
* that the Covered Software is free of defects, merchantable, 
* fit for a particular purpose or non-infringing.
 
* Copyright (c) Wei Dongliang <illigle@163.com>.
*/

#ifndef _IRONBRICK_SYNCQUEUE_H_
#define _IRONBRICK_SYNCQUEUE_H_

#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include "IrkMemPool.h"
#include "IrkSyncUtility.h"

namespace irk {

// unbounded thread-safe FIFO queue( throw std::bad_alloc if memory is exhausted)
// algorithm from: Herb Sutter - Measuring Parallel Performance: Optimizing a Concurrent Queue
template<typename Ty, typename MxType=std::mutex>
class SyncedQueue : IrkNocopy
{
    static_assert( std::is_default_constructible<Ty>::value, "requires default constructible type" );
    struct Node 
    {
        Node() : m_pNext(nullptr) {}
        template<typename... Args>
        explicit Node( Args&&... args ) : m_Data(std::forward<Args>(args)...), m_pNext(nullptr) {}
        Ty                  m_Data;
        std::atomic<Node*>  m_pNext;
    };
public:
    // queueSize: the initial queue size, queue can grow on the fly
    explicit SyncedQueue( size_t queueSize ) 
        : m_Cnt(0), m_pHead(nullptr), m_pTail(nullptr), m_NodeSlots(sizeof(Node), queueSize, alignof(Node))
    {
        m_pHead = m_pTail = irk_new<Node>( m_NodeSlots );  // dummy node
    }
    ~SyncedQueue()
    {
        // normally user should remove all data before destruction
        // the queue may not know how to free all resource(e.g. when pointer is stored)
        if( m_Cnt > 0 )
        {
            this->clear();
        }
        if( m_pHead )
        {
            assert( m_pHead->m_pNext == nullptr );
            m_pHead->~Node();
        }
    }

    // insert data to queue tail
    void push_back( const Ty& data )
    {
        m_SlotsMutex.lock();
        void* mem = m_NodeSlots.alloc();
        m_SlotsMutex.unlock();
        Node* pNewNode = ::new(mem) Node( data );
        this->push_back_node( pNewNode );
        m_Cnt++;
    }
    void push_back( Ty&& data )
    {
        m_SlotsMutex.lock();
        void* mem = m_NodeSlots.alloc();
        m_SlotsMutex.unlock();
        Node* pNewNode = ::new(mem) Node( std::move(data) );
        this->push_back_node( pNewNode );
        m_Cnt++;
    }
    template<typename... Args>
    void emplace_back( Args&&... args )
    {
        m_SlotsMutex.lock();
        void* mem = m_NodeSlots.alloc();
        m_SlotsMutex.unlock();
        Node* pNewNode = ::new(mem) Node( std::forward<Args>(args)... );
        this->push_back_node( pNewNode );
        m_Cnt++;
    }

    // remove data from queue's head, return false if queue is empty
    bool pop_front( Ty* pout )
    {
        static_assert( std::is_move_assignable<Ty>::value, "value should be move assignable" );
        if( m_Cnt == 0 )    // queue is empty
            return false;

        std::unique_lock<MxType> lock_( m_HeadMutex );
        Node* pOldHead = m_pHead;
        Node* pCurHead = m_pHead->m_pNext;
        if( pCurHead )
        {
            *pout = std::move( pCurHead->m_Data );
            m_pHead = pCurHead;
            lock_.unlock();
            m_Cnt--;
            this->delete_node( pOldHead );
            return true;
        }
        return false;
    }
    // discard the first data in the queue
    bool pop_front()
    {
        if( m_Cnt == 0 )    // queue is empty
            return false;

        std::unique_lock<MxType> lock_( m_HeadMutex );
        Node* pOldHead = m_pHead;
        Node* pCurHead = m_pHead->m_pNext;
        if( pCurHead )
        {
            m_pHead = pCurHead;
            lock_.unlock();
            m_Cnt--;
            this->delete_node( pOldHead );
            return true;
        }
        return false;
    }
    // check data in queue's head, does not remove, return false if queue is empty
    bool peek_front( Ty* pout )
    {
        static_assert( std::is_copy_assignable<Ty>::value, "value should be copy assignable" );
        if( m_Cnt == 0 )    // queue is empty
            return false;

        std::lock_guard<MxType> lock_( m_HeadMutex );
        const Node* pCurHead = m_pHead->m_pNext;
        if( pCurHead )
        {
            *pout = pCurHead->m_Data;
            return true;
        }
        return false;
    }

    // remove all data in the queue
    void clear();

    // queue state
    int count() const       { return m_Cnt; }
    bool is_empty() const   { return m_Cnt == 0; }

private:
    void push_back_node( Node* pNode )
    {
        m_TailMutex.lock();
        m_pTail->m_pNext = pNode;
        m_pTail = pNode;
        m_TailMutex.unlock();
    }
    void delete_node( Node* pNode )
    {
        pNode->~Node();
        m_SlotsMutex.lock();
        m_NodeSlots.dealloc( pNode );
        m_SlotsMutex.unlock();
    }
    std::atomic_int m_Cnt;          // number of objects stored
    Node*           m_pHead;
    MxType          m_HeadMutex;
    Node*           m_pTail;
    MxType          m_TailMutex;
    MemSlots        m_NodeSlots;    // internal memory pool
    MxType          m_SlotsMutex;
};

// discard all data in the queue, normally user should pop all data one by one,
// the queue may not know how to free all resource(e.g. when pointer is stored)
template<typename Ty, typename MxType>
void SyncedQueue<Ty,MxType>::clear()
{
    if( m_Cnt > 0 )
    {
        std::lock_guard<MxType> lock_( m_HeadMutex );
        Node* pHead = m_pHead;
        Node* pNext = pHead->m_pNext;
        while( pNext )
        {
            this->delete_node( pHead );
            pHead = pNext;
            pNext = pNext->m_pNext;
        }
        m_pHead = pHead;
        m_Cnt = 0;
    }
}

//======================================================================================================================
// waitable FIFO thead-safe queue

template<typename Ty>
class WaitableQueue : IrkNocopy
{
    static_assert( std::is_default_constructible<Ty>::value, "requires default constructible type" ); 
    struct Node
    {
        Node() : m_pNext(nullptr) {}
        explicit Node( const Ty& data ) : m_Data(data), m_pNext(nullptr) {}
        explicit Node( Ty&& data ) : m_Data(std::move(data) ), m_pNext(nullptr) {}
        Ty      m_Data;
        Node*   m_pNext;
    };
public:
    explicit WaitableQueue( size_t queueSize ) : m_NodeSlots(sizeof(Node), queueSize + 2, alignof(Node))
    {
        assert( queueSize > 0 && queueSize < (1u << 31) );
        m_Capacity = static_cast<int>( queueSize );
        m_Cnt = 0;
        m_pHead = m_pTail = irk_new<Node>( m_NodeSlots );
    }
    ~WaitableQueue()
    {
        // user should remove all data before destruction
        assert( m_Cnt == 0 && m_pHead->m_pNext == nullptr );
        m_pHead->~Node();
    }

    // push data, return false if queue is full or closed
    bool push_back( const Ty& data );
    bool push_back( Ty&& data );

    // push data, wait if queue is full, return WaitStatus::Closed if queue is closed
    WaitStatus push_back_wait( const Ty& data );
    WaitStatus push_back_wait( Ty&& data );

    // push data, wait specified time if queue is full, return WaitStatus::Closed if queue is closed
    WaitStatus push_back_wait_for( const Ty& data, int milliseconds );
    WaitStatus push_back_wait_for( Ty&& data, int milliseconds );

    // pop the first data from the queue, return false if queue is empty
    bool pop_front( Ty* pout );
    bool pop_front();

    // pop the first data from the queue, wait if queue is empty, return WaitStatus::Closed if queue is closed
    WaitStatus pop_front_wait( Ty* pout );

    // pop the first data from the queue, wait specified time if queue is empty
    WaitStatus pop_front_wait_for( Ty* pout, int milliseconds );

    // close this queue, all waiting user will return WaitStatus::Closed
    void close();

    // advanced special function, force push data,return false if queue has closed
    bool force_push_back( const Ty& data );
    bool force_push_back( Ty&& data );
    bool force_push_front( const Ty& data );
    bool force_push_front( Ty&& data );

    // queue state
    bool is_empty() const   { return m_Cnt == 0; }
    bool is_full() const    { return m_Cnt >= m_Capacity; }
    bool is_closed() const  { return m_Capacity <= 0; }
    int  count() const      { return m_Cnt; }
    int  capacity() const   { return m_Capacity; }

private:
    int                     m_Capacity;     // queue's capacity
    std::atomic_int         m_Cnt;          // current number of objects
    std::mutex              m_Mutex; 
    Node*                   m_pTail;
    std::condition_variable m_NotEmptyCV;
    Node*                   m_pHead;
    std::condition_variable m_NotFullCV;
    MemSlots                m_NodeSlots;    // interanl memory pool
};

// push data, return false if queue is full
template<typename Ty>
bool WaitableQueue<Ty>::push_back( const Ty& data )
{
    if( m_Cnt >= m_Capacity )   // queue is full or closed
        return false;
    std::unique_lock<std::mutex> lock_( m_Mutex );

    if( m_Cnt >= m_Capacity )   // double check
        return false;
    Node* pNode = irk_new<Node>( m_NodeSlots, data );
    m_pTail->m_pNext = pNode;
    m_pTail = pNode;
    m_Cnt++;

    lock_.unlock();
    m_NotEmptyCV.notify_one();
    return true;
}

template<typename Ty>
bool WaitableQueue<Ty>::push_back( Ty&& data )
{
    if( m_Cnt >= m_Capacity )   // queue is full or closed
        return false;
    std::unique_lock<std::mutex> lock_( m_Mutex );

    if( m_Cnt >= m_Capacity )   // double check
        return false;
    Node* pNode = irk_new<Node>( m_NodeSlots, std::move(data) );
    m_pTail->m_pNext = pNode;
    m_pTail = pNode;
    m_Cnt++;

    lock_.unlock();
    m_NotEmptyCV.notify_one();
    return true;
}

// push data, wait if queue is full, return WaitStatus::Closed if queue is closed
template<typename Ty>
WaitStatus WaitableQueue<Ty>::push_back_wait( const Ty& data )
{
    std::unique_lock<std::mutex> lock_( m_Mutex );

    while( m_Cnt >= m_Capacity && m_Capacity > 0 )
        m_NotFullCV.wait( lock_ );
    if( m_Capacity <= 0 )   // queue has been closed
        return WaitStatus::Closed;

    Node* pNode = irk_new<Node>( m_NodeSlots, data );
    m_pTail->m_pNext = pNode;
    m_pTail = pNode;
    m_Cnt++;

    lock_.unlock();
    m_NotEmptyCV.notify_one();
    return WaitStatus::Ok;
}

template<typename Ty>
WaitStatus WaitableQueue<Ty>::push_back_wait( Ty&& data )
{
    std::unique_lock<std::mutex> lock_( m_Mutex );

    while( m_Cnt >= m_Capacity && m_Capacity > 0 )
        m_NotFullCV.wait( lock_ );
    if( m_Capacity <= 0 )   // queue has been closed
        return WaitStatus::Closed;

    Node* pNode = irk_new<Node>( m_NodeSlots, std::move(data) );
    m_pTail->m_pNext = pNode;
    m_pTail = pNode;
    m_Cnt++;

    lock_.unlock();
    m_NotEmptyCV.notify_one();
    return WaitStatus::Ok;
}

// push data, wait specified time if queue is full, return WaitStatus::Closed if queue is closed
template<typename Ty>
WaitStatus WaitableQueue<Ty>::push_back_wait_for( const Ty& data, int millis )
{
    using std::chrono::steady_clock;
    steady_clock::time_point absTime = steady_clock::now() + std::chrono::milliseconds(millis);
    std::unique_lock<std::mutex> lock_( m_Mutex );

    while( m_Cnt >= m_Capacity && m_Capacity > 0 )
    {
        if( m_NotFullCV.wait_until( lock_, absTime ) == std::cv_status::timeout )
            return WaitStatus::Timeout;
    }
    if( m_Capacity <= 0 )   // queue has been closed
        return WaitStatus::Closed;

    Node* pNode = irk_new<Node>( m_NodeSlots, data );
    m_pTail->m_pNext = pNode;
    m_pTail = pNode;
    m_Cnt++;

    lock_.unlock();
    m_NotEmptyCV.notify_one();
    return WaitStatus::Ok;
}

template<typename Ty>
WaitStatus WaitableQueue<Ty>::push_back_wait_for( Ty&& data, int millis )
{
    using std::chrono::steady_clock;
    steady_clock::time_point absTime = steady_clock::now() + std::chrono::milliseconds(millis);
    std::unique_lock<std::mutex> lock_( m_Mutex );

    while( m_Cnt >= m_Capacity && m_Capacity > 0 )
    {
        if( m_NotFullCV.wait_until( lock_, absTime ) == std::cv_status::timeout )
            return WaitStatus::Timeout;
    }
    if( m_Capacity <= 0 )   // queue has been closed
        return WaitStatus::Closed;

    Node* pNode = irk_new<Node>( m_NodeSlots, std::move(data) );
    m_pTail->m_pNext = pNode;
    m_pTail = pNode;
    m_Cnt++;

    lock_.unlock();
    m_NotEmptyCV.notify_one();
    return WaitStatus::Ok;
}

// pop the first data from the queue, return false if queue is empty
template<typename Ty>
bool WaitableQueue<Ty>::pop_front( Ty* pout )
{
    if( m_Cnt == 0 )    // queue is empty
        return false;
    std::unique_lock<std::mutex> lock_( m_Mutex );

    if( m_Cnt == 0 )    // double check
        return false;
    Node* pNode = m_pHead->m_pNext;
    *pout = std::move( pNode->m_Data );
    irk_delete( m_NodeSlots, m_pHead );
    m_pHead = pNode;
    int cnt = --m_Cnt;

    lock_.unlock();
    if( cnt < m_Capacity )
        m_NotFullCV.notify_one();
    return true;
}

template<typename Ty>
bool WaitableQueue<Ty>::pop_front()
{
    if( m_Cnt == 0 )    // queue is empty
        return false;
    std::unique_lock<std::mutex> lock_( m_Mutex );

    if( m_Cnt == 0 )    // double check
        return false;
    Node* pNode = m_pHead->m_pNext;
    irk_delete( m_NodeSlots, m_pHead );
    m_pHead = pNode;
    int cnt = --m_Cnt;

    lock_.unlock();
    if( cnt < m_Capacity )
        m_NotFullCV.notify_one();
    return true;
}

// pop the first data from the queue, wait if queue is empty, return WaitStatus::Closed if queue is closed
template<typename Ty>
WaitStatus WaitableQueue<Ty>::pop_front_wait( Ty* pout )
{
    std::unique_lock<std::mutex> lock_( m_Mutex );

    while( m_Cnt == 0 && m_Capacity > 0 )
    {
        m_NotEmptyCV.wait( lock_ );
    }
    if( m_Cnt > 0 )
    {
        Node* pNode = m_pHead->m_pNext;
        *pout = std::move( pNode->m_Data );
        irk_delete( m_NodeSlots, m_pHead );
        m_pHead = pNode;
        int cnt = --m_Cnt;

        lock_.unlock();
        if( cnt < m_Capacity )
            m_NotFullCV.notify_one();
        return WaitStatus::Ok;
    }

    return WaitStatus::Closed;   
}

// pop the first data from the queue, wait specified time if queue is empty
template<typename Ty>
WaitStatus WaitableQueue<Ty>::pop_front_wait_for( Ty* pout, int millis )
{
    using std::chrono::steady_clock;
    steady_clock::time_point absTime = steady_clock::now() + std::chrono::milliseconds(millis);
    std::unique_lock<std::mutex> lock_( m_Mutex );

    while( m_Cnt == 0 && m_Capacity > 0 )
    {
        if( m_NotEmptyCV.wait_until( lock_, absTime ) == std::cv_status::timeout )
            return WaitStatus::Timeout;
    }
    if( m_Cnt > 0 )
    {
        Node* pNode = m_pHead->m_pNext;
        *pout = std::move( pNode->m_Data );
        irk_delete( m_NodeSlots, m_pHead );
        m_pHead = pNode;
        int cnt = --m_Cnt;

        lock_.unlock();
        if( cnt < m_Capacity )
            m_NotFullCV.notify_one();
        return WaitStatus::Ok;
    }

    return WaitStatus::Closed;   
}

// close this queue, all waiting user will return WaitStatus::Closed
template<typename Ty>
void WaitableQueue<Ty>::close()
{
    // mark queue closed
    m_Mutex.lock();
    m_Capacity = 0;
    m_Mutex.unlock();

    // notify all waiters
    m_NotEmptyCV.notify_all();
    m_NotFullCV.notify_all();
}

// advanced special function, force push data
template<typename Ty>
bool WaitableQueue<Ty>::force_push_back( const Ty& data )
{
    std::unique_lock<std::mutex> lock_( m_Mutex );
    if( m_Capacity <= 0 )   // queue closed
        return false;
    
    Node* pNode = irk_new<Node>( m_NodeSlots, data );
    m_pTail->m_pNext = pNode;
    m_pTail = pNode;
    m_Cnt++;

    lock_.unlock();
    m_NotEmptyCV.notify_one();
    return true;
}

template<typename Ty>
bool WaitableQueue<Ty>::force_push_back( Ty&& data )
{
    std::unique_lock<std::mutex> lock_( m_Mutex );
    if( m_Capacity <= 0 )   // queue closed
        return false;
    
    Node* pNode = irk_new<Node>( m_NodeSlots, std::move(data) );
    m_pTail->m_pNext = pNode;
    m_pTail = pNode;
    m_Cnt++;

    lock_.unlock();
    m_NotEmptyCV.notify_one();
    return true;
}

template<typename Ty>
bool WaitableQueue<Ty>::force_push_front( const Ty& data )
{
    std::unique_lock<std::mutex> lock_( m_Mutex );
    if( m_Capacity <= 0 )   // queue closed
        return false;
    
    Node* pNode = irk_new<Node>( m_NodeSlots, data );
    pNode->m_pNext = m_pHead->m_pNext;
    m_pHead->m_pNext = pNode;
    m_Cnt++;

    lock_.unlock();
    m_NotEmptyCV.notify_one();
    return true;
}

template<typename Ty>
bool WaitableQueue<Ty>::force_push_front( Ty&& data )
{
    std::unique_lock<std::mutex> lock_( m_Mutex );
    if( m_Capacity <= 0 )   // queue closed
        return false;
    
    Node* pNode = irk_new<Node>( m_NodeSlots, std::move(data) );
    pNode->m_pNext = m_pHead->m_pNext;
    m_pHead->m_pNext = pNode;
    m_Cnt++;

    lock_.unlock();
    m_NotEmptyCV.notify_one();
    return true;
}

} // namespace irk
#endif

