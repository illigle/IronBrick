#include <random>
#include <openssl/hmac.h>
#include "gtest/gtest.h"
#include "IrkOnExit.h"
#include "IrkHMAC.h"

static void gen_random_data( uint8_t* data, int size, std::minstd_rand& rnd )
{
    int i = 0;
    for( ; i <= size - 4; i += 4 )
    {
        uint32_t val = rnd();
        data[i] = val & 0xFF;
        data[i+1] = (val >> 8) & 0xFF;
        data[i+2] = (val >> 16) & 0xFF;
        data[i+3] = (val >> 24) & 0xFF;
    }
    for( ; i < size; i++ )
    {
        data[i] = (uint8_t)(rnd() & 0xFF);
    }
}

TEST( HMAC, HMAC_MD5 )
{
    std::random_device rdev;
    std::minstd_rand rnd( rdev() );

    const int size = 1024;
    uint8_t* src = new uint8_t[size];
    gen_random_data( src, size, rnd );
    ON_EXIT( delete[] src; );

    const int macLen = 16;
    uint8_t dst1[macLen];
    uint8_t dst2[macLen];
    memset( dst1, 0, macLen );
    memset( dst2, 0x7F, macLen );

    uint8_t key[256];
    const int klen = macLen;
    for( int i = 0; i < klen; i++ )
        key[i] = (uint8_t)('0' + i);

    irk::HmacAuth2 hmac( key, klen, irk::CryptoHashAlg::MD5 );
    uint32_t outlen = 0;

    HMAC( EVP_md5(), key, klen, src, 0, dst1, &outlen );
    hmac.reset();
    hmac.update( src, 0 );
    hmac.get_mac( dst2 );
    EXPECT_TRUE( memcmp(dst1, dst2, macLen)==0 );

    HMAC( EVP_md5(), key, klen, src, 1, dst1, &outlen );
    hmac.reset();
    hmac.update( src, 1 );
    hmac.get_mac( dst2 );
    EXPECT_TRUE( memcmp(dst1, dst2, macLen)==0 );

    HMAC( EVP_md5(), key, klen, src, 64, dst1, &outlen );
    hmac.reset();
    hmac.update( src, 64 );
    hmac.get_mac( dst2 );
    EXPECT_TRUE( memcmp(dst1, dst2, macLen)==0 );

    HMAC( EVP_md5(), key, klen, src, 65, dst1, &outlen );
    hmac.reset();
    hmac.update( src, 65 );
    hmac.get_mac( dst2 );
    EXPECT_TRUE( memcmp(dst1, dst2, macLen)==0 );

    const int step = 33 + (int)(rnd() & 31);
    for( int cnt = 3; cnt < size; cnt += step )
    {
        const int klen = (cnt & 0xFF) != 0 ? (cnt & 0xFF) : 11;
        for( int i = 0; i < klen; i++ )
            key[i] = (uint8_t)(rnd() & 0xFF);

        HMAC( EVP_md5(), key, klen, src, cnt, dst1, &outlen );
        EXPECT_EQ( macLen, outlen );
        irk::hmac_md5( key, klen, src, cnt, dst2 );
        EXPECT_TRUE( memcmp(dst1, dst2, macLen)==0 );
    }
}

