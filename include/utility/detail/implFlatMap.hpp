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

#ifndef _IRK_PRIVATE_IMPLFLATMAP_H_
#define _IRK_PRIVATE_IMPLFLATMAP_H_

#include "../IrkMemUtility.h"
#include "../IrkStringUtility.h"

namespace irk { namespace detail {

// simple sorted vector for trivial type
template<typename T>
class SortedVec : IrkNocopy
{
    static_assert( std::is_trivially_copyable<T>::value, "only support trivially copyable type" );
    T*          m_buff;
    unsigned    m_capacity;
    unsigned    m_size;

    void set_buffer( T* pbuf, unsigned capacity )
    {
        delete[] m_buff;
        m_buff = pbuf;
        m_capacity = capacity;
    }
public:
    explicit SortedVec( unsigned capacity ) : m_buff(nullptr), m_capacity(0), m_size(0)
    {
        if( capacity > 0 )
        {
            m_buff = new T[capacity];
            m_capacity = capacity;
        }
    }
    ~SortedVec()
    {
        delete[] m_buff;
    }
    SortedVec( SortedVec&& other ) noexcept // move constructor
    {
        m_buff = other.m_buff;
        m_capacity = other.m_capacity;
        m_size = other.m_size;
        other.m_buff = nullptr;
        other.m_capacity = 0;
        other.m_size = 0;
    }
    SortedVec& operator=( SortedVec&& other ) noexcept  // move assignment
    {
        if( this != &other )
        {
            delete[] m_buff;
            m_buff = other.m_buff;
            m_capacity = other.m_capacity;
            m_size = other.m_size;
            other.m_buff = nullptr;
            other.m_capacity = 0;
            other.m_size = 0;
        }
        return *this;
    }
    void reset( unsigned capacity )
    {
        if( capacity > 0 )
        {
            T* pbuf = new T[capacity];
            this->set_buffer( pbuf, capacity );
        }
        else    // free inner buffer
        {
            this->set_buffer( nullptr, 0 );
        }
        m_size = 0;
    }
    void reserve( unsigned capacity )
    {
        if( capacity > m_capacity )
        {
            T* pbuf = new T[capacity];
            memcpy( pbuf, m_buff, m_size * sizeof(T) );
            this->set_buffer( pbuf, capacity );
        }
    }
    void insert( unsigned idx, const T& data )
    {
        assert( idx <= m_size );
        if( m_size + 1 > m_capacity )
        {
            unsigned capacity = MAX((m_size + 1) * 2, 8);
            T* pbuf = new T[capacity];
            memcpy( pbuf, m_buff, sizeof(T) * idx );
            pbuf[idx] = data;
            memcpy( pbuf + idx + 1, m_buff + idx, sizeof(T) * (m_size - idx) );
            this->set_buffer( pbuf, capacity );
        }
        else
        {
            memmove( m_buff + idx + 1, m_buff + idx, sizeof(T) * (m_size - idx) );
            m_buff[idx] = data;
        }
        m_size++;
    }
    void erase( unsigned idx )
    {
        assert( idx < m_size );
        unsigned cnt = m_size - idx - 1;
        memmove( m_buff + idx, m_buff + idx + 1, sizeof(T) * cnt );
        m_size--;
    }
    void clear()
    { 
        m_size = 0; 
    }

    // binary search, return index if found( >= 0)
    template<class Pred>
    int bsearch( Pred pred ) const;

