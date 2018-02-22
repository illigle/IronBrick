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

#ifndef _IRONBRICK_IPADDRESS_H_
#define _IRONBRICK_IPADDRESS_H_

#ifdef _WIN32
#include <WS2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif
#include <string.h>
#include <string>
#include "IrkCommon.h"

namespace irk {

//======================================================================================================================
// IPv4 address

class Ipv4Addr
{
public:
    typedef struct in_addr native_type;     // native address type

    explicit Ipv4Addr() : m_addr() {}
    explicit Ipv4Addr(native_type addr) : m_addr(addr) {}
    explicit Ipv4Addr(const char* addr)     { this->from_string(addr); }
    // construct from numeric address in host byte order
    explicit Ipv4Addr(uint32_t addr)        { m_addr.s_addr = htonl(addr); }

    // convert from/to native type
    void from_native(native_type addr)      { m_addr = addr; }
    native_type to_native() const           { return m_addr; }

    // convert from/to string in dotted decimal form
    bool from_string(const char* addr);
    bool to_string(char* buf, size_t size) const;   // out buffer size should >= 16
    std::string to_string() const;                  // return empty string if failed

    // convert from/to 32 bits numeric address in host byte order
    void from_numeric(uint32_t addr)        { m_addr.s_addr = htonl(addr); }
    uint32_t to_numeric() const             { return ntohl(m_addr.s_addr); }

    // check special IPv4 address
    bool is_unspecified() const             { return m_addr.s_addr == 0; }
    bool is_loopback() const                { return m_addr.s_addr == htonl(INADDR_LOOPBACK); }
    bool is_multicast() const               { return IN_MULTICAST(to_numeric()); }

    // create special IPv4 address
    static Ipv4Addr make_unspecified()      { return Ipv4Addr{}; }
    static Ipv4Addr make_loopback()         { return Ipv4Addr{(uint32_t)(INADDR_LOOPBACK)}; }

private:
    native_type m_addr;
};

inline bool operator==(const Ipv4Addr& a, const Ipv4Addr& b)
{
    return a.to_numeric() == b.to_numeric();
}
inline bool operator!=(const Ipv4Addr& a, const Ipv4Addr& b)
{
    return a.to_numeric() != b.to_numeric();
}
inline bool operator<=(const Ipv4Addr& a, const Ipv4Addr& b)
{
    return a.to_numeric() <= b.to_numeric();
}
inline bool operator<(const Ipv4Addr& a, const Ipv4Addr& b)
{
    return a.to_numeric() < b.to_numeric();
}
inline bool operator>=(const Ipv4Addr& a, const Ipv4Addr& b)
{
    return a.to_numeric() >= b.to_numeric();
}
inline bool operator>(const Ipv4Addr& a, const Ipv4Addr& b)
{
    return a.to_numeric() > b.to_numeric();
}

//======================================================================================================================
// IPv6 address

class Ipv6Addr
{
public:
    typedef struct in6_addr native_type;

    explicit Ipv6Addr() : m_addr() {}
    explicit Ipv6Addr(const native_type& addr) : m_addr(addr) {}
    explicit Ipv6Addr(const char* addr) { this->from_string(addr); }

    // convert from/to native type
    void from_native(const native_type& addr)   { m_addr = addr; }
    native_type to_native() const               { return m_addr; }

    // convert from/to IPv6 address string
    bool from_string(const char* addr);
    bool to_string(char* buf, size_t size) const; // out buffer size should >= 64
    std::string to_string() const;                  // return empty string if failed

    // check special IPv6 address
    bool is_unspecified() const             { return IN6_IS_ADDR_UNSPECIFIED(&m_addr) != 0; }
    bool is_loopback() const                { return IN6_IS_ADDR_LOOPBACK(&m_addr) != 0; }
    bool is_multicast() const               { return IN6_IS_ADDR_MULTICAST(&m_addr) != 0; }
    bool is_link_local() const              { return IN6_IS_ADDR_LINKLOCAL(&m_addr) != 0; }
    bool is_site_local() const              { return IN6_IS_ADDR_SITELOCAL(&m_addr) != 0; }
    bool is_ipv4_mapped() const             { return IN6_IS_ADDR_V4MAPPED(&m_addr) != 0; }
    bool is_ipv4_compatible() const         { return IN6_IS_ADDR_V4COMPAT(&m_addr) != 0; }

