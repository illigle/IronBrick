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

#ifndef _IRONBRICK_CFILE_H_
#define _IRONBRICK_CFILE_H_

#ifdef _WIN32
#include <io.h>
#include <direct.h>
#else
#include <unistd.h>
#ifdef __linux__
#include <sys/sendfile.h>
#elif defined(__MACH__)
#include <copyfile.h>
#endif
#endif
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include "IrkContract.h"

#ifdef _WIN32
extern "C"
{
    __declspec(dllimport) int __stdcall CopyFileA(const char*, const char*, int);
    __declspec(dllimport) int __stdcall CopyFileW(const wchar_t*, const wchar_t*, int);
}
#else
#if !defined(_FILE_OFFSET_BITS) || (_FILE_OFFSET_BITS != 64)
#error Please add -D_FILE_OFFSET_BITS=64 compiler flag!
#endif
#endif

namespace irk {

#ifdef _WIN32
typedef struct _stat64 FileStat;
#else
typedef struct stat FileStat;
#endif

// C standard file
class CFile : IrkNocopy
{
public:
    CFile() : m_fp(nullptr) {}
    ~CFile()
    {
        if (m_fp)
            ::fclose(m_fp);
    }
    CFile(const char* filename, const char* mode)
    {
        m_fp = ::fopen(filename, mode);
    }
#ifdef _WIN32
    CFile(const wchar_t* filename, const wchar_t* mode)
    {
        m_fp = ::_wfopen(filename, mode);
    }
#endif
    CFile(CFile&& rhs)  noexcept  // move construction
    {
        this->m_fp = rhs.m_fp;
        rhs.m_fp = nullptr;
    }
    CFile& operator=(CFile&& rhs) noexcept    // move assignment
    {
        if (this != &rhs)
        {
            if (m_fp)
                ::fclose(m_fp);
            this->m_fp = rhs.m_fp;
            rhs.m_fp = nullptr;
        }
        return *this;
    }

    // FILE* access
    FILE* get() const               { return m_fp; }
    operator FILE*() const          { return m_fp; }
    bool operator!() const          { return m_fp == nullptr; }
    explicit operator bool() const  { return m_fp != nullptr; }

    // open file, return false if failed
    bool open(const char* filename, const char* mode)
    {
        irk_expect(!m_fp);
        m_fp = ::fopen(filename, mode);
        return m_fp != nullptr;
    }
#ifdef _WIN32
    bool open(const wchar_t* filename, const wchar_t* mode)
    {
        irk_expect(!m_fp);
        m_fp = ::_wfopen(filename, mode);
        return m_fp != nullptr;
    }
#endif

