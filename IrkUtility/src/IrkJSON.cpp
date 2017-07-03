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
#include "IrkMemPool.h"
#include "IrkStringUtility.h"
#include "IrkCFile.h"
#include "IrkJSON.h"

using std::string;

namespace irk {

// memory allocator used to allocate JSON String, Element, Member, Array, Object
class JsonAlloc
{
public:
    JsonAlloc() : m_mpool(16*1024) {}

    // create new object
    template<class Ty, class... Args>
    Ty* create( Args&&... args )
    {
        void* ptr = m_mpool.alloc( sizeof(Ty), alignof(Ty) );
        return ::new(ptr) Ty( std::forward<Args>(args)... );    // inplace new
    }
    // delete object
    template<class Ty>
    void trash( Ty* ptr )
    {
        if( ptr )
        {
            ptr->~Ty();
            m_mpool.dealloc( ptr, sizeof(Ty) );
        }
    }

    // allocate a memory block
    void* alloc( size_t size, size_t alignment = sizeof(void*) )
    {
        return m_mpool.alloc( size, alignment );
    }
    // deallocate memory block returned from alloc()
    void dealloc( void* ptr, size_t size )
    {
        m_mpool.dealloc( ptr, size );
    }
    // real buffer capacity for the requested size
    size_t bucket_size( size_t size ) const
    {
        return m_mpool.bucket_size( size );
    }

    // release all memory
    void clear()    
    { 
        m_mpool.clear();
    }
private:
    MemPool m_mpool;
};

//======================================================================================================================
// JSON String

class JsonString
{
    static const int kInnerSize = 16;   // for small string optimization
public:
    explicit JsonString( JsonAlloc* alloc ) : m_allocator(alloc), m_blksize(kInnerSize) {}
    explicit JsonString( const char* str, int len, JsonAlloc* alloc ) : JsonString(alloc)
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

    // parse and fill
    int parse( const char* text );

    // return allocator
    JsonAlloc* allocator()  { return m_allocator; }

    // create new string from allocator
    static JsonString* make( JsonAlloc* alloc )
    {
        return alloc->create<JsonString>( alloc );
    }
    static JsonString* make( const char* str, int len, JsonAlloc* alloc )
    {
        return alloc->create<JsonString>( str, len, alloc );
    }

private:
    JsonAlloc*  m_allocator;            // extern block's allocator
    int32_t     m_blksize;              // memory block size
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
        m_allocator->dealloc( m_block, (size_t)m_blksize );
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

