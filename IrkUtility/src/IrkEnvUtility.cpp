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
#include <pwd.h>
#endif
#ifdef __MACH__
#include <libproc.h>
#endif
#include "IrkMemUtility.h"
#include "IrkStringUtility.h"
#include "IrkEnvUtility.h"

#ifdef _WIN32
#include <shlobj.h>
#pragma comment(lib, "shell32.lib")
#endif

#ifdef _WIN32
static const char CSLASH = '\\';
#else
static const char CSLASH = '/';
#endif

namespace irk {

// get current process's full path, return empty string if failed
std::string get_proc_path()
{
    char buff[1024] = {};
#ifdef _WIN32               // Windows
    if (::GetModuleFileNameA(NULL, buff, 1024) > 0)
    {
        return std::string(buff);
    }
#elif defined(__linux__)    // Linux
    if (::readlink("/proc/self/exe", buff, 1023) > 0)
    {
        return std::string(buff);
    }
#elif defined(__MACH__)     // Mac OSX
    if (::proc_pidpath(::getpid(), buff, 1024) > 0)
    {
        return std::string(buff);
    }
#else
#error unsupported OS
#endif
    return std::string();
}

// ditto, but in UTF-8 encoding
std::string get_proc_path_u8()
{
#ifdef _WIN32                   // Windows
    wchar_t buff[512] = {};
    DWORD cnt = ::GetModuleFileNameW(NULL, buff, 512);
    if (cnt > 0)
    {
        // '\' -> '/'
        for (DWORD i = 0; i < cnt; i++)
        {
            if (buff[i] == L'\\')
                buff[i] = L'/';
        }
        // UTF-16 -> UTF-8
        std::string exePath;
        if (str_utf16to8(buff, exePath) >= 0)
            return exePath;
    }
#elif defined(__linux__)    // Linux
    char buff[1024] = {};
    if (::readlink("/proc/self/exe", buff, 1023) > 0)
    {
        return std::string(buff);
    }
#elif defined(__MACH__)     // Mac OSX
    char buff[1024] = {};
    if (::proc_pidpath(::getpid(), buff, 1024) > 0)
    {
        return std::string(buff);
    }
#else
#error unsupported OS
#endif
    return std::string();
}

// ditto, but in UTF-16 encoding
std::u16string get_proc_path_u16()
{
#ifdef _WIN32                   // Windows
    wchar_t buff[512] = {};
    DWORD cnt = ::GetModuleFileNameW(NULL, buff, 512);
    if (cnt > 0)
    {
        // '\' -> '/'
        for (DWORD i = 0; i < cnt; i++)
        {
            if (buff[i] == L'\\')
                buff[i] = L'/';
        }
        return std::u16string((char16_t*)buff);
    }
#elif defined(__linux__)    // Linux
    char buff[1024] = {};
    if (::readlink("/proc/self/exe", buff, 1023) > 0)
    {
        std::u16string exePath;
        if (str_utf8to16(buff, exePath) >= 0)    // UTF-8 -> UTF-16
            return exePath;
    }
#elif defined(__MACH__)     // Mac OSX
    char buff[1024] = {};
    if (::proc_pidpath(::getpid(), buff, 1024) > 0)
    {
        std::u16string exePath;
        if (str_utf8to16(buff, exePath) >= 0)    // UTF-8 -> UTF-16
            return exePath;
    }
#else
#error unsupported OS
#endif
    return std::u16string();
}

// get directory current process resided in, return empty string if failed
std::string get_proc_dir()
{
    std::string exePath = get_proc_path();
    if (!exePath.empty())
    {
        auto pos = exePath.rfind(CSLASH);
        if (pos != std::string::npos)
            exePath.erase(pos);
    }
    return exePath;
}

// ditto, result is in UTF-8 encoding, dir separator is '/'
std::string get_proc_dir_u8()
{
    std::string exePath = get_proc_path_u8();
    if (!exePath.empty())
    {
        auto pos = exePath.rfind('/');
        if (pos != std::string::npos)
            exePath.erase(pos);
    }
    return exePath;
}

// ditto, result is in UTF-16 encoding, dir separator is '/'
std::u16string get_proc_dir_u16()
{
    std::u16string exePath = get_proc_path_u16();
    if (!exePath.empty())
    {
        auto pos = exePath.rfind(u'/');
        if (pos != std::u16string::npos)
            exePath.erase(pos);
    }
    return exePath;
}

// get current process's name, return empty string if failed
std::string get_proc_name()
{
#ifdef __MACH__
    char buff[256] = {};
    if (::proc_name(::getpid(), buff, 255) > 0)
        return buff;
#else
    std::string exePath = get_proc_path();
    if (!exePath.empty())
    {
        auto pos = exePath.rfind(CSLASH);
        return exePath.substr(pos + 1);
    }
#endif
    return std::string();
}

// ditto, result is in UTF-8 encoding
std::string get_proc_name_u8()
{
#ifdef __MACH__
    char buff[256] = {};
    if (::proc_name(::getpid(), buff, 255) > 0)
        return buff;
#else
    std::string exePath = get_proc_path_u8();
    if (!exePath.empty())
    {
        auto pos = exePath.rfind('/');
        if (pos != std::string::npos)
            return exePath.substr(pos + 1);
    }
#endif
    return std::string();
}

// ditto, result is in UTF-16 encoding
std::u16string get_proc_name_u16()
{
#ifdef __MACH__
    char buff[256] = {};
    if (::proc_name(::getpid(), buff, 255) > 0)
    {
        std::u16string exename;
        if (str_utf8to16(buff, exename) >= 0)    // UTF-8 -> UTF-16
            return exename;
    }
#else
    std::u16string exePath = get_proc_path_u16();
    if (!exePath.empty())
    {
        auto pos = exePath.rfind(u'/');
        if (pos != std::u16string::npos)
            return exePath.substr(pos + 1);
    }
#endif
    return std::u16string();
}

// get current user's home directory, return empty string if failed
std::string get_home_dir()
{
#ifdef _WIN32
    char buff[512];
    if (SUCCEEDED(::SHGetFolderPathA(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, buff)))
        return std::string(buff);
#else
    const passwd* pwEnt = ::getpwuid(::getuid());
    if (pwEnt)
        return std::string(pwEnt->pw_dir);
#endif
    return std::string();
}

// ditto, but in UTF-8 encoding, dir separator is '/'
std::string get_home_dir_u8()
{
#ifdef _WIN32
    wchar_t buff[512];
    if (SUCCEEDED(::SHGetFolderPathW(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, buff)))
    {
        // '\' -> '/'
        for (wchar_t* pch = buff; *pch != 0; pch++)
        {
            if (*pch == L'\\')
                *pch = L'/';
        }
        // UTF-16 -> UTF-8
        std::string homeDir;
        if (str_utf16to8(buff, homeDir) >= 0)
            return homeDir;
    }
#else
    const passwd* pwEnt = ::getpwuid(::getuid());
    if (pwEnt)
        return std::string(pwEnt->pw_dir);
#endif
    return std::string();
}

// ditto, but in UTF-16 encoding, dir separator is '/'
std::u16string get_home_dir_u16()
{
#ifdef _WIN32
    wchar_t buff[512];
    if (SUCCEEDED(::SHGetFolderPathW(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, buff)))
    {
        // '\' -> '/'
        for (wchar_t* pch = buff; *pch != 0; pch++)
        {
            if (*pch == L'\\')
                *pch = L'/';
        }
        return std::u16string((char16_t*)buff);
    }
#else
    const passwd* pwEnt = ::getpwuid(::getuid());
    if (pwEnt)
    {
        // UTF-8 -> UTF-16
        std::u16string homeDir;
        if (str_utf8to16(pwEnt->pw_dir, homeDir) >= 0)
            return homeDir;
    }
#endif
    return std::u16string();
}

// get current working directory, return empty string if failed
std::string get_curr_dir()
{
#ifdef _WIN32
    char buff[512];
    DWORD len = ::GetCurrentDirectoryA(512, buff);
    if (len > 0)
    {
        if (len < 512) // Done!
        {
            return std::string(buff);
        }
        else    // need larger buffer
        {
            MallocedBuf mbuf;
            char* buf2 = (char*)mbuf.alloc(len + 1);
            DWORD len2 = ::GetCurrentDirectoryA(len + 1, buf2);
            if (len2 > 0 && len2 <= len)
                return std::string(buf2);
        }
    }
#else
    char buff[1024];
    if (::getcwd(buff, 1024) != nullptr)
        return std::string(buff);
#endif
    return std::string();
}

// ditto, in UTF-8 encoding, dir separator is '/'
std::string get_curr_dir_u8()
{
#ifdef _WIN32
    wchar_t buff[512];
    DWORD len = ::GetCurrentDirectoryW(512, buff);
    if (len > 0)
    {
        std::string curDir;

        if (len < 512) // Done!
        {
            if (str_utf16to8(buff, curDir) < 0)  // UTF-16 -> UTF-8
                curDir.clear();
        }
        else    // need larger buffer
        {
            MallocedBuf mbuf;
            wchar_t* buf2 = (wchar_t*)mbuf.alloc((len + 1) * sizeof(wchar_t));
            DWORD len2 = ::GetCurrentDirectoryW(len + 1, buf2);
            if (len2 > 0 && len2 <= len)
            {
                if (str_utf16to8(buf2, curDir) < 0)  // UTF-16 -> UTF-8
                    curDir.clear();
            }
        }
        if (!curDir.empty())
        {
            for (char& ch : curDir)    // '\' -> '/'
            {
                if (ch == '\\')
                    ch = '/';
            }
            return curDir;
        }
    }
#else
    char buff[1024];
    if (::getcwd(buff, 1024) != nullptr)
        return std::string(buff);
#endif
    return std::string();
}

// ditto, in UTF-16 encoding, dir separator is '/'
std::u16string get_curr_dir_u16()
{
#ifdef _WIN32
    wchar_t buff[512];
    DWORD len = ::GetCurrentDirectoryW(512, buff);
    if (len > 0)
    {
        std::u16string curDir;

        if (len < 512) // Done!
        {
            curDir.assign((char16_t*)buff);
        }
        else    // need larger buffer
        {
            MallocedBuf mbuf;
            wchar_t* buf2 = (wchar_t*)mbuf.alloc((len + 1) * sizeof(wchar_t));
            DWORD len2 = ::GetCurrentDirectoryW(len + 1, buf2);
            if (len2 > 0 && len2 <= len)
                curDir.assign((char16_t*)buf2);
        }
        if (!curDir.empty())
        {
            for (char16_t& ch : curDir)    // '\' -> '/'
            {
                if (ch == u'\\')
                    ch = u'/';
            }
            return curDir;
        }
    }
#else
    char buff[1024];
    if (::getcwd(buff, 1024) != nullptr)
    {
        std::u16string curDir;
        if (str_utf8to16(buff, curDir) >= 0)
            return curDir;
    }
#endif
    return std::u16string();
}

// wchar_t version on Windows, dir separator is L'\'
#ifdef _WIN32

std::wstring get_proc_path_ws()
{
    wchar_t buff[512] = {};
    DWORD cnt = ::GetModuleFileNameW(NULL, buff, 512);
    if (cnt > 0)
        return std::wstring(buff);
    return std::wstring();
}

std::wstring get_proc_dir_ws()
{
    std::wstring exePath = get_proc_path_ws();
    if (!exePath.empty())
    {
        auto pos = exePath.rfind(L'\\');
        if (pos != std::wstring::npos)
            exePath.erase(pos);
    }
    return exePath;
}

std::wstring get_home_dir_ws()
{
    wchar_t buff[512];
    if (SUCCEEDED(::SHGetFolderPathW(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, buff)))
        return std::wstring(buff);
    return std::wstring();
}

std::wstring get_curr_dir_ws()
{
    wchar_t buff[512];
    DWORD len = ::GetCurrentDirectoryW(512, buff);
    if (len > 0)
    {
        if (len < 512) // Done!
        {
            return std::wstring(buff);
        }
        else    // need larger buffer
        {
            MallocedBuf mbuf;
            wchar_t* buf2 = (wchar_t*)mbuf.alloc((len + 1) * sizeof(wchar_t));
            DWORD len2 = ::GetCurrentDirectoryW(len + 1, buf2);
            if (len2 > 0 && len2 <= len)
                return std::wstring(buf2);
        }
    }
    return std::wstring();
}

#endif  // _WIN32

//======================================================================================================================
// environment variable

// get environment variable, return empty string if failed
std::string get_envar(const char* name)
{
    assert(name);
#ifdef _WIN32
    char buff[512];
    DWORD len = ::GetEnvironmentVariableA(name, buff, 512);
    if (len > 0)
    {
        if (len < 512) // Done!
        {
            return std::string(buff);
        }
        else  // insufficient buffer
        {
            MallocedBuf mbuf;
            char* buf2 = (char*)mbuf.alloc(len + 1);
            DWORD len2 = ::GetEnvironmentVariableA(name, buf2, len + 1);
            if (len2 > 0 && len2 <= len)
                return std::string(buf2);
        }
    }
#else
    const char* szval = ::getenv(name);
    if (szval)
        return std::string(szval);
#endif
    return std::string();
}

std::u16string get_envar(const char16_t* name)
{
    assert(name);
#ifdef _WIN32
    wchar_t buff[512];
    DWORD len = ::GetEnvironmentVariableW((const wchar_t*)name, buff, 512);
    if (len > 0)
    {
        if (len < 512) // Done!
        {
            return std::u16string((char16_t*)buff);
        }
        else  // insufficient buffer
        {
            MallocedBuf mbuf;
            wchar_t* buf2 = (wchar_t*)mbuf.alloc((len + 1) * sizeof(wchar_t));
            DWORD len2 = ::GetEnvironmentVariableW((const wchar_t*)name, buf2, len + 1);
            if (len2 > 0 && len2 <= len)
                return std::u16string((char16_t*)buf2);
        }
    }
#else
    std::string u8name;
    if (str_utf16to8(name, u8name) > 0)
    {
        const char* szval = ::getenv(u8name.c_str());
        if (szval)
        {
            std::u16string envVal;
            if (str_utf8to16(szval, envVal) >= 0)
                return envVal;
        }
    }
#endif
    return std::u16string();
}

// modify/insert environment variable, return false if failed
// if value == NULL, delete the environment variable
bool set_envar(const char* name, const char* value)
{
    assert(name);
#ifdef _WIN32
    return ::SetEnvironmentVariableA(name, value) == TRUE;
#else
    if (!value)    // delete env variable
        return ::unsetenv(name) == 0;
    return ::setenv(name, value, 1) == 0;
#endif
}

bool set_envar(const char16_t* name, const char16_t* value)
{
    assert(name);
#ifdef _WIN32
    return ::SetEnvironmentVariableW((const wchar_t*)name, (const wchar_t*)value) == TRUE;
#else
    std::string u8name;
    if (str_utf16to8(name, u8name) < 0)
        return false;
    if (!value)    // delete env variable
        return ::unsetenv(u8name.c_str());

    std::string u8value;
    if (str_utf16to8(value, u8value) < 0)
        return false;
    return ::setenv(u8name.c_str(), u8value.c_str(), 1) == 0;
#endif
}

// remove environment variable, return false if failed
bool remove_envar(const char* name)
{
    assert(name);
#ifdef _WIN32
    return ::SetEnvironmentVariableA(name, NULL) == TRUE;
#else
    return ::unsetenv(name) == 0;
#endif
}

bool remove_envar(const char16_t* name)
{
    assert(name);
#ifdef _WIN32
    return ::SetEnvironmentVariableW((const wchar_t*)name, NULL) == TRUE;
#else
    std::string u8name;
    if (str_utf16to8(name, u8name) < 0)
        return false;
    return ::unsetenv(u8name.c_str()) == 0;
#endif
}

#ifdef _WIN32

// get environment variable, return empty string if failed
std::wstring get_envar(const wchar_t* name)
{
    assert(name);
    wchar_t buff[512];
    DWORD len = ::GetEnvironmentVariableW(name, buff, 512);
    if (len > 0)
    {
        if (len < 512) // Done!
        {
            return std::wstring(buff);
        }
        else  // insufficient buffer
        {
            MallocedBuf mbuf;
            wchar_t* buf2 = (wchar_t*)mbuf.alloc((len + 1) * sizeof(wchar_t));
            DWORD len2 = ::GetEnvironmentVariableW(name, buf2, len + 1);
            if (len2 > 0 && len2 <= len)
                return std::wstring(buf2);
        }
    }
    return std::wstring();
}

// modify/insert environment variable
// if value == NULL, delete the environment variable
bool set_envar(const wchar_t* name, const wchar_t* value)
{
    assert(name);
    return ::SetEnvironmentVariableW(name, value) == TRUE;
}

// remove environment variable
bool remove_envar(const wchar_t* name)
{
    assert(name);
    return ::SetEnvironmentVariableW(name, NULL) == TRUE;
}

#endif  // _WIN32

}   // namespace irk
