/*
* This Source Code Form is subject to the terms of the Mozilla Public License Version 2.0.
* If a copy of the MPL was not distributed with this file, 
* You can obtain one at http://mozilla.org/MPL/2.0/.

* Covered Software is provided on an ¡°as is¡± basis, 
* without warranty of any kind, either expressed, implied, or statutory,
* that the Covered Software is free of defects, merchantable, 
* fit for a particular purpose or non-infringing.
 
* Copyright (c) Wei Dongliang <illigle@163.com>.
*/

#ifndef _IRONBRICK_JSON_H_
#define _IRONBRICK_JSON_H_

#include <string>
#include <utility>
#include <tuple>
#include "IrkCommon.h"
#include "detail/ImplJsonXml.hpp"

//
// Parser + Writer for small JSON file, according to RFC-7159
//
namespace irk {

class CFile;

class JsonDoc;      // JSON Document, created by user
class JsonValue;    // JSON Value
class JsonElem;     // JSON Value in JSON Array
class JsonArray;    // JSON Array
class JsonMember;   // JSON named Value in JSON Object
class JsonObject;   // JSON Object
class JsonString;   // internal string type
class JsonAlloc;    // internal memory allocator

// JSON empty Object, for placeholder only
struct JsonEmptyObject
{};
// JSON empty Array, for placeholder only
struct JsonEmptyArray
{};

// Expection throwed if access JSON value incorrectly
class JsonBadAccess : public std::logic_error
{  
public:
    JsonBadAccess() : std::logic_error( "Json bad access" ) {}
};

//======================================================================================================================
// JSON value, may be JSON Null, Boolean, String, Number, Array, Object

class JsonValue : IrkNocopy
{
public:
    // type query
    bool is_null() const	{ return m_type == Json_null; }
    bool is_bool() const	{ return m_type == Json_boolean; }
    bool is_number() const	{ return m_type == Json_int64 || m_type == Json_uint64 || m_type == Json_double; }
    bool is_string() const	{ return m_type == Json_string; }
    bool is_object() const	{ return m_type == Json_object; }
    bool is_array() const	{ return m_type == Json_array; }
    bool is_valid() const   { return m_type != Json_invalid; }

    // get value, if type does not match, throw JsonBadAccess
    bool                as_bool() const;
    int32_t             as_int() const;
    uint32_t            as_uint() const;
    int64_t             as_int64() const;
    uint64_t            as_uint64() const;
    float               as_float() const;
    double              as_double() const;
    const char*         as_string() const;
    const JsonObject&   as_object() const;
    JsonObject&			as_object();
    const JsonArray&	as_array() const;   
    JsonArray&          as_array();

    // ditto, but return false if type does not match, never throw
    bool get( bool& outFlag ) const;
    bool get( int32_t& outValue ) const;
    bool get( uint32_t& outValue ) const;
    bool get( int64_t& outValue ) const;
    bool get( uint64_t& outValue ) const;
    bool get( float& outValue ) const;
    bool get( double& outValue ) const;
    bool get( std::string& outStr ) const;
    bool get( const JsonObject*& outObj ) const;
    bool get( JsonObject*& outObj );
    bool get( const JsonArray*& outAry ) const;
    bool get( JsonArray*& outAry );

    // set new value
    void set( std::nullptr_t )
    {
        this->clear();
        m_type = Json_null;
    }
    void set( bool flag )
    {
        this->clear();
        m_type = Json_boolean;
        m_value.flag = flag;
    }
    void set( int value )
    {
        this->clear();
        m_type = Json_int64;
        m_value.i64 = value;
    }
    void set( unsigned int value )
    {
        this->clear();
        m_type = Json_uint64;
        m_value.u64 = value;
    }
    void set( int64_t value )
    {
        this->clear();
        m_type = Json_int64;
        m_value.i64 = value;
    }
    void set( uint64_t value )
    {
        this->clear();
        m_type = Json_uint64;
        m_value.u64 = value;
    }
    void set( double value )
    {
        this->clear();
        m_type = Json_double;
        m_value.f64 = value;
    }

    // set string, string must be null-terminated if len < 0
    void set( const char* cstr, int len = -1 );
    void set( const std::string& str );
    
    // set empty JSON Object, return reference to the Object
    JsonObject& set( JsonEmptyObject )
    {
        return this->make_empty_object();
    }

    // set empty JSON Array, return reference to the Array
    JsonArray& set( JsonEmptyArray )
    {
        return this->make_empty_array();
    }

    // ditto, for convenience
    template<typename Ty> 
    void operator=( const Ty& val )
    {
        this->set( val );
    }

    // make as empty JSON String
    JsonString& make_empty_string();

