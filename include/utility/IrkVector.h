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

#ifndef _IRONBRICK_VECTOR_H_
#define _IRONBRICK_VECTOR_H_

#include "IrkMemUtility.h"

namespace irk {

// like std::vector, but can only used to store trivial type, has extended alignment support
template<typename T>
class Vector : IrkNocopy
{
public:
    typedef T value_type;
    static_assert( std::is_trivially_copyable<T>::value, "only support trivially copyable type" );
    static_assert( std::is_default_constructible<T>::value, "only support default constructible type" );

    explicit Vector( size_t capacity=0, size_t alignment=sizeof(void*) ) : m_buff(nullptr)
    {
        if( alignment < alignof(T) )    // make naturally aligned
            alignment = alignof(T);
        if( capacity > 0 )
            m_buff = (T*)aligned_malloc( capacity * sizeof(T), alignment );
        m_align = alignment;
        m_capacity = capacity;
        m_size = 0;
    }
    ~Vector()
    {
        aligned_free( m_buff );
    }
    Vector( Vector&& other ) noexcept   // move constructor
    {
        m_buff = other.m_buff;
        m_align = other.m_align;
        m_capacity = other.m_capacity;
        m_size = other.m_size;
        other.m_buff = nullptr;
        other.m_capacity = 0;
        other.m_size = 0;
    }
    Vector& operator=( Vector&& other ) noexcept    // move assignment
    {
        if( this != &other )
        {
            aligned_free( m_buff );
            m_buff = other.m_buff;
            m_align = other.m_align;
            m_capacity = other.m_capacity;
            m_size = other.m_size;
            other.m_buff = nullptr;
            other.m_capacity = 0;
            other.m_size = 0;
        }
        return *this;
    }

    // classic vector operations
    void reserve( size_t capacity );
    void shrink( size_t capacity );   
    void resize( size_t cnt );
    void assign( const T* pData, size_t cnt );
    void push_back( const T* pData, size_t cnt );
    void push_back( const T& data );
    void pop_back( size_t cnt );

    // extra SLOW operations
    void insert( size_t idx, const T* pData, size_t cnt );
    void insert( size_t idx, const T& data );
    void erase( size_t idx );
    void erase( size_t beg, size_t end );
    void pop_front( size_t cnt );
    void remove( const T& value );
    template<class Pred>
    void remove( Pred pred );

    void        clear()                         { m_size = 0; }
    T&          operator[]( size_t idx )        { assert( idx < m_size ); return m_buff[idx]; }
    const T&    operator[]( size_t idx ) const  { assert( idx < m_size ); return m_buff[idx]; }
    T&          back()                          { assert( m_size > 0 ); return m_buff[m_size-1]; }
    const T&    back() const                    { assert( m_size > 0 ); return m_buff[m_size-1]; }
    T*          data()                          { return m_buff; }
    const T*    data() const                    { return m_buff; }
    size_t      size() const                    { return m_size; }
    size_t      capacity() const                { return m_capacity; }
    bool        empty() const                   { return m_size == 0; }

    // clear existing data and alloc new buffer, if capcity==0, free inner buffer
    void reset( size_t capacity );

    // allocate free memory at the tail of inner buffer
    T* alloc( size_t bufsize );

    // commit data written to the buffer allocated by alloc()
    void commit( size_t cnt );