    // create special IPv6 address
    static Ipv6Addr make_unspecified()      { return Ipv6Addr{}; }
    static Ipv6Addr make_loopback()         { return Ipv6Addr{::in6addr_loopback}; }

    friend bool operator==(const Ipv6Addr& a, const Ipv6Addr& b);
    friend bool operator<=(const Ipv6Addr& a, const Ipv6Addr& b);
    friend bool operator<(const Ipv6Addr& a, const Ipv6Addr& b);
private:
    native_type m_addr;
};

inline bool operator==(const Ipv6Addr& a, const Ipv6Addr& b)
{
    return IN6_ARE_ADDR_EQUAL(&a.m_addr, &b.m_addr) != 0;
}
inline bool operator<=(const Ipv6Addr& a, const Ipv6Addr& b)
{
    return memcmp(&a.m_addr, &b.m_addr, sizeof(a.m_addr)) <= 0;
}
inline bool operator<(const Ipv6Addr& a, const Ipv6Addr& b)
{
    return memcmp(&a.m_addr, &b.m_addr, sizeof(a.m_addr)) < 0;
}
inline bool operator!=(const Ipv6Addr& a, const Ipv6Addr& b)
{
    return !(a == b);
}
inline bool operator>=(const Ipv6Addr& a, const Ipv6Addr& b)
{
    return !(a < b);
}
inline bool operator>(const Ipv6Addr& a, const Ipv6Addr& b)
{
    return !(a <= b);
}

//======================================================================================================================
// general IP address

class IpAddr
{
public:
    IpAddr() : m_family(AF_INET), m_addr() {}    // default to unspecified/any IPv4 address
    explicit IpAddr(const char* addr) { this->from_string(addr); }
    explicit IpAddr(const Ipv4Addr& addr) : m_family(AF_INET), m_addr(addr) {}
    explicit IpAddr(const Ipv6Addr& addr) : m_family(AF_INET6), m_addr(addr) {}
    explicit IpAddr(const Ipv4Addr::native_type& addr) : m_family(AF_INET), m_addr(addr) {}
    explicit IpAddr(const Ipv6Addr::native_type& addr) : m_family(AF_INET6), m_addr(addr) {}
    IpAddr(const IpAddr&) = default;
    IpAddr& operator=(const IpAddr&) = default;

    int family() const              { return m_family; }
    bool is_ipv4() const            { return m_family == AF_INET; }
    bool is_ipv6() const            { return m_family == AF_INET6; }
    Ipv4Addr& as_ipv4()             { assert(is_ipv4()); return m_addr.v4; }
    Ipv6Addr& as_ipv6()             { assert(is_ipv6()); return m_addr.v6; }
    const Ipv4Addr& as_ipv4() const { assert(is_ipv4()); return m_addr.v4; }
    const Ipv6Addr& as_ipv6() const { assert(is_ipv6()); return m_addr.v6; }

    IpAddr& operator=(const Ipv4Addr& addr)
    {
        m_family = AF_INET;
        m_addr.v4 = addr;
        return *this;
    }
    IpAddr& operator=(const Ipv6Addr& addr)
    {
        m_family = AF_INET6;
        m_addr.v6 = addr;
        return *this;
    }
    IpAddr& operator=(const Ipv4Addr::native_type& addr)
    {
        m_family = AF_INET;
        m_addr.v4.from_native(addr);
        return *this;
    }
    IpAddr& operator=(const Ipv6Addr::native_type& addr)
    {
        m_family = AF_INET6;
        m_addr.v6.from_native(addr);
        return *this;
    }

    // convert from/to address string
    bool from_string(const char* addr);
    bool to_string(char* buf, size_t size) const;   // output buffer size should >= 64
    std::string to_string() const;                  // return empty string if failed

     // check special address
    bool is_unspecified() const { return is_ipv4() ? m_addr.v4.is_unspecified() : m_addr.v6.is_unspecified(); }
    bool is_loopback() const    { return is_ipv4() ? m_addr.v4.is_loopback() : m_addr.v6.is_loopback(); }
    bool is_multicast() const   { return is_ipv4() ? m_addr.v4.is_multicast() : m_addr.v6.is_multicast(); }