    // close file
    void close()
    {
        if (m_fp)
        {
            ::fclose(m_fp);
            m_fp = nullptr;
        }
    }

#ifdef _WIN32
    // get file size in bytes, return -1 if failed
    int64_t file_size() const
    {
        if (!m_fp)
            return -1;
        return ::_filelengthi64(_fileno(m_fp));
    }
    // create time, seconds elapsed since 1970-1-1 00:00:00, UTC, return -1 if failed
    int64_t create_time() const
    {
        struct _stat64 st;
        if (m_fp && ::_fstat64(_fileno(m_fp), &st) == 0)
            return st.st_ctime;
        return -1;
    }
    // modify time, seconds elapsed since 1970-1-1 00:00:00, UTC, return -1 if failed
    int64_t modify_time() const
    {
        struct _stat64 st;
        if (m_fp && ::_fstat64(_fileno(m_fp), &st) == 0)
            return st.st_mtime;
        return -1;
    }
    // get file state, return 0 if succeeded, return errno if failed
    int file_stat(FileStat* pst) const
    {
        if (!m_fp)
            return EBADF;
        if (::_fstat64(_fileno(m_fp), pst) == 0)
            return 0;
        return errno;
    }
#else
    int64_t file_size() const
    {
        struct stat st;
        if (m_fp && ::fstat(fileno(m_fp), &st) == 0)
            return st.st_size;
        return -1;
    }
    int64_t create_time() const
    {
        struct stat st;
        if (m_fp && ::fstat(fileno(m_fp), &st) == 0)
            return st.st_ctime;
        return -1;
    }
    int64_t modify_time() const
    {
        struct stat st;
        if (m_fp && ::fstat(fileno(m_fp), &st) == 0)
            return st.st_mtime;
        return -1;
    }
    int file_stat(FileStat* pst) const
    {
        if (!m_fp)
            return EBADF;
        if (::fstat(fileno(m_fp), pst) == 0)
            return 0;
        return errno;
    }
#endif
private:
    FILE*   m_fp;
};

// default permission mode when create new file
#ifdef _WIN32
#define FMODE_DFT (_S_IREAD | _S_IWRITE)
#else
#define FMODE_DFT (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)
#endif

// POSIX standard file
class PosixFile : IrkNocopy
{
public:
    PosixFile() : m_fd(-1) {}
    ~PosixFile()
    {
        if (m_fd >= 0)
            ::close(m_fd);
    }
    PosixFile(const char* filename, int oflag, int pmode = FMODE_DFT)
    {
        if (oflag & O_CREAT)
            m_fd = ::open(filename, oflag, pmode);
        else
            m_fd = ::open(filename, oflag);
    }
#ifdef _WIN32
    PosixFile(const wchar_t* filename, int oflag, int pmode = FMODE_DFT)
    {
        if (oflag & _O_CREAT)
            m_fd = ::_wopen(filename, oflag, pmode);
        else
            m_fd = ::_wopen(filename, oflag);
    }
#endif
    PosixFile(PosixFile&& rhs)  noexcept  // move construction
    {
        this->m_fd = rhs.m_fd;
        rhs.m_fd = -1;
    }
    PosixFile& operator=(PosixFile&& rhs) noexcept    // move assignment
    {
        if (this != &rhs)
        {
            if (m_fd >= 0)
                ::close(m_fd);
            this->m_fd = rhs.m_fd;
            rhs.m_fd = -1;
        }
        return *this;
    }

    // open file, return false if failed
    bool open(const char* filename, int oflag, int pmode = FMODE_DFT)
    {
        irk_expect(m_fd < 0);
        if (oflag & O_CREAT)
            m_fd = ::open(filename, oflag, pmode);
        else
            m_fd = ::open(filename, oflag);
        return m_fd >= 0;
    }
#ifdef _WIN32
    bool open(const wchar_t* filename, int oflag, int pmode = FMODE_DFT)
    {
        irk_expect(m_fd < 0);
        if (oflag & _O_CREAT)
            m_fd = ::_wopen(filename, oflag, pmode);
        else
            m_fd = ::_wopen(filename, oflag);
        return m_fd >= 0;
    }
#endif

    // close file
    void close()
    {
        if (m_fd >= 0)
        {
            ::close(m_fd);
            m_fd = -1;
        }
    }

