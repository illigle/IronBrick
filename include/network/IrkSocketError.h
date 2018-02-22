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

#ifndef _IRONBRICK_SOCKETERROR_H_
#define _IRONBRICK_SOCKETERROR_H_

#ifdef _WIN32
#include <WinSock2.h>
#define OS_SOCK_ERRC(e) WSA##e
#else
#define OS_SOCK_ERRC(e) e
#endif
#include <errno.h>

#ifndef _WIN32
#define socket_last_err()   errno
#else
#define socket_last_err()   ::WSAGetLastError()
#endif

namespace irk {

// common socket transmission return type
struct SockResult
{
    int condition;  // ok, timeout, pending, failed
    int errc;       // native system error code

    // condition enum
    enum
    {
        Ok = 0,
        Timeout,
        Pending,
        Failed,
    };

    explicit operator bool() const          { return this->condition == Ok; } // succeeded
    bool is_ok() const                      { return this->condition == Ok; }
    bool is_timeout() const                 { return this->condition == Timeout; }
    bool is_pending() const                 { return this->condition == Pending; }

    static SockResult make_ok()             { return {Ok, 0}; }
    static SockResult make_timeout(int ec)  { return {Timeout, ec}; }
    static SockResult make_pending(int ec)  { return {Pending, ec}; }
    static SockResult make_failed(int ec)   { return {Failed, ec}; }
    static SockResult make_failed()         { return {Failed, socket_last_err()}; }
};

// common native socket error code
struct SockErrc
{
    enum
    {
        Ok           = 0,                                   // no error
        Interrupted  = OS_SOCK_ERRC(EINTR),                 // interrupted
        Timeout      = OS_SOCK_ERRC(ETIMEDOUT),             // timed out
        BadSocket    = OS_SOCK_ERRC(EBADF),                 // bad socket handle/descriptor
        NotSocket    = OS_SOCK_ERRC(ENOTSOCK),              // not a socket
        AccessDenied = OS_SOCK_ERRC(EACCES),                // access denied
        Fault        = OS_SOCK_ERRC(EFAULT),                // bad pointer
        InvalidArg   = OS_SOCK_ERRC(EINVAL),                // invalid argument
        MsgSize      = OS_SOCK_ERRC(EMSGSIZE),              // invalid/too small message size
        Nobufs       = OS_SOCK_ERRC(ENOBUFS),               // no buffer space available 
        Shutdown     = OS_SOCK_ERRC(ESHUTDOWN),             // socket had already been shutdown
        WouldBlock   = OS_SOCK_ERRC(EWOULDBLOCK),           // non-blocking operation could not be completed immediately
        InProgess    = OS_SOCK_ERRC(EINPROGRESS),           // a operation is currently executing
        Already      = OS_SOCK_ERRC(EALREADY),              // already had an operation in progress
        ReqDestAddr  = OS_SOCK_ERRC(EDESTADDRREQ),          // required address was missing   
        ProtoType    = OS_SOCK_ERRC(EPROTOTYPE),            // invalid/unknow protocol type
        NoProtoOpt   = OS_SOCK_ERRC(ENOPROTOOPT),           // invalid/unknow protocol option
        ProtoNotSupp = OS_SOCK_ERRC(EPROTONOSUPPORT),       // protocol not supported
        OpNotSupp    = OS_SOCK_ERRC(EOPNOTSUPP),            // operation not supported
        AfNotSupp    = OS_SOCK_ERRC(EAFNOSUPPORT),          // address family not supported
        PfNotSupp    = OS_SOCK_ERRC(EPFNOSUPPORT),          // protocol family not supported
        AddrInUse    = OS_SOCK_ERRC(EADDRINUSE),            // address in use
        AddrNotAvail = OS_SOCK_ERRC(EADDRNOTAVAIL),         // address not available
        NetDown      = OS_SOCK_ERRC(ENETDOWN),              // network is down
        NetUnreach   = OS_SOCK_ERRC(ENETUNREACH),           // network is unreachable
        NetReset     = OS_SOCK_ERRC(ENETRESET),             // network dropped connection because of reset
        HostDown     = OS_SOCK_ERRC(EHOSTDOWN),             // host is down
        HostUnreach  = OS_SOCK_ERRC(EHOSTUNREACH),          // host unreachable
        ConnAborted  = OS_SOCK_ERRC(ECONNABORTED),          // connection aborted by the software
        ConnReset    = OS_SOCK_ERRC(ECONNRESET),            // connection closed by the remote host
        ConnRefused  = OS_SOCK_ERRC(ECONNREFUSED),          // Connection refused
        Connected    = OS_SOCK_ERRC(EISCONN),               // socket already connected
        NotConnected = OS_SOCK_ERRC(ENOTCONN),              // socket not connected    
        TooManyFds   = OS_SOCK_ERRC(EMFILE),                // too many sockets/files opened
        TooManyRefs  = OS_SOCK_ERRC(ETOOMANYREFS),          // too many references to some kernel object
        TooManyLoops = OS_SOCK_ERRC(ELOOP),                 // too many levels of symbolic links
        TooManyUsers = OS_SOCK_ERRC(EUSERS),                // too many users
        QuotaRanOut  = OS_SOCK_ERRC(EDQUOT),                // quota exceeded
        NameTooLong  = OS_SOCK_ERRC(ENAMETOOLONG),          // name is too long
        NotEmpty     = OS_SOCK_ERRC(ENOTEMPTY),             // cannot remove a directory that is not empty
        Stale        = OS_SOCK_ERRC(ESTALE),                // stale file handle
        Remote       = OS_SOCK_ERRC(EREMOTE),               // item is not available locally
#ifdef _WIN32
        Cancelled    = WSAECANCELLED,                       // operation canceled
        NotInited    = WSANOTINITIALISED,                   // network not initialized
#else
        Cancelled    = ECANCELED,                           // operation canceled
        NotInited    = ENXIO,                               // network not initialized
#endif
    };
};

// return system error string of the specified socket error
const char* socket_strerror(int errc);

}
#endif
