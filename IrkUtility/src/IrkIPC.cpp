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
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>
#include <semaphore.h>
#endif
#ifdef __linux__
#include <mqueue.h>
#endif
#include <errno.h>
#include <string>
#include "IrkAtomic.h"
#include "IrkIPC.h"

#define ssizeof(x)  static_cast<int>( sizeof(x) )

namespace irk {

#ifdef _WIN32

Semaphore::Semaphore() : m_Handle(NULL)
{
}

Semaphore::~Semaphore()
{
    if (m_Handle)
        ::CloseHandle(m_Handle);
}

// create semaphore
IpcResult Semaphore::create(const char* semaName, int initVal)
{
    assert(!m_Handle);

    HANDLE hSema = ::CreateSemaphoreA(NULL, initVal, INT32_MAX, semaName);
    if (!hSema)
    {
        return IpcResult::make_failed(::GetLastError());
    }
    else if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        ::CloseHandle(hSema);
        return IpcResult::make_failed(ERROR_ALREADY_EXISTS);
    }

    m_Handle = hSema;
    return IpcResult::make_ok();
}

// open an existing semaphore
IpcResult Semaphore::open(const char* semaName)
{
    assert(!m_Handle);

    m_Handle = ::OpenSemaphoreA(SEMAPHORE_MODIFY_STATE | SYNCHRONIZE, FALSE, semaName);
    if (m_Handle)
        return IpcResult::make_ok();

    return IpcResult::make_failed(::GetLastError());
}

// close semaphore
void Semaphore::close()
{
    if (m_Handle)
    {
        ::CloseHandle(m_Handle);
        m_Handle = NULL;
    }
}

// post semaphore
IpcResult Semaphore::post()
{
    if (!m_Handle)
        return IpcResult::make_failed(ERROR_INVALID_HANDLE);

    if (::ReleaseSemaphore(m_Handle, 1, NULL))
        return IpcResult::make_ok();

    return IpcResult::make_failed(::GetLastError());
}

// wait semaphore posted
IpcResult Semaphore::wait(int milliseconds)
{
    if (!m_Handle)
    {
        return IpcResult::make_failed(ERROR_INVALID_HANDLE);
    }

    DWORD timeout = milliseconds >= 0 ? milliseconds : INFINITE;
    DWORD rst = ::WaitForSingleObject(m_Handle, timeout);
    if (rst == WAIT_OBJECT_0)
    {
        return IpcResult::make_ok();
    }
    else if (rst == WAIT_TIMEOUT)
    {
        return IpcResult::make_timeout(ERROR_TIMEOUT);
    }
    else
    {
        return IpcResult::make_failed(::GetLastError());
    }
}

// try wait semaphore, does not block
bool Semaphore::try_wait()
{
    if (m_Handle && ::WaitForSingleObject(m_Handle, 0) == WAIT_OBJECT_0)
        return true;
    return false;
}

//======================================================================================================================
IpcShmObj::IpcShmObj() : m_handle(nullptr)
{
}

IpcShmObj::~IpcShmObj()
{
    if (m_handle)
        ::CloseHandle(m_handle);
}

// create shared memory object
IpcResult IpcShmObj::create(const char* shmName, size_t size)
{
    assert(!m_handle);

    LARGE_INTEGER llsize;
    llsize.QuadPart = size;
    HANDLE hShm = ::CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
        llsize.HighPart, llsize.LowPart, shmName);
    if (!hShm)
    {
        return IpcResult::make_failed(::GetLastError());
    }
    else if (::GetLastError() == ERROR_ALREADY_EXISTS)
    {
        ::CloseHandle(hShm);
        return IpcResult::make_failed(ERROR_ALREADY_EXISTS);
    }

    m_handle = hShm;
    return IpcResult::make_ok();
}

// open existing shared memory object
IpcResult IpcShmObj::open(const char* shmName)
{
    assert(!m_handle);
    HANDLE hShm = ::OpenFileMappingA(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, shmName);
    if (!hShm)
        return IpcResult::make_failed(::GetLastError());

    m_handle = hShm;
    return IpcResult::make_ok();
}

// close/unlink shared memory object
void IpcShmObj::close()
{
    if (m_handle)
    {
        ::CloseHandle(m_handle);
        m_handle = NULL;
    }
}

IpcResult IpcShmObj::map(size_t offset, size_t size, void** outPtr)
{
    if (!m_handle)
    {
        *outPtr = nullptr;
        return IpcResult::make_failed(ERROR_INVALID_HANDLE);
    }

    LARGE_INTEGER lloff;
    lloff.QuadPart = offset;
    void* pbuf = ::MapViewOfFile(m_handle, FILE_MAP_READ | FILE_MAP_WRITE, lloff.HighPart, lloff.LowPart, size);
    if (pbuf)
    {
        *outPtr = pbuf;
        return IpcResult::make_ok();
    }

    *outPtr = nullptr;
    return IpcResult::make_failed(::GetLastError());
}

IpcResult IpcShmObj::unmap(void* buf, size_t)
{
    assert(m_handle && buf);

    if (::UnmapViewOfFile(buf))
        return IpcResult::make_ok();

    return IpcResult::make_failed(::GetLastError());
}

//======================================================================================================================
struct WinAioCtx : public OVERLAPPED
{
    WinAioCtx() : m_hNotify(NULL) {}
    ~WinAioCtx()
    {
        if (m_hNotify)
            ::CloseHandle(m_hNotify);
    }
    WinAioCtx(const WinAioCtx&) = delete;
    WinAioCtx& operator=(const WinAioCtx&) = delete;

    // prepare async io context, return system error code if failed
    int prepare()
    {
        if (!m_hNotify)
        {
            m_hNotify = ::CreateEventA(NULL, TRUE, FALSE, NULL);
            if (!m_hNotify)
                return (int)::GetLastError();
        }
        else
        {
            if (!::ResetEvent(m_hNotify))
                return (int)::GetLastError();
        }

        ::memset((OVERLAPPED*)this, 0, sizeof(OVERLAPPED));
        this->hEvent = this->m_hNotify;
        return 0;
    }

    HANDLE m_hNotify;   // notify event
};

// wait aysnc io completed
static IpcResult wait_aio_result(HANDLE hdl, OVERLAPPED& ovlp, int milliseconds, int& result)
{
    if (milliseconds < 0)      // wait forever
    {
        DWORD aioSize = 0;
        if (::GetOverlappedResult(hdl, &ovlp, &aioSize, TRUE))
        {
            result = (int)aioSize;
            return IpcResult::make_ok();
        }
        else
        {
            return IpcResult::make_failed(::GetLastError());
        }
    }
    else
    {
        ::WaitForSingleObject(ovlp.hEvent, milliseconds);

        DWORD aioSize = 0;
        if (::GetOverlappedResult(hdl, &ovlp, &aioSize, FALSE))
        {
            result = (int)aioSize;
            return IpcResult::make_ok();
        }
        else
        {
            int errc = ::GetLastError();
            if (errc == ERROR_IO_INCOMPLETE)   // still in progress
                return IpcResult::make_timeout(errc);
            else
                return IpcResult::make_failed(errc);
        }
    }
}

// wait connected by a client
static IpcResult pipe_accept(HANDLE hPipe, WinAioCtx& aioCtx, int milliseconds)
{
    int errc = aioCtx.prepare();
    if (errc != 0)
    {
        return IpcResult::make_failed(errc);
    }

    if (::ConnectNamedPipe(hPipe, &aioCtx))
    {
        return IpcResult::make_ok();
    }

    errc = ::GetLastError();
    if (errc == ERROR_PIPE_CONNECTED)  // client alreay connected
    {
        return IpcResult::make_ok();
    }
    else if (errc == ERROR_IO_PENDING)
    {
        int dummy = 0;
        if (wait_aio_result(hPipe, aioCtx, milliseconds, dummy))
            return IpcResult::make_ok();
    }
    return IpcResult::make_failed(errc);
}

// connect to pipe server
static HANDLE pipe_connect(const char* pipeName, int milliseconds, IpcResult& rst)
{
    HANDLE hPipe = INVALID_HANDLE_VALUE;
    while (1)
    {
        hPipe = ::CreateFileA(
            pipeName,
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            FILE_FLAG_OVERLAPPED,
            NULL);
        if (hPipe != INVALID_HANDLE_VALUE) // succeeded
        {
            rst = IpcResult::make_ok();
            return hPipe;
        }

        int errc = ::GetLastError();
        if (errc != ERROR_PIPE_BUSY)
        {
            rst = IpcResult::make_failed(errc);
            break;
        }
        else
        {
            if (milliseconds == 0)
            {
                rst = IpcResult::make_timeout(ERROR_TIMEOUT);
                break;
            }
            DWORD timeout = milliseconds > 0 ? milliseconds : NMPWAIT_WAIT_FOREVER;
            ::WaitNamedPipeA(pipeName, timeout);  // wait server ready
            milliseconds = 0;                       // try once more
        }
    }

    return INVALID_HANDLE_VALUE;
}

// read data from pipe
static IpcResult pipe_read(HANDLE hPipe, WinAioCtx& aioCtx,
    void* buf, int size, int& gotten, int milliseconds)
{
    int errc = aioCtx.prepare();
    if (errc != 0)
        return IpcResult::make_failed(errc);

    DWORD rdSize = 0;
    if (::ReadFile(hPipe, buf, size, &rdSize, &aioCtx))
    {
        gotten = (int)rdSize;
        return IpcResult::make_ok();
    }

    errc = ::GetLastError();
    if (errc != ERROR_IO_PENDING)
        return IpcResult::make_failed(errc);

    return wait_aio_result(hPipe, aioCtx, milliseconds, gotten);
}

// write data to pipe
static IpcResult pipe_write(HANDLE hPipe, WinAioCtx& aioCtx,
    const void* data, int size, int& written, int milliseconds)
{
    int errc = aioCtx.prepare();
    if (errc != 0)
        return IpcResult::make_failed(errc);

    DWORD wtSize = 0;
    if (::WriteFile(hPipe, data, size, &wtSize, &aioCtx))
    {
        written = (int)wtSize;
        return IpcResult::make_ok();
    }

    errc = ::GetLastError();
    if (errc != ERROR_IO_PENDING)
        return IpcResult::make_failed(errc);

    return wait_aio_result(hPipe, aioCtx, milliseconds, written);
}

class ImpMqServer : IrkNocopy
{
public:
    ImpMqServer();
    ~ImpMqServer();

    // create new message queue
    IpcResult create(const char* mqName, int maxMsgSize, int maxMsgNum);

    // delete message queue
    void close();

    // wait client connected, wait forever if "milliseconds" < 0
    IpcResult wait_connected(int milliseconds);

    // disconnect current client
    void disconnect();

    // is message queue connected from a client
    bool connected() const { return m_bConnected; }

    // receive request message from a client, wait forever if "milliseconds" < 0
    // gotten = the real size of message received, undefined if failed
    IpcResult recv(void* buf, int bufSize, int& gotten, int milliseconds);

    // send ack message to the client, wait forever if "milliseconds" < 0
    IpcResult send(const void* ack, int ackSize, int milliseconds);

private:
    bool        m_bConnected;
    int         m_maxMsgSize;
    HANDLE      m_hPipe;
    WinAioCtx   m_connCtx;
    WinAioCtx   m_recvCtx;
    WinAioCtx   m_sendCtx;
};

