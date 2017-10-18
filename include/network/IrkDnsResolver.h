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

#ifndef _IRONBRICK_DNSRESOLVER_H_
#define _IRONBRICK_DNSRESOLVER_H_

#ifndef _WIN32
#include <netdb.h>
#endif
#include "IrkIpAddress.h"
#include "IrkSocketError.h"

namespace irk {

// initialize network, return 0 if succeeded, return system error code if failed
extern int network_setup();

// deinitialize network
extern void network_teardown();

// create SocketAddr from native addrinfo struct
inline SocketAddr make_sockaddr( const struct addrinfo* pai )
{
    return SocketAddr{ pai->ai_addr, (socklen_t)pai->ai_addrlen };
}
inline SocketAddr make_sockaddr( const struct addrinfo& ai )
{
    return SocketAddr{ ai.ai_addr, (socklen_t)ai.ai_addrlen };
}

// resouce-safe addrinfo list
class AddrInfoList : IrkNocopy
{
public:
    AddrInfoList( struct addrinfo* ailist, int errc ) : m_aiList(ailist), m_errc(errc) {}
    ~AddrInfoList() 
    { 
        if( m_aiList ) 
            ::freeaddrinfo( m_aiList ); 
    }
    AddrInfoList( AddrInfoList&& other ) noexcept
    {
        m_aiList = other.m_aiList;
        m_errc = other.m_errc;
        other.m_aiList = nullptr;
        other.m_errc = 0;
    }
    AddrInfoList& operator=( AddrInfoList&& other ) noexcept
    {
        if( this != &other )
        {
            if( m_aiList )
                ::freeaddrinfo( m_aiList );
            m_aiList = other.m_aiList;
            m_errc = other.m_errc;
            other.m_aiList = nullptr;
            other.m_errc = 0;
        }
        return *this;
    }
   
    // is empty(getaddrinfo failed)
    bool empty() const                  { return m_aiList == nullptr; }

    // native error code(EAI_*) when getaddrinfo failed
    int error() const                   { return m_errc; }

    // get native addrinfo list, return NULL if empty
    const struct addrinfo* data() const { return m_aiList; }

    // addrinfo list traversal
    class Iterator
    {
    public:
        typedef const struct addrinfo value_type;
        typedef const struct addrinfo* pointer;
        typedef const struct addrinfo& reference;
        bool operator==( const Iterator& other ) const  { return m_ai == other.m_ai; }
        bool operator!=( const Iterator& other ) const  { return m_ai != other.m_ai; }
        Iterator& operator++()
        {
            if( m_ai ) { m_ai = m_ai->ai_next; }
            return *this;
        }
        pointer operator->() const  { return m_ai; }
        reference operator*() const { return *m_ai; }
    private:
        friend AddrInfoList;
        explicit Iterator( pointer ai ) : m_ai(ai) {}
        pointer m_ai;
    };
    Iterator begin() const  { return Iterator(m_aiList); }
    Iterator end() const    { return Iterator(nullptr); }

private:
    struct addrinfo* m_aiList;
    int m_errc;     // error code from getaddrinfo(), EAI_*
};

// get socket address from host and service name, q.v. getaddrinfo API
// if failed return empty list
inline AddrInfoList resolve_host( const char* host, const char* service, const addrinfo* hint=nullptr )
{
    struct addrinfo* ailist = nullptr;
    int res = ::getaddrinfo( host, service, hint, &ailist );
    return AddrInfoList{ ailist, res };
}
inline AddrInfoList resolve_host( const char* host, const char* service, int socktype,
                                  int family=AF_INET, int protocol=0, int flags=0 )
{
    struct addrinfo hint = {};
    hint.ai_flags = flags;
    hint.ai_family = family;        // address family, IPv4/IPv6
    hint.ai_socktype = socktype;    // socket type, stream(TCP) or datagram(UDP)
    hint.ai_protocol = protocol;    // protocol, TCP or UDP

    struct addrinfo* ailist = nullptr;
    int res = ::getaddrinfo( host, service, &hint, &ailist );
    return AddrInfoList{ ailist, res };
}

// get host and service name from socket address
// return 0 if succeeded, if failed return error code(EAI_*)
inline int get_hostname( const sockaddr* sa, socklen_t salen, std::string& host, std::string& service, int flags=0 )
{
    char szHost[NI_MAXHOST], szSrv[NI_MAXSERV];
    int errc = ::getnameinfo( sa, salen, szHost, NI_MAXHOST, szSrv, NI_MAXSERV, flags );
    if( errc == 0 )
    {
        host = szHost;
        service = szSrv;
    }
    return errc;
}
inline int get_hostname( const SocketAddr& addr, std::string& host, std::string& service, int flags=0 )
{
    return get_hostname( addr.data(), addr.length(), host, service, flags );
}
inline int get_hostname( const sockaddr* sa, socklen_t salen, char* host, socklen_t hostlen,
                        char* service, socklen_t srvlen, int flags=0 )
{
    return ::getnameinfo( sa, salen, host, hostlen, service, srvlen, flags );
}
inline int get_hostname( const SocketAddr& addr, char* host, socklen_t hostlen,
                        char* service, socklen_t srvlen, int flags=0 )
{
    return ::getnameinfo( addr.data(), addr.length(), host, hostlen, service, srvlen, flags );
}

// get error messages of DNS resolve EAI_* errors
inline const char* resolve_strerror( int errc )
{
#ifdef _WIN32
    return ::gai_strerrorA( errc );
#else
    return ::gai_strerror( errc );
#endif
}

}   // end of namespace irk
#endif
