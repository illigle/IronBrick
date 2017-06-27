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

#ifndef _IRONBRICK_RESULT_H_
#define _IRONBRICK_RESULT_H_

#include <new>
#include <memory>
#include <utility>
#include <stdexcept>
#include <type_traits>
#include "IrkCommon.h"

namespace irk {

class ResultBadAccess : public std::logic_error
{  
public:
    ResultBadAccess() : std::logic_error( "Bad Result Access" ) {}
};

// A type that represents either success value or error code
// ValT: value type, ErrT: error type
template<typename ValT, typename ErrT>
class Result
{
    static_assert( !std::is_reference<ValT>::value && !std::is_reference<ErrT>::value, "reference disallowed" );
    static_assert( !std::is_same<ValT,ErrT>::value, "value type and error type should be different" );
    enum class Status : unsigned char
    {
        Empty,
        HasVal,
        GotErr,
    };
    union UnionDVE
    {
        UnionDVE() : dummy() {}
        ~UnionDVE() {};
        char dummy;
        ValT val;
        ErrT err;
    };
    Status   m_status;
    UnionDVE m_valerr;
public:
    typedef ValT value_type;
    typedef ErrT error_type;

    bool has_value() const  { return m_status == Status::HasVal; }
    bool has_error() const  { return m_status == Status::GotErr; }

    Result() : m_status(Status::Empty) {}   // default-constructed object has nothing
    ~Result() { this->reset(); }

    // value construction
    Result( const value_type& val ) : m_status(Status::Empty)
    {
        ::new( std::addressof(m_valerr.val) ) value_type( val );
        m_status = Status::HasVal;
    }
    Result( value_type&& val ) : m_status(Status::Empty)
    {
        ::new( std::addressof(m_valerr.val) ) value_type( std::move(val) );
        m_status = Status::HasVal;
    }
    // error construction
    Result( const error_type& err ) : m_status(Status::Empty)
    {
        ::new( std::addressof(m_valerr.err) ) error_type( err );
        m_status = Status::GotErr;
    }
    Result( error_type&& err ) : m_status(Status::Empty)
    {
        ::new( std::addressof(m_valerr.err) ) error_type( std::move(err) );
        m_status = Status::GotErr;
    }

    // copy, move, assignment
    Result( const Result& other ) : m_status(Status::Empty)
    {
        if( other.has_value() )
        {
            ::new( std::addressof(m_valerr.val) ) value_type( other.m_valerr.val );
            m_status = Status::HasVal;
        }
        else if( other.has_error() )
        {
            ::new( std::addressof(m_valerr.err) ) error_type( other.m_valerr.err );
            m_status = Status::GotErr;
        }
    }
    Result( Result&& other ) : m_status(Status::Empty)
    {
        if( other.has_value() )
        {
            ::new( std::addressof(m_valerr.val) ) value_type( std::move(other.m_valerr.val) );
            m_status = Status::HasVal;
        }
        else if( other.has_error() )
        {
            ::new( std::addressof(m_valerr.err) ) error_type( std::move(other.m_valerr.err) );
            m_status = Status::GotErr;
        }
        other.reset();
    }
    Result& operator=( const Result& other )
    {
        if( this != &other )
        {
            this->reset();

            if( other.has_value() )
            {
                ::new( std::addressof(m_valerr.val) ) value_type( other.m_valerr.val );
                m_status = Status::HasVal;
            }
            else if( other.has_error() )
            {
                ::new( std::addressof(m_valerr.err) ) error_type( other.m_valerr.err );
                m_status = Status::GotErr;
            }
        }
        return *this;
    }
    Result& operator=( Result&& other )
    {
        if( this != &other )
        {
            this->reset();

            if( other.has_value() )
            {
                ::new( std::addressof(m_valerr.val) ) value_type( std::move(other.m_valerr.val) );
                m_status = Status::HasVal;
            }
            else if( other.has_error() )
            {
                ::new( std::addressof(m_valerr.err) ) error_type( std::move(other.m_valerr.err) );
                m_status = Status::GotErr;
            }
            other.reset();
        }
        return *this;
    }

    // destroy engaged value or error
    void reset() noexcept
    {
        if( m_status == Status::HasVal )
            m_valerr.val.~ValT();
        else if( m_status == Status::GotErr )
            m_valerr.err.~ErrT();
        m_status = Status::Empty;
    }

