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

#ifndef _IRONBRICK_REFCNTOBJ_H_
#define _IRONBRICK_REFCNTOBJ_H_

#include <utility>
#include <type_traits>
#include "IrkAtomic.h"

namespace irk {

// base class of intrusive reference counted object
class RefCntObj : IrkNocopy
{
public:
    explicit RefCntObj() : m_refCnt(0) {}
    virtual ~RefCntObj() {};

    // add reference count to retain this object
    int add_ref() noexcept
    {
        return atomic_inc(&m_refCnt);
    }

    // dismiss this object, if reference count become 0 also delete it
    virtual int dismiss()
    {
        const int refcnt = atomic_dec(&m_refCnt);
        assert(refcnt >= 0);
        if (refcnt == 0)
            delete this;    // override this method if object is not create by new
        return refcnt;
    }

    int ref_count() const { return m_refCnt; }

protected:
    volatile int m_refCnt;
};

// shared pointer of intrusive reference counted object
template<typename T>
class RefPtr
{
public:
    RefPtr() : m_ptr(nullptr) {}
    RefPtr(T* ptr)
    {
        m_ptr = ptr;
        if (m_ptr)
            m_ptr->add_ref();
    }
    ~RefPtr()
    {
        if (m_ptr)
            m_ptr->dismiss();
    }
    RefPtr(const RefPtr& other) noexcept
    {
        m_ptr = other.m_ptr;
        if (m_ptr)
            m_ptr->add_ref();
    }
    RefPtr(RefPtr&& other) noexcept
    {
        m_ptr = other.m_ptr;
        other.m_ptr = nullptr;
    }
    RefPtr& operator=(const RefPtr& other) noexcept
    {
        if (this != &other)
            this->rebind(other.m_ptr);
        return *this;
    }
    RefPtr& operator=(RefPtr&& other) noexcept
    {
        if (this != &other)
        {
            if (m_ptr)
                m_ptr->dismiss();
            m_ptr = other.m_ptr;
            other.m_ptr = nullptr;
        }
        return *this;
    }

    // copy/assign from derived class
    template<typename U>
    RefPtr(const RefPtr<U>& other) noexcept
    {
        static_assert(std::is_base_of<T, U>::value, "");
        m_ptr = other.pointer();
        if (m_ptr)
            m_ptr->add_ref();
    }
    template<typename U>
    RefPtr(RefPtr<U>&& other) noexcept
    {
        static_assert(std::is_base_of<T, U>::value, "");
        m_ptr = other.detach();
    }
    template<typename U>
    RefPtr& operator=(const RefPtr<U>& other) noexcept
    {
        static_assert(std::is_base_of<T, U>::value, "");
        if (m_ptr)
            m_ptr->dismiss();
        m_ptr = other.pointer();
        if (m_ptr)
            m_ptr->add_ref();
        return *this;
    }
    template<typename U>
    RefPtr& operator=(RefPtr<U>&& other) noexcept
    {
        static_assert(std::is_base_of<T, U>::value, "");
        if (m_ptr)
            m_ptr->dismiss();
        m_ptr = other.detach();
        return *this;
    }

    T* pointer() const noexcept // view
    {
        return m_ptr;
    }
    T* detach() noexcept        // handout ownership
    {
        T* ptr = m_ptr;
        m_ptr = nullptr;
        return ptr;
    }
    void operator=(decltype(nullptr)) noexcept
    {
        if (m_ptr)
        {
            m_ptr->dismiss();
            m_ptr = nullptr;
        }
    }
    void reset() noexcept
    {
        if (m_ptr)
        {
            m_ptr->dismiss();
            m_ptr = nullptr;
        }
    }
    void rebind(T* ptr) noexcept
    {
        if (m_ptr)
            m_ptr->dismiss();
        m_ptr = ptr;
        if (m_ptr)
            m_ptr->add_ref();
    }
    void swap(RefPtr& other) noexcept
    {
        T* tmp = m_ptr;
        m_ptr = other.m_ptr;
        other.m_ptr = tmp;
    }

    T*          operator->() const      { return m_ptr; }
    T&          operator*() const       { return *m_ptr; }
    explicit    operator bool() const   { return m_ptr != nullptr; }
    bool        operator!() const       { return m_ptr == nullptr; }
private:
    T * m_ptr;
};

template<typename T, typename... Args>
inline RefPtr<T> make_refptr(Args&&... args)
{
    T* ptr = new T{std::forward<Args>(args)...};
    return RefPtr<T>{ ptr };
}

template<typename T, typename U>
inline bool operator==(const RefPtr<T>& p1, const RefPtr<U>& p2)
{
    return p1.pointer() == p2.pointer();
}
template<typename T, typename U>
inline bool operator!=(const RefPtr<T>& p1, const RefPtr<U>& p2)
{
    return p1.pointer() != p2.pointer();
}
template<typename T>
inline bool operator==(const RefPtr<T>& p1, decltype(nullptr))
{
    return p1.pointer() == nullptr;
}
template<typename T>
inline bool operator!=(const RefPtr<T>& p1, decltype(nullptr))
{
    return p1.pointer() != nullptr;
}

}   // namespace irk
#endif
