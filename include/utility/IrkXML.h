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

#ifndef _IRONBRICK_XML_H_
#define _IRONBRICK_XML_H_

#include <string>
#include <type_traits>
#include "IrkCommon.h"
#include "detail/ImplJsonXml.hpp"

//
// Simple reader + writer for small XML file
// NOTE: Only support UTF-8 encoding, does NOT support DTD
//
// All whitespaces in attribute value will be converted to space(0x20) (according to the specification)
// The beginging and trailing whitespaces in element content will be discarded
//

namespace irk {

class CFile;

class XmlDoc;       // XML Document, created by user
class XmlAttr;      // XML Attribute
class XmlElem;      // XML Element
class XmlAlloc;     // XML memory allocator, for internal usage

namespace detail {

// XML string, for internal usage
struct XmlStr : IrkNocopy
{
    XmlStr() : buf((char*)""), cap(0), len(0)
    {}
    ~XmlStr() 
    { 
        assert( this->cap == 0 );   // already cleared
    }
    XmlStr( XmlStr&& other ) noexcept
    {
        assert( this != &other );
        this->buf = other.buf;
        this->cap = other.cap;
        this->len = other.len;
        other.buf = (char*)"";
        other.cap = 0;
        other.len = 0;
    }

    void alloc( XmlAlloc*, int capacity );
    void clear( XmlAlloc* );
    void reserve( XmlAlloc*, int capacity );
    void assign( XmlAlloc*, const char* src, int len = -1 );
    void assign( XmlAlloc*, const std::string& src );
    void append( XmlAlloc*, const char* src, int len = -1 );
    void append( XmlAlloc*, const std::string& src );

    char*   buf;    // string buffer
    int     cap;    // buffer capacity
    int     len;    // string length
};

}   // namespace detail

//======================================================================================================================

// XML Attribute
class XmlAttr : IrkNocopy
{
public:
    // get attribute name
    const char* name() const            { return m_name.buf; }
    const char* name( int& len) const   { len = m_name.len; return m_name.buf; }

    // set attribute name, must be valid XML Name(this method will NOT check)
    void set_name( const char* text, int len = -1 );
    void set_name( const std::string& );

    // get attribute value
    const char* value() const           { return m_value.buf; }
    const char* value( int& len ) const { len = m_value.len; return m_value.buf; }
    bool        as_bool() const;
    int32_t     as_int() const;
    uint32_t    as_uint() const;
    int64_t     as_int64() const;
    uint64_t    as_uint64() const;
    float       as_float() const;
    double      as_double() const;

    // set attribute value, if len < 0 string must be null-terminated
    void        set_value( const char* text, int len = -1 );
    void        set_value( const std::string& );
    void        set_value( bool val );
    void        set_value( int32_t val );
    void        set_value( uint32_t val );
    void        set_value( int64_t val );
    void        set_value( uint64_t val );
    void        set_value( double val );

    // get next sibling
    const XmlAttr* next_sibling() const { return m_pNext; }
    XmlAttr* next_sibling()             { return m_pNext; }

    // format attribute, add to the tail of the output string, for internal use
    void format( std::string& outStr ) const;

    // parse XML text, return characters consumed, for internal use
    int parse( const char* text );

private:
    friend XmlAlloc;
    friend detail::SList<XmlAttr>;
    explicit XmlAttr( XmlAlloc* allocator );
    ~XmlAttr();

    XmlAlloc*       m_alloc;    // memory allocator
    XmlAttr*        m_pNext;    // next sibling in containing element
    detail::XmlStr  m_name;
    detail::XmlStr  m_value;
    int             m_cvtflag;
};

// XML Element
class XmlElem : IrkNocopy
{
    // A simple class used to traverse attributes in the element
    class AttrRange
    {
    public:
        explicit AttrRange( const detail::SList<XmlAttr>* plist ) : m_atlist(plist) {}
        typedef detail::JXIterator<XmlAttr> Iterator;
        typedef detail::JXIterator<const XmlAttr> CIterator;
        Iterator begin()        { return Iterator(m_atlist->m_firstNode); }
        Iterator end()          { return Iterator(nullptr); }
        CIterator begin() const { return CIterator(m_atlist->m_firstNode); }
        CIterator end() const   { return CIterator(nullptr); }
    private:
        const detail::SList<XmlAttr>* m_atlist;
    };
public:
    // get element tag
    const char* tag() const             { return m_tag.buf; }
    const char* tag( int& len )         { len = m_tag.len; return m_tag.buf; }

