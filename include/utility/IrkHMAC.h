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

#ifndef _IRONBRICK_HMAC_H_
#define _IRONBRICK_HMAC_H_

#include "IrkCryptoHash.h"

namespace irk {

// get HMAC-MD5 Authentication Codes
void hmac_md5( const void* key, uint32_t klen, const void* data, size_t size, uint8_t mac[16] );

// get HMAC-SHA1 Authentication Codes
void hmac_sha1( const void* key, uint32_t klen, const void* data, size_t size, uint8_t mac[20] );

// get HMAC-SHA2-224 Authentication Codes
void hmac_sha2_224( const void* key, uint32_t klen, const void* data, size_t size, uint8_t mac[28] );

// get HMAC-SHA2-256 Authentication Codes
void hmac_sha2_256( const void* key, uint32_t klen, const void* data, size_t size, uint8_t mac[32] );

// get HMAC-SHA2-384 Authentication Codes
void hmac_sha2_384( const void* key, uint32_t klen, const void* data, size_t size, uint8_t mac[48] );

// get HMAC-SHA2-512 Authentication Codes
void hmac_sha2_512( const void* key, uint32_t klen, const void* data, size_t size, uint8_t mac[64] );

// max length of the final HMAC result(SHA2-512)
constexpr unsigned int kMaxHmacLen = 64;

// HMAC: Keyed-Hashing for Message Authentication Codes
class HmacAuth : IrkNocopy
{
public:
    explicit HmacAuth( CryptoHashAlg alg, uint32_t flags = 0 );
    ~HmacAuth();

    // return the message authentication codes length
    unsigned int mac_size() const { return m_macSize; }

    // reset internal state, begin new authentication
    void reset( const void* key, uint32_t klen );

    // input data, update message authentication codes
    void update( const void* data, size_t size );

    // output the final message authentication codes
    // WARNING: output buffer size must >= mac_size()
    void get_mac( uint8_t* outbuf );

    // get the final mac as hex-character c-string
    // WARNING: output buffer size must >= 2 * mac_size()
    // WARNING: output buffer is not null-terminated
    uint32_t get_hexmac( char* outbuf )
    {
        uint8_t temp[kMaxHmacLen];
        this->get_mac( temp );
        return to_hexstring( temp, this->mac_size(), outbuf );
    }

    // get the final mac as hex-character std::string
    std::string get_hexmac()
    {
        uint8_t temp[kMaxHmacLen];
        this->get_mac( temp );
        return to_hexstring( temp, this->mac_size() );
    }
private:
    CryptoHasher*   m_iHasher;
    CryptoHasher*   m_oHasher;
    unsigned int    m_macSize;
    unsigned int    m_blockSize;
};

// ditto, optimized for repeated HMAC using the same key
class HmacAuth2 : IrkNocopy
{
public:
    HmacAuth2( const void* key, uint32_t klen, CryptoHashAlg alg, uint32_t flags = 0 );
    ~HmacAuth2();

    // return the message authentication codes length
    unsigned int mac_size() const { return m_macSize; }

    // reset internal state, begin new authentication
    void reset();

    // input data, update message authentication codes
    void update( const void* data, size_t size );

    // output the final message authentication codes
    // WARNING: output buffer size must >= mac_size()
    void get_mac( uint8_t* outbuf );

    // get the final mac as hex-character c-string
    // WARNING: output buffer size must >= 2 * mac_size()
    // WARNING: output buffer is not null-terminated
    uint32_t get_hexmac( char* outbuf )
    {
        uint8_t temp[kMaxHmacLen];
        this->get_mac( temp );
        return to_hexstring( temp, this->mac_size(), outbuf );
    }

    // get the final mac as hex-character std::string
    std::string get_hexmac()
    {
        uint8_t temp[kMaxHmacLen];
        this->get_mac( temp );
        return to_hexstring( temp, this->mac_size() );
    }
private:
    CryptoHasher*   m_iKeyHasher;
    CryptoHasher*   m_oKeyHasher;
    CryptoHasher*   m_iHasher;
    CryptoHasher*   m_oHasher;
    unsigned int    m_macSize;
    unsigned int    m_blockSize;
};

} // namespace irk
#endif
