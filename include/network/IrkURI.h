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

#ifndef _IRONBRICK_URI_H_
#define _IRONBRICK_URI_H_

#include <string>
#include "IrkCommon.h"

namespace irk {

//
// NOTE: all character encoding is UTF-8
//

// Simple URI utility, according RFC-3986
class Uri
{
public:
    Uri() : m_port(0) {}

    // clear uri components
    void clear();

    // split plain URI(not percent-encoded), return false if failed
    bool set_plain_uri(const char* uri);

    // get plain URI, return empty string if any component is invalid
    std::string plain_uri() const;

    // split and decode escaped(percent-encoded) URI, return false if failed
    bool set_escaped_uri(const char* uri);

    // get escaped URI, return empty string if any component is invalid
    std::string escaped_uri() const;

    // get escaped authority("userinfo@host:port"), used to connect server
    std::string escaped_server() const;

    // get escaped resouce("path?query#fragment"), used to refer an entity
    std::string escaped_resource() const;

    // get/set scheme, case-insensitive
    const std::string& scheme() const   { return m_scheme; }
    void set_scheme(const char* scheme) { m_scheme = scheme; }

    // get/set host, case-insensitive
    const std::string& host() const     { return m_host; }
    void set_host(const char* host)     { m_host = host; }

    // get/set port, 0 means default port
    int port() const                    { return m_port; }
    void set_port(int port)             { m_port = port; }

    // get/set user information, e.g. usrname:password
    const std::string& userinfo() const { return m_userinfo; }
    void set_userinfo(const char* info) { m_userinfo = info; }

    // get/set path
    const std::string& path() const     { return m_path; }
    void set_path(const char* path)     { m_path = path; }

    // get/set query
    const std::string& query() const    { return m_query; }
    void set_query(const char* query)   { m_query = query; }

    // get/set fragment
    const std::string& fragment() const { return m_fragment; }
    void set_fragment(const char* frag) { m_fragment = frag; }

private:
    std::string m_scheme;           // case-insensitive
    std::string m_host;             // case-insensitive
    int         m_port;
    std::string m_userinfo;
    std::string m_path;
    std::string m_query;
    std::string m_fragment;
};

inline bool operator==(const Uri& lhs, const Uri& rhs)
{
    return lhs.escaped_uri() == rhs.escaped_uri();
}
inline bool operator!=(const Uri& lhs, const Uri& rhs)
{
    return !(lhs == rhs);
}

// is standard(RFC-3986) compliant escapeded URI
bool is_valid_escaped_uri(const char* uri);

} // namespace irk
#endif
