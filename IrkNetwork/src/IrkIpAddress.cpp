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

#ifndef _WIN32
#include <arpa/inet.h>
#endif
#include <stdio.h>
#include "IrkIpAddress.h"

namespace irk {

// fill from address string in dotted decimal form
bool Ipv4Addr::from_string( const char* szAddr )
{
    if( ::inet_pton( AF_INET, szAddr, (void*)&m_addr ) > 0 )
        return true;
    m_addr.s_addr = 0;
    return false;
}

// return address string in dotted decimal format
bool Ipv4Addr::to_string( char* buf, size_t size ) const
{
    return ::inet_ntop( AF_INET, (void*)&m_addr, buf, size ) != nullptr;
}
std::string Ipv4Addr::to_string() const
{
    char buf[16];
    if( ::inet_ntop( AF_INET, (void*)&m_addr, buf, 16 ) )
        return std::string( buf );
    return std::string{};
}

// fill from IPv6 address string
bool Ipv6Addr::from_string( const char* szAddr )
{
    if( ::inet_pton( AF_INET6, szAddr, (void*)&m_addr ) > 0 )
        return true;
    memset( &m_addr, 0, sizeof(m_addr) );
    return false;
}

// return IPv6 address string
bool Ipv6Addr::to_string( char* buf, size_t size ) const
{
    return ::inet_ntop( AF_INET6, (void*)&m_addr, buf, size ) != nullptr;
}
std::string Ipv6Addr::to_string() const
{
    char buf[INET6_ADDRSTRLEN];
    if( ::inet_ntop( AF_INET6, (void*)&m_addr, buf, INET6_ADDRSTRLEN ) )
        return std::string( buf );  
    return std::string{};
}

// fill from address string
bool IpAddr::from_string( const char* szAddr )
{
    if( m_addr.v4.from_string( szAddr ) )
    {
        m_family = AF_INET;
        return true;
    }
    else if( m_addr.v6.from_string( szAddr ) )
    {
        m_family = AF_INET6;
        return true;
    }

    m_family = AF_INET;
    m_addr = AddrUnion{};
    return false;
}

// return address string
bool IpAddr::to_string( char* buf, size_t size ) const
{
    if( m_family == AF_INET )
        return m_addr.v4.to_string( buf, size );
    else
        return m_addr.v6.to_string( buf, size );
}
std::string IpAddr::to_string() const
{
    if( m_family == AF_INET )
        return m_addr.v4.to_string();
    else
        return m_addr.v6.to_string();
}

// set from address string and port number
bool SocketAddr::set( const char* szAddr, uint16_t port )
{
    if( ::inet_pton( AF_INET, szAddr, (void*)&m_sa.v4.sin_addr ) > 0 )
    {
        m_sa.v4.sin_family = AF_INET;
        m_sa.v4.sin_port = htons( port );
        return true;
    }
    else if( ::inet_pton( AF_INET6, szAddr, (void*)&m_sa.v6.sin6_addr ) > 0 )
    {
        m_sa.v6.sin6_family = AF_INET6;
        m_sa.v6.sin6_port = htons( port );
        return true;
    }

    // set to IPv4 unspecified address if failed
    m_sa.v4.sin_family = AF_INET;
    m_sa.v4.sin_addr.s_addr = 0;
    m_sa.v4.sin_port = 0;
    return false;
}

std::string SocketAddr::to_string() const
{
    std::string out;
    char szAddr[64];

    if( this->is_ipv4() )
    {
        if( ::inet_ntop( AF_INET, (void*)&m_sa.v4.sin_addr, szAddr, 64 ) )
        {
            out = szAddr;
            out += ':';
            out += std::to_string( ntohs(m_sa.v4.sin_port) );
        }
    }
    else if( this->is_ipv6() )
    {
        if( ::inet_ntop( AF_INET6, (void*)&m_sa.v6.sin6_addr, szAddr, 64 ) )
        {
            out = "[";
            out += szAddr;
            out += "]:";
            out += std::to_string( ntohs(m_sa.v6.sin6_port) );
        }
    }
    return out;
}

bool SocketAddr::to_string( char* buf, size_t size ) const
{
    char szAddr[64];

    if( this->is_ipv4() )
    {
        if( ::inet_ntop( AF_INET, (void*)&m_sa.v4.sin_addr, szAddr, 64 ) )
        {
            int cnt = ::snprintf( buf, size, "%s:%u", szAddr, ntohs(m_sa.v4.sin_port) );
            return cnt > 0 && cnt < (int)size;
        }
    }
    else if( this->is_ipv6() )
    {
        if( ::inet_ntop( AF_INET6, (void*)&m_sa.v6.sin6_addr, szAddr, 64 ) )
        {
            int cnt = ::snprintf( buf, size, "[%s]:%u", szAddr, ntohs(m_sa.v6.sin6_port) );
            return cnt > 0 && cnt < (int)size;
        }
    }
    return false;
}

} // namespace irk
