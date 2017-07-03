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

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#include <dirent.h>
#endif
#include <string>
#include <errno.h>
#include "IrkDirWalker.h"

namespace irk {

#ifdef _WIN32

static inline int get_dir_entry_type( DWORD fattr )
{
    if( fattr & FILE_ATTRIBUTE_DIRECTORY )
        return DirEntry::Dir;
    return DirEntry::File;
}

class ImplDirWalk
{
public:
    ImplDirWalk() : m_engaged(false), m_hFind(INVALID_HANDLE_VALUE) {}
    ~ImplDirWalk()
    {
        if( m_hFind != INVALID_HANDLE_VALUE )
            ::FindClose( m_hFind );
    }

    bool open_dir( const char* dirPath )
    {
        m_findStr = dirPath;
        for( char& ch : m_findStr )
        {
            if( ch == '/' )
                ch = '\\';
        }
        m_findStr += "\\*.*";
        return this->find_first();
    }

    // get next entry
    const DirEntry* get_next( int* errc )
    {
        if( m_hFind == INVALID_HANDLE_VALUE )
        {
            *errc = ERROR_INVALID_HANDLE;
            return nullptr;
        }

        while( 1 )
        {
            if( m_engaged )     // already got an entry
            {
                m_engaged = false;
            }
            else
            {
                if( !::FindNextFileA( m_hFind, &m_fdata ) )
                {
                    *errc = ::GetLastError();
                    if( *errc == ERROR_NO_MORE_FILES )  // no more files
                    {
                        ::FindClose( m_hFind );
                        m_hFind = INVALID_HANDLE_VALUE;
                        *errc = 0;
                    }
                    return nullptr;
                }
            }

            if( (::strcmp(m_fdata.cFileName, ".") != 0) && (::strcmp(m_fdata.cFileName, "..") != 0) )
                break;
        }

        *errc = 0;
        m_entry.type = get_dir_entry_type( m_fdata.dwFileAttributes );
        m_entry.name = m_fdata.cFileName;
        return &m_entry;
    }

    // rewind to the beginning of the dir
    bool rewind_dir()
    {
        if( m_hFind != INVALID_HANDLE_VALUE )
        {
            ::FindClose( m_hFind );
            m_hFind = INVALID_HANDLE_VALUE;
        }
        if( !m_findStr.empty() )
        {
            return this->find_first();
        }
        return false;
    }

private:
    bool find_first()
    {
        assert( m_hFind == INVALID_HANDLE_VALUE );
        m_hFind = ::FindFirstFileA( m_findStr.c_str(), &m_fdata );
        m_engaged = (m_hFind != INVALID_HANDLE_VALUE);
        return m_engaged;
    }
    bool                m_engaged;
    HANDLE              m_hFind;
    WIN32_FIND_DATAA    m_fdata;
    DirEntry            m_entry;
    std::string         m_findStr;
};

class ImplDirWalkW
{
public:
    ImplDirWalkW() : m_engaged(false), m_hFind(INVALID_HANDLE_VALUE) {}
    ~ImplDirWalkW()
    {
        if( m_hFind != INVALID_HANDLE_VALUE )
            ::FindClose( m_hFind );
    }

    bool open_dir( const wchar_t* dirPath )
    {
        m_findStr = dirPath;
        for( wchar_t& ch : m_findStr )
        {
            if( ch == L'/' )
                ch = L'\\';
        }
        m_findStr += L"\\*.*";
        return this->find_first();
    }

    // get next entry
    const DirEntryW* get_next( int* errc )
    {
        if( m_hFind == INVALID_HANDLE_VALUE )
        {
            *errc = ERROR_INVALID_HANDLE;
            return nullptr;
        }

        while( 1 )
        {
            if( m_engaged )     // already got an entry
            {
                m_engaged = false;
            }
            else
            {
                if( !::FindNextFileW( m_hFind, &m_fdata ) )
                {
                    *errc = ::GetLastError();
                    if( *errc == ERROR_NO_MORE_FILES )  // no more files
                    {
                        ::FindClose( m_hFind );
                        m_hFind = INVALID_HANDLE_VALUE;
                        *errc = 0;
                    }
                    return nullptr;
                }
            }

            if( (::wcscmp(m_fdata.cFileName, L".") != 0) && (::wcscmp(m_fdata.cFileName, L"..") != 0) )
                break;
        }

        *errc = 0;
        m_entry.type = get_dir_entry_type( m_fdata.dwFileAttributes );
        m_entry.name = m_fdata.cFileName;
        return &m_entry;
    }

