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

#ifndef _IRK_PRIVATE_IMPLBOX_H_
#define _IRK_PRIVATE_IMPLBOX_H_

#include <limits.h>
#include <utility>
#include <type_traits>
#include "../IrkRational.h"

namespace irk { namespace detail {

// faked type id
enum
{
    TYPEID_INVALID = 0,     // nothing
    TYPEID_BOOL,
    TYPEID_INT32,
    TYPEID_UINT32,
    TYPEID_INT64,
    TYPEID_UINT64,
    TYPEID_FLOAT,
    TYPEID_DOUBLE,
    TYPEID_RATIONAL,        // rational number
    TYPEID_PTR,             // pointer
    TYPEID_OBJ = 256,       // other types
};

template<typename T>
struct BoxTid
{
    // faked type id, not perfect but simple
    static_assert( !std::is_scalar<T>::value && !std::is_reference<T>::value, "" );
    static const uint32_t tid = TYPEID_OBJ + static_cast<uint32_t>(sizeof(T));
};
template<typename T>
struct BoxTid<T*>
{
    static const uint32_t tid = TYPEID_PTR;
};
template<>
struct BoxTid<bool>
{
    static const uint32_t tid = TYPEID_BOOL;
};
template<>
struct BoxTid<char>
{
    static const uint32_t tid = TYPEID_INT32;
};
template<>
struct BoxTid<int8_t>
{
    static const uint32_t tid = TYPEID_INT32;
};
template<>
struct BoxTid<uint8_t>
{
    static const uint32_t tid = TYPEID_INT32;
};
template<>
struct BoxTid<int16_t>
{
    static const uint32_t tid = TYPEID_INT32;
};
template<>
struct BoxTid<uint16_t>
{
    static const uint32_t tid = TYPEID_INT32;
};
template<>
struct BoxTid<int32_t>
{
    static const uint32_t tid = TYPEID_INT32;
};
template<>
struct BoxTid<uint32_t>
{
    static const uint32_t tid = TYPEID_UINT32;
};
template<>
struct BoxTid<int64_t>
{
    static const uint32_t tid = TYPEID_INT64;
};
template<>
struct BoxTid<uint64_t>
{
    static const uint32_t tid = TYPEID_UINT64;
};
template<>
struct BoxTid<float>
{
    static const uint32_t tid = TYPEID_FLOAT;
};
template<>
struct BoxTid<double>
{
    static const uint32_t tid = TYPEID_DOUBLE;
};
template<>
struct BoxTid<Rational>
{
    static const uint32_t tid = TYPEID_RATIONAL;
};

//======================================================================================================================
struct IBoxedObj
{
    IBoxedObj() = default;
    IBoxedObj( const IBoxedObj& ) = delete;
    IBoxedObj& operator=( const IBoxedObj& ) = delete;
    virtual ~IBoxedObj() {}
    virtual bool copyable() const = 0;
    virtual IBoxedObj* clone() const = 0;
};

template<typename T>
struct BoxedObj : public IBoxedObj
{
    template<typename... Args>
    explicit BoxedObj( Args&&... args ) : m_obj{std::forward<Args>(args)...} {};
    ~BoxedObj() override {}
    bool copyable() const override  { return std::is_copy_constructible<T>::value; }
    IBoxedObj* clone() const override;
    T m_obj;
};

template<typename T, bool Copyable>
struct BoxClone
{
    static IBoxedObj* clone( const T& ) 
    { 
        static_assert( !std::is_copy_constructible<T>::value, "" );
        assert( false );    // should never be called
        return nullptr;
    }
};
template<typename T>
struct BoxClone<T, true>
{
    static IBoxedObj* clone( const T& obj ) 
    { 
        static_assert( std::is_copy_constructible<T>::value, "" );
        return new BoxedObj<T>( obj );
    }
};

template<typename T>
inline IBoxedObj* BoxedObj<T>::clone() const
{
    return BoxClone<T,std::is_copy_constructible<T>::value>::clone( m_obj );
}

template<typename T, typename... Args>
inline BoxedObj<T>* make_boxed_obj( Args&&... args )
{
    return new BoxedObj<T>( std::forward<Args>(args)... );
}

struct BoxData
{
    union
    {
        bool        flag;
        int32_t     i32;
        uint32_t    u32;
        int64_t     i64;
        uint64_t    u64;
        float       f32;
        double      f64;
        void*       ptr;
        Rational    rational;
        IBoxedObj*  pobj;
    };
    uint32_t tid;   // faked type id

