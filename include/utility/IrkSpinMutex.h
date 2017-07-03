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

#ifndef _IRONBRICK_SPINMUTEX_H_
#define _IRONBRICK_SPINMUTEX_H_

#include "IrkAtomic.h"

namespace irk {

//======================================================================================================================
// Common spin lock
// The interface is similar to std::mutex, can be used with std::lock_guard and std::unique_lock

class SpinMutex : IrkNocopy
{
public:
    SpinMutex() : m_Flag(0) {}
    ~SpinMutex() { assert( m_Flag == 0 ); }

    void lock()
    {
        if( atomic_exchange( &m_Flag, 1 ) == 0 )    // try lock
            return;
        poll_and_lock();    // someone has locked, wait and poll
    }
    bool try_lock() { return atomic_exchange( &m_Flag, 1 ) == 0; }
    void unlock()   { atomic_store( &m_Flag, 0 ); }

    // similar to std::lock_guard
    class Guard
    {
    public:
        explicit Guard( SpinMutex& mx ) { m_pMutex = &mx; mx.lock(); }
        ~Guard()            { if( m_pMutex ) { m_pMutex->unlock(); } }
        void unlock()       { if( m_pMutex ) { m_pMutex->unlock(); m_pMutex = nullptr; } }
        Guard(const Guard&) = delete;
        Guard& operator=(const Guard&) = delete;
    private:
        SpinMutex* m_pMutex;
    };
private:
    void poll_and_lock();
    volatile int m_Flag;        // 0: free, 1: locked
};

//======================================================================================================================
// Shared(Reader/Writer) spin mutex

class SpinSharedMutex : IrkNocopy
{
public:
    SpinSharedMutex() : m_Flag(0) {}
    ~SpinSharedMutex() { assert( m_Flag == 0 ); }

    // Exclusive ownership
    void lock();
    bool try_lock();
    void unlock();

    // Shared ownership
    void lock_shared(); 
    bool try_lock_shared();
    void unlock_shared();

    // similar to std::lock_guard
    class Guard
    {
    public:
        explicit Guard( SpinSharedMutex& mx )   { m_pMutex = &mx; mx.lock(); }
        ~Guard()                                { if( m_pMutex ) m_pMutex->unlock(); }
        void unlock()                           { if( m_pMutex ) { m_pMutex->unlock(); m_pMutex = nullptr; } }
        Guard(const Guard&) = delete;
        Guard& operator=(const Guard&) = delete;
    private:
        SpinSharedMutex* m_pMutex;
    };

    // similar to std::shared_lock
    class SharedGuard
    {
    public:
        explicit SharedGuard( SpinSharedMutex& mx ) { m_pMutex = &mx; mx.lock_shared(); }
        ~SharedGuard()                              { if( m_pMutex ) { m_pMutex->unlock_shared(); } }
        void unlock()                               { if( m_pMutex ) { m_pMutex->unlock_shared(); m_pMutex = nullptr; } }
        SharedGuard(const SharedGuard&) = delete;
        SharedGuard& operator=(const SharedGuard&) = delete;
    private:
        SpinSharedMutex* m_pMutex;
    };

private:
    volatile int m_Flag;
};

//=====================================================================================================================
// FIFO spin mutex, lock mutex in FIFO manner

// mutex waiter, must be created in current thread's stack !
class FifoMxWaiter : IrkNocopy
{
    friend class    SpinFifoMutex;
    int volatile    m_Flag;         // 1: signaled
    void* volatile  m_pNext;        // next waiter    
};

class SpinFifoMutex : IrkNocopy
{
public:
    SpinFifoMutex() : m_pTail(nullptr) {}
    ~SpinFifoMutex() { assert( m_pTail == nullptr ); }

    // NOTE: waiter must be created in current thread's stack
    void lock( FifoMxWaiter& waiter );
    bool try_lock( FifoMxWaiter& waiter );
    void unlock( FifoMxWaiter& waiter );

    // similar to std::lock_guard
    class Guard
    {
    public:
        Guard( SpinFifoMutex& mx, FifoMxWaiter& waiter )
        {
            m_pMutex = &mx;
            m_pWaiter = &waiter;
            mx.lock( waiter );
        }
        ~Guard()        { if( m_pMutex ) { m_pMutex->unlock( *m_pWaiter ); } }
        void unlock()   { if( m_pMutex ) { m_pMutex->unlock( *m_pWaiter ); m_pMutex = nullptr; } }
        Guard(const Guard&) = delete;
        Guard& operator=(const Guard&) = delete;
    private:
        SpinFifoMutex*  m_pMutex;
        FifoMxWaiter*   m_pWaiter;
    };
private:
    void* volatile m_pTail;
};

} // namespace irk

#endif