TEST( HMAC, HMAC_SHA1 )
{
    std::random_device rdev;
    std::minstd_rand rnd( rdev() );

    const int size = 1024;
    uint8_t* src = new uint8_t[size];
    gen_random_data( src, size, rnd );
    ON_EXIT( delete[] src; );

    const int macLen = 20;
    uint8_t dst1[macLen];
    uint8_t dst2[macLen];
    memset( dst1, 0, macLen );
    memset( dst2, 0x7F, macLen );

    uint8_t key[256];
    const int klen = macLen;
    for( int i = 0; i < klen; i++ )
        key[i] = (uint8_t)('0' + i);

    irk::HmacAuth2 hmac( key, klen, irk::CryptoHashAlg::SHA1 );
    uint32_t outlen = 0;

    HMAC( EVP_sha1(), key, klen, src, 0, dst1, &outlen );
    hmac.reset();
    hmac.update( src, 0 );
    hmac.get_mac( dst2 );
    EXPECT_TRUE( memcmp(dst1, dst2, macLen)==0 );

    HMAC( EVP_sha1(), key, klen, src, 1, dst1, &outlen );
    hmac.reset();
    hmac.update( src, 1 );
    hmac.get_mac( dst2 );
    EXPECT_TRUE( memcmp(dst1, dst2, macLen)==0 );

    HMAC( EVP_sha1(), key, klen, src, 64, dst1, &outlen );
    hmac.reset();
    hmac.update( src, 64 );
    hmac.get_mac( dst2 );
    EXPECT_TRUE( memcmp(dst1, dst2, macLen)==0 );

    HMAC( EVP_sha1(), key, klen, src, 65, dst1, &outlen );
    hmac.reset();
    hmac.update( src, 65 );
    hmac.get_mac( dst2 );
    EXPECT_TRUE( memcmp(dst1, dst2, macLen)==0 );

    const int step = 33 + (int)(rnd() & 31);
    for( int cnt = 4; cnt < size; cnt += step )
    {
        const int klen = (cnt & 0xFF) != 0 ? (cnt & 0xFF) : 17;
        for( int i = 0; i < klen; i++ )
            key[i] = (uint8_t)(rnd() & 0xFF);

        HMAC( EVP_sha1(), key, klen, src, cnt, dst1, &outlen );
        EXPECT_EQ( macLen, outlen );
        irk::hmac_sha1( key, klen, src, cnt, dst2 );
        EXPECT_TRUE( memcmp(dst1, dst2, macLen)==0 );
    }
}

TEST( HMAC, HMAC_SHA2_224 )
{
    std::random_device rdev;
    std::minstd_rand rnd( rdev() );

    const int size = 1024;
    uint8_t* src = new uint8_t[size];
    gen_random_data( src, size, rnd );
    ON_EXIT( delete[] src; );

    const int macLen = 28;
    uint8_t dst1[macLen];
    uint8_t dst2[macLen];
    memset( dst1, 0, macLen );
    memset( dst2, 0x7F, macLen );

    uint8_t key[256];
    const int klen = macLen;
    for( int i = 0; i < klen; i++ )
        key[i] = (uint8_t)('0' + i);

    irk::HmacAuth2 hmac( key, klen, irk::CryptoHashAlg::SHA2_224 );
    uint32_t outlen = 0;

    HMAC( EVP_sha224(), key, klen, src, 0, dst1, &outlen );
    hmac.reset();
    hmac.update( src, 0 );
    hmac.get_mac( dst2 );
    EXPECT_TRUE( memcmp(dst1, dst2, macLen)==0 );

    HMAC( EVP_sha224(), key, klen, src, 1, dst1, &outlen );
    hmac.reset();
    hmac.update( src, 1 );
    hmac.get_mac( dst2 );
    EXPECT_TRUE( memcmp(dst1, dst2, macLen)==0 );

    HMAC( EVP_sha224(), key, klen, src, 64, dst1, &outlen );
    hmac.reset();
    hmac.update( src, 64 );
    hmac.get_mac( dst2 );
    EXPECT_TRUE( memcmp(dst1, dst2, macLen)==0 );

    HMAC( EVP_sha224(), key, klen, src, 65, dst1, &outlen );
    hmac.reset();
    hmac.update( src, 65 );
    hmac.get_mac( dst2 );
    EXPECT_TRUE( memcmp(dst1, dst2, macLen)==0 );

    const int step = 33 + (int)(rnd() & 31);
    for( int cnt = 8; cnt < size; cnt += step )
    {
        const int klen = (cnt & 0xFF) != 0 ? (cnt & 0xFF) : 21;
        for( int i = 0; i < klen; i++ )
            key[i] = (uint8_t)(rnd() & 0xFF);

        HMAC( EVP_sha224(), key, klen, src, cnt, dst1, &outlen );
        EXPECT_EQ( macLen, outlen );
        irk::hmac_sha2_224( key, klen, src, cnt, dst2 );
        EXPECT_TRUE( memcmp(dst1, dst2, macLen)==0 );
    }
}

