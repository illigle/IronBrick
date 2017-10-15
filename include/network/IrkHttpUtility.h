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

#ifndef _IRONBRICK_HTTPUTILITY_H_
#define _IRONBRICK_HTTPUTILITY_H_

#include <unordered_map>
#include <vector>
#include <utility>          // std::move
#include "IrkCiString.h"    // case-insensitive string

// HTTP client utility
namespace irk {

// Convert POSIX time(seconds elapsed since midnight 1-1-1970, UTC) to HTTP Date(RFC 1123)
std::string time_to_httpdate( time_t tmsp );

// Convert HTTP Date(RFC 1123) to POSIX time(seconds elapsed since midnight 1-1-1970, UTC)
bool httpdate_to_time( const char* date, time_t* pTime );

// Parse HTTP response status line, "size" < 0 means input string is null-terminated
// return character count of the status line, return -1 if got invalid response
int http_parse_status_line( const char* response, int size, int& status,
                                int version[2] = nullptr, std::string* phrase = nullptr );

// HTTP timeout record
struct HttpTimeout
{
    int	resovleTimeout;     // name resolution timeout in milliseconds
    int	connectTimeout;     // server connection timeout in milliseconds
    int	sendTimeout;        // request sending timeout in milliseconds
    int	recvTimeout;        // response receiving timeout in milliseconds
};

// HTTP header field(name:value), UTF-8 encoding
struct HttpHeaderField
{
    HttpHeaderField( const char* nm, const char* val ) : name(nm), value(val) {}
    HttpHeaderField( const std::string& nm, const std::string& val ) : name(nm), value(val) {}
    HttpHeaderField( std::string&& nm, std::string&& val ) : name(std::move(nm)), value(std::move(val)) {}
    std::string to_string() const
    {
        std::string field( name );
        field += ": ";
        field += value;
        return field;
    }
    std::string name;       // field name
    std::string value;      // field value
};
inline bool operator==( const HttpHeaderField& field1, const HttpHeaderField& field2 )
{
    // field name is case-insensitive
    return (stricmp(field1.name.c_str(), field2.name.c_str()) == 0) && (field1.value == field2.value);
}
inline bool operator!=( const HttpHeaderField& field1, const HttpHeaderField& field2 )
{
    return !(field1 == field2);
}

// NOTE 1:
// RFC-2616: Multiple message-header fields with the same field-name MAY be present in a message 
// if and only if the entire field-value for that header field is defined as a comma-separated list

// HTTP header(header fields vector)
class HttpHeader : public std::vector<HttpHeaderField>  
{
public:
    // add a field(name:value), if field with the same name exists, append value to the existing field(NOTE 1)
    // using vector::emplace_back to add multiple fields with the same name
    bool add_field( const char* field );
    void add_field( const char* name, const char* value );

    // add a field(name:value), if field with the same name exists, overwrite old value
    bool add_or_replace_field( const char* field );
    void add_or_replace_field( const char* name, const char* value );

    // remove all field with the name, field name is case-insensitive
    bool remove_field( const char* name );

    // search field sequentially, use HttpHeaderMap for fast search
    // return this->end() if failed
    HttpHeader::iterator       find_field( const char* name );
    HttpHeader::const_iterator find_field( const char* name ) const;

    // fill this from HTTP header string, size < 0 means input string is null-terminated
    void from_string( const char* str, int size = -1 );
    void from_string( const std::string& str )
    {
        this->from_string( str.c_str(), -1 );
    }

    // output HTTP header string, each header field will be ended with "\r\n"
    std::string to_string() const;
};

// HTTP header hash table, used to find header field quickly
class HttpHeaderMap
    : public std::unordered_map<std::string,std::string,ci_hash<std::string>,ci_equal_to<std::string>>
{
public:
    // add a field(name:value), if field with the same name exists, append value to the existing field(NOTE 1)
    bool add_field( const char* field );
    void add_field( const char* name, const char* value );

    // fill this from HTTP headers string, size < 0 means input string is null-terminated
    void from_string( const char* str, int size = -1 );
    void from_string( const std::string& str )
    {
        this->from_string( str.c_str(), -1 );
    }

    // output HTTP headers string, each header will be ended with "\r\n"
    std::string to_string() const;
};

// HTTP request, UTF-8 encoding
class HttpRequest
{
public:
    HttpRequest();

    // reset request, remove everything
    void reset();

