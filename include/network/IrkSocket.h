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

#ifndef _IRONBRICK_SOCKET_H_
#define _IRONBRICK_SOCKET_H_

#ifndef _WIN32
#include <unistd.h>
#endif
#include "IrkIpAddress.h"
#include "IrkSocketError.h"

#ifndef _WIN32
typedef int SOCKET;
#define INVALID_SOCKET      -1
#define SOCKET_ERROR        -1
#define closesocket         close
#define ioctlsocket         ioctl
#else
#define SHUT_RD             SD_RECEIVE
#define SHUT_WR             SD_SEND
#define SHUT_RDWR           SD_BOTH
#endif
#define ssizeof(x)  static_cast<int>(sizeof(x))

namespace irk {

// initialize network, return 0 if succeeded, return system error code if failed
extern int network_setup();

// deinitialize network
extern void network_teardown();

class BaseSocket
{
    // NOTE: most methods return 0 if succeeded, return system error code if failed
public:
    // create a socket
    int open(int family, int type, int protocol)
    {
        assert(m_sockfd == INVALID_SOCKET);
        m_sockfd = ::socket(family, type, protocol);
        return (m_sockfd != INVALID_SOCKET) ? 0 : socket_last_err();
    }

    // close socket
    int close()
    {
        if (m_sockfd != INVALID_SOCKET)
        {
            if (::closesocket(m_sockfd) != 0)
                return socket_last_err();
            m_sockfd = INVALID_SOCKET;
        }
        return 0;
    }

    // is valid socket
    explicit operator bool() const  { return m_sockfd != INVALID_SOCKET; }
    bool valid() const              { return m_sockfd != INVALID_SOCKET; }

    // get native socket handle/descriptor
    SOCKET nativefd() const         { return m_sockfd; }

    // make bind address reusable
    int set_addr_reusable(bool reusable = true)
    {
        int reuse = reusable;
        return this->set_option(SOL_SOCKET, SO_REUSEADDR, &reuse, ssizeof(reuse));
    }

    // bind local address
    int bind(const SocketAddr& addr)
    {
        int res = ::bind(m_sockfd, addr.data(), addr.length());
        return res == 0 ? 0 : socket_last_err();
    }
    int bind(const sockaddr* addr, socklen_t len)
    {
        int res = ::bind(m_sockfd, addr, len);
        return res == 0 ? 0 : socket_last_err();
    }
    int bind(const char* ip, uint16_t port)
    {
        SocketAddr saddr(ip, port);
        return this->bind(saddr);
    }

    // get local address of the socket(if binded)
    int get_local_addr(SocketAddr* paddr) const
    {
        socklen_t len = paddr->maxlen();
        int res = ::getsockname(m_sockfd, paddr->data(), &len);
        return res == 0 ? 0 : socket_last_err();
    }
    // get peer address of the socket(if connected)
    int get_peer_addr(SocketAddr* paddr) const
    {
        socklen_t len = paddr->maxlen();
        int res = ::getpeername(m_sockfd, paddr->data(), &len);
        return res == 0 ? 0 : socket_last_err();
    }

    // get socket option
    int get_option(int level, int name, void* value, socklen_t* length) const
    {
        int res = ::getsockopt(m_sockfd, level, name, (char*)value, length);
        return res == 0 ? 0 : socket_last_err();
    }
    // set socket option
    int set_option(int level, int name, const void* value, socklen_t length) const
    {
        int res = ::setsockopt(m_sockfd, level, name, (const char*)value, length);
        return res == 0 ? 0 : socket_last_err();
    }

    // set recv/send buffer size
    int set_recv_buff_size(int size) const
    {
        return this->set_option(SOL_SOCKET, SO_RCVBUF, &size, ssizeof(int));
    }
    int set_send_buff_size(int size) const
    {
        return this->set_option(SOL_SOCKET, SO_SNDBUF, &size, ssizeof(int));
    }

    // get recv/send buffer size
    int get_recv_buff_size(int* psize) const
    {
        socklen_t len = ssizeof(int);
        return this->get_option(SOL_SOCKET, SO_RCVBUF, psize, &len);
    }
    int get_send_buff_size(int* psize) const
    {
        socklen_t len = ssizeof(int);
        return this->get_option(SOL_SOCKET, SO_SNDBUF, psize, &len);
    }

