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

#ifndef _IRONBRICK_JSON_H_
#define _IRONBRICK_JSON_H_

#include <stdint.h>
#include <assert.h>
#include <cstddef>
#include <utility>
#include <new>
#include <string>
#include <initializer_list>
#include <tuple>
#include "IrkMemPool.h"
#include "detail/ImplJsonXml.hpp"

//
// Parser + Writer for small JSON file, according to RFC-7159
//
namespace irk {

class JsonAllocator
{
public:
    virtual ~JsonAllocator() {}
    virtual void* alloc(size_t size, size_t alignment) = 0;
    virtual void dealloc(void* ptr, size_t size) = 0;

    template<class Ty, class... Args>
    Ty* create(Args&&... args)
    {
        void* ptr = this->alloc(sizeof(Ty), alignof(Ty));
        return ::new(ptr) Ty{std::forward<Args>(args)...};
    }
    template<class Ty>
    void trash(Ty* ptr)
    {
        if (ptr)
        {
            ptr->~Ty();
            this->dealloc(ptr, sizeof(Ty));
        }
    }
};

// JSON default allocator, using std new/delete
class JsonDefaultAllocator : public JsonAllocator
{
public:
    void* alloc(size_t size, size_t alignment) override
    {
        assert(alignment <= sizeof(std::max_align_t));
        return ::operator new(size);
    }
    void dealloc(void* ptr, size_t /*size*/) override
    {
        ::operator delete(ptr);
    }
};

// JSON optimized allocator, using memory pool
class JsonMempoolAllocator : public JsonAllocator
{
public:
    explicit JsonMempoolAllocator(size_t size = 16 * 1024) : m_pool(size) {}
    void* alloc(size_t size, size_t alignment) override
    {
        return m_pool.alloc(size, alignment);
    }
    void dealloc(void* ptr, size_t size) override
    {
        m_pool.dealloc(ptr, size);
    }
private:
    MemPool m_pool;
};

// Exception throwed if access JSON value incorrectly
class JsonBadAccess : public std::logic_error
{
public:
    JsonBadAccess() : std::logic_error("JSON bad access") {}
};

class JsonValue;    // JSON Value
class JsonArray;    // JSON Array
class JsonObject;   // JSON Object
class JsonString;   // JSON String

//======================================================================================================================
// JSON value, may be JSON Null, Boolean, String, Number, Array, Object

class JsonValue
{
public:
    // construct a JSON Null value using default allocator
    explicit JsonValue(std::nullptr_t);
    // construct a JSON Null using external allocator
    // NOTE: user must keep allocator alive during the lifetime of this object
    explicit JsonValue(std::nullptr_t, JsonAllocator* alloc);

    // construct an empty/invalid JSON value
    // NOTE: empty value is invalid and can't be outputed, it's only for implemention usage
    explicit JsonValue();
    explicit JsonValue(JsonAllocator* alloc);

    // destructor no need to be virtual
    ~JsonValue() { this->clear(); }

    // copy, move, assignment, move assignment
    JsonValue(const JsonValue&);
    JsonValue(JsonValue&&);
    JsonValue& operator=(const JsonValue&);
    JsonValue& operator=(JsonValue&&);

    // parse and load from JSON text
    // return character count parsed, return -1 if failed
    // NOTE: text must be null-terminated, depth is for internal usage
    int load(const char* text, int depth = 0);

    // dump to JSON text
    // NOTE: add text to the tail of the output string
    void dump(std::string& out) const;

    // dump to JSON text with pretty indentation
    // NOTE: add text to the tail of the output string
    void pretty_dump(std::string& out, int indent = 0) const;

    // value type query
    bool is_null() const    { return m_type == Json_null; }
    bool is_bool() const    { return m_type == Json_boolean; }
    bool is_number() const  { return m_type == Json_int64 || m_type == Json_uint64 || m_type == Json_double; }
    bool is_string() const  { return m_type == Json_string; }
    bool is_object() const  { return m_type == Json_object; }
    bool is_array() const   { return m_type == Json_array; }
    bool is_empty() const   { return m_type == Json_empty; }

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
    JsonObject&         as_object();
    const JsonArray&    as_array() const;
    JsonArray&          as_array();

