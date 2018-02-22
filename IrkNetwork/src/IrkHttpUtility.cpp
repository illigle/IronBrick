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

#include <time.h>
#include <algorithm>
#include "IrkHttpUtility.h"

namespace irk {

// convert POSIX time to HTTP Date
std::string time_to_httpdate(time_t tmsp)
{
    struct tm tmtm = {};
#ifdef _WIN32
    _gmtime64_s(&tmtm, &tmsp);
#else
    gmtime_r(&tmsp, &tmtm);
#endif
    char buf[64] = {};
    strftime(buf, 64, "%a, %d %b %Y %H:%M:%S GMT", &tmtm);
    return std::string(buf);
}

// convert HTTP Date to POSIX time
bool httpdate_to_time(const char* date, time_t* pTime)
{
#ifdef _WIN32
    static const char* s_month[12] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    struct tm tmtm = {};
    char month[4] = {}, unused[4] = {};
    if (8 != sscanf(date,
        "%3c, %2d %3c %4d %2d:%2d:%2d %3c",
        unused,
        &tmtm.tm_mday, month, &tmtm.tm_year,
        &tmtm.tm_hour, &tmtm.tm_min, &tmtm.tm_sec,
        &unused))
    {
        return false;
    }
    // search month sequentially
    int midx = 0;
    for (; midx < 12; midx++)
    {
        if (_stricmp(s_month[midx], month) == 0)
            break;
    }
    if (midx < 12)     // got month index
    {
        tmtm.tm_year -= 1900;
        tmtm.tm_mon = midx;
        *pTime = _mkgmtime64(&tmtm);
    }
    else
    {
        return false;
    }
#else
    struct tm tmtm = {};
    if (!strptime(date, "%a, %d %b %Y %H:%M:%S GMT", &tmtm))
        return false;
    *pTime = timegm(&tmtm);
#endif
    return true;
}

// parse HTTP response status
// return character count of the status line, return -1 if got invalid response
int http_parse_status_line(const char* src, int size, int& status, int version[2], std::string* phrase)
{
    std::string line;
    int crlf = 0;

    // retrieve status line
    if (size < 0)                   // null-terminated string   
    {
        const char* end = strchr(src, '\r');
        if (end)
        {
            if (end[1] != '\n')     // invalid status line
                return -1;
            line.assign(src, end);
            crlf = 2;
        }
        else
        {
            line.assign(src);
        }
    }
    else
    {
        int i = 0;
        for (; i < size; i++)
        {
            if (src[i] == '\r')
            {
                if ((i + 1) >= size || src[i + 1] != '\n') // invalid status line
                    return -1;
                crlf = 2;
                break;
            }
        }
        line.assign(src, i);
    }

    // check http version prefix
    if (line.compare(0, 5, "HTTP/") != 0)
        return -1;

    // get version
    int dummy[2];
    if (!version)
        version = dummy;
    char* s = nullptr;
    version[0] = (int)strtol(line.c_str() + 5, &s, 10);
    if (*s != '.')
        return -1;
    version[1] = (int)strtol(s + 1, &s, 10);
    if (*s != ' ')
        return -1;

    // get stauts code
    status = (int)strtol(s + 1, &s, 10);
    if (*s != ' ')
        return -1;

    // get reason-phrase
    if (phrase)
        phrase->assign(s + 1);

    return crlf + (int)line.length();
}

// split http header field(name: value), return pointer to the remaining string, return nullptr if failed
static const char* split_header_field(const char* field, std::string& name, std::string& value)
{
    // skip leading LWS
    const char* s = field;
    for (; CTypeTraits<char>::is_ctlws(*s); s++)
    {
    }

   // split name
    const char* t = strchr(s, ':');
    if (!t || s == t)
        return nullptr;
    name.assign(s, t);

    // skip LWS before value
    s = t + 1;
    for (; CTypeTraits<char>::is_ctlws(*s); s++)
    {
    }

   // split value
    int crlf = 0;
    for (t = s; *t != 0; t++)
    {
        if (t[0] == '\r' && t[1] == '\n')   // find CRLF
        {
            crlf = 2;
            break;
        }
    }
    value.assign(s, t);

    // remove tailing LWS of value
    while (!value.empty() && CTypeTraits<char>::is_ctlws(value.back()))
        value.pop_back();

    return t + crlf;
}

// split http header field(name: value), return character count used, return -1 if failed
static int split_header_field(const char* field, int size, std::string& name, std::string& value)
{
    // skip leading LWS
    int i = 0;
    for (; i < size; i++)
    {
        if (!CTypeTraits<char>::is_ctlws(field[i]))
            break;
    }

    // split name
    int k = i;
    for (; k < size; k++)
    {
        if (field[k] == ':')
            break;
    }
    if ((k == i) || (k == size))   // not found
        return -1;
    name.assign(field + i, field + k);

    // skip LWS before value
    i = k + 1;
    for (; i < size; i++)
    {
        if (!CTypeTraits<char>::is_ctlws(field[i]))
            break;
    }

    // split value
    int crlf = 0;
    for (k = i; k < size; k++)
    {
        if ((field[k] == '\r') && ((k + 1) < size) && (field[k + 1] == '\n'))    // find CRLF
        {
            crlf = 2;
            break;
        }
    }
    value.assign(field + i, field + k);

    // remove tailing LWS of value
    while (!value.empty() && CTypeTraits<char>::is_ctlws(value.back()))
        value.pop_back();

    return k + crlf;
}

// NOTE 1:
// RFC-2616: Multiple message-header fields with the same field-name MAY be present in a message 
// if and only if the entire field-value for that header field is defined as a comma-separated list

// add a field(name:value), if field with the same name exists, append value to the existing field(NOTE 1)
bool HttpHeader::add_field(const char* field)
{
    std::string name, value;
    if (!split_header_field(field, name, value))
        return false;

    auto itr = this->find_field(name.c_str());
    if (itr != this->end())    // NOTE 1
    {
        itr->value += ", ";
        itr->value += value;
    }
    else
    {
        this->emplace_back(std::move(name), std::move(value));
    }
    return true;
}
void HttpHeader::add_field(const char* name, const char* value)
{
    auto itr = this->find_field(name);
    if (itr != this->end())    // NOTE 1
    {
        itr->value += ", ";
        itr->value += value;
    }
    else
    {
        this->emplace_back(name, value);
    }
}

// add a field(name:value), if field with the same name exists, overwrite old value
bool HttpHeader::add_or_replace_field(const char* field)
{
    std::string name, value;
    if (!split_header_field(field, name, value))
        return false;

    auto itr = this->find_field(name.c_str());
    if (itr != this->end())
        itr->value = value;
    else
        this->emplace_back(std::move(name), std::move(value));

    return true;
}
void HttpHeader::add_or_replace_field(const char* name, const char* value)
{
    auto itr = this->find_field(name);
    if (itr != this->end())
        itr->value = value;
    else
        this->emplace_back(name, value);
}

// remove all field with the name, field name is case-insensitive
bool HttpHeader::remove_field(const char* name)
{
    auto itr = std::remove_if(this->begin(), this->end(),
        [=](const auto& field) {return stricmp(field.name.c_str(), name) == 0; });
    if (itr != this->end())
    {
        this->erase(itr, this->end());
        return true;
    }
    return false;
}

// search field sequentially
HttpHeader::iterator HttpHeader::find_field(const char* name)
{
    return std::find_if(this->begin(), this->end(),
        [=](const auto& field) {return stricmp(field.name.c_str(), name) == 0; });
}
HttpHeader::const_iterator HttpHeader::find_field(const char* name) const
{
    return std::find_if(this->cbegin(), this->cend(),
        [=](const auto& field) {return stricmp(field.name.c_str(), name) == 0; });
}

// fill this from HTTP header string, size < 0 means input string is null-terminated
void HttpHeader::from_string(const char* str, int size)
{
    // clear old header
    this->clear();

    // split all header fields
    std::string name, value;
    if (size >= 0)
    {
        while (size > 0)
        {
            int len = split_header_field(str, size, name, value);
            if (len <= 0)
                break;
            this->emplace_back(std::move(name), std::move(value));
            str += len;
            size -= len;
        }
    }
    else
    {
        for (const char* cur = str; true; )
        {
            cur = split_header_field(cur, name, value);
            if (!cur)
                break;
            this->emplace_back(std::move(name), std::move(value));
        }
    }
}

// output HTTP header string, each header field will be ended with "\r\n"
std::string HttpHeader::to_string() const
{
    std::string header;
    header.reserve(256);

    for (const auto& field : *this)
    {
        header += field.name;
        header += ": ";
        header += field.value;
        header += "\r\n";
    }
    return header;
}

// add a field(name:value), if field with the same name exists, append value to the existing field(NOTE 1)
bool HttpHeaderMap::add_field(const char* field)
{
    std::string name, value;
    if (!split_header_field(field, name, value))
        return false;

    auto res = this->emplace(name, value);
    if (!res.second)   // NOTE 1
    {
        res.first->second += ", ";
        res.first->second += value;
    }
    return true;
}
void HttpHeaderMap::add_field(const char* name, const char* value)
{
    auto res = this->emplace(name, value);
    if (!res.second)   // NOTE 1
    {
        res.first->second += ", ";
        res.first->second += value;
    }
}

// fill this from HTTP header string, size < 0 means input string is null-terminated
void HttpHeaderMap::from_string(const char* str, int size)
{
    // clear old header
    this->clear();

    // split all header fields
    std::string name, value;
    if (size >= 0)
    {
        while (size > 0)
        {
            int len = split_header_field(str, size, name, value);
            if (len <= 0)
                break;
            str += len;
            size -= len;

            auto res = this->emplace(name, value);
            if (!res.second)   // NOTE 1
            {
                res.first->second += ", ";
                res.first->second += value;
            }
        }
    }
    else
    {
        for (const char* cur = str; true; )
        {
            cur = split_header_field(cur, name, value);
            if (!cur)
                break;

            auto res = this->emplace(name, value);
            if (!res.second)   // NOTE 1
            {
                res.first->second += ", ";
                res.first->second += value;
            }
        }
    }
}

// output HTTP header string, each header field will be ended with "\r\n"
std::string HttpHeaderMap::to_string() const
{
    std::string header;
    header.reserve(256);

    for (const auto& field : *this)
    {
        header += field.first;
        header += ": ";
        header += field.second;
        header += "\r\n";
    }
    return header;
}

HttpRequest::HttpRequest()
{
    m_contentLength = 0;
    m_customTimeout = false;
    memset(&m_timeout, 0, sizeof(m_timeout));
}

std::string HttpRequest::get_reqline() const
{
    std::string line = m_method;
    line += ' ';
    line += m_resource;
    line += ' ';
    line += "HTTP/1.1";
    line += "\r\n";
    return line;
}

// set timeout in milliseconds, 0 means default, if !pTimeout, reset timeout to default
void HttpRequest::set_timeout(const HttpTimeout* pTimeout)
{
    if (!pTimeout)  // reset timeout
    {
        m_customTimeout = false;
        memset(&m_timeout, 0, sizeof(m_timeout));
    }
    else
    {
        m_customTimeout = true;
        m_timeout = *pTimeout;
    }
}

// reset request, remove everything
void HttpRequest::reset()
{
    m_method.clear();
    m_resource.clear();
    m_htab.clear();
    m_body.clear();
    m_contentLength = 0;
    m_customTimeout = false;
    memset(&m_timeout, 0, sizeof(m_timeout));
}

//======================================================================================================================
// wchar_t version for Windows

#ifdef _WIN32

// Convert POSIX time(seconds elapsed since midnight 1-1-1970, UTC) to HTTP Date(RFC 1123)
std::wstring time_to_whttpdate(time_t tmsp)
{
    struct tm tmtm = {0};
    _gmtime64_s(&tmtm, &tmsp);
    wchar_t buf[64] = {0};
    wcsftime(buf, 64, L"%a, %d %b %Y %H:%M:%S GMT", &tmtm);
    return std::wstring(buf);
}

// Convert HTTP Date(RFC 1123) to POSIX time(seconds elapsed since midnight 1-1-1970, UTC)
bool whttpdate_to_time(const wchar_t* date, time_t* pTime)
{
    static const wchar_t* s_month[12] = {
        L"Jan", L"Feb", L"Mar", L"Apr", L"May", L"Jun", L"Jul", L"Aug", L"Sep", L"Oct", L"Nov", L"Dec"
    };

    struct tm tmtm = {0};
    wchar_t month[4] = {0}, unused[4] = {0};
    if (swscanf(date,
        L"%3c, %2d %3c %4d %2d:%2d:%2d %3c",
        unused,
        &tmtm.tm_mday, month, &tmtm.tm_year,
        &tmtm.tm_hour, &tmtm.tm_min, &tmtm.tm_sec,
        &unused) != 8)
    {
        return false;
    }

    // search month sequentially
    int midx = 0;
    for (; midx < 12; midx++)
    {
        if (_wcsicmp(s_month[midx], month) == 0)
            break;
    }
    if (midx < 12)     // got month index
    {
        tmtm.tm_year -= 1900;
        tmtm.tm_mon = midx;
        *pTime = _mkgmtime64(&tmtm);
        return true;
    }

    return false;
}

// Parse HTTP status from response header
// return character count of the status line, return -1 if got invalid response
int http_parse_status_line(const wchar_t* src, int size, int& status, int version[2], std::wstring* phrase)
{
    std::wstring line;
    int crlf = 0;

    // retrieve status line
    if (size < 0)   // null-terminated string   
    {
        const wchar_t* end = wcschr(src, L'\r');
        if (end)
        {
            if (end[1] != '\n')    // invalid status line
                return -1;
            line.assign(src, end);
            crlf = 2;
        }
        else
        {
            line.assign(src);
        }
    }
    else
    {
        int i = 0;
        for (; i < size; i++)
        {
            if (src[i] == '\r')
            {
                if ((i + 1) >= size || src[i + 1] != '\n')  // invalid status line
                    return -1;
                crlf = 2;
                break;
            }
        }
        line.assign(src, i);
    }

    // check http version prefix
    if (line.compare(0, 5, L"HTTP/") != 0)
        return -1;

    // get version
    int dummy[2];
    if (!version)
        version = dummy;
    wchar_t* s = nullptr;
    version[0] = (int)wcstol(line.c_str() + 5, &s, 10);
    if (*s != '.')
        return -1;
    version[1] = (int)wcstol(s + 1, &s, 10);
    if (*s != ' ')
        return -1;

    // get stauts code
    status = (int)wcstol(s + 1, &s, 10);
    if (*s != ' ')
        return -1;

    // get reason-phrase
    if (phrase)
        phrase->assign(s + 1);

    return crlf + (int)line.length();
}

// split http header field(name: value), return pointer to the remaining string, return nullptr if failed
static const wchar_t* split_header_field(const wchar_t* field, std::wstring& name, std::wstring& value)
{
    // skip leading LWS
    const wchar_t* s = field;
    for (; CTypeTraits<wchar_t>::is_ctlws(*s); s++)
    {
    }

   // split name
    const wchar_t* t = wcschr(s, L':');
    if (!t || s == t)
        return nullptr;
    name.assign(s, t);

    // skip LWS before value
    s = t + 1;
    for (; CTypeTraits<wchar_t>::is_ctlws(*s); s++)
    {
    }

   // split value
    int crlf = 0;
    for (t = s; *t != 0; t++)
    {
        if (t[0] == L'\r' && t[1] == L'\n')    // find CRLF
        {
            crlf = 2;
            break;
        }
    }
    value.assign(s, t);

    // remove tailing LWS of value
    while (!value.empty() && CTypeTraits<wchar_t>::is_ctlws(value.back()))
        value.pop_back();

    return t + crlf;
}

// split http header field(name: value), return character count used, return -1 if failed
static int split_header_field(const wchar_t* field, int size, std::wstring& name, std::wstring& value)
{
    // skip leading LWS
    int i = 0;
    for (; i < size; i++)
    {
        if (!CTypeTraits<wchar_t>::is_ctlws(field[i]))
            break;
    }

    // split name
    int k = i;
    for (; k < size; k++)
    {
        if (field[k] == L':')
            break;
    }
    if ((k == i) || (k == size))   // not found
        return -1;
    name.assign(field + i, field + k);

    // skip LWS before value
    i = k + 1;
    for (; i < size; i++)
    {
        if (!CTypeTraits<wchar_t>::is_ctlws(field[i]))
            break;
    }

    // split value
    int crlf = 0;
    for (k = i; k < size; k++)
    {
        if ((field[k] == L'\r') && ((k + 1) < size) && (field[k + 1] == L'\n'))  // find CRLF
        {
            crlf = 2;
            break;
        }
    }
    value.assign(field + i, field + k);

    // remove tailing LWS of value
    while (!value.empty() && CTypeTraits<wchar_t>::is_ctlws(value.back()))
        value.pop_back();

    return k + crlf;
}

// convert from UTF-8 version
void WHttpHeader::from_utf8(const HttpHeader& srcHeader)
{
    std::wstring wname, wvalue;
    for (const auto& field : srcHeader)
    {
        str_utf8to16(field.name, wname);
        str_utf8to16(field.value, wvalue);
        this->emplace_back(std::move(wname), std::move(wvalue));
    }
}

// NOTE 1:
// RFC-2616: Multiple message-header fields with the same field-name MAY be present in a message 
// if and only if the entire field-value for that header field is defined as a comma-separated list

// add a field(name:value), if field with the same name exists, append value to the existing field(NOTE 1)
bool WHttpHeader::add_field(const wchar_t* field)
{
    std::wstring name, value;
    if (!split_header_field(field, name, value))
        return false;

    auto itr = this->find_field(name.c_str());
    if (itr != this->end())    // NOTE 1
    {
        itr->value += L", ";
        itr->value += value;
    }
    else
    {
        this->emplace_back(std::move(name), std::move(value));
    }
    return true;
}
void WHttpHeader::add_field(const wchar_t* name, const wchar_t* value)
{
    auto itr = this->find_field(name);
    if (itr != this->end())    // NOTE 1
    {
        itr->value += L", ";
        itr->value += value;
    }
    else
    {
        this->emplace_back(name, value);
    }
}

// remove all field with the name, field name is case-insensitive
bool WHttpHeader::remove_field(const wchar_t* name)
{
    auto itr = std::remove_if(this->begin(), this->end(),
        [=](const auto& field) {return ::wcscmp(field.name.c_str(), name) == 0; });
    if (itr != this->end())
    {
        this->erase(itr, this->end());
        return true;
    }
    return false;
}

// search field sequentially
WHttpHeader::iterator WHttpHeader::find_field(const wchar_t* name)
{
    return std::find_if(this->begin(), this->end(),
        [=](const auto& field) {return wcsicmp(field.name.c_str(), name) == 0; });
}
WHttpHeader::const_iterator WHttpHeader::find_field(const wchar_t* name) const
{
    return std::find_if(this->begin(), this->end(),
        [=](const auto& field) {return wcsicmp(field.name.c_str(), name) == 0; });
}

// fill this from HTTP header wstring, size < 0 means input wstring is null-terminated
void WHttpHeader::from_string(const wchar_t* str, int size)
{
    // clear old header
    this->clear();

    // split all header fields
    std::wstring name, value;
    if (size >= 0)
    {
        while (size > 0)
        {
            int len = split_header_field(str, size, name, value);
            if (len <= 0)
                break;
            this->emplace_back(std::move(name), std::move(value));
            str += len;
            size -= len;
        }
    }
    else
    {
        for (const wchar_t* cur = str; true; )
        {
            cur = split_header_field(cur, name, value);
            if (!cur)
                break;
            this->emplace_back(std::move(name), std::move(value));
        }
    }
}

// output HTTP header wstring, each header field will be ended with "\r\n"
std::wstring WHttpHeader::to_string() const
{
    std::wstring header;
    header.reserve(256);

    for (const auto& field : *this)
    {
        header += field.name;
        header += L": ";
        header += field.value;
        header += L"\r\n";
    }
    return header;
}

// convert from UTF-8 version
void WHttpHeaderMap::from_utf8(const HttpHeaderMap& srcMap)
{
    std::wstring wname, wvalue;
    for (const auto& field : srcMap)
    {
        str_utf8to16(field.first, wname);
        str_utf8to16(field.second, wvalue);
        this->emplace(std::move(wname), std::move(wvalue));
    }
}

// add a field(name:value), if field with the same name exists, append value to the existing field(NOTE 1)
bool WHttpHeaderMap::add_field(const wchar_t* field)
{
    std::wstring name, value;
    if (!split_header_field(field, name, value))
        return false;

    auto res = this->emplace(name, value);
    if (!res.second)   // NOTE 1
    {
        res.first->second += L", ";
        res.first->second += value;
    }
    return true;
}
void WHttpHeaderMap::add_field(const wchar_t* name, const wchar_t* value)
{
    auto res = this->emplace(name, value);
    if (!res.second)   // NOTE 1
    {
        res.first->second += L", ";
        res.first->second += value;
    }
}

// fill this from HTTP header wstring, size < 0 means input wstring is null-terminated
void WHttpHeaderMap::from_string(const wchar_t* str, int size)
{
    // clear old header
    this->clear();

    // split all header fields
    std::wstring name, value;
    if (size >= 0)
    {
        while (size > 0)
        {
            int len = split_header_field(str, size, name, value);
            if (len <= 0)
                break;
            str += len;
            size -= len;

            auto res = this->emplace(name, value);
            if (!res.second)   // NOTE 1
            {
                res.first->second += L", ";
                res.first->second += value;
            }
        }
    }
    else
    {
        for (const wchar_t* cur = str; true; )
        {
            cur = split_header_field(cur, name, value);
            if (!cur)
                break;

            auto res = this->emplace(name, value);
            if (!res.second)   // NOTE 1
            {
                res.first->second += L", ";
                res.first->second += value;
            }
        }
    }
}

// output HTTP header wstring, each header field will be ended with "\r\n"
std::wstring WHttpHeaderMap::to_string() const
{
    std::wstring header;
    header.reserve(256);

    for (const auto& field : *this)
    {
        header += field.first;
        header += L": ";
        header += field.second;
        header += L"\r\n";
    }
    return header;
}

WHttpRequest::WHttpRequest()
{
    m_contentLength = 0;
    m_customTimeout = false;
    memset(&m_timeout, 0, sizeof(m_timeout));
}

 // convert form UTF-8 version
void WHttpRequest::from_utf8(const HttpRequest& src)
{
    str_utf8to16(src.method(), m_method);
    str_utf8to16(src.resource(), m_resource);

    m_htab.from_utf8(src.header());

    if (src.body_size() > 0)
        m_body.assign(src.body_data(), src.body_data() + src.body_size());
    else
        m_body.clear();

    this->set_content_length(src.content_length());

    this->set_timeout(src.get_timeout());
}

// set timeout in milliseconds, 0 means default, if !pTimeout, reset timeout to default
void WHttpRequest::set_timeout(const HttpTimeout* pTimeout)
{
    if (!pTimeout)     // reset timeout
    {
        m_customTimeout = false;
        memset(&m_timeout, 0, sizeof(m_timeout));
    }
    else
    {
        m_customTimeout = true;
        m_timeout = *pTimeout;
    }
}

// reset request, remove everything
void WHttpRequest::reset()
{
    m_method.clear();
    m_resource.clear();
    m_htab.clear();
    m_body.clear();
    m_contentLength = 0;
    m_customTimeout = false;
    memset(&m_timeout, 0, sizeof(m_timeout));
}

#endif  // WIN32

}   // namespace irk