TEST( HMAC, HMAC_SHA2_256 )
{
    std::random_device rdev;
    std::minstd_rand rnd( rdev() );

    const int size = 1024;
    uint8_t* src = new uint8_t[size];
    gen_random_data( src, size, rnd );
    ON_EXIT( delete[] src; );

    const int macLen = 32;
    uint8_t dst1[macLen];
    uint8_t dst2[macLen];
    memset( dst1, 0, macLen );
    memset( dst2, 0x7F, macLen );

    uint8_t key[256];
    const int klen = macLen;
    for( int i = 0; i < klen; i++ )
        key[i] = (uint8_t)('0' + i);

    irk::HmacAuth2 hmac( key, klen, irk::CryptoHashAlg::SHA2_256 );
    uint32_t outlen = 0;

    HMAC( EVP_sha256(), key, klen, src, 0, dst1, &outlen );
    hmac.reset();
    hmac.update( src, 0 );
    hmac.get_mac( dst2 );
    EXPECT_TRUE( memcmp(dst1, dst2, macLen)==0 );

    HMAC( EVP_sha256(), key, klen, src, 1, dst1, &outlen );
    hmac.reset();
    hmac.update( src, 1 );
    hmac.get_mac( dst2 );
    EXPECT_TRUE( memcmp(dst1, dst2, macLen)==0 );

    HMAC( EVP_sha256(), key, klen, src, 64, dst1, &outlen );
    hmac.reset();
    hmac.update( src, 64 );
    hmac.get_mac( dst2 );
    EXPECT_TRUE( memcmp(dst1, dst2, macLen)==0 );

    HMAC( EVP_sha256(), key, klen, src, 65, dst1, &outlen );
    hmac.reset();
    hmac.update( src, 65 );
    hmac.get_mac( dst2 );
    EXPECT_TRUE( memcmp(dst1, dst2, macLen)==0 );

    const int step = 33 + (int)(rnd() & 31);
    for( int cnt = 7; cnt < size; cnt += step )
    {
        const int klen = (cnt & 0xFF) != 0 ? (cnt & 0xFF) : 31;
        for( int i = 0; i < klen; i++ )
            key[i] = (uint8_t)(rnd() & 0xFF);

        HMAC( EVP_sha256(), key, klen, src, cnt, dst1, &outlen );
        EXPECT_EQ( macLen, outlen );
        irk::hmac_sha2_256( key, klen, src, cnt, dst2 );
        EXPECT_TRUE( memcmp(dst1, dst2, macLen)==0 );
    }
}