    // ditto, but return false if type does not match, never throw
    bool get(bool& outFlag) const;
    bool get(int32_t& outValue) const;
    bool get(uint32_t& outValue) const;
    bool get(int64_t& outValue) const;
    bool get(uint64_t& outValue) const;
    bool get(float& outValue) const;
    bool get(double& outValue) const;
    bool get(std::string& outStr) const;
    bool get(const JsonObject*& outObj) const;
    bool get(JsonObject*& outObj);
    bool get(const JsonArray*& outAry) const;
    bool get(JsonArray*& outAry);

    // assign new value
    void set(std::nullptr_t)
    {
        this->clear();
        m_type = Json_null;
    }
    void set(bool flag)
    {
        this->clear();
        m_type = Json_boolean;
        m_value.flag = flag;
    }
    void set(int value)
    {
        this->clear();
        m_type = Json_int64;
        m_value.i64 = value;
    }
    void set(unsigned int value)
    {
        this->clear();
        m_type = Json_uint64;
        m_value.u64 = value;
    }
    void set(int64_t value)
    {
        this->clear();
        m_type = Json_int64;
        m_value.i64 = value;
    }
    void set(uint64_t value)
    {
        this->clear();
        m_type = Json_uint64;
        m_value.u64 = value;
    }
    void set(double value)
    {
        this->clear();
        m_type = Json_double;
        m_value.f64 = value;
    }

    // assign string, string must be null-terminated if len < 0
    void set(const char* str, int len = -1);
    void set(const std::string& str);

    // assign JSON Object, return reference to the new Object
    JsonObject& set(const JsonObject&);

    // assign JSON Array, return reference to the new Array
    JsonArray& set(const JsonArray&);

    // ditto, for convenience
    template<typename Ty>
    void operator=(const Ty& val)
    {
        this->set(val);
    }

    // make as empty JSON String
    JsonString& make_empty_string();

    // make as empty JSON Object
    JsonObject& make_empty_object();

    // make as empty JSON Array
    JsonArray& make_empty_array();

    // clear self, set to empty/invalid state
    void clear();

protected:
    enum
    {
        Json_empty = 0,     // Empty/Invalid tag, for internal usage
        Json_null,          // JSON null type
        Json_boolean,       // JSON Boolean
        Json_int64,         // JSON Number
        Json_uint64,
        Json_double,
        Json_scalar = 100,  // Scalar tag, for internal usage
        Json_string,        // JSON String
        Json_array,         // JSON Array
        Json_object,        // JSON Object
    };
    union ValUnion
    {
        bool        flag;
        int64_t     i64;
        uint64_t    u64;
        double      f64;
        JsonString* str;
        JsonArray*  ary;
        JsonObject* obj;
    };
    int             m_type;
    ValUnion        m_value;
    JsonAllocator*  m_alloc;    // internal memory allocator
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
    const JsonArray* parent() const { return m_parent; }
    JsonArray* parent()             { return m_parent; }

private:
    friend JsonAllocator;
    friend detail::SList<JsonElem>;

    // no public constructor, must be created by parent JSON Array 
    explicit JsonElem(JsonArray* parent);
    ~JsonElem() {}
    JsonElem(const JsonElem&) = delete;
    JsonElem& operator=(const JsonElem&) = delete;

    JsonElem*   m_pNext;    // next element in parent JSON Array
    JsonArray*  m_parent;   // parent JSON Array
};

// JSON Array
class JsonArray
{
public:
    // construct an empty JSON array using default allocator
    explicit JsonArray();

    // construct an empty JSON array using external allocator
    // NOTE: user must keep allocator alive during the lifetime of this object
    explicit JsonArray(JsonAllocator* alloc);

    // destructor clear all elements
    ~JsonArray() { this->clear(); }

    // copy, move, assignment, move assignment
    JsonArray(const JsonArray&);
    JsonArray(JsonArray&&);
    JsonArray& operator=(const JsonArray&);
    JsonArray& operator=(JsonArray&&);

    // parse and load from JSON text
    // return character count parsed, return -1 if failed
    // NOTE: text must be null-terminated, depth is for internal usage
    int load(const char* text, int depth = 0);

    // dump to JSON text
    // NOTE: add text to the tail of the output string
    void dump(std::string& out) const;

    // dump to JSON text with pretty indentation
    // NOTE: add text to the tail of the output string
    void pretty_dump(std::string& out, int indent = 0) const;