    // try to get value, return nullptr if no value is engaged
    value_type* try_get_value()             { return has_value() ? &m_valerr.val : nullptr; }
    const value_type* try_get_value() const { return has_value() ? &m_valerr.val : nullptr; }
    // try to get error, return nullptr if no error is engaged
    error_type* try_get_error()             { return has_error() ? &m_valerr.err : nullptr; }
    const error_type* try_get_error() const { return has_error() ? &m_valerr.err : nullptr; }

    // return value reference, throw ResultBadAccess if no value is engaged
    value_type& value()
    {
        if( !has_value() )
            throw ResultBadAccess();
        return m_valerr.val;
    }
    const value_type& value() const
    {
        if( !has_value() )
            throw ResultBadAccess();
        return m_valerr.val;
    }
    // return error reference, throw ResultBadAccess if no error is engaged
    error_type& error()
    {
        if( !has_error() )
            throw ResultBadAccess();
        return m_valerr.err;
    }
    const error_type& error() const
    {
        if( !has_error() )
            throw ResultBadAccess();
        return m_valerr.err;
    }

    // set value
    template<typename V>
    void set_value( const V& val )
    {
        if( has_value() )
        {
            m_valerr.val = val;
        }
        else
        {
            this->reset();
            ::new( std::addressof(m_valerr.val) ) value_type( val );
            m_status = Status::HasVal;
        }
    }
    template<typename V>
    void set_value( V&& val )
    {
        if( has_value() )
        {
            m_valerr.val = std::move(val);
        }
        else
        {
            this->reset();
            ::new( std::addressof(m_valerr.val) ) value_type( std::move(val) );
            m_status = Status::HasVal;
        }
    }
    template<typename... Args>
    void emplace_value( Args&&... args )
    {
        this->reset();
        ::new( std::addressof(m_valerr.val) ) value_type( std::forward<Args>(args)... );
        m_status = Status::HasVal;
    }
    Result& operator=( const value_type& val )
    {
        this->set_value( val );
        return *this;
    }
    Result& operator=( value_type&& val )
    {
        this->set_value( std::move(val) );
        return *this;
    }

    // set error
    template<typename E>
    void set_error( const E& err )
    {
        if( has_error() )
        {
            m_valerr.err = err;
        }
        else
        {
            this->reset();
            ::new( std::addressof(m_valerr.err) ) error_type( err );
            m_status = Status::GotErr;
        }
    }
    template<typename E>
    void set_error( E&& err )
    {
        if( has_error() )
        {
            m_valerr.err = std::move(err);
        }
        else
        {
            this->reset();
            ::new( std::addressof(m_valerr.err) ) error_type( std::move(err) );
            m_status = Status::GotErr;
        }
    }
    template<typename... Args>
    void emplace_error( Args&&... args )
    {
        this->reset();
        ::new( std::addressof(m_valerr.err) ) error_type( std::forward<Args>(args)... );
        m_status = Status::GotErr;
    }
    Result& operator=( const error_type& err )
    {
        this->set_error( err );
        return *this;
    }
    Result& operator=( error_type&& err )
    {
        this->set_error( std::move(err) );
        return *this;
    }
};

// A type that represents either success value or nothing
template<typename ValT>
class Result<ValT,void>
{
    static_assert( !std::is_reference<ValT>::value, "reference disallowed" );
    union UnionDV
    {
        UnionDV() : dummy() {}
        ~UnionDV() {};
        char dummy;
        ValT val;
    };
    bool    m_hasVal;
    UnionDV m_valnil;
public:
    typedef ValT value_type;

    Result() : m_hasVal(false) {}   // default-constructed object has nothing
    ~Result() { this->reset(); }
    Result( const value_type& val ) : m_hasVal(false)
    {
        ::new( std::addressof(m_valnil.val) ) value_type( val );
        m_hasVal = true;
    }
    Result( value_type&& val ) : m_hasVal(false)
    {
        ::new( std::addressof(m_valnil.val) ) value_type( std::move(val) );
        m_hasVal = true;
    }
    Result( const Result& other ) : m_hasVal(false)
    {
        if( other.m_hasVal )
        {
            ::new( std::addressof(m_valnil.val) ) value_type( other.m_valnil.val );
            m_hasVal = true;
        }
    }
    Result( Result&& other ) : m_hasVal(false)
    {
        if( other.m_hasVal )
        {
            ::new( std::addressof(m_valnil.val) ) value_type( std::move(other.m_valnil.val) );
            m_hasVal = true;
            other.reset();
        }
    }
    Result& operator=( const Result& other )
    {
        if( this != &other )
        {
            this->reset();
            if( other.m_hasVal )
            {
                ::new( std::addressof(m_valnil.val) ) value_type( other.m_valnil.val );
                m_hasVal = true;
            }
        }
        return *this;
    }
    Result& operator=( Result&& other )
    {
        if( this != &other )
        {
            this->reset();
            if( other.m_hasVal )
            {
                ::new( std::addressof(m_valnil.val) ) value_type( std::move(other.m_valnil.val) );
                m_hasVal = true;
                other.reset();
            }
        }
        return *this;
    }