TEST( HMAC, HMAC_SHA2_384 )
{
    std::random_device rdev;
    std::minstd_rand rnd( rdev() );

    const int size = 1024;
    uint8_t* src = new uint8_t[size];
    gen_random_data( src, size, rnd );
    ON_EXIT( delete[] src; );

    const int macLen = 48;
    uint8_t dst1[macLen];
    uint8_t dst2[macLen];
    memset( dst1, 0, macLen );
    memset( dst2, 0x7F, macLen );

    uint8_t key[256];
    const int klen = macLen;
    for( int i = 0; i < klen; i++ )
        key[i] = (uint8_t)('0' + i);

    irk::HmacAuth2 hmac( key, klen, irk::CryptoHashAlg::SHA2_384 );
    uint32_t outlen = 0;

    HMAC( EVP_sha384(), key, klen, src, 0, dst1, &outlen );
    hmac.reset();
    hmac.update( src, 0 );
    hmac.get_mac( dst2 );
    EXPECT_TRUE( memcmp(dst1, dst2, macLen)==0 );

    HMAC( EVP_sha384(), key, klen, src, 1, dst1, &outlen );
    hmac.reset();
    hmac.update( src, 1 );
    hmac.get_mac( dst2 );
    EXPECT_TRUE( memcmp(dst1, dst2, macLen)==0 );

    HMAC( EVP_sha384(), key, klen, src, 127, dst1, &outlen );
    hmac.reset();
    hmac.update( src, 127 );
    hmac.get_mac( dst2 );
    EXPECT_TRUE( memcmp(dst1, dst2, macLen)==0 );

    HMAC( EVP_sha384(), key, klen, src, 128, dst1, &outlen );
    hmac.reset();
    hmac.update( src, 128 );
    hmac.get_mac( dst2 );
    EXPECT_TRUE( memcmp(dst1, dst2, macLen)==0 );

    const int step = 33 + (int)(rnd() & 31);
    for( int cnt = 13; cnt < size; cnt += step )
    {
        const int klen = (cnt & 0xFF) != 0 ? (cnt & 0xFF) : 41;
        for( int i = 0; i < klen; i++ )
            key[i] = (uint8_t)(rnd() & 0xFF);

        HMAC( EVP_sha384(), key, klen, src, cnt, dst1, &outlen );
        EXPECT_EQ( macLen, outlen );
        irk::hmac_sha2_384( key, klen, src, cnt, dst2 );
        EXPECT_TRUE( memcmp(dst1, dst2, macLen)==0 );
    }
}

TEST( HMAC, HMAC_SHA2_512 )
{
    std::random_device rdev;
    std::minstd_rand rnd( rdev() );

    const int size = 1024;
    uint8_t* src = new uint8_t[size];
    gen_random_data( src, size, rnd );
    ON_EXIT( delete[] src; );

    const int macLen = 64;
    uint8_t dst1[macLen];
    uint8_t dst2[macLen];
    memset( dst1, 0, macLen );
    memset( dst2, 0x7F, macLen );

    uint8_t key[256];
    const int klen = macLen;
    for( int i = 0; i < klen; i++ )
        key[i] = (uint8_t)('0' + i);

    irk::HmacAuth2 hmac( key, klen, irk::CryptoHashAlg::SHA2_512 );
    uint32_t outlen = 0;

    HMAC( EVP_sha512(), key, klen, src, 0, dst1, &outlen );
    hmac.reset();
    hmac.update( src, 0 );
    hmac.get_mac( dst2 );
    EXPECT_TRUE( memcmp(dst1, dst2, macLen)==0 );

    HMAC( EVP_sha512(), key, klen, src, 1, dst1, &outlen );
    hmac.reset();
    hmac.update( src, 1 );
    hmac.get_mac( dst2 );
    EXPECT_TRUE( memcmp(dst1, dst2, macLen)==0 );

    HMAC( EVP_sha512(), key, klen, src, 127, dst1, &outlen );
    hmac.reset();
    hmac.update( src, 127 );
    hmac.get_mac( dst2 );
    EXPECT_TRUE( memcmp(dst1, dst2, macLen)==0 );

    HMAC( EVP_sha512(), key, klen, src, 128, dst1, &outlen );
    hmac.reset();
    hmac.update( src, 128 );
    hmac.get_mac( dst2 );
    EXPECT_TRUE( memcmp(dst1, dst2, macLen)==0 );

    const int step = 33 + (int)(rnd() & 31);
    for( int cnt = 3; cnt < size; cnt += step )
    {
        const int klen = (cnt & 0xFF) != 0 ? (cnt & 0xFF) : 56;
        for( int i = 0; i < klen; i++ )
            key[i] = (uint8_t)(rnd() & 0xFF);

        HMAC( EVP_sha512(), key, klen, src, cnt, dst1, &outlen );
        EXPECT_EQ( macLen, outlen );
        irk::hmac_sha2_512( key, klen, src, cnt, dst2 );
        EXPECT_TRUE( memcmp(dst1, dst2, macLen)==0 );
    }
}