    // children traversal
    typedef detail::JXIterator<JsonElem> Iterator;          // forward iterator
    typedef detail::JXIterator<const JsonElem> CIterator;   // forward const iterator
    Iterator begin()        { return Iterator(m_slist.m_firstNode); }
    Iterator end()          { return Iterator(nullptr); }
    CIterator begin() const { return CIterator(m_slist.m_firstNode); }
    CIterator end() const   { return CIterator(nullptr); }

    // number of elements
    int count() const               { return m_count; }

    // return the first element, return nullptr if array is empty 
    const JsonElem* first() const   { return m_slist.m_firstNode; }
    JsonElem* first()               { return m_slist.m_firstNode; }

    // find idx'th element
    // WARNING: use sequential search, for traversal using iterator instead.
    JsonElem& operator[](int idx)
    {
        assert(idx >= 0 && idx < m_count);
        JsonElem* pelem = m_slist.find(idx);
        return *pelem;
    }
    const JsonElem& operator[](int idx) const
    {
        assert(idx >= 0 && idx < m_count);
        const JsonElem* pelem = m_slist.find(idx);
        return *pelem;
    }

    // resize the array, if the current count is less than count, additional JSON Null value are appended
    void resize(int count);

    // add a null element to the tail of array, return reference to the new element
    JsonElem& add(std::nullptr_t);

    // add a generic value to the tail of array, return reference to the new element
    template<typename Ty>
    JsonElem& add(const Ty& val)
    {
        JsonElem& elem = this->add(nullptr);
        elem.set(val);
        return elem;
    }

    // add a JSON value to the tail of array, return reference to the new element
    JsonElem& add(const JsonValue& val)
    {
        JsonElem& elem = this->add(nullptr);
        (JsonValue&)elem = val;
        return elem;
    }

    // add an empty JSON Object o the tail of array, return reference to the new Object
    JsonObject& add_object()
    {
        JsonElem& elem = this->add(nullptr);
        return elem.make_empty_object();
    }

    // add an empty JSON Array o the tail of array, return reference to the new Array
    JsonArray& add_array()
    {
        JsonElem& elem = this->add(nullptr);
        return elem.make_empty_array();
    }

    // add sequence of elements in the tuple
    template<typename... Ts>
    void add(const std::tuple<Ts...>& tup)
    {
        this->add_tuple(tup, std::make_index_sequence<sizeof...(Ts)>());
    }

    // ditto, for convenience
    template<typename Ty>
    void operator+=(const Ty& val)
    {
        this->add(val);
    }

    // add sequence of elements of the same type
    template<typename Ty>
    void operator+=(std::initializer_list<Ty> initlist)
    {
        for (const Ty& val : initlist)
            this->add(val);
    }

    // add sequence of elements
    template<typename... Ts>
    void adds(Ts... values)
    {
#if __cplusplus >= 201703L
        ((void)this->add(values), ...);
#else
        char dummy[] = {((void)this->add(values), '0')...};
        (void)dummy;
#endif
    }

    // assignment for convenience
    template<typename Ty>
    void operator=(std::initializer_list<Ty> initlist)
    {
        this->clear();
        for (const Ty& val : initlist)
            this->add(val);
    }
    template<typename... Ts>
    void operator=(const std::tuple<Ts...>& tup)
    {
        this->clear();
        this->add_tuple(tup, std::make_index_sequence<sizeof...(Ts)>());
    }

    // erase element from the array, return false if failed
    // WARNING: O(N) sequential search
    bool erase(int idx);
    bool erase(JsonElem&);

    // delete all elements
    void clear();

    // return allocator
    JsonAllocator* allocator() const { return m_alloc; }

private:
    template<typename... Ts, size_t... Is>
    void add_tuple(const std::tuple<Ts...>& tup, std::index_sequence<Is...>)
    {
#if __cplusplus >= 201703L
        (this->add(std::get<Is>(tup)), ...);
#else
        char dummy[] = {((void)this->add(std::get<Is>(tup)), '0')...};
        (void)dummy;
#endif
    }

    JsonAllocator*          m_alloc;    // memory allocator
    int                     m_count;    // number of elements
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
    void set_name(const char* name);
    void set_name(const std::string& name);

    // get next member in parent JSON Object
    const JsonMember* next_sibling() const  { return m_pNext; }
    JsonMember* next_sibling()              { return m_pNext; }

    // get parent JSON Object
    const JsonObject* parent() const        { return m_parent; }
    JsonObject* parent()                    { return m_parent; }

    // parse and load from JSON text
    // return character count parsed, return -1 if failed
    // NOTE: text must be null-terminated, depth is for internal usage
    int load(const char* text, int depth = 0);