    // set element tag, must be valid XML Name(this method will NOT check)
    void set_tag( const char* tag, int len = -1 );
    void set_tag( const std::string& tag );

    // get element content
    const char* content() const         { return m_content.buf; }
    const char* content(int& len) const { len = m_content.len; return m_content.buf; }
    bool        as_bool() const;
    int32_t     as_int() const;
    uint32_t    as_uint() const;
    int64_t     as_int64() const;
    uint64_t    as_uint64() const;
    float       as_float() const;
    double      as_double() const;

    // set element content, if len < 0 string must be null-terminated
    void        set_content( const char* text, int len = -1 );
    void        set_content( const std::string& );
    void        set_content( bool val );
    void        set_content( int32_t val );
    void        set_content( uint32_t val );
    void        set_content( int64_t val );
    void        set_content( uint64_t val );
    void        set_content( double val );

    // convert binary data to Base64 text and set as content
    void        set_content_base64( const void* data, int bytes );

    // set CDATA content, text can NOT contains "]]>", if len < 0 text must be null-terminated
    void        set_content_cdata( const char* text, int len = -1 );

    // get comment
    const char* comment() const         { return m_comment.buf; }
    const char* comment(int& len) const { len = m_comment.len; return m_comment.buf; }

    // set comment
    void set_comment( const char* comm, int len = -1 );
    void set_comment( const std::string& comm );

    // attributes traversal
    AttrRange   attr_range() const      { return AttrRange( &m_attrList ); }

    // attributes count
    int attr_count() const              { return m_attrCnt; }

    // the first attribute
    const XmlAttr* first_attr() const   { return m_attrList.m_firstNode; }
    XmlAttr* first_attr()               { return m_attrList.m_firstNode; }
    // the last attribute
    const XmlAttr* last_attr() const    { return m_attrList.m_lastNode; }
    XmlAttr* last_attr()                { return m_attrList.m_lastNode; }

    // find attribute, return nullptr if failed
    // NOTE: using sequencial search
    const XmlAttr* find_attr( const char* name ) const;
    XmlAttr* find_attr( const char* name );

    // add attribute, attribute name must be unique, must be valid XML Name
    XmlAttr* add_attr( const char* name, const char* value = nullptr )
    {
        XmlAttr* attr = this->add_null_attr();
        attr->set_name( name );
        if( value != nullptr )
            attr->set_value( value );
        return attr;
    }
    XmlAttr* add_attr( const std::string& name, const std::string& value = std::string() )
    {
        XmlAttr* attr = this->add_null_attr();
        attr->set_name( name );
        if( !value.empty() )
            attr->set_value( value );
        return attr;
    }
    // convenient method used to add numeric attribute 
    template<typename Ty>
    XmlAttr* add_attr( const char* name, Ty value )
    {
        static_assert( std::is_arithmetic<Ty>::value, "requires numeric" );
        XmlAttr* attr = this->add_null_attr();
        attr->set_name( name );
        attr->set_value( value );
        return attr;
    }
    template<typename Ty>
    XmlAttr* add_attr( const std::string& name, Ty value )
    {
        static_assert( std::is_arithmetic<Ty>::value, "requires numeric" );
        XmlAttr* attr = this->add_null_attr();
        attr->set_name( name );
        attr->set_value( value );
        return attr;
    }

    // remove and destroy attribute
    bool erase_attr( const char* name );

    // remove and destroy all attributes
    void clear_attrs();

    // child elements traversal
    typedef detail::JXIterator<XmlElem> Iterator;
    typedef detail::JXIterator<const XmlElem> CIterator;
    Iterator begin()                    { return Iterator(m_children.m_firstNode); }
    Iterator end()                      { return Iterator(nullptr); }
    CIterator begin() const             { return CIterator(m_children.m_firstNode); }
    CIterator end() const               { return CIterator(nullptr); }

    // children count
    int child_count() const             { return m_childCnt; }

    // the first child
    const XmlElem* first_child() const  { return m_children.m_firstNode; }
    XmlElem* first_child()              { return m_children.m_firstNode; }
    // the last child
    const XmlElem* last_child() const   { return m_children.m_lastNode; }
    XmlElem* last_child()               { return m_children.m_lastNode; }

    // find child element, return nullptr if failed
    // NOTE: use sequencial search
    // NOTE: if mulitple children with the same tag exist, return the first one
    const XmlElem* find_child( const char* tag ) const;
    XmlElem* find_child( const char* tag );