ImpMqServer::ImpMqServer()
{
    m_bConnected = false;
    m_maxMsgSize = 0;
    m_hPipe = INVALID_HANDLE_VALUE;
}

ImpMqServer::~ImpMqServer()
{
    if (m_hPipe != INVALID_HANDLE_VALUE)
    {
        ::CancelIoEx(m_hPipe, NULL);
        ::DisconnectNamedPipe(m_hPipe);
        ::CloseHandle(m_hPipe);
    }
}

IpcResult ImpMqServer::create(const char* mqName, int maxMsgSize, int maxMsgNum)
{
    assert(m_hPipe == INVALID_HANDLE_VALUE);
    assert(maxMsgSize > 0 && maxMsgNum > 0);
    m_maxMsgSize = (maxMsgSize + 63) & ~63;

    // create pipe
    m_hPipe = ::CreateNamedPipeA(
        mqName,
        PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        1,
        m_maxMsgSize * maxMsgNum,
        m_maxMsgSize * maxMsgNum,
        3000,   // default timeout
        NULL);
    if (m_hPipe == INVALID_HANDLE_VALUE)
        return IpcResult::make_failed(::GetLastError());

    return IpcResult::make_ok();
}

void ImpMqServer::close()
{
    if (m_hPipe != INVALID_HANDLE_VALUE)
    {
        ::FlushFileBuffers(m_hPipe);

        if (m_bConnected)
        {
            ::DisconnectNamedPipe(m_hPipe);
            m_bConnected = false;
        }
        ::CloseHandle(m_hPipe);
        m_hPipe = INVALID_HANDLE_VALUE;
    }
}

// wait client connected, wait forever if "milliseconds" < 0
IpcResult ImpMqServer::wait_connected(int milliseconds)
{
    if (m_hPipe == INVALID_HANDLE_VALUE)   // pipe not created
    {
        return IpcResult::make_failed(ERROR_INVALID_HANDLE);
    }
    if (m_bConnected)  // someone already connected
    {
        return IpcResult::make_failed(ERROR_PIPE_CONNECTED);
    }

    IpcResult rst = pipe_accept(m_hPipe, m_connCtx, milliseconds);
    if (!rst)
        return rst;

    m_bConnected = true;
    return IpcResult::make_ok();
}

// disconnect current client
void ImpMqServer::disconnect()
{
    if (m_bConnected)
    {
        ::FlushFileBuffers(m_hPipe);
        ::DisconnectNamedPipe(m_hPipe);
        m_bConnected = false;
    }
}

IpcResult ImpMqServer::recv(void* buf, int bufSize, int& gotten, int milliseconds)
{
    if (!m_bConnected)              // pipe not connected
        return IpcResult::make_failed(ERROR_PIPE_NOT_CONNECTED);

    return pipe_read(m_hPipe, m_recvCtx, buf, bufSize, gotten, milliseconds);
}

IpcResult ImpMqServer::send(const void* ack, int ackSize, int milliseconds)
{
    if (!m_bConnected)              // pipe not connected
        return IpcResult::make_failed(ERROR_PIPE_NOT_CONNECTED);

    if (ackSize > m_maxMsgSize)     // invalid message size
        return IpcResult::make_failed(ERROR_INVALID_PARAMETER);

    int sent = 0;
    return pipe_write(m_hPipe, m_sendCtx, ack, ackSize, sent, milliseconds);
}

class ImpMqClient : IrkNocopy
{
public:
    ImpMqClient() : m_hPipe(INVALID_HANDLE_VALUE) {}
    ~ImpMqClient()
    {
        if (m_hPipe != INVALID_HANDLE_VALUE)
        {
            ::CancelIoEx(m_hPipe, NULL);
            ::CloseHandle(m_hPipe);
        }
    }

    // connect to message queue server, wait forever if "milliseconds" < 0
    IpcResult connect(const char* mqName, int milliseconds);

    // disconnect
    void disconnect();

    // is connection to the server succeeded
    bool connected() const { return m_hPipe != INVALID_HANDLE_VALUE; }

    // send request message to the server, wait forever if "milliseconds" < 0
    IpcResult send(const void* req, int reqSize, int milliseconds);

    // receive ack message from the server, wait forever if "milliseconds" < 0
    // gotten = the real size of ack received, undefined if failed
    IpcResult recv(void* buf, int bufSize, int& gotten, int milliseconds);

    // send request and wait ack, wait forever if "milliseconds" < 0
    // gotten = the real size of ack received, undefined if failed
    IpcResult transact(const void* req, int reqSize, void* ackBuf, int bufSize,
        int& gotten, int milliseconds);
private:
    HANDLE      m_hPipe;
    WinAioCtx   m_recvCtx;
    WinAioCtx   m_sendCtx;
    WinAioCtx   m_tranCtx;
};

// connect to message queue server, wait forever if "milliseconds" < 0
IpcResult ImpMqClient::connect(const char* mqName, int milliseconds)
{
    assert(m_hPipe == INVALID_HANDLE_VALUE);

    // connect to pipe server
    IpcResult rst;
    HANDLE hPipe = pipe_connect(mqName, milliseconds, rst);
    if (hPipe == INVALID_HANDLE_VALUE)
        return rst;

    // set to message mode
    DWORD pipeMode = PIPE_READMODE_MESSAGE | PIPE_WAIT;
    if (!::SetNamedPipeHandleState(hPipe, &pipeMode, NULL, NULL))
    {
        int errc = ::GetLastError();
        ::CloseHandle(hPipe);
        return IpcResult::make_failed(errc);
    }

    // all done
    m_hPipe = hPipe;
    return IpcResult::make_ok();
}

// disconnect
void ImpMqClient::disconnect()
{
    if (m_hPipe != INVALID_HANDLE_VALUE)
    {
        ::FlushFileBuffers(m_hPipe);
        ::CloseHandle(m_hPipe);
        m_hPipe = INVALID_HANDLE_VALUE;
    }
}

// send request message to the server, wait forever if "milliseconds" < 0
IpcResult ImpMqClient::send(const void* req, int reqSize, int milliseconds)
{
    if (m_hPipe == INVALID_HANDLE_VALUE)   // pipe not connected
        return IpcResult::make_failed(ERROR_PIPE_NOT_CONNECTED);

    int sent = 0;
    return pipe_write(m_hPipe, m_sendCtx, req, reqSize, sent, milliseconds);
}

// receive ack message from the server, wait forever if "milliseconds" < 0
IpcResult ImpMqClient::recv(void* buf, int bufSize, int& gotten, int milliseconds)
{
    if (m_hPipe == INVALID_HANDLE_VALUE)   // pipe not connected
        return IpcResult::make_failed(ERROR_PIPE_NOT_CONNECTED);

    return pipe_read(m_hPipe, m_recvCtx, buf, bufSize, gotten, milliseconds);
}

// send request and wait ack, wait forever if "milliseconds" < 0
IpcResult ImpMqClient::transact(const void* req, int reqSize, void* ackBuf, int ackBufSize,
    int& gotten, int milliseconds)
{
    if (m_hPipe == INVALID_HANDLE_VALUE)   // pipe not connected
        return IpcResult::make_failed(ERROR_PIPE_NOT_CONNECTED);

    int errc = m_tranCtx.prepare();
    if (errc != 0)
        return IpcResult::make_failed(errc);

    DWORD ackRet = 0;
    if (::TransactNamedPipe(m_hPipe, (void*)req, reqSize, ackBuf, ackBufSize, &ackRet, &m_tranCtx))
    {
        gotten = (int)ackRet;
        return IpcResult::make_ok();
    }

    errc = ::GetLastError();
    if (errc != ERROR_IO_PENDING)
        return IpcResult::make_failed(errc);

    return wait_aio_result(m_hPipe, m_tranCtx, milliseconds, gotten);
}

//======================================================================================================================
class ImpPipeServer : IrkNocopy
{
public:
    ImpPipeServer();
    ~ImpPipeServer();

    // create new pipe
    IpcResult create(const char* pipeName, int txBufSize);

    // delete pipe
    void close();

    // wait client connected, wait forever if "milliseconds" < 0
    IpcResult wait_connected(int milliseconds);

    // disconnect current client
    void disconnect();

    // is pipe connected from a client
    bool connected() const { return m_bConnected; }

    // receive request from a client, wait forever if "milliseconds" < 0
    // gotten = the size of data received, undefined if failed
    IpcResult recv(void* buf, int bufSize, int& gotten, int milliseconds);

    // send ack to the client, wait forever if "milliseconds" < 0
    IpcResult send(const void* ack, int ackSize, int milliseconds);

private:
    bool        m_bConnected;
    HANDLE      m_hPipe;
    WinAioCtx   m_connCtx;
    WinAioCtx   m_recvCtx;
    WinAioCtx   m_sendCtx;
};

ImpPipeServer::ImpPipeServer()
{
    m_bConnected = false;
    m_hPipe = INVALID_HANDLE_VALUE;
}

ImpPipeServer::~ImpPipeServer()
{
    if (m_hPipe != INVALID_HANDLE_VALUE)
    {
        ::CancelIoEx(m_hPipe, NULL);
        ::DisconnectNamedPipe(m_hPipe);
        ::CloseHandle(m_hPipe);
    }
}

IpcResult ImpPipeServer::create(const char* pipeName, int txBufSize)
{
    assert(m_hPipe == INVALID_HANDLE_VALUE && txBufSize);

    // create pipe
    m_hPipe = ::CreateNamedPipeA(
        pipeName,
        PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        1,
        txBufSize,
        txBufSize,
        3000,   // default timeout
        NULL);
    if (m_hPipe == INVALID_HANDLE_VALUE)
        return IpcResult::make_failed(::GetLastError());

    return IpcResult::make_ok();
}

void ImpPipeServer::close()
{
    if (m_hPipe != INVALID_HANDLE_VALUE)
    {
        ::FlushFileBuffers(m_hPipe);

        if (m_bConnected)
        {
            ::DisconnectNamedPipe(m_hPipe);
            m_bConnected = false;
        }
        ::CloseHandle(m_hPipe);
        m_hPipe = INVALID_HANDLE_VALUE;
    }
}

IpcResult ImpPipeServer::wait_connected(int milliseconds)
{
    if (m_hPipe == INVALID_HANDLE_VALUE)    // pipe not created
    {
        return IpcResult::make_failed(ERROR_INVALID_HANDLE);
    }
    if (m_bConnected)   // someone already connected
    {
        return IpcResult::make_failed(ERROR_PIPE_CONNECTED);
    }

    IpcResult rst = pipe_accept(m_hPipe, m_connCtx, milliseconds);
    if (!rst)
        return rst;

    m_bConnected = true;
    return IpcResult::make_ok();
}

void ImpPipeServer::disconnect()
{
    if (m_bConnected)
    {
        ::FlushFileBuffers(m_hPipe);
        ::DisconnectNamedPipe(m_hPipe);
        m_bConnected = false;
    }
}

IpcResult ImpPipeServer::recv(void* buf, int bufSize, int& gotten, int milliseconds)
{
    if (!m_bConnected)  // pipe not connected
        return IpcResult::make_failed(ERROR_PIPE_NOT_CONNECTED);

    return pipe_read(m_hPipe, m_recvCtx, buf, bufSize, gotten, milliseconds);
}

IpcResult ImpPipeServer::send(const void* ack, int ackSize, int milliseconds)
{
    if (!m_bConnected)     // pipe not connected
        return IpcResult::make_failed(ERROR_PIPE_NOT_CONNECTED);

    int sent = 0;
    return pipe_write(m_hPipe, m_sendCtx, ack, ackSize, sent, milliseconds);
}

