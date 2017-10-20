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

#include <string.h>
#include "IrkBitsUtility.h"
#include "IrkCryptoHash.h"

// rotate left
#ifdef _MSC_VER
extern "C" unsigned int __cdecl _rotl(unsigned int value, int shift);
extern "C" unsigned int __cdecl _rotr(unsigned int value, int shift);
extern "C" unsigned __int64 __cdecl _rotl64(unsigned __int64 value, int shift);
extern "C" unsigned __int64 __cdecl _rotr64(unsigned __int64 value, int shift);
#pragma intrinsic(_rotl, _rotr, _rotl64, _rotr64)
#define ROL(x, n)   _rotl(x, n)
#define ROR(x, n)   _rotr(x, n)
#define ROL64(x, n) _rotl64(x, n)
#define ROR64(x, n) _rotr64(x, n)
#else
#define ROL(x, n)   (((x) << (n)) | ((x) >> (32-(n))))
#define ROR(x, n)   (((x) >> (n)) | ((x) << (32-(n))))
#define ROL64(x, n) (((x) << (n)) | ((x) >> (64-(n))))
#define ROR64(x, n) (((x) >> (n)) | ((x) << (64-(n))))
#endif

namespace irk {

//======================================================================================================================
// MD5

// FF, GG, HH, and II transformations for rounds 1~4
#define FF(a, b, c, d, x, s, ac) { \
    a += (d ^ (b & (c ^ d))) + x + ac; \
    a =  ROL(a, s); \
    a += b; }
#define GG(a, b, c, d, x, s, ac) { \
    a += (c ^ (d & (b ^ c))) + x + ac; \
    a =  ROL(a, s); \
    a += b; }
#define HH(a, b, c, d, x, s, ac) { \
    a += (b ^ c ^ d) + x + ac; \
    a =  ROL(a, s); \
    a += b; }
#define II(a, b, c, d, x, s, ac) { \
    a += (c ^ (b | ~d)) + x + ac; \
    a =  ROL(a, s); \
    a += b; }

// MD5 basic transformation, algorithm from rfc-1321
static void md5_transform( uint32_t state[4], const uint8_t data[64] )
{
    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];
    const uint32_t* x = (const uint32_t*)data;  // assume littel-endian

    /* Round 1 */
    FF(a, b, c, d, x[0],  7,  0xd76aa478u);
    FF(d, a, b, c, x[1],  12, 0xe8c7b756u);
    FF(c, d, a, b, x[2],  17, 0x242070dbu);
    FF(b, c, d, a, x[3],  22, 0xc1bdceeeu);
    FF(a, b, c, d, x[4],  7,  0xf57c0fafu);
    FF(d, a, b, c, x[5],  12, 0x4787c62au);
    FF(c, d, a, b, x[6],  17, 0xa8304613u);
    FF(b, c, d, a, x[7],  22, 0xfd469501u);
    FF(a, b, c, d, x[8],  7,  0x698098d8u);
    FF(d, a, b, c, x[9],  12, 0x8b44f7afu);
    FF(c, d, a, b, x[10], 17, 0xffff5bb1u);
    FF(b, c, d, a, x[11], 22, 0x895cd7beu);
    FF(a, b, c, d, x[12], 7,  0x6b901122u);
    FF(d, a, b, c, x[13], 12, 0xfd987193u);
    FF(c, d, a, b, x[14], 17, 0xa679438eu);
    FF(b, c, d, a, x[15], 22, 0x49b40821u);

    /* Round 2 */
    GG(a, b, c, d, x[1],  5,  0xf61e2562u); 
    GG(d, a, b, c, x[6],  9,  0xc040b340u);
    GG(c, d, a, b, x[11], 14, 0x265e5a51u);
    GG(b, c, d, a, x[0],  20, 0xe9b6c7aau);
    GG(a, b, c, d, x[5],  5,  0xd62f105du);
    GG(d, a, b, c, x[10], 9,  0x2441453u); 
    GG(c, d, a, b, x[15], 14, 0xd8a1e681u);
    GG(b, c, d, a, x[4],  20, 0xe7d3fbc8u);
    GG(a, b, c, d, x[9],  5,  0x21e1cde6u);
    GG(d, a, b, c, x[14], 9,  0xc33707d6u);
    GG(c, d, a, b, x[3],  14, 0xf4d50d87u);
    GG(b, c, d, a, x[8],  20, 0x455a14edu);
    GG(a, b, c, d, x[13], 5,  0xa9e3e905u);
    GG(d, a, b, c, x[2],  9,  0xfcefa3f8u);
    GG(c, d, a, b, x[7],  14, 0x676f02d9u);
    GG(b, c, d, a, x[12], 20, 0x8d2a4c8au);

    /* Round 3 */
    HH(a, b, c, d, x[5],  4,  0xfffa3942u);
    HH(d, a, b, c, x[8],  11, 0x8771f681u);
    HH(c, d, a, b, x[11], 16, 0x6d9d6122u);
    HH(b, c, d, a, x[14], 23, 0xfde5380cu);
    HH(a, b, c, d, x[1],  4,  0xa4beea44u);
    HH(d, a, b, c, x[4],  11, 0x4bdecfa9u);
    HH(c, d, a, b, x[7],  16, 0xf6bb4b60u);
    HH(b, c, d, a, x[10], 23, 0xbebfbc70u);
    HH(a, b, c, d, x[13], 4,  0x289b7ec6u);
    HH(d, a, b, c, x[0],  11, 0xeaa127fau);
    HH(c, d, a, b, x[3],  16, 0xd4ef3085u);
    HH(b, c, d, a, x[6],  23, 0x4881d05u); 
    HH(a, b, c, d, x[9],  4,  0xd9d4d039u);
    HH(d, a, b, c, x[12], 11, 0xe6db99e5u);
    HH(c, d, a, b, x[15], 16, 0x1fa27cf8u);
    HH(b, c, d, a, x[2],  23, 0xc4ac5665u);

    /* Round 4 */
    II(a, b, c, d, x[0],  6,  0xf4292244u);
    II(d, a, b, c, x[7],  10, 0x432aff97u);
    II(c, d, a, b, x[14], 15, 0xab9423a7u);
    II(b, c, d, a, x[5],  21, 0xfc93a039u);
    II(a, b, c, d, x[12], 6,  0x655b59c3u);
    II(d, a, b, c, x[3],  10, 0x8f0ccc92u);
    II(c, d, a, b, x[10], 15, 0xffeff47du);
    II(b, c, d, a, x[1],  21, 0x85845dd1u);
    II(a, b, c, d, x[8],  6,  0x6fa87e4fu);
    II(d, a, b, c, x[15], 10, 0xfe2ce6e0u);
    II(c, d, a, b, x[6],  15, 0xa3014314u);
    II(b, c, d, a, x[13], 21, 0x4e0811a1u);
    II(a, b, c, d, x[4],  6,  0xf7537e82u);
    II(d, a, b, c, x[11], 10, 0xbd3af235u);
    II(c, d, a, b, x[2],  15, 0x2ad7d2bbu);
    II(b, c, d, a, x[9],  21, 0xeb86d391u);

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
}

// MD5 Hasher, used to hash long data
class MD5_Hasher : public CryptoHasher
{
public:
    unsigned int digest_size() const override   { return 16; }
    unsigned int block_size() const override    { return 64; }
    void reset() override;
    void update( const void* data, size_t size ) override;
    void get_digest( uint8_t* md5 ) override;
    void fill_state_from( const CryptoHasher* other ) override;
private:
    uint64_t m_size;        // total size
    uint32_t m_state[4];
    uint8_t  m_buff[64];
};

