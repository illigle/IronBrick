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
#include <spawn.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#endif
#include <string.h>
#include "IrkProcess.h"

namespace irk {

OSProcess::~OSProcess()
{
#ifdef _WIN32
    if( m_handle )
    {
        ::CloseHandle( m_handle );
    }
#else
    if( m_handle )
    {
        pid_t pid = (pid_t)(intptr_t)m_handle;
        int status = 0;
        ::waitpid( pid, &status, WNOHANG ); // clear zombie
    }
#endif
}

// launch process
// return 0 on success, return system error code if failed
int OSProcess::launch( const char* cmdstr, const char* const* envp, int flags )
{
    assert( !m_handle );
#ifdef _WIN32
    STARTUPINFOA startInfo = {};
    PROCESS_INFORMATION procInfo = {};
    startInfo.cb = (DWORD)sizeof(startInfo);
    if( ::CreateProcessA(
        NULL,
        (LPSTR)cmdstr,
        NULL,
        NULL,
        FALSE,              // do NOT inherit handles
        flags,              // create flags
        (LPVOID)envp,       // enviroment variables
        NULL,               // current dir
        &startInfo,
        &procInfo ) )
    {
        m_handle = procInfo.hProcess;
        return 0;
    }
    return ::GetLastError();
#else
    CmdLine cmd;
    cmd.parse( cmdstr );
    return this->launch( cmd, envp, flags );
#endif
}

// launch process
// return 0 on success, return system error code if failed
int OSProcess::launch( const CmdLine& cmdline, const char* const* envp, int flags )
{
    assert( !m_handle );
#ifdef _WIN32
    std::string cmdstr = cmdline.to_string();
    return this->launch( cmdstr.c_str(), envp, flags );
#else
    static_assert( sizeof(pid_t) <= sizeof(void*) && alignof(pid_t) <= alignof(void*), "" );
    (void)flags;
    pid_t pid = 0;
    std::vector<const char*> argv( cmdline.arg_count() + 1, nullptr );
    for( unsigned i = 0; i < cmdline.arg_count(); i++ )
        argv[i] = cmdline[i].c_str();
    const char* pch = strrchr( argv[0], '/' );
    if( pch )
        argv[0] = pch + 1;      // filename only

    const int res = ::posix_spawn( &pid,
        cmdline[0].c_str(),
        NULL,
        NULL,
        (char**)argv.data(), 
        (char**)envp );
    if( res == 0 )
    {
        assert( pid != 0 );
        m_handle = (void*)(intptr_t)pid;
    }
    return res;
#endif
}

// kill process and close handle
// return 0 on success, return system error code if failed
int OSProcess::kill()
{
#ifdef _WIN32
    if( m_handle )
    {
        if( !::TerminateProcess( m_handle, -1 ) )
            return ::GetLastError();
        ::CloseHandle( m_handle );
        m_handle = nullptr;
    }
#else
    if( m_handle )
    {
        pid_t pid = (pid_t)(intptr_t)m_handle;
        if( ::kill( pid, SIGKILL ) != 0 )
            return errno;
        else
            m_handle = nullptr;
    }
#endif
    return 0;
}

// wait process exit and close handle
void OSProcess::wait()
{
#ifdef _WIN32
    if( m_handle )
    {
        if( ::WaitForSingleObject( m_handle, INFINITE ) == WAIT_OBJECT_0 )
        {
            ::CloseHandle( m_handle );
            m_handle = nullptr;
        }
    }
#else
    if( m_handle )
    {
        pid_t pid = (pid_t)(intptr_t)m_handle;
        pid_t wpid = 0;
        int status = 0;
        do 
        {
            wpid = ::waitpid( pid, &status, 0 );
        } 
        while( (wpid == -1) && (errno == EINTR) );

        if( wpid == pid )
        {
            m_handle = nullptr;
        }
    }
#endif
}

// wait process eixt for at most specified milliseconds
// if process exited, close handle and return true
bool OSProcess::wait_for( unsigned millisenconds )
{
#ifdef _WIN32
    if( m_handle )
    {
        DWORD res = ::WaitForSingleObject( m_handle, millisenconds );
        if( res == WAIT_OBJECT_0 )
        {
            ::CloseHandle( m_handle );
            m_handle = nullptr;
            return true;
        }
        return false;
    }
#else
    if( m_handle )
    {
        pid_t pid = (pid_t)(intptr_t)m_handle;
        int status = 0;
        while( 1 )
        {
            if( ::waitpid( pid, &status, WNOHANG ) == pid )     // already exited
            {
                m_handle = nullptr;
                return true;
            }

            if( millisenconds == 0 )
                return false;

            unsigned ms = millisenconds < 5 ? millisenconds : 5;
            millisenconds -= ms;
            ::usleep( ms * 1000 );
        }
    }
#endif
    return true;
}

// check whether process is running
bool OSProcess::alive()
{
#ifdef _WIN32
    if( m_handle )
    {
        DWORD exitCode = 0;
        if( ::GetExitCodeProcess( m_handle, &exitCode ) )
            return exitCode == STILL_ACTIVE;
    }
#else
    if( m_handle )
    {
        pid_t pid = (pid_t)(intptr_t)m_handle;
        int status = 0;
        if( ::waitpid( pid, &status, WNOHANG ) == 0 )
            return true;
        else
            m_handle = nullptr;
    }
#endif
    return false;
}

}   // namespace irk
