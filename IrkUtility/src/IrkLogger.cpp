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
#include <io.h>
#include <direct.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif
#include <time.h>
#include <string>
#include "IrkLogger.h"

namespace irk {

// Simple file logger
bool FileLogger::open( const char* filePath )
{
    std::lock_guard<std::mutex> guard( m_Mutex );

    if( m_pFile )           // already opend
        return false;

    // open file
    m_pFile = fopen( filePath, "a" );   // Open in append mode
    if( !m_pFile )
        return false;

    // print current time
    struct tm localtm = {};
    time_t curtm = ::time(nullptr);
#ifdef _WIN32
    ::localtime_s( &localtm, &curtm );
#else
    ::localtime_r( &curtm, &localtm );
#endif
    char tmstr[32] = {};
    strftime( tmstr, 32, "%Y-%m-%d %H:%M:%S", &localtm );
    fprintf( m_pFile, "#######################################\n" );
    fprintf( m_pFile, "# %s\n", tmstr );
    fflush( m_pFile );

    return true;
}

void FileLogger::close()
{
    std::lock_guard<std::mutex> guard( m_Mutex );

    if( m_pFile )
    {
        fclose( m_pFile );
        m_pFile = nullptr;
    }
}

void FileLogger::vlog( int level, const char* fmt, va_list args )
{
    if( !m_pFile )
        return;

    // get current time
    struct tm localtm = {};
    time_t curtm = ::time(nullptr);
#ifdef _WIN32
    ::localtime_s( &localtm, &curtm );
#else
    ::localtime_r( &curtm, &localtm );
#endif
    char tmstr[16] = {};
    strftime( tmstr, 16, "%H:%M:%S", &localtm );

    std::lock_guard<std::mutex> guard( m_Mutex );
    if( !m_pFile )
        return;

    if( level >= IRK_LOG_ERROR )    // error
    {
        fprintf( m_pFile, "[E][%s] ", tmstr );
        vfprintf( m_pFile, fmt, args );
        fflush( m_pFile );
    }
    else if( level == IRK_LOG_WARNING ) // warning
    {
        fprintf( m_pFile, "[W][%s] ", tmstr );
        vfprintf( m_pFile, fmt, args );
        fflush( m_pFile );
    }
    else    // infomation
    {
        fprintf( m_pFile, "[T][%s] ", tmstr );
        vfprintf( m_pFile, fmt, args );
    }
}

#ifdef _WIN32   // Optional wchar_t version on Windows

bool FileLogger::open( const wchar_t* filePath )
{
    std::lock_guard<std::mutex> guard( m_Mutex );
    if( m_pFile )           // already opend
        return false;

    // open file in UTF-8 mode
    m_pFile = _wfopen( filePath, L"at, ccs=UTF-8" );
    if( !m_pFile )
        return false;

    // print current time
    struct tm localtm = {};
    time_t curtm = ::time(nullptr);
    ::localtime_s( &localtm, &curtm );
    wchar_t tmstr[32] = {};
    wcsftime( tmstr, 32, L"%Y-%m-%d %H:%M:%S", &localtm );
    fwprintf( m_pFile, L"#######################################\n" );
    fwprintf( m_pFile, L"# %ls\n", tmstr );
    fflush( m_pFile );

    return true;
}

void FileLogger::vlog( int level, const wchar_t* fmt, va_list args )
{
    if( !m_pFile )
        return;

    // get current time
    struct tm localtm = {};
    time_t curtm = ::time(nullptr);
    ::localtime_s( &localtm, &curtm );
    wchar_t tmstr[16] = {};
    wcsftime( tmstr, 16, L"%H:%M:%S", &localtm );

    std::lock_guard<std::mutex> guard( m_Mutex );
    if( !m_pFile )
        return;

    if( level >= IRK_LOG_ERROR )    // error
    {
        fwprintf( m_pFile, L"[E][%s] ", tmstr );
        vfwprintf( m_pFile, fmt, args );
        fflush( m_pFile );
    }
    else if( level == IRK_LOG_WARNING )     // warning
    {
        fwprintf( m_pFile, L"[W][%s] ", tmstr );
        vfwprintf( m_pFile, fmt, args );
        fflush( m_pFile );
    }
    else
    {
        fwprintf( m_pFile, L"[T][%s] ", tmstr );
        vfwprintf( m_pFile, fmt, args );
    }
}

#endif  // _WIN32

//======================================================================================================================
// Monthly file logger

#if defined(_MSC_VER) && _MSC_VER < 1900
#define snprintf _snprintf
#endif

// open log file, will delete log files two months ago !
bool MonthlyLogger::open( const char* logFolder, const char* filePrefix )
{
    // create log folder if not exists
#ifdef _WIN32
    if( _access( logFolder, 0 ) != 0 )
        _mkdir( logFolder );
#else
    if( access( logFolder, F_OK ) != 0 )
        mkdir( logFolder, 0755 );
#endif

    time_t curTime = ::time(nullptr);
    struct tm localtm = {};
#ifdef _WIN32
    ::localtime_s( &localtm, &curTime );
#else
    ::localtime_r( &curTime, &localtm );
#endif
    const int curMonth = localtm.tm_mon + 1;    // month, 1 - 12

    std::string logFileName = logFolder;
    if( !logFileName.empty() )
    {
#ifdef _WIN32
        if( logFileName.back() != '\\' )
            logFileName += '\\';
#else
        if( logFileName.back() != '/' )
            logFileName += '/';
#endif
    }
    logFileName += filePrefix;
    const size_t suffix = logFileName.length();

    // delete log files created two months ago !
    char monthStr[16] = {};
    int lastMonth2 = curMonth > 2 ? curMonth - 2 : curMonth + 10;
    snprintf( monthStr, 15, "-%02d.log", lastMonth2 );
    logFileName += monthStr;
    ::remove( logFileName.c_str() );

    // get current month's log file name
    snprintf( monthStr, 15, "-%02d.log", curMonth );
    logFileName.replace( suffix, std::string::npos, monthStr );

    return FileLogger::open( logFileName.c_str() );
}

#ifdef _WIN32

bool MonthlyLogger::open( const wchar_t* logFolder, const wchar_t* filePrefix )
{
    // create log folder if not exists
    if( _waccess( logFolder, 0 ) != 0 )
        _wmkdir( logFolder );

    time_t curTime = ::time(nullptr);
    struct tm localtm = {};
    ::localtime_s( &localtm, &curTime );
    const int curMonth = localtm.tm_mon + 1;    // month, 1 - 12

    std::wstring logFileName = logFolder;
    if( !logFileName.empty() )
    {
        if( logFileName.back() != L'\\' )
            logFileName += L'\\';
    }
    logFileName += filePrefix;
    const size_t suffix = logFileName.length();

    // delete log files created two months ago !
    wchar_t monthStr[16] = {};
    int lastMonth2 = curMonth > 2 ? curMonth - 2 : curMonth + 10;
    swprintf( monthStr, 15, L"-%02d.log", lastMonth2 );
    logFileName += monthStr;
    _wremove( logFileName.c_str() );

    // get current month's log file name
    swprintf( monthStr, 15, L"-%02d.log", curMonth );
    logFileName.replace( suffix, std::wstring::npos, monthStr );

    return FileLogger::open( logFileName.c_str() );
}

#endif

//======================================================================================================================
// Colorful stdio logger : log to stdout/stderr in different colors

#ifdef _WIN32

#define TEXT_COLOR_GRAY     7
#define TEXT_COLOR_GREEN    10
#define TEXT_COLOR_RED      12
#define TEXT_COLOR_YELLOW   14

// get console handle and default color, return NULL if failed
static inline HANDLE get_handle_and_color( DWORD stdHdlType, WORD* pColor )
{
    HANDLE hConsole = ::GetStdHandle( stdHdlType );
    if( hConsole != INVALID_HANDLE_VALUE && hConsole != NULL )
    {
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if( ::GetConsoleScreenBufferInfo( hConsole, &csbi ) )
        {
            *pColor = csbi.wAttributes;     // default color
            return hConsole;
        }
    }
    return NULL;
}

void ColorLogger::vlog( int level, const char* fmt, va_list args )
{
    std::lock_guard<std::mutex> guard( m_Mutex );

    if( level >= IRK_LOG_ERROR )    // error, print in red
    {
        WORD dftColor = TEXT_COLOR_GRAY;
        HANDLE hStderr = get_handle_and_color( STD_ERROR_HANDLE, &dftColor );
        if( hStderr )
            ::SetConsoleTextAttribute( hStderr, TEXT_COLOR_RED );

        vfprintf( stderr, fmt, args );

        // reset color
        if( hStderr )
            ::SetConsoleTextAttribute( hStderr, dftColor );
    }
    else if( level == IRK_LOG_WARNING ) // warning, print in yellow
    {
        WORD dftColor = TEXT_COLOR_GRAY;
        HANDLE hStderr = get_handle_and_color( STD_ERROR_HANDLE, &dftColor );
        if( hStderr )
            ::SetConsoleTextAttribute( hStderr, TEXT_COLOR_YELLOW );

        vfprintf( stderr, fmt, args );

        // reset color
        if( hStderr )
            ::SetConsoleTextAttribute( hStderr, dftColor );
    }
    else
    {
        vfprintf( stdout, fmt, args );
    }
}

void ColorLogger::vlog( int level, const wchar_t* fmt, va_list args )
{
    std::lock_guard<std::mutex> guard( m_Mutex );

    if( level >= IRK_LOG_ERROR )    // error, print in red
    {
        WORD dftColor = TEXT_COLOR_GRAY;
        HANDLE hStderr = get_handle_and_color( STD_ERROR_HANDLE, &dftColor );
        if( hStderr )
            ::SetConsoleTextAttribute( hStderr, TEXT_COLOR_RED );

        vfwprintf( stderr, fmt, args );

        // reset color
        if( hStderr )
            ::SetConsoleTextAttribute( hStderr, dftColor );
    }
    else if( level == IRK_LOG_WARNING ) // warning, print in yellow
    {
        WORD dftColor = TEXT_COLOR_GRAY;
        HANDLE hStderr = get_handle_and_color( STD_ERROR_HANDLE, &dftColor );
        if( hStderr )
            ::SetConsoleTextAttribute( hStderr, TEXT_COLOR_YELLOW );

        vfwprintf( stderr, fmt, args );

        // reset color
        if( hStderr )
            ::SetConsoleTextAttribute( hStderr, dftColor );
    }
    else
    {
        vfwprintf( stdout, fmt, args );
    }
}

#else

/* NOTE: color codes are defined in ECMA-48 standard */

void ColorLogger::vlog( int level, const char* fmt, va_list args )
{
    std::lock_guard<std::mutex> guard( m_Mutex );

    if( level >= IRK_LOG_ERROR )    // error, print in red
    {
        // text color: red
        fputs( "\033[31;1m", stderr );

        vfprintf( stderr, fmt, args );

        // reset color
        fputs( "\033[0m", stderr );
    }
    else if( level == IRK_LOG_WARNING ) // warning, print in yellow
    {
        // text color: yellow
        fputs( "\033[33;1m", stderr );

        vfprintf( stderr, fmt, args );

        // reset color
        fputs( "\033[0m", stderr );
    }
    else
    {
        vfprintf( stdout, fmt, args );
    }
}

void ColorLogger::vlog( int level, const wchar_t* fmt, va_list args )
{
    std::lock_guard<std::mutex> guard( m_Mutex );

    if( level >= IRK_LOG_ERROR )    // error, print in red
    {
        // text color: red
        fputws( L"\033[31;1m", stderr );

        vfwprintf( stderr, fmt, args );

        // reset color
        fputws( L"\033[0m", stderr );
    }
    else if( level == IRK_LOG_WARNING ) // waring, print in yellow
    {
        // text color: yellow
        fputws( L"\033[33;1m", stderr );

        vfwprintf( stderr, fmt, args );

        // reset color
        fputws( L"\033[0m", stderr );
    }
    else
    {
        vfwprintf( stdout, fmt, args );
    }
}

#endif

} // namespace irk