    // destroy engaged value
    void reset()
    {
        if( m_hasVal )
            m_valnil.val.~ValT();
        m_hasVal = false;
    }

    bool has_value() const              { return m_hasVal; }

    // try to get value, return nullptr if no value is engaged
    value_type* try_get_value()             { return m_hasVal ? &m_valnil.val : nullptr; }
    const value_type* try_get_value() const { return m_hasVal ? &m_valnil.val : nullptr; }

    // return value reference, throw ResultBadAccess if no value is engaged
    value_type& value()
    {
        if( !m_hasVal )
            throw ResultBadAccess();
        return m_valnil.val;
    }
    const value_type& value() const
    {
        if( !m_hasVal )
            throw ResultBadAccess();
        return m_valnil.val;
    }
    
    // set value
    template<typename V>
    void set_value( const V& val )
    {
        if( m_hasVal )
        {
            m_valnil.val = val;
        }
        else
        {
            ::new( std::addressof(m_valnil.val) ) value_type( val );
            m_hasVal = true;
        }
    }
    template<typename V>
    void set_value( V&& val )
    {
        if( m_hasVal )
        {
            m_valnil.val = std::move(val);
        }
        else
        {
            ::new( std::addressof(m_valnil.val) ) value_type( std::move(val) );
            m_hasVal = true;
        }
    }
    template<typename... Args>
    void emplace_value( Args&&... args )
    {
        this->reset();
        ::new( std::addressof(m_valnil.val) ) value_type( std::forward<Args>(args)... );
        m_hasVal = true;
    }
    Result& operator=( const value_type& val )
    {
        this->set_value( val );
        return *this;
    }
    Result& operator=( value_type&& val )
    {
        this->set_value( std::move(val) );
        return *this;
    }
};

// similar to c++17 std::optional
template<typename V>
using Optional = Result<V,void>;

//======================================================================================================================
template<typename ValT, typename ErrT, typename... Args>
inline Result<ValT,ErrT> make_success_result( Args&& ...args )
{
    Result<ValT,ErrT> res;
    res.emplace_value( std::forward<Args>(args)... );
    return res;
}
template<typename ValT, typename ErrT, typename... Args>
inline Result<ValT,ErrT> make_error_result( Args&& ...args )
{
    Result<ValT,ErrT> res;
    res.emplace_error( std::forward<Args>(args)... );
    return res;
}
template<typename ValT, typename... Args>
inline Optional<ValT> make_optional_result( Args&& ...args )
{
    Optional<ValT> res;
    res.emplace_value( std::forward<Args>(args)... );
    return res;
}

template<typename ValT, typename ErrT>
inline bool operator==( const Result<ValT,ErrT>& a, const Result<ValT,ErrT>& b )
{
    if( a.has_value() != b.has_value() || a.has_error() != b.has_error() )
        return false;
    if( a.has_value() )
        return a.value() == b.value();
    else if( a.has_error() )
        return a.error() == b.error();
    return true;
}
template<typename ValT, typename ErrT>
inline bool operator!=( const Result<ValT,ErrT>& a, const Result<ValT,ErrT>& b )
{
    return !(a == b);
}

template<typename ValT>
inline bool operator==( const Optional<ValT>& a, const Optional<ValT>& b )
{
    if( a.has_value() != b.has_value() )
        return false;
    if( a.has_value() )
        return a.value() == b.value();
    return true;
}
template<typename ValT, typename ErrT>
inline bool operator!=( const Optional<ValT>& a, const Optional<ValT>& b )
{
    return !(a == b);
}

}   // end namespace irk
#endif
