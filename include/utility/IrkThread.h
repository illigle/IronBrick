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

#ifndef _IRONBRICK_THREAD_H_
#define _IRONBRICK_THREAD_H_

#include "IrkSyncUtility.h"

namespace irk {

// get cpu core count in the system
extern int cpu_core_count();

// base class of OS thread
class OSThread : IrkNocopy
{
public:
    // yield current thread
    static void yield();

    // sleep current thread for milliseconds
    static void sleep(int milliseconds);

    explicit OSThread();
    virtual ~OSThread();

    // launch thread
    // return 0 on success, return errno if failed
    int launch();

    // wait thread exit and close handle
    void join();

    // detach thread and close handle
    void detach();

    // wait thread eixt for at most specified milliseconds
    // if thread exited, close handle and return true
    bool wait_exit(int milliseconds);

    // kill thread
    void kill();

    // is thread still running
    bool alive() const;

    // native thread handle
    void* native_handle() const { return m_hThread; }

private:
    virtual void thread_proc() = 0;     // OVERRIDE this method

    void* m_hThread;    // native thread handle

#ifdef _WIN32
    static unsigned __stdcall thread_routine(void* param);
#else
    SyncEvent m_evExited;
    static void* thread_routine(void* param);
#endif
};

}   // namespace irk
#endif