// reset internal state, begin new MD5 hashing
void MD5_Hasher::reset()
{
    m_size = 0;
    m_state[0] = 0x67452301u;
    m_state[1] = 0xefcdab89u;
    m_state[2] = 0x98badcfeu;
    m_state[3] = 0x10325476u;
}

// hash data, update MD5 digest
void MD5_Hasher::update( const void* buf, size_t size )
{
    const uint8_t* data = (const uint8_t*)buf;
    const uint32_t remain = (uint32_t)m_size & 0x3F;    // remain size in the buffer
    m_size += size;

    if( remain > 0 )
    {
        uint32_t fillLen = 64 - remain; // length to fill the buffer
        if( fillLen <= size )
        {
            memcpy( m_buff + remain, data, fillLen );
            data += fillLen;
            size -= fillLen;
            md5_transform( m_state, m_buff );
        }
        else    // data is not enough
        {
            memcpy( m_buff + remain, data, size );
            return;
        }
    }

    while( size >= 64 )
    {
        md5_transform( m_state, data );
        data += 64;
        size -= 64;
    }

    // save remain data
    memcpy( m_buff, data, size );
}

// get the final digest
void MD5_Hasher::get_digest( uint8_t* md5 )
{
    const uint32_t remain = (uint32_t)m_size & 0x3F;    // remain size in the buffer
    memset( m_buff + remain, 0, 64 - remain );
    m_buff[remain] = 0x80;

    if( remain >= 56 )
    {
        md5_transform( m_state, m_buff );
        memset( m_buff, 0, 64 );
    }

    *(uint64_as*)(m_buff + 56) = m_size * 8;    // total bits
    md5_transform( m_state, m_buff );

    memcpy( md5, m_state, 16 );
    m_size = 0;
}

// copy internal state
void MD5_Hasher::fill_state_from( const CryptoHasher* other )
{
    assert( other->digest_size() == this->digest_size() );
    const MD5_Hasher* src = static_cast<const MD5_Hasher*>( other );
    this->m_size = src->m_size;
    memcpy( this->m_state, src->m_state, sizeof(this->m_state) );
    memcpy( this->m_buff, src->m_buff, sizeof(this->m_buff) );
}

//======================================================================================================================
// SHA-1

#define SHA1_00_19(a,b,c,d,e,f,w) \
    f = 0x5a827999U + (e) + ROL(a,5) + (((c ^ d) & b) ^ d) + (w); \
    b = ROL(b, 30)
#define SHA1_20_39(a,b,c,d,e,f,w) \
    f = 0x6ed9eba1U + (e) + ROL(a,5) + (b ^ c ^ d) + (w); \
    b = ROL(b, 30)
#define SHA1_40_59(a,b,c,d,e,f,w) \
    f = 0x8f1bbcdcU + (e) + ROL(a,5) + ((b & c) | ((b|c) & d)) + (w); \
    b = ROL(b, 30)
#define SHA1_60_79(a,b,c,d,e,f,w) \
    f = 0xca62c1d6U + (e) + ROL(a,5) + (b ^ c ^ d) + (w); \
    b = ROL(b, 30)

// get w from previous w
#define SHA1_DATA(w0,w1,w2,w3) \
    w0 ^= (w1 ^ w2 ^ w3);  \
    w0 = ROL(w0, 1);

