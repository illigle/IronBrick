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

#ifndef _IRONBRICK_SYNCUTILITY_H_
#define _IRONBRICK_SYNCUTILITY_H_

#include "IrkCommon.h"

namespace irk {

// Similar to std::mutex, defined here only for completeness
// NOTE: can only be used to sync between threads
class Mutex : IrkNocopy
{
public:
    Mutex();            // NOTE: throw std::system_error if get OS resource failed
    ~Mutex();
    void lock();
    bool try_lock();
    void unlock();

    // similar to std::lock_guard
    class Guard
    {
    public:
        explicit Guard( Mutex& mx ) { m_pMutex = &mx; mx.lock(); }
        ~Guard()            { if( m_pMutex ) { m_pMutex->unlock(); } }
        void unlock()       { if( m_pMutex ) { m_pMutex->unlock(); m_pMutex = nullptr; } }
        Guard(const Guard&) = delete;
        Guard& operator=(const Guard&) = delete;
    private:
        Mutex* m_pMutex;
    };

    void* native_handle()   
    {
#ifdef _WIN32
        return (void*)&m_Handle;
#else
        return m_Handle;
#endif
    }
private:
    void* m_Handle;
};

// C++11 does not have shared_mutex
// NOTE: can only be used to sync between threads
class SharedMutex : IrkNocopy
{
public:
    SharedMutex();      // NOTE: throw std::system_error if get OS resource failed
    ~SharedMutex();

    // Exclusive ownership, others can not read or write
    void lock();
    bool try_lock();
    void unlock();

    // Shared ownership, others can read but not write
    void lock_shared(); 
    bool try_lock_shared();
    void unlock_shared();

    // similar to std::lock_guard
    class Guard
    {
    public:
        explicit Guard( SharedMutex& mx )   { m_pMutex = &mx; mx.lock(); }
        ~Guard()                            { if( m_pMutex ) m_pMutex->unlock(); }
        void unlock()                       { if( m_pMutex ) { m_pMutex->unlock(); m_pMutex = nullptr; } }
        Guard(const Guard&) = delete;
        Guard& operator=(const Guard&) = delete;
    private:
        SharedMutex* m_pMutex;
    };

    // similar to std::shared_lock
    class SharedGuard
    {
    public:
        explicit SharedGuard( SharedMutex& mx ) { m_pMutex = &mx; mx.lock_shared(); }
        ~SharedGuard()                          { if( m_pMutex ) { m_pMutex->unlock_shared(); } }
        void unlock()                           { if( m_pMutex ) { m_pMutex->unlock_shared(); m_pMutex = nullptr; } }
        SharedGuard(const SharedGuard&) = delete;
        SharedGuard& operator=(const SharedGuard&) = delete;
    private:
        SharedMutex* m_pMutex;
    };

    void* native_handle()   
    {
#ifdef _WIN32
        return (void*)&m_Handle;
#else
        return m_Handle;
#endif
    }
private:
    void* m_Handle;
};

// Condition Variable and Event wait result
enum class WaitStatus
{
    Ok = 0,             // succeeded        
    Timeout,            // timeout
    OwnerDead,          // previous owner is dead
    Closed,             // sync object closed
    Failed,             // other os error
};

// Condition Variable
// NOTE: can only be used to sync between threads
class CondVar : IrkNocopy
{
public:
    CondVar();      // NOTE: throw std::system_error if get OS resource failed
    ~CondVar();

    // wait this condition variable notified
    WaitStatus wait( Mutex& mx );

    // wait this condition variable notified, wait at most milliseconds
    WaitStatus wait_for( Mutex& mx, int milliseconds );

    // if any threads are waiting on this condition variable, unblocks one of the waiting threads
    void notify_one();

    // unblocks all threads currently waiting for this condition variable. 
    void notify_all();

private:
    void* m_Handle;
};

// Anonymous event, model Win32 Event
class SyncEvent : IrkNocopy
{
public:
    explicit SyncEvent( bool bManualReset );    // NOTE: throw std::system_error if get OS resource failed
    ~SyncEvent();

    // wait event signaled forever
    WaitStatus wait();

    // wait event signaled, wait at most milliseconds
    WaitStatus wait_for( int milliseconds );

    // try wait event, doen not block, return true if event has signaled
    bool try_wait();

    // set event to signaled status
    bool set();

    // reset event status
    bool reset();

private:
    void* m_Handle;
};

// Counted event, threads may block until the internal counter is decremented to zero
class CounterEvent
{
public:
    CounterEvent();     // NOTE: throw std::system_error if get OS resource failed
    ~CounterEvent();

    // set internal counter(must > 0), can be called many times.
    // NOTE: it is undefined if called when any thread is blocking on this object
    void reset( int count );

    // decrements the internal counter by 1 and if necessary blocks until the counter reaches zero
    // NOTE: it is undefined if resulting counter is less than zero
    void dec_and_wait();

    // decrements the internal counter but never block
    // NOTE: it is undefined if resulting counter is less than zero
    void dec( int val = 1 );

    // blocks until the internal counter reaches zero
    void wait();

    // test if internal counter == 0
    bool ready() const;

private:
    void* m_Handle;
};

} // namespace irk
#endif