class ImpPipeClient : IrkNocopy
{
public:
    ImpPipeClient() : m_hPipe(INVALID_HANDLE_VALUE) {}
    ~ImpPipeClient()
    {
        if (m_hPipe != INVALID_HANDLE_VALUE)
        {
            ::CancelIoEx(m_hPipe, NULL);
            ::CloseHandle(m_hPipe);
        }
    }

    // connect to pipe server, wait forever if "milliseconds" < 0
    IpcResult connect(const char* pipeName, int milliseconds);

    // disconnect
    void disconnect();

    // is connection to the server succeeded
    bool connected() const { return m_hPipe != INVALID_HANDLE_VALUE; }

    // send request to the server, wait forever if "milliseconds" < 0
    IpcResult send(const void* req, int reqSize, int milliseconds);

    // receive ack from the server, wait forever if "milliseconds" < 0
    // gotten = the real size of data received, undefined if failed
    IpcResult recv(void* buf, int bufSize, int& gotten, int milliseconds);

private:
    HANDLE      m_hPipe;
    WinAioCtx   m_recvCtx;
    WinAioCtx   m_sendCtx;
};

IpcResult ImpPipeClient::connect(const char* pipeName, int milliseconds)
{
    assert(m_hPipe == INVALID_HANDLE_VALUE);

    // connect to pipe server
    IpcResult rst;
    HANDLE hPipe = pipe_connect(pipeName, milliseconds, rst);
    if (hPipe == INVALID_HANDLE_VALUE)
        return rst;

    // set to byte stream mode
    DWORD pipeMode = PIPE_READMODE_BYTE | PIPE_WAIT;
    if (!::SetNamedPipeHandleState(hPipe, &pipeMode, NULL, NULL))
    {
        int errc = ::GetLastError();
        ::CloseHandle(hPipe);
        return IpcResult::make_failed(errc);
    }

    // all done
    m_hPipe = hPipe;
    return IpcResult::make_ok();
}

void ImpPipeClient::disconnect()
{
    if (m_hPipe != INVALID_HANDLE_VALUE)
    {
        ::FlushFileBuffers(m_hPipe);
        ::CloseHandle(m_hPipe);
        m_hPipe = INVALID_HANDLE_VALUE;
    }
}

IpcResult ImpPipeClient::send(const void* req, int reqSize, int milliseconds)
{
    if (m_hPipe == INVALID_HANDLE_VALUE)    // pipe not connected
        return IpcResult::make_failed(ERROR_PIPE_NOT_CONNECTED);

    int sent = 0;
    return pipe_write(m_hPipe, m_sendCtx, req, reqSize, sent, milliseconds);
}

IpcResult ImpPipeClient::recv(void* buf, int bufSize, int& gotten, int milliseconds)
{
    if (m_hPipe == INVALID_HANDLE_VALUE)   // pipe not connected
        return IpcResult::make_failed(ERROR_PIPE_NOT_CONNECTED);

    return pipe_read(m_hPipe, m_recvCtx, buf, bufSize, gotten, milliseconds);
}

#else   // POSIX

// Mac OSX does NOT have POSIX clock_gettime
#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>

#define CLOCK_REALTIME  0x01
#define CLOCK_MONOTONIC 0x02

static int clock_gettime(int clockId, struct timespec* pts)
{
    (void)clockId;
    clock_serv_t clksrv;
    mach_timespec_t mts;
    host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &clksrv);
    clock_get_time(clksrv, &mts);
    mach_port_deallocate(mach_task_self(), clksrv);
    pts->tv_sec = mts.tv_sec;
    pts->tv_nsec = mts.tv_nsec;
    return 0;
}
#endif

// return 0 if succeeded, return system error code if failed
static int get_abs_timeout(struct timespec& tmsp, int milliseconds)
{
    if (clock_gettime(CLOCK_REALTIME, &tmsp) != 0)
    {
        return errno;
    }
    int secs = milliseconds / 1000;
    int nsecs = 1e6 * (milliseconds - secs * 1000);
    if (nsecs < 1e9 - tmsp.tv_nsec)
    {
        tmsp.tv_sec += secs;
        tmsp.tv_nsec += nsecs;
    }
    else
    {
        tmsp.tv_sec += secs + 1;
        tmsp.tv_nsec = tmsp.tv_nsec + nsecs - 1e9;
    }
    return 0;
}

class PosixSema
{
public:
    PosixSema() : m_pSema(SEM_FAILED) {}
    ~PosixSema()
    {
        this->close();
    }

    // create new semaphore, semaName must begin with '/'
    IpcResult create(const char* semaName, int initVal)
    {
        assert(m_pSema == SEM_FAILED);

        m_pSema = ::sem_open(semaName, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, initVal);
        if (m_pSema == SEM_FAILED)
            return IpcResult::make_failed(errno);

        m_Name = semaName;
        return IpcResult::make_ok();
    }

    // open an existing semaphore
    IpcResult open(const char* semaName)
    {
        assert(m_pSema == SEM_FAILED);

        m_pSema = ::sem_open(semaName, 0);
        if (m_pSema == SEM_FAILED)
            return IpcResult::make_failed(errno);

        return IpcResult::make_ok();
    }

    // close semaphore and unlink created new semaphore
    void close()
    {
        if (m_pSema != SEM_FAILED)
        {
            ::sem_close(m_pSema);
            m_pSema = SEM_FAILED;
        }
        if (!m_Name.empty())    // created semaphore
        {
            ::sem_unlink(m_Name.c_str());
            m_Name.clear();
        }
    }

    // post semaphore
    IpcResult post()
    {
        if (::sem_post(m_pSema) == 0)
            return IpcResult::make_ok();

        return IpcResult::make_failed(errno);
    }

    // wait semaphore posted, wait forever
    IpcResult wait()
    {
        if (::sem_wait(m_pSema) == 0)
            return IpcResult::make_ok();

        return IpcResult::make_failed(errno);
    }

    // wait semaphore posted at most milliseconds
    IpcResult wait_for(int milliseconds);

    // try wait semaphore, never block
    bool try_wait()
    {
        return ::sem_trywait(m_pSema) == 0;
    }
private:
    sem_t * m_pSema;
    std::string m_Name;     // created semaphore name
};

IpcResult PosixSema::wait_for(int milliseconds)
{
    if (m_pSema == SEM_FAILED)
    {
        return IpcResult::make_failed(EINVAL);
    }

#ifdef __MACH__
    // MacOSX does not support sem_timedwait
    ::sched_yield();

    while (1)
    {
        if (::sem_trywait(m_pSema) == 0)
        {
            return IpcResult::make_ok();
        }

        if (errno == EAGAIN)       // still locked
        {
            if (milliseconds > 0)  // sleep 1ms
            {
                milliseconds -= 1;
                ::usleep(1000);
            }
            else
            {
                return IpcResult::make_timeout(ETIMEDOUT);
            }
        }
        else    // other error
        {
            return IpcResult::make_failed(errno);
        }
    }
#else
    // make time spec
    struct timespec tmsp;
    int errc = get_abs_timeout(tmsp, milliseconds);
    if (errc)
    {
        return IpcResult::make_failed(errc);
    }

    // wait until the time spec
    if (::sem_timedwait(m_pSema, &tmsp) == 0)
    {
        return IpcResult::make_ok();
    }
    else if (errno == ETIMEDOUT)
    {
        return IpcResult::make_timeout(ETIMEDOUT);
    }
    else
    {
        return IpcResult::make_failed(errno);
    }
#endif
}

Semaphore::Semaphore() : m_Handle(NULL)
{
    m_Handle = new PosixSema;
}

Semaphore::~Semaphore()
{
    delete (PosixSema*)m_Handle;
}

// create new semaphore, the name of semaphore must begin with '/'
IpcResult Semaphore::create(const char* semaName, int initVal)
{
    PosixSema* pImpl = (PosixSema*)m_Handle;
    return pImpl->create(semaName, initVal);
}

// open an existing semaphore
IpcResult Semaphore::open(const char* semaName)
{
    PosixSema* pImpl = (PosixSema*)m_Handle;
    return pImpl->open(semaName);
}

// close semaphore, also unlink created new semaphore
void Semaphore::close()
{
    PosixSema* pImpl = (PosixSema*)m_Handle;
    pImpl->close();
}

// post semaphore
IpcResult Semaphore::post()
{
    PosixSema* pImpl = (PosixSema*)m_Handle;
    return pImpl->post();
}

// wait semaphore posted
IpcResult Semaphore::wait(int milliseconds)
{
    PosixSema* pImpl = (PosixSema*)m_Handle;
    if (milliseconds < 0)      // wait forever
        return pImpl->wait();
    else
        return pImpl->wait_for(milliseconds);
}

// try wait semaphore, doen not block, return true if semaphore has been posted
bool Semaphore::try_wait()
{
    PosixSema* pImpl = (PosixSema*)m_Handle;
    return pImpl->try_wait();
}

//======================================================================================================================
class PosixShmObj : IrkNocopy
{
public:
    PosixShmObj() : m_ShmObj(-1) {}
    ~PosixShmObj()
    {
        this->close();
    }

    IpcResult create(const char* shmName, size_t size)
    {
        assert(m_ShmObj == -1 && m_ShmObjName.empty());

        // create shared memory object
        int shmObj = ::shm_open(shmName, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
        if (shmObj < 0)
        {
            return IpcResult::make_failed(errno);
        }

        // set shared memory size
        if (::ftruncate(shmObj, size) != 0)
        {
            int errc = errno;
            ::close(shmObj);
            ::shm_unlink(shmName);
            return IpcResult::make_failed(errc);
        }

        m_ShmObj = shmObj;
        m_ShmObjName = shmName;
        return IpcResult::make_ok();
    }

    IpcResult open(const char* shmName)
    {
        assert(m_ShmObj == -1 && m_ShmObjName.empty());

        int shmObj = ::shm_open(shmName, O_RDWR, S_IRUSR | S_IWUSR);
        if (shmObj < 0)
        {
            return IpcResult::make_failed(errno);
        }
        m_ShmObj = shmObj;
        return IpcResult::make_ok();
    }

    void close()
    {
        if (m_ShmObj >= 0)
        {
            ::close(m_ShmObj);
            m_ShmObj = -1;
        }
        if (!m_ShmObjName.empty())
        {
            ::shm_unlink(m_ShmObjName.c_str());
            m_ShmObjName.clear();
        }
    }

    IpcResult map(size_t offset, size_t size, void** outPtr)
    {
        if (m_ShmObj < 0)
        {
            *outPtr = nullptr;
            return IpcResult::make_failed(EINVAL);
        }

        void* pbuf = ::mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, m_ShmObj, (off_t)offset);
        if (pbuf != MAP_FAILED)
        {
            *outPtr = pbuf;
            return IpcResult::make_ok();
        }
        *outPtr = nullptr;
        return IpcResult::make_failed(errno);
    }

    IpcResult unmap(void* buf, size_t size)
    {
        assert(m_ShmObj >= 0 && buf != nullptr);
        if (::munmap(buf, size) == 0)
        {
            return IpcResult::make_ok();
        }
        return IpcResult::make_failed(errno);
    }
private:
    int         m_ShmObj;       // shared memory object
    std::string m_ShmObjName;   // name of created shared memory object
};

