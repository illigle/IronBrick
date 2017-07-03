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

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <string>
#endif
#include <system_error>
#include "IrkAtomic.h"
#include "IrkSyncUtility.h"

namespace irk {

//======================================================================================================================
#ifdef _WIN32

Mutex::Mutex()
{
    static_assert( sizeof(SRWLOCK) == sizeof(void*), "" );
    static_assert( alignof(SRWLOCK) == alignof(void*), "" );
    ::InitializeSRWLock( (PSRWLOCK)&m_Handle );
}

Mutex::~Mutex()
{
}

void Mutex::lock()
{
    ::AcquireSRWLockExclusive( (PSRWLOCK)&m_Handle );
}

bool Mutex::try_lock()
{
    return ::TryAcquireSRWLockExclusive( (PSRWLOCK)&m_Handle ) != 0;
}

void Mutex::unlock()
{
    ::ReleaseSRWLockExclusive( (PSRWLOCK)&m_Handle );
}

#else       // linux or Mac OSX

Mutex::Mutex() : m_Handle(NULL)
{
    // set pthread mutex attributes
    pthread_mutexattr_t muxattr;
    if( pthread_mutexattr_init( &muxattr ) != 0 )
    {
        std::error_code err( errno, std::system_category() );
        throw std::system_error( err, "pthread_mutexattr_init failed" );
    }
    pthread_mutexattr_setpshared( &muxattr, PTHREAD_PROCESS_PRIVATE );
    pthread_mutexattr_settype( &muxattr, PTHREAD_MUTEX_RECURSIVE );

    // create pthread mutex
    pthread_mutex_t* pMutex = new pthread_mutex_t;
    int initRes = pthread_mutex_init( pMutex, &muxattr );
    pthread_mutexattr_destroy( &muxattr );
    if( initRes != 0 )  // failed
    {
        delete pMutex;
        std::error_code err( errno, std::system_category() );
        throw std::system_error( err, "pthread_mutex_init failed" );
    }
    m_Handle = pMutex;
}

Mutex::~Mutex()
{
    if( m_Handle )
    {
        pthread_mutex_destroy( (pthread_mutex_t*)m_Handle );
        delete (pthread_mutex_t*)m_Handle;
    }
}

void Mutex::lock()
{
    if( m_Handle )
    {
        pthread_mutex_lock( (pthread_mutex_t*)m_Handle );
    }
}

bool Mutex::try_lock()
{
    if( m_Handle )
    {
        if( pthread_mutex_trylock( (pthread_mutex_t*)m_Handle ) == 0 )
            return true;
    }
    return false;
}

void Mutex::unlock()
{
    if( m_Handle )
    {
        pthread_mutex_unlock( (pthread_mutex_t*)m_Handle );
    }
}

#endif

//======================================================================================================================
// shared mutex

#ifdef _WIN32

SharedMutex::SharedMutex()
{
    static_assert( sizeof(SRWLOCK) == sizeof(void*), "" );
    static_assert( alignof(SRWLOCK) == alignof(void*), "" );
    ::InitializeSRWLock( (PSRWLOCK)&m_Handle );
}

SharedMutex::~SharedMutex()
{
}

// get exclusive ownership, others can not read or write
void SharedMutex::lock()
{
    ::AcquireSRWLockExclusive( (PSRWLOCK)&m_Handle );
}

bool SharedMutex::try_lock()
{
    return ::TryAcquireSRWLockExclusive( (PSRWLOCK)&m_Handle ) != 0;
}

void SharedMutex::unlock()
{
    ::ReleaseSRWLockExclusive( (PSRWLOCK)&m_Handle );
}

// get shared ownership, others can read but not write
void SharedMutex::lock_shared()
{
    ::AcquireSRWLockShared( (PSRWLOCK)&m_Handle );
}

bool SharedMutex::try_lock_shared()
{
    return ::TryAcquireSRWLockShared( (PSRWLOCK)&m_Handle ) != 0;
}

void SharedMutex::unlock_shared()
{
    ::ReleaseSRWLockShared( (PSRWLOCK)&m_Handle );
}

#else   // linux or Mac OSX

SharedMutex::SharedMutex() : m_Handle( NULL )
{
    // set pthread read-write mutex attributes
    pthread_rwlockattr_t rwAttr;
    if( pthread_rwlockattr_init( &rwAttr ) != 0 )
    {
        std::error_code err( errno, std::system_category() );
        throw std::system_error( err, "pthread_rwlockattr_init failed" );
    }
    pthread_rwlockattr_setpshared( &rwAttr, PTHREAD_PROCESS_PRIVATE );

    // create pthread read-write mutex
    pthread_rwlock_t* pRwLock = new pthread_rwlock_t;
    int initRes = pthread_rwlock_init( pRwLock, &rwAttr );
    pthread_rwlockattr_destroy( &rwAttr );
    if( initRes != 0 )
    {
        delete pRwLock;
        std::error_code err( errno, std::system_category() );
        throw std::system_error( err, "pthread_rwlock_init failed" );
    }
    m_Handle = pRwLock;
}

SharedMutex::~SharedMutex()
{
    if( m_Handle )
    {
        pthread_rwlock_destroy( (pthread_rwlock_t*)m_Handle );
        delete (pthread_rwlock_t*)m_Handle;
        m_Handle = NULL;
    }
}

// get exclusive ownership, others can not read or write
void SharedMutex::lock()
{
    if( m_Handle )
    {
        pthread_rwlock_wrlock( (pthread_rwlock_t*)m_Handle );
    }
}

bool SharedMutex::try_lock()
{
    if( m_Handle )
    {
        return pthread_rwlock_trywrlock( (pthread_rwlock_t*)m_Handle ) == 0;
    }
    return false;
}

void SharedMutex::unlock()
{
    if( m_Handle )
    {
        pthread_rwlock_unlock( (pthread_rwlock_t*)m_Handle );
    }
}

// get shared ownership, others can read but not write
void SharedMutex::lock_shared()
{
    if( m_Handle )
    {
        pthread_rwlock_rdlock( (pthread_rwlock_t*)m_Handle );
    }
}

bool SharedMutex::try_lock_shared()
{
    if( m_Handle )
    {
        return pthread_rwlock_tryrdlock( (pthread_rwlock_t*)m_Handle ) == 0;
    }
    return false;
}

void SharedMutex::unlock_shared()
{
    if( m_Handle )
    {
        pthread_rwlock_unlock( (pthread_rwlock_t*)m_Handle );
    }
}

#endif

//======================================================================================================================

// Mac OSX does NOT have POSIX clock_gettime
#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>

#define CLOCK_REALTIME  0x01
#define CLOCK_MONOTONIC 0x02

static int clock_gettime( int clockId, struct timespec* pts )
{
    (void)clockId;
    clock_serv_t clksrv;
    mach_timespec_t mts;
    host_get_clock_service( mach_host_self(), CALENDAR_CLOCK, &clksrv );
    clock_get_time( clksrv, &mts );
    mach_port_deallocate( mach_task_self(), clksrv );
    pts->tv_sec = mts.tv_sec;
    pts->tv_nsec = mts.tv_nsec;
    return 0;
}
#endif

//======================================================================================================================
// Condition variable

#ifdef _WIN32

CondVar::CondVar() : m_Handle(NULL)
{
    static_assert( sizeof(void*) >= sizeof(CONDITION_VARIABLE) && alignof(void*) >= alignof(CONDITION_VARIABLE), "" );
    ::InitializeConditionVariable( (PCONDITION_VARIABLE)&m_Handle );
}
CondVar::~CondVar()
{
}

WaitStatus CondVar::wait( Mutex& mx )
{
    if( ::SleepConditionVariableSRW( (PCONDITION_VARIABLE)&m_Handle, 
                                    (PSRWLOCK)mx.native_handle(), INFINITE, 0 ) )
    {
        return WaitStatus::Ok;
    }

    if( ::GetLastError() == ERROR_ABANDONED_WAIT_0 )
        return WaitStatus::OwnerDead;
    return WaitStatus::Failed;
}

WaitStatus CondVar::wait_for( Mutex& mx, int milliseconds )
{
    if( ::SleepConditionVariableSRW( (PCONDITION_VARIABLE)&m_Handle, 
                                    (PSRWLOCK)mx.native_handle(), milliseconds, 0 ) )
    {
        return WaitStatus::Ok;
    }

    const DWORD errc = ::GetLastError();
    if( errc == ERROR_TIMEOUT )
        return WaitStatus::Timeout;
    else if( errc == ERROR_ABANDONED_WAIT_0 )
        return WaitStatus::OwnerDead;
    return WaitStatus::Failed;
}

void CondVar::notify_one()
{
    ::WakeConditionVariable( (PCONDITION_VARIABLE)&m_Handle );
}

void CondVar::notify_all()
{
    ::WakeAllConditionVariable( (PCONDITION_VARIABLE)&m_Handle );
}

#else

CondVar::CondVar() : m_Handle(nullptr)
{
    pthread_cond_t* pcond = new pthread_cond_t;
    if( ::pthread_cond_init( pcond, nullptr ) != 0 )
    {
        delete pcond;
        std::error_code err( errno, std::system_category() );
        throw std::system_error( err, "pthread_cond_init failed" );
    }
    m_Handle = pcond;
}

CondVar::~CondVar()
{
    if( m_Handle )
    {
        ::pthread_cond_destroy( (pthread_cond_t*)m_Handle );
        delete (pthread_cond_t*)m_Handle;
    }
}

WaitStatus CondVar::wait( Mutex& mx )
{
    int res = ::pthread_cond_wait( (pthread_cond_t*)m_Handle, (pthread_mutex_t*)mx.native_handle() );
    if( res == 0 )
        return WaitStatus::Ok;
    else if( res == EOWNERDEAD )
        return WaitStatus::OwnerDead;
    return WaitStatus::Failed;
}

WaitStatus CondVar::wait_for( Mutex& mx, int milliseconds )
{
    struct timespec tmsp;
    if( clock_gettime( CLOCK_REALTIME, &tmsp ) != 0 )
        return WaitStatus::Failed;
    int secs = milliseconds / 1000;
    int nsecs = 1e6 * (milliseconds - secs * 1000);   
    if( nsecs < 1e9 - tmsp.tv_nsec )
    {
        tmsp.tv_sec += secs;
        tmsp.tv_nsec += nsecs;
    }
    else
    {
        tmsp.tv_sec += secs + 1;
        tmsp.tv_nsec = tmsp.tv_nsec + nsecs - 1e9;
    }

    const int res = ::pthread_cond_timedwait(
            (pthread_cond_t*)m_Handle,
            (pthread_mutex_t*)mx.native_handle(),
            &tmsp );
    if( res == 0 )
        return WaitStatus::Ok;
    else if( res == ETIMEDOUT )
        return WaitStatus::Timeout;
    else if( res == EOWNERDEAD )
        return WaitStatus::OwnerDead;
    return WaitStatus::Failed;
}

void CondVar::notify_one()
{
    ::pthread_cond_signal( (pthread_cond_t*)m_Handle );
}

void CondVar::notify_all()
{
    ::pthread_cond_broadcast( (pthread_cond_t*)m_Handle );
}

#endif

//======================================================================================================================
// Anonymous sync event

#ifdef _WIN32

SyncEvent::SyncEvent( bool bManualReset ) : m_Handle( NULL )
{
    m_Handle = ::CreateEvent( NULL, bManualReset, FALSE, NULL );
    if( !m_Handle )
    {
        std::error_code err( (int)GetLastError(), std::system_category() );
        throw std::system_error( err, "CreateEvent failed" );
    }
}

SyncEvent::~SyncEvent()
{
    if( m_Handle )
        ::CloseHandle( m_Handle );
}

WaitStatus SyncEvent::wait()
{
    DWORD waitRes = ::WaitForSingleObject( m_Handle, INFINITE );
    if( waitRes == WAIT_OBJECT_0 )
        return WaitStatus::Ok;
    else if( waitRes == WAIT_ABANDONED )
        return WaitStatus::OwnerDead;
    else
        return WaitStatus::Failed;
}

WaitStatus SyncEvent::wait_for( int milliseconds )
{
    assert( milliseconds >= 0 );
    DWORD waitRes = ::WaitForSingleObject( m_Handle, milliseconds );
    if( waitRes == WAIT_OBJECT_0 )
        return WaitStatus::Ok;
    else if( waitRes == WAIT_TIMEOUT )
        return WaitStatus::Timeout;
    else if( waitRes == WAIT_ABANDONED )
        return WaitStatus::OwnerDead;
    else
        return WaitStatus::Failed;
}

bool SyncEvent::try_wait()
{
    return ::WaitForSingleObject( m_Handle, 0 ) == WAIT_OBJECT_0;
}

bool SyncEvent::set()
{
    return ::SetEvent( m_Handle ) != FALSE;
}

bool SyncEvent::reset()
{
    return ::ResetEvent( m_Handle ) != FALSE;
}

#else   // linux or Mac OSX

class PosixEvent
{
public:
    explicit PosixEvent( bool bManual )
    {
        if( pthread_mutex_init( &m_Mutex, NULL ) != 0 )
        {
            std::error_code err( errno, std::system_category() );
            throw std::system_error( err, "pthread_mutex_init failed" );
        }
        if( pthread_cond_init( &m_CondVar, NULL ) != 0 )
        {
            pthread_mutex_destroy( &m_Mutex );
            std::error_code err( errno, std::system_category() );
            throw std::system_error( err, "pthread_cond_init failed" );
        }
        m_bManualReset = bManual;
        m_bSignaled = false;
    }
    ~PosixEvent()
    {
        pthread_cond_destroy( &m_CondVar );
        pthread_mutex_destroy( &m_Mutex );
    }