// SHA-1 basic transformation, algorithm from rfc-3174
static void sha1_transform( uint32_t state[5], const uint8_t data[64] )
{
    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];
    uint32_t e = state[4];
    uint32_t f;
    uint32_t w[16];     // input data as big-endian 32bit word

    w[0] = BE_READ32( data );
    SHA1_00_19( a, b, c, d, e, f, w[0] );
    w[1] = BE_READ32( data + 4 );
    SHA1_00_19( f, a, b, c, d, e, w[1] );
    w[2] = BE_READ32( data + 8 );
    SHA1_00_19( e, f, a, b, c, d, w[2] );
    w[3] = BE_READ32( data + 12 );
    SHA1_00_19( d, e, f, a, b, c, w[3] );
    w[4] = BE_READ32( data + 16 );
    SHA1_00_19( c, d, e, f, a, b, w[4] );  
    w[5] = BE_READ32( data + 20 );
    SHA1_00_19( b, c, d, e, f, a, w[5] );
    w[6] = BE_READ32( data + 24 );
    SHA1_00_19( a, b, c, d, e, f, w[6] );
    w[7] = BE_READ32( data + 28 );
    SHA1_00_19( f, a, b, c, d, e, w[7] );
    w[8] = BE_READ32( data + 32 );
    SHA1_00_19( e, f, a, b, c, d, w[8] );
    w[9] = BE_READ32( data + 36 );
    SHA1_00_19( d, e, f, a, b, c, w[9] );
    w[10] = BE_READ32( data + 40 );
    SHA1_00_19( c, d, e, f, a, b, w[10] );
    w[11] = BE_READ32( data + 44 );
    SHA1_00_19( b, c, d, e, f, a, w[11] );
    w[12] = BE_READ32( data + 48 );
    SHA1_00_19( a, b, c, d, e, f, w[12] );
    w[13] = BE_READ32( data + 52 );
    SHA1_00_19( f, a, b, c, d, e, w[13] );
    w[14] = BE_READ32( data + 56 );
    SHA1_00_19( e, f, a, b, c, d, w[14] );
    w[15] = BE_READ32( data + 60 );
    SHA1_00_19( d, e, f, a, b, c, w[15] );

    SHA1_DATA( w[0], w[2], w[8], w[13] );
    SHA1_00_19( c, d, e, f, a, b, w[0] );
    SHA1_DATA( w[1], w[3], w[9], w[14] );
    SHA1_00_19( b, c, d, e, f, a, w[1] );
    SHA1_DATA( w[2], w[4], w[10], w[15] );
    SHA1_00_19( a, b, c, d, e, f, w[2] );
    SHA1_DATA( w[3], w[5], w[11], w[0] );
    SHA1_00_19( f, a, b, c, d, e, w[3] );

    SHA1_DATA( w[4], w[6], w[12], w[1] );
    SHA1_20_39( e, f, a, b, c, d, w[4] );
    SHA1_DATA( w[5], w[7], w[13], w[2] );
    SHA1_20_39( d, e, f, a, b, c, w[5] );
    SHA1_DATA( w[6], w[8], w[14], w[3] );
    SHA1_20_39( c, d, e, f, a, b, w[6] );
    SHA1_DATA( w[7], w[9], w[15], w[4] );
    SHA1_20_39( b, c, d, e, f, a, w[7] );
    SHA1_DATA( w[8], w[10], w[0], w[5] );
    SHA1_20_39( a, b, c, d, e, f, w[8] );
    SHA1_DATA( w[9], w[11], w[1], w[6] );
    SHA1_20_39( f, a, b, c, d, e, w[9] );
    SHA1_DATA( w[10], w[12], w[2], w[7] );
    SHA1_20_39( e, f, a, b, c, d, w[10] );
    SHA1_DATA( w[11], w[13], w[3], w[8] );
    SHA1_20_39( d, e, f, a, b, c, w[11] );
    SHA1_DATA( w[12], w[14], w[4], w[9] );
    SHA1_20_39( c, d, e, f, a, b, w[12] );
    SHA1_DATA( w[13], w[15], w[5], w[10] );
    SHA1_20_39( b, c, d, e, f, a, w[13] );
    SHA1_DATA( w[14], w[0], w[6], w[11] );
    SHA1_20_39( a, b, c, d, e, f, w[14] );
    SHA1_DATA( w[15], w[1], w[7], w[12] );
    SHA1_20_39( f, a, b, c, d, e, w[15] );
    SHA1_DATA( w[0], w[2], w[8], w[13] );
    SHA1_20_39( e, f, a, b, c, d, w[0] );
    SHA1_DATA( w[1], w[3], w[9], w[14] );
    SHA1_20_39( d, e, f, a, b, c, w[1] );
    SHA1_DATA( w[2], w[4], w[10], w[15] );
    SHA1_20_39( c, d, e, f, a, b, w[2] );
    SHA1_DATA( w[3], w[5], w[11], w[0] );
    SHA1_20_39( b, c, d, e, f, a, w[3] );
    SHA1_DATA( w[4], w[6], w[12], w[1] );
    SHA1_20_39( a, b, c, d, e, f, w[4] );
    SHA1_DATA( w[5], w[7], w[13], w[2] );
    SHA1_20_39( f, a, b, c, d, e, w[5] );
    SHA1_DATA( w[6], w[8], w[14], w[3] );
    SHA1_20_39( e, f, a, b, c, d, w[6] );
    SHA1_DATA( w[7], w[9], w[15], w[4] );
    SHA1_20_39( d, e, f, a, b, c, w[7] );

    SHA1_DATA( w[8], w[10], w[0], w[5] );
    SHA1_40_59( c, d, e, f, a, b, w[8] );
    SHA1_DATA( w[9], w[11], w[1], w[6] );
    SHA1_40_59( b, c, d, e, f, a, w[9] );
    SHA1_DATA( w[10], w[12], w[2], w[7] );
    SHA1_40_59( a, b, c, d, e, f, w[10] );
    SHA1_DATA( w[11], w[13], w[3], w[8] );
    SHA1_40_59( f, a, b, c, d, e, w[11] );
    SHA1_DATA( w[12], w[14], w[4], w[9] );
    SHA1_40_59( e, f, a, b, c, d, w[12] );
    SHA1_DATA( w[13], w[15], w[5], w[10] );
    SHA1_40_59( d, e, f, a, b, c, w[13] );
    SHA1_DATA( w[14], w[0], w[6], w[11] );
    SHA1_40_59( c, d, e, f, a, b, w[14] );
    SHA1_DATA( w[15], w[1], w[7], w[12] );
    SHA1_40_59( b, c, d, e, f, a, w[15] );
    SHA1_DATA( w[0], w[2], w[8], w[13] );
    SHA1_40_59( a, b, c, d, e, f, w[0] );
    SHA1_DATA( w[1], w[3], w[9], w[14] );
    SHA1_40_59( f, a, b, c, d, e, w[1] );
    SHA1_DATA( w[2], w[4], w[10], w[15] );
    SHA1_40_59( e, f, a, b, c, d, w[2] );
    SHA1_DATA( w[3], w[5], w[11], w[0] );
    SHA1_40_59( d, e, f, a, b, c, w[3] );
    SHA1_DATA( w[4], w[6], w[12], w[1] );
    SHA1_40_59( c, d, e, f, a, b, w[4] );
    SHA1_DATA( w[5], w[7], w[13], w[2] );
    SHA1_40_59( b, c, d, e, f, a, w[5] );
    SHA1_DATA( w[6], w[8], w[14], w[3] );
    SHA1_40_59( a, b, c, d, e, f, w[6] );
    SHA1_DATA( w[7], w[9], w[15], w[4] );
    SHA1_40_59( f, a, b, c, d, e, w[7] );
    SHA1_DATA( w[8], w[10], w[0], w[5] );
    SHA1_40_59( e, f, a, b, c, d, w[8] );
    SHA1_DATA( w[9], w[11], w[1], w[6] );
    SHA1_40_59( d, e, f, a, b, c, w[9] );
    SHA1_DATA( w[10], w[12], w[2], w[7] );
    SHA1_40_59( c, d, e, f, a, b, w[10] );
    SHA1_DATA( w[11], w[13], w[3], w[8] );
    SHA1_40_59( b, c, d, e, f, a, w[11] );

    SHA1_DATA( w[12], w[14], w[4], w[9] );
    SHA1_60_79( a, b, c, d, e, f, w[12] );
    SHA1_DATA( w[13], w[15], w[5], w[10] );
    SHA1_60_79( f, a, b, c, d, e, w[13] );
    SHA1_DATA( w[14], w[0], w[6], w[11] );
    SHA1_60_79( e, f, a, b, c, d, w[14] );
    SHA1_DATA( w[15], w[1], w[7], w[12] );
    SHA1_60_79( d, e, f, a, b, c, w[15] );
    SHA1_DATA( w[0], w[2], w[8], w[13] );
    SHA1_60_79( c, d, e, f, a, b, w[0] );
    SHA1_DATA( w[1], w[3], w[9], w[14] );
    SHA1_60_79( b, c, d, e, f, a, w[1] );
    SHA1_DATA( w[2], w[4], w[10], w[15] );
    SHA1_60_79( a, b, c, d, e, f, w[2] );
    SHA1_DATA( w[3], w[5], w[11], w[0] );
    SHA1_60_79( f, a, b, c, d, e, w[3] );
    SHA1_DATA( w[4], w[6], w[12], w[1] );
    SHA1_60_79( e, f, a, b, c, d, w[4] );
    SHA1_DATA( w[5], w[7], w[13], w[2] );
    SHA1_60_79( d, e, f, a, b, c, w[5] );
    SHA1_DATA( w[6], w[8], w[14], w[3] );
    SHA1_60_79( c, d, e, f, a, b, w[6] );
    SHA1_DATA( w[7], w[9], w[15], w[4] );
    SHA1_60_79( b, c, d, e, f, a, w[7] );
    SHA1_DATA( w[8], w[10], w[0], w[5] );
    SHA1_60_79( a, b, c, d, e, f, w[8] );
    SHA1_DATA( w[9], w[11], w[1], w[6] );
    SHA1_60_79( f, a, b, c, d, e, w[9] );
    SHA1_DATA( w[10], w[12], w[2], w[7] );
    SHA1_60_79( e, f, a, b, c, d, w[10] );
    SHA1_DATA( w[11], w[13], w[3], w[8] );
    SHA1_60_79( d, e, f, a, b, c, w[11] );
    SHA1_DATA( w[12], w[14], w[4], w[9] );
    SHA1_60_79( c, d, e, f, a, b, w[12] );
    SHA1_DATA( w[13], w[15], w[5], w[10] );
    SHA1_60_79( b, c, d, e, f, a, w[13] );
    SHA1_DATA( w[14], w[0], w[6], w[11] );
    SHA1_60_79( a, b, c, d, e, f, w[14] );
    SHA1_DATA( w[15], w[1], w[7], w[12] );
    SHA1_60_79( f, a, b, c, d, e, w[15] );

    state[0] += e;
    state[1] += f;
    state[2] += a;
    state[3] += b;
    state[4] += c;
}

//======================================================================================================================
class SHA1_hasher : public CryptoHasher
{
public:
    unsigned int digest_size() const override   { return 20; }
    unsigned int block_size() const override    { return 64; }
    void reset() override;
    void update( const void* data, size_t size ) override;
    void get_digest( uint8_t* outbuf ) override;
    void fill_state_from( const CryptoHasher* other ) override;
private:
    uint64_t m_size;        // total size
    uint32_t m_state[5];
    uint8_t  m_buff[64];
};