    // take away the inner buffer, new owner is responsible for buffer free
    T* take();

private:
    T* alloc_copy( size_t capacity )   // alloc new buffer and copy existing data
    {
        assert( capacity > 0 && capacity >= m_size );
        T* pbuf = (T*)aligned_malloc( capacity * sizeof(T), m_align );
        if( m_size > 0 )
            memcpy( pbuf, m_buff, m_size * sizeof(T) );
        return pbuf;
    }
    // free current buffer, using external buffer
    void set_buffer( T* pbuf, size_t capacity )
    {
        assert( ((uintptr_t)pbuf & (m_align-1)) == 0 );
        aligned_free( m_buff );
        m_buff = pbuf;
        m_capacity = capacity;
    }
    T*      m_buff;         // inner buffer
    size_t  m_align;        // buffer alignment
    size_t  m_capacity;     // buffer capacity
    size_t  m_size;         // current used size
};

// clear existing data and alloc new buffer, if capcity==0, free inner buffer
template<class T>
inline void Vector<T>::reset( size_t capacity )
{
    if( capacity > 0 ) // create new buffer
    {
        T* pbuf = (T*)aligned_malloc( capacity * sizeof(T), m_align );
        this->set_buffer( pbuf, capacity );
    }
    else // free buffer
    {
        this->set_buffer( nullptr, 0 );
    }
    m_size = 0;
}

// like std::vector::reserve, reserve memory, never shrink
template<class T>
inline void Vector<T>::reserve( size_t capacity )
{
    if( capacity > m_capacity )     // create new buffer
    {
        T* pbuf = this->alloc_copy( capacity );
        this->set_buffer( pbuf, capacity );
    }
}

// shrink inner buffer's size
template<class T>
inline void Vector<T>::shrink( size_t capacity )
{
    if( capacity < m_size )    // shrink will not discard data !
    {
        capacity = m_size;
    }
    if( capacity < m_capacity )
    {
        if( capacity > 0 )
        {
            T* pbuf = this->alloc_copy( capacity );
            this->set_buffer( pbuf, capacity );
        }
        else
        {
            this->set_buffer( nullptr, 0 );
        }
    }
}

// like std::vector::resize, but will not fill zeros !
template<class T>
inline void Vector<T>::resize( size_t cnt )
{
    if( cnt > m_capacity )  // buffer is not big enough
    {
        size_t capacity = MAX( cnt, m_capacity*3/2 );
        this->reserve( capacity );
    }
    if( !std::is_trivially_default_constructible<T>::value )
    {
        // defalut initialize new items if not trivial
        for( size_t i = m_size; i < cnt; i++ )
            ::new ((void*)(m_buff + i)) T{};
    }
    m_size = cnt;
}

// like std::vector::assign
template<class T>
inline void Vector<T>::assign( const T* pData, size_t cnt )
{
    if( cnt > m_capacity )      // buffer is not big enough
    {
        T* pbuf = (T*)aligned_malloc( cnt * sizeof(T), m_align );
        memcpy( pbuf, pData, cnt * sizeof(T) );
        this->set_buffer( pbuf, cnt );
    }
    else
    {
        memcpy( m_buff, pData, cnt * sizeof(T) );
    }
    m_size = cnt;
}

// like std::vector::push_back, append value to the back
template<class T>
inline void Vector<T>::push_back( const T* pData, size_t cnt )
{
    if( m_size + cnt > m_capacity ) // buffer is not big enough
    {
        size_t capacity = MAX( (m_size + cnt)*3/2, 8 );
        T* pbuf = this->alloc_copy( capacity );
        memcpy( pbuf + m_size, pData, cnt * sizeof(T) );
        this->set_buffer( pbuf, capacity );
    }
    else
    {
        memcpy( m_buff + m_size, pData, cnt * sizeof(T) );
    }
    m_size += cnt;
}

// like std::vector::push_back
template<class T>
inline void Vector<T>::push_back( const T& value )
{
    if( m_size + 1 > m_capacity )   // buffer is not big enough
    {
        size_t capacity = MAX( (m_size + 1)*3/2, 8 );
        T* pbuf = this->alloc_copy( capacity );
        pbuf[m_size] = value;
        this->set_buffer( pbuf, capacity );
    }
    else
    {
        m_buff[m_size] = value;
    }
    m_size++;
}

// discard [cnt] value from the back
template<class T>
inline void Vector<T>::pop_back( size_t cnt )
{
    assert( cnt <= m_size );
    m_size -= cnt;
}

// insert [cnt] value at position [idx]
template<class T>
inline void Vector<T>::insert( size_t idx, const T* pData, size_t cnt )
{
    assert( idx <= m_size );
    if( m_size + cnt > m_capacity )     // buffer is not big enough
    {
        size_t capacity = MAX( (m_size + cnt)*3/2, 8 );
        T* pbuf = (T*)aligned_malloc( capacity * sizeof(T), m_align );
        memcpy( pbuf, m_buff, idx * sizeof(T) );
        memcpy( pbuf + idx, pData, cnt * sizeof(T) );
        memcpy( pbuf + idx + cnt, m_buff + idx, (m_size - idx) * sizeof(T) );
        this->set_buffer( pbuf, capacity );
    }
    else
    {
        if( pData >= m_buff && pData < m_buff + m_size )  // pData is in the inner buffer !
        {
            AlignedBuf abuf( cnt, alignof(T) );
            memcpy( abuf.buffer(), pData, cnt * sizeof(T) );
            memmove( m_buff + idx + cnt, m_buff + idx, (m_size - idx) * sizeof(T) );
            memcpy( m_buff + idx, abuf.buffer(), cnt * sizeof(T) );
        }
        else
        {
            memmove( m_buff + idx + cnt, m_buff + idx, (m_size - idx) * sizeof(T) );
            memcpy( m_buff + idx, pData, cnt * sizeof(T) );
        }      
    }
    m_size += cnt;
}

// insert one value at position [idx]
template<class T>
inline void Vector<T>::insert( size_t idx, const T& value )
{
    assert( idx <= m_size );
    if( m_size + 1 > m_capacity )   // buffer is not big enough
    {
        size_t capacity = MAX( (m_size + 1)*3/2, 8 );
        T* pbuf = (T*)aligned_malloc( capacity * sizeof(T), m_align );
        memcpy( pbuf, m_buff, idx * sizeof(T) );
        pbuf[idx] = value;
        memcpy( pbuf + idx + 1, m_buff + idx, (m_size - idx) * sizeof(T) );
        this->set_buffer( pbuf, capacity );
    }
    else
    {
        T tmp = value;     // in case value is in the inner buffer
        memmove( m_buff + idx + 1, m_buff + idx,  (m_size - idx) * sizeof(T) );
        m_buff[idx] = tmp;
    }
    m_size++;
}

// like std::vector::erase
template<class T>
inline void Vector<T>::erase( size_t idx )
{
    assert( idx < m_size );
    size_t cnt = m_size - idx - 1;
    if( cnt > 0 )
        memmove( m_buff + idx, m_buff + idx + 1, cnt * sizeof(T) );
    m_size--;
}

// erase data between [beg, end)
template<class T>
inline void Vector<T>::erase( size_t beg, size_t end )
{
    assert( end > beg && end <= m_size );
    size_t cnt = m_size - end;
    if( cnt > 0 )
        memmove( m_buff + beg, m_buff + end, cnt * sizeof(T) );
    m_size -= end - beg;
}

// discard [cnt] value from the front
template<class T>
inline void Vector<T>::pop_front( size_t cnt )
{
    assert( cnt <= m_size );
    m_size -= cnt;
    if( m_size > 0 )
        memmove( m_buff, m_buff + cnt, m_size * sizeof(T) );
}

// remove the specific value from the vector
template<class T>
void Vector<T>::remove( const T& value )
{
    size_t k = m_size;
    size_t i = 0;
    for( ; i < m_size; i++ )  // find the first value
    {
        if( m_buff[i] == value )
        {
            k = i;
            i++;
            break;
        }
    }
    for( ; i < m_size; i++ )
    {
        if( m_buff[i] != value )
            m_buff[k++] = m_buff[i];
    }
    m_size = k;     // new size
}

template<class T>
template<class Pred>
void Vector<T>::remove( Pred pred )
{
    size_t k = m_size;
    size_t i = 0;
    for( ; i < m_size; i++ )  // find the first value
    {
        if( pred( m_buff[i]) )
        {
            k = i;
            i++;
            break;
        }
    }
    for( ; i < m_size; i++ )
    {
        if( !pred( m_buff[i] ) )
            m_buff[k++] = m_buff[i];
    }
    m_size = k;     // new size
}

// special function, allocate free memory at the tail of inner buffer
template<class T>
inline T* Vector<T>::alloc( size_t bufsize )
{
    assert( bufsize > 0 );
    this->reserve( m_size + bufsize );
    return m_buff + m_size;
}

// special function, commit data written to the buffer allocated by alloc()
template<class T>
inline void Vector<T>::commit( size_t cnt )
{
    assert( m_size + cnt <= m_capacity );
    m_size += cnt;
}

// special function, take away the inner buffer, new owner is responsible for buffer free
template<class T>
inline T* Vector<T>::take()
{
    T* pbuf = m_buff;
    m_buff = nullptr;  
    m_capacity = 0;
    m_size = 0;
    return pbuf;
}

template<typename T>
inline T* begin( Vector<T>& vec )
{
    return vec.data();
}
template<typename T>
inline T* end( Vector<T>& vec )
{
    return vec.data() + vec.size();
}

template<typename T>
inline const T* begin( const Vector<T>& vec )
{
    return vec.data();
}
template<typename T>
inline const T* end( const Vector<T>& vec )
{
    return vec.data() + vec.size();
}

} // namespace irk
#endif