    // make as empty JSON Object
    JsonObject& make_empty_object();

    // make as empty JSON Array
    JsonArray& make_empty_array();

    // clear self, set to invalid state
    void clear();

    // format to JSON text, add to the tail of the output string, for internal use
    void format( std::string& );
    void format( std::string&, int );

    // parse and fill from JSON text, for internal use
    int parse( const char*, int );
    int parse_number( const char* );

protected:
    enum
    {
        Json_invalid = 0,   // Invalid tag, for internal usage
        Json_null,          // JSON null type
        Json_boolean,       // JSON Boolean
        Json_int64,         // JSON Number
        Json_uint64,
        Json_double,
        Json_scalar = 100,  // Scalar tag, for internal usage
        Json_string,        // JSON String
        Json_array,         // JSON Array
        Json_object,	    // JSON Object
    };
    union ValUnion
    {
        bool        flag;
        int64_t     i64;
        uint64_t    u64;
        double      f64;
        JsonString* str;
        JsonArray*  ary;
        JsonObject*	obj;
    };
    int		    m_type;
    ValUnion    m_value;
    JsonAlloc*  m_allocator;    // internal memory allocator

    friend JsonDoc;

    // protected constructor, can only be created by derived class or JsonDoc
    JsonValue() : m_type(Json_invalid), m_allocator(nullptr)
    {}
    JsonValue( std::nullptr_t, JsonAlloc* alloc ) : m_type(Json_null), m_allocator(alloc)
    {}
    ~JsonValue() // no need to be virtual, outsider can't delete directly
    {
        this->clear();
    }
    JsonValue( JsonValue&& ) noexcept;
    JsonValue& operator=( JsonValue&& ) noexcept;
};

//======================================================================================================================

// JSON value in JSON Array, element of the array
class JsonElem : public JsonValue
{
public:
    // NOTE: use JsonValue's public interface to get/set value
    using JsonValue::operator=;

    // get next element in parent JSON Array
    const JsonElem* next_sibling() const    { return m_pNext; }
    JsonElem* next_sibling()                { return m_pNext; }

    // get parent JSON Array
    const JsonArray* parent() const	        { return m_parent; }
    JsonArray* parent()						{ return m_parent; }

private:
    friend JsonAlloc;
    friend detail::SList<JsonElem>;

    // no public constructor, must be created by parent JSON Array 
    explicit JsonElem( JsonArray* parent );
    ~JsonElem() {}

    JsonElem*   m_pNext;    // next element in parent JSON Array
    JsonArray*  m_parent;   // parent JSON Array
};

// JSON Array
class JsonArray : IrkNocopy
{
public:
    // children traversal
    typedef detail::JXIterator<JsonElem> Iterator;          // forward iterator
    typedef detail::JXIterator<const JsonElem> CIterator;   // forward const iterator
    Iterator begin()	            { return Iterator(m_slist.m_firstNode); }
    Iterator end()			        { return Iterator(nullptr); }
    CIterator begin() const	        { return CIterator(m_slist.m_firstNode); }
    CIterator end() const	        { return CIterator(nullptr); }

    // number of elements
    int count() const		        { return m_count; }

    // return the first element, return nullptr if array is empty 
    const JsonElem* first() const   { return m_slist.m_firstNode; }
    JsonElem* first()               { return m_slist.m_firstNode; }

    // return the last element, return nullptr if array is empty 
    const JsonElem* last() const    { return m_slist.m_lastNode; }
    JsonElem* last()                { return m_slist.m_lastNode; }

    // find idx'th element, return nullptr if failed
    // WARNING: use sequential search, for traversal using iterator instead.
    JsonElem* find( int idx );
    const JsonElem* find( int idx ) const;

    // add a null element to the tail of array, return reference to the new element
    JsonElem& add( std::nullptr_t );

    // add a scalar value of array, return reference to the new element
    template<typename Ty>
    JsonElem& add( const Ty& val )
    {
        JsonElem& elem = this->add( nullptr );
        elem.set( val );
        return elem;
    }
    // add an empty JSON Object, return reference to the new Object
    JsonObject& add( JsonEmptyObject )
    {
        JsonElem& elem = this->add( nullptr );
        return elem.make_empty_object();
    }
    // add an empty JSON Array, return reference to the new Array
    JsonArray& add( JsonEmptyArray )
    {
        JsonElem& elem = this->add( nullptr );
        return elem.make_empty_array();
    }

    // ditto, for convenience
    template<typename Ty>
    void operator+=( const Ty& val )
    {
        this->add( val );
    }