        size_t capacity = m_allocator->bucket_size( size );
        m_block = (char*)m_allocator->alloc( capacity );
        m_blksize = (int)capacity;
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

JsonValue::JsonValue( JsonValue&& other ) noexcept
{
    m_type = other.m_type;
    m_value = other.m_value;
    m_allocator = other.m_allocator;
    other.m_type = Json_invalid;
}

JsonValue& JsonValue::operator=( JsonValue&& other ) noexcept
{
    if( this != &other )    // protect from the mad
    {
        this->clear();
        m_type = other.m_type;
        m_value = other.m_value;
        m_allocator = other.m_allocator;
        other.m_type = Json_invalid;
    }
    return *this;
}

// delete non-scalar value stored in the JSON Value
void JsonValue::clear()
{
    if( m_type > Json_scalar )
    {
        assert( m_allocator != nullptr );

        if( m_type == Json_string )
        {
            m_allocator->trash( m_value.str );
        }
        else if( m_type == Json_object )
        {
            m_allocator->trash( m_value.obj );
        }
        else if( m_type == Json_array )
        {
            m_allocator->trash( m_value.ary );
        }
    }
    m_type = Json_invalid;
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

// set string, str must be null-terminated if len < 0
void JsonValue::set( const char* str, int len )
{
    if( m_type == Json_string )		// already contains string
    {
        m_value.str->assign( str, len );
    }
    else
    {
        assert( m_allocator );
        this->clear();
        m_value.str = JsonString::make( str, len, m_allocator );
        m_type = Json_string;
    }
}
void JsonValue::set( const std::string& str )
{
    this->set( str.c_str(), (int)str.length() );
}

// make as empty JSON String, return reference to the String
JsonString& JsonValue::make_empty_string()
{
    if( m_type == Json_string )		// already contains string
    {
        m_value.str->clear();
    }
    else
    {
        assert( m_allocator );
        this->clear();
        m_value.str = JsonString::make( m_allocator );
        m_type = Json_string;
    }
    return *m_value.str;
}

// make as empty JSON Object, return reference to the Object
JsonObject& JsonValue::make_empty_object()
{
    if( m_type == Json_object ) // already contains JSON Object
    {
        m_value.obj->clear();	// make empty
    }
    else
    {
        assert( m_allocator );
        this->clear();
        m_type = Json_object;
        m_value.obj = m_allocator->create<JsonObject>( m_allocator );
    }
    return *m_value.obj;
}

// make as empty JSON Array, return reference to the Array
JsonArray& JsonValue::make_empty_array()
{
    if( m_type == Json_array )	// already contains JSON Array
    {
        m_value.ary->clear();	// make empty
    }
    else
    {
        assert( m_allocator );
        this->clear();
        m_type = Json_array;
        m_value.ary = m_allocator->create<JsonArray>( m_allocator );
    }
    return *m_value.ary;
}

//======================================================================================================================
// JSON Array

JsonElem::JsonElem(JsonArray* parent) 
    : JsonValue(nullptr, parent->allocator()), m_pNext(nullptr), m_parent(parent)
{}

// delete all elements
void JsonArray::clear()
{
    JsonElem* curr = m_slist.m_firstNode;
    while( curr )
    {
        JsonElem* next = curr->next_sibling();
        m_allocator->trash( curr );
        curr = next;
    }
    m_count = 0;
    m_slist.clear();
}

// get idx'th element, return nullptr if failed
// WARNING: O(N) sequential search, for traversal use iterator instead
JsonElem* JsonArray::find( int idx )
{
    return m_slist.find( idx );
}
const JsonElem* JsonArray::find( int idx ) const
{
    return m_slist.find( idx );
}

// add a null element to the tail of array, return reference to the new element
JsonElem& JsonArray::add( std::nullptr_t )
{
    JsonElem* pElem = m_allocator->create<JsonElem>( this );
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
        m_allocator->trash( victim );
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
        m_allocator->trash( victim );
        m_count--;
        return true;
    }
    return false;
}

//======================================================================================================================
// JSON Members in JSON Object

JsonMember::JsonMember( JsonObject* parent ) 
    : JsonValue(nullptr, parent->allocator()), m_pName(nullptr), m_pNext(nullptr), m_parent(parent)
{
    m_pName = JsonString::make( parent->allocator() );
}

JsonMember::JsonMember( const char* name, JsonObject* parent )
    : JsonValue(nullptr, parent->allocator()), m_pName(nullptr), m_pNext(nullptr), m_parent(parent)
{
    m_pName = JsonString::make( name, -1, parent->allocator() );
}

JsonMember::JsonMember( const std::string& name, JsonObject* parent )
    : JsonValue(nullptr, parent->allocator()), m_pName(nullptr), m_pNext(nullptr), m_parent(parent)
{
    m_pName = JsonString::make( name.c_str(), (int)name.length(), parent->allocator() );
}

JsonMember::~JsonMember()
{
    m_allocator->trash( m_pName );
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

// delete all members
void JsonObject::clear()
{
    JsonMember* curr = m_slist.m_firstNode;
    while( curr )
    {
        JsonMember* next = curr->next_sibling();
        m_allocator->trash( curr );
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
JsonMember& JsonObject::add_null()
{
    JsonMember* pmemb = m_allocator->create<JsonMember>( this );
    m_slist.push_back( pmemb );
    m_count++;
    return *pmemb;
}
JsonMember& JsonObject::add( const char* name, std::nullptr_t )
{
    JsonMember* pmemb = m_allocator->create<JsonMember>( name, this );
    m_slist.push_back( pmemb );
    m_count++;
    return *pmemb;
}
JsonMember& JsonObject::add( const std::string& name, std::nullptr_t )
{
    JsonMember* pmemb = m_allocator->create<JsonMember>( name, this );
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
        m_allocator->trash( victim );
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
        m_allocator->trash( victim );
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
        else if( *src < 0x20 )	// control character
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
        else	// normal character
        {
            dst.push_back( (char)*src );
        }
        src++;
    }
}

// to JSON text, add to the tail of the <outStr>
void JsonValue::format( std::string& outStr )
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
        str_format( sbuf, bufsize, "%0.9g", m_value.f64 );
        outStr += sbuf;
        break;
    case Json_string:
        outStr += '\"';
        to_json_text( m_value.str->data(), outStr );
        outStr += '\"';
        break;
    case Json_object:
        m_value.obj->format( outStr );
        break;
    case Json_array:
        m_value.ary->format( outStr );
        break;
    default:
        break;
    }
}

// to JSON text, add to the tail of the <outStr>
void JsonValue::format( std::string& outStr, int indent )
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
        str_format( sbuf, bufsize, "%0.9g", m_value.f64 );
        outStr += sbuf;
        break;
    case Json_string:
        outStr += '\"';
        to_json_text( m_value.str->data(), outStr );
        outStr += '\"';
        break;
    case Json_object:
        m_value.obj->format( outStr, indent );
        break;
    case Json_array:
        m_value.ary->format( outStr, indent );
        break;
    default:
        break;
    }
}

