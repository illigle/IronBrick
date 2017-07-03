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

#ifndef _IRONBRICK_PROCESS_H_
#define _IRONBRICK_PROCESS_H_

#include "IrkCommon.h"
#include "IrkCmdLine.h"

namespace irk {

// manage OS process
class OSProcess : IrkNocopy
{
public:
    OSProcess() : m_handle(0) {}
    virtual ~OSProcess();

    // launch process
    // return 0 on success, return system error code if failed
    // "envp" is environment variable array of null-terminated strings, the array is terminated by a null pointer
    int launch( const char* cmdstr, const char* const* envp = nullptr, int flags = 0 );
    int launch( const CmdLine& cmdline, const char* const* envp = nullptr, int flags = 0 );

    // kill process and close handle
    // return 0 on success, return system error code if failed
    int kill();

    // wait process exit and close handle
    void wait();

    // wait process eixt for at most specified milliseconds
    // if process exited, close handle and return true
    bool wait_for( unsigned millisenconds );

    // check whether process is running
    bool alive();

    // get native process handle
    void* native_handle() const { return m_handle; }

private:
    void* m_handle;
};

}
#endif