    T&          operator[]( unsigned idx )          { assert( idx < m_size ); return m_buff[idx]; }
    const T&    operator[]( unsigned idx ) const    { assert( idx < m_size ); return m_buff[idx]; }
    unsigned    size() const                        { return m_size; }
    unsigned    capacity() const                    { return m_capacity; }
};

// binary search, return index if found( >= 0)
template<typename T>
template<class Pred>
int SortedVec<T>::bsearch( Pred pred ) const
{
    int idxL = 0;
    int idxR = static_cast<int>(m_size) - 1;
    while( idxL <= idxR )
    {
        int idx = (idxL + idxR) >> 1;
        int res = pred( m_buff[idx] );
        if( res == 0 )
            return idx;
        if( res > 0 )
            idxL = idx + 1;
        else
            idxR = idx - 1;
    }
    return -1 - idxL;   // idxL == where should new key inserted
}

//======================================================================================================================

// map key compare functor
template<typename ValTy>
struct DftCmper
{
    int operator()( const ValTy& x, const ValTy& y ) const
    {
        return static_cast<int>(x > y) - static_cast<int>(x < y);
    }
};
template<>
struct DftCmper<char*>
{
    int operator()( const char* str1, const char* str2 ) const
    {
        return ::strcmp( str1, str2 );
    }
};
template<>
struct DftCmper<const char*>
{
    int operator()( const char* str1, const char* str2 ) const
    {
        return ::strcmp( str1, str2 );
    }
};
template<>
struct DftCmper<wchar_t*>
{
    int operator()( const wchar_t* str1, const wchar_t* str2 ) const
    {
        return ::wcscmp( str1, str2 );
    }
};
template<>
struct DftCmper<const wchar_t*>
{
    int operator()( const wchar_t* str1, const wchar_t* str2 ) const
    {
        return ::wcscmp( str1, str2 );
    }
};
template<>
struct DftCmper<char16_t*>
{
    int operator()( const char16_t* str1, const char16_t* str2 ) const
    {
        return ::irk::str_compare( str1, str2 );
    }
};
template<>
struct DftCmper<const char16_t*>
{
    int operator()( const char16_t* str1, const char16_t* str2 ) const
    {
        return ::irk::str_compare( str1, str2 );
    }
};

//======================================================================================================================
template<typename KeyTy, typename ValTy, class KeyCmp, bool BigItem>
class MapImpl
{
    struct Item 
    { 
        KeyTy   m_key; 
        ValTy   m_val; 
    };
    KeyCmp          m_Cmp;
    SortedVec<Item> m_Items;
public:
    MapImpl( unsigned initSize, const KeyCmp& cmp ) : m_Cmp(cmp), m_Items(initSize) {}
    MapImpl( MapImpl&& other ) noexcept : m_Cmp(other.m_Cmp), m_Items(std::move(other.m_Items)) {}
    MapImpl& operator=( MapImpl&& other ) noexcept
    {
        if( this != &other )
        {
            m_Cmp = other.m_Cmp;
            m_Items = std::move( other.m_Items );
        }
        return *this;
    }

    void reset( unsigned capacity )
    {
        m_Items.reset( capacity );
    }
    void reserve( unsigned capacity )
    {
        m_Items.reserve( capacity );
    }
    bool get( const KeyTy& key, ValTy* pVal ) const
    {
        int idx = m_Items.bsearch( [&](const Item& item){ return m_Cmp(key, item.m_key); } );
        if( idx >= 0 )
        {
            *pVal = m_Items[idx].m_val;
            return true;
        }
        return false;
    }
    int indexof( const KeyTy& key ) const   // get index in the sorted array, return < 0 if not found
    {
        int idx = m_Items.bsearch( [&](const Item& item){ return m_Cmp(key, item.m_key); } );
        return idx;
    }
    void insert_or_assign( const KeyTy& key, const ValTy& value )
    {
        int idx = m_Items.bsearch( [&](const Item& item){ return m_Cmp(key, item.m_key); } );
        if( idx >= 0 )          // if already exists, overwrite
        {
            m_Items[idx].m_val = value;
        }
        else
        {
            int k = -1 - idx;   // where should new key inserted
            m_Items.insert( k, Item{key, value} );
        }
    }
    void erase( unsigned idx )
    {
        m_Items.erase( idx );
    }
    bool remove( const KeyTy& key )         // remove item
    {
        int idx = m_Items.bsearch( [&](const Item& item){ return m_Cmp(key, item.m_key); } );
        if( idx >= 0 )
        {
            m_Items.erase( idx );
            return true;
        }
        return false;
    }
    const KeyTy& key( unsigned idx ) const   // get map key according the sorted index
    {
        return m_Items[idx].m_key;
    }
    ValTy& operator[]( unsigned idx )               // sequential access
    {
        return m_Items[idx].m_val;
    }
    const ValTy& operator[]( unsigned idx ) const
    {
        return m_Items[idx].m_val;
    }
    unsigned size() const 
    { 
        return m_Items.size(); 
    }
    void clear() 
    { 
        m_Items.clear(); 
    }
};

// for big object, store pointer in sorted array
template<typename KeyTy, typename ValTy, class KeyCmp>
class MapImpl<KeyTy,ValTy,KeyCmp,true>
{
    struct Item 
    { 
        KeyTy   m_key; 
        ValTy   m_val; 
    };
    KeyCmp           m_Cmp;
    SortedVec<Item*> m_ItemPtrs;    // sorted pointer array
public:
    MapImpl( unsigned initSize, const KeyCmp& cmp ) : m_Cmp(cmp), m_ItemPtrs(initSize) {}
    ~MapImpl() { this->clear(); }
    MapImpl( MapImpl&& other ) noexcept : m_Cmp(other.m_Cmp), m_ItemPtrs(std::move(other.m_ItemPtrs)) {}
    MapImpl& operator=( MapImpl&& other ) noexcept
    {
        if( this != &other )
        {
            this->clear();
            m_Cmp = other.m_Cmp;
            m_ItemPtrs = std::move( other.m_ItemPtrs );
        }
        return *this;
    }

