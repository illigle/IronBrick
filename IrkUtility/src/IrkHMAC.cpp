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

#include "IrkHMAC.h"

namespace irk {

constexpr unsigned int kMaxHashBlockSize = 128; // block size of SHA2-512

static const uint8_t s_iPad[kMaxHashBlockSize] =
{
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
};

static const uint8_t s_oPad[kMaxHashBlockSize] =
{
    0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
    0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
    0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
    0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
    0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
    0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
    0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
    0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
};

HmacAuth::HmacAuth(CryptoHashAlg alg, uint32_t flags) : m_iHasher(nullptr), m_oHasher(nullptr)
{
    m_iHasher = create_crypto_hasher(alg, flags);
    m_oHasher = create_crypto_hasher(alg, flags);
    m_macSize = m_iHasher->digest_size();
    m_blockSize = m_iHasher->block_size();
    assert(m_macSize <= kMaxHmacLen && m_blockSize <= kMaxHashBlockSize);
}

HmacAuth::~HmacAuth()
{
    delete_crypto_hasher(m_iHasher);
    delete_crypto_hasher(m_oHasher);
}

// reset internal state, begin new authentication
void HmacAuth::reset(const void* okey, uint32_t klen)
{
    uint8_t temp[kMaxHashBlockSize];
    const uint8_t* key = (const uint8_t*)okey;

    // if key longer than hash block size, hash it to get actual key
    if (klen > m_blockSize)
    {
        m_iHasher->reset();
        m_iHasher->update(key, klen);
        m_iHasher->get_digest(temp);
        key = temp;
        klen = m_macSize;
    }
    assert(klen > 0 && klen <= m_blockSize);

    for (uint32_t i = 0; i < klen; i++)
        temp[i] = key[i] ^ 0x36;
    m_iHasher->reset();
    m_iHasher->update(temp, klen);
    m_iHasher->update(s_iPad, m_blockSize - klen);

    for (uint32_t i = 0; i < klen; i++)
        temp[i] ^= (0x36 ^ 0x5c);
    m_oHasher->reset();
    m_oHasher->update(temp, klen);
    m_oHasher->update(s_oPad, m_blockSize - klen);
}

// input data, update message authentication codes
void HmacAuth::update(const void* data, size_t size)
{
    m_iHasher->update(data, size);
}

// get the final message authentication codes
// WARNING: output buffer size must >= mac_size()
void HmacAuth::get_mac(uint8_t* outbuf)
{
    uint8_t iDigest[kMaxHmacLen];
    m_iHasher->get_digest(iDigest);
    m_oHasher->update(iDigest, m_macSize);
    m_oHasher->get_digest(outbuf);
}

//======================================================================================================================
HmacAuth2::HmacAuth2(const void* okey, uint32_t klen, CryptoHashAlg alg, uint32_t flags)
    : m_iKeyHasher(nullptr), m_oKeyHasher(nullptr), m_iHasher(nullptr), m_oHasher(nullptr)
{
    m_iKeyHasher = create_crypto_hasher(alg, flags);
    m_oKeyHasher = create_crypto_hasher(alg, flags);
    m_macSize = m_iKeyHasher->digest_size();
    m_blockSize = m_iKeyHasher->block_size();
    assert(m_macSize <= kMaxHmacLen && m_blockSize <= kMaxHashBlockSize);

    uint8_t temp[kMaxHashBlockSize];
    const uint8_t* key = (const uint8_t*)okey;

    // if key longer than hash block size, hash it to get actual key
    if (klen > m_blockSize)
    {
        m_iKeyHasher->reset();
        m_iKeyHasher->update(key, klen);
        m_iKeyHasher->get_digest(temp);
        key = temp;
        klen = m_macSize;
    }
    assert(klen > 0 && klen <= m_blockSize);

    // hash key
    for (uint32_t i = 0; i < klen; i++)
        temp[i] = key[i] ^ 0x36;
    m_iKeyHasher->reset();
    m_iKeyHasher->update(temp, klen);
    m_iKeyHasher->update(s_iPad, m_blockSize - klen);

    for (uint32_t i = 0; i < klen; i++)
        temp[i] ^= (0x36 ^ 0x5c);
    m_oKeyHasher->reset();
    m_oKeyHasher->update(temp, klen);
    m_oKeyHasher->update(s_oPad, m_blockSize - klen);

    m_iHasher = create_crypto_hasher(alg, flags);
    m_oHasher = create_crypto_hasher(alg, flags);
}

HmacAuth2::~HmacAuth2()
{
    delete_crypto_hasher(m_iKeyHasher);
    delete_crypto_hasher(m_oKeyHasher);
    delete_crypto_hasher(m_iHasher);
    delete_crypto_hasher(m_oHasher);
}

// reset internal state, begin new authentication
void HmacAuth2::reset()
{
    m_iHasher->fill_state_from(m_iKeyHasher);
    m_oHasher->fill_state_from(m_oKeyHasher);
}

// input data, update message authentication codes
void HmacAuth2::update(const void* data, size_t size)
{
    m_iHasher->update(data, size);
}

// get the final message authentication codes
// WARNING: output buffer size must >= mac_size()
void HmacAuth2::get_mac(uint8_t* outbuf)
{
    uint8_t iDigest[kMaxHmacLen];
    m_iHasher->get_digest(iDigest);
    m_oHasher->update(iDigest, m_macSize);
    m_oHasher->get_digest(outbuf);
}

//======================================================================================================================

// get HMAC-MD5 Authentication Codes
void hmac_md5(const void* key, uint32_t klen, const void* data, size_t size, uint8_t mac[16])
{
    HmacAuth hmac(CryptoHashAlg::MD5);
    hmac.reset(key, klen);
    hmac.update(data, size);
    hmac.get_mac(mac);
}

// get HMAC-SHA1 Authentication Codes
void hmac_sha1(const void* key, uint32_t klen, const void* data, size_t size, uint8_t mac[20])
{
    HmacAuth hmac(CryptoHashAlg::SHA1);
    hmac.reset(key, klen);
    hmac.update(data, size);
    hmac.get_mac(mac);
}

// get HMAC-SHA2-224 Authentication Codes
void hmac_sha2_224(const void* key, uint32_t klen, const void* data, size_t size, uint8_t mac[28])
{
    HmacAuth hmac(CryptoHashAlg::SHA2_224);
    hmac.reset(key, klen);
    hmac.update(data, size);
    hmac.get_mac(mac);
}

// get HMAC-SHA2-256 Authentication Codes
void hmac_sha2_256(const void* key, uint32_t klen, const void* data, size_t size, uint8_t mac[32])
{
    HmacAuth hmac(CryptoHashAlg::SHA2_256);
    hmac.reset(key, klen);
    hmac.update(data, size);
    hmac.get_mac(mac);
}

// get HMAC-SHA2-384 Authentication Codes
void hmac_sha2_384(const void* key, uint32_t klen, const void* data, size_t size, uint8_t mac[48])
{
    HmacAuth hmac(CryptoHashAlg::SHA2_384);
    hmac.reset(key, klen);
    hmac.update(data, size);
    hmac.get_mac(mac);
}

// get HMAC-SHA2-512 Authentication Codes
void hmac_sha2_512(const void* key, uint32_t klen, const void* data, size_t size, uint8_t mac[64])
{
    HmacAuth hmac(CryptoHashAlg::SHA2_512);
    hmac.reset(key, klen);
    hmac.update(data, size);
    hmac.get_mac(mac);
}

} // namespace irk
