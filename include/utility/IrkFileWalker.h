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

#ifndef _IRONBRICK_FILEWALKER_H_
#define _IRONBRICK_FILEWALKER_H_

#include <string>
#include "IrkCFile.h"

namespace irk {

// read text file line by line
class FileWalker : IrkNocopy
{
    static constexpr size_t BUFSZ = 1024;
    static constexpr size_t INSANELINESZ = 64 * 1024 * 1024;
public:
    explicit FileWalker( const char* filename ) : m_File(filename, "r"), m_Buf(nullptr)
    {
        m_Buf = new char[BUFSZ];
    }
#ifdef _WIN32
    explicit FileWalker( const wchar_t* filename ) : m_File(filename, L"r"), m_Buf(nullptr)
    {
        m_Buf = new char[BUFSZ];
    }
#endif
    ~FileWalker()
    {
        delete[] m_Buf;
    }
    FileWalker( FileWalker&& rhs ) noexcept : m_File( std::move(rhs.m_File) )   // move construction      
    {
        m_Buf = rhs.m_Buf;
        rhs.m_Buf = nullptr;
    }
    FileWalker& operator=( FileWalker&& rhs ) noexcept  // move assignment
    {
        if( this != &rhs )
            m_File = std::move( rhs.m_File );
        // no need to move inner buffer
        return *this;
    }

    // check file opened
    explicit operator bool() const  { return m_File.get() != nullptr; }
    bool is_opened() const          { return m_File.get() != nullptr; }
   
    // read next line, NULL is returned to indicate an error or an end-of-file condition
    // if pLength != NULL, *pLength = line's length
    // NOTE: if the line's length is more than 64M, NULL is returned
    // NOTE: The newline character is discarded
    const char* next_line( int* pLength = nullptr );

    // check end-of-file
    bool eof() const
    {
        if( m_File )
            return feof( m_File.get() );
        return false;
    }

    // rewind to the beginning of the file
    void rewind()
    {
        if( m_File )
            ::rewind( m_File );
    }

    // uncanonical iterator, use with ranged-for
    class Iterator
    {
    public:
        typedef const char* value_type;
        bool operator==( const Iterator& other ) const  { return m_line == other.m_line; }
        bool operator!=( const Iterator& other ) const  { return m_line != other.m_line; }
        Iterator& operator++()
        {
            if( m_line ) { m_line = m_walker->next_line(); }
            return *this;
        }
        const char* operator*() const   { return m_line; }
    private:
        friend FileWalker;
        Iterator() : m_walker(nullptr), m_line(nullptr) {}
        Iterator( FileWalker* walker ) : m_walker(walker), m_line(walker->next_line()) {}
        FileWalker* m_walker;
        const char* m_line;
    };
    // WARNING: begin will fetch next line, you may need to call rewind() if you have called next_line() before
    Iterator begin()    { return Iterator{this}; }
    Iterator end()      { return Iterator{}; }
private:
    const char* getlongline( int* );
    CFile       m_File;
    char*       m_Buf;
    std::string m_LongLine;
};

} // namespace irk

#endif