    bool set()
    {
        pthread_mutex_lock( &m_Mutex );
        m_bSignaled = true;
        pthread_mutex_unlock( &m_Mutex );

        if( m_bManualReset )
            pthread_cond_broadcast( &m_CondVar );
        else
            pthread_cond_signal( &m_CondVar );      // auto-reset event, notify only one
        return true;
    }

    bool reset()
    {
        pthread_mutex_lock( &m_Mutex );
        m_bSignaled = false;
        pthread_mutex_unlock( &m_Mutex );
        return true;
    }

    WaitStatus wait();

    WaitStatus wait_for( int milliseconds );
    
    bool try_wait()
    {
        pthread_mutex_lock( &m_Mutex );
        if( m_bSignaled )       // event has signaled
        {
            if( !m_bManualReset )
                m_bSignaled = false;
            pthread_mutex_unlock( &m_Mutex );
            return true;
        }
        else
        {
            pthread_mutex_unlock( &m_Mutex );
            return false;
        }
    }
private:
    pthread_mutex_t m_Mutex;
    pthread_cond_t  m_CondVar;
    bool            m_bManualReset;
    volatile bool   m_bSignaled;
};

WaitStatus PosixEvent::wait()
{
    pthread_mutex_lock( &m_Mutex );

    if( m_bSignaled )               // event has signaled
    {
        if( !m_bManualReset )       // auto-reset
            m_bSignaled = false;
        pthread_mutex_unlock( &m_Mutex );
        return WaitStatus::Ok;
    }
    else
    {
        int res = 0;
        do
        {
            res = pthread_cond_wait( &m_CondVar, &m_Mutex );
        }
        while( !m_bSignaled && res == 0 );

        if( res == 0 && !m_bManualReset )   // auto-reset
        {
            assert( m_bSignaled );
            m_bSignaled = false;
        }
        pthread_mutex_unlock( &m_Mutex );

        if( res == 0 )
            return WaitStatus::Ok;
        else if( res == EOWNERDEAD )
            return WaitStatus::OwnerDead;
        return WaitStatus::Failed;
    }
}

WaitStatus PosixEvent::wait_for( int milliseconds )
{
    pthread_mutex_lock( &m_Mutex );

    if( m_bSignaled )               // event has signaled
    {
        if( !m_bManualReset )       // auto-reset
            m_bSignaled = false;
        pthread_mutex_unlock( &m_Mutex );
        return WaitStatus::Ok;
    }
    else
    {
        // calc wait time
        struct timespec tmsp;
        if( clock_gettime( CLOCK_REALTIME, &tmsp ) != 0 )
            return WaitStatus::Failed;
        int secs = milliseconds / 1000;
        int nsecs = 1e6 * (milliseconds - secs * 1000);
        if( nsecs < 1e9 - tmsp.tv_nsec )
        {
            tmsp.tv_sec += secs;
            tmsp.tv_nsec += nsecs;
        }
        else
        {
            tmsp.tv_sec += secs + 1;
            tmsp.tv_nsec = tmsp.tv_nsec + nsecs - 1e9;
        }

        int res = 0;
        do
        {
            res = pthread_cond_timedwait( &m_CondVar, &m_Mutex, &tmsp );
        }
        while( !m_bSignaled && res == 0 );

        if( res == 0 && !m_bManualReset )   // auto-reset
        {
            assert( m_bSignaled );
            m_bSignaled = false;
        }
        pthread_mutex_unlock( &m_Mutex );

        if( res == 0 )
            return WaitStatus::Ok;
        else if( res == ETIMEDOUT )
            return WaitStatus::Timeout;
        else if( res == EOWNERDEAD )
            return WaitStatus::OwnerDead;
        return WaitStatus::Failed;
    }
}
    
SyncEvent::SyncEvent( bool bManualReset ) : m_Handle( NULL )
{
    m_Handle = new PosixEvent( bManualReset );
}

SyncEvent::~SyncEvent()
{
    delete (PosixEvent*)m_Handle;
}

WaitStatus SyncEvent::wait()
{
    PosixEvent* pimpl = (PosixEvent*)m_Handle;
    return pimpl->wait();
}

WaitStatus SyncEvent::wait_for( int milliseconds )
{
    assert( milliseconds >= 0 );
    PosixEvent* pimpl = (PosixEvent*)m_Handle;
    return pimpl->wait_for( milliseconds );
}

bool SyncEvent::try_wait()
{
    PosixEvent* pimpl = (PosixEvent*)m_Handle;
    return pimpl->try_wait();
}

bool SyncEvent::set()
{
    PosixEvent* pimpl = (PosixEvent*)m_Handle;
    return pimpl->set();
}

bool SyncEvent::reset()
{
    PosixEvent* pimpl = (PosixEvent*)m_Handle;
    return pimpl->reset();
}

#endif

//======================================================================================================================
// counted event

struct ImpCntedEvt
{
    ImpCntedEvt() : m_cnt(0), m_evt(true) {}
    volatile int    m_cnt;
    SyncEvent       m_evt;
};

CounterEvent::CounterEvent() : m_Handle(NULL)
{
    m_Handle = new ImpCntedEvt;
}
CounterEvent::~CounterEvent()
{
    delete (ImpCntedEvt*)m_Handle;
}

void CounterEvent::reset( int count )
{
    assert( count > 0 );
    ImpCntedEvt* pImpl = (ImpCntedEvt*)m_Handle;
    atomic_store( &pImpl->m_cnt, count );
    pImpl->m_evt.reset();
}

void CounterEvent::dec_and_wait()
{
    ImpCntedEvt* pImpl = (ImpCntedEvt*)m_Handle;

    if( atomic_dec( &pImpl->m_cnt ) == 0 )
        pImpl->m_evt.set();
    else
        pImpl->m_evt.wait();
}

void CounterEvent::dec( int val )
{
    ImpCntedEvt* pImpl = (ImpCntedEvt*)m_Handle;
    if( atomic_fetch_sub( &pImpl->m_cnt, val ) == val )
        pImpl->m_evt.set();
}

void CounterEvent::wait()
{
    ImpCntedEvt* pImpl = (ImpCntedEvt*)m_Handle;
    pImpl->m_evt.wait();
}

bool CounterEvent::ready() const
{
    const ImpCntedEvt* pImpl = (ImpCntedEvt*)m_Handle;
    return pImpl->m_cnt == 0;
}

} // namespace irk
