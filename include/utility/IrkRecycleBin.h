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

#ifndef _IRONBRICK_RECYCLEBIN_H_
#define _IRONBRICK_RECYCLEBIN_H_

#include <memory>
#include <mutex>
#include "IrkSpinMutex.h"
#include "IrkContract.h"

namespace irk {

// simple recycle bin, used to cache objects
template<class Ty, class Dx = std::default_delete<Ty>>
class RecycleBin : IrkNocopy
{
public:
    explicit RecycleBin(int capacity, Dx deleter = Dx())
        : m_Cache(nullptr), m_Cnt(0), m_Capacity(0), m_Deleter(deleter)
    {
        if (capacity > 0)
        {
            m_Cache = new Ty*[capacity];
            m_Capacity = capacity;
        }
    }
    ~RecycleBin()
    {
        this->clear();
        delete[] m_Cache;
    }

    // reset the size of recycle bin, delete all cached objects(if exists)
    void reset(int capacity = 0);

    // delete all cached objects
    void clear();

    bool is_empty() const   { return m_Cnt == 0; }
    bool is_full() const    { return m_Cnt >= m_Capacity; }
    int  count() const      { return m_Cnt; }
    int  capacity() const   { return m_Capacity; }

    // dump object into recycle bin, delete it if recycle bin is full
    void dump(Ty* obj)
    {
        irk_expect(obj != nullptr);

        if (m_Cnt < m_Capacity)
            m_Cache[m_Cnt++] = obj;
        else
            m_Deleter(obj);
    }

    // get back object, return NULL if recycle bin is empty
    Ty* getback()
    {
        if (m_Cnt > 0)
            return m_Cache[--m_Cnt];
        return nullptr;
    }
private:
    Ty * *    m_Cache;
    int     m_Cnt;          // current cached object number
    int     m_Capacity;     // max number of objects can be cached
    Dx      m_Deleter;      // the functor used to delete object
};

// delete all cached objects
template<class Ty, class Dx>
void RecycleBin<Ty, Dx>::clear()
{
    for (int i = 0; i < m_Cnt; i++)
        m_Deleter(m_Cache[i]);
    m_Cnt = 0;
}

// reset size of the recycle bin, delete all cached objects(if exists)
template<class Ty, class Dx>
void RecycleBin<Ty, Dx>::reset(int capacity)
{
    if (m_Cache)   // purge old objects
    {
        for (int i = 0; i < m_Cnt; i++)
            m_Deleter(m_Cache[i]);
        delete[] m_Cache;
        m_Cache = nullptr;
        m_Cnt = 0;
        m_Capacity = 0;
    }

    irk_ensure(m_Cache == nullptr);
    irk_ensure(m_Cnt == 0 && m_Capacity == 0);

    if (capacity > 0)
    {
        m_Cache = new Ty*[capacity];
        m_Capacity = capacity;
    }
}

// the thread-safe version
template<class Ty, class Dx = std::default_delete<Ty>, class Mx = SpinMutex>
class SyncedRecycleBin : IrkNocopy
{
public:
    explicit SyncedRecycleBin(int capacity, Dx deleter = Dx())
        : m_Cache(nullptr), m_Cnt(0), m_Capacity(0), m_Deleter(deleter)
    {
        if (capacity > 0)
        {
            m_Cache = new Ty*[capacity];
            m_Capacity = capacity;
        }
    }
    ~SyncedRecycleBin()
    {
        this->clear();
        delete[] m_Cache;
    }

    // reset the size of recycle bin, delete all cached objects(if exists)
    void reset(int capacity = 0);

    // delete all cached objects
    void clear();

    bool is_empty() const   { return m_Cnt == 0; }
    bool is_full() const    { return m_Cnt >= m_Capacity; }
    int  count() const      { return m_Cnt; }
    int  capacity() const   { return m_Capacity; }

    // dump object into recycle bin, delete it if recycle bin is full
    void dump(Ty* obj)
    {
        irk_expect(obj != nullptr);
        std::lock_guard<Mx> lock_(m_Mutex);

        if (m_Cnt < m_Capacity)
            m_Cache[m_Cnt++] = obj;
        else
            m_Deleter(obj);
    }

    // getback object, return NULL if recycle bin is empty
    Ty* getback()
    {
        std::lock_guard<Mx> lock_(m_Mutex);

        if (m_Cnt > 0)
            return m_Cache[--m_Cnt];
        return nullptr;
    }

private:
    Ty**    m_Cache;
    int     m_Cnt;              // current cached object number
    int     m_Capacity;         // max number of objects can be cached
    Dx      m_Deleter;          // the functor used to delete object
    Mx      m_Mutex;            // inner mutex
};

// delete all cached objects
template<class Ty, class Dx, class Mx>
void SyncedRecycleBin<Ty, Dx, Mx>::clear()
{
    std::lock_guard<Mx> lock_(m_Mutex);

    for (int i = 0; i < m_Cnt; i++)
        m_Deleter(m_Cache[i]);
    m_Cnt = 0;
}

// reset size of the recycle bin, delete all cached objects(if exists)
template<class Ty, class Dx, class Mx>
void SyncedRecycleBin<Ty, Dx, Mx>::reset(int capacity)
{
    std::lock_guard<Mx> lock_(m_Mutex);

    if (m_Cache)    // purge old objects
    {
        for (int i = 0; i < m_Cnt; i++)
            m_Deleter(m_Cache[i]);
        delete[] m_Cache;
        m_Cache = nullptr;
        m_Cnt = 0;
        m_Capacity = 0;
    }

    irk_ensure(m_Cache == nullptr);
    irk_ensure(m_Cnt == 0 && m_Capacity == 0);

    if (capacity > 0)
    {
        m_Cache = new Ty*[capacity];
        m_Capacity = capacity;
    }
}

} // namespace irk
#endif
