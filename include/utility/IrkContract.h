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

#ifndef _IRONBRICK_CONTRACT_H_
#define _IRONBRICK_CONTRACT_H_

#include <assert.h>
#include <stdexcept>
#include "IrkCommon.h"

namespace irk {

// default contract violation exception
class ContractViolation : public std::runtime_error
{
public:
    explicit ContractViolation( const char* msg ) : std::runtime_error( msg ) {}
};

// contract violation information
struct CViolationInfo
{
    const char* expression;     // the expression eval to false
    const char* file;           // the file where contract violated
    unsigned    line;           // the line where contract violated
};

typedef void (*PFN_CViolationHandler)( const CViolationInfo& );

// set contract violation handler, return previous contract violation handler
PFN_CViolationHandler set_violation_handler( PFN_CViolationHandler handler );

// get current contract violation handler
PFN_CViolationHandler get_violation_handler();

// handle contract violation, the default handler will throw "ContractViolation"
void handle_violation( const char* expression, const char* filename, unsigned lineno );

} // namespace irk


#ifdef IRK_ENABLE_CONTRACTS
#define irk_expect( cond )  (void)( !!(cond) || (irk::handle_violation(#cond, __FILE__, __LINE__), 0) )
#define irk_ensure( cond )  (void)( !!(cond) || (irk::handle_violation(#cond, __FILE__, __LINE__), 0) )
#else
#define irk_expect( cond )  assert( cond )
#define irk_ensure( cond )  assert( cond )
#endif

#endif
