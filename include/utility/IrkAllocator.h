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

#ifndef _IRONBRICK_ALLOCATOR_H_
#define _IRONBRICK_ALLOCATOR_H_

#include "IrkMemPool.h"

namespace irk {

/* 
* Arena allocator, used to allocate small object on stack, thread-unsafe
* char buff[128];
* MemArena arena( buff, 128 );
* ArenaAllocator allocor( &arena ); 
*/
template<class T>
class ArenaAllocator
{
    template <class U> friend class ArenaAllocator;
    MemArena* m_Arena;  // observer, not owner
public:
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;
    typedef T* pointer;
    typedef const T* const_pointer;
    typedef T& reference;
    typedef const T& const_reference;
    typedef T value_type;
    typedef std::false_type propagate_on_container_copy_assignment;
    typedef std::false_type is_always_equal;

    template <class U> struct rebind { typedef ArenaAllocator<U> other; };

    // NOTE: arena must remain valid while allocator is alive
    explicit ArenaAllocator( MemArena* arena ) : m_Arena(arena) {}
    ArenaAllocator( const ArenaAllocator& ) = default;
    ArenaAllocator& operator=( const ArenaAllocator& ) = default;
    ArenaAllocator( ArenaAllocator&& ) = default;
    ArenaAllocator& operator=( ArenaAllocator&& ) = default;
    template <class U>
    ArenaAllocator( const ArenaAllocator<U>& other ) : m_Arena(other.m_Arena) {}
    
    size_t max_size() const 
    { 
        return UINT16_MAX;  // this class is for small buffer
    }
    T* allocate( size_t num, const void* hint = nullptr )
    {
        (void)hint;
        return reinterpret_cast<T*>( m_Arena->alloc(num*sizeof(T), alignof(T)) );
    }
    void deallocate( T* ptr, size_t num )
    {
        m_Arena->dealloc( ptr, num*sizeof(T) );
    }
    template<class U, class... Args>
    void construct( U* ptr, Args&&... args )
    {
        ::new((void*)ptr) U( std::forward<Args>(args)... );
    }
    template <class U>
    void destroy( U* ptr )
    {
        ptr->~U();
    }

    // get underlying arena
    MemArena* get_arena()               { return m_Arena; }
    const MemArena* get_arena() const   { return m_Arena; }
};

template <class T, class U>
inline bool operator==( const ArenaAllocator<T>& x, const ArenaAllocator<U>& y )
{
    return x.get_arena() == y.get_arena();
}
template <class T, class U>
inline bool operator!=( const ArenaAllocator<T>& x, const ArenaAllocator<U>& y )
{
    return !(x == y);
}

/* 
* Chunk allocator, used to allocate small object, thread-unsafe
* MemChunk chunk( 4096 );
* ChunkAllocator allocor( &chunk );
*/
template<class T>
class ChunkAllocator
{
    template <class U> friend class ChunkAllocator;
    MemChunks* m_Chunk;  // observer, not owner
public:
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;
    typedef T* pointer;
    typedef const T* const_pointer;
    typedef T& reference;
    typedef const T& const_reference;
    typedef T value_type;
    typedef std::false_type propagate_on_container_copy_assignment;
    typedef std::false_type is_always_equal;

    template <class U> struct rebind { typedef ChunkAllocator<U> other; };

    // NOTE: chunk must remain valid while allocator is alive
    explicit ChunkAllocator( MemChunks* pchunk ) : m_Chunk(pchunk) {}
    ChunkAllocator( const ChunkAllocator& ) = default;
    ChunkAllocator& operator=( const ChunkAllocator& ) = default;
    ChunkAllocator( ChunkAllocator&& ) = default;
    ChunkAllocator& operator=( ChunkAllocator&& ) = default;
    template <class U>
    ChunkAllocator( const ChunkAllocator<U>& other ) : m_Chunk(other.m_Chunk) {}
    
    size_t max_size() const 
    { 
        return UINT16_MAX;  // this class is for small buffer
    }
    T* allocate( size_t num, const void* hint = nullptr )
    {
        (void)hint;
        return reinterpret_cast<T*>( m_Chunk->alloc(num*sizeof(T), alignof(T)) );
    }
    void deallocate( T*, size_t )
    {
        // NOTE: memory item can not be freed individually
    }
    template<class U, class... Args>
    void construct( U* ptr, Args&&... args )
    {
        ::new((void*)ptr) U( std::forward<Args>(args)... );
    }
    template <class U>
    void destroy( U* ptr )
    {
        ptr->~U();
    }

    // get underlying memory chunck
    MemChunks* get_chunk()               { return m_Chunk; }
    const MemChunks* get_chunk() const   { return m_Chunk; }
};

template <class T, class U>
inline bool operator==(const ChunkAllocator<T>& x, const ChunkAllocator<U>& y)
{
    return x.get_chunk() == y.get_chunk();
}
template <class T, class U>
inline bool operator!=(const ChunkAllocator<T>& x, const ChunkAllocator<U>& y)
{
    return !(x == y);
}

/* 
* Pool allocator, used to allocate small object, thread-unsafe
* MemPool pool( 4096 );
* PoolAllocator allocor( &pool );
*/
template<class T>
class PoolAllocator
{
    template <class U> friend class PoolAllocator;
    MemPool* m_Pool;    // observer, not owner
public:
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;
    typedef T* pointer;
    typedef const T* const_pointer;
    typedef T& reference;
    typedef const T& const_reference;
    typedef T value_type;
    typedef std::false_type propagate_on_container_copy_assignment;
    typedef std::false_type is_always_equal;

    template <class U> struct rebind { typedef PoolAllocator<U> other; };

    // NOTE: pool must remain valid while allocator is alive
    PoolAllocator( MemPool* pool ) : m_Pool(pool) {}
    PoolAllocator( const PoolAllocator& ) = default;
    PoolAllocator& operator=( const PoolAllocator& ) = default;
    PoolAllocator( PoolAllocator&& ) = default;
    PoolAllocator& operator=( PoolAllocator&& ) = default;
    template <class U>
    PoolAllocator( const PoolAllocator<U>& other ) : m_Pool(other.m_Pool) {}
    
    size_t max_size() const 
    { 
        return UINT16_MAX;  // this class is for small buffer
    }
    T* allocate( size_t num, const void* hint = nullptr )
    {
        (void)hint;
        return reinterpret_cast<T*>( m_Pool->alloc(num*sizeof(T), alignof(T)) );
    }
    void deallocate( T* ptr, size_t num )
    {
        m_Pool->dealloc( ptr, num*sizeof(T) );
    }
    template<class U, class... Args>
    void construct( U* ptr, Args&&... args )
    {
        ::new((void*)ptr) U( std::forward<Args>(args)... );
    }
    template <class U>
    void destroy( U* ptr )
    {
        ptr->~U();
    }

    // get underlying memory pool
    MemPool* get_pool()             { return m_Pool; }
    const MemPool* get_pool() const { return m_Pool; }
};

template <class T, class U>
inline bool operator==(const PoolAllocator<T>& x, const PoolAllocator<U>& y)
{
    return x.get_pool() == y.get_pool();
}
template <class T, class U>
inline bool operator!=(const PoolAllocator<T>& x, const PoolAllocator<U>& y)
{
    return !(x == y);
}

} // namespace irk
#endif