    BaseSocket(BaseSocket&& other) noexcept
    {
        m_sockfd = other.m_sockfd;
        other.m_sockfd = INVALID_SOCKET;
    }
    BaseSocket& operator=(BaseSocket&& other) noexcept
    {
        if (&other != this)
        {
            if (m_sockfd != INVALID_SOCKET)
                ::closesocket(m_sockfd);
            m_sockfd = other.m_sockfd;
            other.m_sockfd = INVALID_SOCKET;
        }
        return *this;
    }
    void swap(BaseSocket& other) noexcept
    {
        auto tmp = m_sockfd;
        m_sockfd = other.m_sockfd;
        other.m_sockfd = tmp;
    }

protected:
    BaseSocket(const BaseSocket&) = delete;
    BaseSocket& operator=(const BaseSocket&) = delete;
    BaseSocket() : m_sockfd(INVALID_SOCKET) {}
    BaseSocket(SOCKET fd) : m_sockfd(fd) {}
    ~BaseSocket() { this->close(); }
    SOCKET  m_sockfd;
};

//======================================================================================================================

// UDP socket
class UdpSocket : public BaseSocket
{
public:
    UdpSocket() = default;
    explicit UdpSocket(SOCKET fd) : BaseSocket(fd) {}

    // open a udp socket, return 0 if succeeded, return system error code if failed
    int open(int family = PF_INET)
    {
        return BaseSocket::open(family, SOCK_DGRAM, IPPROTO_UDP);
    }

    // send data to the specified address
    SockResult sendto(const SocketAddr& addr, const void* buf, size_t size, int& result, int flags = 0)
    {
        result = (int)::sendto(m_sockfd, (const char*)buf, (int)size, flags, addr.data(), addr.length());
        return result < 0 ? SockResult::make_failed() : SockResult::make_ok();
    }

    // recv data
    SockResult recvfrom(SocketAddr& addr, void* buf, size_t size, int& result, int flags = 0)
    {
        socklen_t len = addr.maxlen();
        result = (int)::recvfrom(m_sockfd, (char*)buf, (int)size, flags, addr.data(), &len);
        return result < 0 ? SockResult::make_failed() : SockResult::make_ok();
    }

    // ditto, but wait at most the specified time(in milliseconds)
    SockResult sendto_wait(const SocketAddr& addr, const void* buf, size_t size, int& result,
        int timeout, int flags = 0);

    // ditto, but wait at most the specified time(in milliseconds)
    SockResult recvfrom_wait(SocketAddr& addr, void* buf, size_t size, int& result,
        int timeout, int flags = 0);
};

// multicast socket
class McastSocket : public UdpSocket
{
public:
    McastSocket() : UdpSocket(), m_family(PF_INET) {}

    // open a multicast socket, no need to be called befor join
    int open(int family = PF_INET)
    {
        assert(family == PF_INET || family == PF_INET6);
        m_family = family;
        return BaseSocket::open(family, SOCK_DGRAM, IPPROTO_UDP);
    }

    // join multicast group
    // port: port for the mcast data
    // ifidx: the index of local interface on which the multicast group should be joined
    // return 0 if succeeded, return system error code if failed
    int join(const IpAddr& grpIP, uint16_t port, int ifidx = 0);
    int join(const char* grpIP, uint16_t port, int ifidx = 0)
    {
        IpAddr groupIP(grpIP);
        return this->join(groupIP, port, ifidx);
    }

    // leave multicast group
    int leave(const IpAddr& grpIP, int ifidx = 0);
    int leave(const char* grpIP, int ifidx = 0)
    {
        IpAddr groupIP(grpIP);
        return this->leave(groupIP, ifidx);
    }

    // ditto, but with source filter
    int join(const IpAddr& grpIP, const IpAddr& srcIP, uint16_t port, int ifidx = 0);
    int join(const char* grpIP, const char* srcIP, uint16_t port, int ifidx = 0)
    {
        IpAddr groupIP(grpIP), sourceIP(srcIP);
        return this->join(groupIP, sourceIP, port, ifidx);
    }
    int leave(const IpAddr& grpIP, const IpAddr& srcIP, int ifidx = 0);
    int leave(const char* grpIP, const char* srcIP, int ifidx = 0)
    {
        IpAddr groupIP(grpIP), sourceIP(srcIP);
        return this->leave(groupIP, sourceIP, ifidx);
    }

    // block/unblock source
    int block(const IpAddr& grpIP, const IpAddr& srcIP, int ifidx = 0);
    int block(const char* grpIP, const char* srcIP, int ifidx = 0)
    {
        IpAddr groupIP(grpIP), sourceIP(srcIP);
        return this->block(groupIP, sourceIP, ifidx);
    }
    int unblock(const IpAddr& grpIP, const IpAddr& srcIP, int ifidx = 0);
    int unblock(const char* grpIP, const char* srcIP, int ifidx = 0)
    {
        IpAddr groupIP(grpIP), sourceIP(srcIP);
        return this->unblock(groupIP, sourceIP, ifidx);
    }