    // rewind to the beginning of the dir
    bool rewind_dir()
    {
        if( m_hFind != INVALID_HANDLE_VALUE )
        {
            ::FindClose( m_hFind );
            m_hFind = INVALID_HANDLE_VALUE;
        }
        if( !m_findStr.empty() )
        {
            return this->find_first();
        }
        return false;
    }

private:
    bool find_first()
    {
        assert( m_hFind == INVALID_HANDLE_VALUE );
        m_hFind = ::FindFirstFileW( m_findStr.c_str(), &m_fdata );
        m_engaged = (m_hFind != INVALID_HANDLE_VALUE);
        return m_engaged;
    }
    bool                m_engaged;
    HANDLE              m_hFind;
    WIN32_FIND_DATAW    m_fdata;
    DirEntryW           m_entry;
    std::wstring        m_findStr;
};

DirWalkerW::DirWalkerW( const wchar_t* dirPath ) : m_pWalker(nullptr)
{
    m_pWalker = new ImplDirWalkW;
    if( !m_pWalker->open_dir(dirPath) )
    {
        delete m_pWalker;
        m_pWalker = nullptr;
    }
}
DirWalkerW::~DirWalkerW()
{
    delete m_pWalker;
}
DirWalkerW::DirWalkerW( DirWalkerW&& other ) noexcept
{
    m_pWalker = other.m_pWalker;
    other.m_pWalker = nullptr;
}
DirWalkerW& DirWalkerW::operator=( DirWalkerW&& other ) noexcept
{
    if( this != &other )
    {
        if( m_pWalker )
            delete m_pWalker;
        m_pWalker = other.m_pWalker;
        other.m_pWalker = nullptr;
    }
    return *this;
}

// rewind to the beginning of the dir
bool DirWalkerW::rewind()
{
    if( !m_pWalker )
        return false;
    return m_pWalker->rewind_dir();
}

// get next entry,
// if no more entry exists or an error occured, NULL is returned
const DirEntryW* DirWalkerW::next_entry( int* perr )
{
    if( !m_pWalker )
    {
        if( perr )
            *perr = ERROR_INVALID_HANDLE;
        return nullptr;
    }
    int temp = 0;
    int* errc = perr ? perr : &temp;
    return m_pWalker->get_next( errc );
}

#else

static inline int get_dir_entry_type( const struct dirent* dire )
{
    if( dire->d_type == DT_REG )
        return DirEntry::File;
    else if( dire->d_type == DT_DIR )
        return DirEntry::Dir;
    else if( dire->d_type == DT_LNK )
        return DirEntry::Link;
    return DirEntry::Other;
}

class ImplDirWalk
{
public:
    ImplDirWalk() : m_hDir(NULL) {}
    ~ImplDirWalk()
    {
        if( m_hDir )
            ::closedir( m_hDir );
    }

    bool open_dir( const char* dirPath )
    {
        assert( !m_hDir );
        m_hDir = ::opendir( dirPath );
        return m_hDir != NULL;
    }

    // get next entry
    const DirEntry* get_next( int* errc )
    {
        if( !m_hDir )
        {
            *errc = EBADF;
            return nullptr;
        }
        while( 1 )
        {
            const struct dirent* dire = ::readdir( m_hDir );
            if( !dire )
            {
                *errc = errno;
                return nullptr;
            }
            if( ::strcmp(dire->d_name, ".")==0 || ::strcmp(dire->d_name, "..")==0 )
                continue;

            *errc = 0;
            m_entry.type = get_dir_entry_type( dire );
            m_entry.name = dire->d_name;
            break;
        }
        return &m_entry;
    }

    // rewind to the beginning of the dir
    bool rewind_dir()
    {
        if( m_hDir )
        {
            ::rewinddir( m_hDir );
            return true;
        }
        return false;
    }

private:
    DIR*        m_hDir;
    DirEntry    m_entry;
};

#endif

DirWalker::DirWalker( const char* dirPath ) : m_pWalker(nullptr)
{
    m_pWalker = new ImplDirWalk;
    if( !m_pWalker->open_dir(dirPath) )
    {
        delete m_pWalker;
        m_pWalker = nullptr;
    }
}
DirWalker::~DirWalker()
{
    delete m_pWalker;
}
DirWalker::DirWalker( DirWalker&& other ) noexcept
{
    m_pWalker = other.m_pWalker;
    other.m_pWalker = nullptr;
}
DirWalker& DirWalker::operator=( DirWalker&& other ) noexcept
{
    if( this != &other )
    {
        if( m_pWalker )
            delete m_pWalker;
        m_pWalker = other.m_pWalker;
        other.m_pWalker = nullptr;
    }
    return *this;
}

// rewind to the beginning of the dir
bool DirWalker::rewind()
{
    if( !m_pWalker )
        return false;
    return m_pWalker->rewind_dir();
}

// get next entry,
// if no more entry exists or an error occured, NULL is returned
const DirEntry* DirWalker::next_entry( int* perr )
{
    if( !m_pWalker )
    {
        if( perr )
        {
        #ifdef _WIN32
            *perr = ERROR_INVALID_HANDLE;
        #else
            *perr = EBADF;
        #endif
        }
        return nullptr;
    }
    int temp = 0;
    int* errc = perr ? perr : &temp;
    return m_pWalker->get_next( errc );
}

}   // namespace irk