    // add child
    XmlElem* add_child( const char* tag, const char* content = nullptr )
    {
        XmlElem* elem = this->add_null_child();
        elem->set_tag( tag );
        if( content != nullptr )
            elem->set_content( content );
        return elem;
    }
    XmlElem* add_child( const std::string& tag, const std::string& content = std::string() )
    {
        XmlElem* elem = this->add_null_child();
        elem->set_tag( tag );
        if( !content.empty() )
            elem->set_content( content );
        return elem;
    }
    // convenient method used to add numeric child
    template<typename Ty>
    XmlElem* add_child( const char* tag, Ty val )
    {
        static_assert( std::is_arithmetic<Ty>::value, "requires numeric" );
        XmlElem* elem = this->add_null_child();
        elem->set_tag( tag );
        elem->set_content( val );
        return elem;
    }
    template<typename Ty>
    XmlElem* add_child( const std::string& tag, Ty val )
    {
        static_assert( std::is_arithmetic<Ty>::value, "requires numeric" );
        XmlElem* elem = this->add_null_child();
        elem->set_tag( tag );
        elem->set_content( val );
        return elem;
    }

    // remove and destroy child element
    // NOTE: if mulitple children with the same tag exist, remove the first one
    bool erase_child( const char* tag );

    // remove and destroy all children
    void clear_children();

    // get next sibling
    const XmlElem* next_sibling() const { return m_pNext; }
    XmlElem* next_sibling()             { return m_pNext; }

    // format element, add to the tail of the output string, for internal use
    void format( std::string& outStr ) const;
    void format( std::string& outStr, int indent ) const;

    // parse XML text, return characters consumed, for internal use
    int parse( const char* text, int depth );

    // get internal allocator
    XmlAlloc* allocator()   { return m_alloc; }
private:
    friend XmlAlloc;
    friend detail::SList<XmlElem>;

    explicit XmlElem( XmlAlloc* allocator );
    explicit XmlElem( XmlElem* parent );
    ~XmlElem();

    XmlAttr* add_null_attr();
    XmlElem* add_null_child();

    XmlAlloc*               m_alloc;        // memory allocator
    XmlElem*                m_parent;       // parent element
    XmlElem*                m_pNext;        // next sibling in parent element
    detail::XmlStr          m_tag;          // element tag
    detail::XmlStr          m_content;      // element content
    detail::XmlStr          m_comment;      // element comment
    int                     m_cvtflag;
    detail::SList<XmlAttr>  m_attrList;     // list of attributes
    int                     m_attrCnt;
    detail::SList<XmlElem>  m_children;     // list of child elements
    int                     m_childCnt;
};

// XML parsing status
enum class XmlStatus
{
    Ok = 0,         // No Error
    Invalid,        // invalid XML content or syntax
    IOFailed,       // file read/write failed
    Unsupported,    // unsupported (such as UTF-16 encoding)
};

class XmlDoc : IrkNocopy
{
public:
    static const int kMaxDepth = 80;    // max depth to prevent stack overflow
    static const int kMaxIndent = 40;   // max indent when print pretty

    XmlDoc();
    ~XmlDoc();

    // parse XML file
    XmlStatus parse_file( const char* filepath );

    // parse XML text, text must be null terminated
    XmlStatus parse_text( const char* text );

    // is XML document valid/parsed
    explicit operator bool() const  { return m_root != nullptr; }

    // return the root element
    const XmlElem* root() const     { return m_root; }
    XmlElem* root()                 { return m_root; }

    // create an empty XML, return the root element, old doc will be destroyed(if exists)
    XmlElem* create_xml( const char* rootTag );

    // dump XML text as UTF-8 file
    XmlStatus dump_file( const char* filename );
    
    // dump XML text as UTF-8 string
    XmlStatus dump_text( std::string& out );

    // ditto, slower but indented nicely
    XmlStatus pretty_dump_file( const char* filename );
    XmlStatus pretty_dump_text( std::string& out );

#ifdef _WIN32
    XmlStatus parse_file( const wchar_t* filepath );
    XmlStatus dump_file( const wchar_t* filename );
    XmlStatus pretty_dump_file( const wchar_t* filename );
#endif

private:
    XmlStatus parse_file( CFile& );
    XmlStatus write_file( CFile&, const std::string& );
    XmlAlloc*   m_allocator;
    XmlElem*    m_root;
};

} // namespace irk
#endif
