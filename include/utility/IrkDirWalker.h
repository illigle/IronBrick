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

#ifndef _IRONBRICK_FILEWALKER_H_
#define _IRONBRICK_FILEWALKER_H_

#include <string.h>
#include "IrkCommon.h"

namespace irk {

struct DirEntry
{
    enum    // entry type
    {
        File,           // regular file  
        Dir,            // sub-directory
        Link,           // symbolic link
        Other,          // other type
    };
    int         type;   // entry type
    const char* name;   // entry name

    bool is_file() const    { return this->type == DirEntry::File; }
    bool is_dir() const     { return this->type == DirEntry::Dir; }

    // file extension
    const char* extension() const
    {
        if( this->is_file() )
        {
            const char* dot = ::strrchr( this->name, '.' );
            if( dot && dot != this->name )
                return dot;
        }
        return "";
    }
};

class DirWalker : IrkNocopy
{
public:
    explicit DirWalker( const char* dirPath );
    ~DirWalker();
    DirWalker( DirWalker&& other ) noexcept;
    DirWalker& operator=( DirWalker&& other ) noexcept;

    // check dir opened
    explicit operator bool() const  { return m_pWalker != nullptr; }
    bool is_opened() const          { return m_pWalker != nullptr; }
    
    // rewind to the beginning of the dir
    bool rewind();

    // get next entry,
    // if no more entry exists or an error occured, NULL is returned
    // if error occured and perr != NULL, *perr = native system error code
    const DirEntry* next_entry( int* perr = nullptr );

    // uncanonical iterator, use with ranged-for
    class Iterator
    {
    public:
        typedef const DirEntry* value_type;
        bool operator==( const Iterator& other ) const  { return m_entry == other.m_entry; }
        bool operator!=( const Iterator& other ) const  { return m_entry != other.m_entry; }
        Iterator& operator++()
        {
            if( m_entry ) { m_entry = m_walker->next_entry(); }
            return *this;
        }
        const DirEntry* operator*() const { return m_entry; }
    private:
        friend DirWalker;
        Iterator() : m_walker(nullptr), m_entry(nullptr) {}
        Iterator( DirWalker* walker ) : m_walker(walker), m_entry(walker->next_entry()) {}
        DirWalker*      m_walker;
        const DirEntry* m_entry;
    };
    // WARNING: begin will fetch next entry, you may need to call rewind() if you have called next_entry() before
    Iterator begin()    { return Iterator{this}; }
    Iterator end()      { return Iterator{}; }

private:
    class ImplDirWalk* m_pWalker;
};

//======================================================================================================================
// wchar_t version under windows

#ifdef _WIN32

struct DirEntryW
{
    int             type;   // entry type
    const wchar_t*  name;   // entry name

    bool is_file() const    { return this->type == DirEntry::File; }
    bool is_dir() const     { return this->type == DirEntry::Dir; }

    // file extension
    const wchar_t* extension() const
    {
        if( this->is_file() )
        {
            const wchar_t* dot = ::wcsrchr( this->name, '.' );
            if( dot && dot != this->name )
                return dot;
        }
        return L"";
    }
};

class DirWalkerW : IrkNocopy
{
public:
    explicit DirWalkerW( const wchar_t* dirPath );
    ~DirWalkerW();
    DirWalkerW( DirWalkerW&& other ) noexcept;
    DirWalkerW& operator=( DirWalkerW&& other ) noexcept;

    // check dir opened
    explicit operator bool() const  { return m_pWalker != nullptr; }
    bool is_opened() const          { return m_pWalker != nullptr; }
    
    // rewind to the beginning of the dir
    bool rewind();

    // get next entry,
    // if no more entry exists or an error occured, NULL is returned
    // if error occured and perr != NULL, *perr = native system error code
    const DirEntryW* next_entry( int* perr = nullptr );

    // uncanonical iterator, use with ranged-for
    class Iterator
    {
    public:
        typedef const DirEntryW* value_type;
        bool operator==( const Iterator& other ) const  { return m_entry == other.m_entry; }
        bool operator!=( const Iterator& other ) const  { return m_entry != other.m_entry; }
        Iterator& operator++()
        {
            if( m_entry ) { m_entry = m_walker->next_entry(); }
            return *this;
        }
        const DirEntryW* operator*() const { return m_entry; }
    private:
        friend DirWalkerW;
        Iterator() : m_walker(nullptr), m_entry(nullptr) {}
        Iterator( DirWalkerW* walker ) : m_walker(walker), m_entry(walker->next_entry()) {}
        DirWalkerW*      m_walker;
        const DirEntryW* m_entry;
    };
    // WARNING: begin will fetch next entry, you may need to call rewind() if you have called next_entry() before
    Iterator begin()    { return Iterator{this}; }
    Iterator end()      { return Iterator{}; }

private:
    class ImplDirWalkW* m_pWalker;
};

#endif

}   // namespace irk

#endif