    // enable/disable loopback of outgoing mcast data
    int set_loopback(bool enabled);

    // set TTL of outgoing mcast data
    int set_mcast_ttl(uint32_t ttl);

    // choose interface for outgoing mcast data
    int set_interface(uint32_t ifidx);
private:
    int m_family;
};

//======================================================================================================================

// TCP client socket
class TcpSocket : public BaseSocket
{
public:
    TcpSocket() = default;
    explicit TcpSocket(SOCKET fd) : BaseSocket(fd) {}

    // open a tcp client socket, no need to be called before connecting
    int open(int family = PF_INET)
    {
        return BaseSocket::open(family, SOCK_STREAM, IPPROTO_TCP);
    }

    // connect to server
    int connect(const SocketAddr& addr)
    {
        if (m_sockfd == INVALID_SOCKET)
        {
            int res = this->open(addr.family());
            if (res)
                return res;
        }
        int res = ::connect(m_sockfd, addr.data(), addr.length());
        return res == 0 ? 0 : socket_last_err();
    }
    int connect(const sockaddr* addr, socklen_t len)
    {
        if (m_sockfd == INVALID_SOCKET)
        {
            int res = this->open(addr->sa_family);
            if (res)
                return res;
        }
        int res = ::connect(m_sockfd, addr, len);
        return res == 0 ? 0 : socket_last_err();
    }
    int connect(const char* srvIP, uint16_t port)
    {
        SocketAddr srvAddr(srvIP, port);
        return this->connect(srvAddr);
    }

    // is already connected
    bool connected() const
    {
        struct sockaddr_storage tmp;
        socklen_t len = ssizeof(tmp);
        return (0 == ::getpeername(m_sockfd, (sockaddr*)&tmp, &len));
    }

    // shutdown connected socket
    int shutdown(int how = SHUT_RDWR)
    {
        int res = ::shutdown(m_sockfd, how);
        return res == 0 ? 0 : socket_last_err();
    }

    // send some data, may return before all data sent
    SockResult send(const void* buf, size_t size, int& result, int flags = 0)
    {
        result = (int)::send(m_sockfd, (const char*)buf, (int)size, flags);
        return result < 0 ? SockResult::make_failed() : SockResult::make_ok();
    }

    // recv some data, may return before all data received
    SockResult recv(void* buf, size_t size, int& result, int flags = 0)
    {
        result = (int)::recv(m_sockfd, (char*)buf, (int)size, flags);
        return result < 0 ? SockResult::make_failed() : SockResult::make_ok();
    }

    // ditto, but wait at most the specified time(in milliseconds)
    SockResult send_wait(const void* buf, size_t size, int& result, int timeout, int flags = 0);

    // ditto, but wait at most the specified time(in milliseconds)
    SockResult recv_wait(void* buf, size_t size, int& result, int timeout, int flags = 0);

    // repeatedly send data until all data are sent
    // return the total number of bytes sent
    // return system error code if failed
    SockResult sendall(const void* buf, size_t size, int& result, int flags = 0);

    // repeatedly receive data until all data are received
    // return the total number of bytes received
    // return system error code if failed
    SockResult recvall(void* buf, size_t size, int& result, int flags = 0);
};

// TCP acceptor
class TcpAcceptor : public BaseSocket
{
public:
    // open a tcp acceptor socket, no need to be called before listenning
    int open(int family = PF_INET)
    {
        return BaseSocket::open(family, SOCK_STREAM, IPPROTO_TCP);
    }

    // listen at the specified address
    // return 0 if succeeded, return system error code if failed
    int listen_at(const sockaddr* srvAddr, socklen_t len, int backlog = 10);
    int listen_at(const SocketAddr& srvAddr, int backlog = 10)
    {
        return this->listen_at(srvAddr.data(), srvAddr.length(), backlog);
    }
    int listen_at(const char* srvIP, uint16_t port, int backlog = 10)
    {
        SocketAddr srvAddr(srvIP, port);
        return this->listen_at(srvAddr, backlog);
    }

    // accept an incomming connection
    // return new connection socket, if failed return invalid socket
    TcpSocket accept(int* errc = nullptr)
    {
        SOCKET sockfd = ::accept(m_sockfd, nullptr, nullptr);
        if (errc)
            *errc = (sockfd != INVALID_SOCKET) ? 0 : socket_last_err();
        return TcpSocket{sockfd};
    }
};

} // namespace irk

#endif
