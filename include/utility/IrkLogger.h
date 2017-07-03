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

#ifndef _IRONBRICK_LOGGER_H_
#define _IRONBRICK_LOGGER_H_

#include <stdio.h>
#include <stdarg.h>
#include <mutex>
#include "IrkCommon.h"

// log criticality level: informational message
#define IRK_LOG_INFO        1
// log criticality level: something may be wrong
#define IRK_LOG_WARNING     2
// log criticality level: fatal error
#define IRK_LOG_ERROR       3

namespace irk {

// abstract logger interface
class Logger : IrkNocopy
{
public:
    virtual ~Logger() {}

    // Syntax is the same as vprintf, the first parameter is criticality level, see above
    virtual void vlog( int /*level*/, const char* /*fmt*/, va_list ) = 0;

    // Optional wchar_t version
    virtual void vlog( int /*level*/, const wchar_t* /*fmt*/, va_list ) {}

    // Syntax is the same as printf
    void log( int level, const char* fmt, ... )
    {
        va_list args;
        va_start( args, fmt );
        this->vlog( level, fmt, args );
        va_end( args );
    }

    // Optional wchar_t version
    void log( int level, const wchar_t* fmt, ... )
    {
        va_list args;
        va_start( args, fmt );
        this->vlog( level, fmt, args );
        va_end( args );
    }
};

//======================================================================================================================
// some simple logger

// Dumb logger: log nothing !
class DumbLogger : public Logger
{
public:
    void vlog( int, const char*, va_list ) override {}
};

// Simple stdio logger: log to stdout or stderr
class StdLogger : public Logger
{
public:
    void vlog( int level, const char* fmt, va_list args ) override
    {
        FILE* stdfp = (level >= IRK_LOG_WARNING) ? stderr : stdout;
        vfprintf( stdfp, fmt, args );
    }
    void vlog( int level, const wchar_t* fmt, va_list args ) override
    {
        FILE* stdfp = (level >= IRK_LOG_WARNING) ? stderr : stdout;
        vfwprintf( stdfp, fmt, args );
    }
};

// Colorful stdio logger : log to stdout or stderr in different colors, thead-safe
class ColorLogger : public Logger
{
public:
    // if error, print in red
    // if warning, print in yellow
    // otherwise, print in default color
    void vlog( int level, const char* fmt, va_list ) override;
    void vlog( int level, const wchar_t* fmt, va_list ) override;
private:
    std::mutex m_Mutex;
};

// Simple file logger: append log to local text file, thead-safe
class FileLogger : public Logger
{
public:
    FileLogger() : m_pFile(nullptr) {}
    ~FileLogger() { this->close(); }

    // open log file, return false if open file failed or already opened
    bool open( const char* filePath );

    // close log file
    void close();

    void vlog( int level, const char* fmt, va_list ) override;

    // wchar_t version on Windows, save as UTF-8 file
    // WARNING: do NOT mix use of [char] version's methods !
#ifdef _WIN32
    bool open( const wchar_t* filePath );
    void vlog( int level, const wchar_t* fmt, va_list ) override;
#endif
private:
    FILE*       m_pFile;
    std::mutex  m_Mutex;
};

// Monthly file logger: log to local file, only keep last two month's log file
class MonthlyLogger : public FileLogger
{
public:
    // open log file, will delete log files two months ago
    // @param logFolder   The folder in which log files created
    // @param filePrefix  The whole log file name will be filePrefix-month.log
    bool open( const char* logFolder, const char* filePrefix );
#ifdef _WIN32
    bool open( const wchar_t* logFolder, const wchar_t* filePrefix );
#endif
};

} // namespace irk

//======================================================================================================================
#ifndef IRK_LOG_THRESHOLD
/* if not explicitly set by user, enable all logging under debug mode */
#ifndef NDEBUG
#define IRK_LOG_THRESHOLD   IRK_LOG_INFO
#else
#define IRK_LOG_THRESHOLD   IRK_LOG_ERROR
#endif
#endif

/* Log fatal error to blame someone */
#if IRK_LOG_ERROR >= IRK_LOG_THRESHOLD
#define irk_blame( loger, ... )     (loger)->log( IRK_LOG_ERROR, __VA_ARGS__ )
#else
#define irk_blame( loger, ... )     (void)0
#endif

/* Log warning */
#if IRK_LOG_WARNING >= IRK_LOG_THRESHOLD
#define irk_warn( loger, ... )      (loger)->log( IRK_LOG_WARNING, __VA_ARGS__ )
#else
#define irk_warn( loger, ... )      (void)0
#endif

/* Log infomation */
#if IRK_LOG_INFO >= IRK_LOG_THRESHOLD
#define irk_trace( loger, ... )     (loger)->log( IRK_LOG_INFO, __VA_ARGS__ )
#else
#define irk_trace( loger, ... )     (void)0
#endif

#endif      /* _IRONBRICK_COMMON_H_ */