    void reset( unsigned capacity )
    {
        this->clear();
        m_ItemPtrs.reset( capacity );
    }
    void reserve( unsigned capacity )
    {
        m_ItemPtrs.reserve( capacity );
    }
    bool get( const KeyTy& key, ValTy* pVal ) const
    {
        int idx = m_ItemPtrs.bsearch( [&](const Item* pm){ return m_Cmp(key, pm->m_key); } );
        if( idx >= 0 )
        {
            *pVal = m_ItemPtrs[idx]->m_val;
            return true;
        }
        return false;
    }
    int indexof( const KeyTy& key ) const   // get index in the sorted array, return < 0 if not found
    {
        int idx = m_ItemPtrs.bsearch( [&](const Item* pm){ return m_Cmp(key, pm->m_key); } );
        return idx;
    }
    void insert_or_assign( const KeyTy& key, const ValTy& value )
    {
        int idx = m_ItemPtrs.bsearch( [&](const Item* pm){ return m_Cmp(key, pm->m_key); } );
        if( idx >= 0 )    // if already exists, overwrite
        {     
            m_ItemPtrs[idx]->m_val = value;
        }
        else
        {
            Item* pitem = new Item{key, value};
            int k = -1 - idx;
            m_ItemPtrs.insert( k, pitem );
        }
    }
    void erase( unsigned idx )
    {
        m_ItemPtrs.erase( idx );
    }
    bool remove( const KeyTy& key )
    {
        int idx = m_ItemPtrs.bsearch( [&](const Item* pm){ return m_Cmp(key, pm->m_key); } );
        if( idx >= 0 )
        {
            delete m_ItemPtrs[idx];
            m_ItemPtrs.erase( idx );
            return true;
        }
        return false;
    }
    const KeyTy& key( unsigned idx ) const  // get map key according the sorted index
    {
        return m_ItemPtrs[idx]->m_key;
    }
    ValTy& operator[]( unsigned idx )       // sequential access
    {
        return m_ItemPtrs[idx]->m_val;
    }
    const ValTy& operator[]( unsigned idx ) const
    {
        return m_ItemPtrs[idx]->m_val;
    }
    unsigned size() const    
    { 
        return m_ItemPtrs.size(); 
    }
    void clear()        
    { 
        for( unsigned i = 0; i < m_ItemPtrs.size(); i++ )
            delete m_ItemPtrs[i];
        m_ItemPtrs.clear();
    }
};

//======================================================================================================================
template<typename ValTy, class Cmper, bool BigItem>
class SetImpl
{
    Cmper               m_Cmp;
    SortedVec<ValTy>    m_Items; 
public:
    SetImpl( unsigned initSize, const Cmper& cmp ) : m_Cmp(cmp), m_Items(initSize) {}
    SetImpl( SetImpl&& other ) noexcept : m_Cmp(other.m_Cmp), m_Items(std::move(other.m_Items)) {}
    SetImpl& operator=( SetImpl&& other ) noexcept
    {
        if( this != &other )
        {
            m_Cmp = other.m_Cmp;
            m_Items = std::move( other.m_Items );
        }
        return *this;
    }

