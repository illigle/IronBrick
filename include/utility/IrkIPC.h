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

#ifndef _IRONBRICK_IPC_H_
#define _IRONBRICK_IPC_H_

#include "IrkCommon.h"

// some inter-process community utilities

namespace irk {

struct IpcResult
{
    int condition;  // ok, timeout, failed
    int errc;       // native system error code

    enum
    {
        Ok = 0,
        Timeout,
        Failed,
    };

    explicit operator bool() const  { return this->condition == Ok; }   // succeeded
    bool is_ok() const              { return this->condition == Ok; }
    bool is_timeout() const         { return this->condition == Timeout; }

    static IpcResult make_ok()              { return {Ok, 0}; }
    static IpcResult make_timeout(int ec)   { return {Timeout, ec}; }
    static IpcResult make_failed(int ec)    { return {Failed, ec}; }
};

// Named semaphore
class Semaphore : IrkNocopy
{
public:
    Semaphore();
    ~Semaphore();

    // create new semaphore
    // semaName: the name of semaphore, on Linux/Mac must begin with '/'
    IpcResult create(const char* semaName, int initVal = 0);

    // open an existing semaphore
    IpcResult open(const char* semaName);

    // close semaphore, on Linux/Mac also unlink created new semaphore
    void close();

    // post semaphore( semaphore count += 1 )
    IpcResult post();

    // wait semaphore, wait forever if milliseconds < 0, otherwise wait at most milliseconds
    IpcResult wait(int milliseconds = -1);

    // try wait semaphore, return true if semaphore count already > 0
    bool try_wait();

private:
    void* m_Handle;
};

// shared memory object
class IpcShmObj : IrkNocopy
{
public:
    IpcShmObj();
    ~IpcShmObj();

    // create new shared memory object
    IpcResult create(const char* shmName, size_t size);

    // open existing shared memory object
    IpcResult open(const char* shmName);

    // close/unlink shared memory object
    void close();

    // map shared memory object
    IpcResult map(size_t offset, size_t size, void** outPtr);

    // release mapped buffer
    IpcResult unmap(void* buf, size_t size);

private:
    void* m_handle;
};

//======================================================================================================================
// shared memory freight service, used to tansfer massive data between two processes efficiently.
// the client should create a cargo, load it with data, then send it to server,
// the client may then get a ack cargo back, read the data and discard it.
// the server can receive a request cargo, then either send it back as ack or discard it.
// NOTE: created/received cargo must be delivered or discarded.
// NOTE: thread-unsafe.

// IPC cargo == shared memory buffer
struct IpcCargo
{
    IpcCargo() : data(nullptr), size(0), capacity(0), opaque(-1) {}

    void*   data;           // shared memory buffer, always 16 byte aligned
    int     size;           // current data size in shared memory buffer
    int     capacity;       // maximum size of the shared memory buffer
    int     opaque;         // for private usage, do NOT modify
};

class IpcFreightServer : IrkNocopy
{
public:
    IpcFreightServer();
    ~IpcFreightServer();

    // create new shared memory freight service
    // maxCargoSize: the maximum size of a single cargo
    // maxCargoCount: the maximum number of cargos can be created at one time
    IpcResult create(const char* name, int maxCargoSize, int maxCargoCount = 10);

    // close freight service
    void close();

    // receive request from client, wait forever if "milliseconds" < 0, get ownership if succedded
    IpcResult recv(IpcCargo& cargo, int milliseconds = -1);

    // send ack back to client, transfer ownership if succedded
    IpcResult send(IpcCargo& cargo);

    // discard used/unwanted cargo, release ownership
    void discard(IpcCargo& cargo);

private:
    class ImpFrtServer* m_pImpl;
};

class IpcFreightClient : IrkNocopy
{
public:
    IpcFreightClient();
    ~IpcFreightClient();

    // open existing shared memory freight service
    IpcResult open(const char* name);

    // create new cargo, get ownership if succedded
    IpcResult create(IpcCargo& cargo);

    // send request to the server, transfer ownership if succedded
    IpcResult send(IpcCargo& cargo);

    // receive ack from server, wait forever if "milliseconds" < 0, get ownership if succedded
    IpcResult recv(IpcCargo& cargo, int milliseconds = -1);