    // set/get HTTP method( "GET", "POST" ... )
    void set_method( const char* method )           { m_method = method; }
    void set_method( const std::string& method )    { m_method = method; }
    void set_method( std::string&& method )         { m_method = std::move(method); }
    const std::string& method() const               { return m_method; }

    // set/get resource(path?query#fragment)
    void set_resource( const char* res )            { m_resource = res; }
    void set_resource( const std::string& res )     { m_resource = res; }
    void set_resource( std::string&& res )          { m_resource = std::move(res); }
    const std::string& resource() const             { return m_resource; }

    // get request line: <method> <resouce> <version> CRLF
    std::string get_reqline() const;

    // header accessor
    const HttpHeader& header() const                { return m_htab; }
    HttpHeader& header()                            { return m_htab; }
    
    // set header from string, each header field must be ended with "\r\n"
    void set_header( const char* str )              { m_htab.from_string( str ); }
    void set_header( const std::string& str )       { m_htab.from_string( str ); }

    // add body data for sending, body data can also be sent by HttpClientHandler::on_send_body
    void add_body_data( const char* data, size_t size )
    {
        m_body.insert( m_body.end(), data, data + size );
        if( m_contentLength < (int64_t)m_body.size() )
            m_contentLength = (int64_t)m_body.size();
    }

    // currently saved body data
    const char* body_data() const   { return m_body.data(); }
    // currently saved body size
    size_t body_size() const        { return m_body.size(); }
    // remove currently saved body data
    void clear_body()               { m_body.clear(); }

    // set Content-Length(the total size of body that will be sent)
    // if Content-Length > current body size, extra data must be provided by HttpClientHandler::on_send_body
    void set_content_length( int64_t len )
    {
        if( m_body.size() > (size_t)len )
            m_body.resize( (size_t)len );   // discard extra data
        m_contentLength = len;
    }
    // get Content-Length(the total size of body that will be sent)
    int64_t content_length() const  { return m_contentLength; }

    // set timeout, if pTimeout==nullptr, reset timeout to default
    void set_timeout( const HttpTimeout* pTimeout );
    // get timeout, return nullptr if no custom timeout set
    const HttpTimeout* get_timeout() const { return m_customTimeout ? &m_timeout : nullptr; }
private:
    std::string         m_method;           // request method("GET", "POST" ...)
    std::string         m_resource;         // resouce URI
    HttpHeader          m_htab;             // header table
    std::vector<char>   m_body;             // body data
    int64_t             m_contentLength;    // Content-Length
    bool			    m_customTimeout;    // using custom timeout
    HttpTimeout         m_timeout;
};

//======================================================================================================================
// wchar_t version for Windows
#ifdef _WIN32

// Convert POSIX time(seconds elapsed since midnight 1-1-1970, UTC) to HTTP Date(RFC 1123)
std::wstring time_to_whttpdate( time_t tmsp );

// Convert HTTP Date(RFC 1123) to POSIX time(seconds elapsed since midnight 1-1-1970, UTC)
bool whttpdate_to_time( const wchar_t* date, time_t* pTime );

// Parse HTTP status from response header, [size] < 0 means input string is null-terminated
// return character count of the status line, return -1 if got invalid response
int http_parse_status_line( const wchar_t* response, int size, int& status,
                                int version[2] = nullptr, std::wstring* phrase = nullptr );

// HTTP header field(name:value)
struct WHttpHeaderField
{
    WHttpHeaderField( const wchar_t* nm, const wchar_t* val ) : name(nm), value(val) {}
    WHttpHeaderField( const std::wstring& nm, const std::wstring& val ) : name(nm), value(val) {}
    WHttpHeaderField( std::wstring&& nm, std::wstring&& val ) : name(std::move(nm)), value(std::move(val)) {}
    std::wstring to_string() const
    {
        std::wstring field( name );
        field += L": ";
        field += value;
        return field;
    }
    std::wstring name;  // field name
    std::wstring value; // field value
};
inline bool operator==( const WHttpHeaderField& field1, const WHttpHeaderField& field2 )
{
    // field name is case-insensitive
    return (wcsicmp(field1.name.c_str(), field2.name.c_str()) == 0) && (field1.value == field2.value);
}
inline bool operator!=( const WHttpHeaderField& field1, const WHttpHeaderField& field2 )
{
    return !(field1 == field2);
}

// HTTP header(header field vector)
class WHttpHeader : public std::vector<WHttpHeaderField>  
{
public:
    // convert from UTF-8 version
    void from_utf8( const HttpHeader& header );