void SHA1_hasher::reset()
{
    m_size = 0;
    m_state[0] = 0x67452301U;
    m_state[1] = 0xefcdab89U;
    m_state[2] = 0x98badcfeU;
    m_state[3] = 0x10325476U;
    m_state[4] = 0xc3d2e1f0U;
}

void SHA1_hasher::update( const void* buf, size_t size )
{
    const uint8_t* data = (const uint8_t*)buf;
    const uint32_t remain = (uint32_t)m_size & 0x3F;    // remain size in the buffer
    m_size += size;

    if( remain > 0 )
    {
        uint32_t fillLen = 64 - remain; // length to fill the buffer
        if( fillLen <= size )
        {
            memcpy( m_buff + remain, data, fillLen );
            data += fillLen;
            size -= fillLen;
            sha1_transform( m_state, m_buff );
        }
        else    // data is not enough
        {
            memcpy( m_buff + remain, data, size );
            return;
        }
    }

    while( size >= 64 )
    {
        sha1_transform( m_state, data );
        data += 64;
        size -= 64;
    }

    // save remain data
    memcpy( m_buff, data, size );
}

void SHA1_hasher::get_digest( uint8_t* outbuf )
{
    const uint32_t remain = (uint32_t)m_size & 0x3F;    // remain size in the buffer
    memset( m_buff + remain, 0, 64 - remain );
    m_buff[remain] = 0x80;

    if( remain >= 56 )
    {
        sha1_transform( m_state, m_buff );
        memset( m_buff, 0, 64 );
    }
    BE_WRITE64( m_buff + 56, m_size * 8 );      // total bits
    sha1_transform( m_state, m_buff );

    BE_WRITE32( outbuf, m_state[0] );
    BE_WRITE32( outbuf + 4, m_state[1] );
    BE_WRITE32( outbuf + 8, m_state[2] );
    BE_WRITE32( outbuf + 12, m_state[3] );
    BE_WRITE32( outbuf + 16, m_state[4] );
}

// copy internal state
void SHA1_hasher::fill_state_from( const CryptoHasher* other )
{
    assert( other->digest_size() == this->digest_size() );
    const SHA1_hasher* src = static_cast<const SHA1_hasher*>( other );
    this->m_size = src->m_size;
    memcpy( this->m_state, src->m_state, sizeof(this->m_state) );
    memcpy( this->m_buff, src->m_buff, sizeof(this->m_buff) );
}

//======================================================================================================================
// SHA2-256

#define SHA2_Ch(x,y,z)      ((x & y) ^ (~(x) & z))
#define SHA2_Maj(x,y,z)     ((x & y) ^ (x & z) ^ (y & z))
#define SHA2_256_BSIG0(x)   (ROR(x,2) ^ ROR(x,13) ^ ROR(x,22))
#define SHA2_256_BSIG1(x)   (ROR(x,6) ^ ROR(x,11) ^ ROR(x,25))
#define SHA2_256_SSIG0(x)   (ROR(x,7) ^ ROR(x,18) ^ (x >> 3))
#define SHA2_256_SSIG1(x)   (ROR(x,17) ^ ROR(x,19) ^ (x >> 10))

#define SHA2_256_ROUND(a,b,c,d,e,f,g,h,k,w) do { \
    uint32_t t = SHA2_256_BSIG1(e) + SHA2_Ch(e,f,g) + (h) + (k) + (w); \
    h = SHA2_256_BSIG0(a) + SHA2_Maj(a,b,c); \
    d += t; \
    h += t; }while(0)

// calculate data from previous data
#define SHA2_256_DATA(w0,w1,w2,w3) (w0 += SHA2_256_SSIG0(w1) + (w2) + SHA2_256_SSIG1(w3))

