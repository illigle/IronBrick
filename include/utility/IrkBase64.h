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

#ifndef _IRONBRICK_BASE64_H_
#define _IRONBRICK_BASE64_H_

#include <string>
#include "IrkCommon.h"

namespace irk {

// simple base64 encode + decode utility

// Base64 encoding, return encoded character count
// [srcsize]: input data's bytes count
// [dstsize]: output buffer's size, should >= (4 * (srcsize/3 + 1))
// NOTE: output string is NOT null-terminated
extern int encode_base64( const uint8_t* src, int srcsize, char* dst, int dstsize );
extern std::string encode_base64( const uint8_t* src, int srcsize );

// Base64 decoding, return decoded bytes count
// [srcsize]: input base64 string's length, must be multiple of 4
// [dstsize]: output buffer's size, should >= (3 * srcsize / 4)
extern int decode_base64( const char* src, int srcsize, uint8_t* dst, int dstsize );
extern int decode_base64( const std::string& src, uint8_t* dst, int dstsize );

}
#endif
