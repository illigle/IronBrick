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
#include <process.h>
#else
#include <unistd.h>
#include <sched.h>
#include <pthread.h>
#include <signal.h>
#endif
#include <errno.h>
#include "IrkThread.h"

namespace irk {

#ifdef _WIN32

int cpu_core_count()
{
    SYSTEM_INFO si;
    ::GetSystemInfo( &si );
    if( si.dwNumberOfProcessors > 1 )
        return (int)si.dwNumberOfProcessors;
    return 1;
}

// yield current thread
void OSThread::yield()
{
    ::SwitchToThread();
}

// sleep current thread for milliseconds
void OSThread::sleep( int milliseconds )
{
    ::Sleep( milliseconds );
}

unsigned __stdcall OSThread::thread_routine( void* param )
{
    OSThread* pthrd = static_cast<OSThread*>(param);
    pthrd->thread_proc();
    return 0;
}

OSThread::OSThread() : m_hThread(NULL)
{}
OSThread::~OSThread()
{
    if( m_hThread )
    {
        ::WaitForSingleObject( m_hThread, INFINITE );
        ::CloseHandle( m_hThread );
    }
}

int OSThread::launch()
{
    assert( !m_hThread );
    unsigned int threadId = 0;
    m_hThread = (HANDLE)::_beginthreadex( NULL, 0, &thread_routine, this, 0, &threadId );
    if( !m_hThread )
        return errno;
    return 0;
}

void OSThread::join()
{
    if( m_hThread )
    {
        ::WaitForSingleObject( m_hThread, INFINITE );
        ::CloseHandle( m_hThread );
        m_hThread = NULL;
    }
}

void OSThread::detach()
{
    if( m_hThread )
    {
        ::CloseHandle( m_hThread );
        m_hThread = NULL;
    }
}

bool OSThread::wait_exit( int milliseconds )
{
    if( !m_hThread )
        return true;

    if( ::WaitForSingleObject( m_hThread, milliseconds ) == WAIT_OBJECT_0 )
    {
        ::CloseHandle( m_hThread );
        m_hThread = NULL;
        return true;
    }
    return false;
}

void OSThread::kill()
{
    if( m_hThread )
    {
        ::TerminateThread( m_hThread, -1 );
        ::CloseHandle( m_hThread );
        m_hThread = NULL;
    }
}

bool OSThread::alive() const
{
    if( m_hThread )
    {
        DWORD state = STILL_ACTIVE;
        ::GetExitCodeThread( m_hThread, &state );
        return state == STILL_ACTIVE;
    }
    return false;
}

#else   // POSIX

int cpu_core_count()
{
    long cores = ::sysconf( _SC_NPROCESSORS_ONLN );
    if( cores < 1 )
        return 1;
    return cores;
}

// yield current thread
void OSThread::yield()
{
#ifdef __linux__
    ::pthread_yield();
#else
    ::sched_yield();
#endif
}

// sleep current thread for milliseconds
void OSThread::sleep( int milliseconds )
{
    ::usleep( milliseconds * 1000 );
}

void* OSThread::thread_routine( void* param )
{
    OSThread* pthrd = static_cast<OSThread*>(param);
    pthrd->thread_proc();
    pthrd->m_evExited.set();    // notify thread exit
    return NULL;
}

OSThread::OSThread() : m_evExited(true), m_hThread(NULL)
{}
OSThread::~OSThread()
{
    if( m_hThread )
        ::pthread_join( (pthread_t)m_hThread, NULL );
}

int OSThread::launch()
{
    static_assert( sizeof(void*) >= sizeof(pthread_t) && alignof(void*) >= alignof(pthread_t), "" );
    assert( !m_hThread );

    m_evExited.reset();
    int errc = ::pthread_create( (pthread_t*)&m_hThread, NULL, &thread_routine, this );
    if( errc != 0 )
        m_hThread = NULL;
    return errc;
}

void OSThread::join()
{
    if( m_hThread )
    {
        ::pthread_join( (pthread_t)m_hThread, NULL );
        m_hThread = NULL;
    }
}

void OSThread::detach()
{
    if( m_hThread )
    {
        ::pthread_detach( (pthread_t)m_hThread );
        m_hThread = NULL;
    }
}

bool OSThread::wait_exit( int milliseconds )
{
    if( !m_hThread )
        return true;

    if( m_evExited.wait_for( milliseconds ) == WaitStatus::Ok )
    {
        ::pthread_join( (pthread_t)m_hThread, NULL );
        m_hThread = NULL;
        return true;
    }
    return false;
}

void OSThread::kill()
{
    if( m_hThread )
    {
        ::pthread_kill( (pthread_t)m_hThread, SIGKILL );
        ::pthread_join( (pthread_t)m_hThread, NULL );
        m_hThread = NULL;
    }
}

bool OSThread::alive() const
{
    if( m_hThread )
        return ::pthread_kill( (pthread_t)m_hThread, 0 ) == 0;
    return false;
}

#endif

}   // namespace irk