    // add sequence of elements
    template<typename... Ts>
    void adds( Ts... values )
    {
#if __cplusplus >= 201703L
        (this->add(values), ...);
#else
        char dummy[] = { ((void)this->add(values), '0')... };
        (void)dummy;
#endif
    }
    template<typename... Ts>
    void operator+=( const std::tuple<Ts...>& tup )
    {
        this->add_tuple( tup, std::make_index_sequence<sizeof...(Ts)>() );
    }

    // erase element from the array, return false if failed
    // WARNING: O(N) sequential search
    bool erase( int idx );
    bool erase( JsonElem& );

    // delete all elements
    void clear();

    // format JSON text, add to the tail of the output string, for internal use
    void format( std::string& outStr );
    void format( std::string& outStr, int indent );

    // parse and fill from JSON text, for internal use
    int parse( const char*, int );

    // return allocator, for internal use
    JsonAlloc* allocator()  { return m_allocator; }

private:
    // can not create directly, must be created by parent or JSON Document
    friend JsonAlloc;
    explicit JsonArray(JsonAlloc* alloc) : m_count(0), m_allocator(alloc) {}
    ~JsonArray() { this->clear(); }

    template<typename... Ts, size_t... Is>
    void add_tuple( const std::tuple<Ts...>& tup, std::index_sequence<Is...> )
    {
#if __cplusplus >= 201703L
        (this->add( std::get<Is>(tup) ), ...);
#else
        char dummy[] = { ((void)this->add( std::get<Is>(tup) ), '0')... };
        (void)dummy;
#endif
    }

    int         m_count;                // number of children
    JsonAlloc*  m_allocator;            // memory allocator
    detail::SList<JsonElem> m_slist;    // elements list
};

//======================================================================================================================

// JSON named Value in JSON Object, member in the Object
class JsonMember : public JsonValue
{
public:
    // NOTE: use JsonValue's public interface to get/set value
    using JsonValue::operator=;

    // get/set member's name
    const char* name() const;
    void set_name( const char* name );
    void set_name( const std::string& name );

    // get next member in parent JSON Object
    const JsonMember* next_sibling() const	{ return m_pNext; }
    JsonMember* next_sibling()				{ return m_pNext; }

    // get parent JSON Object
    const JsonObject* parent() const	    { return m_parent; }
    JsonObject* parent()				    { return m_parent; }

    // format JSON text, add to the tail of the output string, for internal use
    void format( std::string& );
    void format( std::string&, int );

    // parse and fill from JSON text, for internal use
    int parse( const char*, int );

private:
    friend JsonAlloc;
    friend detail::SList<JsonMember>;

    // no public constructor, must be created by parent JSON Object
    explicit JsonMember( JsonObject* parent );
    JsonMember( const char* name, JsonObject* parent );
    JsonMember( const std::string& name, JsonObject* parent );
    ~JsonMember();

    JsonString*	m_pName;    // member name
    JsonMember*	m_pNext;    // next member in parent JSON Object
    JsonObject*	m_parent;   // parent JSON Object
};

// JSON Object
class JsonObject : IrkNocopy
{
public:
    // children traversal
    typedef detail::JXIterator<JsonMember> Iterator;        // forward iterator
    typedef detail::JXIterator<const JsonMember> CIterator; // forward const iterator
    Iterator begin()                { return Iterator(m_slist.m_firstNode); }
    Iterator end()                  { return Iterator(nullptr); }
    CIterator begin() const			{ return CIterator(m_slist.m_firstNode); }
    CIterator end() const			{ return CIterator(nullptr); }

    // number of members
    int count() const			    { return m_count; }

    // return the first member, return nullptr if empty 
    const JsonMember* first() const	{ return m_slist.m_firstNode; }
    JsonMember* first()				{ return m_slist.m_firstNode; }

    // return the last member, return nullptr if empty 
    const JsonMember* last() const	{ return m_slist.m_lastNode; }
    JsonMember* last()				{ return m_slist.m_lastNode; }

    // find the desired member, return nullptr if failed
    // WARNING: if duplicate name exists, only return the first one.
    // WARNING: use sequential search, for traversal using iterator instead.
    JsonMember* find( const char* name );
    JsonMember* find( const std::string& name );
    const JsonMember* find( const char* name ) const;
    const JsonMember* find( const std::string& name ) const;

    // add null member to the obj, return reference to the new member
    JsonMember& add( const char* name, std::nullptr_t );
    JsonMember& add( const std::string& name, std::nullptr_t );

    // add scalar member to the object, return reference to the new member
    template<typename Ty>
    JsonMember& add( const char* name, const Ty& val )
    {
        JsonMember& member = this->add( name, nullptr );
        member.set( val );
        return member;
    }
    template<typename Ty>
    JsonMember& add( const std::string& name, const Ty& val )
    {
        JsonMember& member = this->add( name, nullptr );
        member.set( val );
        return member;
    }