IpcShmObj::IpcShmObj() : m_handle(nullptr)
{
    m_handle = new PosixShmObj;
}

IpcShmObj::~IpcShmObj()
{
    delete (PosixShmObj*)m_handle;
}

// create shared memory object
IpcResult IpcShmObj::create(const char* shmName, size_t size)
{
    return ((PosixShmObj*)m_handle)->create(shmName, size);
}

// open existing shared memory object
IpcResult IpcShmObj::open(const char* shmName)
{
    return ((PosixShmObj*)m_handle)->open(shmName);
}

// close/unlink shared memory object
void IpcShmObj::close()
{
    ((PosixShmObj*)m_handle)->close();
}

IpcResult IpcShmObj::map(size_t offset, size_t size, void** outPtr)
{
    return ((PosixShmObj*)m_handle)->map(offset, size, outPtr);
}

IpcResult IpcShmObj::unmap(void* buf, size_t size)
{
    return ((PosixShmObj*)m_handle)->unmap(buf, size);
}

//======================================================================================================================
static IpcResult poll_wait_event(int fdes, int evflag, int milliseconds)
{
    struct pollfd pfd[1];
    pfd[0].fd = fdes;
    pfd[0].events = evflag;
    pfd[0].revents = 0;

    int res = ::poll(pfd, 1, milliseconds);
    if (res == 0)
        return IpcResult::make_timeout(ETIMEDOUT);
    else if (res < 1)
        return IpcResult::make_failed(errno);

    if (!(pfd[0].revents & evflag))
        return IpcResult::make_failed(ECONNRESET);

    return IpcResult::make_ok();
}

#ifdef __linux__

static IpcResult mq_checked_send(mqd_t mqd, const void* buf, int size, int milliseconds)
{
    if (mqd == (mqd_t)-1)
    {
        return IpcResult::make_failed(EBADF);
    }

    if (milliseconds < 0)
    {
        if (::mq_send(mqd, (const char*)buf, size, 0) != 0)
            return IpcResult::make_failed(errno);
    }
    else
    {
        struct timespec tmsp;
        int errc = get_abs_timeout(tmsp, milliseconds);
        if (errc)
            return IpcResult::make_failed(errc);

        if (::mq_timedsend(mqd, (const char*)buf, size, 0, &tmsp) != 0)
        {
            if (errno == ETIMEDOUT)
                return IpcResult::make_timeout(ETIMEDOUT);
            return IpcResult::make_failed(errno);
        }
    }

    return IpcResult::make_ok();
}

static IpcResult mq_checked_recv(mqd_t mqd, void* buf, int size, int& result, int milliseconds)
{
    if (mqd == (mqd_t)-1)
    {
        return IpcResult::make_failed(EBADF);
    }

    if (milliseconds < 0)
    {
        result = (int)::mq_receive(mqd, (char*)buf, size, NULL);
        if (result < 0)
            return IpcResult::make_failed(errno);
    }
    else
    {
        struct timespec tmsp;
        int errc = get_abs_timeout(tmsp, milliseconds);
        if (errc)
            return IpcResult::make_failed(errc);

        result = ::mq_timedreceive(mqd, (char*)buf, size, NULL, &tmsp);
        if (result < 0)
        {
            if (errno == ETIMEDOUT)
                return IpcResult::make_timeout(ETIMEDOUT);
            return IpcResult::make_failed(errno);
        }
    }

    return IpcResult::make_ok();
}

class ImpMqServer : IrkNocopy
{
public:
    ImpMqServer();
    ~ImpMqServer();

    // create new message queue
    IpcResult create(const char* mqName, int maxMsgSize, int maxMsgNum);

    // delete message queue
    void close();

    // wait client connected, wait forever if "milliseconds" < 0
    IpcResult wait_connected(int milliseconds);

    // disconnect current client
    void disconnect() {}

    // is message queue connected from a client
    bool connected() const { return m_mqdReq != (mqd_t)-1 && m_mqdAck != (mqd_t)-1; }

    // receive request message from a client, wait forever if "milliseconds" < 0
    // gotten = the real size of message received, undefined if failed
    IpcResult recv(void* buf, int bufSize, int& gotten, int milliseconds);

    // send ack message to the client, wait forever if "milliseconds" < 0
    IpcResult send(const void* ack, int ackSize, int milliseconds);

private:
    mqd_t       m_mqdReq;
    mqd_t       m_mqdAck;
    int         m_maxMsgSize;
    std::string m_mqName;
};

ImpMqServer::ImpMqServer()
{
    m_mqdReq = (mqd_t)-1;
    m_mqdAck = (mqd_t)-1;
    m_maxMsgSize = 0;
}
ImpMqServer::~ImpMqServer()
{
    this->close();
}

IpcResult ImpMqServer::create(const char* mqName, int maxMsgSize, int maxMsgNum)
{
    assert(m_mqdReq == (mqd_t)-1 && m_mqdAck == (mqd_t)-1);
    assert(maxMsgSize > 0 && maxMsgNum > 0);

    m_maxMsgSize = maxMsgSize;
    m_mqName = mqName;
    std::string reqName = m_mqName + "REQ";
    std::string ackName = m_mqName + "ACK";

    mq_attr attr = {};
    attr.mq_msgsize = maxMsgSize;
    attr.mq_maxmsg = maxMsgNum;

    m_mqdReq = ::mq_open(reqName.c_str(), O_RDWR | O_CREAT | O_EXCL, 0644, &attr);
    if (m_mqdReq == (mqd_t)-1)
    {
        return IpcResult::make_failed(errno);
    }

    m_mqdAck = ::mq_open(ackName.c_str(), O_RDWR | O_CREAT | O_EXCL, 0644, &attr);
    if (m_mqdAck == (mqd_t)-1)
    {
        int errc = errno;
        ::mq_close(m_mqdReq);
        m_mqdReq = (mqd_t)-1;
        ::mq_unlink(reqName.c_str());
        return IpcResult::make_failed(errc);
    }

    return IpcResult::make_ok();
}

void ImpMqServer::close()
{
    if (m_mqdReq != (mqd_t)-1)
    {
        ::mq_close(m_mqdReq);
        m_mqdReq = (mqd_t)-1;
        std::string reqName = m_mqName + "REQ";
        ::mq_unlink(reqName.c_str());
    }
    if (m_mqdAck != (mqd_t)-1)
    {
        ::mq_close(m_mqdAck);
        m_mqdAck = (mqd_t)-1;
        std::string ackName = m_mqName + "ACK";
        ::mq_unlink(ackName.c_str());
    }
}

IpcResult ImpMqServer::wait_connected(int /*milliseconds*/)
{
    if (m_mqdReq != (mqd_t)-1 && m_mqdAck != (mqd_t)-1)
        return IpcResult::make_ok();
    return IpcResult::make_failed(EBADF);
}

IpcResult ImpMqServer::recv(void* buf, int bufSize, int& gotten, int milliseconds)
{
    return mq_checked_recv(m_mqdReq, buf, bufSize, gotten, milliseconds);
}

IpcResult ImpMqServer::send(const void* ack, int ackSize, int milliseconds)
{
    return mq_checked_send(m_mqdAck, ack, ackSize, milliseconds);
}

class ImpMqClient : IrkNocopy
{
public:
    ImpMqClient();
    ~ImpMqClient();

    // connect to message queue server, wait forever if "milliseconds" < 0
    IpcResult connect(const char* mqName, int milliseconds);

    // disconnect
    void disconnect();

    // is connection to the server succeeded
    bool connected() const { return m_mqdReq != (mqd_t)-1 && m_mqdAck != (mqd_t)-1; }

    // send request message to the server, wait forever if "milliseconds" < 0
    IpcResult send(const void* req, int reqSize, int milliseconds);

    // receive ack message from the server, wait forever if "milliseconds" < 0
    // gotten = the real size of ack received, undefined if failed
    IpcResult recv(void* buf, int bufSize, int& gotten, int milliseconds);

    // send request and wait ack, wait forever if "milliseconds" < 0
    // gotten = the real size of ack received, undefined if failed
    IpcResult transact(const void* req, int reqSize, void* ackBuf, int bufSize,
        int& gotten, int milliseconds);
private:
    mqd_t   m_mqdReq;
    mqd_t   m_mqdAck;
};

ImpMqClient::ImpMqClient()
{
    m_mqdReq = (mqd_t)-1;
    m_mqdAck = (mqd_t)-1;
}
ImpMqClient::~ImpMqClient()
{
    this->disconnect();
}

IpcResult ImpMqClient::connect(const char* mqName, int milliseconds)
{
    assert(m_mqdReq == (mqd_t)-1 && m_mqdAck == (mqd_t)-1);

    std::string strMqName = mqName;
    std::string reqName = strMqName + "REQ";
    std::string ackName = strMqName + "ACK";
    int errc = 0;
    while (1)
    {
        m_mqdReq = ::mq_open(reqName.c_str(), O_RDWR);
        if (m_mqdReq != (mqd_t)-1)   // succeeded
        {
            m_mqdAck = ::mq_open(ackName.c_str(), O_RDWR);
            if (m_mqdAck != (mqd_t)-1)
                break;
            errc = errno;
            ::mq_close(m_mqdReq);
            m_mqdReq = (mqd_t)-1;
        }
        else
        {
            errc = errno;
        }

        if (errc != ENOENT)
            return IpcResult::make_failed(errc);

        if (milliseconds == 0)
            return IpcResult::make_timeout(ETIMEDOUT);

        int timeout = 15;
        if (milliseconds > 0)
        {
            if (timeout > milliseconds)
                timeout = milliseconds;
            milliseconds -= timeout;
        }
        ::usleep(timeout * 1000);
    }

    return IpcResult::make_ok();
}

void ImpMqClient::disconnect()
{
    if (m_mqdReq != (mqd_t)-1)
    {
        ::mq_close(m_mqdReq);
        m_mqdReq = (mqd_t)-1;
    }
    if (m_mqdAck != (mqd_t)-1)
    {
        ::mq_close(m_mqdAck);
        m_mqdAck = (mqd_t)-1;
    }
}

IpcResult ImpMqClient::send(const void* req, int reqSize, int milliseconds)
{
    return mq_checked_send(m_mqdReq, req, reqSize, milliseconds);
}

IpcResult ImpMqClient::recv(void* buf, int bufSize, int& gotten, int milliseconds)
{
    return mq_checked_recv(m_mqdAck, buf, bufSize, gotten, milliseconds);
}

IpcResult ImpMqClient::transact(const void* req, int reqSize, void* ackBuf, int bufSize,
    int& gotten, int milliseconds)
{
    if (m_mqdReq == (mqd_t)-1 || m_mqdAck == (mqd_t)-1)
    {
        return IpcResult::make_failed(EBADF);
    }

    if (milliseconds < 0)
    {
        if (::mq_send(m_mqdReq, (const char*)req, reqSize, 0) != 0)
            return IpcResult::make_failed(errno);

        gotten = ::mq_receive(m_mqdAck, (char*)ackBuf, bufSize, NULL);
        if (gotten < 0)
            return IpcResult::make_failed(errno);
    }
    else
    {
        struct timespec tmsp;
        int errc = get_abs_timeout(tmsp, milliseconds);
        if (errc)
            return IpcResult::make_failed(errc);

        if (::mq_timedsend(m_mqdReq, (const char*)req, reqSize, 0, &tmsp) != 0)
        {
            if (errno == ETIMEDOUT)
                return IpcResult::make_timeout(ETIMEDOUT);
            return IpcResult::make_failed(errno);
        }

        gotten = ::mq_timedreceive(m_mqdAck, (char*)ackBuf, bufSize, NULL, &tmsp);
        if (gotten < 0)
        {
            if (errno == ETIMEDOUT)
                return IpcResult::make_timeout(ETIMEDOUT);
            return IpcResult::make_failed(errno);
        }
    }

    return IpcResult::make_ok();
}