// to JSON text, add to the tail of the <outStr>
void JsonMember::format( std::string& outStr )
{
    outStr += '\"';
    to_json_text( m_pName->data(), outStr );
    outStr += "\": ";
    JsonValue::format( outStr );
}

// to JSON text, add to the tail of the <outStr>
void JsonMember::format( std::string& outStr, int indent )
{
    outStr += '\"';
    to_json_text( m_pName->data(), outStr );
    outStr += "\": ";
    JsonValue::format( outStr, indent );
}

// to JSON text, add to the tail of the <outStr>
void JsonArray::format( std::string& outStr )
{
    outStr += "[\n";

    JsonElem* curr = m_slist.m_firstNode;
    if( curr )
    {
        // the first element
        curr->format( outStr );
        curr = curr->next_sibling();

        // other elements
        while( curr )
        {
            outStr += ",\n";
            curr->format( outStr );
            curr = curr->next_sibling();
        }

        outStr += '\n';
    }

    outStr += "]";
}

// to JSON text, add to the tail of the <outStr>
void JsonArray::format( std::string& outStr, int indent )
{
    outStr += "[\n";

    JsonElem* curr = m_slist.m_firstNode;
    if( curr )
    {
        const int indent2 = MIN(indent + 2, JsonDoc::kMaxIndent);

        // the first element
        outStr.append( indent2, ' ' );
        curr->format( outStr, indent2 );
        curr = curr->next_sibling();

        // other elements
        while( curr )
        {
            outStr += ",\n";
            outStr.append( indent2, ' ' );
            curr->format( outStr, indent2 );
            curr = curr->next_sibling();
        }

        outStr += '\n';
    }

    outStr.append( indent, ' ' );
    outStr += "]";
}

// to JSON text, add to the tail of the <outStr>
void JsonObject::format( std::string& outStr )
{
    outStr += "{\n";

    JsonMember* curr = m_slist.m_firstNode;
    if( curr )
    {
        // the first element
        curr->format( outStr );
        curr = curr->next_sibling();

        // other elements
        while( curr )
        {
            outStr += ",\n";
            curr->format( outStr );
            curr = curr->next_sibling();
        }

        outStr += '\n';
    }

    outStr += "}";
}

