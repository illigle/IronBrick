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

#include <errno.h>
#include <math.h>
#include "IrkStringUtility.h"
#include "IrkCFile.h"
#include "IrkJSON.h"

using std::string;

namespace irk {

// default JSON allocator
static JsonAllocator* GetDefaultAllocator()
{
    static JsonDefaultAllocator s_DefaultAllocator;
    return &s_DefaultAllocator;
}

//======================================================================================================================
// JSON String

class JsonString : IrkNocopy
{
    static constexpr int kInnerSize = 16;   // for small string optimization
public:
    explicit JsonString( JsonAllocator* alloc ) : m_alloc(alloc), m_blksize(kInnerSize) 
    {}
    explicit JsonString( const char* str, int len, JsonAllocator* alloc ) : JsonString(alloc)
    {
        this->assign( str, len );
    }
    ~JsonString()
    {
        this->clear(); 
    }

    // get string data
    const char* data() const    { return m_blksize > kInnerSize ? m_block : m_inbuf; }
    char* data()                { return m_blksize > kInnerSize ? m_block : m_inbuf; }

    // assignment, if len < 0, str must be null-terminated
    void assign( const char* str, int len = -1 );

    // clear string and release external block(if exists)
    void clear();

    // clear and allocate buffer of at least <size> bytes
    char* alloc( int size );

    // parse and load from JSON text
    int load( const char* text );