    // add empty JSON Object, return reference to the new Object
    JsonObject& add( const char* name, JsonEmptyObject )
    {
        JsonMember& member = this->add( name, nullptr );
        return member.make_empty_object();
    }
    JsonObject& add( const std::string& name, JsonEmptyObject )
    {
        JsonMember& member = this->add( name, nullptr );
        return member.make_empty_object();
    }

    // add empty JSON Array, return reference to the new Array
    JsonArray& add( const char* name, JsonEmptyArray )
    {
        JsonMember& member = this->add( name, nullptr );
        return member.make_empty_array();
    }
    JsonArray& add( const std::string& name, JsonEmptyArray )
    {
        JsonMember& member = this->add( name, nullptr );
        return member.make_empty_array();
    }

    // if member exist, return the member, else add new member
    JsonMember& operator[]( const char* name )
    {
        JsonMember* pmemb = this->find( name );
        if( pmemb )
            return *pmemb;
        return this->add( name, nullptr );
    }
    JsonMember& operator[]( const std::string& name )
    {
        JsonMember* pmemb = this->find( name );
        if( pmemb )
            return *pmemb;
        return this->add( name, nullptr );
    }

    // add sequence of scalar members: (name, value, name, value, ...)
    template<typename Cy, typename Vy, typename ...Ps>
    void adds( const Cy& name, const Vy& val, Ps&& ...pairs )
    {
        this->add( name, val );
        this->adds( std::forward<Ps>(pairs)... );
    }

    // erase member from the array, return false if failed
    // WARNING: if duplicate name exists, only remove the first one
    // WARNING: O(N) sequential search
    bool erase( const char* name );
    bool erase( JsonMember& memb );

    // delete all members
    void clear();

    // format JSON text, add to the tail of the output string, for internal use
    void format( std::string& );
    void format( std::string&, int );

    // parse and fill from JSON text, for internal use
    int parse( const char*, int );

    // return allocator, for internal use
    JsonAlloc* allocator()  { return m_allocator; }

private:
    // can not create directly, must be created by parent or JSON Document
    friend JsonAlloc;
    explicit JsonObject( JsonAlloc* alloc ) : m_count(0), m_allocator(alloc) {}
    ~JsonObject() { this->clear(); }

    JsonMember& add_null();
    void adds() {}

    int         m_count;                // number of children
    JsonAlloc*  m_allocator;            // memory allocator
    detail::SList<JsonMember> m_slist;  // member list
};

//======================================================================================================================
// JSON Document

// JSON parsing status
enum class JsonStatus
{
    Ok = 0,         // No Error
    Invalid,        // invalid JSON content or syntax
    IOFailed,		// file read/write failed
    Unsupported,	// unsupported (such as UTF-16 encoding)
};

class JsonDoc : IrkNocopy
{
public:
    static const int kMaxDepth = 80;    // max depth to prevent stack overflow
    static const int kMaxIndent = 40;   // max indent when print pretty

    JsonDoc();
    ~JsonDoc();

    // parse Json file
    JsonStatus parse_file( const char* filepath );

    // parse Json text, text must be null terminated
    JsonStatus parse_text( const char* text );

    // is JSON document valid/parsed
    explicit operator bool() const      { return m_root.is_valid(); }

    // return the JSON root Object/Array
    const JsonValue& root() const       { return m_root; }
    JsonValue& root()                   { return m_root; }

    // create empty root Object, old root will be destroyed(if exists)
    JsonObject& create_root_object()    { return m_root.make_empty_object(); }

    // create empty root Array, old root will be destroyed(if exists)
    JsonArray& create_root_array()      { return m_root.make_empty_array(); }

    // dump Json document as UTF-8 file
    JsonStatus dump_file( const char* filename );
    
    // dump Json document as UTF-8 string
    JsonStatus dump_text( std::string& out );

    // ditto, slower but indented nicely
    JsonStatus pretty_dump_file( const char* filename );
    JsonStatus pretty_dump_text( std::string& out );

#ifdef _WIN32
    JsonStatus parse_file( const wchar_t* filepath );
    JsonStatus dump_file( const wchar_t* filename );
    JsonStatus pretty_dump_file( const wchar_t* filename );
#endif

private:
    JsonStatus parse_file( CFile& );
    JsonStatus write_file( CFile&, const std::string& );
    JsonAlloc*  m_allocator;    // internal memory allocator
    JsonValue   m_root;         // the root JSON Value
};

} // namespace irk
#endif
