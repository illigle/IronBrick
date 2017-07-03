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

#include "IrkThreadPool.h"
#include "IrkThread.h"

namespace irk {

class WorkerThread : public OSThread
{
public:
    WorkerThread() : m_pTaskQueue(nullptr) {}
    void set_task_queue( AsyncTaskQueue* pQueue ) { m_pTaskQueue = pQueue; }
private:
    void thread_proc() override;
    AsyncTaskQueue* m_pTaskQueue;
};

void WorkerThread::thread_proc()
{
    assert( m_pTaskQueue );
    IAsyncTask* pTask = nullptr;

    while( !m_pTaskQueue->is_closed() )
    {
        WaitStatus status = m_pTaskQueue->pop_front_wait( &pTask );
        if( status != WaitStatus::Ok )
            break;

        assert( pTask );
        pTask->work();       // do actual work
        pTask->notify( true );  // notify task completed
        pTask->dismiss();
    }
}

// init thread pool
bool ThreadPool::setup( int threadCnt )
{
    if( threadCnt <= 0 )
        threadCnt = cpu_core_count();   // cpu cores in the system

    // launch worker threads
    assert( m_workers == nullptr && m_workerCnt == 0 );
    m_workers = new WorkerThread[threadCnt];
    for( int i = 0; i < threadCnt; i++ )
    {
        m_workers[i].set_task_queue( &m_taskQueue );
        if( m_workers[i].launch() != 0 )
            break;
        m_workerCnt++;
    }

    if( m_workerCnt < threadCnt )   // failed
    {
        for( int i = 0; i < m_workerCnt; i++ )
            m_workers[i].kill();
        delete[] m_workers;
        m_workers = nullptr;
        m_workerCnt = 0;
        return false;
    }

    return true;
}

// shutdown thread pool
void ThreadPool::shutdown()
{
    int nWorkers = atomic_exchange( &m_workerCnt, 0 );
    if( nWorkers > 0 )
    {
        // close task queue, make all worker threads exit
        m_taskQueue.close();

        // wait all worker threads exited
        for( int i = 0; i < nWorkers; i++ )
            m_workers[i].join();
        delete[] m_workers;
        m_workers = nullptr;

        // discard pending tasks
        IAsyncTask* pTask = nullptr;
        while( m_taskQueue.pop_front( &pTask ) )
        {
            pTask->notify( false );     // notify task abandoned
            pTask->dismiss();
        }
    }
}

//======================================================================================================================
bool TaskGroup::run_task( INotifyTask* ptask )
{
    assert( m_pThrPool );
    ptask->set_notify_callback( TaskGroup::notify_callback, this );

    m_mutex.lock();
    m_pending++;
    m_mutex.unlock();

    if( !m_pThrPool->run_task( ptask ) )    // failed if thread pool shutdown
    {
        m_mutex.lock();
        m_pending--;
        m_mutex.unlock();
        return false;
    }
    return true;
}

void TaskGroup::notify_callback( bool done, void* param )
{
    TaskGroup* pgrp = (TaskGroup*)param;
    pgrp->notify_one( done );
}

void TaskGroup::notify_one( bool /*done*/)
{
    std::unique_lock<std::mutex> lock_( m_mutex );
    assert( m_pending > 0 );
    if( --m_pending == 0 )      // all pending tasks done
    {
        lock_.unlock();
        m_cvDone.notify_all();
    }
}

// wait all pending tasks done
void TaskGroup::wait()
{
    std::unique_lock<std::mutex> lock_( m_mutex );
    while( m_pending > 0 )
        m_cvDone.wait( lock_ );
}

}   // namespace irk
