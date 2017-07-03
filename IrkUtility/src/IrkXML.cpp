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

#include "IrkMemPool.h"
#include "IrkStringUtility.h"
#include "IrkCFile.h"
#include "IrkBase64.h"
#include "IrkXML.h"

namespace irk {

using std::string;
using detail::XmlStr;

// XML memory allocator
class XmlAlloc : IrkNocopy
{
public:
    XmlAlloc() : m_memPool(16*1024)
    {
        m_tempStr.reserve( 128 );
    }

    // create new object
    template<class Ty, class... Args>
    Ty* create( Args&&... args )
    {
        void* ptr = m_memPool.alloc( sizeof(Ty), alignof(Ty) );
        return ::new(ptr) Ty( std::forward<Args>(args)... );    // inplace new
    }
    // delete object
    template<class Ty>
    void trash( Ty* ptr )
    {
        if( ptr )
        {
            ptr->~Ty();
            m_memPool.dealloc( ptr, sizeof(Ty) );
        }
    }

    // allocate memory block
    void* alloc( size_t size, size_t alignment = sizeof(void*) )
    {
        return m_memPool.alloc( size, alignment );
    }
    // deallocate memory block returned from alloc()
    void dealloc( void* ptr, size_t size )
    {
        m_memPool.dealloc( ptr, size );
    }
    // real buffer capacity for the requested size
    size_t bucket_size( size_t size ) const
    {
        return m_memPool.bucket_size( size );
    }

