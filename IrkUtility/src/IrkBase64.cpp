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

#include "IrkBase64.h"
#include "IrkContract.h"

// Base64 encoding table
static const char s_Base64EncTab[64] =
{
    0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,
    0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f,0x50,
    0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,
    0x59,0x5a,0x61,0x62,0x63,0x64,0x65,0x66,
    0x67,0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,
    0x6f,0x70,0x71,0x72,0x73,0x74,0x75,0x76,
    0x77,0x78,0x79,0x7a,0x30,0x31,0x32,0x33,
    0x34,0x35,0x36,0x37,0x38,0x39,0x2b,0x2f,
};

// Base64 decoding table, 64 means invalid character
static const uint8_t s_Base64DecTab[256] =
{
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,62,64,63,62,63,
    52,53,54,55,56,57,58,59,60,61,63,64,64, 0,64,64,
    64, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
    15,16,17,18,19,20,21,22,23,24,25,64,64,64,64,62,
    64,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
    41,42,43,44,45,46,47,48,49,50,51,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
};

namespace irk {

// Base64 encoding, return encoded character count
// [srcsize]: input data's bytes count
// [dstsize]: output buffer's size, should >= (4 * (srcsize/3 + 1))
// NOTE: output string is NOT null-terminated
int encode_base64(const uint8_t* src, int srcsize, char* dst, int dstsize)
{
    irk_expect(srcsize >= 0);

    int i = 0, k = 0;
    for (; i <= srcsize - 3; i += 3)
    {
        uint32_t b24 = (src[i] << 16) | (src[i + 1] << 8) | src[i + 2];
        dst[k + 3] = s_Base64EncTab[b24 & 0x3F];
        b24 >>= 6;
        dst[k + 2] = s_Base64EncTab[b24 & 0x3F];
        b24 >>= 6;
        dst[k + 1] = s_Base64EncTab[b24 & 0x3F];
        b24 >>= 6;
        dst[k + 0] = s_Base64EncTab[b24 & 0x3F];
        k += 4;
    }
    if (i + 1 == srcsize)
    {
        uint32_t b12 = src[i] << 4;
        dst[k + 3] = dst[k + 2] = '=';
        dst[k + 1] = s_Base64EncTab[b12 & 0x3F];
        b12 >>= 6;
        dst[k + 0] = s_Base64EncTab[b12 & 0x3F];
        k += 4;
    }
    else if (i + 2 == srcsize)
    {
        uint32_t b18 = (src[i] << 10) | (src[i + 1] << 2);
        dst[k + 3] = '=';
        dst[k + 2] = s_Base64EncTab[b18 & 0x3F];
        b18 >>= 6;
        dst[k + 1] = s_Base64EncTab[b18 & 0x3F];
        b18 >>= 6;
        dst[k + 0] = s_Base64EncTab[b18 & 0x3F];
        k += 4;
    }

    irk_ensure((k & 3) == 0 && k <= dstsize);
    return k;
}
std::string encode_base64(const uint8_t* src, int srcsize)
{
    irk_expect(srcsize >= 0);
    std::string out;
    out.resize(4 * (srcsize / 3 + 1));
    int len = encode_base64(src, srcsize, (char*)out.data(), (int)out.size());
    out.resize(len);
    return out;
}

// Base64 decoding, return decoded bytes count
// [size]: input base64 string length, must be multiple of 4
// [dst]: output buffer, buffer size should >= 3 * srcsize / 4
int decode_base64(const char* src, int srcsize, uint8_t* dst, int dstsize)
{
    irk_expect((srcsize & 0x80000003) == 0);    // positive and multiple of 4

    const uint8_t* us = (const uint8_t*)src;
    int i = 0, k = 0;
    for (; i < srcsize - 4; i += 4)
    {
        uint32_t b24 = s_Base64DecTab[us[i]];
        b24 = (b24 << 6) | s_Base64DecTab[us[i + 1]];
        b24 = (b24 << 6) | s_Base64DecTab[us[i + 2]];
        b24 = (b24 << 6) | s_Base64DecTab[us[i + 3]];
        dst[k++] = (b24 >> 16) & 0xFF;
        dst[k++] = (b24 >> 8) & 0xFF;
        dst[k++] = (b24 & 0xFF);
    }

    uint32_t b24 = s_Base64DecTab[us[i]];
    b24 = (b24 << 6) | s_Base64DecTab[us[i + 1]];
    b24 = (b24 << 6) | s_Base64DecTab[us[i + 2]];
    b24 = (b24 << 6) | s_Base64DecTab[us[i + 3]];
    if (src[i + 2] == '=')
    {
        dst[k++] = (b24 >> 16) & 0xFF;
    }
    else if (src[i + 3] == '=')
    {
        dst[k++] = (b24 >> 16) & 0xFF;
        dst[k++] = (b24 >> 8) & 0xFF;
    }
    else
    {
        dst[k++] = (b24 >> 16) & 0xFF;
        dst[k++] = (b24 >> 8) & 0xFF;
        dst[k++] = (b24 & 0xFF);
    }

    irk_ensure(k <= dstsize);
    return k;
}
int decode_base64(const std::string& src, uint8_t* dst, int dstsize)
{
    return decode_base64(src.data(), (int)src.size(), dst, dstsize);
}

} // namespace irk
