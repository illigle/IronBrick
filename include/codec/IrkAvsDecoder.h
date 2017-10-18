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

/* AVS+ decoder, according to GY/T 257.1—2012 */

#ifndef _IRONBRICK_AVS_DECODER_H_
#define _IRONBRICK_AVS_DECODER_H_

#include "IrkCodec.h"

// profile
#define AVS_PROFILE_JIZHUN              0x20
#define AVS_PROFILE_BROADCAST           0x48    // avs+ broadcast profile

// chroma format
#define AVS_CHROMA_420                  0x1
#define AVS_CHROMA_422                  0x2

// picture coding type
#define AVS_PICTURE_TYPE_I              1
#define AVS_PICTURE_TYPE_P              2
#define AVS_PICTURE_TYPE_B              3

// opaque AVS+ decoder
struct IrkAvsDecoder;

// AVS+ decoder configuration, for defaults set the whole struct to 0
struct IrkAvsDecConfig
{
    // 0: output decoded picuture in display order
    // 1: output decoded picuture in encoded order
    // NOTE: if low_delay is set in bitstream, always output in encoded order
    int     output_order;

    // thread_cnt == 0: using multi-thread decoding, thread count = CPU cores
    // thread_cnt == 1: using single-thread decoding
    // thread_cnt > 1 && thread_cnt <= CPU cores: using the specified number of threads
    int     thread_cnt;

    // 1: disable loop filter
    // 0: decided by the bitstream
    int     disable_lf;

    // NOTE: only decoded YUV data will be allocated by custom allocator
    PFN_CodecAlloc      alloc_callback;         // custom memory allocator
    void*               alloc_cbparam;          // callback parameter of custom memory allocator
    PFN_CodecDealloc    dealloc_callback;       // custom memory deallocator
    void*               dealloc_cbparam;        // callback parameter of custom memory deallocator
};

// decoded AVS+ picture
struct IrkAvsDecedPic : IrkDecedPic
{
    uint8_t     pic_type;
    uint8_t     progressive;
    uint8_t     topfield_first;
    uint8_t     repeat_first_field;
};

// AVS+ coded stream basic information
struct IrkAvsStreamInfo
{
    int     profile;     
    int     level;
    int     width;              // video width in pixels
    int     height;             // video height in pixels
    int     chroma_format;
    int     frame_rate_num;     // numerator of frame rate
    int     frame_rate_den;     // denominator of frame rate
    int     bitrate;            // bits per second
    int     progressive_seq;    // 1: progressive sequence, 0: interlace sequence
};

//======================================================================================================================
#ifdef _MSC_VER
#ifdef IRK_AVSDECLIB_IMPL
#define IRK_AVSDEC_EXPORT __declspec(dllexport)
#else
#define IRK_AVSDEC_EXPORT __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#ifdef IRK_AVSDECLIB_IMPL
#define IRK_AVSDEC_EXPORT __attribute__((visibility("default")))
#else
#define IRK_AVSDEC_EXPORT
#endif
#else
#define IRK_AVSDEC_EXPORT 
#endif

#ifdef __cplusplus
extern "C" {
#endif

// create AVS+ decoder
// return NULL if failed(CPU does not support SSE, create internal threads failed)
IRK_AVSDEC_EXPORT IrkAvsDecoder* irk_create_avs_decoder( const IrkAvsDecConfig* config );

// destroy AVS+ decoder
// all resource allocated by the decoder will be destoryed
IRK_AVSDEC_EXPORT void irk_destroy_avs_decoder( IrkAvsDecoder* decoder );

// reset AVS+ decoder(before decoding new bitstream)
// if "resetAll" == false, global setting such as sequece header will not be reset
IRK_AVSDEC_EXPORT void irk_avs_decoder_reset( IrkAvsDecoder* decoder, bool resetAll );

// set decoding notify callback
// when got IRK_CODEC_DONE code, notify data point to IrkAvsDecedPic struct
IRK_AVSDEC_EXPORT void irk_avs_decoder_set_notify( IrkAvsDecoder* decoder, PFN_CodecNotify callback, void* cbparam );

// set skip mode, if "skip_mode" != 0, skip non-reference pictures
IRK_AVSDEC_EXPORT void irk_avs_decoder_set_skip( IrkAvsDecoder* decoder, int skip_mode );

#define IRK_AVS_DEC_BAD_STREAM  -1
#define IRK_AVS_DEC_UNAVAILABLE -2
#define IRK_AVS_DEC_UNSUPPORTED -3

// decode one AVS+ picture
// if succeeded return data size consumed, 
// if failed return negtive error code(see above)
// NOTE 1: decoded picture will be send to user by the notify callback,
//          notify callback will always be called in the thread calling this function
// NOTE 2: input NULL will flush cached pictures
IRK_AVSDEC_EXPORT int irk_avs_decoder_decode( IrkAvsDecoder* decoder, const IrkCodedPic* encPic );

// get AVS+ stream basic infomation
// if succeeded return 0, if failed return negtive error code(see above)
IRK_AVSDEC_EXPORT int irk_avs_decoder_get_info( IrkAvsDecoder* decoder, IrkAvsStreamInfo* pinfo );

// normally decoded picture is only valid in notify callback function,
// in case user wants to use decoded picture outside callback function, 
// user can either use custom memory allocator or retain the decoded picture
IRK_AVSDEC_EXPORT void irk_avs_decoder_retain_picture( IrkAvsDecedPic* pic );

// retained picture must be released before decoder destroyed
IRK_AVSDEC_EXPORT void irk_avs_decoder_dismiss_picture( IrkAvsDecedPic* pic );

#ifdef __cplusplus
}
#endif

#endif