    // add a field(name:value), if field with the same name exists, append value to the existing field(NOTE 1)
    // using vector::emplace_back to add multiple fields with the same name
    bool add_field( const wchar_t* field );
    void add_field( const wchar_t* name, const wchar_t* value );

    // remove all field with the name, field name is case-insensitive
    bool remove_field( const wchar_t* name );

    // search field sequentially, use HttpHeaderMap for fast search
    // return this->end() if failed
    WHttpHeader::iterator       find_field( const wchar_t* name );
    WHttpHeader::const_iterator find_field( const wchar_t* name ) const;

    // fill this from HTTP headers string, size < 0 means input string is null-terminated
    void from_string( const wchar_t* str, int size = -1 );
    void from_string( const std::wstring& str )
    {
        this->from_string( str.c_str(), -1 );
    }

    // output HTTP headers string, each header will be ended with "\r\n"
    std::wstring to_string() const;
};

// HTTP header hash table, used to find header field quickly
class WHttpHeaderMap
    : public std::unordered_map<std::wstring,std::wstring,ci_hash<std::wstring>,ci_equal_to<std::wstring>>
{
public:
    // convert from UTF-8 version
    void from_utf8( const HttpHeaderMap& hmap );

    // add a field(name:value), if field with the same name exists, append value to the existing field(NOTE 1)
    // NOTE: use vector::emplace_back to retain the origin format
    bool add_field( const wchar_t* field );
    void add_field( const wchar_t* name, const wchar_t* value );

    // fill this from HTTP headers string, size < 0 means input string is null-terminated
    void from_string( const wchar_t* str, int size = -1 );
    void from_string( const std::wstring& str )
    {
        this->from_string( str.c_str(), -1 );
    }

    // output HTTP headers string, each header will be ended with "\r\n"
    std::wstring to_string() const;
};

// HTTP request
class WHttpRequest
{
public:
    WHttpRequest();

    // convert from UTF-8 version
    void from_utf8( const HttpRequest& req );

    // reset request, remove everything
    void reset();

    // set/get HTTP method( "GET", "POST" ... )
    void set_method( const wchar_t* method )        { m_method = method; }
    void set_method( const std::wstring& method )   { m_method = method; }
    void set_method( std::wstring&& method )        { m_method = std::move(method); }
    const std::wstring& method() const              { return m_method; }

    // set/get resource(path?query#fragment)
    void set_resource( const wchar_t* res )         { m_resource = res; }
    void set_resource( const std::wstring& res )    { m_resource = res; }
    void set_resource( std::wstring&& res )         { m_resource = std::move(res); }
    const std::wstring& resource() const            { return m_resource; }

    // header accessor
    const WHttpHeader& header() const               { return m_htab; }
    WHttpHeader& header()                           { return m_htab; }
    
    void set_header( const wchar_t* str )           { m_htab.from_string( str ); }
    void set_header( const std::wstring& str )      { m_htab.from_string( str ); }

    // add body data for sending, body data can also be sent by HttpClientHandler::on_send_body
    void add_body_data( const char* data, int size )
    {
        m_body.insert( m_body.end(), data, data + size );
        if( m_contentLength < m_body.size() )
            m_contentLength = m_body.size();
    }
    // currently saved body data
    const char* body_data() const   { return m_body.data(); }
    // currently saved body size
    size_t body_size() const        { return m_body.size(); }
    // remove saved body data
    void clear_body()               { m_body.clear(); }

    // set Content-Length (the total size of body that will be sent)
    // if Content-Length > current body size, extra data must be provided by HttpClientHandler::on_send_body
    void set_content_length( size_t len )
    {
        m_contentLength = len;
        if( m_body.size() > len )
            m_body.resize( len );   // discard extra data
    }
    // get Content-Length(the total size of body that will be sent)
    size_t content_length() const   { return m_contentLength; }

    // set timeout, if pTimeout==nullptr, reset timeout to default
    void set_timeout( const HttpTimeout* pTimeout );
    // get timeout, return nullptr if no custom timeout set
    const HttpTimeout* get_timeout() const { return m_customTimeout ? &m_timeout : nullptr; }

private:
    std::wstring        m_method;           // request method("GET", "POST" ...)
    std::wstring        m_resource;         // resouce URI
    WHttpHeader         m_htab;             // header table
    std::vector<char>   m_body;             // body data
    size_t              m_contentLength;    // Content-Length
    bool			    m_customTimeout;    // using custom timeout
    HttpTimeout         m_timeout;
};

#endif  // _Win32

}   // namespace irk
#endif