#else  // __linux__

class ImpMqServer : IrkNocopy
{
public:
    ImpMqServer() : m_srvSkt(-1), m_cliAddr() {};
    ~ImpMqServer() { this->close(); }

    // create new unix domain socket
    IpcResult create(const char* path, int maxMsgSize, int maxMsgNum);

    // close unix domain socket
    void close();

    // wait client connected
    IpcResult wait_connected(int);

    // disconnect current client
    void disconnect();

    // is socket connected from a client
    bool connected() const { return m_srvSkt != -1; }

    // receive request from a client, wait forever if "milliseconds" < 0
    // gotten = the size of data received, undefined if failed
    IpcResult recv(void* buf, int bufSize, int& gotten, int milliseconds);

    // send ack to the client, wait forever if "milliseconds" < 0
    IpcResult send(const void* ack, int ackSize, int milliseconds);

private:
    int                 m_srvSkt;
    std::string         m_path;
    struct sockaddr_un  m_cliAddr;
};

IpcResult ImpMqServer::create(const char* path, int maxMsgSize, int maxMsgNum)
{
    if (m_srvSkt != -1)
    {
        return IpcResult::make_ok();
    }

    if (::strlen(path) >= sizeof(sockaddr_un::sun_path))
    {
        return IpcResult::make_failed(EINVAL);
    }
    if (::unlink(path) != 0 && errno != ENOENT)
    {
        return IpcResult::make_failed(errno);
    }

    // create server socket
    int skt = ::socket(AF_UNIX, SOCK_DGRAM, 0);
    if (skt == -1)
    {
        return IpcResult::make_failed(errno);
    }

    // bind to the path
    struct sockaddr_un addr;
    ::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    ::strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
    if (::bind(skt, (const sockaddr*)&addr, ssizeof(addr)) != 0)
    {
        int errc = errno;
        ::close(skt);
        return IpcResult::make_failed(errc);
    }

    // set recv and send buffer size
    int txBufSize = maxMsgSize * maxMsgNum;
    if (txBufSize > 0)
    {
        ::setsockopt(skt, SOL_SOCKET, SO_RCVBUF, (char*)&txBufSize, ssizeof(int));
        ::setsockopt(skt, SOL_SOCKET, SO_SNDBUF, (char*)&txBufSize, ssizeof(int));
    }

    m_srvSkt = skt;
    m_path = path;
    ::memset(&m_cliAddr, 0, sizeof(m_cliAddr));
    return IpcResult::make_ok();
}

inline void ImpMqServer::close()
{
    if (m_srvSkt != -1)
    {
        ::close(m_srvSkt);
        m_srvSkt = -1;
    }
    if (!m_path.empty())
    {
        ::unlink(m_path.c_str());
        m_path.clear();
    }
    ::memset(&m_cliAddr, 0, sizeof(m_cliAddr));
}

inline IpcResult ImpMqServer::wait_connected(int)
{
    if (m_srvSkt == -1)
        return IpcResult::make_failed(EBADF);
    return IpcResult::make_ok();
}

inline void ImpMqServer::disconnect()
{
    ::memset(&m_cliAddr, 0, sizeof(m_cliAddr));
}

IpcResult ImpMqServer::recv(void* buf, int bufSize, int& gotten, int milliseconds)
{
    if (m_srvSkt == -1)
    {
        return IpcResult::make_failed(EBADF);
    }

    if (milliseconds >= 0)
    {
        auto rst = poll_wait_event(m_srvSkt, POLLIN, milliseconds);
        if (!rst)
            return rst;
    }

    socklen_t addrLen = ssizeof(m_cliAddr);
    gotten = ::recvfrom(m_srvSkt, buf, bufSize, 0, (sockaddr*)&m_cliAddr, &addrLen);
    if (gotten == 0)
        return IpcResult::make_failed(ESHUTDOWN);
    else if (gotten < 0)
        return IpcResult::make_failed(errno);

    return IpcResult::make_ok();
}

IpcResult ImpMqServer::send(const void* ack, int ackSize, int milliseconds)
{
    if (m_srvSkt == -1)
    {
        return IpcResult::make_failed(EBADF);
    }

    if (milliseconds >= 0)
    {
        auto rst = poll_wait_event(m_srvSkt, POLLOUT, milliseconds);
        if (!rst)
            return rst;
    }

    int sent = ::sendto(m_srvSkt, ack, ackSize, 0, (const sockaddr*)&m_cliAddr, ssizeof(m_cliAddr));
    if (sent < 0)
        return IpcResult::make_failed(errno);
    return IpcResult::make_ok();
}

class ImpMqClient : IrkNocopy
{
public:
    ImpMqClient() : m_cliAddr(), m_srvAddr(), m_cliSkt(-1) {}
    ~ImpMqClient() { this->disconnect(); }

    // connect to server, wait forever if "milliseconds" < 0
    IpcResult connect(const char* path, int milliseconds);

    // disconnect
    void disconnect();

    // is connection to the server succeeded
    bool connected() const { return m_cliSkt != -1; }

    // send request to the server, wait forever if "milliseconds" < 0
    IpcResult send(const void* req, int reqSize, int milliseconds);

    // receive ack from the server, wait forever if "milliseconds" < 0
    // gotten = the real size of data received, undefined if failed
    IpcResult recv(void* buf, int bufSize, int& gotten, int milliseconds);

    // send request and wait ack, wait forever if "milliseconds" < 0
    // gotten = the real size of ack received, undefined if failed
    IpcResult transact(const void* req, int reqSize, void* ackBuf, int ackBufSize,
        int& gotten, int milliseconds);
private:
    struct sockaddr_un  m_cliAddr;
    struct sockaddr_un  m_srvAddr;
    int                 m_cliSkt;
};

IpcResult ImpMqClient::connect(const char* path, int /*milliseconds*/)
{
    assert(m_cliSkt == -1);
    ::memset(&m_cliAddr, 0, sizeof(m_cliAddr));
    ::memset(&m_srvAddr, 0, sizeof(m_srvAddr));

    if (::strlen(path) >= sizeof(sockaddr_un::sun_path))
    {
        return IpcResult::make_failed(EINVAL);
    }

    // create server socket
    int skt = ::socket(AF_UNIX, SOCK_DGRAM, 0);
    if (skt == -1)
    {
        return IpcResult::make_failed(errno);
    }

    // bind to a local temp path
    m_cliAddr.sun_family = AF_UNIX;
    ::tmpnam(m_cliAddr.sun_path);
    if (::bind(skt, (const sockaddr*)&m_cliAddr, ssizeof(m_cliAddr)) != 0)
    {
        int errc = errno;
        ::close(skt);
        return IpcResult::make_failed(errc);
    }

    // save server address
    m_srvAddr.sun_family = AF_UNIX;
    ::strncpy(m_srvAddr.sun_path, path, sizeof(m_srvAddr.sun_path) - 1);

    m_cliSkt = skt;
    return IpcResult::make_ok();
}

void ImpMqClient::disconnect()
{
    if (m_cliSkt != -1)
    {
        ::close(m_cliSkt);
        m_cliSkt = -1;
    }
    if (m_cliAddr.sun_family == AF_UNIX)
    {
        ::unlink(m_cliAddr.sun_path);
        ::memset(&m_cliAddr, 0, sizeof(m_cliAddr));
    }
    if (m_srvAddr.sun_family == AF_UNIX)
    {
        ::memset(&m_srvAddr, 0, sizeof(m_srvAddr));
    }
}

IpcResult ImpMqClient::send(const void* req, int reqSize, int milliseconds)
{
    if (m_cliSkt == -1)
    {
        return IpcResult::make_failed(EBADF);
    }

    if (milliseconds >= 0)
    {
        auto rst = poll_wait_event(m_cliSkt, POLLOUT, milliseconds);
        if (!rst)
            return rst;
    }

    int sent = ::sendto(m_cliSkt, req, reqSize, 0, (const sockaddr*)&m_srvAddr, ssizeof(m_srvAddr));
    if (sent < 0)
        return IpcResult::make_failed(errno);
    return IpcResult::make_ok();
}

IpcResult ImpMqClient::recv(void* buf, int bufSize, int& gotten, int milliseconds)
{
    if (m_cliSkt == -1)
    {
        return IpcResult::make_failed(EBADF);
    }

    if (milliseconds >= 0)
    {
        auto rst = poll_wait_event(m_cliSkt, POLLIN, milliseconds);
        if (!rst)
            return rst;
    }

    gotten = ::recvfrom(m_cliSkt, buf, bufSize, 0, NULL, NULL);
    if (gotten == 0)
        return IpcResult::make_failed(ESHUTDOWN);
    else if (gotten < 0)
        return IpcResult::make_failed(errno);
    return IpcResult::make_ok();
}

IpcResult ImpMqClient::transact(const void* req, int reqSize, void* ackBuf, int ackBufSize,
    int& gotten, int milliseconds)
{
    if (m_cliSkt == -1)
    {
        return IpcResult::make_failed(EBADF);
    }

    // send request
    if (milliseconds >= 0)
    {
        auto rst = poll_wait_event(m_cliSkt, POLLOUT, milliseconds);
        if (!rst)
            return rst;
    }
    int sent = ::sendto(m_cliSkt, req, reqSize, 0, (const sockaddr*)&m_srvAddr, ssizeof(m_srvAddr));
    if (sent < 0)
        return IpcResult::make_failed(errno);

    // receive ack
    if (milliseconds >= 0)
    {
        auto rst = poll_wait_event(m_cliSkt, POLLIN, milliseconds);
        if (!rst)
            return rst;
    }
    gotten = ::recvfrom(m_cliSkt, ackBuf, ackBufSize, 0, NULL, NULL);
    if (gotten == 0)
        return IpcResult::make_failed(ESHUTDOWN);
    else if (gotten < 0)
        return IpcResult::make_failed(errno);

    return IpcResult::make_ok();
}

#endif  // __linux__

//======================================================================================================================
class ImpPipeServer : IrkNocopy
{
public:
    ImpPipeServer() : m_srvSkt(-1), m_cliSkt(-1) {}
    ~ImpPipeServer() { this->close(); }

    // create new unix domain socket
    IpcResult create(const char* path, int txBufSize);

    // close unix domain socket
    void close();

    // wait client connected, wait forever if "milliseconds" < 0
    IpcResult wait_connected(int milliseconds);

    // disconnect current client
    void disconnect();

    // is socket connected from a client
    bool connected() const { return m_cliSkt != -1; }

    // receive request from a client, wait forever if "milliseconds" < 0
    // gotten = the size of data received, undefined if failed
    IpcResult recv(void* buf, int bufSize, int& gotten, int milliseconds);

    // send ack to the client, wait forever if "milliseconds" < 0
    IpcResult send(const void* ack, int ackSize, int milliseconds);

private:
    int         m_srvSkt;
    int         m_cliSkt;
    std::string m_path;
};