    // dump to JSON text
    // NOTE: add text to the tail of the output string
    void dump(std::string& out) const;

    // dump to JSON text with pretty indentation
    // NOTE: add text to the tail of the output string
    void pretty_dump(std::string& out, int indent = 0) const;

private:
    friend JsonAllocator;
    friend detail::SList<JsonMember>;

    // no public constructor, must be created by parent JSON Object
    JsonMember(const char* name, JsonObject* parent);
    JsonMember(const std::string& name, JsonObject* parent);
    ~JsonMember();
    JsonMember(const JsonMember&) = delete;
    JsonMember& operator=(const JsonMember&) = delete;

    JsonString* m_pName;    // member name
    JsonMember* m_pNext;    // next member in parent JSON Object
    JsonObject* m_parent;   // parent JSON Object
};

// JSON Object
class JsonObject : IrkNocopy
{
public:
    // construct an empty JSON Object using default allocator
    explicit JsonObject();

    // construct an empty JSON Object using external allocator
    // NOTE: user must keep allocator alive during the lifetime of this object
    explicit JsonObject(JsonAllocator* alloc);

    // destructor clear all elements
    ~JsonObject() { this->clear(); }

    // copy, move, assignment, move assignment
    JsonObject(const JsonObject&);
    JsonObject(JsonObject&&);
    JsonObject& operator=(const JsonObject&);
    JsonObject& operator=(JsonObject&&);

    // parse and load from JSON text
    // return character count parsed, return -1 if failed
    // NOTE: text must be null-terminated, depth is for internal usage
    int load(const char* text, int depth = 0);

    // dump to JSON text
    // NOTE: add text to the tail of the output string
    void dump(std::string& out) const;

    // dump to JSON text with pretty indentation
    // NOTE: add text to the tail of the output string
    void pretty_dump(std::string& out, int indent = 0) const;

    // children traversal
    typedef detail::JXIterator<JsonMember> Iterator;        // forward iterator
    typedef detail::JXIterator<const JsonMember> CIterator; // forward const iterator
    Iterator begin()        { return Iterator(m_slist.m_firstNode); }
    Iterator end()          { return Iterator(nullptr); }
    CIterator begin() const { return CIterator(m_slist.m_firstNode); }
    CIterator end() const   { return CIterator(nullptr); }

    // number of members
    int count() const               { return m_count; }

    // return the first member, return nullptr if empty 
    const JsonMember* first() const { return m_slist.m_firstNode; }
    JsonMember* first()             { return m_slist.m_firstNode; }

    // find the desired member, return nullptr if failed
    // WARNING: if duplicate name exists, only return the first one.
    // WARNING: use sequential search, for traversal using iterator instead.
    JsonMember* find(const char* name);
    JsonMember* find(const std::string& name);
    const JsonMember* find(const char* name) const;
    const JsonMember* find(const std::string& name) const;

    // add null member to the obj, return reference to the new member
    JsonMember& add(const char* name, std::nullptr_t);
    JsonMember& add(const std::string& name, std::nullptr_t);

    // add a generic value to the object, return reference to the new member
    template<typename Ty>
    JsonMember& add(const char* name, const Ty& val)
    {
        JsonMember& member = this->add(name, nullptr);
        member.set(val);
        return member;
    }
    template<typename Ty>
    JsonMember& add(const std::string& name, const Ty& val)
    {
        JsonMember& member = this->add(name, nullptr);
        member.set(val);
        return member;
    }

    // add a named JSON value to the object, return reference to the new member
    JsonMember& add(const char* name, const JsonValue& val)
    {
        JsonMember& member = this->add(name, nullptr);
        (JsonValue&)member = val;
        return member;
    }
    JsonMember& add(const std::string& name, const JsonValue& val)
    {
        JsonMember& member = this->add(name, nullptr);
        (JsonValue&)member = val;
        return member;
    }

    // add an empty JSON Object, return reference to the new Object
    JsonObject& add_object(const char* name)
    {
        JsonMember& member = this->add(name, nullptr);
        return member.make_empty_object();
    }
    JsonObject& add_object(const std::string& name)
    {
        JsonMember& member = this->add(name, nullptr);
        return member.make_empty_object();
    }

    // add an empty JSON Array, return reference to the new Array
    JsonArray& add_array(const char* name)
    {
        JsonMember& member = this->add(name, nullptr);
        return member.make_empty_array();
    }
    JsonArray& add_array(const std::string& name)
    {
        JsonMember& member = this->add(name, nullptr);
        return member.make_empty_array();
    }

