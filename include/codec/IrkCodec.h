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

#ifndef _IRONBRICK_CODEC_H_
#define _IRONBRICK_CODEC_H_

#include "IrkCommon.h"

// memory block used by custom memory allocator for IronBrick codecs
struct IrkMemBlock
{
    void*       buf;            // memory buffer
    size_t      size;           // memory buffer size in bytes
    intptr_t    usercnt;        // opaque user data 1
    void*       userdata;       // opaque user data 2
};

// prototype of codec's custom memory allocator callback
// "cbparam": callback parameter set by user
typedef IrkMemBlock (*PFN_CodecAlloc)( size_t size, size_t alignment, void* cbparam );

// prototype of codec's custom memory deallocator callback
// "cbparam": callback parameter set by user
typedef void (*PFN_CodecDealloc)( IrkMemBlock* memblock, void* cbparam );

// encoded/compressed bitstream picture
struct IrkCodedPic
{
    uint8_t*    data;       // compressed bitstream
    size_t      size;       // compressed data size in bytes
    int         pic_type;   // picture type I/P/B
    int64_t     userpts;    // opaque user data 1, copy to/from decoded/raw picture
    void*       userdata;   // opaque user data 2, copy to/from decoded/raw picture
    IrkMemBlock mblock;
};

// decoded/raw picture
struct IrkDecedPic
{
    uint8_t*    plane[4];   // Y, Cb, Cr
    int         width[4];   // width in pixels
    int         height[4];  // height in pixels
    int         pitch[4];   // line size in bytes
    int64_t     userpts;    // opaque user data 1, copy to/from coded picture
    void*       userdata;   // opaque user data 2, copy to/from coded picture
    IrkMemBlock mblock;
};

// prototype of codec's event notify callback
// "cbparam": callback parameter set by user
typedef void (*PFN_CodecNotify)( int code, void* data, void* cbparam );

// codec's notify code: encode/decode one picture succeeded
#define IRK_CODEC_DONE      0
// codec's notify code: encode/decode one picture failed
#define IRK_CODEC_FAILED    -1

#endif