    // file descriptor access
    int get() const                 { return m_fd; }
    operator int() const            { return m_fd; }
    bool operator!() const          { return m_fd < 0; }
    explicit operator bool() const  { return m_fd >= 0; }

#ifdef _WIN32
    // get file size in bytes, return -1 if failed
    int64_t file_size() const
    {
        if (m_fd < 0)
            return -1;
        return ::_filelengthi64(m_fd);
    }
    // create time, seconds elapsed since 1970-1-1 00:00:00, UTC, return -1 if failed
    int64_t create_time() const
    {
        struct _stat64 st;
        if (m_fd >= 0 && ::_fstat64(m_fd, &st) == 0)
            return st.st_ctime;
        return -1;
    }
    // modify time, seconds elapsed since 1970-1-1 00:00:00, UTC, return -1 if failed
    int64_t modify_time() const
    {
        struct _stat64 st;
        if (m_fd >= 0 && ::_fstat64(m_fd, &st) == 0)
            return st.st_mtime;
        return -1;
    }
    // get file state, return 0 if succeeded, return errno if failed
    int file_stat(FileStat* pst) const
    {
        if (::_fstat64(m_fd, pst) == 0)
            return 0;
        return errno;
    }
#else
    int64_t file_size() const
    {
        struct stat st;
        if (m_fd >= 0 && ::fstat(m_fd, &st) == 0)
            return st.st_size;
        return -1;
    }
    int64_t create_time() const
    {
        struct stat st;
        if (m_fd >= 0 && ::fstat(m_fd, &st) == 0)
            return st.st_ctime;
        return -1;
    }
    int64_t modify_time() const
    {
        struct stat st;
        if (m_fd >= 0 && ::fstat(m_fd, &st) == 0)
            return st.st_mtime;
        return -1;
    }
    int file_stat(FileStat* pst) const
    {
        if (::fstat(m_fd, pst) == 0)
            return 0;
        return errno;
    }
#endif
private:
    int m_fd;
};

} // namespace irk

//======================================================================================================================
// common functions

// open/create file
inline irk::CFile irk_fopen(const char* filename, const char* mode)
{
    return irk::CFile(filename, mode);
}
inline irk::PosixFile irk_fopen(const char* filename, int oflag, int pmode = FMODE_DFT)
{
    return irk::PosixFile(filename, oflag, pmode);
}
#ifdef _WIN32
inline irk::CFile irk_fopen(const wchar_t* filename, const wchar_t* mode)
{
    return irk::CFile(filename, mode);
}
inline irk::PosixFile irk_fopen(const wchar_t* filename, int oflag, int pmode = FMODE_DFT)
{
    return irk::PosixFile(filename, oflag, pmode);
}
#endif

// check whether the file exists
inline bool irk_fexist(const char* filename)
{
#ifdef _WIN32
    return ::_access(filename, 0) == 0;
#else
    return ::access(filename, F_OK) == 0;
#endif
}
#ifdef _WIN32
inline bool irk_fexist(const wchar_t* filename)
{
    return ::_waccess(filename, 0) == 0;
}
#endif

// get file stat, return 0 if succeeded, return errno if failed
inline int irk_fstat(FILE* fp, irk::FileStat* pst)
{
#ifdef _WIN32
    if (::_fstat64(_fileno(fp), pst) == 0)
        return 0;
#else
    if (::fstat(fileno(fp), pst) == 0)
        return 0;
#endif
    return errno;
}
inline int irk_fstat(int fd, irk::FileStat* pst)
{
#ifdef _WIN32
    if (::_fstat64(fd, pst) == 0)
        return 0;
#else
    if (::fstat(fd, pst) == 0)
        return 0;
#endif
    return errno;
}
inline int irk_fstat(const char* filename, irk::FileStat* pst)
{
#ifdef _WIN32
    if (::_stat64(filename, pst) == 0)
        return 0;
#else
    if (::stat(filename, pst) == 0)
        return 0;
#endif
    return errno;
}
#ifdef _WIN32
inline int irk_fstat(const wchar_t* filename, irk::FileStat* pst)
{
    if (::_wstat64(filename, pst) == 0)
        return 0;
    return errno;
}
#endif

