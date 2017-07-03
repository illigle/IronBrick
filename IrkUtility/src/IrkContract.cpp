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

#include <stdio.h>
#include <string.h>
#include <atomic>
#include "IrkContract.h"

#if defined(_MSC_VER) && _MSC_VER < 1900
#define snprintf _snprintf
#endif

namespace irk {

static void default_violation_handler( const CViolationInfo& info )
{
    // get source file name
#ifdef _WIN32
    const char* filename = strrchr( info.file, '\\' );
#else
    const char* filename = strrchr( info.file, '/' );
#endif
    filename = filename ? (filename + 1) : info.file;

    // format error message
    char msgbuf[512] = {};
    snprintf( msgbuf, 511, "contract violation: %s, %u@%s", info.expression, info.line, filename );
    throw ContractViolation( msgbuf );
}

static std::atomic<PFN_CViolationHandler> s_CvHandler( &default_violation_handler );

PFN_CViolationHandler set_violation_handler( PFN_CViolationHandler handler )
{
    return s_CvHandler.exchange( handler );
}

PFN_CViolationHandler get_violation_handler()
{
    return s_CvHandler.load();
}

void handle_violation( const char* express, const char* filename, unsigned lineno )
{
    CViolationInfo info = { express, filename, lineno };
    auto handler = s_CvHandler.load();
    if( handler )
    {
        (*handler)( info );
    }
}

} // namespace irk