    // if member exist, return the member, else add new member
    JsonMember& operator[](const char* name)
    {
        JsonMember* pmemb = this->find(name);
        if (pmemb)
            return *pmemb;
        return this->add(name, nullptr);
    }
    JsonMember& operator[](const std::string& name)
    {
        JsonMember* pmemb = this->find(name);
        if (pmemb)
            return *pmemb;
        return this->add(name, nullptr);
    }

    // for user familiar with std::map
    template<typename Cy, typename Vy>
    decltype(auto) add(const std::pair<Cy, Vy>& item)
    {
        return this->add(item.first, item.second);
    }
    template<typename Cy, typename Vy>
    void operator+=(const std::pair<Cy, Vy>& item)
    {
        this->add(item.first, item.second);
    }

    // add sequence of scalar members: (name, value, name, value, ...)
    template<typename Cy, typename Vy, typename ...Ps>
    void adds(const Cy& name, const Vy& val, Ps&& ...pairs)
    {
        static_assert((sizeof...(pairs) & 1) == 0, "must be name:vlaue pair");
        this->add(name, val);
        this->adds(std::forward<Ps>(pairs)...);
    }

    // erase member from the array, return false if failed
    // WARNING: if duplicate name exists, only remove the first one
    // WARNING: O(N) sequential search
    bool erase(const char* name);
    bool erase(JsonMember& memb);

    // delete all members
    void clear();

    // return allocator
    JsonAllocator* allocator() const { return m_alloc; }

private:
    void adds() {}
    JsonAllocator*              m_alloc;    // memory allocator
    int                         m_count;    // number of members
    detail::SList<JsonMember>   m_slist;    // member list
};

//======================================================================================================================
// JSON Document

class CFile;

class JsonDoc : IrkNocopy
{
public:
    static const int kMaxDepth = 80;    // max depth to prevent stack overflow
    static const int kMaxIndent = 40;   // max indent when print pretty

    explicit JsonDoc() : m_alloc(), m_root(&m_alloc) {}
    explicit JsonDoc(size_t size) : m_alloc(size), m_root(&m_alloc) {}
    ~JsonDoc() { m_root.clear(); }

    // parse and load from Json file, 
    // return -1 if failed
    int load_file(const char* filepath);

    // parse and load from Json text, text must be null terminated
    // return -1 if failed
    int load_text(const char* text);

    // is JSON document valid/parsed
    explicit operator bool() const      { return !m_root.is_empty(); }

    // return the JSON root Object/Array
    const JsonValue& root() const       { return m_root; }
    JsonValue& root()                   { return m_root; }

    // create empty root Object, old root will be destroyed(if exists)
    JsonObject& create_root_object()    { return m_root.make_empty_object(); }

    // create empty root Array, old root will be destroyed(if exists)
    JsonArray& create_root_array()      { return m_root.make_empty_array(); }

    // dump Json document as UTF-8 file
    // return -1 if failed
    int dump_file(const char* filename);

    // dump Json document as UTF-8 string
    // return -1 if failed
    int dump_text(std::string& out);

    // ditto, slower but with pretty indentation
    int pretty_dump_file(const char* filename);
    int pretty_dump_text(std::string& out);

#ifdef _WIN32
    int load_file(const wchar_t* filepath);
    int dump_file(const wchar_t* filename);
    int pretty_dump_file(const wchar_t* filename);
#endif

private:
    int load_file(CFile&);
    int write_file(CFile&, const std::string&);
    JsonMempoolAllocator    m_alloc;    // memory allocator, must be declared first
    JsonValue               m_root;     // the root JSON Value
};

//======================================================================================================================
// convenient free functions

inline JsonValue json_parse(const char* text)
{
    JsonValue val;
    val.load(text);
    return std::move(val);
}

inline JsonValue json_parse(const char* text, JsonAllocator* allocator)
{
    JsonValue val(allocator);
    val.load(text);
    return std::move(val);
}

template<class Ty>
inline std::string json_dump(const Ty& obj)
{
    std::string text;
    obj.dump(text);
    return std::move(text);
}

template<class Ty>
inline std::string json_pretty_dump(const Ty& obj)
{
    std::string text;
    obj.pretty_dump(text);
    return std::move(text);
}

} // namespace irk
#endif
