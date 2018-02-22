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
#include <Ws2tcpip.h>       // inet_pton
#else
#include <arpa/inet.h>
#endif
#include "IrkURI.h"
#include "IrkStringUtility.h"

using std::string;

#define CT_ALPHA        0x1     // a-zA-Z
#define CT_DIGIT        0x2     // 0-9
#define CT_SCHEME       0x4     // -.+
#define CT_SAFEPUNC     0x8     // -._~
#define CT_GENDELIM     0x10    // :/?#[]@
#define CT_SUBDELIM     0x20    // !$&'()*+,;=
#define CT_PWD          0x40    // :@

#define CT_ALNUM        (CT_ALPHA | CT_DIGIT)
#define CT_UNRESV       (CT_ALPHA | CT_DIGIT | CT_SAFEPUNC)
#define CT_PCHAR        (CT_UNRESV | CT_SUBDELIM | CT_PWD)

// URI charactor types, q.v. RFC-3986
static const uint8_t s_UriCtype[256] =
{
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x20, 0x00, 0x10, 0x20, 0x00, 0x20, 0x20, 0x20, 0x20, 0x20, 0x24, 0x20, 0x0c, 0x0c, 0x10,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x50, 0x20, 0x00, 0x20, 0x00, 0x10,
    0x50, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x10, 0x00, 0x10, 0x00, 0x08,
    0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x08, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// charcter type
#define URI_CT(ch)          (s_UriCtype[(uint8_t)(ch)])
// is alpha
#define URI_IS_ALPHA(ch)    ((s_UriCtype[(uint8_t)(ch)] & CT_ALPHA) != 0)
// is 10-base digit
#define URI_IS_DIGIT(ch)    ((s_UriCtype[(uint8_t)(ch)] & CT_DIGIT) != 0)
// is alpha or number
#define URI_IS_ALNUM(ch)    ((s_UriCtype[(uint8_t)(ch)] & CT_ALNUM) != 0)
// is unreserved URI character
#define URI_IS_UNRESV(ch)   ((s_UriCtype[(uint8_t)(ch)] & CT_UNRESV) != 0)
// is valid pchar
#define URI_IS_PCHAR(ch)    ((s_UriCtype[(uint8_t)(ch)] & CT_PCHAR) != 0)
// is valid scheme character
#define URI_IS_SCHEME(ch)   ((s_UriCtype[(uint8_t)(ch)] & (CT_ALPHA | CT_DIGIT | CT_SCHEME)) != 0)

// escape(percent encode) char, add to tail of out string
static inline void escape(int chr, string& out)
{
    static const char s_HexChar[] = "0123456789ABCDEF";
    out.push_back('%');
    out.push_back(s_HexChar[(uint8_t)chr >> 4]);
    out.push_back(s_HexChar[(uint8_t)chr & 0xF]);
}

// unescape char, return -1 if failed
static inline int unescape(const char* src)
{
    assert(src[0] == '%');
    int chr = 0;

    if (src[1] >= '0' && src[1] <= '9')
        chr = 16 * (src[1] - '0');
    else if (src[1] >= 'A' && src[1] <= 'F')
        chr = 16 * (src[1] - 'A' + 10);
    else if (src[1] >= 'a' && src[1] <= 'f')
        chr = 16 * (src[1] - 'a' + 10);
    else
        return -1;

    if (src[2] >= '0' && src[2] <= '9')
        chr += (src[2] - '0');
    else if (src[2] >= 'A' && src[2] <= 'F')
        chr += (src[2] - 'A' + 10);
    else if (src[2] >= 'a' && src[2] <= 'f')
        chr += (src[2] - 'a' + 10);
    else
        return -1;

    return chr;
}

// unescape string, return false if failed
static bool unescape_str(const char* str, size_t len, string& out)
{
    size_t i = 0;
    while (i < len)
    {
        if (str[i] == '%')
        {
            if (len < i + 3)
                break;
            int chr = unescape(str + i);
            if (chr < 0)
                break;
            out.push_back((char)chr);
            i += 3;
        }
        else
        {
            out.push_back(str[i]);
            i++;
        }
    }

    if (i < len)   // failed
    {
        out.clear();
        return false;
    }
    return true;
}

// unescape null-terminated string, return false if failed
static bool unescape_str(const char* str, string& out)
{
    const char* s = str;
    while (*s)
    {
        if (*s == '%')
        {
            int chr = unescape(s);
            if (chr < 0)
            {
                out.clear();
                return false;
            }
            out.push_back((char)chr);
            s += 3;
        }
        else
        {
            out.push_back(*s);
            s++;
        }
    }

    return true;
}

//======================================================================================================================
namespace irk {

// clear uri
void Uri::clear()
{
    m_scheme.clear();
    m_host.clear();
    m_port = 0;
    m_userinfo.clear();
    m_path.clear();
    m_query.clear();
    m_fragment.clear();
}

// split URI(not percent-encoded)
bool Uri::set_plain_uri(const char* uri)
{
    this->clear();

    // get scheme
    const char* cur = strchr(uri, ':');
    if (!cur || cur == uri)
    {
        return false;
    }
    m_scheme.assign(uri, cur);
    for (char ch : m_scheme)
    {
        if (!URI_IS_SCHEME(ch))
        {
            m_scheme.clear();
            return false;
        }
    }

    // check authority
    if (cur[1] == '/' && cur[2] == '/')
    {
        cur += 3;
        const char* end = strpbrk(cur, "@/?#");

        // check userinfo
        if (end && *end == '@')
        {
            m_userinfo.assign(cur, end);
            cur = end + 1;
            end = strpbrk(cur, "/?#");    // search the end of authority component
        }

        // get host
        if (end)
            m_host.assign(cur, end);
        else
            m_host.assign(cur);
        cur = end;

        // get port(if exists)
        for (int i = (int)m_host.length() - 1; i >= 0; i--)
        {
            if (!URI_IS_DIGIT(m_host[i]))
            {
                if (m_host[i] == ':')
                {
                    m_port = str_to_int(m_host.c_str() + i + 1);
                    m_host.erase(i, string::npos);
                }
                break;
            }
        }
    }

    // unescape path
    if (cur && *cur != '?' && *cur != '#')
    {
        const char* end = strpbrk(cur, "?#");
        if (end)
            m_path.assign(cur, end);
        else
            m_path.assign(cur);
        cur = end;
    }

    // unescape query
    if (cur && *cur == '?')
    {
        const char* end = strchr(++cur, '#');
        if (end)
            m_query.assign(cur, end);
        else
            m_query.assign(cur);
        cur = end;
    }

    // unescape fragment
    if (cur && *cur == '#')
    {
        m_fragment.assign(++cur);
    }

    return true;
}

// get plain URI, return empty string if any component is invalid
std::string Uri::plain_uri() const
{
    std::string outUri;

    // scheme
    if (m_scheme.empty() || !URI_IS_ALPHA(m_scheme[0])) // first character must be alpha
    {
        return std::string();
    }
    for (char ch : m_scheme)
    {
        if (!URI_IS_SCHEME(ch))    // invalid character
            return std::string();

        if (ch >= 'A' && ch <= 'Z')
            outUri.push_back((char)(ch | 0x20));  // to lowercase
        else
            outUri.push_back(ch);
    }
    outUri.push_back(':');

    // make authority component
    if (!m_host.empty())
    {
        outUri += "//";

        // userinfo
        if (!m_userinfo.empty())
        {
            outUri += m_userinfo;
            outUri.push_back('@');
        }

        // host
        for (char ch : m_host)
        {
            if (ch >= 'A' && ch <= 'Z')
                outUri.push_back((char)(ch | 0x20));  // to lowercase
            else
                outUri.push_back(ch);
        }

        // port
        if (m_port != 0)
        {
            IntToStrCvt<char> i2str;
            outUri.push_back(':');
            outUri += i2str(m_port);
        }

        // If a URI contains an authority component, 
        // then the path component must either be empty or begin with a "/" character
        if (!m_path.empty() && m_path[0] != '/')
        {
            outUri.push_back('/');
        }
    }
    else
    {
        // If a URI does not contain an authority component, then the path cannot begin with "//"
        if (m_path.compare(0, 2, "//") == 0)
            return std::string();
    }

    // path
    outUri += m_path;

    // query
    if (!m_query.empty())
    {
        outUri.push_back('?');
        outUri += m_query;
    }

    // fragment
    if (!m_fragment.empty())
    {
        outUri.push_back('#');
        outUri += m_fragment;
    }

    return outUri;
}

// split and decode escaped(percent-encoded) URI
bool Uri::set_escaped_uri(const char* uri)
{
    this->clear();

    // get scheme
    const char* cur = strchr(uri, ':');
    if (!cur || cur == uri)
    {
        return false;
    }
    m_scheme.assign(uri, cur);
    for (char ch : m_scheme)
    {
        if (!URI_IS_SCHEME(ch))
        {
            m_scheme.clear();
            return false;
        }
    }

    // check authority
    if (cur[1] == '/' && cur[2] == '/')
    {
        cur += 3;
        const char* end = strpbrk(cur, "@/?#");

        // check userinfo
        if (end && *end == '@')
        {
            if (!unescape_str(cur, (size_t)(end - cur), m_userinfo))
                return false;
            cur = end + 1;
            end = strpbrk(cur, "/?#");    // search the end of authority component
        }

        // get host
        bool success = (end != nullptr) ?
            unescape_str(cur, (size_t)(end - cur), m_host) :
            unescape_str(cur, m_host);
        if (!success)
        {
            this->clear();
            return false;
        }
        cur = end;

        // get port(if exists)
        for (int i = (int)m_host.length() - 1; i >= 0; i--)
        {
            if (!URI_IS_DIGIT(m_host[i]))
            {
                if (m_host[i] == ':')
                {
                    m_port = str_to_int(m_host.c_str() + i + 1);
                    m_host.erase(i, string::npos);
                }
                break;
            }
        }
    }

    // unescape path
    if (cur && *cur != '?' && *cur != '#')
    {
        const char* end = strpbrk(cur, "?#");
        bool success = (end != nullptr) ?
            unescape_str(cur, (size_t)(end - cur), m_path) :
            unescape_str(cur, m_path);
        if (!success)
        {
            this->clear();
            return false;
        }
        cur = end;
    }

    // unescape query
    if (cur && *cur == '?')
    {
        const char* end = strchr(++cur, '#');
        bool success = (end != nullptr) ?
            unescape_str(cur, (size_t)(end - cur), m_query) :
            unescape_str(cur, m_query);
        if (!success)
        {
            this->clear();
            return false;
        }
        cur = end;
    }

    // unescape fragment
    if (cur && *cur == '#')
    {
        if (!unescape_str(++cur, m_fragment))
        {
            this->clear();
            return false;
        }
    }

    return true;
}

// get escaped(percent-encoded) URI, return empty string if error occur
std::string Uri::escaped_uri() const
{
    // scheme
    if (m_scheme.empty() || !URI_IS_ALPHA(m_scheme[0])) // first character must be alpha
    {
        return std::string();
    }
    for (char ch : m_scheme)
    {
        if (!URI_IS_SCHEME(ch))    // invalid character
            return std::string();
    }
    std::string outUri = get_lowered(m_scheme.c_str());   // to lower-case
    outUri.push_back(':');

    // make authority component
    if (!m_host.empty())
    {
        outUri += "//";

        // escape userinfo
        if (!m_userinfo.empty())
        {
            for (char ch : m_userinfo)
            {
                if ((URI_CT(ch) & (CT_UNRESV | CT_SUBDELIM)) || ch == ':')
                    outUri.push_back(ch);
                else
                    escape(ch, outUri);
            }
            outUri.push_back('@');
        }

        // escape host
        if ((m_host[0] == '[') && (m_host.back() == ']'))  // IPv6 address
        {
            outUri += m_host;
        }
        else
        {
            for (char ch : m_host)
            {
                if (URI_CT(ch) & (CT_UNRESV | CT_SUBDELIM))
                {
                    if (ch >= 'A' && ch <= 'Z')
                        outUri.push_back((char)(ch | 0x20));  // to lower-case
                    else
                        outUri.push_back(ch);
                }
                else
                {
                    escape(ch, outUri);
                }
            }
        }

        // add port
        if (m_port != 0)
        {
            IntToStrCvt<char> i2str;
            outUri.push_back(':');
            outUri += i2str(m_port);
        }

        // If a URI contains an authority component, 
        // then the path component must either be empty or begin with a "/" character
        if (!m_path.empty() && m_path[0] != '/')
        {
            outUri.push_back('/');
        }
    }
    else
    {
        // If a URI does not contain an authority component, then the path cannot begin with "//"
        if (m_path.compare(0, 2, "//") == 0)
            return std::string();
    }

    // escape path
    for (char ch : m_path)
    {
        if (URI_IS_PCHAR(ch) || ch == '/')
            outUri.push_back(ch);
        else
            escape(ch, outUri);
    }

    // escape query
    if (!m_query.empty())
    {
        outUri.push_back('?');

        for (char ch : m_query)
        {
            if (URI_IS_PCHAR(ch) || ch == '/' || ch == '?')
                outUri.push_back(ch);
            else
                escape(ch, outUri);
        }
    }

    // escape fragment
    if (!m_fragment.empty())
    {
        outUri.push_back('#');

        for (char ch : m_fragment)
        {
            if (URI_IS_PCHAR(ch) || ch == '/' || ch == '?')
                outUri.push_back(ch);
            else
                escape(ch, outUri);
        }
    }

    return outUri;
}

// get escaped "userinfo@host", used to login server
std::string Uri::escaped_server() const
{
    std::string strSrv;

    if (!m_host.empty())
    {
        // escape userinfo
        if (!m_userinfo.empty())
        {
            for (char ch : m_userinfo)
            {
                if ((URI_CT(ch) & (CT_UNRESV | CT_SUBDELIM)) || ch == ':')
                    strSrv.push_back(ch);
                else
                    escape(ch, strSrv);
            }
            strSrv.push_back('@');
        }

        // escape host
        if ((m_host[0] == '[') && (m_host.back() == ']'))  // IPv6 address
        {
            strSrv += m_host;
        }
        else
        {
            for (char ch : m_host)
            {
                if (URI_CT(ch) & (CT_UNRESV | CT_SUBDELIM))
                {
                    if (ch >= 'A' && ch <= 'Z')
                        strSrv.push_back((char)(ch | 0x20));  // to lowercase
                    else
                        strSrv.push_back(ch);
                }
                else
                {
                    escape(ch, strSrv);
                }
            }
        }

        // add port
        if (m_port != 0)
        {
            IntToStrCvt<char> i2str;
            strSrv.push_back(':');
            strSrv += i2str(m_port);
        }
    }

    return strSrv;
}

// get escaped "path?query#fragment", used to refer resource
std::string Uri::escaped_resource() const
{
    std::string strRes;

    // escape path
    for (char ch : m_path)
    {
        if (URI_IS_PCHAR(ch) || ch == '/')
            strRes.push_back(ch);
        else
            escape(ch, strRes);
    }

    // escape query
    if (!m_query.empty())
    {
        strRes.push_back('?');

        for (char ch : m_query)
        {
            if (URI_IS_PCHAR(ch) || ch == '/' || ch == '?')
                strRes.push_back(ch);
            else
                escape(ch, strRes);
        }
    }

    // escape fragment
    if (!m_fragment.empty())
    {
        strRes.push_back('#');

        for (char ch : m_fragment)
        {
            if (URI_IS_PCHAR(ch) || ch == '/' || ch == '?')
                strRes.push_back(ch);
            else
                escape(ch, strRes);
        }
    }

    return strRes;
}

//======================================================================================================================

// remove escaped characters, return false if failed
static bool remove_escaped_chars(const char* str, size_t len, string& out)
{
    out.clear();

    for (size_t i = 0; i < len; )
    {
        if (str[i] == '%')
        {
            if ((len < i + 3) || (unescape(str + i) < 0))
                return false;
            i += 3;
        }
        else
        {
            out.push_back(str[i]);
            i++;
        }
    }
    return true;
}

// remove escaped characters, return false if failed
static bool remove_escaped_chars(const char* str, string& out)
{
    out.clear();

    for (const char* s = str; *s != 0; )
    {
        if (*s == '%')
        {
            if (unescape(s) < 0)
                return false;
            s += 3;
        }
        else
        {
            out.push_back(*s);
            s++;
        }
    }

    return true;
}

// is standard compliant escapeded URI
bool is_valid_escaped_uri(const char* uri)
{
    std::string temp;

    // check scheme
    const char* cur = strchr(uri, ':');
    if (!cur || cur == uri)
        return false;
    for (const char* s = uri; s != cur; s++)
    {
        if (!URI_IS_SCHEME(*s))
            return false;
    }

    // check authority
    if (cur[1] == '/' && cur[2] == '/')
    {
        cur += 3;
        const char* end = strpbrk(cur, "@/?#");

        // check userinfo
        if (end && *end == '@')
        {
            if (!remove_escaped_chars(cur, (size_t)(end - cur), temp))
            {
                return false;
            }
            for (char ch : temp)
            {
                if ((URI_CT(ch) & (CT_UNRESV | CT_SUBDELIM)) == 0 && ch != ':')
                    return false;
            }

            // search the end of authority component
            cur = end + 1;
            end = strpbrk(cur, "/?#");
        }

        // get host
        if (end)
        {
            if (!remove_escaped_chars(cur, (size_t)(end - cur), temp))
                return false;
        }
        else
        {
            if (!remove_escaped_chars(cur, temp))
                return false;
        }
        cur = end;

        // check port(if exists)
        for (int i = (int)temp.length() - 1; i >= 0; i--)
        {
            if (!URI_IS_DIGIT(temp[i]))
            {
                if (temp[i] == ':')
                {
                    int port = str_to_int(temp.c_str() + i + 1);
                    if (port > 65535)
                        return false;
                    temp.erase(i, string::npos);
                }
                break;
            }
        }

        // check host
        if (!temp.empty() && temp[0] == '[')   // Ipv6 address
        {
            if (temp.back() != ']')
                return false;
            temp.pop_back();
            struct in6_addr addr;
            if (inet_pton(AF_INET6, temp.c_str() + 1, (void*)&addr) == 0)
                return false;
        }
        else
        {
            for (char ch : temp)
            {
                if ((URI_CT(ch) & (CT_UNRESV | CT_SUBDELIM)) == 0)
                    return false;
            }
        }
    }

    // check path
    if (cur && *cur != '?' && *cur != '#')
    {
        const char* end = strpbrk(cur, "?#");
        if (end)
        {
            if (!remove_escaped_chars(cur, (size_t)(end - cur), temp))
                return false;
        }
        else
        {
            if (!remove_escaped_chars(cur, temp))
                return false;
        }
        for (char ch : temp)
        {
            if (!URI_IS_PCHAR(ch) && ch != '/')
                return false;
        }
        cur = end;
    }

    // check query
    if (cur && *cur == '?')
    {
        const char* end = strchr(++cur, '#');
        if (end)
        {
            if (!remove_escaped_chars(cur, (size_t)(end - cur), temp))
                return false;
        }
        else
        {
            if (!remove_escaped_chars(cur, temp))
                return false;
        }
        for (char ch : temp)
        {
            if (!URI_IS_PCHAR(ch) && ch != '/' && ch != '?')
                return false;
        }
        cur = end;
    }

    // check fragment
    if (cur && *cur == '#')
    {
        if (!remove_escaped_chars(++cur, temp))
        {
            return false;
        }
        for (char ch : temp)
        {
            if (!URI_IS_PCHAR(ch) && ch != '/' && ch != '?')
                return false;
        }
    }

    return true;
}

} // namespace irk