// SHA2-256 basic transformation, algorithm from rfc-6234
static void sha2_transform256( uint32_t state[8], const uint8_t data[64] )
{
    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];
    uint32_t e = state[4];
    uint32_t f = state[5];
    uint32_t g = state[6];
    uint32_t h = state[7];
    uint32_t w[16];

    w[0] = BE_READ32( data + 0 );
    SHA2_256_ROUND( a, b, c, d, e, f, g, h, 0x428a2f98U, w[0] );
    w[1] = BE_READ32( data + 4 );
    SHA2_256_ROUND( h, a, b, c, d, e, f, g, 0x71374491U, w[1] );
    w[2] = BE_READ32( data + 8 );
    SHA2_256_ROUND( g, h, a, b, c, d, e, f, 0xb5c0fbcfU, w[2] );
    w[3] = BE_READ32( data + 12 );
    SHA2_256_ROUND( f, g, h, a, b, c, d, e, 0xe9b5dba5U, w[3] );
    w[4] = BE_READ32( data + 16 );
    SHA2_256_ROUND( e, f, g, h, a, b, c, d, 0x3956c25bU, w[4] );
    w[5] = BE_READ32( data + 20 );
    SHA2_256_ROUND( d, e, f, g, h, a, b, c, 0x59f111f1U, w[5] );
    w[6] = BE_READ32( data + 24 );
    SHA2_256_ROUND( c, d, e, f, g, h, a, b, 0x923f82a4U, w[6] );
    w[7] = BE_READ32( data + 28 );
    SHA2_256_ROUND( b, c, d, e, f, g, h, a, 0xab1c5ed5U, w[7] );
    w[8] = BE_READ32( data + 32 );
    SHA2_256_ROUND( a, b, c, d, e, f, g, h, 0xd807aa98U, w[8] );
    w[9] = BE_READ32( data + 36 );
    SHA2_256_ROUND( h, a, b, c, d, e, f, g, 0x12835b01U, w[9] );
    w[10] = BE_READ32( data + 40 );
    SHA2_256_ROUND( g, h, a, b, c, d, e, f, 0x243185beU, w[10] );
    w[11] = BE_READ32( data + 44 );
    SHA2_256_ROUND( f, g, h, a, b, c, d, e, 0x550c7dc3U, w[11] );
    w[12] = BE_READ32( data + 48 );
    SHA2_256_ROUND( e, f, g, h, a, b, c, d, 0x72be5d74U, w[12] );
    w[13] = BE_READ32( data + 52 );
    SHA2_256_ROUND( d, e, f, g, h, a, b, c, 0x80deb1feU, w[13] );
    w[14] = BE_READ32( data + 56 );
    SHA2_256_ROUND( c, d, e, f, g, h, a, b, 0x9bdc06a7U, w[14] );
    w[15] = BE_READ32( data + 60 );
    SHA2_256_ROUND( b, c, d, e, f, g, h, a, 0xc19bf174U, w[15] );

    SHA2_256_DATA( w[0], w[1], w[9], w[14] );
    SHA2_256_ROUND( a, b, c, d, e, f, g, h, 0xe49b69c1U, w[0] );
    SHA2_256_DATA( w[1], w[2], w[10], w[15] );
    SHA2_256_ROUND( h, a, b, c, d, e, f, g, 0xefbe4786U, w[1] );
    SHA2_256_DATA( w[2], w[3], w[11], w[0] );
    SHA2_256_ROUND( g, h, a, b, c, d, e, f, 0xfc19dc6U, w[2] );
    SHA2_256_DATA( w[3], w[4], w[12], w[1] );
    SHA2_256_ROUND( f, g, h, a, b, c, d, e, 0x240ca1ccU, w[3] );
    SHA2_256_DATA( w[4], w[5], w[13], w[2] );
    SHA2_256_ROUND( e, f, g, h, a, b, c, d, 0x2de92c6fU, w[4] );
    SHA2_256_DATA( w[5], w[6], w[14], w[3] );
    SHA2_256_ROUND( d, e, f, g, h, a, b, c, 0x4a7484aaU, w[5] );
    SHA2_256_DATA( w[6], w[7], w[15], w[4] );
    SHA2_256_ROUND( c, d, e, f, g, h, a, b, 0x5cb0a9dcU, w[6] );
    SHA2_256_DATA( w[7], w[8], w[0], w[5] );
    SHA2_256_ROUND( b, c, d, e, f, g, h, a, 0x76f988daU, w[7] );
    SHA2_256_DATA( w[8], w[9], w[1], w[6] );
    SHA2_256_ROUND( a, b, c, d, e, f, g, h, 0x983e5152U, w[8] );
    SHA2_256_DATA( w[9], w[10], w[2], w[7] );
    SHA2_256_ROUND( h, a, b, c, d, e, f, g, 0xa831c66dU, w[9] );
    SHA2_256_DATA( w[10], w[11], w[3], w[8] );
    SHA2_256_ROUND( g, h, a, b, c, d, e, f, 0xb00327c8U, w[10] );
    SHA2_256_DATA( w[11], w[12], w[4], w[9] );
    SHA2_256_ROUND( f, g, h, a, b, c, d, e, 0xbf597fc7U, w[11] );
    SHA2_256_DATA( w[12], w[13], w[5], w[10] );
    SHA2_256_ROUND( e, f, g, h, a, b, c, d, 0xc6e00bf3U, w[12] );
    SHA2_256_DATA( w[13], w[14], w[6], w[11] );
    SHA2_256_ROUND( d, e, f, g, h, a, b, c, 0xd5a79147U, w[13] );
    SHA2_256_DATA( w[14], w[15], w[7], w[12] );
    SHA2_256_ROUND( c, d, e, f, g, h, a, b, 0x6ca6351U, w[14] );
    SHA2_256_DATA( w[15], w[0], w[8], w[13] );
    SHA2_256_ROUND( b, c, d, e, f, g, h, a, 0x14292967U, w[15] );
    SHA2_256_DATA( w[0], w[1], w[9], w[14] );
    SHA2_256_ROUND( a, b, c, d, e, f, g, h, 0x27b70a85U, w[0] );
    SHA2_256_DATA( w[1], w[2], w[10], w[15] );
    SHA2_256_ROUND( h, a, b, c, d, e, f, g, 0x2e1b2138U, w[1] );
    SHA2_256_DATA( w[2], w[3], w[11], w[0] );
    SHA2_256_ROUND( g, h, a, b, c, d, e, f, 0x4d2c6dfcU, w[2] );
    SHA2_256_DATA( w[3], w[4], w[12], w[1] );
    SHA2_256_ROUND( f, g, h, a, b, c, d, e, 0x53380d13U, w[3] );
    SHA2_256_DATA( w[4], w[5], w[13], w[2] );
    SHA2_256_ROUND( e, f, g, h, a, b, c, d, 0x650a7354U, w[4] );
    SHA2_256_DATA( w[5], w[6], w[14], w[3] );
    SHA2_256_ROUND( d, e, f, g, h, a, b, c, 0x766a0abbU, w[5] );
    SHA2_256_DATA( w[6], w[7], w[15], w[4] );
    SHA2_256_ROUND( c, d, e, f, g, h, a, b, 0x81c2c92eU, w[6] );
    SHA2_256_DATA( w[7], w[8], w[0], w[5] );
    SHA2_256_ROUND( b, c, d, e, f, g, h, a, 0x92722c85U, w[7] );
    SHA2_256_DATA( w[8], w[9], w[1], w[6] );
    SHA2_256_ROUND( a, b, c, d, e, f, g, h, 0xa2bfe8a1U, w[8] );
    SHA2_256_DATA( w[9], w[10], w[2], w[7] );
    SHA2_256_ROUND( h, a, b, c, d, e, f, g, 0xa81a664bU, w[9] );
    SHA2_256_DATA( w[10], w[11], w[3], w[8] );
    SHA2_256_ROUND( g, h, a, b, c, d, e, f, 0xc24b8b70U, w[10] );
    SHA2_256_DATA( w[11], w[12], w[4], w[9] );
    SHA2_256_ROUND( f, g, h, a, b, c, d, e, 0xc76c51a3U, w[11] );
    SHA2_256_DATA( w[12], w[13], w[5], w[10] );
    SHA2_256_ROUND( e, f, g, h, a, b, c, d, 0xd192e819U, w[12] );
    SHA2_256_DATA( w[13], w[14], w[6], w[11] );
    SHA2_256_ROUND( d, e, f, g, h, a, b, c, 0xd6990624U, w[13] );
    SHA2_256_DATA( w[14], w[15], w[7], w[12] );
    SHA2_256_ROUND( c, d, e, f, g, h, a, b, 0xf40e3585U, w[14] );
    SHA2_256_DATA( w[15], w[0], w[8], w[13] );
    SHA2_256_ROUND( b, c, d, e, f, g, h, a, 0x106aa070U, w[15] );
    SHA2_256_DATA( w[0], w[1], w[9], w[14] );
    SHA2_256_ROUND( a, b, c, d, e, f, g, h, 0x19a4c116U, w[0] );
    SHA2_256_DATA( w[1], w[2], w[10], w[15] );
    SHA2_256_ROUND( h, a, b, c, d, e, f, g, 0x1e376c08U, w[1] );
    SHA2_256_DATA( w[2], w[3], w[11], w[0] );
    SHA2_256_ROUND( g, h, a, b, c, d, e, f, 0x2748774cU, w[2] );
    SHA2_256_DATA( w[3], w[4], w[12], w[1] );
    SHA2_256_ROUND( f, g, h, a, b, c, d, e, 0x34b0bcb5U, w[3] );
    SHA2_256_DATA( w[4], w[5], w[13], w[2] );
    SHA2_256_ROUND( e, f, g, h, a, b, c, d, 0x391c0cb3U, w[4] );
    SHA2_256_DATA( w[5], w[6], w[14], w[3] );
    SHA2_256_ROUND( d, e, f, g, h, a, b, c, 0x4ed8aa4aU, w[5] );
    SHA2_256_DATA( w[6], w[7], w[15], w[4] );
    SHA2_256_ROUND( c, d, e, f, g, h, a, b, 0x5b9cca4fU, w[6] );
    SHA2_256_DATA( w[7], w[8], w[0], w[5] );
    SHA2_256_ROUND( b, c, d, e, f, g, h, a, 0x682e6ff3U, w[7] );
    SHA2_256_DATA( w[8], w[9], w[1], w[6] );
    SHA2_256_ROUND( a, b, c, d, e, f, g, h, 0x748f82eeU, w[8] );
    SHA2_256_DATA( w[9], w[10], w[2], w[7] );
    SHA2_256_ROUND( h, a, b, c, d, e, f, g, 0x78a5636fU, w[9] );
    SHA2_256_DATA( w[10], w[11], w[3], w[8] );
    SHA2_256_ROUND( g, h, a, b, c, d, e, f, 0x84c87814U, w[10] );
    SHA2_256_DATA( w[11], w[12], w[4], w[9] );
    SHA2_256_ROUND( f, g, h, a, b, c, d, e, 0x8cc70208U, w[11] );
    SHA2_256_DATA( w[12], w[13], w[5], w[10] );
    SHA2_256_ROUND( e, f, g, h, a, b, c, d, 0x90befffaU, w[12] );
    SHA2_256_DATA( w[13], w[14], w[6], w[11] );
    SHA2_256_ROUND( d, e, f, g, h, a, b, c, 0xa4506cebU, w[13] );
    SHA2_256_DATA( w[14], w[15], w[7], w[12] );
    SHA2_256_ROUND( c, d, e, f, g, h, a, b, 0xbef9a3f7U, w[14] );
    SHA2_256_DATA( w[15], w[0], w[8], w[13] );
    SHA2_256_ROUND( b, c, d, e, f, g, h, a, 0xc67178f2U, w[15] );

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

