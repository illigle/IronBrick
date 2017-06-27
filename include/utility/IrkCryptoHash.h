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

#ifndef _IRONBRICK_CRYPTO_HASH_H_
#define _IRONBRICK_CRYPTO_HASH_H_

#include <string>
#include "IrkCommon.h"

namespace irk {

// hash data to get 16-bytes md5 digest
void md5_digest( const void* data, size_t size, uint8_t md5[16] );

// hash data to get 20-bytes SHA-1 digest
void sha1_digest( const void* data, size_t size, uint8_t digest[20] );

// hash data to get 32-bytes SHA2-256 digest
void sha2_256_digest( const void* data, size_t size, uint8_t digest[32] );

// hash data to get 28-bytes SHA2-224 digest
void sha2_224_digest( const void* data, size_t size, uint8_t digest[28] );

// hash data to get 64-bytes SHA2-512 digest
void sha2_512_digest( const void* data, size_t size, uint8_t digest[64] );

// hash data to get 48-bytes SHA2-384 digest
void sha2_384_digest( const void* data, size_t size, uint8_t digest[48] );

// convert binary data to hex-character string, return character count
// WARNING: dest buffer size must >= 2 * data size
// WARNING: dest buffer is not null-terminated
uint32_t to_hexstring( const uint8_t* digest, uint32_t size, char* dstbuf );

// convert binary data to hex-character string
std::string to_hexstring( const uint8_t* digest, uint32_t size );

//
// get digest as hex-character string
//
inline std::string md5_hexdigest( const void* data, size_t size )
{
    uint8_t digest[16];
    md5_digest( data, size, digest );
    return to_hexstring( digest, 16 );
}
inline std::string sha1_hexdigest( const void* data, size_t size )
{
    uint8_t digest[20];
    sha1_digest( data, size, digest );
    return to_hexstring( digest, 20 );
}
inline std::string sha2_256_hexdigest( const void* data, size_t size )
{
    uint8_t digest[32];
    sha2_256_digest( data, size, digest );
    return to_hexstring( digest, 32 );
}
inline std::string sha2_224_hexdigest( const void* data, size_t size )
{
    uint8_t digest[28];
    sha2_224_digest( data, size, digest );
    return to_hexstring( digest, 28 );
}
inline std::string sha2_512_hexdigest( const void* data, size_t size )
{
    uint8_t digest[64];
    sha2_512_digest( data, size, digest );
    return to_hexstring( digest, 64 );
}
inline std::string sha2_384_hexdigest( const void* data, size_t size )
{
    uint8_t digest[48];
    sha2_384_digest( data, size, digest );
    return to_hexstring( digest, 48 );
}

//======================================================================================================================

// max length of final message digest(SHA2-512)
constexpr unsigned int kMaxDigestLen = 64;

// cryptographic hash algorithm
enum class CryptoHashAlg
{
    MD5 = 1,        // 16 byte / 64 bit
    SHA1,           // 20 byte / 160 bit
    SHA2_224,       // 28 byte / 224 bit
    SHA2_256,       // 32 byte / 256 bit
    SHA2_384,       // 48 byte / 384 bit
    SHA2_512,       // 64 byte / 512 bit
};

// base class of cryptographic hasher, used to hash long data
class CryptoHasher : IrkNocopy
{
public:
    virtual ~CryptoHasher() {}

    // the final digest size
    virtual unsigned int digest_size() const = 0;

    // internal block size
    virtual unsigned int block_size() const = 0;

    // reset internal state, begin new hashing
    virtual void reset() = 0;

    // hash data, update digest
    virtual void update( const void* data, size_t size ) = 0;

    // output the final digest
    // WARNING: output buffer size must >= digest_size()
    virtual void get_digest( uint8_t* outbuf ) = 0;

    // get the final digest as hex-character c-string
    // WARNING: output buffer size must >= 2 * digest_size()
    // WARNING: output buffer is not null-terminated
    uint32_t get_hexdigest( char* outbuf )
    {
        uint8_t temp[kMaxDigestLen];
        this->get_digest( temp );
        return to_hexstring( temp, this->digest_size(), outbuf );
    }

    // get the final digest as hex-character std::string
    std::string get_hexdigest()
    {
        uint8_t temp[kMaxDigestLen];
        this->get_digest( temp );
        return to_hexstring( temp, this->digest_size() );
    }

    // fill internal state
    // NOTE: the source hasher must using the same hash algorithm
    virtual void fill_state_from( const CryptoHasher* src ) = 0;
};

// create cryptographic hasher
extern CryptoHasher* create_crypto_hasher( CryptoHashAlg alg, uint32_t flags = 0 );

// delete cryptographic hasher
extern void delete_crypto_hasher( CryptoHasher* );

} // namespace irk
#endif
