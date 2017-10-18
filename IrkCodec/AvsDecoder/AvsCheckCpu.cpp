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

#ifdef _MSC_VER

#include <intrin.h>
#pragma intrinsic(__cpuidex)
#define cpuidex __cpuidex

#elif defined(__GNUC__) || defined(__clang__)

static void cpuidex( int result[4], int eaxVal, int ecxVal )
{
    int a, b, c, d;
#if defined(__i386__) && defined(__PIC__)
    __asm__("xchgl\t%%ebx, %k1\n\t"	
            "cpuid\n\t"	
            "xchgl\t%%ebx, %k1\n\t"
            : "=a"(a), "=&r"(b), "=c"(c), "=d"(d)
            : "0"(eaxVal), "2"(ecxVal) );
#elif defined(__x86_64__) && defined(__PIC__)
    __asm__("xchgq\t%%rbx, %q1\n\t"
            "cpuid\n\t"
            "xchgq\t%%rbx, %q1\n\t"
            : "=a"(a), "=&r"(b), "=c"(c), "=d"(d)
            : "0"(eaxVal), "2"(ecxVal) );
#else
    __asm__( "cpuid" : "=a"(a),"=b"(b),"=c"(c),"=d"(d) : "0"(eaxVal), "2"(ecxVal) );
#endif
    result[0] = a;
    result[1] = b;
    result[2] = c;
    result[3] = d;
}

#else
#error unsupport compiler
#endif

// SSE1 = 100
// SSE2 = 200
// SSE3 = 300, SSSE3 = 301
// SSE4.1 = 401, SSE4.2 = 402
extern "C" int get_sse_version()
{
    int abcd[4] = {0};
    cpuidex( abcd, 1, 0 );
    
    if( abcd[2] & 0x100000 )        // SSE4.2
        return 402;
    else if( abcd[2] & 0x80000 )    // SSE4.1
        return 401;
    else if( abcd[2] & 0x200 )      // SSSE3
        return 301;
    else if( abcd[2] & 0x1 )        // SSE3
        return 300;
    else if( abcd[3] & 0x4000000 )  // SSE2
        return 200;
    else if( abcd[3] & 0x2000000 )  // SSE
        return 100;

    return 0;
}