    // return allocator
    JsonAllocator* allocator() const { return m_alloc; }

private:
    JsonAllocator*  m_alloc;            // extern block's allocator
    int32_t         m_blksize;          // memory block size
    union {
        char    m_inbuf[kInnerSize];    // inner buffer for small string optimization
        char*   m_block;                // external memory block
    };
};

// clear string and release external block(if exists)
inline void JsonString::clear()
{
    if( m_blksize > kInnerSize )   // using external buffer
    {
        m_alloc->dealloc( m_block, (size_t)m_blksize );
        m_blksize = kInnerSize;
        m_block = nullptr;
    }
}

// clear old string and alloc empty buffer
inline char* JsonString::alloc( int size )
{
    if( size > m_blksize )
    {
        this->clear();
        m_block = (char*)m_alloc->alloc( size, sizeof(void*) );
        m_blksize = size;
        assert( m_blksize > kInnerSize );
        return m_block;
    }
    return this->data();
}

// assign new string
inline void JsonString::assign( const char* str, int len )
{
    // get length(if not provided)
    if( len < 0 )
        len = static_cast<int>( strlen( str ) );

    // allocate enough buffer
    char* dst = this->alloc( len + 1 );
    assert( m_blksize > len );

    // copy string
    memcpy( dst, str, len );
    dst[len] = '\0';
}

//======================================================================================================================
// JSON general value

// construct a JSON Null value
JsonValue::JsonValue( std::nullptr_t )
{
    m_type = Json_null;
    m_alloc = GetDefaultAllocator();
}
JsonValue::JsonValue( std::nullptr_t, JsonAllocator* alloc )
{
    m_type = Json_null;
    m_alloc = alloc;    // user must keep allocator alive during the lifetime of this object
}

// construct an empty JSON value, empty JSON value is in invalid state
JsonValue::JsonValue()
{
    m_type = Json_empty;
    m_alloc = GetDefaultAllocator();
}
JsonValue::JsonValue( JsonAllocator* alloc )
{
    m_type = Json_empty;
    m_alloc = alloc;    // user must keep allocator alive during the lifetime of this object
}

JsonValue::JsonValue( const JsonValue& other )
{
    m_alloc = other.m_alloc;

    if( other.m_type < Json_scalar )
    {
        m_value = other.m_value;
    }
    else if( other.m_type == Json_string )
    {
        m_value.str = m_alloc->create<JsonString>( other.m_value.str->data(), -1, m_alloc );
    }
    else if( other.m_type == Json_object )
    {
        m_value.obj = m_alloc->create<JsonObject>( *other.m_value.obj );
    }
    else if( other.m_type == Json_array )
    {
        m_value.ary = m_alloc->create<JsonArray>( *other.m_value.ary );
    }

    m_type = other.m_type;
}

JsonValue::JsonValue( JsonValue&& other )
{
    m_type = other.m_type;
    m_value = other.m_value;
    m_alloc = other.m_alloc;
    other.m_type = Json_empty;
}

JsonValue& JsonValue::operator=( const JsonValue& other )
{
    if( this != &other )    // protect from the mad
    {
        if( other.m_type < Json_scalar )
        {
            this->clear();
            m_value = other.m_value;
            m_type = other.m_type;
        }
        else if( other.m_type == Json_string )
        {
            this->set( other.m_value.str->data() );
        }
        else if( other.m_type == Json_object )
        {
            this->set( *other.m_value.obj );
        }
        else if( other.m_type == Json_array )
        {
            this->set( *other.m_value.ary );
        }
    }
    return *this;
}

JsonValue& JsonValue::operator=( JsonValue&& other )
{
    if( m_alloc != other.m_alloc )  // different allocator, can't move, copy instead
    {
        *this = other;
    }
    else if( this != &other )       // protect from the mad
    {
        this->clear();
        m_type = other.m_type;
        m_value = other.m_value;
        other.m_type = Json_empty;
    }
    return *this;
}

// delete non-scalar value stored in the JSON Value
void JsonValue::clear()
{
    if( m_type > Json_scalar )
    {
        if( m_type == Json_string )
        {
            assert( m_alloc == m_value.str->allocator() );
            m_alloc->trash( m_value.str );
        }
        else if( m_type == Json_object )
        {
            assert( m_alloc == m_value.obj->allocator() );
            m_alloc->trash( m_value.obj );
        }
        else if( m_type == Json_array )
        {
            assert( m_alloc == m_value.ary->allocator() );
            m_alloc->trash( m_value.ary );
        }
    }
    m_type = Json_empty;
}

bool JsonValue::as_bool() const
{
    if( m_type == Json_boolean )
    {
        return m_value.flag;
    }
    else if( m_type == Json_int64 || m_type == Json_uint64 )
    {
        return m_value.u64 != 0;
    }
    throw JsonBadAccess();
}

int32_t JsonValue::as_int() const
{
    if( m_type == Json_int64 )
    {
        if( m_value.i64 >= INT32_MIN && m_value.i64 <= INT32_MAX )
            return static_cast<int32_t>( m_value.i64 );
    }
    else if( m_type == Json_uint64 )
    {
        if( m_value.u64 <= INT32_MAX )
            return static_cast<int32_t>( m_value.u64 );
    }
    else if( m_type == Json_boolean )
    {
        return m_value.flag;
    }
    else if( m_type == Json_double )
    {
        return static_cast<int32_t>( m_value.f64 );
    }
    throw JsonBadAccess();
}

uint32_t JsonValue::as_uint() const
{
    if( m_type == Json_uint64 )
    {
        if( m_value.u64 <= UINT32_MAX )
            return static_cast<uint32_t>( m_value.u64 );
    }
    else if( m_type == Json_int64 )
    {
        if( m_value.i64 >= 0 && m_value.i64 <= UINT32_MAX )
            return static_cast<uint32_t>( m_value.i64 );
    }
    else if( m_type == Json_boolean )
    {
        return m_value.flag;
    }
    else if( m_type == Json_double )
    {
        if( m_value.f64 >= 0 )
            return static_cast<uint32_t>( m_value.f64 );
    }
    throw JsonBadAccess();
}

int64_t JsonValue::as_int64() const
{
    if( m_type == Json_int64 )
    {
        return m_value.i64;
    }
    else if( m_type == Json_uint64 )
    {
        if( m_value.u64 <= INT64_MAX )
            return static_cast<int64_t>( m_value.u64 );
    }
    else if( m_type == Json_boolean )
    {
        return m_value.flag;
    }
    else if( m_type == Json_double )
    {
        return static_cast<int64_t>( m_value.f64 );
    }
    throw JsonBadAccess();
}

uint64_t JsonValue::as_uint64() const
{
    if( m_type == Json_uint64 )
    {
        return m_value.u64;
    }
    else if( m_type == Json_int64 )
    {
        if( m_value.i64 >= 0 )
            return static_cast<uint64_t>( m_value.i64 );
    }
    else if( m_type == Json_boolean )
    {
        return m_value.flag;
    }
    else if( m_type == Json_double )
    {
        if( m_value.f64 >= 0 )
            return static_cast<uint64_t>( m_value.f64 );
    }
    throw JsonBadAccess();
}

float JsonValue::as_float() const
{
    if( m_type == Json_double )
    {
        return static_cast<float>( m_value.f64 );
    }
    else if( m_type == Json_int64 )
    {
        return static_cast<float>( m_value.i64 );
    }
    else if( m_type == Json_uint64 )
    {
        return static_cast<float>( m_value.u64 );
    }
    throw JsonBadAccess();
}

double JsonValue::as_double() const
{
    if( m_type == Json_double )
    {
        return m_value.f64;
    }
    else if( m_type == Json_int64 )
    {
        return static_cast<double>( m_value.i64 );
    }
    else if( m_type == Json_uint64 )
    {
        return static_cast<double>( m_value.u64 );
    }
    throw JsonBadAccess();
}

const char* JsonValue::as_string() const
{
    if( m_type == Json_string )
    {
        return m_value.str->data();
    }
    throw JsonBadAccess();
}

const JsonObject& JsonValue::as_object() const
{
    if( m_type == Json_object )
    {
        return *m_value.obj;
    }
    throw JsonBadAccess();
}
JsonObject& JsonValue::as_object()
{
    if( m_type == Json_object )
    {
        return *m_value.obj;
    }
    throw JsonBadAccess();
}

const JsonArray& JsonValue::as_array() const
{
    if( m_type == Json_array )
    {
        return *m_value.ary;
    }
    throw JsonBadAccess();
}
JsonArray& JsonValue::as_array()
{
    if( m_type == Json_array )
    {
        return *m_value.ary;
    }
    throw JsonBadAccess();
}

bool JsonValue::get( bool& outFlag ) const
{
    if( m_type == Json_boolean )
    {
        outFlag = m_value.flag;
        return true;
    }
    else if( m_type == Json_int64 || m_type == Json_uint64 )
    {
        outFlag = (m_value.u64 != 0);
        return true;
    }
    return false;
}

bool JsonValue::get( int32_t& outValue ) const
{
    if( m_type == Json_int64 )
    {
        if( m_value.i64 >= INT32_MIN && m_value.i64 <= INT32_MAX )
        {
            outValue = static_cast<int32_t>( m_value.i64 );
            return true;
        }
    }
    else if( m_type == Json_uint64 )
    {
        if( m_value.u64 <= INT32_MAX )
        {
            outValue = static_cast<int32_t>( m_value.u64 );
            return true;
        }
    }
    else if( m_type == Json_boolean )
    {
        outValue = m_value.flag;
        return true;
    }
    else if( m_type == Json_double )
    {
        outValue = static_cast<int32_t>( m_value.f64 );
        return true;
    }
    return false;
}

bool JsonValue::get( uint32_t& outValue ) const
{
    if( m_type == Json_uint64 )
    {
        if( m_value.u64 <= UINT32_MAX )
        {
            outValue = static_cast<uint32_t>( m_value.u64 );
            return true;
        }
    }
    else if( m_type == Json_int64 )
    {
        if( m_value.i64 >= 0 && m_value.i64 <= UINT32_MAX )
        {
            outValue = static_cast<uint32_t>( m_value.i64 );
            return true;
        }
    }
    else if( m_type == Json_boolean )
    {
        outValue = m_value.flag;
        return true;
    }
    else if( m_type == Json_double )
    {
        if( m_value.f64 >= 0 )
        {
            outValue = static_cast<uint32_t>( m_value.f64 );
            return true;
        }
    }
    return false;
}

bool JsonValue::get( int64_t& outValue ) const
{
    if( m_type == Json_int64 )
    {
        outValue = m_value.i64;
        return true;
    }
    else if( m_type == Json_uint64 )
    {
        if( m_value.u64 <= INT64_MAX )
        {
            outValue = static_cast<int64_t>( m_value.u64 );
            return true;
        }
    }
    else if( m_type == Json_boolean )
    {
        outValue = m_value.flag;
        return true;
    }
    else if( m_type == Json_double )
    {
        outValue = static_cast<int64_t>( m_value.f64 );
        return true;
    }
    return false;
}

bool JsonValue::get( uint64_t& outValue ) const
{
    if( m_type == Json_uint64 )
    {
        outValue = m_value.u64;
        return true;
    }
    else if( m_type == Json_int64 )
    {
        if( m_value.i64 >= 0 )
        {
            outValue = static_cast<uint64_t>( m_value.i64 );
            return true;
        }
    }
    else if( m_type == Json_boolean )
    {
        outValue = m_value.flag;
        return true;
    }
    else if( m_type == Json_double )
    {
        if( m_value.f64 >= 0 )
        {
            outValue = static_cast<uint64_t>( m_value.f64 );
            return true;
        }
    }
    return false;
}

bool JsonValue::get( float& outValue ) const
{
    if( m_type == Json_double )
    {
        outValue = static_cast<float>( m_value.f64 );
        return true;
    }
    else if( m_type == Json_int64 )
    {
        outValue = static_cast<float>( m_value.i64 );
        return true;
    }
    else if( m_type == Json_uint64 )
    {
        outValue = static_cast<float>( m_value.u64 );
        return true;
    }
    return false;
}

bool JsonValue::get( double& outValue ) const
{
    if( m_type == Json_double )
    {
        outValue = m_value.f64;
        return true;
    }
    else if( m_type == Json_int64 )
    {
        outValue = static_cast<double>( m_value.i64 );
        return true;
    }
    else if( m_type == Json_uint64 )
    {
        outValue = static_cast<double>( m_value.u64 );
        return true;
    }
    return false;
}

bool JsonValue::get( std::string& outString ) const
{
    if( m_type == Json_string )
    {
        outString = m_value.str->data();
        return true;
    }
    outString.clear();
    return false;
}

bool JsonValue::get( const JsonObject*& outObj ) const
{
    if( m_type == Json_object )
    {
        outObj = m_value.obj;
        return true;
    }
    outObj = nullptr;
    return false;
}
bool JsonValue::get( JsonObject*& outObj )
{
    if( m_type == Json_object )
    {
        outObj = m_value.obj;
        return true;
    }
    outObj = nullptr;
    return false;
}

bool JsonValue::get( const JsonArray*& outAry ) const
{
    if( m_type == Json_array )
    {
        outAry = m_value.ary;
        return true;
    }
    outAry = nullptr;
    return false;
}
bool JsonValue::get( JsonArray*& outAry )
{
    if( m_type == Json_array )
    {
        outAry = m_value.ary;
        return true;
    }
    outAry = nullptr;
    return false;
}

// assign string, str must be null-terminated if len < 0
void JsonValue::set( const char* str, int len )
{
    if( m_type == Json_string )     // already contains string
    {
        m_value.str->assign( str, len );
    }
    else
    {
        this->clear();
        m_value.str = m_alloc->create<JsonString>( str, len, m_alloc );
        m_type = Json_string;
    }
}
void JsonValue::set( const std::string& str )
{
    this->set( str.c_str(), (int)str.length() );
}

// assign JSON Object, return reference to the new Object
JsonObject& JsonValue::set( const JsonObject& obj )
{
    this->make_empty_object();
    *m_value.obj = obj;
    return *m_value.obj;
}

// assign JSON Array, return reference to the new Array
JsonArray& JsonValue::set( const JsonArray& ary )
{
    this->make_empty_array();
    *m_value.ary = ary;
    return *m_value.ary;
}

// make as empty JSON String, return reference to the String
JsonString& JsonValue::make_empty_string()
{
    if( m_type == Json_string )     // already contains string
    {
        m_value.str->clear();
    }
    else
    {
        this->clear();
        m_value.str = m_alloc->create<JsonString>( m_alloc );
        m_type = Json_string;
    }
    return *m_value.str;
}

// make as empty JSON Object, return reference to the Object
JsonObject& JsonValue::make_empty_object()
{
    if( m_type == Json_object ) // already contains JSON Object
    {
        m_value.obj->clear();   // make empty
    }
    else
    {
        this->clear();
        m_value.obj = m_alloc->create<JsonObject>( m_alloc );
        m_type = Json_object;
    }
    return *m_value.obj;
}

// make as empty JSON Array, return reference to the Array
JsonArray& JsonValue::make_empty_array()
{
    if( m_type == Json_array )  // already contains JSON Array
    {
        m_value.ary->clear();   // make empty
    }
    else
    {
        this->clear();
        m_value.ary = m_alloc->create<JsonArray>( m_alloc );
        m_type = Json_array;
    }
    return *m_value.ary;
}

//======================================================================================================================
// JSON Array

JsonElem::JsonElem(JsonArray* parent) 
    : JsonValue(nullptr, parent->allocator()), m_pNext(nullptr), m_parent(parent)
{}

JsonArray::JsonArray()
{
    m_alloc = GetDefaultAllocator();
    m_count = 0;
}

// NOTE: user must keep allocator alive during the lifetime of this object
JsonArray::JsonArray( JsonAllocator* alloc )
{
    m_alloc = alloc;
    m_count = 0;
}

JsonArray::JsonArray( const JsonArray& other )
{
    m_alloc = other.m_alloc;
    m_count = 0;
    for( const JsonValue& val : other )
        this->add( val );
}

JsonArray::JsonArray( JsonArray&& other )
{
    m_alloc = other.m_alloc;
    m_count = other.m_count;
    m_slist.m_firstNode = other.m_slist.m_firstNode;
    m_slist.m_lastNode = other.m_slist.m_lastNode;
    other.m_count = 0;
    other.m_slist.clear();
}

JsonArray& JsonArray::operator=( const JsonArray& other )
{
    if( this != &other )
    {
        this->clear();
        for( const JsonValue& val : other )
            this->add( val );
    }
    return *this;
}

JsonArray& JsonArray::operator=( JsonArray&& other )
{
    if( this != &other )
    {
        if( m_alloc != other.m_alloc )  // allocator diff, can't move
        {
            *this = other;
        }
        else
        {
            this->clear();
            m_count = other.m_count;
            m_slist.m_firstNode = other.m_slist.m_firstNode;
            m_slist.m_lastNode = other.m_slist.m_lastNode;
            other.m_count = 0;
            other.m_slist.clear();
        }
    }
    return *this;
}

// delete all elements
void JsonArray::clear()
{
    JsonElem* curr = m_slist.m_firstNode;
    while( curr )
    {
        JsonElem* next = curr->next_sibling();
        m_alloc->trash( curr );
        curr = next;
    }
    m_count = 0;
    m_slist.clear();
}

// resize the array, if the current count is less than count, additional JSON Null value are appended
void JsonArray::resize( int cnt )
{
    if( cnt < m_count )
    {
        JsonElem* elem = m_slist.shrink( cnt );
        while( elem )
        {
            JsonElem* next = elem->next_sibling();
            m_alloc->trash( elem );
            elem = next;
        }
        m_count = cnt;
    }
    else
    {
        for( int i = m_count; i < cnt; i++ )
        {
            this->add( nullptr );
        }
    }
}

// add a null element to the tail of array, return reference to the new element
JsonElem& JsonArray::add( std::nullptr_t )
{
    JsonElem* pElem = m_alloc->create<JsonElem>( this );
    m_slist.push_back( pElem );
    m_count++;
    return *pElem;
}

// erase element from the array
bool JsonArray::erase( int idx )
{
    JsonElem* victim = m_slist.remove( idx );
    if( victim )
    {
        m_alloc->trash( victim );
        m_count--;
        return true;
    }
    return false;
}
bool JsonArray::erase( JsonElem& elem )
{
    JsonElem* victim = m_slist.remove( &elem );
    if( victim )
    {
        m_alloc->trash( victim );
        m_count--;
        return true;
    }
    return false;
}

//======================================================================================================================
// JSON Members in JSON Object

JsonMember::JsonMember( const char* name, JsonObject* parent )
    : JsonValue(nullptr, parent->allocator()), m_pName(nullptr), m_pNext(nullptr), m_parent(parent)
{
    if( name )
        m_pName = m_alloc->create<JsonString>( name, -1, m_alloc );
    else
        m_pName = m_alloc->create<JsonString>( m_alloc );
}

JsonMember::JsonMember( const std::string& name, JsonObject* parent )
    : JsonValue(nullptr, parent->allocator()), m_pName(nullptr), m_pNext(nullptr), m_parent(parent)
{
    m_pName = m_alloc->create<JsonString>( name.c_str(), (int)name.length(), m_alloc );
}

JsonMember::~JsonMember()
{
    m_alloc->trash( m_pName );
}

// get name
const char* JsonMember::name() const
{
    return m_pName->data(); 
}

// set name
void JsonMember::set_name( const char* name )
{ 
    m_pName->assign( name );
}
void JsonMember::set_name( const std::string& name )
{ 
    m_pName->assign( name.c_str(), (int)name.length() ); 
}

//======================================================================================================================
// JSON Object

JsonObject::JsonObject()
{
    m_alloc = GetDefaultAllocator();
    m_count = 0;
}

// NOTE: user must keep allocator alive during the lifetime of this object
JsonObject::JsonObject( JsonAllocator* alloc )
{
    m_alloc = alloc;
    m_count = 0;
}

JsonObject::JsonObject( const JsonObject& other )
{
    m_alloc = other.m_alloc;
    m_count = 0;
    for( const auto& memb : other )
        this->add( memb.name(), (const JsonValue&)memb );
}

JsonObject::JsonObject( JsonObject&& other )
{
    m_alloc = other.m_alloc;
    m_count = other.m_count;
    m_slist.m_firstNode = other.m_slist.m_firstNode;
    m_slist.m_lastNode = other.m_slist.m_lastNode;
    other.m_count = 0;
    other.m_slist.clear();
}

JsonObject& JsonObject::operator=( const JsonObject& other )
{
    if( this != &other )
    {
        this->clear();
        for( const auto& memb : other )
            this->add( memb.name(), (const JsonValue&)memb );
    }
    return *this;
}

JsonObject& JsonObject::operator=( JsonObject&& other )
{
    if( this != &other )
    {
        if( m_alloc != other.m_alloc )  // allocator diff, can't move
        {
            *this = other;
        }
        else
        {
            this->clear();
            m_count = other.m_count;
            m_slist.m_firstNode = other.m_slist.m_firstNode;
            m_slist.m_lastNode = other.m_slist.m_lastNode;
            other.m_count = 0;
            other.m_slist.clear();
        }
    }
    return *this;
}

// delete all members
void JsonObject::clear()
{
    JsonMember* curr = m_slist.m_firstNode;
    while( curr )
    {
        JsonMember* next = curr->next_sibling();
        m_alloc->trash( curr );
        curr = next;
    }
    m_count = 0;
    m_slist.clear();
}

// find the desired member, return nullptr if failed
// NOTE: if duplicate name exists, only return the first one.
// NOTE: use O(N) sequential search. For traversal use iterator instead
JsonMember* JsonObject::find( const char* name )
{
    return m_slist.find( [=](JsonMember* m){ return strcmp(m->name(), name)==0; } );
}
JsonMember* JsonObject::find( const std::string& name )
{
    return m_slist.find( [&](JsonMember* m){ return strcmp(m->name(), name.c_str())==0; } );
}
const JsonMember* JsonObject::find( const char* name ) const
{
    return m_slist.find( [=](JsonMember* m){ return strcmp(m->name(), name)==0; } );
}
const JsonMember* JsonObject::find( const std::string& name ) const
{
    return m_slist.find( [&](JsonMember* m){ return strcmp(m->name(), name.c_str())==0; } );
}

// add null member to the obj, return reference to the new member
JsonMember& JsonObject::add( const char* name, std::nullptr_t )
{
    JsonMember* pmemb = m_alloc->create<JsonMember>( name, this );
    m_slist.push_back( pmemb );
    m_count++;
    return *pmemb;
}
JsonMember& JsonObject::add( const std::string& name, std::nullptr_t )
{
    JsonMember* pmemb = m_alloc->create<JsonMember>( name, this );
    m_slist.push_back( pmemb );
    m_count++;
    return *pmemb;
}

// erase member from the obj, return false if failed
bool JsonObject::erase( const char* name )
{
    JsonMember* victim = m_slist.remove( [name](JsonMember* m){ return strcmp(m->name(), name)==0; } );
    if( victim )
    {
        m_alloc->trash( victim );
        m_count--;
        return true;
    }
    return false;
}
bool JsonObject::erase( JsonMember& member )
{
    JsonMember* victim = m_slist.remove( &member );
    if( victim )
    {
        m_alloc->trash( victim );
        m_count--;
        return true;
    }
    return false;
}

//======================================================================================================================
// format to JSON text

// convert normal string to JSON text
static void to_json_text( const char* str, string& dst )
{
    static const char s_HexChar[] = "0123456789abcdef";

    const uint8_t* src = (const uint8_t*)str;
    while( *src )
    {
        if( *src == '\"' )
        {
            dst += "\\\"";
        }
        else if( *src == '\\' )
        {
            dst += "\\\\";
        }
        else if( *src < 0x20 )  // control character
        {
            if( *src == '\n' )
                dst += "\\n";
            else if( *src == '\t' )
                dst += "\\t";
            else if( *src == '\r' )
                dst += "\\r";
            else if( *src == '\b' )
                dst += "\\b";
            else if( *src == '\f' )
                dst += "\\f";
            else
            {
                dst += "\\u00";
                dst += s_HexChar[*src >> 4];
                dst += s_HexChar[*src & 0xF];
            }
        }
        else    // normal character
        {
            dst.push_back( (char)*src );
        }
        src++;
    }
}

// to JSON text, add to the tail of the <outStr>
void JsonValue::dump( std::string& outStr ) const
{
    constexpr int bufsize = 80;
    char sbuf[bufsize];

    switch( m_type )
    {
    case Json_null:
        outStr += "null";
        break;
    case Json_boolean:
        outStr += m_value.flag ? "true" : "false";
        break;
    case Json_int64:
        int_to_str( m_value.i64, sbuf, bufsize );
        outStr += sbuf;
        break;
    case Json_uint64:
        int_to_str( m_value.u64, sbuf, bufsize );
        outStr += sbuf;
        break;
    case Json_double:
        if( ::fabs(m_value.f64) < 1.0e7)
            str_format( sbuf, bufsize, "%0.7f", m_value.f64 );
        else
            str_format( sbuf, bufsize, "%0.15e", m_value.f64 );
        outStr += sbuf;
        break;
    case Json_string:
        outStr += '\"';
        to_json_text( m_value.str->data(), outStr );
        outStr += '\"';
        break;
    case Json_object:
        m_value.obj->dump( outStr );
        break;
    case Json_array:
        m_value.ary->dump( outStr );
        break;
    default:
        assert( false );    // empty value is invalid
        outStr += "null";
        break;
    }
}

// to JSON text, add to the tail of the <outStr>
void JsonValue::pretty_dump( std::string& outStr, int indent ) const
{
    switch( m_type )
    {
    case Json_object:
        m_value.obj->pretty_dump( outStr, indent );
        break;
    case Json_array:
        m_value.ary->pretty_dump( outStr, indent );
        break;
    default:
        this->dump( outStr );
        break;
    }
}

// to JSON text, add to the tail of the <outStr>
void JsonMember::dump( std::string& outStr ) const
{
    outStr += '\"';
    to_json_text( m_pName->data(), outStr );
    outStr += "\": ";
    JsonValue::dump( outStr );
}

// to JSON text, add to the tail of the <outStr>
void JsonMember::pretty_dump( std::string& outStr, int indent ) const
{
    outStr += '\"';
    to_json_text( m_pName->data(), outStr );
    outStr += "\": ";
    JsonValue::pretty_dump( outStr, indent );
}

// to JSON text, add to the tail of the <outStr>
void JsonArray::dump( std::string& outStr ) const
{
    outStr += '[';

    JsonElem* curr = m_slist.m_firstNode;
    if( curr )
    {
        // the first element
        curr->dump( outStr );
        curr = curr->next_sibling();

        // other elements
        while( curr )
        {
            outStr += ',';
            curr->dump( outStr );
            curr = curr->next_sibling();
        }
    }

    outStr += ']';
}

// to JSON text, add to the tail of the <outStr>
void JsonArray::pretty_dump( std::string& outStr, int indent ) const
{
    outStr += "[\n";

    JsonElem* curr = m_slist.m_firstNode;
    if( curr )
    {
        const int indent2 = MIN(indent + 2, JsonDoc::kMaxIndent);

        // the first element
        outStr.append( indent2, ' ' );
        curr->pretty_dump( outStr, indent2 );
        curr = curr->next_sibling();

        // other elements
        while( curr )
        {
            outStr += ",\n";
            outStr.append( indent2, ' ' );
            curr->pretty_dump( outStr, indent2 );
            curr = curr->next_sibling();
        }

        outStr += '\n';
    }

    outStr.append( indent, ' ' );
    outStr += "]";
}

// to JSON text, add to the tail of the <outStr>
void JsonObject::dump( std::string& outStr ) const
{
    outStr += '{';

    JsonMember* curr = m_slist.m_firstNode;
    if( curr )
    {
        // the first element
        curr->dump( outStr );
        curr = curr->next_sibling();

        // other elements
        while( curr )
        {
            outStr += ',';
            curr->dump( outStr );
            curr = curr->next_sibling();
        }
    }

    outStr += '}';
}

// to JSON text, add to the tail of the <outStr>
void JsonObject::pretty_dump( std::string& outStr, int indent ) const
{
    outStr += "{\n";

    JsonMember* curr = m_slist.m_firstNode;
    if( curr )
    {
        const int indent2 = MIN(indent + 2, JsonDoc::kMaxIndent);

        // the first member
        outStr.append( indent2, ' ' );
        curr->pretty_dump( outStr, indent2 );
        curr = curr->next_sibling();

        // other member
        while( curr )
        {
            outStr += ",\n";
            outStr.append( indent2, ' ' );
            curr->pretty_dump( outStr, indent2 );
            curr = curr->next_sibling();
        }

        outStr += '\n';
    }

    outStr.append( indent, ' ' );
    outStr += "}";
}

//======================================================================================================================
// JSON parsing

// convert \uxxxx to UTF-8 char, return byte count, return -1 if failed
static int json_escape_to_utf8( const char* text, char* dst )
{
    assert( ::strlen(text) >= 6 && text[0] == '\\' && text[1] == 'u' );

    // get unicode character number
    char buf[8] = {0};
    ::strncpy( buf, text + 2, 4 );
    uint32_t chnum = ::strtoul( buf, nullptr, 16 );
    if( chnum >= 0xD800 && chnum <= 0xDBFF )  // 4 bytes UTF-16
    {
        if( ::strlen(text + 6) < 6 || text[6] != '\\' || text[7] != 'u' )
            return -1;

        ::strncpy( buf, text + 8, 4 );
        uint32_t chnum2 = ::strtoul( buf, nullptr, 16 );
        if( chnum2 >= 0xDC00 && chnum2 <= 0xDFFF )
        {
            chnum = (chnum - 0xD800) << 10;
            chnum += (chnum2 - 0xDC00) + 0x10000;
        }
        else    // invalid character
        {
            return -1;
        }
    }
    else if( chnum >= 0xDC00 && chnum <= 0xDFFF ) // invalid character
    {
        return -1;
    }

    // write UTF-8 chars
    uint8_t* out = (uint8_t*)dst;
    if( chnum < 0x80 )
    {
        out[0] = (uint8_t)chnum;
        return 1;
    }
    else if( chnum < 0x800 )
    {
        out[0] = (uint8_t)(0xC0 | (chnum >> 6));
        out[1] = (uint8_t)(0x80 | (chnum & 0x3F));
        return 2;
    }
    else if( chnum < 0x10000 )
    {
        out[0] = (uint8_t)( 0xE0 | (chnum >> 12) );
        out[1] = (uint8_t)( 0x80 | ((chnum >> 6) & 0x3F) );
        out[2] = (uint8_t)( 0x80 | (chnum & 0x3F) );
        return 3;
    }
    else
    {
        out[0] = (uint8_t)( 0xF0 | (chnum >> 18) );
        out[1] = (uint8_t)( 0x80 | ((chnum >> 12) & 0x3F) );
        out[2] = (uint8_t)( 0x80 | ((chnum >> 6) & 0x3F) );
        out[3] = (uint8_t)( 0x80 | (chnum & 0x3F) );
        return 4;
    }
}

// parse and load from JSON text, return character count consumed, return -1 if failed
int JsonString::load( const char* text )
{
    // skip \"
    assert( text[0] == '\"' );
    text++;

    // get string length and ensure string is valid
    int srcLen = 0;
    int dstLen = 0;
    while( text[srcLen] != 0 && text[srcLen] != '\"' )
    {
        if( (uint8_t)text[srcLen] < 0x20 )  // can not contains control charcter
        {
            return -1;
        }
        else if( text[srcLen] != '\\' )     // normal character
        {
            srcLen++;
            dstLen++;
        }
        else    // escape character
        {
            if( text[srcLen+1] == 0 )   // invalid string
                return -1;

            if( text[srcLen+1] == 'u' ) // unicode code number
            {
                for( int j = srcLen + 2; j < srcLen + 6; j++ )
                {
                    if( !IS_XDIGIT( text[j] ) )
                        return -1;
                }
                srcLen += 6;
                dstLen += 4;    // a UTF-8 char may need 4 bytes
            }
            else
            {
                srcLen += 2;
                dstLen++;
            }
        }
    }
    if( text[srcLen] != '\"' )  // can't find ending \", invalid string
    {
        return -1;
    }

    if( dstLen == srcLen )      // normal string, no conversion needed
    {
        this->assign( text, srcLen );
        return srcLen + 2;
    }

    // unescape JSON text
    char* dst = this->alloc( dstLen + 1 );
    for( int k = 0; k < srcLen; )
    {
        if( text[k] != '\\' )   // normal character
        {
            *dst++ = text[k++];
        }
        else    // escape character
        {
            if( text[k+1] == 'n' )
            {
                *dst++ = '\n';
                k += 2;
            }
            else if( text[k+1] == 'r' )
            {
                *dst++ = '\t';
                k += 2;
            }
            else if( text[k+1] == 't' )
            {
                *dst++ = '\t';
                k += 2;
            }
            else if( text[k+1] == 'b' )
            {
                *dst++ = '\b';
                k += 2;
            }
            else if( text[k+1] == 'f' )
            {
                *dst++ = '\f';
                k += 2;
            }
            else if( text[k+1] == '\"' )
            {
                *dst++ = '\"';
                k += 2;
            }
            else if( text[k+1] == '\\' )
            {
                *dst++ = '\\';
                k += 2;
            }
            else if( text[k+1] == '/' )
            {
                *dst++ = '/';
                k += 2;
            }
            else if( text[k+1] == 'u' )     // unicode code number
            {
                // convert to UTF-8
                int cnt = json_escape_to_utf8( text + k, dst );
                if( cnt <= 0 )
                    return -1;
                else if( cnt > 3 )  // 4-bytes UTF-8
                    k += 6;
                k += 6;
                dst += cnt;
            }
            else    // invalid string
            {
                return -1;
            }
        }
    }
    assert( this->data() + m_blksize > dst );
    *dst = 0;

    return srcLen + 2;
}

// parse and load from JSON text, return character count consumed, return -1 if failed
int JsonMember::load( const char* text, int depth )
{
    int len = 0;

    // parse member's name
    while( IS_SPACE( text[len] ) )  // skip space
        len++;
    if( text[len] != '\"' )         // invalid, must be string
        return -1;
    int used = m_pName->load( text + len );
    if( used > 0 )
        len += used;
    else
        return -1;      // parsing failed

    // parse member's value
    while( IS_SPACE( text[len] ) )  // skip space
        len++;
    if( text[len++] != ':' )        // skip colon
        return -1;
    used = JsonValue::load( text + len, depth );
    if( used > 0 )
        len += used;
    else
        return -1;      // parsing failed

    return len;
}

// parse and load from JSON text, return character count consumed, return -1 if failed
int JsonArray::load( const char* text, int depth )
{
    // clear old elements if exists
    if( m_count > 0 )
        this->clear();

    // skip leading white-space
    int len = 0;
    while( IS_SPACE( text[len] ) )
        len++;

    // is JSON Array
    if( text[len++] != '[' )
        return -1;

    // skip white-space
    while( IS_SPACE( text[len] ) )
        len++;

    // check empty JSON Array
    if( text[len] == ']' )
        return len + 1;

    // check depth, prevent stack overflow
    if( depth++ > JsonDoc::kMaxDepth )
        return -1;

    while( text[len] != 0 )
    {
        // add emtpy element to the array
        JsonElem& elem = this->add( nullptr );

        // parse/load element
        int used = elem.load( text + len, depth );
        if( used > 0 )
            len += used;
        else
            break;  // parsing failed

        // check next element
        while( IS_SPACE( text[len] ) )
            len++;
        if( text[len] == ',' )          // next element avaiable
            len++;
        else if( text[len] == ']' )     // end of array
            return len + 1;
        else
            break;  // parsing failed
    }

    this->clear();
    return -1;
}

// parse and load from JSON text, return character count consumed, return -1 if failed
int JsonObject::load( const char* text, int depth )
{
    // clear old members if exists
    if( m_count > 0 )
        this->clear();

    // skip leading white-space
    int len = 0;
    while( IS_SPACE( text[len] ) )
        len++;

    // is JSON Object
    if( text[len++] != '{' )
        return -1;

    // skip white-space
    while( IS_SPACE( text[len] ) )
        len++;

    // check empty JSON Object
    if( text[len] == '}' )
        return len + 1;

    // check depth, prevent stack overflow
    if( depth++ > JsonDoc::kMaxDepth )
        return -1;

    while( text[len] != 0 )
    {
        // add empty member to the obj
        JsonMember& member = this->add(nullptr, nullptr);

        // parse/load member
        int used = member.load( text + len, depth );
        if( used > 0 )
            len += used;
        else
            break;  // parsing failed

        // check next member
        while( IS_SPACE( text[len] ) )
            len++;
        if( text[len] == ',' )          // next member avaiable
            len++;
        else if( text[len] == '}' )     // end of obj
            return len + 1;
        else
            break;  // parsing failed
    }

    this->clear();
    return -1;
}

// parse and load from JSON text, return character count consumed, return -1 if failed
int JsonValue::load( const char* text, int depth )
{
    // skip leading white-space
    int len = 0;
    while( IS_SPACE( *text ) )
    {
        text++;
        len++;
    }

    if( *text == '{' )          // JSON Object
    {
        JsonObject& obj = this->make_empty_object();
        int used = obj.load( text, depth );
        if( used > 0 )
            len += used;
        else
            len = -1;
    }
    else if( *text == '[' )     // JSON Array
    {
        JsonArray& ary = this->make_empty_array();
        int used = ary.load( text, depth );
        if( used > 0 )
            len += used;
        else
            len = -1;
    }
    else if( *text == '\"' )    // string
    {
        JsonString& str = this->make_empty_string();
        int used = str.load( text );
        if( used > 0 )
            len += used;
        else
            len = -1;
    }
    else
    {
        // clear old value
        this->clear();

        if( *text == '-' || IS_DIGIT(*text) )   // number
        {
            // try to parse interger first
            const char* end = nullptr;
            int64_t i64 = ::strtoll( text, (char**)&end, 10 );
            if( *end == '.' || *end == 'e' || *end == 'E' )     // float number, re-parse
            {
                double f64 = ::strtod( text, (char**)&end );
                m_type = Json_double;
                m_value.f64 = f64;
            }
            else
            {
                if( i64 == INT64_MAX && text[0] != '-' && errno == ERANGE ) // interger overflow
                {
                    m_type = Json_uint64;
                    m_value.u64 = ::strtoull( text, (char**)&end, 10 );
                }
                else
                {
                    m_type = Json_int64;
                    m_value.i64 = i64;
                }
            }

            int used = static_cast<int>(end - text);
            if( used > 0 )
                len += used;
            else
                len = -1;
        }
        else if( strncmp( text, "true", 4 )==0 )
        {
            m_value.flag = true;
            m_type = Json_boolean;
            len += 4;
        }
        else if( strncmp( text, "false", 5 )==0 )
        {
            m_value.flag = false;
            m_type = Json_boolean;
            len += 5;
        }
        else if( strncmp( text, "null", 4 )==0 )
        {
            m_type = Json_null;
            len += 4;
        }
        else    // unknow value
        {
            len = -1;
        }
    }

    if( len < 0 )   // parsing failed
    {
        this->clear();
    }
    return len;
}

//======================================================================================================================
// JSON document

int JsonDoc::load_file( CFile& file )
{
    if( !file )
        return -1;

    // check file size
    int64_t filesz = file.file_size();
    if( filesz < 0 || filesz > 128 * 1024 * 1024 )  // this utility is not for huge file!
        return -1;
   
    // read file into temp memory, add padding
    MallocedBuf mBuf;
    uint8_t* text = (uint8_t*)mBuf.alloc( filesz + 4 ); // must use uint8_t to compare to BOM
    size_t readcnt = ::fread( text, 1, filesz, file.get() );
    if( readcnt == 0 )
        return -1;
    text[readcnt] = 0;
    text[readcnt+1] = 0;

    // check BOM, has padding, so always safe
    if( text[0]==0xEF && text[1]==0xBB && text[2]==0xBF )   // UTF-8
    {
        text += 3;      // skip BOM
    }
    else if( text[0]==0xFF && text[1]==0xFE )   // UTF-16, little-endian
    {
        return -1;
    }
    else if( text[0]==0xFE && text[1]==0xFF )   // UTF-16, big-endian
    {
        return -1;
    }

    // parse json text
    return this->load_text( (char*)text );
}

int JsonDoc::write_file( CFile& file, const std::string& text )
{
    if( file )
    {
        int cnt = ::fputs( text.c_str(), file.get() );
        cnt;
    }
    return -1;
}

// parse Json file
int JsonDoc::load_file( const char* filepath )
{
    // open file
    CFile file( filepath, "r" );
    return this->load_file( file );
}

// parse Json string
int JsonDoc::load_text( const char* text )
{
    int parsed = m_root.load( text );
    if( parsed < 0 )    // parsing failed
        m_root.clear();

    return parsed;
}

// dump Json Document to UTF-8 file
int JsonDoc::dump_file( const char* filename )
{
    string tmpstr;
    if( this->dump_text( tmpstr ) < 0 )
        return -1;

    CFile file( filename, "w" );
    return this->write_file( file, tmpstr );
}

// ditto, slower but with pretty indentation
int JsonDoc::pretty_dump_file( const char* filename )
{
    string tmpstr;
    if( this->pretty_dump_text( tmpstr ) < 0 )
        return -1;

    CFile file( filename, "w" );
    return this->write_file( file, tmpstr );
}

// dump Json Document to UTF-8 string
int JsonDoc::dump_text( std::string& out )
{
    if( m_root.is_empty() )
        return -1;

    out.clear();
    out.reserve( 4096 );
    m_root.dump( out );
    return static_cast<int>( out.length() );
}

// ditto, slower but with pretty indentation
int JsonDoc::pretty_dump_text( std::string& out )
{
    if( m_root.is_empty() )
        return -1;

    out.clear();
    out.reserve( 4096 );
    m_root.pretty_dump( out );
    return static_cast<int>( out.length() );
}

#ifdef _WIN32

int JsonDoc::load_file( const wchar_t* filepath )
{
    CFile file( filepath, L"r" );
    return this->load_file( file );
}

int JsonDoc::dump_file( const wchar_t* filename )
{
    string tmpstr;
    if( this->dump_text( tmpstr ) < 0 )
        return -1;

    CFile file( filename, L"w" );
    return this->write_file( file, tmpstr );
}

int JsonDoc::pretty_dump_file( const wchar_t* filename )
{
    string tmpstr;
    if( this->pretty_dump_text( tmpstr ) < 0 )
        return -1;

    CFile file( filename, L"w" );
    return this->write_file( file, tmpstr );
}

#endif

} // namespace irk
