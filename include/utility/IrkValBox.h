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

#ifndef _IRONBRICK_VALBOX_H_
#define _IRONBRICK_VALBOX_H_

#include <stdexcept>
#include "detail/ImplValBox.hpp"

namespace irk {

class BoxBadAccess : public std::logic_error
{  
public:
    BoxBadAccess() : std::logic_error( "Bad Box Access" ) {}
};

// A box contains value of any type, optimized for scalar value
class ValBox
{
    detail::BoxData m_box;

    template<typename T>
    static constexpr bool is_nonscalar()    // general compound type
    {
        return (!std::is_scalar<std::decay_t<T>>::value && 
                !std::is_same<T,Rational>::value &&
                !std::is_reference<T>::value &&
                !std::is_base_of<ValBox,T>::value);
    }
    template<typename T>
    static constexpr uint32_t tidof()
    {
        using NoCV_t = std::remove_cv_t<T>;
        using NoRef_t = std::remove_reference_t<NoCV_t>;
        return detail::BoxTid<NoRef_t>::tid;
    }
    template<typename T, bool>
    struct EmplaceHelper
    {
        template<typename... Args>
        static void emplace( ValBox& dst, Args&&... args )
        {
            dst = T{std::forward<Args>(args)...};
        }
    };
    template<typename T>
    struct EmplaceHelper<T,true>
    {
        template<typename... Args>
        static void emplace( ValBox& dst, Args&&... args )
        {
            dst.m_box.reset();
            dst.m_box.pobj = detail::make_boxed_obj<T>(std::forward<Args>(args)...);
            dst.m_box.tid = tidof<T>();
        }
    };
public:
    ValBox() = default;
    ~ValBox() = default;
    ValBox( const ValBox& other )
    {
        m_box.copy_from( other.m_box );
    }
    ValBox& operator=( const ValBox& other )
    {
        if( this != &other )
        {
            m_box.reset();
            m_box.copy_from( other.m_box );
        }
        return *this;
    }
    ValBox( ValBox&& other ) noexcept
    {
        m_box = other.m_box;
        other.m_box.tid = detail::TYPEID_INVALID;
    }
    ValBox& operator=( ValBox&& other ) noexcept
    {
        if( this != &other )
        {
            m_box.reset();
            m_box = other.m_box;
            other.m_box.tid = detail::TYPEID_INVALID;
        }
        return *this;
    }

    explicit ValBox( bool val )
    {
        m_box.flag = val;
        m_box.tid = detail::TYPEID_BOOL;
    }
    explicit ValBox( int32_t val )
    {
        m_box.i32 = val;
        m_box.tid = detail::TYPEID_INT32;
    }
    explicit ValBox( uint32_t val )
    {
        m_box.u32 = val;
        m_box.tid = detail::TYPEID_UINT32;
    }
    explicit ValBox( int64_t val )
    {
        m_box.i64 = val;
        m_box.tid = detail::TYPEID_INT64;
    }
    explicit ValBox( uint64_t val )
    {
        m_box.u64 = val;
        m_box.tid = detail::TYPEID_UINT64;
    }
    explicit ValBox( float val )
    {
        m_box.f32 = val;
        m_box.tid = detail::TYPEID_FLOAT;
    }
    explicit ValBox( double val )
    {
        m_box.f64 = val;
        m_box.tid = detail::TYPEID_DOUBLE;
    }
    explicit ValBox( Rational val )
    {
        m_box.rational = val;
        m_box.tid = detail::TYPEID_RATIONAL;
    }
    template<typename T>
    explicit ValBox( const T* ptr )
    {
        static_assert( !std::is_base_of<ValBox,T>::value, "" );
        m_box.ptr = (void*)ptr;
        m_box.tid = detail::TYPEID_PTR;
    }
    template<typename T>
    explicit ValBox( const T& val, std::enable_if_t<is_nonscalar<T>(),int> = 0 )
    {
        static_assert( std::is_copy_constructible<T>::value, "require copy constructible" );
        m_box.pobj = detail::make_boxed_obj<T>(val);
        m_box.tid = tidof<T>();
    }
    template<typename T>
    explicit ValBox( T&& val, std::enable_if_t<is_nonscalar<T>(),int> = 0 )
    {
        static_assert( std::is_move_constructible<T>::value, "require move constructible" );
        m_box.pobj = detail::make_boxed_obj<T>(std::move(val));
        m_box.tid = tidof<T>();
    }