    friend bool operator==(const IpAddr& a, const IpAddr& b);
    friend bool operator<(const IpAddr& a, const IpAddr& b);
private:
    union AddrUnion
    {
        AddrUnion() : v4() {}
        explicit AddrUnion(Ipv4Addr addr) : v4(addr) {}
        explicit AddrUnion(Ipv6Addr addr) : v6(addr) {}
        explicit AddrUnion(const Ipv4Addr::native_type& addr) : v4(addr) {}
        explicit AddrUnion(const Ipv6Addr::native_type& addr) : v6(addr) {}
        Ipv4Addr v4;
        Ipv6Addr v6;
    };
    int         m_family;
    AddrUnion   m_addr;
};

inline bool operator==(const IpAddr& a, const IpAddr& b)
{
    if (a.m_family != b.m_family)
        return false;
    if (a.m_family == AF_INET)
        return a.m_addr.v4 == b.m_addr.v4;
    return a.m_addr.v6 == b.m_addr.v6;
}
inline bool operator<(const IpAddr& a, const IpAddr& b)
{
    if (a.m_family != b.m_family)
        return a.m_family < b.m_family;
    if (a.m_family == AF_INET)
        return a.m_addr.v4 < b.m_addr.v4;
    return a.m_addr.v6 < b.m_addr.v6;
}
inline bool operator!=(const IpAddr& a, const IpAddr& b)
{
    return !(a == b);
}
inline bool operator>(const IpAddr& a, const IpAddr& b)
{
    return !(a == b) && !(a < b);
}
inline bool operator<=(const IpAddr& a, const IpAddr& b)
{
    return (a == b) || (a < b);
}
inline bool operator>=(const IpAddr& a, const IpAddr& b)
{
    return !(a < b);
}

//======================================================================================================================
// socket address = ip address + port

class SocketAddr
{
public:
    explicit SocketAddr() : m_sa()  // default to unspecified/any IPv4 address and port
    {
        m_sa.v4.sin_family = AF_INET;
        m_sa.v4.sin_port = 0;
    }
    explicit SocketAddr(const sockaddr_in& saddr) : m_sa()
    {
        m_sa.v4 = saddr;
    }
    explicit SocketAddr(const sockaddr_in6& saddr) : m_sa()
    {
        m_sa.v6 = saddr;
    }
    SocketAddr(int family, uint16_t port) : m_sa()
    {
        if (family == AF_INET)
        {
            m_sa.v4.sin_family = family;
            m_sa.v4.sin_port = htons(port);
        }
        else
        {
            m_sa.v6.sin6_family = AF_INET6;
            m_sa.v6.sin6_port = htons(port);
        }
    }
    SocketAddr(const sockaddr* saddr, socklen_t len) : m_sa()
    {
        this->set(saddr, len);
    }
    SocketAddr(const char* addr, uint16_t port) : m_sa()
    {
        this->set(addr, port);
    }
    SocketAddr(const Ipv4Addr& addr, uint16_t port) : m_sa()
    {
        this->set(addr, port);
    }
    SocketAddr(const Ipv6Addr& addr, uint16_t port) : m_sa()
    {
        this->set(addr, port);
    }
    SocketAddr(const IpAddr& addr, uint16_t port) : m_sa()
    {
        this->set(addr, port);
    }

    int family() const              { return m_sa.base.sa_family; }
    bool is_ipv4() const            { return m_sa.base.sa_family == AF_INET; }
    bool is_ipv6() const            { return m_sa.base.sa_family == AF_INET6; }

    // get pointer to native sockaddr
    sockaddr* data()                { return &m_sa.base; }
    const sockaddr* data() const    { return &m_sa.base; }

    // current socket address length
    socklen_t length() const        { return static_cast<socklen_t>(is_ipv4() ? sizeof(m_sa.v4) : sizeof(m_sa.v6)); }
    // max socket address length
    socklen_t maxlen() const        { return static_cast<socklen_t>(sizeof(m_sa)); }