IpcResult ImpPipeServer::create(const char* path, int txBufSize)
{
    if (m_srvSkt != -1)
    {
        return IpcResult::make_ok();
    }

    if (::strlen(path) >= sizeof(sockaddr_un::sun_path))
    {
        return IpcResult::make_failed(EINVAL);
    }
    if (::unlink(path) != 0 && errno != ENOENT)
    {
        return IpcResult::make_failed(errno);
    }

    // create server socket
    int skt = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (skt == -1)
    {
        return IpcResult::make_failed(errno);
    }

    // bind to the path
    struct sockaddr_un addr;
    ::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    ::strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
    if (::bind(skt, (const sockaddr*)&addr, ssizeof(addr)) != 0)
    {
        int errc = errno;
        ::close(skt);
        return IpcResult::make_failed(errc);
    }

    // set recv and send buffer size
    if (txBufSize > 0)
    {
        ::setsockopt(skt, SOL_SOCKET, SO_RCVBUF, (char*)&txBufSize, ssizeof(int));
        ::setsockopt(skt, SOL_SOCKET, SO_SNDBUF, (char*)&txBufSize, ssizeof(int));
    }

    if (::listen(skt, 1) != 0)
    {
        int errc = errno;
        ::close(skt);
        return IpcResult::make_failed(errc);
    }

    m_srvSkt = skt;
    m_path = path;
    return IpcResult::make_ok();
}

void ImpPipeServer::close()
{
    if (m_cliSkt != -1)
    {
        ::shutdown(m_cliSkt, SHUT_RDWR);
        ::close(m_cliSkt);
        m_cliSkt = -1;
    }
    if (m_srvSkt != -1)
    {
        ::close(m_srvSkt);
        m_srvSkt = -1;
    }
    if (!m_path.empty())
    {
        ::unlink(m_path.c_str());
        m_path.clear();
    }
}

IpcResult ImpPipeServer::wait_connected(int milliseconds)
{
    if (m_cliSkt != -1)
    {
        return IpcResult::make_failed(EISCONN);
    }

    if (milliseconds >= 0)
    {
        auto rst = poll_wait_event(m_srvSkt, POLLIN, milliseconds);
        if (!rst)
            return rst;
    }

    m_cliSkt = ::accept(m_srvSkt, NULL, NULL);
    if (m_cliSkt == -1)
    {
        return IpcResult::make_failed(errno);
    }

    return IpcResult::make_ok();
}

void ImpPipeServer::disconnect()
{
    if (m_cliSkt != -1)
    {
        ::shutdown(m_cliSkt, SHUT_RDWR);
        ::close(m_cliSkt);
        m_cliSkt = -1;
    }
}

IpcResult ImpPipeServer::recv(void* buf, int bufSize, int& gotten, int milliseconds)
{
    if (m_cliSkt == -1)
    {
        return IpcResult::make_failed(ENOTCONN);
    }

    if (milliseconds >= 0)
    {
        auto rst = poll_wait_event(m_cliSkt, POLLIN, milliseconds);
        if (!rst)
            return rst;
    }

    gotten = ::recv(m_cliSkt, buf, bufSize, 0);
    if (gotten == 0)
        return IpcResult::make_failed(ESHUTDOWN);
    else if (gotten < 0)
        return IpcResult::make_failed(errno);
    return IpcResult::make_ok();
}

IpcResult ImpPipeServer::send(const void* ack, int ackSize, int milliseconds)
{
    if (m_cliSkt == -1)
    {
        return IpcResult::make_failed(ENOTCONN);
    }

    if (milliseconds >= 0)
    {
        auto rst = poll_wait_event(m_cliSkt, POLLOUT, milliseconds);
        if (!rst)
            return rst;
    }

    int sent = ::send(m_cliSkt, ack, ackSize, 0);
    if (sent < 0)
        return IpcResult::make_failed(errno);
    return IpcResult::make_ok();
}

class ImpPipeClient : IrkNocopy
{
public:
    ImpPipeClient() : m_connSkt(-1) {}
    ~ImpPipeClient() { this->disconnect(); }

    // connect to server, wait forever if "milliseconds" < 0
    IpcResult connect(const char* path, int milliseconds);

    // disconnect
    void disconnect();

    // is connection to the server succeeded
    bool connected() const { return m_connSkt != -1; }

    // send request to the server, wait forever if "milliseconds" < 0
    IpcResult send(const void* req, int reqSize, int milliseconds);

    // receive ack from the server, wait forever if "milliseconds" < 0
    // gotten = the real size of data received, undefined if failed
    IpcResult recv(void* buf, int bufSize, int& gotten, int milliseconds);

private:
    int  m_connSkt;
};

IpcResult ImpPipeClient::connect(const char* path, int /*milliseconds*/)
{
    assert(m_connSkt == -1);

    if (::strlen(path) >= sizeof(sockaddr_un::sun_path))
    {
        return IpcResult::make_failed(EINVAL);
    }

    // create server socket
    int skt = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (skt == -1)
    {
        return IpcResult::make_failed(errno);
    }

    // connect to the server
    struct sockaddr_un addr;
    ::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    ::strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
    if (::connect(skt, (const sockaddr*)&addr, ssizeof(addr)) != 0)
    {
        int errc = errno;
        ::close(skt);
        return IpcResult::make_failed(errc);
    }

    m_connSkt = skt;
    return IpcResult::make_ok();
}

void ImpPipeClient::disconnect()
{
    if (m_connSkt != -1)
    {
        ::shutdown(m_connSkt, SHUT_RDWR);
        ::close(m_connSkt);
        m_connSkt = -1;
    }
}

IpcResult ImpPipeClient::send(const void* req, int reqSize, int milliseconds)
{
    if (m_connSkt == -1)
    {
        return IpcResult::make_failed(ENOTCONN);
    }

    if (milliseconds >= 0)
    {
        auto rst = poll_wait_event(m_connSkt, POLLOUT, milliseconds);
        if (!rst)
            return rst;
    }

    int sent = ::send(m_connSkt, req, reqSize, 0);
    if (sent < 0)
        return IpcResult::make_failed(errno);
    return IpcResult::make_ok();
}

IpcResult ImpPipeClient::recv(void* buf, int bufSize, int& gotten, int milliseconds)
{
    if (m_connSkt == -1)
    {
        return IpcResult::make_failed(ENOTCONN);
    }

    if (milliseconds >= 0)
    {
        auto rst = poll_wait_event(m_connSkt, POLLIN, milliseconds);
        if (!rst)
            return rst;
    }

    gotten = ::recv(m_connSkt, buf, bufSize, 0);
    if (gotten == 0)
        return IpcResult::make_failed(ESHUTDOWN);
    else if (gotten < 0)
        return IpcResult::make_failed(errno);
    return IpcResult::make_ok();
}

#endif  // Posix

//======================================================================================================================
#ifndef _WIN32
#define ERROR_BROKEN_PIPE EPIPE
#endif

#define ALIGN_16(x)         static_cast<int>(((x) + 15) & ~15)
#define SHMFRT_HEADER_SIZE  64

const char* s_suffixShm = "SHM";
const char* s_suffixFreeCnt = "FRC";
const char* s_suffixFreeLock = "FRL";
const char* s_suffixReqCnt = "REQC";
const char* s_suffixReqLock = "REQL";
const char* s_suffixAckCnt = "ACKC";
const char* s_suffixAckLock = "ACKL";

class WorkerTracer : IrkNocopy
{
public:
    explicit WorkerTracer(int32_t* cnter) : m_cnter(cnter)
    {
        atomic_inc(m_cnter);
    }
    ~WorkerTracer()
    {
        atomic_dec(m_cnter);
    }
private:
    int32_t * m_cnter;
};

class SemaGuard : IrkNocopy
{
public:
    explicit SemaGuard(Semaphore& sema)
    {
        m_pSema = &sema;
        m_pSema->wait();
    }
    ~SemaGuard()
    {
        if (m_pSema)
            m_pSema->post();
    }
    void unlock()
    {
        if (m_pSema)
        {
            m_pSema->post();
            m_pSema = NULL;
        }
    }
private:
    Semaphore * m_pSema;
};

// queue used to store cargo index, allocated in the shared memory
class CargoQueue : IrkNocopy
{
public:
    struct Header
    {
        int32_t maxCnt;     // max item count allowed
        int32_t curCnt;     // current available item count
        int32_t rIdx;       // current read index
        int32_t nWaiters;   // number of waiting clients
    };
    static const size_t kHeaderSize = sizeof(Header);

    static int shm_bytes(int maxCnt)
    {
        size_t qsize = kHeaderSize + maxCnt * sizeof(int);
        return static_cast<int>(ALIGN_16(qsize));
    }

    CargoQueue() : m_header(nullptr), m_buff(nullptr) {}
    ~CargoQueue() {}

    // create new queue
    IpcResult create(const char*, const char*, char* shmBuf, int curCnt, int maxCnt);

    // open existing queue
    IpcResult open(const char*, const char*, char* shmBuf);

    // shutdown queue, make all waiters return
    void shutdown();

    // push cargo index
    IpcResult push_back(int carIdx);

    // pop cargo index
    IpcResult pop_front(int* carIdx, int timeout);
    IpcResult pop_back(int* carIdx, int timeout);

private:
    Header * m_header;
    int*        m_buff;
    Semaphore   m_cntSema;
    Semaphore   m_lockSema;
};

IpcResult CargoQueue::create(const char* cntSemaName, const char* lockSemaName, char* shmBuf,
    int curCnt, int maxCnt)
{
    IpcResult rst = m_cntSema.create(cntSemaName, curCnt);
    if (!rst)
        return rst;
    rst = m_lockSema.create(lockSemaName, 1);
    if (!rst)
        return rst;

    m_header = (Header*)shmBuf;
    m_header->maxCnt = maxCnt;
    m_header->curCnt = curCnt;
    m_header->rIdx = 0;
    m_header->nWaiters = 0;
    m_buff = (int*)(shmBuf + kHeaderSize);
    return IpcResult::make_ok();
}

IpcResult CargoQueue::open(const char* cntSemaName, const char* lockSemaName, char* shmBuf)
{
    IpcResult rst = m_cntSema.open(cntSemaName);
    if (!rst)
        return rst;
    rst = m_lockSema.open(lockSemaName);
    if (!rst)
        return rst;

    m_header = (Header*)shmBuf;
    m_buff = (int*)(shmBuf + kHeaderSize);
    return IpcResult::make_ok();
}

void CargoQueue::shutdown()
{
    if (m_header)
    {
        m_lockSema.wait();
        m_header->curCnt = -1;      // mark shutdown
        int waiterCnt = m_header->nWaiters;
        m_lockSema.post();

        // make all waiters return
        while (waiterCnt-- > 0)
            m_cntSema.post();
    }
}

IpcResult CargoQueue::push_back(int carIdx)
{
    assert(m_header && m_buff);
    SemaGuard guard_(m_lockSema);

    if (m_header->curCnt < 0 || m_header->curCnt >= m_header->maxCnt)
    {
        return IpcResult::make_failed(ERROR_BROKEN_PIPE);
    }
    int wIdx = (m_header->rIdx + m_header->curCnt) % m_header->maxCnt;  // write index
    m_header->curCnt += 1;
    m_buff[wIdx] = carIdx;

    guard_.unlock();
    m_cntSema.post();
    return IpcResult::make_ok();
}