    // assignment
    ValBox& operator=(bool val)
    {
        m_box.reset();
        m_box.flag = val;
        m_box.tid = detail::TYPEID_BOOL;
        return *this;
    }
    ValBox& operator=( int32_t val )
    {
        m_box.reset();
        m_box.i32 = val;
        m_box.tid = detail::TYPEID_INT32;
        return *this;
    }
    ValBox& operator=( uint32_t val )
    {
        m_box.reset();
        m_box.u32 = val;
        m_box.tid = detail::TYPEID_UINT32;
        return *this;
    }
    ValBox& operator=( int64_t val )
    {
        m_box.reset();
        m_box.i64 = val;
        m_box.tid = detail::TYPEID_INT64;
        return *this;
    }
    ValBox& operator=( uint64_t val )
    {
        m_box.reset();
        m_box.u64 = val;
        m_box.tid = detail::TYPEID_UINT64;
        return *this;
    }
    ValBox& operator=( float val )
    {
        m_box.reset();
        m_box.f32 = val;
        m_box.tid = detail::TYPEID_FLOAT;
        return *this;
    }
    ValBox& operator=( double val )
    {
        m_box.reset();
        m_box.f64 = val;
        m_box.tid = detail::TYPEID_DOUBLE;
        return *this;
    }
    ValBox& operator=( Rational val )
    {
        m_box.reset();
        m_box.rational = val;
        m_box.tid = detail::TYPEID_RATIONAL;
        return *this;
    }
    template<typename T>
    ValBox& operator=( const T* ptr )
    {
        static_assert( !std::is_base_of<ValBox,T>::value, "" );
        m_box.reset();
        m_box.ptr = (void*)ptr;
        m_box.tid = detail::TYPEID_PTR;
        return *this;
    }
    template<typename T>
    auto operator=( const T& val ) -> std::enable_if_t<is_nonscalar<T>(),ValBox&>
    {
        static_assert( std::is_copy_constructible<T>::value, "require copy constructible" );
        m_box.reset();
        m_box.pobj = detail::make_boxed_obj<T>(val);
        m_box.tid = tidof<T>();
        return *this;
    }
    template<typename T>
    auto operator=( T&& val ) -> std::enable_if_t<is_nonscalar<T>(),ValBox&>
    {
        static_assert( std::is_move_constructible<T>::value, "require move constructible" );
        m_box.reset();
        m_box.pobj = detail::make_boxed_obj<T>(std::move(val));
        m_box.tid = tidof<T>();
        return *this;
    }

    template<typename T, typename... Args>
    void emplace( Args&&... args )
    {
        static_assert( !std::is_reference<T>::value, "" );
        EmplaceHelper<T, (tidof<T>() > detail::TYPEID_OBJ)>::emplace(*this, std::forward<Args>(args)...);
    }

    // destroy value in the box
    void reset()            { m_box.reset(); }

    // is value engaged ?
    bool has_value() const  { return m_box.tid != detail::TYPEID_INVALID; }
    bool empty() const      { return m_box.tid == detail::TYPEID_INVALID; }

    // can get value of the specified type
    template<typename T>
    bool can_get() const    { return detail::BoxUnwrap<T>::matched( m_box ); }

    // try to get value of the specified type, return false if failed
    template<typename T>
    bool try_get( T* pval ) const &
    {
        if( detail::BoxUnwrap<T>::matched( m_box ) )
        {
            *pval = detail::BoxUnwrap<T>::get( m_box );
            return true;
        }
        return false;
    }
    template<typename T>
    bool try_get( T* pval ) &&
    {
        if( detail::BoxUnwrap<T>::matched( m_box ) )
        {
            *pval = std::move( detail::BoxUnwrap<T>::get(m_box) );
            return true;
        }
        return false;
    }

    //get value of the specified type, throw BoxBadAccess if failed
    template<typename T>
    T get() const &
    {
        if( !detail::BoxUnwrap<T>::matched( m_box ) )
            throw BoxBadAccess{};
        return detail::BoxUnwrap<T>::get( m_box );
    }
    template<typename T>
    T get() &&
    {
        if( !detail::BoxUnwrap<T>::matched( m_box ) )
            throw BoxBadAccess{};
        return std::move( detail::BoxUnwrap<T>::get(m_box) );
    }

    void swap( ValBox& other ) noexcept
    {
        auto tmp = this->m_box;
        this->m_box = other.m_box;
        other.m_box = tmp;
    }
};

template<typename T, typename... Args>
inline ValBox make_valuebox( Args&&... args )
{
    ValBox box;
    box.emplace<T>( std::forward<Args>(args)... );
    return box;
}

}   // namespace irk
#endif