    void reset( unsigned capacity )
    {
        m_Items.reset( capacity );
    }
    void reserve( unsigned capacity )
    {
        m_Items.reserve( capacity );
    }
    int indexof( const ValTy& val ) const   // get index in the sorted array, return < 0 if not found
    {
        return m_Items.bsearch( [&](const ValTy& item){ return m_Cmp(val,item); } );
    }
    void insert( const ValTy& val )
    {
        int idx = m_Items.bsearch( [&](const ValTy& item){ return m_Cmp(val,item); } );
        if( idx < 0 )
            m_Items.insert( -1 - idx, val );
    }
    void erase( unsigned idx )
    {
        m_Items.erase( idx );
    }
    bool remove( const ValTy& val )
    {
        int idx = m_Items.bsearch( [&](const ValTy& item){ return m_Cmp(val,item); } );
        if( idx >= 0 )
        {
            m_Items.erase( idx );
            return true;
        }
        return false;
    }
    const ValTy& operator[]( unsigned idx ) const
    {
        return m_Items[idx];
    }
    unsigned size() const
    {
        return m_Items.size();
    }
    void clear()
    {
        m_Items.clear();
    }
};

// for big object, store pointer in sorted array
template<typename ValTy, class Cmper>
class SetImpl<ValTy,Cmper,true>
{
    Cmper               m_Cmp;
    SortedVec<ValTy*>   m_ItemPtrs; // sorted pointer array
public:
    SetImpl( unsigned initSize, const Cmper& cmp ) : m_Cmp(cmp), m_ItemPtrs(initSize) {}
    ~SetImpl() { this->clear(); }
    SetImpl( SetImpl&& other ) noexcept : m_Cmp(other.m_Cmp), m_ItemPtrs(std::move(other.m_ItemPtrs)) {}
    SetImpl& operator=( SetImpl&& other ) noexcept
    {
        if( this != &other )
        {
            this->clear();
            m_Cmp = other.m_Cmp;
            m_ItemPtrs = std::move( other.m_ItemPtrs );
        }
        return *this;
    }

    void reset( unsigned capacity )
    {
        this->clear();
        m_ItemPtrs.reset( capacity );
    }
    void reserve( unsigned capacity )
    {
        m_ItemPtrs.reserve( capacity );
    }
    int indexof( const ValTy& val ) const   // get index in the sorted array, return < 0 if not found
    {
        return m_ItemPtrs.bsearch( [&](const ValTy* pv){ return m_Cmp(val,*pv); } );
    }
    void insert( const ValTy& val )
    {
        int idx = m_ItemPtrs.bsearch( [&](const ValTy* pv){ return m_Cmp(val,*pv); } );
        if( idx < 0 )
        {
            ValTy* pval = new ValTy{val};
            m_ItemPtrs.insert( -1 - idx, pval );
        }
    }
    void erase( unsigned idx )
    {
        m_ItemPtrs.erase( idx );
    }
    bool remove( const ValTy& val )
    {
        int idx = m_ItemPtrs.bsearch( [&](const ValTy* pv){ return m_Cmp(val,*pv); } );
        if( idx >= 0 )
        {
            delete m_ItemPtrs[idx];
            m_ItemPtrs.erase( idx );
            return true;
        }
        return false;
    }
    const ValTy& operator[]( unsigned idx ) const
    {
        return *m_ItemPtrs[idx];
    }
    unsigned size() const
    {
        return m_ItemPtrs.size();
    }
    void clear()
    {
        for( unsigned i = 0; i < m_ItemPtrs.size(); i++ )
            delete m_ItemPtrs[i];
        m_ItemPtrs.clear();
    }
};

}}  // namespace irk::detail
#endif