//======================================================================================================================
class SHA2_hasher256 : public CryptoHasher
{
public:
    unsigned int digest_size() const override   { return 32; }
    unsigned int block_size() const override    { return 64; }
    void reset() override;
    void update( const void* data, size_t size ) override;
    void get_digest( uint8_t* outbuf ) override;
    void fill_state_from( const CryptoHasher* other ) override;
protected:
    uint64_t m_size;        // total size
    uint32_t m_state[8];
    uint8_t  m_buff[64];
};

void SHA2_hasher256::reset()
{
    m_size = 0;
    m_state[0] = 0x6a09e667U;
    m_state[1] = 0xbb67ae85U;
    m_state[2] = 0x3c6ef372U;
    m_state[3] = 0xa54ff53aU;
    m_state[4] = 0x510e527fU;
    m_state[5] = 0x9b05688cU;
    m_state[6] = 0x1f83d9abU;
    m_state[7] = 0x5be0cd19U;
}

void SHA2_hasher256::update( const void* buf, size_t size )
{
    const uint8_t* data = (const uint8_t*)buf;
    const uint32_t remain = (uint32_t)m_size & 0x3F;    // remain size in the buffer
    m_size += size;

    if( remain > 0 )
    {
        uint32_t fillLen = 64 - remain; // length to fill the buffer
        if( fillLen <= size )
        {
            memcpy( m_buff + remain, data, fillLen );
            data += fillLen;
            size -= fillLen;
            sha2_transform256( m_state, m_buff );
        }
        else    // data is not enough
        {
            memcpy( m_buff + remain, data, size );
            return;
        }
    }

    while( size >= 64 )
    {
        sha2_transform256( m_state, data );
        data += 64;
        size -= 64;
    }

    // save remain data
    memcpy( m_buff, data, size );
}

void SHA2_hasher256::get_digest( uint8_t* outbuf )
{
    const uint32_t remain = (uint32_t)m_size & 0x3F;    // remain size in the buffer
    memset( m_buff + remain, 0, 64 - remain );
    m_buff[remain] = 0x80;

    if( remain >= 56 )
    {
        sha2_transform256( m_state, m_buff );
        memset( m_buff, 0, 64 );
    }
    BE_WRITE64( m_buff + 56, m_size * 8 );      // total bits
    sha2_transform256( m_state, m_buff );

    BE_WRITE32( outbuf, m_state[0] );
    BE_WRITE32( outbuf + 4, m_state[1] );
    BE_WRITE32( outbuf + 8, m_state[2] );
    BE_WRITE32( outbuf + 12, m_state[3] );
    BE_WRITE32( outbuf + 16, m_state[4] );
    BE_WRITE32( outbuf + 20, m_state[5] );
    BE_WRITE32( outbuf + 24, m_state[6] );
    BE_WRITE32( outbuf + 28, m_state[7] );
}

// copy internal state
void SHA2_hasher256::fill_state_from( const CryptoHasher* other )
{
    assert( other->digest_size() == this->digest_size() );
    const SHA2_hasher256* src = static_cast<const SHA2_hasher256*>( other );
    this->m_size = src->m_size;
    memcpy( this->m_state, src->m_state, sizeof(this->m_state) );
    memcpy( this->m_buff, src->m_buff, sizeof(this->m_buff) );
}

class SHA2_hasher224 : public SHA2_hasher256
{
public:
    unsigned int digest_size() const override   { return 28; }
    void reset() override;
    void get_digest( uint8_t* outbuf ) override;
};

void SHA2_hasher224::reset()
{
    m_size = 0;
    m_state[0] = 0xc1059ed8U;
    m_state[1] = 0x367cd507U;
    m_state[2] = 0x3070dd17U;
    m_state[3] = 0xf70e5939U;
    m_state[4] = 0xffc00b31U;
    m_state[5] = 0x68581511U;
    m_state[6] = 0x64f98fa7U;
    m_state[7] = 0xbefa4fa4U;
}

void SHA2_hasher224::get_digest( uint8_t* outbuf )
{
    const uint32_t remain = (uint32_t)m_size & 0x3F;    // remain size in the buffer
    memset( m_buff + remain, 0, 64 - remain );
    m_buff[remain] = 0x80;

    if( remain >= 56 )
    {
        sha2_transform256( m_state, m_buff );
        memset( m_buff, 0, 64 );
    }
    BE_WRITE64( m_buff + 56, m_size * 8 );      // total bits
    sha2_transform256( m_state, m_buff );

    BE_WRITE32( outbuf, m_state[0] );
    BE_WRITE32( outbuf + 4, m_state[1] );
    BE_WRITE32( outbuf + 8, m_state[2] );
    BE_WRITE32( outbuf + 12, m_state[3] );
    BE_WRITE32( outbuf + 16, m_state[4] );
    BE_WRITE32( outbuf + 20, m_state[5] );
    BE_WRITE32( outbuf + 24, m_state[6] );
}

//======================================================================================================================
// SHA2-512

static const uint64_t SHA2K512[80] = 
{
    0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL,
    0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
    0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL,
    0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
    0xd807aa98a3030242ULL, 0x12835b0145706fbeULL,
    0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
    0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL,
    0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
    0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL,
    0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
    0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL,
    0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
    0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL,
    0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
    0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL,
    0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
    0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL,
    0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
    0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL,
    0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
    0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL,
    0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
    0xd192e819d6ef5218ULL, 0xd69906245565a910ULL,
    0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
    0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL,
    0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
    0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL,
    0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
    0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL,
    0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
    0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL,
    0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
    0xca273eceea26619cULL, 0xd186b8c721c0c207ULL,
    0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
    0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL,
    0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
    0x28db77f523047d84ULL, 0x32caab7b40c72493ULL,
    0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
    0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL,
    0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL
};

#define SHA2_512_BSIG0(x)   (ROR64(x,28) ^ ROR64(x,34) ^ ROR64(x,39))
#define SHA2_512_BSIG1(x)   (ROR64(x,14) ^ ROR64(x,18) ^ ROR64(x,41))
#define SHA2_512_SSIG0(x)   (ROR64(x,1) ^ ROR64(x,8) ^ (x >> 7))
#define SHA2_512_SSIG1(x)   (ROR64(x,19) ^ ROR64(x,61) ^ (x >> 6))

#define SHA2_512_ROUND(a,b,c,d,e,f,g,h,k,w) do { \
    uint64_t t = SHA2_512_BSIG1(e) + SHA2_Ch(e,f,g) + (h) + (k) + (w); \
    h = SHA2_512_BSIG0(a) + SHA2_Maj(a,b,c); \
    d += t; \
    h += t; }while(0)

// calaculate data from previous data
#define SHA2_512_DATA(w0,w1,w2,w3) (w0 += SHA2_512_SSIG0(w1) + (w2) + SHA2_512_SSIG1(w3))