// get file size in bytes, return -1 if failed
inline int64_t irk_fsize(FILE* fp)
{
#ifdef _WIN32
    return ::_filelengthi64(_fileno(fp));
#else
    struct stat st;
    if (::fstat(fileno(fp), &st) == 0)
        return st.st_size;
    return -1;
#endif
}
inline int64_t irk_fsize(int fd)
{
#ifdef _WIN32
    return ::_filelengthi64(fd);
#else
    struct stat st;
    if (::fstat(fd, &st) == 0)
        return st.st_size;
    return -1;
#endif
}
inline int64_t irk_fsize(const char* filename)
{
#ifdef _WIN32
    struct _stat64 st;
    if (::_stat64(filename, &st) == 0)
        return st.st_size;
#else
    struct stat st;
    if (::stat(filename, &st) == 0)
        return st.st_size;
#endif
    return -1;
}
#ifdef _WIN32
inline int64_t irk_fsize(const wchar_t* filename)
{
    struct _stat64 st;
    if (::_wstat64(filename, &st) == 0)
        return st.st_size;
    return -1;
}
#endif

// get file create time(seconds elapsed since 1970-1-1 00:00:00, UTC), return -1 if failed
inline int64_t irk_fctime(FILE* fp)
{
#ifdef _WIN32
    struct _stat64 st;
    if (::_fstat64(_fileno(fp), &st) == 0)
        return st.st_ctime;
#else
    struct stat st;
    if (::fstat(fileno(fp), &st) == 0)
        return st.st_ctime;
#endif
    return -1;
}
inline int64_t irk_fctime(int fd)
{
#ifdef _WIN32
    struct _stat64 st;
    if (::_fstat64(fd, &st) == 0)
        return st.st_ctime;
#else
    struct stat st;
    if (::fstat(fd, &st) == 0)
        return st.st_ctime;
#endif
    return -1;
}
inline int64_t irk_fctime(const char* filename)
{
#ifdef _WIN32
    struct _stat64 st;
    if (::_stat64(filename, &st) == 0)
        return st.st_ctime;
#else
    struct stat st;
    if (::stat(filename, &st) == 0)
        return st.st_ctime;
#endif
    return -1;
}
#ifdef _WIN32
inline int64_t irk_fctime(const wchar_t* filename)
{
    struct _stat64 st;
    if (::_wstat64(filename, &st) == 0)
        return st.st_ctime;
    return -1;
}
#endif

// get file modify time(seconds elapsed since 1970-1-1 00:00:00, UTC), return -1 if failed
inline int64_t irk_fmtime(FILE* fp)
{
#ifdef _WIN32
    struct _stat64 st;
    if (::_fstat64(_fileno(fp), &st) == 0)
        return st.st_mtime;
#else
    struct stat st;
    if (::fstat(fileno(fp), &st) == 0)
        return st.st_mtime;
#endif
    return -1;
}
inline int64_t irk_fmtime(int fd)
{
#ifdef _WIN32
    struct _stat64 st;
    if (::_fstat64(fd, &st) == 0)
        return st.st_mtime;
#else
    struct stat st;
    if (::fstat(fd, &st) == 0)
        return st.st_mtime;
#endif
    return -1;
}
inline int64_t irk_fmtime(const char* filename)
{
#ifdef _WIN32
    struct _stat64 st;
    if (::_stat64(filename, &st) == 0)
        return st.st_mtime;
#else
    struct stat st;
    if (::stat(filename, &st) == 0)
        return st.st_mtime;
#endif
    return -1;
}
#ifdef _WIN32
inline int64_t irk_fmtime(const wchar_t* filename)
{
    struct _stat64 st;
    if (::_wstat64(filename, &st) == 0)
        return st.st_mtime;
    return -1;
}
#endif

// set the position of the file pointer
inline bool irk_fseek(FILE* fp, int64_t offset, int whence = SEEK_SET)
{
#ifdef _WIN32
    return ::_fseeki64(fp, offset, whence) == 0;
#else
    return ::fseeko(fp, offset, whence) == 0;
#endif
}
inline bool irk_fseek(int fd, int64_t offset, int whence = SEEK_SET)
{
#ifdef _WIN32
    return ::_lseeki64(fd, offset, whence) != -1;
#else
    return ::lseek(fd, offset, whence) != -1;
#endif
}