IpcResult CargoQueue::pop_front(int* pCarIdx, int timeout)
{
    assert(m_header && m_buff);
    WorkerTracer wtrac_(&m_header->nWaiters);

    while (!(m_header->curCnt < 0))
    {
        // wait cargo available 
        auto rst = m_cntSema.wait(timeout);
        if (!rst)
            return rst;

        SemaGuard guard_(m_lockSema);
        if (m_header->curCnt < 0)          // queuq shutdown
            break;
        else if (m_header->curCnt == 0)    // someone take it
            continue;

        *pCarIdx = m_buff[m_header->rIdx];
        m_header->curCnt -= 1;
        if (m_header->curCnt == 0)
            m_header->rIdx = 0;
        else
            m_header->rIdx = (m_header->rIdx + 1) % m_header->maxCnt;
        return IpcResult::make_ok();
    }
    return IpcResult::make_failed(ERROR_BROKEN_PIPE);
}

IpcResult CargoQueue::pop_back(int* pCarIdx, int timeout)
{
    assert(m_header && m_buff);
    WorkerTracer wtrac_(&m_header->nWaiters);

    while (!(m_header->curCnt < 0))
    {
        // wait cargo available 
        auto rst = m_cntSema.wait(timeout);
        if (!rst)
            return rst;

        SemaGuard guard_(m_lockSema);
        if (m_header->curCnt < 0)          // queuq shutdown
            break;
        else if (m_header->curCnt == 0)    // someone take it
            continue;

        m_header->curCnt -= 1;
        int backIdx = (m_header->rIdx + m_header->curCnt) % m_header->maxCnt;
        *pCarIdx = m_buff[backIdx];
        return IpcResult::make_ok();
    }
    return IpcResult::make_failed(ERROR_BROKEN_PIPE);
}

inline void purge_cargo(IpcCargo& cargo)
{
    cargo.data = nullptr;
    cargo.size = 0;
    cargo.capacity = 0;
    cargo.opaque = -1;
}

class ImpFrtServer : IrkNocopy
{
public:
    ImpFrtServer() : m_carSize(0), m_shmSize(0), m_shmBuf(nullptr), m_shmCars(nullptr) {}
    ~ImpFrtServer()
    {
        if (m_shmBuf)
            m_shmObj.unmap(m_shmBuf, m_shmSize);
    }

    // create freight service
    IpcResult create(const char* name, int cargoSize, int cargoCnt);

    // shutdown freight service, make all waiters return
    void shutdown();

    // receive request from client, wait forever if "milliseconds" < 0
    IpcResult recv(IpcCargo& cargo, int milliseconds);

    // send ack back to client
    IpcResult send(IpcCargo& cargo);

    // discard unwanted cargo
    IpcResult discard(IpcCargo& cargo);

private:
    int         m_carSize;
    int         m_shmSize;
    char*       m_shmBuf;
    char*       m_shmCars;
    IpcShmObj   m_shmObj;
    CargoQueue  m_freeQueue;
    CargoQueue  m_reqQueue;
    CargoQueue  m_ackQueue;
    Semaphore   m_serverSeam;
};

IpcResult ImpFrtServer::create(const char* name, int cargoSize, int cargoCnt)
{
    assert(name != nullptr && cargoSize > 0 && cargoCnt > 0);
    assert(!m_shmBuf && !m_shmCars);

    const std::string serverName(name);
    const int queueSize = CargoQueue::shm_bytes(cargoCnt);
    m_carSize = ALIGN_16(cargoSize) + 16;     // extra 16 bytes for data size
    m_shmSize = SHMFRT_HEADER_SIZE + queueSize * 3 + m_carSize * cargoCnt;

    // create shm object
    std::string shmName = serverName + s_suffixShm;
    IpcResult rst = m_shmObj.create(shmName.c_str(), m_shmSize);
    if (!rst)
        return rst;
    rst = m_shmObj.map(0, m_shmSize, (void**)&m_shmBuf);
    if (!rst)
        return rst;
    assert(((uintptr_t)m_shmBuf & 15) == 0);  // at least 16-byte aligned

    // fill free cargo index
    char* shmFreeQ = m_shmBuf + SHMFRT_HEADER_SIZE;
    int* cIdxAry = (int*)(shmFreeQ + CargoQueue::kHeaderSize);
    for (int idx = 0; idx < cargoCnt; idx++)
        cIdxAry[idx] = cargoCnt - 1 - idx;

    // create free queue
    std::string cntSemaName = serverName + s_suffixFreeCnt;
    std::string lockSemaName = serverName + s_suffixFreeLock;
    rst = m_freeQueue.create(cntSemaName.c_str(), lockSemaName.c_str(), shmFreeQ, cargoCnt, cargoCnt);
    if (!rst)
        return rst;

    // create request queue
    char* shmReqQ = shmFreeQ + queueSize;
    cntSemaName = serverName + s_suffixReqCnt;
    lockSemaName = serverName + s_suffixReqLock;
    rst = m_reqQueue.create(cntSemaName.c_str(), lockSemaName.c_str(), shmReqQ, 0, cargoCnt);
    if (!rst)
        return rst;

    // create ack queue
    char* shmAckQ = shmReqQ + queueSize;
    cntSemaName = serverName + s_suffixAckCnt;
    lockSemaName = serverName + s_suffixAckLock;
    rst = m_ackQueue.create(cntSemaName.c_str(), lockSemaName.c_str(), shmAckQ, 0, cargoCnt);
    if (!rst)
        return rst;

    *(int32_t*)m_shmBuf = 0;                    // server status ready
    *(int32_t*)(m_shmBuf + 4) = 0;              // number of workers in progress
    *(int32_t*)(m_shmBuf + 8) = m_carSize;      // cargo size
    *(int32_t*)(m_shmBuf + 12) = cargoCnt;      // max cargo count allowed
    m_shmCars = shmAckQ + queueSize;            // cargos shared memory

    // mark server ready
    rst = m_serverSeam.create(name, 1);
    if (!rst)
        return rst;

    return IpcResult::make_ok();
}

void ImpFrtServer::shutdown()
{
    m_serverSeam.close();
    if (!m_shmBuf)
        return;

    // mark server shutdown
    atomic_store((int*)m_shmBuf, -1);

    // shutdown all queue
    m_freeQueue.shutdown();
    m_reqQueue.shutdown();
    m_ackQueue.shutdown();

    // wait all waiters exited
    const int* nWaiters = (const int*)(m_shmBuf + 4);
    for (int i = 0; i < 100; i++)
    {
        if (atomic_load(nWaiters) == 0)    // no waiters
            break;
#ifdef _WIN32
        ::Sleep(15);
#else
        ::usleep(15 * 1000);
#endif
    }
}

IpcResult ImpFrtServer::recv(IpcCargo& cargo, int milliseconds)
{
    if (!m_shmBuf || *(int*)m_shmBuf != 0) // already shutdown
        return IpcResult::make_failed(ERROR_BROKEN_PIPE);

    WorkerTracer wtrac_((int*)(m_shmBuf + 4));

    int carIdx = -1;
    auto rst = m_reqQueue.pop_front(&carIdx, milliseconds);
    if (!rst)
        return rst;

    assert(carIdx >= 0);
    char* carShm = m_shmCars + carIdx * m_carSize;
    cargo.data = carShm + 16;
    cargo.size = *(int*)carShm;
    cargo.capacity = m_carSize - 16;
    cargo.opaque = carIdx;
    return IpcResult::make_ok();
}

IpcResult ImpFrtServer::send(IpcCargo& cargo)
{
    if (!m_shmBuf || *(int*)m_shmBuf != 0) // already shutdown
        return IpcResult::make_failed(ERROR_BROKEN_PIPE);

    WorkerTracer wtrac_((int*)(m_shmBuf + 4));

    const int carIdx = cargo.opaque;
    char* carShm = m_shmCars + carIdx * m_carSize;
    assert(carIdx >= 0 && (carShm + 16) == cargo.data);

    *(int*)carShm = cargo.size;
    auto rst = m_ackQueue.push_back(carIdx);
    if (!rst)
        return rst;

    purge_cargo(cargo);
    return IpcResult::make_ok();
}

IpcResult ImpFrtServer::discard(IpcCargo& cargo)
{
    if (!m_shmBuf || *(int*)m_shmBuf != 0) // already shutdown
        return IpcResult::make_failed(ERROR_BROKEN_PIPE);

    WorkerTracer wtrac_((int*)(m_shmBuf + 4));

    const int carIdx = cargo.opaque;
    char* carShm = m_shmCars + carIdx * m_carSize;
    assert(carIdx >= 0 && (carShm + 16) == cargo.data);

    *(int*)carShm = -1;
    auto rst = m_freeQueue.push_back(carIdx);
    if (!rst)
        return rst;

    purge_cargo(cargo);
    return IpcResult::make_ok();
}

class ImpFrtClient : IrkNocopy
{
public:
    ImpFrtClient() : m_carSize(0), m_shmSize(0), m_shmBuf(nullptr), m_shmCars(nullptr) {}
    ~ImpFrtClient()
    {
        if (m_shmBuf)
            m_shmObj.unmap(m_shmBuf, m_shmSize);
    }

    // open existing freight service
    IpcResult open(const char* name);

    // create new cargo
    IpcResult create(IpcCargo& cargo);

    // send request to server
    IpcResult send(IpcCargo& cargo);

    // receive ack from server, wait forever if "milliseconds" < 0
    IpcResult recv(IpcCargo& cargo, int milliseconds);

    // discard unwanted cargo
    IpcResult discard(IpcCargo& cargo);

private:
    int         m_carSize;
    int         m_shmSize;
    char*       m_shmBuf;
    char*       m_shmCars;
    IpcShmObj   m_shmObj;
    CargoQueue  m_freeQueue;
    CargoQueue  m_reqQueue;
    CargoQueue  m_ackQueue;
    Semaphore   m_serverSeam;
};

IpcResult ImpFrtClient::open(const char* name)
{
    IpcResult rst = m_serverSeam.open(name);
    if (!rst)          // server not ready
        return rst;

    // open shm object
    const std::string serverName(name);
    const std::string shmName = serverName + s_suffixShm;
    rst = m_shmObj.open(shmName.c_str());
    if (!rst)
        return rst;

    // get shm info
    int* header = nullptr;
    rst = m_shmObj.map(0, SHMFRT_HEADER_SIZE, (void**)&header);
    if (!rst)
        return rst;
    if (header[0] != 0)    // server already shutdown
        return IpcResult::make_failed(ERROR_BROKEN_PIPE);
    m_carSize = header[2];
    const int cargoCnt = header[3];
    const int queueSize = CargoQueue::shm_bytes(cargoCnt);
    m_shmSize = SHMFRT_HEADER_SIZE + queueSize * 3 + m_carSize * cargoCnt;
    m_shmObj.unmap(header, SHMFRT_HEADER_SIZE);
    header = nullptr;

    rst = m_shmObj.map(0, m_shmSize, (void**)&m_shmBuf);
    if (!rst)
        return rst;

    // open free queue
    char* shmFreeQ = m_shmBuf + SHMFRT_HEADER_SIZE;
    std::string cntSemaName = serverName + s_suffixFreeCnt;
    std::string lockSemaName = serverName + s_suffixFreeLock;
    rst = m_freeQueue.open(cntSemaName.c_str(), lockSemaName.c_str(), shmFreeQ);
    if (!rst)
        return rst;

    // open request queue
    char* shmReqQ = shmFreeQ + queueSize;
    cntSemaName = serverName + s_suffixReqCnt;
    lockSemaName = serverName + s_suffixReqLock;
    rst = m_reqQueue.open(cntSemaName.c_str(), lockSemaName.c_str(), shmReqQ);
    if (!rst)
        return rst;

    // open ack queue
    char* shmAckQ = shmReqQ + queueSize;
    cntSemaName = serverName + s_suffixAckCnt;
    lockSemaName = serverName + s_suffixAckLock;
    rst = m_ackQueue.open(cntSemaName.c_str(), lockSemaName.c_str(), shmAckQ);
    if (!rst)
        return rst;

    m_shmCars = shmAckQ + queueSize;    // cargo array
    return IpcResult::make_ok();
}