    // discard used/unwanted cargo, release ownership
    void discard(IpcCargo& cargo);

private:
    class ImpFrtClient* m_pImpl;
};

//======================================================================================================================
// duplex message queue, tansfer data in message/datagram mode.
// on windows using named pipe in PIPE_TYPE_MESSAGE mode,
// on linux using POSIX message queue,
// on other platforms using UNIX domain socket.
// NOTE: on Windows mq name must begin with "\\.\pipe\", on Posix platform must begin with "/"
// NOTE: thread-unsafe

class IpcMqServer : IrkNocopy
{
public:
    IpcMqServer();
    ~IpcMqServer();

    // create new message queue
    // maxMsgSize: the maximum size of single message
    // maxMsgCount: the maximum number of messages allowed in the queue
    IpcResult create(const char* mqName, int maxMsgSize, int maxMsgCount = 10);

    // delete/close message queue
    void close();

    // wait new client connected, wait forever if "milliseconds" < 0
    IpcResult wait_connected(int milliseconds = -1);

    // disconnect current client
    void disconnect();

    // is already connected by a client
    bool connected() const;

    // receive request message from a client, wait forever if "milliseconds" < 0
    // gotten = the real size of message received, undefined if failed
    IpcResult recv(void* buf, int bufSize, int& gotten, int milliseconds = -1);

    // send ack message to the client, wait forever if "milliseconds" < 0
    IpcResult send(const void* ack, int ackSize, int milliseconds = -1);

private:
    class ImpMqServer* m_pImpl;
};

class IpcMqClient : IrkNocopy
{
public:
    IpcMqClient();
    ~IpcMqClient();

    // connect to existing message queue server, wait forever if "milliseconds" < 0
    IpcResult connect(const char* mqName, int milliseconds = -1);

    // disconnect
    void disconnect();

    // is connection to the server succeeded
    bool connected() const;

    // send request message to the server, wait forever if "milliseconds" < 0
    IpcResult send(const void* req, int reqSize, int milliseconds = -1);

    // receive ack message from the server, wait forever if "milliseconds" < 0
    // gotten = the real size of ack received, undefined if failed
    IpcResult recv(void* buf, int bufSize, int& gotten, int milliseconds = -1);

    // send request and wait ack, wait forever if "milliseconds" < 0
    // gotten = the real size of ack received, undefined if failed
    IpcResult transact(const void* req, int reqSize, void* ackBuf, int ackBufSize,
        int& gotten, int milliseconds = -1);

private:
    class ImpMqClient* m_pImpl;
};

//======================================================================================================================
// duplex pipe, tansfer data in byte/stream mode.
// on windows using named pipe in PIPE_TYPE_BYTE mode,
// other platform using UNIX domain socket.
// NOTE: on Windows pipe name must begin with "\\.\pipe\", on Posix platform must begin with "/"
// NOTE: thread-unsafe

class IpcPipeServer
{
public:
    IpcPipeServer();
    ~IpcPipeServer();

    // create new pipe server
    // txBufSize: the size of transmission buffer
    IpcResult create(const char* pipeName, int txBufSize);

    // delete/close pipe server
    void close();

    // wait new client connected, wait forever if "milliseconds" < 0
    IpcResult wait_connected(int milliseconds = -1);

    // disconnect current client
    void disconnect();

    // is already connected by a client
    bool connected() const;

    // receive request from a client, wait forever if "milliseconds" < 0
    // gotten = the size of data received, undefined if failed
    IpcResult recv(void* buf, int bufSize, int& gotten, int milliseconds = -1);

    // send ack to the client, wait forever if "milliseconds" < 0
    IpcResult send(const void* ack, int ackSize, int milliseconds = -1);

private:
    class ImpPipeServer* m_pImpl;
};

class IpcPipeClient
{
public:
    IpcPipeClient();
    ~IpcPipeClient();

    // connect to existing server, wait forever if "milliseconds" < 0
    IpcResult connect(const char* pipeName, int milliseconds = -1);

    // disconnect
    void disconnect();

    // is connection to the server succeeded
    bool connected() const;

    // send request to the server, wait forever if "milliseconds" < 0
    IpcResult send(const void* req, int reqSize, int milliseconds = -1);

    // receive ack from the server, wait forever if "milliseconds" < 0
    // gotten = the size of data received, undefined if failed
    IpcResult recv(void* buf, int bufSize, int& gotten, int milliseconds = -1);

private:
    class ImpPipeClient* m_pImpl;
};

}   // namespace irk
#endif
