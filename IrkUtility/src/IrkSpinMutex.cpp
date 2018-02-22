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
#include <sched.h>
#endif
#include "IrkSpinMutex.h"

namespace {

class Spinner
{
public:
    explicit Spinner(unsigned maxSpinCnt = 256)
    {
        m_Cnt = 0;
        m_MaxSpinCnt = maxSpinCnt;
    }
    void reset() { m_Cnt = 0; }
    void pause();
private:
    unsigned m_Cnt;
    unsigned m_MaxSpinCnt;
};

void Spinner::pause()
{
    if (m_Cnt < m_MaxSpinCnt)
    {
        atomic_cpu_pause();
        ++m_Cnt;
    }
    else if (m_Cnt < m_MaxSpinCnt + 2)
    {
#ifdef _WIN32
        ::SwitchToThread();
#else
        ::sched_yield();
#endif
        ++m_Cnt;
    }
    else
    {
#ifdef _WIN32
        ::Sleep(15);
#else
        ::usleep(15000);
#endif
    }
}

}   // private namespace

namespace irk {

void SpinMutex::poll_and_lock()
{
    Spinner spin;
    while (1)
    {
        if (atomic_load(&m_Flag) == 0)   // double check to mitigate cache thrash
        {
            if (atomic_exchange(&m_Flag, 1) == 0)    // try lock
                return;
            spin.reset();
        }
        spin.pause();
    }
}

//======================================================================================================================
// Reader/Writer spin mutex

constexpr int WR_MASK = 0x1;
constexpr int PENDING_MASK = 0x2;
constexpr int WR_PENDING_MASK = (WR_MASK | PENDING_MASK);
constexpr int RD_MASK = ~WR_PENDING_MASK;
constexpr int RD_WR_MASK = (RD_MASK | WR_MASK);
constexpr int RD_ONE = 4;

void SpinSharedMutex::lock()
{
    Spinner spin;
    while (1)
    {
        int flag = atomic_load(&m_Flag);
        if ((flag & RD_WR_MASK) == 0)      // no reader and no writer
        {
            if (atomic_compare_swap(&m_Flag, flag, WR_MASK) == flag)
                return;
            spin.reset();
        }
        else if ((flag & PENDING_MASK) == 0)
        {
            atomic_fetch_or(&m_Flag, PENDING_MASK);
        }
        spin.pause();
    }
}

bool SpinSharedMutex::try_lock()
{
    int flag = atomic_load(&m_Flag);
    if ((flag & RD_WR_MASK) == 0)      // no reader and no writer
    {
        if (atomic_compare_swap(&m_Flag, flag, WR_MASK) == flag)
            return true;
    }
    return false;
}

void SpinSharedMutex::unlock()
{
    assert(m_Flag & WR_MASK);
    atomic_fetch_and(&m_Flag, ~WR_MASK);
}

void SpinSharedMutex::lock_shared()
{
    Spinner spin;
    while (1)
    {
        int flag = atomic_load(&m_Flag);
        if ((flag & WR_PENDING_MASK) == 0) // no writer and writer pending
        {
            flag = atomic_fetch_add(&m_Flag, RD_ONE);
            if ((flag & WR_MASK) == 0)
                return;

            atomic_fetch_sub(&m_Flag, RD_ONE);
            spin.reset();
        }
        spin.pause();
    }
}

bool SpinSharedMutex::try_lock_shared()
{
    int flag = atomic_load(&m_Flag);
    if ((flag & WR_PENDING_MASK) == 0)     // no writer and writer pending
    {
        flag = atomic_fetch_add(&m_Flag, RD_ONE);
        if ((flag & WR_MASK) == 0)
            return true;

        atomic_fetch_sub(&m_Flag, RD_ONE);
    }
    return false;
}

void SpinSharedMutex::unlock_shared()
{
    assert(m_Flag >= RD_ONE);
    atomic_fetch_sub(&m_Flag, RD_ONE);
}

//======================================================================================================================
// FIFO spin mutex

void SpinFifoMutex::lock(FifoMxWaiter& waiter)
{
    waiter.m_Flag = 0;
    waiter.m_pNext = nullptr;

    FifoMxWaiter* pPrev = (FifoMxWaiter*)atomic_exchange(&m_pTail, &waiter);
    if (pPrev)
    {
        // append to waiting queue
        atomic_store(&pPrev->m_pNext, &waiter);

        // wait for pPrev to release the mutex
        Spinner spin(16);
        while (atomic_load(&waiter.m_Flag) == 0)
            spin.pause();
    }
}

bool SpinFifoMutex::try_lock(FifoMxWaiter& waiter)
{
    if (atomic_load(&m_pTail) == nullptr)
    {
        waiter.m_Flag = 1;
        waiter.m_pNext = nullptr;
        if (atomic_compare_swap(&m_pTail, nullptr, &waiter) == nullptr)
            return true;
    }
    return false;
}

void SpinFifoMutex::unlock(FifoMxWaiter& waiter)
{
    assert(atomic_load(&m_pTail) != nullptr);

    void* pNext = atomic_load(&waiter.m_pNext);
    if (pNext == nullptr)  // no other waiters now
    {
        if (atomic_compare_swap(&m_pTail, &waiter, nullptr) == &waiter)
            return;

        // new waiter is comming, wait for him to link to this waiter
        Spinner spin(16);
        while ((pNext = atomic_load(&waiter.m_pNext)) == nullptr)
            spin.pause();
    }

    // signal next waiter
    assert(pNext != nullptr);
    FifoMxWaiter* pnxt = (FifoMxWaiter*)pNext;
    atomic_store(&pnxt->m_Flag, 1);
}

} // namespace irk