// to JSON text, add to the tail of the <outStr>
void JsonObject::format( std::string& outStr, int indent )
{
    outStr += "{\n";

    JsonMember* curr = m_slist.m_firstNode;
    if( curr )
    {
        const int indent2 = MIN(indent + 2, JsonDoc::kMaxIndent);

        // the first member
        outStr.append( indent2, ' ' );
        curr->format( outStr, indent2 );
        curr = curr->next_sibling();

        // other member
        while( curr )
        {
            outStr += ",\n";
            outStr.append( indent2, ' ' );
            curr->format( outStr, indent2 );
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

// parse and fill from JSON text, return character count consumed, return -1 if failed
int JsonString::parse( const char* text )
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
        else if( text[srcLen] != '\\' )	    // normal character
        {
            srcLen++;
            dstLen++;
        }
        else	// escape character
        {
            if( text[srcLen+1] == 0 )	// invalid string
                return -1;

            if( text[srcLen+1] == 'u' )	// unicode code number
            {
                for( int j = srcLen + 2; j < srcLen + 6; j++ )
                {
                    if( !IS_XDIGIT( text[j] ) )
                        return -1;
                }
                srcLen += 6;
                dstLen += 4;	// a UTF-8 char may need 4 bytes
            }
            else
            {
                srcLen += 2;
                dstLen++;
            }
        }
    }
    if( text[srcLen] != '\"' )	// can't find ending \", invalid string
    {
        return -1;
    }

    if( dstLen == srcLen )	    // normal string, no conversion needed
    {
        this->assign( text, srcLen );
        return srcLen + 2;
    }

    // unescape JSON text
    char* dst = this->alloc( dstLen + 1 );
    for( int k = 0; k < srcLen; )
    {
        if( text[k] != '\\' )	// normal character
        {
            *dst++ = text[k++];
        }
        else	// escape character
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
            else if( text[k+1] == 'u' )		// unicode code number
            {
                // convert to UTF-8
                int cnt = json_escape_to_utf8( text + k, dst );
                if( cnt <= 0 )
                    return -1;
                else if( cnt > 3 )	// 4-bytes UTF-8
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

// parse and fill from JSON text, return character count consumed, return -1 if failed
int JsonMember::parse( const char* text, int depth )
{
    int len = 0;

    // fill member's name
    while( IS_SPACE( text[len] ) )  // skip space
        len++;
    if( text[len] != '\"' )		    // invalid, must be string
        return -1;
    int used = m_pName->parse( text + len );
    if( used > 0 )
        len += used;
    else
        return -1;		// parsing failed

    // fill member's value
    while( IS_SPACE( text[len] ) )  // skip space
        len++;
    if( text[len++] != ':' )	    // skip colon
        return -1;
    used = JsonValue::parse( text + len, depth );
    if( used > 0 )
        len += used;
    else
        return -1;		// parsing failed

    return len;
}

// parse and fill from JSON text, return character count consumed, return -1 if failed
int JsonArray::parse( const char* text, int depth )
{
    // skip leading white-space
    assert( text[0] == '[' );
    int len = 1;
    while( IS_SPACE( text[len] ) )
        len++;

    // check empty array
    if( text[len] == ']' )
        return len + 1;

    // check depth, prevent stack overflow
    if( depth++ > JsonDoc::kMaxDepth )
        return -1;

    while( text[len] != 0 )
    {
        // add emtpy element to the array
        JsonElem& elem = this->add( nullptr );

        // parse/fill element
        int used = elem.parse( text + len, depth );
        if( used > 0 )
            len += used;
        else
            break;	// parsing failed

        // check next element
        while( IS_SPACE( text[len] ) )
            len++;
        if( text[len] == ',' )          // next element avaiable
            len++;
        else if( text[len] == ']' )     // end of array
            return len + 1;
        else
            return -1;
    }

    this->clear();
    return -1;
}

// parse and fill from JSON text, return character count consumed, return -1 if failed
int JsonObject::parse( const char* text, int depth )
{
    // skip leading white-space
    assert( text[0] == '{' );
    int len = 1;
    while( IS_SPACE( text[len] ) )
        len++;

    // check empty obj
    if( text[len] == '}' )
        return len + 1;

    // check depth, prevent stack overflow
    if( depth++ > JsonDoc::kMaxDepth )
        return -1;

    while( text[len] != 0 )
    {
        // add empty member to the obj
        JsonMember& member = this->add_null();

        // parse/fill member
        int used = member.parse( text + len, depth );
        if( used > 0 )
            len += used;
        else
            break;	// parsing failed

        // check next member
        while( IS_SPACE( text[len] ) )
            len++;
        if( text[len] == ',' )          // next member avaiable
            len++;
        else if( text[len] == '}' )     // end of obj
            return len + 1;
        else
            return -1;
    }

    // parsing failed
    this->clear();
    return -1;
}

// parse JSON Number, return character count consumed, return -1 if failed
int JsonValue::parse_number( const char* text )
{
    assert( m_type == Json_invalid || m_type == Json_null );
    const char* end = text;

    // try to parse interger first
    int64_t i64 = ::strtoll( text, (char**)&end, 10 );

    if( *end == '.' || *end == 'e' || *end == 'E' ) // float number, re-parse
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

    return static_cast<int>(end - text);
}

// parse JSON value, return character count consumed, return -1 if failed
int JsonValue::parse( const char* text, int depth )
{
    assert( m_type == Json_invalid || m_type == Json_null );

    // skip leading white-space
    int len = 0;
    while( IS_SPACE( *text ) )
    {
        text++;
        len++;
    }

    if( *text == '-' || (*text >= '0' && *text <= '9') )    // number
    {
        int used = this->parse_number( text );
        if( used > 0 )
            len += used;
        else
            len = -1;
    }
    else if( *text == '\"' )	// string
    {
        JsonString& str = this->make_empty_string();
        int used = str.parse( text );
        if( used > 0 )
            len += used;
        else
            len = -1;
    }
    else if( *text == '{' )		// JSON Object
    {
        JsonObject& obj = this->make_empty_object();
        int used = obj.parse( text, depth );
        if( used > 0 )
            len += used;
        else
            len = -1;
    }
    else if( *text == '[' )		// JSON Array
    {
        JsonArray& ary = this->make_empty_array();
        int used = ary.parse( text, depth );
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

    if( len < 0 )	// parsing failed
    {
        this->clear();
    }
    return len;
}

//======================================================================================================================
JsonDoc::JsonDoc() : m_allocator(nullptr)
{
    m_allocator = new JsonAlloc;
    m_root.m_allocator = m_allocator;
}

JsonDoc::~JsonDoc()
{
    m_root.clear();         // must called before deleting allocator
    delete m_allocator;
}

JsonStatus JsonDoc::parse_file( CFile& file )
{
    if( !file  )
        return JsonStatus::IOFailed;

    // check file size
    const int64_t filesz = file.file_size();
    if( filesz < 0 )
        return JsonStatus::IOFailed;
    else if( filesz > 128 * 1024 * 1024 )   // this utility is not for huge file!
        return JsonStatus::Unsupported;
    
    // read file into temp memory, add padding
    MallocedBuf mBuf;
    uint8_t* text = (uint8_t*)mBuf.alloc( (int)filesz + 4 );    // must use uint8_t to compare to BOM
    size_t readcnt = fread( text, 1, filesz, file );
    if( readcnt == 0 )
        return JsonStatus::IOFailed;
    text[readcnt] = 0;
    text[readcnt+1] = 0;

    // check BOM, has padding, so always safe
    if( text[0]==0xEF && text[1]==0xBB && text[2]==0xBF )   // UTF-8
    {
        text += 3;      // skip BOM
    }
    else if( text[0]==0xFF && text[1]==0xFE )   // UTF-16, little-endian
    {
        return JsonStatus::Unsupported;
    }
    else if( text[0]==0xFE && text[1]==0xFF )   // UTF-16, big-endian
    {
        return JsonStatus::Unsupported;
    }

    // parse json text
    return parse_text( (char*)text );
}

JsonStatus JsonDoc::write_file( CFile& file, const std::string& text )
{
    if( file )
    {
        if( fprintf( file, "%s", text.c_str() ) > 0 )
            return JsonStatus::Ok;
    }
    return JsonStatus::IOFailed;
}

// parse Json file
JsonStatus JsonDoc::parse_file( const char* filepath )
{
    // open file
    CFile file( filepath, "r" );
    return this->parse_file( file );
}

// parse Json string
JsonStatus JsonDoc::parse_text( const char* text )
{
    m_root.clear();     // remove old doc(if exists)

    int parsed = m_root.parse( text, 0 );
    if( parsed <= 0 )	// parsing failed
    {
        m_root.clear();
        return JsonStatus::Invalid;
    }

    // if more text exists, return error but does not delete parsed content
    for( ; text[parsed] != 0; parsed++ )
    {
        if( !IS_CTLWS( text[parsed] ) )
            return JsonStatus::Invalid;
    }

    return JsonStatus::Ok;
}

// dump Json Document to UTF-8 file, return 0 if succeeded, error code defined in JsonErr
JsonStatus JsonDoc::dump_file( const char* filename )
{
    string tmpstr;
    auto res = dump_text( tmpstr );
    if( res != JsonStatus::Ok )
        return res;

    CFile file( filename, "w" );
    return this->write_file( file, tmpstr );
}

// ditto, slower but indented nicely
JsonStatus JsonDoc::pretty_dump_file( const char* filename )
{
    string tmpstr;
    auto res = pretty_dump_text( tmpstr );
    if( res != JsonStatus::Ok )
        return res;

    CFile file( filename, "w" );
    return this->write_file( file, tmpstr );
}

// dump Json Document to UTF-8 string, return 0 if succeeded
JsonStatus JsonDoc::dump_text( std::string& out )
{
    if( !m_root.is_valid() )
        return JsonStatus::Invalid;

    out.clear();
    out.reserve( 4096 );
    m_root.format( out );
    return JsonStatus::Ok;
}

// ditto, slower but indented nicely
JsonStatus JsonDoc::pretty_dump_text( std::string& out )
{
    if( !m_root.is_valid() )
        return JsonStatus::Invalid;

    out.clear();
    out.reserve( 4096 );
    m_root.format( out, 0 );
    return JsonStatus::Ok;
}

#ifdef _WIN32

JsonStatus JsonDoc::parse_file( const wchar_t* filepath )
{
    CFile file( filepath, L"r" );
    return this->parse_file( file );
}

JsonStatus JsonDoc::dump_file( const wchar_t* filename )
{
    string tmpstr;
    auto res = dump_text( tmpstr );
    if( res != JsonStatus::Ok )
        return res;

    CFile file( filename, L"w" );
    return this->write_file( file, tmpstr );
}

JsonStatus JsonDoc::pretty_dump_file( const wchar_t* filename )
{
    string tmpstr;
    auto res = pretty_dump_text( tmpstr );
    if( res != JsonStatus::Ok )
        return res;

    CFile file( filename, L"w" );
    return this->write_file( file, tmpstr );
}

#endif

} // namespace irk