// SHA2-256 basic transformation, algorithm from rfc-6234
static void sha2_transform512( uint64_t state[8], const uint8_t data[128] )
{
    uint64_t a = state[0];
    uint64_t b = state[1];
    uint64_t c = state[2];
    uint64_t d = state[3];
    uint64_t e = state[4];
    uint64_t f = state[5];
    uint64_t g = state[6];
    uint64_t h = state[7];
    uint64_t w[16];

    w[0] = BE_READ64( data + 0 );
    SHA2_512_ROUND( a, b, c, d, e, f, g, h, SHA2K512[0], w[0] );
    w[1] = BE_READ64( data + 8 );
    SHA2_512_ROUND( h, a, b, c, d, e, f, g, SHA2K512[1], w[1] );
    w[2] = BE_READ64( data + 16 );
    SHA2_512_ROUND( g, h, a, b, c, d, e, f, SHA2K512[2], w[2] );
    w[3] = BE_READ64( data + 24 );
    SHA2_512_ROUND( f, g, h, a, b, c, d, e, SHA2K512[3], w[3] );
    w[4] = BE_READ64( data + 32 );
    SHA2_512_ROUND( e, f, g, h, a, b, c, d, SHA2K512[4], w[4] );
    w[5] = BE_READ64( data + 40 );
    SHA2_512_ROUND( d, e, f, g, h, a, b, c, SHA2K512[5], w[5] );
    w[6] = BE_READ64( data + 48 );
    SHA2_512_ROUND( c, d, e, f, g, h, a, b, SHA2K512[6], w[6] );
    w[7] = BE_READ64( data + 56 );
    SHA2_512_ROUND( b, c, d, e, f, g, h, a, SHA2K512[7], w[7] );
    w[8] = BE_READ64( data + 64 );
    SHA2_512_ROUND( a, b, c, d, e, f, g, h, SHA2K512[8], w[8] );
    w[9] = BE_READ64( data + 72 );
    SHA2_512_ROUND( h, a, b, c, d, e, f, g, SHA2K512[9], w[9] );
    w[10] = BE_READ64( data + 80 );
    SHA2_512_ROUND( g, h, a, b, c, d, e, f, SHA2K512[10], w[10] );
    w[11] = BE_READ64( data + 88 );
    SHA2_512_ROUND( f, g, h, a, b, c, d, e, SHA2K512[11], w[11] );
    w[12] = BE_READ64( data + 96 );
    SHA2_512_ROUND( e, f, g, h, a, b, c, d, SHA2K512[12], w[12] );
    w[13] = BE_READ64( data + 104 );
    SHA2_512_ROUND( d, e, f, g, h, a, b, c, SHA2K512[13], w[13] );
    w[14] = BE_READ64( data + 112 );
    SHA2_512_ROUND( c, d, e, f, g, h, a, b, SHA2K512[14], w[14] );
    w[15] = BE_READ64( data + 120 );
    SHA2_512_ROUND( b, c, d, e, f, g, h, a, SHA2K512[15], w[15] );

    for( int i = 16; i < 80; i += 16 )
    {
        SHA2_512_DATA( w[0], w[1], w[9], w[14] );
        SHA2_512_ROUND( a, b, c, d, e, f, g, h, SHA2K512[i], w[0] );
        SHA2_512_DATA( w[1], w[2], w[10], w[15] );
        SHA2_512_ROUND( h, a, b, c, d, e, f, g, SHA2K512[i+1], w[1] );
        SHA2_512_DATA( w[2], w[3], w[11], w[0] );
        SHA2_512_ROUND( g, h, a, b, c, d, e, f, SHA2K512[i+2], w[2] );
        SHA2_512_DATA( w[3], w[4], w[12], w[1] );
        SHA2_512_ROUND( f, g, h, a, b, c, d, e, SHA2K512[i+3], w[3] );
        SHA2_512_DATA( w[4], w[5], w[13], w[2] );
        SHA2_512_ROUND( e, f, g, h, a, b, c, d, SHA2K512[i+4], w[4] );
        SHA2_512_DATA( w[5], w[6], w[14], w[3] );
        SHA2_512_ROUND( d, e, f, g, h, a, b, c, SHA2K512[i+5], w[5] );
        SHA2_512_DATA( w[6], w[7], w[15], w[4] );
        SHA2_512_ROUND( c, d, e, f, g, h, a, b, SHA2K512[i+6], w[6] );
        SHA2_512_DATA( w[7], w[8], w[0], w[5] );
        SHA2_512_ROUND( b, c, d, e, f, g, h, a, SHA2K512[i+7], w[7] );
        SHA2_512_DATA( w[8], w[9], w[1], w[6] );
        SHA2_512_ROUND( a, b, c, d, e, f, g, h, SHA2K512[i+8], w[8] );
        SHA2_512_DATA( w[9], w[10], w[2], w[7] );
        SHA2_512_ROUND( h, a, b, c, d, e, f, g, SHA2K512[i+9], w[9] );
        SHA2_512_DATA( w[10], w[11], w[3], w[8] );
        SHA2_512_ROUND( g, h, a, b, c, d, e, f, SHA2K512[i+10], w[10] );
        SHA2_512_DATA( w[11], w[12], w[4], w[9] );
        SHA2_512_ROUND( f, g, h, a, b, c, d, e, SHA2K512[i+11], w[11] );
        SHA2_512_DATA( w[12], w[13], w[5], w[10] );
        SHA2_512_ROUND( e, f, g, h, a, b, c, d, SHA2K512[i+12], w[12] );
        SHA2_512_DATA( w[13], w[14], w[6], w[11] );
        SHA2_512_ROUND( d, e, f, g, h, a, b, c, SHA2K512[i+13], w[13] );
        SHA2_512_DATA( w[14], w[15], w[7], w[12] );
        SHA2_512_ROUND( c, d, e, f, g, h, a, b, SHA2K512[i+14], w[14] );
        SHA2_512_DATA( w[15], w[0], w[8], w[13] );
        SHA2_512_ROUND( b, c, d, e, f, g, h, a, SHA2K512[i+15], w[15] );
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

//======================================================================================================================
class SHA2_hasher512 : public CryptoHasher
{
public:
    unsigned int digest_size() const override   { return 64; }
    unsigned int block_size() const override    { return 128; }
    void reset() override;
    void update( const void* data, size_t size ) override;
    void get_digest( uint8_t* outbuf ) override;
    void fill_state_from( const CryptoHasher* other ) override;
protected:
    uint64_t m_size[2];     // total size
    uint64_t m_state[8];
    uint8_t  m_buff[128];
};

void SHA2_hasher512::reset()
{
    m_size[0] = 0;
    m_size[1] = 0;
    m_state[0] = 0x6a09e667f3bcc908ULL;
    m_state[1] = 0xbb67ae8584caa73bULL;
    m_state[2] = 0x3c6ef372fe94f82bULL;
    m_state[3] = 0xa54ff53a5f1d36f1ULL;
    m_state[4] = 0x510e527fade682d1ULL;
    m_state[5] = 0x9b05688c2b3e6c1fULL;
    m_state[6] = 0x1f83d9abfb41bd6bULL;
    m_state[7] = 0x5be0cd19137e2179ULL;
}

void SHA2_hasher512::update( const void* buf, size_t size )
{
    const uint8_t* data = (const uint8_t*)buf;
    const uint32_t remain = (uint32_t)m_size[0] & 0x7F; // remain size in the buffer
    const uint64_t newSize = m_size[0] + size;
    if( newSize < m_size[0] )   // 64 bits overflow
        m_size[1] += 1;
    m_size[0] = newSize;

    if( remain > 0 )
    {
        uint32_t fillLen = 128 - remain;    // length to fill the buffer
        if( fillLen <= size )
        {
            memcpy( m_buff + remain, data, fillLen );
            data += fillLen;
            size -= fillLen;
            sha2_transform512( m_state, m_buff );
        }
        else    // data is not enough
        {
            memcpy( m_buff + remain, data, size );
            return;
        }
    }

    while( size >= 128 )
    {
        sha2_transform512( m_state, data );
        data += 128;
        size -= 128;
    }

    // save remain data
    memcpy( m_buff, data, size );
}

void SHA2_hasher512::get_digest( uint8_t* outbuf )
{
    const uint32_t remain = (uint32_t)m_size[0] & 0x7F;    // remain size in the buffer
    memset( m_buff + remain, 0, 128 - remain );
    m_buff[remain] = 0x80;

    if( remain >= 112 )
    {
        sha2_transform512( m_state, m_buff );
        memset( m_buff, 0, 128 );
    }
    BE_WRITE64( m_buff + 112, m_size[1] * 8 );
    BE_WRITE64( m_buff + 120, m_size[0] * 8 );  // total bits
    sha2_transform512( m_state, m_buff );

    BE_WRITE64( outbuf, m_state[0] );
    BE_WRITE64( outbuf + 8, m_state[1] );
    BE_WRITE64( outbuf + 16, m_state[2] );
    BE_WRITE64( outbuf + 24, m_state[3] );
    BE_WRITE64( outbuf + 32, m_state[4] );
    BE_WRITE64( outbuf + 40, m_state[5] );
    BE_WRITE64( outbuf + 48, m_state[6] );
    BE_WRITE64( outbuf + 56, m_state[7] );
}

// copy internal state
void SHA2_hasher512::fill_state_from( const CryptoHasher* other )
{
    assert( other->digest_size() == this->digest_size() );
    const SHA2_hasher512* src = static_cast<const SHA2_hasher512*>( other );
    this->m_size[0] = src->m_size[0];
    this->m_size[1] = src->m_size[1];
    memcpy( this->m_state, src->m_state, sizeof(this->m_state) );
    memcpy( this->m_buff, src->m_buff, sizeof(this->m_buff) );
}

class SHA2_hasher384 : public SHA2_hasher512
{
public:
    unsigned int digest_size() const override   { return 48; }
    void reset() override;
    void get_digest( uint8_t* outbuf ) override;
};

void SHA2_hasher384::reset()
{
    m_size[0] = 0;
    m_size[1] = 0;
    m_state[0] = 0xcbbb9d5dc1059ed8ULL;
    m_state[1] = 0x629a292a367cd507ULL;
    m_state[2] = 0x9159015a3070dd17ULL;
    m_state[3] = 0x152fecd8f70e5939ULL;
    m_state[4] = 0x67332667ffc00b31ULL;
    m_state[5] = 0x8eb44a8768581511ULL;
    m_state[6] = 0xdb0c2e0d64f98fa7ULL;
    m_state[7] = 0x47b5481dbefa4fa4ULL;
}

void SHA2_hasher384::get_digest( uint8_t* outbuf )
{
    const uint32_t remain = (uint32_t)m_size[0] & 0x7F;    // remain size in the buffer
    memset( m_buff + remain, 0, 128 - remain );
    m_buff[remain] = 0x80;

    if( remain >= 112 )
    {
        sha2_transform512( m_state, m_buff );
        memset( m_buff, 0, 128 );
    }
    BE_WRITE64( m_buff + 112, m_size[1] * 8 );
    BE_WRITE64( m_buff + 120, m_size[0] * 8 );  // total bits
    sha2_transform512( m_state, m_buff );

    BE_WRITE64( outbuf, m_state[0] );
    BE_WRITE64( outbuf + 8, m_state[1] );
    BE_WRITE64( outbuf + 16, m_state[2] );
    BE_WRITE64( outbuf + 24, m_state[3] );
    BE_WRITE64( outbuf + 32, m_state[4] );
    BE_WRITE64( outbuf + 40, m_state[5] );
}

//======================================================================================================================

// hash data to get 16-bytes md5 digest
void md5_digest( const void* data, size_t size, uint8_t md5[16] )
{
    MD5_Hasher hasher;
    hasher.reset();
    hasher.update( data, size );
    hasher.get_digest( md5 );
}

// hash data to get 20-bytes SHA-1 digest
void sha1_digest( const void* data, size_t size, uint8_t outbuf[20] )
{
    SHA1_hasher hasher;
    hasher.reset();
    hasher.update( data, size );
    hasher.get_digest( outbuf );
}

// hash data to get 32-bytes SHA2-256 digest
void sha2_256_digest( const void* data, size_t size, uint8_t outbuf[32] )
{
    SHA2_hasher256 hasher;
    hasher.reset();
    hasher.update( data, size );
    hasher.get_digest( outbuf );
}

// hash data to get 28-bytes SHA2-224 digest
void sha2_224_digest( const void* data, size_t size, uint8_t outbuf[28] )
{
    SHA2_hasher224 hasher;
    hasher.reset();
    hasher.update( data, size );
    hasher.get_digest( outbuf );
}

// hash data to get 64-bytes SHA2-512 digest
void sha2_512_digest( const void* data, size_t size, uint8_t outbuf[64] )
{
    SHA2_hasher512 hasher;
    hasher.reset();
    hasher.update( data, size );
    hasher.get_digest( outbuf );
}

// hash data to get 48-bytes SHA2-384 digest
void sha2_384_digest( const void* data, size_t size, uint8_t outbuf[48] )
{
    SHA2_hasher384 hasher;
    hasher.reset();
    hasher.update( data, size );
    hasher.get_digest( outbuf );
}

//======================================================================================================================
static const char s_HexChar[] = "0123456789abcdef";

// convert binary data to hex-character string
uint32_t to_hexstring( const uint8_t* digest, uint32_t size, char* dstbuf )
{
    uint32_t k = 0;
    for( uint32_t i = 0; i < size; i++ )
    {
        dstbuf[k]   = s_HexChar[digest[i] >> 4];
        dstbuf[k+1] = s_HexChar[digest[i] & 0xF];
        k += 2;
    }
    return k;
}

std::string to_hexstring( const uint8_t* digest, uint32_t size )
{
    std::string str;
    str.reserve( size * 2 );
    for( uint32_t i = 0; i < size; i++ )
    {
        str.push_back( s_HexChar[digest[i] >> 4] );
        str.push_back( s_HexChar[digest[i] & 0xF] );
    }
    return str;
}

//======================================================================================================================
CryptoHasher* create_crypto_hasher( CryptoHashAlg alg, uint32_t /*flags*/ )
{
    switch( alg )
    {
    case CryptoHashAlg::MD5:
        return new MD5_Hasher;
    case CryptoHashAlg::SHA1:
        return new SHA1_hasher;
    case CryptoHashAlg::SHA2_224:
        return new SHA2_hasher224;
    case CryptoHashAlg::SHA2_256:
        return new SHA2_hasher256;
    case CryptoHashAlg::SHA2_384:
        return new SHA2_hasher384;
    case CryptoHashAlg::SHA2_512:
        return new SHA2_hasher512;
    default:
        break;
    }
    return nullptr;
}

void delete_crypto_hasher( CryptoHasher* hasher )
{
    delete hasher;
}

} // namespace irk
