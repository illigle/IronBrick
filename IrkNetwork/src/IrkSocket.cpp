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
#include <ctype.h>
#else
#include <net/if.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <signal.h>
#endif
#include "IrkSocket.h"

namespace irk {

#ifdef _WIN32

// initialize network
int network_setup()
{
    WSADATA wsaData;
    return ::WSAStartup( MAKEWORD(2, 2), &wsaData );
}

// deinitialize network
void network_teardown()
{
    ::WSACleanup();
}

// return system error string of the specified socket error
const char* socket_strerror( int errc )
{
    static thread_local char s_MsgBuf[1024] = {};

    DWORD len = ::FormatMessageA( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, 
                                NULL, errc, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
                                s_MsgBuf, 1023, NULL );
    s_MsgBuf[len] = 0;

    // remove tailing line-break
    while( len > 0 && ::isspace(s_MsgBuf[len-1]) )
        s_MsgBuf[--len] = 0;

    return s_MsgBuf;
}

#else

// initialize network
int network_setup()
{
    ::signal( SIGPIPE, SIG_IGN );   // disable SIGPIPE
    return 0;
}

// deinitialize network
void network_teardown()
{
}

// return system error string of the specified socket error
const char* socket_strerror( int errc )
{
    return ::strerror( errc );
}

static SockResult poll_wait_event( int sockfd, int evt, int milliseconds )
{
    struct pollfd pfd[1];
    pfd[0].fd = sockfd;
    pfd[0].events = evt;
    pfd[0].revents = 0;
    
    int res = ::poll( pfd, 1, milliseconds );
    if( res == 0 )
        return SockResult::make_timeout( ETIMEDOUT );
    else if( res < 0 )
        return SockResult::make_failed( errno );
    
    if( !(pfd[0].revents & evt) )
        return SockResult::make_failed( ECONNRESET );
    
    return SockResult::make_ok();
}

#endif

//======================================================================================================================

// wait socket writeable
static inline SockResult wait_sendable( SOCKET sockfd, int milliseconds )
{
#ifdef _WIN32
    struct timeval tmout;
    tmout.tv_sec = (milliseconds / 1000);
    tmout.tv_usec = (milliseconds - 1000 * tmout.tv_sec) * 1000;
    fd_set wrset;
    FD_ZERO( &wrset );
    FD_SET( sockfd, &wrset );
    int res = ::select( 0, NULL, &wrset, NULL, &tmout );
    if( res == 0 )  // timeout
    {
        return SockResult::make_timeout( WSAETIMEDOUT );
    }
    else if( res < 0 || !FD_ISSET(sockfd, &wrset) ) // error
    {
        return SockResult::make_failed( socket_last_err() );
    }
    return SockResult::make_ok();
#else
    return poll_wait_event( sockfd, POLLOUT, milliseconds );
#endif
}

// wait socket readable
static inline SockResult wait_recvable( SOCKET sockfd, int ms )
{
#ifdef _WIN32
    struct timeval tmout;
    tmout.tv_sec = (ms / 1000);
    tmout.tv_usec = (ms - 1000 * tmout.tv_sec) * 1000;
    fd_set rdset;
    FD_ZERO( &rdset );
    FD_SET( sockfd, &rdset );
    int res = ::select( 0, &rdset, NULL, NULL, &tmout );
    if( res == 0 )  // timeout
    {
        return SockResult::make_timeout( WSAETIMEDOUT );
    }
    else if( res < 0 || !FD_ISSET(sockfd, &rdset) ) // error
    {
        return SockResult::make_failed( socket_last_err() );
    }
    return SockResult::make_ok();
#else
    return poll_wait_event( sockfd, POLLIN, ms );
#endif
}

SockResult UdpSocket::sendto_wait( const SocketAddr& addr, const void* buf, size_t size,
                                int& result, int timeout, int flags )
{
    auto rst = wait_sendable( m_sockfd, timeout );
    if( !rst )
    {
        result = -1;
        return rst;
    }
    return this->sendto( addr, buf, size, result, flags );
}

SockResult UdpSocket::recvfrom_wait( SocketAddr& addr, void* buf, size_t size,
                                int& result, int timeout, int flags )
{
    auto rst = wait_recvable( m_sockfd, timeout );
    if( !rst )
    {
        result = -1;
        return rst;
    }
    return this->recvfrom( addr, buf, size, result, flags );
}

SockResult TcpSocket::send_wait( const void* buf, size_t size, int& result, int timeout, int flags )
{
    auto rst = wait_sendable( m_sockfd, timeout );
    if( !rst )
    {
        result = -1;
        return rst;
    }
    return this->send( buf, size, result, flags );
}

SockResult TcpSocket::recv_wait( void* buf, size_t size, int& result, int timeout, int flags )
{
    auto rst = wait_recvable( m_sockfd, timeout );
    if( !rst )
    {
        result = -1;
        return rst;
    }
    return this->recv( buf, size, result, flags );
}

// repeatedly send data until all data are sent
SockResult TcpSocket::sendall( const void* buf, size_t size, int& result, int flags )
{
    const char* data = (const char*)buf;
    size_t left = size;
    while( left > 0 )
    {
        int sent = 0;
        auto rst = this->send( data, left, sent, flags );
        if( !rst )
        {
            result = (left == size) ? -1 : static_cast<int>(size - left);
            return rst;
        }
        data += sent;
        left -= sent;
    }

    result = static_cast<int>( size );
    return SockResult::make_ok();
}
    
// repeatedly receive data until all data are received
SockResult TcpSocket::recvall( void* buf, size_t size, int& result, int flags )
{
    char* data = (char*)buf;
    size_t left = size;
    while( left > 0 )
    {
        int gotten = 0;
        auto rst = this->recv( data, left, gotten, flags );
        if( !rst )
        {
            result = (left == size) ? -1 : static_cast<int>(size - left);
            return rst;
        }
        else if( gotten == 0 )  // connection has been gracefully closed
        {
            break;
        }
        data += gotten;
        left -= gotten;
    }

    result = static_cast<int>(size - left);
    return SockResult::make_ok();
}

//======================================================================================================================

#ifndef _WIN32

// get interface IPv4 address from index
static in_addr get_interface_addr( SOCKET sockfd, int ifidx )
{
    struct ifreq ifreq;
    if( ifidx > 0 && ::if_indextoname( ifidx, ifreq.ifr_name ) != NULL )
    {
        if( ::ioctl( sockfd, SIOCGIFADDR, &ifreq ) != -1 )
        {
            return ((sockaddr_in*)(&ifreq.ifr_addr))->sin_addr;
        }
    }
    return in_addr{};
}
#endif

static inline void fill_grp_req( const IpAddr& gip, int ifidx, group_req& greq )
{
    greq.gr_interface = ifidx;

    if( gip.is_ipv4() )
    {
        struct sockaddr_in sa = {};
        sa.sin_family = AF_INET;
        sa.sin_addr = gip.as_ipv4().to_native();
        memcpy( &greq.gr_group, &sa, ssizeof(sa) );
    }
    else
    {
        struct sockaddr_in6 sa = {};
        sa.sin6_family = AF_INET6;
        sa.sin6_addr = gip.as_ipv6().to_native();
        memcpy( &greq.gr_group, &sa, ssizeof(sa) );
    }
}

static inline void fill_grp_src( const IpAddr& gip, const IpAddr& sip, int ifidx, group_source_req& gsReq )
{
    gsReq.gsr_interface = ifidx;

    if( gip.is_ipv4() )
    {
        struct sockaddr_in gsa = {};
        gsa.sin_family = AF_INET;
        gsa.sin_addr = gip.as_ipv4().to_native();
        memcpy( &gsReq.gsr_group, &gsa, ssizeof(gsa) );
        struct sockaddr_in ssa = {};
        ssa.sin_family = AF_INET;
        ssa.sin_addr = sip.as_ipv4().to_native();
        memcpy( &gsReq.gsr_source, &ssa, ssizeof(ssa) );
    }
    else
    {
        struct sockaddr_in6 gsa = {};
        gsa.sin6_family = AF_INET6;
        gsa.sin6_addr = gip.as_ipv6().to_native();
        memcpy( &gsReq.gsr_group, &gsa, ssizeof(gsa) );
        struct sockaddr_in6 ssa = {};
        ssa.sin6_family = AF_INET6;
        ssa.sin6_addr = sip.as_ipv6().to_native();
        memcpy( &gsReq.gsr_source, &ssa, ssizeof(ssa) );
    }
}

int McastSocket::join( const IpAddr& grpIP, uint16_t port, int ifidx )
{
    // open socket
    if( m_sockfd == INVALID_SOCKET )
    {
        int res = this->open( grpIP.family() );
        if( res )
            return res;
    }
    assert( m_family == grpIP.family() );

    // bind port
    if( port != 0 )
    {
        SocketAddr saddr( grpIP.family(), port );
        int res = this->bind( saddr );
        if( res )
            return res;
    }

    // join mcast group
    struct group_req grpReq = {};
    fill_grp_req( grpIP, ifidx, grpReq );
    int level = grpIP.is_ipv4() ? IPPROTO_IP : IPPROTO_IPV6;
    return this->set_option( level, MCAST_JOIN_GROUP, &grpReq, ssizeof(grpReq) );
}

int McastSocket::leave( const IpAddr& grpIP, int ifidx )
{
    struct group_req grpReq = {};
    fill_grp_req( grpIP, ifidx, grpReq );
    int level = grpIP.is_ipv4() ? IPPROTO_IP : IPPROTO_IPV6;
    return this->set_option( level, MCAST_LEAVE_GROUP, &grpReq, ssizeof(grpReq) );
}

int McastSocket::join( const IpAddr& grpIP, const IpAddr& srcIP, uint16_t port, int ifidx )
{
    // open socket
    if( m_sockfd == INVALID_SOCKET )
    {
        int res = this->open( grpIP.family() );
        if( res )
            return res;
    }
    assert( m_family == grpIP.family() && m_family == srcIP.family() );

    // bind port
    if( port != 0 )
    {
        SocketAddr saddr( grpIP.family(), port );
        int res = this->bind( saddr );
        if( res )
            return res;
    }

    // join mcast group
    struct group_source_req grpSrc = {};
    fill_grp_src( grpIP, srcIP, ifidx, grpSrc );
    int level = grpIP.is_ipv4() ? IPPROTO_IP : IPPROTO_IPV6;
    return this->set_option( level, MCAST_JOIN_SOURCE_GROUP, &grpSrc, ssizeof(grpSrc) );
}

int McastSocket::leave( const IpAddr & grpIP, const IpAddr & srcIP, int ifidx )
{
    struct group_source_req grpSrc = {};
    fill_grp_src( grpIP, srcIP, ifidx, grpSrc );
    int level = grpIP.is_ipv4() ? IPPROTO_IP : IPPROTO_IPV6;
    return this->set_option( level, MCAST_LEAVE_SOURCE_GROUP, &grpSrc, ssizeof(grpSrc) );
}

int McastSocket::block( const IpAddr& grpIP, const IpAddr& srcIP, int ifidx )
{
    struct group_source_req grpSrc = {};
    fill_grp_src( grpIP, srcIP, ifidx, grpSrc );
    int level = grpIP.is_ipv4() ? IPPROTO_IP : IPPROTO_IPV6;
    return this->set_option( level, MCAST_BLOCK_SOURCE, &grpSrc, ssizeof(grpSrc) );
}

int McastSocket::unblock( const IpAddr& grpIP, const IpAddr& srcIP, int ifidx )
{
    struct group_source_req grpSrc = {};
    fill_grp_src( grpIP, srcIP, ifidx, grpSrc );
    int level = grpIP.is_ipv4() ? IPPROTO_IP : IPPROTO_IPV6;
    return this->set_option( level, MCAST_UNBLOCK_SOURCE, &grpSrc, ssizeof(grpSrc) );
}

int McastSocket::set_loopback( bool enabled )
{
    if( m_family == PF_INET )
    {
#ifdef _WIN32
        uint32_t loop = enabled;
        return this->set_option( IPPROTO_IP, IP_MULTICAST_LOOP, &loop, ssizeof(loop) );
#else
        u_char loop = enabled;
        return this->set_option( IPPROTO_IP, IP_MULTICAST_LOOP, &loop, ssizeof(loop) );
#endif
    }
    else
    {
        uint32_t loop = enabled;
        return this->set_option( IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &loop, ssizeof(loop) );
    }
}

int McastSocket::set_mcast_ttl( uint32_t ttl )
{
    if( m_family == PF_INET )
    {
#ifdef _WIN32
        return this->set_option( IPPROTO_IP, IP_MULTICAST_TTL, &ttl, ssizeof(ttl) );
#else
        u_char ttl8 = (u_char)ttl;
        return this->set_option( IPPROTO_IP, IP_MULTICAST_TTL, &ttl8, ssizeof(ttl8) );
#endif
    }
    else
    {
        return this->set_option( IPPROTO_IPV6, IPV6_MULTICAST_HOPS, &ttl, ssizeof(ttl) );
    }
}

int McastSocket::set_interface( uint32_t ifidx )
{
    if( m_family == PF_INET )
    {
    #ifdef _WIN32
        uint32_t idx32 = htonl( ifidx );
        return this->set_option( IPPROTO_IP, IP_MULTICAST_IF, &idx32, ssizeof(idx32) );
    #else
        struct in_addr addr = get_interface_addr( m_sockfd, ifidx );
        return this->set_option( IPPROTO_IP, IP_MULTICAST_IF, &addr, ssizeof(addr) );
    #endif
    }
    else
    {
        return this->set_option( IPPROTO_IPV6, IPV6_MULTICAST_IF, &ifidx, ssizeof(ifidx) );
    }
}

//======================================================================================================================

int TcpAcceptor::listen_at( const sockaddr* srvAddr, socklen_t len, int backlog )
{
    // open socket
    if( m_sockfd == INVALID_SOCKET )
    {
        int res = this->open( srvAddr->sa_family );
        if( res )
            return res;
        this->set_addr_reusable( true );
    }

    // bind server address
    if( ::bind( m_sockfd, srvAddr, len ) != 0 )
    {
        return socket_last_err();
    }

    // make the socket listening for an incoming connection
    if( ::listen( m_sockfd, backlog ) != 0 )
    {
        return socket_last_err();
    }

    return 0;
}

} // namespace irk
