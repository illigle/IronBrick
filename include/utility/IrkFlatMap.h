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

#ifndef _IRONBRICK_FLATMAP_H_
#define _IRONBRICK_FLATMAP_H_

#include "detail/implFlatMap.hpp"

namespace irk {

// like std::map, but implemented using sorted array, designed for a small number of items
template<typename KeyTy, typename ValTy, class KeyCmp=detail::DftCmper<KeyTy>>
class FlatMap : IrkNocopy
{
    struct Item { KeyTy m_Key; ValTy m_Value; };
    static_assert( std::is_trivially_copyable<Item>::value, "only support trivially copyable type" );
    static_assert( std::is_default_constructible<Item>::value, "only support default constructible type" );
    detail::MapImpl<KeyTy, ValTy, KeyCmp, (sizeof(Item)>128)> m_Impl;
public:
    typedef KeyTy   key_type;
    typedef ValTy   value_type;
    typedef KeyCmp  key_compare;

    // initSize: the initial map capacity
    explicit FlatMap( unsigned int initSize, const KeyCmp& cmp=KeyCmp() ) : m_Impl(initSize, cmp) {}
    FlatMap( FlatMap&& other ) noexcept : m_Impl(std::move(other.m_Impl)) {}
    FlatMap& operator=( FlatMap&& other ) noexcept
    {
        m_Impl = std::move( other.m_Impl );
        return *this;
    }

    // clear existing data and alloc new buffer, if capcity == 0, free inner buffer
    void reset( unsigned int capacity )   
    {
        m_Impl.reset( capacity );
    }
    // reserve memory, never shrink inner buffer
    void reserve( unsigned int capacity )        
    {
        m_Impl.reserve( capacity );
    }
    // get value, return false if not exists
    bool get( const KeyTy& key, ValTy* pValue ) const
    {    
        return m_Impl.get( key, pValue );
    }
    // is item exists in the map
    bool contains( const KeyTy& key ) const
    {
        return m_Impl.indexof( key ) >= 0;
    }
    // get item's index in the sorted array, return < 0 if not found
    // WARNING: index may be invalidated if map is modified
    int indexof( const KeyTy& key ) const
    {
        return m_Impl.indexof( key );
    }
    // insert new item, if already exists overwrite old value
    void insert_or_assign( const KeyTy& key, const ValTy& value )
    {
        return m_Impl.insert_or_assign( key, value );
    }
    // remove specified item
    bool remove( const KeyTy& key )
    {
        return m_Impl.remove( key );
    }
    // erase item according to the sorted index
    void erase( unsigned int idx )
    {
        m_Impl.erase( idx );
    }
    // get map key according to the sorted index
    const KeyTy& key( unsigned int idx ) const
    {
        return m_Impl.key( idx );
    }
    // sequential access
    ValTy& operator[]( unsigned int idx ) 
    {
        return m_Impl[idx];
    }
    const ValTy& operator[]( unsigned int idx ) const
    {
        return m_Impl[idx];
    }
    unsigned int size() const   
    { 
        return m_Impl.size(); 
    }
    bool empty() const
    {
        return m_Impl.size() == 0;
    }
    void clear()        
    { 
        m_Impl.clear();  
    }
};

// like std::set, but implemented using sorted array, designed for a small number of items
template<typename ValTy, class KeyCmp=detail::DftCmper<ValTy>>
class FlatSet: IrkNocopy
{
    static_assert( std::is_trivially_copyable<ValTy>::value, "only support trivially copyable type" );
    static_assert( std::is_default_constructible<ValTy>::value, "only support default constructible type" );
    detail::SetImpl<ValTy, KeyCmp, (sizeof(ValTy)>128)> m_Impl;
public:
    typedef ValTy   key_type;
    typedef ValTy   value_type;
    typedef KeyCmp  key_compare;

    // initSize: the initial set capacity
    explicit FlatSet( unsigned int initSize, const KeyCmp& cmp=KeyCmp() ) : m_Impl(initSize, cmp) {}
    FlatSet( FlatSet&& other ) noexcept : m_Impl(std::move(other.m_Impl)) {}
    FlatSet& operator=( FlatSet&& other ) noexcept
    {
        m_Impl = std::move( other.m_Impl );
        return *this;
    }

    // clear existing data and alloc new buffer, if capcity == 0, free inner buffer
    void reset( unsigned int capacity )   
    {
        m_Impl.reset( capacity );
    }
    // reserve memory, never shrink inner buffer
    void reserve( unsigned int capacity )        
    {
        m_Impl.reserve( capacity );
    }
    // is value exists in the set
    bool contains( const ValTy& value ) const
    {
        return m_Impl.indexof( value ) >= 0;
    }
    // get value's index in the sorted array, return < 0 if not found
    // WARNING: index may be invalidated if set is modified
    int indexof( const ValTy& value ) const
    {
        return m_Impl.indexof( value );
    }
    // insert new value
    void insert( const ValTy& value )
    {
        return m_Impl.insert( value );
    }
    // remove specified value
    bool remove( const ValTy& value )
    {
        return m_Impl.remove( value );
    }
    // erase value according to the sorted index
    void erase( unsigned int idx )
    {
        m_Impl.erase( idx );
    }
    // sequential access
    const ValTy& operator[]( unsigned int idx ) const
    {
        return m_Impl[idx];
    }
    unsigned int size() const   
    { 
        return m_Impl.size(); 
    }
    bool empty() const
    {
        return m_Impl.size() == 0;
    }
    void clear()        
    { 
        m_Impl.clear();  
    }
};

}   // namespace irk
#endif