// get current position of the file pointer
inline int64_t irk_ftell(FILE* fp)
{
#ifdef _WIN32
    return ::_ftelli64(fp);
#else
    return ::ftello(fp);
#endif
}
inline int64_t irk_ftell(int fd)
{
#ifdef _WIN32
    return ::_telli64(fd);
#else
    return ::lseek(fd, 0, SEEK_CUR);
#endif
}

// default permission mode when create new directory
#ifdef _WIN32
#define DIRMODE_DFT (_S_IREAD | _S_IWRITE)
#else
#define DIRMODE_DFT (S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)
#endif

// create directory
inline bool irk_mkdir(const char* dirPath, int mode = DIRMODE_DFT)
{
#ifdef _WIN32
    (void)mode;
    return ::_mkdir(dirPath) == 0;
#else
    return ::mkdir(dirPath, mode) == 0;
#endif
}
#ifdef _WIN32
inline bool irk_mkdir(const wchar_t* dirPath)
{
    return ::_wmkdir(dirPath) == 0;
}
#endif

// remove directory
inline bool irk_rmdir(const char* dirPath)
{
#ifdef _WIN32
    return ::_rmdir(dirPath) == 0;
#else
    return ::rmdir(dirPath) == 0;
#endif
}
#ifdef _WIN32
inline bool irk_rmdir(const wchar_t* dirPath)
{
    return ::_wrmdir(dirPath) == 0;
}
#endif

// remove file
inline bool irk_unlink(const char* filePath)
{
#ifdef _WIN32
    return ::_unlink(filePath) == 0;
#else
    return ::unlink(filePath) == 0;
#endif
}
#ifdef _WIN32
inline bool irk_unlink(const wchar_t* filePath)
{
    return ::_wunlink(filePath) == 0;
}
#endif

// rename/move file
#ifdef _WIN32
inline bool irk_rename(const char* oldPath, const char* newPath, bool failIfExists = false)
{
    if (!failIfExists && ::_access(newPath, 0) == 0)
    {
        if (::_unlink(newPath) != 0)
            return false;
    }
    return ::rename(oldPath, newPath) == 0;
}
inline bool irk_rename(const wchar_t* oldPath, const wchar_t* newPath, bool failIfExists = false)
{
    if (!failIfExists && ::_waccess(newPath, 0) == 0)
    {
        if (::_wunlink(newPath) != 0)
            return false;
    }
    return ::_wrename(oldPath, newPath) == 0;
}
#else
inline bool irk_rename(const char* oldPath, const char* newPath, bool failIfExists = false)
{
    if (failIfExists && ::access(newPath, F_OK) == 0)  // destination already exists
    {
        return false;
    }
    return ::rename(oldPath, newPath) == 0;
}
#endif

// copy file
inline bool irk_fcopy(const char* srcPath, const char* dstPath, bool failIfExists = false)
{
#ifdef _WIN32
    return ::CopyFileA(srcPath, dstPath, failIfExists) != 0;
#elif defined(__linux__)
    irk::PosixFile src(srcPath, O_RDONLY);
    if (!src)
        return false;
    int oflags = failIfExists ? (O_WRONLY | O_CREAT | O_EXCL) : (O_WRONLY | O_CREAT | O_TRUNC);
    irk::PosixFile dst(dstPath, oflags);
    if (!dst)
        return false;
    return ::sendfile(dst, src, NULL, src.file_size()) >= 0;
#elif defined(__MACH__)
    copyfile_flags_t flags = failIfExists ? (COPYFILE_ALL | COPYFILE_EXCL) : COPYFILE_ALL;
    return ::copyfile(srcPath, dstPath, 0, flags) == 0;
#else
#error unsupported OS
#endif
}
#ifdef _WIN32
inline bool irk_fcopy(const wchar_t* srcPath, const wchar_t* dstPath, bool failIfExists = false)
{
    return ::CopyFileW(srcPath, dstPath, failIfExists) != 0;
}
#endif

#endif
