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

#ifndef _IRONBRICK_THREADPOOL_H_
#define _IRONBRICK_THREADPOOL_H_

#include "IrkSyncQueue.h"
#include "IrkRefCntObj.h"

namespace irk {

// base class of async task
class IAsyncTask : public RefCntObj
{
public:
    IAsyncTask() {}
    virtual ~IAsyncTask() {}

    // worker thread calls this method to do actual work
    virtual void work() = 0;

    // worker thread calls this method to notify work completed or abandoned
    virtual void notify(bool /*done*/) {}
};

// base class of waitable async task
class IWaitableTask : public IAsyncTask
{
public:
    IWaitableTask() : m_done(false), m_waitEvt(true) {}

    void notify(bool done) override
    {
        m_done = done;
        m_waitEvt.set();
    }
    void wait() { m_waitEvt.wait(); }
    bool completed() const { return m_done; }
protected:
    std::atomic_bool m_done;
    SyncEvent m_waitEvt;
};

// base class of notifiable async task
class INotifyTask : public IAsyncTask
{
public:
    typedef void(*PFN_Notify)(bool done, void* param);
    INotifyTask() : m_pfnNotify(nullptr), m_notifyParam(nullptr) {}

    void notify(bool done) override
    {
        if (m_pfnNotify)
            (*m_pfnNotify)(done, m_notifyParam);
    }
    void set_notify_callback(PFN_Notify callback, void* cbParam)
    {
        m_pfnNotify = callback;
        m_notifyParam = cbParam;
    }
protected:
    PFN_Notify  m_pfnNotify;
    void*       m_notifyParam;
};

template<class FUNC>
class WaitableTaskTPL : public IWaitableTask
{
public:
    explicit WaitableTaskTPL(FUNC func) : m_func(func) {}
    void work() override
    {
        m_waitEvt.reset();
        m_func();
    }
private:
    FUNC m_func;
};

template<class FUNC>
class NotifyTaskTPL : public INotifyTask
{
public:
    explicit NotifyTaskTPL(FUNC func) : m_func(func) {}
    void work() override
    {
        m_func();
    }
private:
    FUNC m_func;
};

// make async task from function/functor/lambda
template<class F>
inline RefPtr<IWaitableTask> make_waitable_task(F func)
{
    IWaitableTask* ptask = new WaitableTaskTPL<F>(func);
    return RefPtr<IWaitableTask>(ptask);
}
template<class F>
inline RefPtr<INotifyTask> make_notify_task(F func)
{
    INotifyTask* ptask = new NotifyTaskTPL<F>(func);
    return RefPtr<INotifyTask>(ptask);
}

//======================================================================================================================

class WorkerThread;     // worker thread, for internal usage

typedef WaitableQueue<IAsyncTask*> AsyncTaskQueue;  // thread-safe queue used to store async tasks

class ThreadPool : IrkNocopy
{
public:
    explicit ThreadPool(size_t queueSize = 64) : m_workers(nullptr), m_workerCnt(0), m_taskQueue(queueSize)
    {}
    ~ThreadPool()
    {
        this->shutdown();
        assert(m_taskQueue.count() == 0);
    }

    // init this thread pool, must be called first
    // threadCnt: number of threads in this thread pool, 0 means cpu core count
    bool setup(int threadCnt = 0);

    // shutdown this thread pool
    // wait all launched tasks completed, abandon all pending tasks!
    // this thread pool can not be used anymore after shutdown
    void shutdown();

    // run async task in the thread pool
    // return false if thread pool has been shutdown
    bool run_task(IAsyncTask* ptask)
    {
        if (m_workerCnt == 0)  // thread pool already shutdown
            return false;

        ptask->add_ref();       // retain task
        if (!m_taskQueue.force_push_back(ptask))
        {
            ptask->dismiss();
            return false;
        }
        return true;
    }
    bool run_task(const RefPtr<IAsyncTask>& task)
    {
        return this->run_task(task.pointer());
    }
    bool run_task(RefPtr<IAsyncTask>&& task)
    {
        return this->run_task(task.detach());
    }

    // unlaunched tasks pending in this thread pool
    int pending_count() const
    {
        return m_taskQueue.count();
    }

private:
    WorkerThread * m_workers;
    int             m_workerCnt;
    AsyncTaskQueue  m_taskQueue;
};

// simple utility used to run and wait a bundle of tasks
class TaskGroup : IrkNocopy
{
public:
    explicit TaskGroup(ThreadPool* pool) : m_pending(0), m_pThrPool(pool)
    {}
    ~TaskGroup()
    {
        // NOTE: destroy task group is undefined if any task is pending, call wait() first
        assert(m_pending == 0);
    }
    void set_thread_pool(ThreadPool* pool)
    {
        // NOTE: setting thread pool is undefined if any task is pending in this group
        m_pThrPool = pool;
    }

    // run async task in the thread pool, return false if thread pool has been shutdown
    bool run_task(INotifyTask* ptask);
    bool run_task(const RefPtr<INotifyTask>& task)
    {
        return this->run_task(task.pointer());
    }
    bool run_task(RefPtr<IAsyncTask>&& task)
    {
        return this->run_task(task.detach());
    }

    // run general function/functor/lambda int the thread pool
    template<class F>
    bool run(F func)
    {
        static_assert(!std::is_convertible<F, IAsyncTask*>::value, "");
        static_assert(!std::is_base_of<RefPtr<INotifyTask>, std::remove_reference_t<F> >::value, "");
        INotifyTask* ptask = new NotifyTaskTPL<F>(func);
        return this->run_task(ptask);
    }

    // wait all pending tasks in this group completed or abandoned
    // NOTE: while waiting, adding new task is undefined
    void wait();

private:
    static void notify_callback(bool, void*);
    void notify_one(bool);
    int                     m_pending;
    std::mutex              m_mutex;
    std::condition_variable m_cvDone;
    ThreadPool*             m_pThrPool;
};

}   // namespace irk
#endif