    // set address and port
    bool set(const char* addr, uint16_t port);
    void set(const Ipv4Addr& addr, uint16_t port)
    {
        m_sa.v4.sin_family = AF_INET;
        m_sa.v4.sin_addr = addr.to_native();
        m_sa.v4.sin_port = htons(port);
    }
    void set(const Ipv6Addr& addr, uint16_t port)
    {
        m_sa.v6.sin6_family = AF_INET6;
        m_sa.v6.sin6_addr = addr.to_native();
        m_sa.v6.sin6_port = htons(port);
    }
    void set(const IpAddr& addr, uint16_t port)
    {
        if (addr.is_ipv4())
            this->set(addr.as_ipv4(), port);
        else
            this->set(addr.as_ipv6(), port);
    }
    void set(const sockaddr_in& saddr)
    {
        m_sa.v4 = saddr;
    }
    void set(const sockaddr_in6& saddr)
    {
        m_sa.v6 = saddr;
    }
    void set(const sockaddr* saddr, socklen_t len)
    {
        assert(len <= maxlen());
        memcpy(&m_sa.base, saddr, len);
    }

    // get IP address
    Ipv4Addr addressv4() const      { assert(is_ipv4()); return Ipv4Addr{m_sa.v4.sin_addr}; }
    Ipv6Addr addressv6() const      { assert(is_ipv6()); return Ipv6Addr{m_sa.v6.sin6_addr}; }
    IpAddr address() const          { return is_ipv4() ? IpAddr{m_sa.v4.sin_addr} : IpAddr{m_sa.v6.sin6_addr}; }

    // get/set port
    void set_port(uint16_t port)
    {
        if (m_sa.base.sa_family == AF_INET)
            m_sa.v4.sin_port = htons(port);
        else
            m_sa.v6.sin6_port = htons(port);
    }
    uint16_t port() const           { return is_ipv4() ? ntohs(m_sa.v4.sin_port) : ntohs(m_sa.v6.sin6_port); }

    // get/set scope id
    void set_scope_id(uint32_t sid) { m_sa.v6.sin6_scope_id = sid; }
    uint32_t scope_id() const       { return m_sa.v6.sin6_scope_id; }

    // is contains unspecified address and port
    bool is_unspecified() const
    {
        if (m_sa.base.sa_family == AF_INET)
            return (m_sa.v4.sin_addr.s_addr == 0) && (m_sa.v4.sin_port == 0);
        else
            return (IN6_IS_ADDR_UNSPECIFIED(&m_sa.v6.sin6_addr) != 0) && (m_sa.v6.sin6_port == 0);
    }

    bool to_string(char* buf, size_t size) const;   // out buffer size should >= 65
    std::string to_string() const;                  // return empty string if failed

    friend bool operator==(const SocketAddr& a, const SocketAddr& b);
    friend bool operator<(const SocketAddr& a, const SocketAddr& b);
private:
    union
    {
        struct sockaddr base;
        struct sockaddr_in v4;
        struct sockaddr_in6 v6;
        struct sockaddr_storage storage;
    }
    m_sa;
};

inline bool operator==(const SocketAddr& a, const SocketAddr& b)
{
    if (a.m_sa.base.sa_family != b.m_sa.base.sa_family)
        return false;
    if (a.is_ipv4())
        return (a.m_sa.v4.sin_addr.s_addr == b.m_sa.v4.sin_addr.s_addr
            && a.m_sa.v4.sin_port == b.m_sa.v4.sin_port);
    else
        return ((IN6_ARE_ADDR_EQUAL(&a.m_sa.v6.sin6_addr, &b.m_sa.v6.sin6_addr) != 0)
            && a.m_sa.v6.sin6_port == b.m_sa.v6.sin6_port);
}
inline bool operator<(const SocketAddr& a, const SocketAddr& b)
{
    auto a1 = a.address(), b1 = b.address();
    if (a1 != b1)
        return a1 < b1;
    return a.port() < b.port();
}
inline bool operator!=(const SocketAddr& a, const SocketAddr& b)
{
    return !(a == b);
}
inline bool operator>(const SocketAddr& a, const SocketAddr& b)
{
    return !(a == b) && !(a < b);
}
inline bool operator<=(const SocketAddr& a, const SocketAddr& b)
{
    return (a == b) || (a < b);
}
inline bool operator>=(const SocketAddr& a, const SocketAddr& b)
{
    return !(a < b);
}

} // namespace irk

#endif