IpcResult ImpFrtClient::create(IpcCargo& cargo)
{
    int carIdx = -1;
    auto rst = m_freeQueue.pop_back(&carIdx, 0);
    if (!rst)
    {
        if (rst.is_timeout())
        {
#ifdef _WIN32
            return IpcResult::make_failed(ERROR_INSUFFICIENT_BUFFER);
#else
            return IpcResult::make_failed(ENOBUFS);
#endif
        }
        return rst;
    }

    assert(carIdx >= 0);
    cargo.data = m_shmCars + carIdx * m_carSize + 16;
    cargo.size = 0;
    cargo.capacity = m_carSize - 16;
    cargo.opaque = carIdx;
    return IpcResult::make_ok();
}

IpcResult ImpFrtClient::send(IpcCargo& cargo)
{
    const int carIdx = cargo.opaque;
    char* carShm = m_shmCars + carIdx * m_carSize;
    assert(carIdx >= 0 && (carShm + 16) == cargo.data);

    *(int*)carShm = cargo.size;
    auto rst = m_reqQueue.push_back(carIdx);
    if (!rst)
        return rst;

    purge_cargo(cargo);
    return IpcResult::make_ok();
}

IpcResult ImpFrtClient::recv(IpcCargo& cargo, int milliseconds)
{
    int carIdx = -1;
    auto rst = m_ackQueue.pop_front(&carIdx, milliseconds);
    if (!rst)
        return rst;

    assert(carIdx >= 0);
    char* carShm = m_shmCars + carIdx * m_carSize;
    cargo.data = carShm + 16;
    cargo.size = *(int*)carShm;
    cargo.capacity = m_carSize - 16;
    cargo.opaque = carIdx;
    return IpcResult::make_ok();
}

IpcResult ImpFrtClient::discard(IpcCargo& cargo)
{
    const int carIdx = cargo.opaque;
    char* carShm = m_shmCars + carIdx * m_carSize;
    assert(carIdx >= 0 && (carShm + 16) == cargo.data);

    *(int*)carShm = -1;
    auto rst = m_freeQueue.push_back(carIdx);
    if (!rst)
        return rst;

    purge_cargo(cargo);
    return IpcResult::make_ok();
}

IpcFreightServer::IpcFreightServer() : m_pImpl(nullptr)
{
}
IpcFreightServer::~IpcFreightServer()
{
    this->close();
}

// create new shared memory freight service
// maxCargoSize: the maximum size of a single cargo
// maxCargoCnt:  the maximum number of cargos can be created at one time
IpcResult IpcFreightServer::create(const char* name, int maxCargoSize, int maxCargoCnt)
{
    assert(!m_pImpl);
    m_pImpl = new ImpFrtServer;

    auto rst = m_pImpl->create(name, maxCargoSize, maxCargoCnt);
    if (!rst)
    {
        delete m_pImpl;
        m_pImpl = nullptr;
    }
    return rst;
}

// close freight service
void IpcFreightServer::close()
{
    if (m_pImpl)
    {
        m_pImpl->shutdown();    // shutdown cargo server, make all waiters return
        delete m_pImpl;
        m_pImpl = nullptr;
    }
}

// receive request from client, wait forever if "milliseconds" < 0
IpcResult IpcFreightServer::recv(IpcCargo& cargo, int milliseconds)
{
    purge_cargo(cargo);

    if (!m_pImpl)
        return IpcResult::make_failed(-1);
    return m_pImpl->recv(cargo, milliseconds);
}

// send ack back to client
IpcResult IpcFreightServer::send(IpcCargo& cargo)
{
    if (!m_pImpl)
        return IpcResult::make_failed(-1);
    return m_pImpl->send(cargo);
}

// discard used/unwanted cargo
void IpcFreightServer::discard(IpcCargo& cargo)
{
    if (m_pImpl)
        m_pImpl->discard(cargo);
}

IpcFreightClient::IpcFreightClient() : m_pImpl(nullptr)
{
}
IpcFreightClient::~IpcFreightClient()
{
    delete m_pImpl;
}

// open existing shared memory freight service
IpcResult IpcFreightClient::open(const char* name)
{
    assert(!m_pImpl);
    m_pImpl = new ImpFrtClient;

    auto rst = m_pImpl->open(name);
    if (!rst)
    {
        delete m_pImpl;
        m_pImpl = nullptr;
    }
    return rst;
}

// create new cargo
IpcResult IpcFreightClient::create(IpcCargo& cargo)
{
    purge_cargo(cargo);

    if (!m_pImpl)
        return IpcResult::make_failed(-1);
    return m_pImpl->create(cargo);
}

// send request to server
IpcResult IpcFreightClient::send(IpcCargo& cargo)
{
    if (!m_pImpl)
        return IpcResult::make_failed(-1);
    return m_pImpl->send(cargo);
}

// receive ack from server, wait forever if "milliseconds" < 0
IpcResult IpcFreightClient::recv(IpcCargo& cargo, int milliseconds)
{
    purge_cargo(cargo);

    if (!m_pImpl)
        return IpcResult::make_failed(-1);
    return m_pImpl->recv(cargo, milliseconds);
}

// discard used/unwanted cargo
void IpcFreightClient::discard(IpcCargo& cargo)
{
    if (m_pImpl)
        m_pImpl->discard(cargo);
}

//======================================================================================================================
IpcMqServer::IpcMqServer() : m_pImpl(nullptr)
{
    m_pImpl = new ImpMqServer;
}
IpcMqServer::~IpcMqServer()
{
    delete m_pImpl;
}

// create message queue
// maxMsgSize: the maximum size of single message
// maxMsgNum: the maximum number of messages allowed in the queue
IpcResult IpcMqServer::create(const char* mqName, int maxMsgSize, int maxMsgNum)
{
    return m_pImpl->create(mqName, maxMsgSize, maxMsgNum);
}

// delete message queue
void IpcMqServer::close()
{
    m_pImpl->close();
}

// wait client connected, wait forever if "milliseconds" < 0
IpcResult IpcMqServer::wait_connected(int milliseconds)
{
    return m_pImpl->wait_connected(milliseconds);
}

// disconnect current client
void IpcMqServer::disconnect()
{
    m_pImpl->disconnect();
}

// is message queue connected from a client
bool IpcMqServer::connected() const
{
    return m_pImpl->connected();
}

// receive request message from a client, wait forever if "milliseconds" < 0
IpcResult IpcMqServer::recv(void* buf, int bufSize, int& gotten, int milliseconds)
{
    gotten = -1;
    return m_pImpl->recv(buf, bufSize, gotten, milliseconds);
}

// send ack message to the client, wait forever if "milliseconds" < 0
IpcResult IpcMqServer::send(const void* ack, int ackSize, int milliseconds)
{
    return m_pImpl->send(ack, ackSize, milliseconds);
}

IpcMqClient::IpcMqClient()
{
    m_pImpl = new ImpMqClient;
}
IpcMqClient::~IpcMqClient()
{
    delete m_pImpl;
}

// connect to message queue server, wait forever if "milliseconds" < 0
IpcResult IpcMqClient::connect(const char* mqName, int milliseconds)
{
    return m_pImpl->connect(mqName, milliseconds);
}

// is connection to the server succeeded
bool IpcMqClient::connected() const
{
    return m_pImpl->connected();
}

// disconnect/close message queue
void IpcMqClient::disconnect()
{
    return m_pImpl->disconnect();
}

// send request message to the server, wait forever if "milliseconds" < 0
IpcResult IpcMqClient::send(const void* req, int reqSize, int milliseconds)
{
    return m_pImpl->send(req, reqSize, milliseconds);
}

// receive ack message from the server, wait forever if "milliseconds" < 0
IpcResult IpcMqClient::recv(void* buf, int bufSize, int& gotten, int milliseconds)
{
    gotten = -1;
    return m_pImpl->recv(buf, bufSize, gotten, milliseconds);
}

// send request and wait ack, wait forever if "milliseconds" < 0
IpcResult IpcMqClient::transact(const void* req, int reqSize, void* ackBuf, int ackBufSize,
    int& gotten, int milliseconds)
{
    gotten = -1;
    return m_pImpl->transact(req, reqSize, ackBuf, ackBufSize, gotten, milliseconds);
}

//======================================================================================================================
IpcPipeServer::IpcPipeServer() : m_pImpl(nullptr)
{
    m_pImpl = new ImpPipeServer;
}
IpcPipeServer::~IpcPipeServer()
{
    delete m_pImpl;
}

// create new pipe server
// txBufSize: the size of transmission buffer
IpcResult IpcPipeServer::create(const char* pipeName, int txBufSize)
{
    return m_pImpl->create(pipeName, txBufSize);
}

// delete/close pipe server
void IpcPipeServer::close()
{
    m_pImpl->close();
}

// wait new client connected, wait forever if "milliseconds" < 0
IpcResult IpcPipeServer::wait_connected(int milliseconds)
{
    return m_pImpl->wait_connected(milliseconds);
}

// disconnect current client
void IpcPipeServer::disconnect()
{
    m_pImpl->disconnect();
}

// is already connected by a client
bool IpcPipeServer::connected() const
{
    return m_pImpl->connected();
}

// receive request from a client, wait forever if "milliseconds" < 0
IpcResult IpcPipeServer::recv(void* buf, int bufSize, int& gotten, int milliseconds)
{
    gotten = -1;
    return m_pImpl->recv(buf, bufSize, gotten, milliseconds);
}

// send ack to the client, wait forever if "milliseconds" < 0
IpcResult IpcPipeServer::send(const void* ack, int ackSize, int milliseconds)
{
    return m_pImpl->send(ack, ackSize, milliseconds);
}

IpcPipeClient::IpcPipeClient() : m_pImpl(nullptr)
{
    m_pImpl = new ImpPipeClient;
}
IpcPipeClient::~IpcPipeClient()
{
    delete m_pImpl;
}

// connect to existing server, wait forever if "milliseconds" < 0
IpcResult IpcPipeClient::connect(const char* pipeName, int milliseconds)
{
    return m_pImpl->connect(pipeName, milliseconds);
}

// disconnect
void IpcPipeClient::disconnect()
{
    m_pImpl->disconnect();
}

// is connection to the server succeeded
bool IpcPipeClient::connected() const
{
    return m_pImpl->connected();
}

// send request to the server, wait forever if "milliseconds" < 0
IpcResult IpcPipeClient::send(const void* req, int reqSize, int milliseconds)
{
    return m_pImpl->send(req, reqSize, milliseconds);
}

// receive ack from the server, wait forever if "milliseconds" < 0
IpcResult IpcPipeClient::recv(void* buf, int bufSize, int& gotten, int milliseconds)
{
    gotten = -1;
    return m_pImpl->recv(buf, bufSize, gotten, milliseconds);
}

}   // namespace irk