    void reset()
    {
        if( this->tid >= TYPEID_OBJ )   // no-scalar value
            delete this->pobj;
        this->tid = TYPEID_INVALID;
    }

    BoxData() : tid(TYPEID_INVALID) {}
    BoxData( const BoxData& other ) = default;
    BoxData& operator=( const BoxData& ) = default;
    ~BoxData() { this->reset(); }

    void copy_from( const BoxData& src )
    {
        assert( this->tid == TYPEID_INVALID );

        if( src.tid < detail::TYPEID_OBJ )      // scalar types
        {
            *this = src;
        }
        else
        {
            irk_expect( src.pobj->copyable() ); // must be copyable
            this->pobj = src.pobj->clone();
            this->tid = src.tid;
        }
    }
};

//======================================================================================================================
template<typename T>
struct BoxUnwrap
{
    static_assert( !std::is_scalar<T>::value && !std::is_reference<T>::value, "" );

    static inline bool matched( const BoxData& box )    // can get data of the type ?
    {
        return BoxTid<T>::tid == box.tid;
    }
    static inline T& get( const BoxData& box )  // unwrap box and get real data
    {
        assert( BoxTid<T>::tid == box.tid );
        auto realobj = static_cast<BoxedObj<T>*>( box.pobj );
        return realobj->m_obj;
    }
};

template<typename T>
struct BoxUnwrap<T*>
{
    static inline bool matched( const BoxData& box )
    {
        return BoxTid<T*>::tid == box.tid;
    }
    static inline T* get( const BoxData& box )
    {
        assert( BoxTid<T*>::tid == box.tid );
        return reinterpret_cast<T*>( box.ptr );
    }
};

template<>
struct BoxUnwrap<bool>
{
    static inline bool matched( const BoxData& box )
    {
        return BoxTid<bool>::tid == box.tid;
    }
    static inline bool get( const BoxData& box )
    {
        assert( BoxTid<bool>::tid == box.tid );
        return box.flag;
    }
};

template<>
struct BoxUnwrap<float>
{
    static inline bool matched( const BoxData& box )
    {
        return BoxTid<float>::tid == box.tid;
    }
    static inline float get( const BoxData& box )
    {
        assert( BoxTid<float>::tid == box.tid );
        return box.f32;
    }
};

template<>
struct BoxUnwrap<double>
{
    static inline bool matched( const BoxData& box )
    {
        return (TYPEID_DOUBLE == box.tid || TYPEID_FLOAT == box.tid);
    }
    static inline double get( const BoxData& box )
    {
        assert( TYPEID_DOUBLE == box.tid || TYPEID_FLOAT == box.tid );
        return (TYPEID_DOUBLE == box.tid) ? box.f64 : box.f32;
    }
};

template<>
struct BoxUnwrap<Rational>
{
    static inline bool matched( const BoxData& box )
    {
        return BoxTid<Rational>::tid == box.tid;
    }
    static inline Rational get( const BoxData& box )
    {
        assert( BoxTid<Rational>::tid == box.tid );
        return box.rational;
    }
};

template<>
struct BoxUnwrap<char>
{
    static inline bool matched( const BoxData& box )
    {
        switch( box.tid )
        {
        case TYPEID_INT32:
            return (box.i32 >= CHAR_MIN && box.i32 <= CHAR_MAX);
        case TYPEID_UINT32:
            return (box.u32 <= (uint32_t)CHAR_MAX);
        case TYPEID_INT64:
            return (box.i64 >= CHAR_MIN && box.i64 <= CHAR_MAX);
        case TYPEID_UINT64:
            return (box.u64 <= (uint64_t)CHAR_MAX);
        default:
            break;
        }
        return false;
    }
    static inline char get( const BoxData& box )
    {
        switch( box.tid )
        {
        case TYPEID_INT32:
            return static_cast<char>( box.i32 );
        case TYPEID_UINT32:
            return static_cast<char>( box.u32 );
        case TYPEID_INT64:
            return static_cast<char>( box.i64 );
        case TYPEID_UINT64:
            return static_cast<char>( box.u64 );
        default:
            assert(0);
            break;
        }
        return 0;
    }
};

template<>
struct BoxUnwrap<int8_t>
{
    static inline bool matched( const BoxData& box )
    {
        switch( box.tid )
        {
        case TYPEID_INT32:
            return (box.i32 >= INT8_MIN && box.i32 <= INT8_MAX);
        case TYPEID_UINT32:
            return (box.u32 <= (uint32_t)INT8_MAX);
        case TYPEID_INT64:
            return (box.i64 >= INT8_MIN && box.i64 <= INT8_MAX);
        case TYPEID_UINT64:
            return (box.u64 <= (uint64_t)INT8_MAX);
        default:
            break;
        }
        return false;
    }
    static inline int8_t get( const BoxData& box )
    {
        switch( box.tid )
        {
        case TYPEID_INT32:
            return static_cast<int8_t>( box.i32 );
        case TYPEID_UINT32:
            return static_cast<int8_t>( box.u32 );
        case TYPEID_INT64:
            return static_cast<int8_t>( box.i64 );
        case TYPEID_UINT64:
            return static_cast<int8_t>( box.u64 );
        default:
            assert(0);
            break;
        }
        return 0;
    }
};

template<>
struct BoxUnwrap<int16_t>
{
    static inline bool matched( const BoxData& box )
    {
        switch( box.tid )
        {
        case TYPEID_INT32:
            return (box.i32 >= INT16_MIN && box.i32 <= INT16_MAX);
        case TYPEID_UINT32:
            return (box.u32 <= (uint32_t)INT16_MAX);
        case TYPEID_INT64:
            return (box.i64 >= INT16_MIN && box.i64 <= INT16_MAX);
        case TYPEID_UINT64:
            return (box.u64 <= (uint64_t)INT16_MAX);
        default:
            break;
        }
        return false;
    }
    static inline int16_t get( const BoxData& box )
    {
        switch( box.tid )
        {
        case TYPEID_INT32:
            return static_cast<int16_t>( box.i32 );
        case TYPEID_UINT32:
            return static_cast<int16_t>( box.u32 );
        case TYPEID_INT64:
            return static_cast<int16_t>( box.i64 );
        case TYPEID_UINT64:
            return static_cast<int16_t>( box.u64 );
        default:
            assert(0);
            break;
        }
        return 0;
    }
};

template<>
struct BoxUnwrap<int32_t>
{
    static inline bool matched( const BoxData& box )
    {
        switch( box.tid )
        {
        case TYPEID_INT32:
            return true;
        case TYPEID_UINT32:
            return (box.u32 <= (uint32_t)INT32_MAX);
        case TYPEID_INT64:
            return (box.i64 >= INT32_MIN && box.i64 <= INT32_MAX);
        case TYPEID_UINT64:
            return (box.u64 <= (uint64_t)INT32_MAX);
        default:
            break;
        }
        return false;
    }
    static inline int32_t get( const BoxData& box )
    {
        switch( box.tid )
        {
        case TYPEID_INT32:
            return box.i32;
        case TYPEID_UINT32:
            return static_cast<int32_t>( box.u32 );
        case TYPEID_INT64:
            return static_cast<int32_t>( box.i64 );
        case TYPEID_UINT64:
            return static_cast<int32_t>( box.u64 );
        default:
            assert(0);
            break;
        }
        return 0;
    }
};

template<>
struct BoxUnwrap<int64_t>
{
    static inline bool matched( const BoxData& box )
    {
        switch( box.tid )
        {
        case TYPEID_INT32:
        case TYPEID_UINT32:
        case TYPEID_INT64:
            return true;
        case TYPEID_UINT64:
            return (box.u64 <= (uint64_t)INT64_MAX);
        default:
            break;
        }
        return false;
    }
    static inline int64_t get( const BoxData& box )
    {
        switch( box.tid )
        {
        case TYPEID_INT32:
            return box.i32;
        case TYPEID_UINT32:
            return box.u32;
        case TYPEID_INT64:
            return box.i64;
        case TYPEID_UINT64:
            return static_cast<int64_t>( box.u64 );
        default:
            assert(0);
            break;
        }
        return 0;
    }
};

template<>
struct BoxUnwrap<uint8_t>
{
    static inline bool matched( const BoxData& box )
    {
        switch( box.tid )
        {
        case TYPEID_INT32:
            return (box.i32 >= 0 && box.i32 <= UINT8_MAX);
        case TYPEID_UINT32:
            return (box.u32 <= UINT8_MAX);
        case TYPEID_INT64:
            return (box.i64 >= 0 && box.i64 <= UINT8_MAX);
        case TYPEID_UINT64:
            return (box.u64 <= UINT8_MAX);
        default:
            break;
        }
        return false;
    }
    static inline uint8_t get( const BoxData& box )
    {
        switch( box.tid )
        {
        case TYPEID_INT32:
            return static_cast<uint8_t>( box.i32 );
        case TYPEID_UINT32:
            return static_cast<uint8_t>( box.u32 );
        case TYPEID_INT64:
            return static_cast<uint8_t>( box.i64 );
        case TYPEID_UINT64:
            return static_cast<uint8_t>( box.u64 );
        default:
            assert(0);
            break;
        }
        return 0;
    }
};

template<>
struct BoxUnwrap<uint16_t>
{
    static inline bool matched( const BoxData& box )
    {
        switch( box.tid )
        {
        case TYPEID_INT32:
            return (box.i32 >= 0 && box.i32 <= UINT16_MAX);
        case TYPEID_UINT32:
            return (box.u32 <= UINT16_MAX);
        case TYPEID_INT64:
            return (box.i64 >= 0 && box.i64 <= UINT16_MAX);
        case TYPEID_UINT64:
            return (box.u64 <= UINT16_MAX);
        default:
            break;
        }
        return false;
    }
    static inline uint16_t get( const BoxData& box )
    {
        switch( box.tid )
        {
        case TYPEID_INT32:
            return static_cast<uint16_t>( box.i32 );
        case TYPEID_UINT32:
            return static_cast<uint16_t>( box.u32 );
        case TYPEID_INT64:
            return static_cast<uint16_t>( box.i64 );
        case TYPEID_UINT64:
            return static_cast<uint16_t>( box.u64 );
        default:
            assert(0);
            break;
        }
        return 0;
    }
};

template<>
struct BoxUnwrap<uint32_t>
{
    static inline bool matched( const BoxData& box )
    {
        switch( box.tid )
        {
        case TYPEID_INT32:
            return (box.i32 >= 0);
        case TYPEID_UINT32:
            return true;
        case TYPEID_INT64:
            return (box.i64 >= 0 && box.i64 <= UINT32_MAX);
        case TYPEID_UINT64:
            return (box.u64 <= UINT32_MAX);
        default:
            break;
        }
        return false;
    }
    static inline uint32_t get( const BoxData& box )
    {
        switch( box.tid )
        {
        case TYPEID_INT32:
            return static_cast<uint32_t>( box.i32 );
        case TYPEID_UINT32:
            return box.u32;
        case TYPEID_INT64:
            return static_cast<uint32_t>( box.i64 );
        case TYPEID_UINT64:
            return static_cast<uint32_t>( box.u64 );
        default:
            assert(0);
            break;
        }
        return 0;
    }
};

template<>
struct BoxUnwrap<uint64_t>
{
    static inline bool matched( const BoxData& box )
    {
        switch( box.tid )
        {
        case TYPEID_INT32:
            return (box.i32 >= 0);
        case TYPEID_INT64:
            return (box.i64 >= 0);
        case TYPEID_UINT32:
        case TYPEID_UINT64:
            return true;
        default:
            break;
        }
        return false;
    }
    static inline uint64_t get( const BoxData& box )
    {
        switch( box.tid )
        {
        case TYPEID_INT32:
            return static_cast<uint64_t>( box.i32 );
        case TYPEID_UINT32:
            return box.u32;
        case TYPEID_INT64:
            return static_cast<uint64_t>( box.i64 );
        case TYPEID_UINT64:
            return box.u64;
        default:
            assert(0);
            break;
        }
        return 0;
    }
};

// end of namespace irk::detail
}}

#endif