    // temp string used to parse xml text
    std::string& temp_string()
    {
        m_tempStr.clear();
        return m_tempStr;
    }

private:
    MemPool     m_memPool;
    std::string m_tempStr;
};

// alloc an empty string, assure capacity, discard existing data
inline void detail::XmlStr::alloc( XmlAlloc* allocator, int capacity )
{
    if( this->cap < capacity )
    {
        size_t buksize = allocator->bucket_size( capacity );
        void* newbuf = allocator->alloc( buksize );
        assert( (int)buksize >= capacity );

        if( this->cap > 0 )
            allocator->dealloc( this->buf, (size_t)this->cap );
        this->buf = (char*)newbuf;
        this->cap = (int)buksize;
    }
    this->len = 0;  // discard existing data
}

// deallocate string buffer
inline void detail::XmlStr::clear( XmlAlloc* allocator )
{
    if( this->cap > 0 )
    {
        allocator->dealloc( this->buf, (size_t)this->cap );
        this->buf = (char*)"";
        this->cap = 0;
        this->len = 0;
    }
}

// reserve string capacity, copy existing data
void detail::XmlStr::reserve( XmlAlloc* allocator, int capacity )
{
    if( this->cap < capacity )
    {
        // alloc new buffer, copy existing data
        size_t buksize = allocator->bucket_size( capacity );
        void* newbuf = allocator->alloc( buksize );
        assert( (int)buksize >= capacity );

        if( this->len > 0 )
            memcpy( newbuf, this->buf, this->len );
        
        if( this->cap > 0 )
            allocator->dealloc( this->buf, (size_t)this->cap );
        this->buf = (char*)newbuf;
        this->cap = (int)buksize;
    }
}

// set new string
inline void detail::XmlStr::assign( XmlAlloc* allocator, const char* src, int length )
{
    // assure buffer is big enough
    if( length < 0 )
        length = (int)strlen( src );
    this->alloc( allocator, length + 1 );

    assert( this->cap > length );
    memcpy( this->buf, src, length );
    this->buf[length] = '\0';
    this->len = length;
}
inline void detail::XmlStr::assign( XmlAlloc* allocator, const string& src )
{
    // assure buffer is big enough
    int length = (int)src.length();
    this->alloc( allocator, length + 1 );

    assert( this->cap > length );
    memcpy( this->buf, src.data(), length );
    this->buf[length] = '\0';
    this->len = length;
}

// append string
inline void detail::XmlStr::append( XmlAlloc* allocator, const char* src, int length )
{
    if( length < 0 )
        length = (int)strlen( src );
    const int newlen = this->len + length;
    this->reserve( allocator, newlen + 1 );

    assert( this->cap > newlen );
    memcpy( this->buf + this->len, src, length );
    this->len = newlen;
    this->buf[newlen] = '\0';
}
inline void detail::XmlStr::append( XmlAlloc* allocator, const string& src )
{
    const int newlen = this->len + (int)src.length();
    this->reserve( allocator, newlen + 1 );

    assert( this->cap > newlen );
    memcpy( this->buf + this->len, src.data(), (int)src.length() );
    this->len = newlen;
    this->buf[newlen] = '\0';
}

//======================================================================================================================

// default, string may need escaped
#define CVT_FLAG_DEFAULT    0
// plain text, no conversion needed
#define CVT_FLAG_NOCVT      1
// save as CDATA
#define CVT_FLAG_CDATA      2

// XML Control + MarkUp char(<, >, ", ', &)
static const uint8_t s_CtrlMarkChar[256] =
{
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    0,0,1,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,1,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};

// XML NameChar
static const uint8_t s_NameChar[256] =
{
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,
    0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,1,
    0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
};

#define IS_CTRLMARK(ch) (s_CtrlMarkChar[(uint8_t)(ch)] != 0)
#define IS_NAMECHAR(ch) (s_NameChar[(uint8_t)(ch)] != 0)

// get the length of string literal
template<size_t N>
static constexpr int lengthof( const char (&str)[N] )
{
    static_assert( N > 0, "" );
    return static_cast<int>( N - 1 );
}

// whether s1 is begin with s2
static bool str_begwith( const char* s1, const char* s2 )
{
    while( *s2 )
    {
        if( *s1 != *s2 )
            return false;
        s1++;
        s2++;
    }
    return true;
}

// convert string to XML text, add to the back of the dst string
static void escape_xml_string( const char* src, string& dst )
{
    while( 1 )
    {
        if( IS_CTRLMARK( *src  ) )  // character need to be escaped
        {
            if( *src == 0 )
                break;
            else if( *src == '&' )
                dst += "&amp;";
            else if( *src == '\'' )
                dst += "&apos;";
            else if( *src == '\"' )
                dst += "&quot;";       
            else if( *src == '<' )
                dst += "&lt;";
            else if( *src == '>' )
                dst += "&gt;";
            else if( *src == '\t' )
                dst += "&#9;";
            else if( *src == '\n' )
                dst += "&#10;";
            else if( *src == '\r' )
                dst += "&#13;";        
            else
            {}  // discard other control characters
        }
        else    // plain text
        {
            dst += *src;
        }
        src++;
    }
}

// convert XML text to normal string, return character count comsumed, return -1 if failed
static int unescape_xml_text( const char* text, std::string& dst )
{
    assert( text[0] == '&' );

    if( text[1] == '#' )        // number
    {
        uint32_t chNum = 0;
        int k = 2;
        if( text[2] == 'x' )    // hex-decimal
        {
            for( k = 3; true; k++ )
            {
                int ch = text[k] | 0x20;         // convert to lower
                if( ch >= '0' && ch <= '9' )
                    chNum = (chNum << 4) + (ch - '0');
                else if( ch >= 'a' && ch <= 'f' )
                    chNum = (chNum << 4) + (ch - 'a' + 10);
                else
                    break;
            }
        }
        else    // decimal
        {
            while( text[k] >= '0' && text[k] <= '9' )
            {
                chNum = (chNum * 10) + (text[k] - '0');
                k++;
            }
        }

        // is valid character number
        if( text[k] != ';' || chNum == 0 || chNum > 0x10FFFF )
        {
            return -1;
        }

        // convert to UTF-8 character
        if( chNum < 0x80 )
        {
            dst += (char)chNum;
        }
        else if( chNum < 0x800 )
        {
            dst += (char)(0xC0 | (chNum >> 6));
            dst += (char)(0x80 | (chNum & 0x3F));
        }
        else if( chNum < 0x10000 )
        {
            dst += (char)(0xE0 | (chNum >> 12));
            dst += (char)(0x80 | ((chNum >> 6) & 0x3F));
            dst += (char)(0x80 | (chNum & 0x3F));
        }
        else
        {
            dst += (char)(0xF0 | (chNum >> 18));
            dst += (char)(0x80 | ((chNum >> 12) & 0x3F));
            dst += (char)(0x80 | ((chNum >> 6) & 0x3F));
            dst += (char)(0x80 | (chNum & 0x3F));
        }

        return k + 1;
    }
    else if( str_begwith( text, "&lt;" ) )
    {
        dst += '<';
        return 4;
    }
    else if( str_begwith( text, "&gt;" ) )
    {
        dst += '>';
        return 4;
    }
    else if( str_begwith( text, "&amp;" ) )
    {
        dst += '&';
        return 5;
    }
    else if( str_begwith( text, "&apos;" ) )
    {
        dst += '\'';
        return 6;
    }
    else if( str_begwith( text, "&quos;" ) )
    {
        dst += '\"';
        return 6;
    }
    else    // unknow XML Entity, doen NOT support yet
    {   
        int k = 1;
        while( IS_NAMECHAR(text[k]) )
            k++;
        if( text[k] != ';' || k == 1 )  // invalid grammer
            return -1;

        dst.append( text, k + 1 );
        return k + 1;
    }

    return -1;
}

// parse text content of element, return character count comsumed, return -1 if failed
static int parse_xml_content( const char* text, std::string& dst )
{
    int k = 0;
    while( 1 )
    {
        if( !IS_CTRLMARK( text[k] ) )   // plain character
        {
            dst.push_back( text[k] );
            k++;
        }
        else    // control or markup character
        {
            if( text[k] == '<' )
            {
                // find end-tag, child element, comment, etc
                break;
            }
            else if( text[k] == '&' )   // escape character
            {
                int used = unescape_xml_text( text + k, dst );
                if( used <= 0 )
                    return -1;
                k += used;
            }
            else
            {
                if( text[k] < 32 && !IS_SPACE( text[k] ) )  // control char except whitespace is NOT allowed
                    return -1;
                dst.push_back( text[k] );
                k++;
            }
        }
    }

    // remove trailing whitespace
    while( !dst.empty() && IS_SPACE(dst.back()) )
    {
        dst.pop_back();
    }

    assert( text[k] == '<' );
    return k;
}

//======================================================================================================================
XmlAttr::XmlAttr( XmlAlloc* allocator )
{
    assert( allocator != nullptr );
    m_alloc = allocator;
    m_pNext = nullptr;
    m_cvtflag = CVT_FLAG_DEFAULT;
}

XmlAttr::~XmlAttr()
{
    m_name.clear( m_alloc );
    m_value.clear( m_alloc );
}

// set attribute name, must be valid XML Name(this method will NOT check)
void XmlAttr::set_name( const char* text, int len )
{
    m_name.assign( m_alloc, text, len );
}
void XmlAttr::set_name( const string& text )
{
    m_name.assign( m_alloc, text );
}

bool XmlAttr::as_bool() const
{
    if( stricmp(m_value.buf, "true") == 0 )
    {
        return true;
    }
    else if( stricmp(m_value.buf, "false") == 0 )
    {
        return false;
    }
    return this->as_int() != 0;
}

int32_t XmlAttr::as_int() const
{
    return str_to_int( m_value.buf );
}

uint32_t XmlAttr::as_uint() const
{
    return str_to_uint( m_value.buf );
}

int64_t XmlAttr::as_int64() const
{
    return str_to_int64( m_value.buf );
}

uint64_t XmlAttr::as_uint64() const
{
    return str_to_uint64( m_value.buf );
}

float XmlAttr::as_float() const
{
    return (float)strtod( m_value.buf, nullptr );
}

double XmlAttr::as_double() const
{
    return strtod( m_value.buf, nullptr );
}

void XmlAttr::set_value( const char* text, int len )
{
    m_value.assign( m_alloc, text, len );
    m_cvtflag = CVT_FLAG_DEFAULT;
}

void XmlAttr::set_value( const string& text )
{
    m_value.assign( m_alloc, text );
    m_cvtflag = CVT_FLAG_DEFAULT;
}

void XmlAttr::set_value( bool val )
{
    if( val )
        m_value.assign( m_alloc, "true", 4 );
    else
        m_value.assign( m_alloc, "false", 5 );
    m_cvtflag = CVT_FLAG_NOCVT;
}

void XmlAttr::set_value( int32_t val )
{
    m_value.alloc( m_alloc, 16 );
    m_value.len = int_to_str( val, m_value.buf, m_value.cap );
    m_cvtflag = CVT_FLAG_NOCVT;
}

void XmlAttr::set_value( uint32_t val )
{
    m_value.alloc( m_alloc, 16 );
    m_value.len = int_to_str( val, m_value.buf, m_value.cap );
    m_cvtflag = CVT_FLAG_NOCVT;
}

void XmlAttr::set_value( int64_t val )
{
    m_value.alloc( m_alloc, 32 );
    m_value.len = int_to_str( val, m_value.buf, m_value.cap );
    m_cvtflag = CVT_FLAG_NOCVT;
}

void XmlAttr::set_value( uint64_t val )
{
    m_value.alloc( m_alloc, 32 );
    m_value.len = int_to_str( val, m_value.buf, m_value.cap );
    m_cvtflag = CVT_FLAG_NOCVT;
}

void XmlAttr::set_value( double val )
{
    char buf[128] = {0};
    int cnt = str_format( buf, 128, "%0.9g", val );
    m_value.assign( m_alloc, buf, cnt );
    m_cvtflag = CVT_FLAG_NOCVT;
}

// format attribute, add to the tail of the output string
void XmlAttr::format( std::string& dst ) const
{
    if( m_name.len > 0 )
    {
        dst.append( m_name.buf, m_name.len );
        dst.append( "=\"", 2 );
        if( m_cvtflag == CVT_FLAG_NOCVT )
            dst.append( m_value.buf, m_value.len );
        else
            escape_xml_string( m_value.buf, dst );
        dst.push_back( '\"' );
    }
}

// parse and fill, return chars consumed, return -1 if failed
int XmlAttr::parse( const char* text )
{
    // parse attribute name
    int k = 0;
    while( IS_NAMECHAR( text[k] ) )
        k++;
    m_name.assign( m_alloc, text, k );

    // skip EQ and space
    while( IS_SPACE( text[k] ) )
        k++;
    if( text[k] != '=' )
        return -1;
    k++;
    while( IS_SPACE( text[k] ) )
        k++;

    // check value delimit, " or '
    const char delimit = text[k];
    if( delimit != '\"' && delimit != '\'' )
        return -1;
    k++;

    // parse attribute value
    std::string& value = m_alloc->temp_string();
    while( text[k] != delimit )
    {
        if( !IS_CTRLMARK( text[k] ) )   // plain character
        {
            value.push_back( text[k] );
            k++;
        }
        else    // control or markup character   
        {
            if( text[k] < 32 )  // control character
            {
                if( IS_SPACE( text[k] ) )   // normalize space
                {
                    value.push_back(' ');
                    k++;
                }
                else
                {
                    // control char except whitespace is NOT allowed
                    return -1;
                }
            }
            else
            {
                if( text[k] == '&' )    // escape or reference
                {
                    int used = unescape_xml_text( text + k, value );
                    if( used <= 0 )
                        return -1;
                    k += used;
                }
                else if( text[k] == '<' )   // invalid
                {
                    return -1;
                }
                else
                {
                    value.push_back( text[k] );
                    k++;
                }
            }
        }
    }
    m_value.assign( m_alloc, value );

    assert( text[k] == delimit );
    return k + 1;
}

//======================================================================================================================
XmlElem::XmlElem( XmlAlloc* allocator )
{
    assert( allocator != nullptr );
    m_alloc = allocator;
    m_parent = nullptr;
    m_pNext = nullptr;
    m_attrCnt = 0;
    m_childCnt = 0;
    m_cvtflag = CVT_FLAG_DEFAULT;
}

XmlElem::XmlElem( XmlElem* parent ) : XmlElem( parent->allocator() )
{
    m_parent = parent;
}

XmlElem::~XmlElem()
{
    this->clear_attrs();
    this->clear_children();
    m_tag.clear( m_alloc );
    m_content.clear( m_alloc );
    m_comment.clear( m_alloc );
}

// set element tag, must be valid XML Name(this method will NOT check)
void XmlElem::set_tag( const char* tag, int len )
{
    m_tag.assign( m_alloc, tag, len );
}
void XmlElem::set_tag( const string& tag )
{
    m_tag.assign( m_alloc, tag );
}

bool XmlElem::as_bool() const
{
    if( stricmp(m_content.buf, "true") == 0 )
    {
        return true;
    }
    else if( stricmp(m_content.buf, "false") == 0 )
    {
        return false;
    }
    return this->as_int() != 0;
}

int32_t XmlElem::as_int() const
{
    return str_to_int( m_content.buf );
}

uint32_t XmlElem::as_uint() const
{
    return str_to_uint( m_content.buf );
}

int64_t XmlElem::as_int64() const
{
    return str_to_int64( m_content.buf );
}

uint64_t XmlElem::as_uint64() const
{
    return str_to_uint64( m_content.buf );
}

float XmlElem::as_float() const
{
    return (float)::strtod( m_content.buf, nullptr );
}

double XmlElem::as_double() const
{
    return ::strtod( m_content.buf, nullptr );
}

void XmlElem::set_content( const char* text, int len )
{
    m_content.assign( m_alloc, text, len );
    m_cvtflag = CVT_FLAG_DEFAULT;
}

void XmlElem::set_content( const string& text )
{
    m_content.assign( m_alloc, text );
    m_cvtflag = CVT_FLAG_DEFAULT;
}

void XmlElem::set_content( bool val )
{
    if( val )
        m_content.assign( m_alloc, "true", 4 );
    else
        m_content.assign( m_alloc, "false", 5 );
    m_cvtflag = CVT_FLAG_NOCVT;
}

void XmlElem::set_content( int32_t val )
{
    m_content.alloc( m_alloc, 16 );
    m_content.len = int_to_str( val, m_content.buf, m_content.cap );
    m_cvtflag = CVT_FLAG_NOCVT;
}

void XmlElem::set_content( uint32_t val )
{
    m_content.alloc( m_alloc, 16 );
    m_content.len = int_to_str( val, m_content.buf, m_content.cap );
    m_cvtflag = CVT_FLAG_NOCVT;
}

void XmlElem::set_content( int64_t val )
{
    m_content.alloc( m_alloc, 32 );
    m_content.len = int_to_str( val, m_content.buf, m_content.cap );
    m_cvtflag = CVT_FLAG_NOCVT;
}

void XmlElem::set_content( uint64_t val )
{
    m_content.alloc( m_alloc, 32 );
    m_content.len = int_to_str( val, m_content.buf, m_content.cap );
    m_cvtflag = CVT_FLAG_NOCVT;
}

void XmlElem::set_content( double val )
{
    char buf[128] = {0};
    int cnt = str_format( buf, 128, "%0.9g", val );
    m_content.assign( m_alloc, buf, cnt );
    m_cvtflag = CVT_FLAG_NOCVT;
}

void XmlElem::set_content_base64( const void* data, int bytes )
{
    const int charCnt = 4 * (bytes/3 + 1);
    m_content.alloc( m_alloc, charCnt );
    m_content.len = encode_base64( (const uint8_t*)data, bytes, m_content.buf, m_content.cap );
    m_cvtflag = CVT_FLAG_NOCVT;
}

void XmlElem::set_content_cdata( const char* text, int len )
{
    m_content.assign( m_alloc, text, len );
    m_cvtflag = CVT_FLAG_CDATA;
}

void XmlElem::set_comment( const char* comm, int len )
{
    m_comment.assign( m_alloc, comm, len );
}
void XmlElem::set_comment( const std::string& comm )
{
    m_comment.assign( m_alloc, comm );
}

// find attribute, use sequencial search
const XmlAttr* XmlElem::find_attr( const char* name ) const
{
    return m_attrList.find( [name](XmlAttr* attr){ return strcmp(attr->name(), name)==0; } );
}
XmlAttr* XmlElem::find_attr( const char* name )
{
    return m_attrList.find( [name](XmlAttr* attr){ return strcmp(attr->name(), name)==0; } );
}

// add dummy attribute, for internal usage
XmlAttr* XmlElem::add_null_attr()
{
    XmlAttr* attr = m_alloc->create<XmlAttr>( m_alloc );
    m_attrList.push_back( attr );
    m_attrCnt++;
    return attr;
}

// remove and destroy attribute
bool XmlElem::erase_attr( const char* name )
{
    XmlAttr* victim = m_attrList.remove( [name](XmlAttr* attr){ return strcmp(attr->name(), name)==0; } );
    if( victim )
    {
        m_alloc->trash( victim );
        m_attrCnt--;
        return true;
    }
    return false;
}

// remove all attributes
void XmlElem::clear_attrs()
{
    XmlAttr* attr = m_attrList.m_firstNode;
    while( attr )
    {
        XmlAttr* next = attr->next_sibling();
        m_alloc->trash( attr );
        attr = next;
    }
    m_attrList.clear();
    m_attrCnt = 0;
}

// find child element, return nullptr if failed, use sequencial search
// if mulitple elements with the same tag exist, return the first one
const XmlElem* XmlElem::find_child( const char* tag ) const
{
    return m_children.find( [tag](XmlElem* e){ return strcmp(e->tag(), tag)==0; } );
}
XmlElem* XmlElem::find_child( const char* tag )
{
    return m_children.find( [tag](XmlElem* e){ return strcmp(e->tag(), tag)==0; } );
}

// add dummy child, for internal usage
XmlElem* XmlElem::add_null_child()
{
    XmlElem* elem = m_alloc->create<XmlElem>( this );
    m_children.push_back( elem );
    m_childCnt++;
    return elem;
}

// remove and destroy child
// if mulitple children with the same tag exist, remove the first one
bool XmlElem::erase_child( const char* tag )
{
    XmlElem* victim = m_children.remove( [tag](XmlElem* e){ return strcmp(e->tag(), tag)==0; } );
    if( victim )
    {
        m_alloc->trash( victim );
        m_childCnt--;
        return true;
    }
    return false;
}

// remove all children
void XmlElem::clear_children()
{
    XmlElem* elem = m_children.m_firstNode;
    while( elem )
    {
        XmlElem* next = elem->next_sibling();
        m_alloc->trash( elem );
        elem = next;
    }
    m_children.clear();
    m_childCnt = 0;
}

// format element, add to the tail of the output string
void XmlElem::format( string& dst ) const
{
    if( m_tag.len <= 0 )
        return;

    // comment
    if( m_comment.len > 0 )
    {
        dst += "<!-- ";
        dst.append( m_comment.buf, m_comment.len );
        dst += " -->\n";
    }

    // start-tag
    dst += '<';
    dst.append( m_tag.buf, m_tag.len );

    // attributes
    const XmlAttr* attr = m_attrList.m_firstNode;
    while( attr )
    {
        dst += ' ';
        attr->format( dst );
        attr = attr->next_sibling();
    }

    // check empty element
    if( m_content.len == 0 && m_childCnt == 0 )
    {
        dst.append( "/>\n", 3 );     // end tag
        return;
    }
    dst += '>';

    // element content
    if( m_content.len > 0 )
    {
        if( m_cvtflag == CVT_FLAG_NOCVT )       // plain text
        {
            dst.append( m_content.buf, m_content.len );
        }
        else if( m_cvtflag == CVT_FLAG_CDATA )  // CDATA 
        {
            dst += "<![CDATA[";
            dst.append( m_content.buf, m_content.len );
            dst += "]]>";
        }
        else    // default, may need escape
        {
            escape_xml_string( m_content.buf, dst );
        }
    }

    // child elements
    if( m_childCnt > 0 )
    {
        dst += '\n';
        const XmlElem* elem = m_children.m_firstNode;
        while( elem )
        {
            elem->format( dst );
            elem = elem->next_sibling();
        }
    }

    // end-tag 
    dst.append( "</", 2 );
    dst.append( m_tag.buf, m_tag.len );
    dst.append( ">\n", 2 );
}

// format element with indent, add to the tail of the output string
void XmlElem::format( std::string & dst, int indent ) const
{
    if( m_tag.len <= 0 )
        return;

    // comment
    if( m_comment.len > 0 )
    {
        dst.append( indent, ' ' );
        dst += "<!-- ";
        dst.append( m_comment.buf, m_comment.len );
        dst += " -->\n";
    }

    // start-tag
    dst.append( indent, ' ' );
    dst += '<';
    dst.append( m_tag.buf, m_tag.len );

    // attributes
    const XmlAttr* attr = m_attrList.m_firstNode;
    while( attr )
    {
        dst += ' ';
        attr->format( dst );
        attr = attr->next_sibling();
    }

    // check empty element
    if( m_content.len == 0 && m_childCnt == 0 )
    {
        dst.append( "/>\n", 3 );     // end tag
        return;
    }
    dst += '>';

    // element content
    if( m_content.len > 0 )
    {
        if( m_cvtflag == CVT_FLAG_NOCVT )       // plain text
        {
            dst.append( m_content.buf, m_content.len );
        }
        else if( m_cvtflag == CVT_FLAG_CDATA )  // CDATA 
        {
            dst += "<![CDATA[";
            dst.append( m_content.buf, m_content.len );
            dst += "]]>";
        }
        else    // default, may need escape
        {
            escape_xml_string( m_content.buf, dst );
        }
    }

    // children elements
    if( m_childCnt > 0 )
    {
        const int indent2 = MIN(indent + 2, XmlDoc::kMaxIndent);
        dst += '\n';

        const XmlElem* elem = m_children.m_firstNode;
        while( elem )
        {
            elem->format( dst, indent2 );
            elem = elem->next_sibling();
        }

        dst.append( indent, ' ' );
    }

    // end-tag
    dst.append( "</", 2 );
    dst.append( m_tag.buf, m_tag.len );
    dst.append( ">\n", 2 );
}

// parse XML text, return characters consumed, return -1 if failed
int XmlElem::parse( const char* text, int depth )
{
    assert( text[0] == '<' );
    if( depth > XmlDoc::kMaxDepth )     // prevent stack overflow
        return -1;

    // parse tag
    int k = 1;
    while( IS_NAMECHAR( text[k] ) )
        k++;
    if( k == 1 )    // invalid name
        return -1;
    m_tag.assign( m_alloc, text + 1, k - 1 );

    // parse attributes
    while( 1 )
    {
        // skip whitespace
        while( IS_SPACE( text[k] ) )
            k++;
        
        if( text[k] == '>' )    // end of start-tag
        {
            k++;
            break;
        }
        else if( text[k] == '/' )
        {
            if( text[k+1] == '>' )  // empty element
                return k + 2;
            else
                return -1;
        }
        else    // attribute
        {
            if( !IS_NAMECHAR( text[k] ) )
                return -1;
            XmlAttr* attr = this->add_null_attr();
            int used = attr->parse( text + k );
            if( used <= 0 )
                return -1;
            k += used;
        }
    }

    // parse content and child elements
    while( 1 )
    {
        // skip whitespace
        while( IS_SPACE( text[k] ) )
        {
            k++;
        }

        if( text[k] == '<' )
        {
            if( text[k+1] == '/' )      // end-tag
            {
                k += 2;
                break;
            }
            else if( text[k+1] == '!' )
            {
                if( text[k+2] == '-' && text[k+3] == '-' )      // comment
                {
                    const char* end = strstr( text + k + 4, "-->" );
                    if( !end )
                        return -1;
                    k = (int)(end - text) + 3;
                }
                else if( str_begwith( text + k + 2, "[CDATA[" ) )   // CDATA
                {
                    const char* cdata = text + k + 9;
                    const char* end = strstr( cdata, "]]>" );
                    if( !end )
                        return -1;
                    k = (int)(end - text) + 3;
                    m_content.append( m_alloc, cdata, (int)(end - cdata) );
                }
                else    // DTD is not allowed in element content
                {
                    return -1;
                }
            }
            else if( text[k+1] == '?' ) // processing instructions
            {
                const char* end = strstr( text + k + 2, "?>" );
                if( !end )
                    return -1;
                k = (int)(end - text) + 2;
            }
            else    // child element
            {
                XmlElem* child = this->add_null_child();
                int used = child->parse( text + k, depth + 1 );
                if( used <= 0 )
                    return -1;
                k += used;
            }
        }
        else    // element content
        {
            std::string& temp = m_alloc->temp_string();
            int used = parse_xml_content( text + k, temp );
            if( used <= 0 )
                return used;
            k += used;
            m_content.append( m_alloc, temp );
        }
    }

    // end-tag should match start-tag
    if( strncmp( text + k, m_tag.buf, m_tag.len ) != 0 )
        return -1;
    k += m_tag.len;

    // skip space
    while( IS_SPACE( text[k] ) )
        k++;
    if( text[k] != '>' )
        return -1;

    return k + 1;
}

//======================================================================================================================
XmlDoc::XmlDoc() : m_allocator(nullptr), m_root(nullptr)
{
    m_allocator = new XmlAlloc;
}

XmlDoc::~XmlDoc()
{
    if( m_root )
    {
        m_allocator->trash( m_root );
        m_root = nullptr;
    }
    delete m_allocator;
}

XmlStatus XmlDoc::parse_file( CFile& file )
{
    if( !file  )
        return XmlStatus::IOFailed;

    // check file size
    const int64_t filesz = file.file_size();
    if( filesz < 0 )
        return XmlStatus::IOFailed;
    else if( filesz > 128 * 1024 * 1024 )   // this utility is not for huge file!
        return XmlStatus::Unsupported;
    
    // read file into temp memory, add padding
    MallocedBuf mBuf;
    uint8_t* text = (uint8_t*)mBuf.alloc( (int)filesz + 4 );    // must use uint8_t to compare to BOM
    size_t readcnt = fread( text, 1, filesz, file );
    if( readcnt == 0 )
        return XmlStatus::IOFailed;
    text[readcnt] = 0;
    text[readcnt+1] = 0;

    // check BOM, has padding, so always safe
    if( text[0]==0xEF && text[1]==0xBB && text[2]==0xBF )   // UTF-8
    {
        text += 3;      // skip BOM
    }
    else if( text[0]==0xFF && text[1]==0xFE )   // UTF-16, little-endian
    {
        return XmlStatus::Unsupported;
    }
    else if( text[0]==0xFE && text[1]==0xFF )   // UTF-16, big-endian
    {
        return XmlStatus::Unsupported;
    }

    // parse XML text
    return this->parse_text( (char*)text );
}

XmlStatus XmlDoc::write_file( CFile& file, const std::string& xmlstr )
{
    if( file )
    {
        if( fprintf( file, "%s", xmlstr.c_str() ) > 0 )
            return XmlStatus::Ok;
    }
    return XmlStatus::IOFailed;
}

XmlStatus XmlDoc::parse_file( const char* filepath )
{
    CFile file( filepath, "r" );
    return this->parse_file( file );
}

XmlStatus XmlDoc::parse_text( const char* text )
{
    // delete old document(if exists)
    if( m_root )
    {
        m_allocator->trash( m_root );
        m_root = nullptr;
    }

    // skip leading whitespace
    const char* s = text;
    while( IS_SPACE(*s) )
        s++;
    if( *s != '<' )
        return XmlStatus::Invalid;

    // skip XML prolog(declaration, DTD, comment, etc)
    while( 1 )
    {
        assert( s[0] == '<' );

        if( s[1] == '?' )       // XML declaration, processing instructions
        {
            const char* t = strstr( s + 2, "?>" );
            if( !t )
                return XmlStatus::Invalid;
            s = t + 2;
        }
        else if( s[1] == '!' )  // DTD or comment
        {
            if( s[2] == '-' && s[3] == '-' )    // comment
            {
                const char* t = strstr( s + 4, "-->" );
                if( !t )
                    return XmlStatus::Invalid;
                s = t + 3;
            }
            else    // DTD is NOT supported, just skip
            {
                s += 2;

                if( str_begwith( s, "DOCTYPE" ) )
                    s += lengthof( "DOCTYPE" );
                else if( str_begwith( s, "ELEMENT" ) )
                    s += lengthof( "ELEMENT" );
                else if( str_begwith( s, "ENTITY" ) )
                    s += lengthof( "ENTITY" );
                else if( str_begwith( s, "ATTLIST" ) )
                    s += lengthof( "ATTLIST" );
                else if( str_begwith( s, "NOTATION" ) )
                    s += lengthof( "NOTATION" );
                else
                    return XmlStatus::Invalid;
            }
        }
        else // find root element
        {
            break;
        }

        // find next '<'
        while( *s != '<' && *s != 0 )
            s++;
        if( *s == 0 )
            return XmlStatus::Invalid;
    }

    assert( !m_root );
    m_root = m_allocator->create<XmlElem>( m_allocator );
    if( m_root->parse( s, 0 ) < 0 )
    {
        m_allocator->trash( m_root );
        m_root = nullptr;
        return XmlStatus::Invalid;
    }

    return XmlStatus::Ok;
}

XmlElem* XmlDoc::create_xml( const char* rootTag )
{
    // delete old document(if exists)
    if( m_root )
    {
        m_allocator->trash( m_root );
        m_root = nullptr;
    }

    m_root = m_allocator->create<XmlElem>( m_allocator );
    m_root->set_tag( rootTag );
    return m_root;
}

XmlStatus XmlDoc::dump_file( const char* filename )
{
    string tmpstr;
    auto res = this->dump_text( tmpstr );
    if( res != XmlStatus::Ok )
        return res;

    CFile file( filename, "w" );
    return this->write_file( file, tmpstr );
}

XmlStatus XmlDoc::dump_text( std::string& out )
{
    if( !m_root )
        return XmlStatus::Invalid;

    out.clear();
    out.reserve( 4096 );
    out = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    m_root->format( out );

    return XmlStatus::Ok;
}

XmlStatus XmlDoc::pretty_dump_file( const char* filename )
{
    string tmpstr;
    auto res = this->pretty_dump_text( tmpstr );
    if( res != XmlStatus::Ok )
        return res;

    CFile file( filename, "w" );
    return this->write_file( file, tmpstr );
}

XmlStatus XmlDoc::pretty_dump_text( std::string& out )
{
    if( !m_root )
        return XmlStatus::Invalid;

    out.clear();
    out.reserve( 4096 );
    out = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    m_root->format( out, 0 );

    return XmlStatus::Ok;
}

#ifdef _WIN32

XmlStatus XmlDoc::parse_file( const wchar_t* filepath )
{
    CFile file( filepath, L"r" );
    return this->parse_file( file );
}

XmlStatus XmlDoc::dump_file( const wchar_t* filename )
{
    string tmpstr;
    auto res = this->dump_text( tmpstr );
    if( res != XmlStatus::Ok )
        return res;

    CFile file( filename, L"w" );
    return this->write_file( file, tmpstr );
}

XmlStatus XmlDoc::pretty_dump_file( const wchar_t* filename )
{
    string tmpstr;
    auto res = this->pretty_dump_text( tmpstr );
    if( res != XmlStatus::Ok )
        return res;

    CFile file( filename, L"w" );
    return this->write_file( file, tmpstr );
}

#endif

} // namespace irk
