#include <random>
#include <openssl/sha.h>
#include "gtest/gtest.h"
#include "IrkOnExit.h"
#include "IrkCryptoHash.h"

static void gen_random_data(uint8_t* data, int size, std::minstd_rand& rnd)
{
    int i = 0;
    for (; i <= size - 4; i += 4)
    {
        uint32_t val = rnd();
        data[i] = val & 0xFF;
        data[i + 1] = (val >> 8) & 0xFF;
        data[i + 2] = (val >> 16) & 0xFF;
        data[i + 3] = (val >> 24) & 0xFF;
    }
    for (; i < size; i++)
    {
        data[i] = (uint8_t)(rnd() & 0xFF);
    }
}

TEST(SHA, SHA1)
{
    std::random_device rdev;
    std::minstd_rand rnd(rdev());

    const int size = 1024;
    uint8_t* src = new uint8_t[size];
    gen_random_data(src, size, rnd);
    ON_EXIT(delete[] src; );

    const int len = 20;
    uint8_t dst1[len];
    uint8_t dst2[len];
    memset(dst1, 0, len);
    memset(dst2, 0x7F, len);

    irk::sha1_digest(src, 0, dst1);
    ::SHA1(src, 0, dst2);
    EXPECT_TRUE(memcmp(dst1, dst2, len) == 0);

    irk::sha1_digest(src, 1, dst1);
    ::SHA1(src, 1, dst2);
    EXPECT_TRUE(memcmp(dst1, dst2, len) == 0);

    irk::sha1_digest(src, 63, dst1);
    ::SHA1(src, 63, dst2);
    EXPECT_TRUE(memcmp(dst1, dst2, len) == 0);

    irk::sha1_digest(src, 64, dst1);
    ::SHA1(src, 64, dst2);
    EXPECT_TRUE(memcmp(dst1, dst2, len) == 0);

    const int step = 33 + (int)(rnd() & 31);
    for (int cnt = 2; cnt < size; cnt += step)
    {
        irk::sha1_digest(src, cnt, dst1);
        ::SHA1(src, cnt, dst2);
        EXPECT_TRUE(memcmp(dst1, dst2, len) == 0);
    }
}

TEST(SHA, SHA2_256)
{
    std::random_device rdev;
    std::minstd_rand rnd(rdev());

    const int size = 1024;
    uint8_t* src = new uint8_t[size];
    gen_random_data(src, size, rnd);
    ON_EXIT(delete[] src; );

    const int len = 32;
    uint8_t dst1[len];
    uint8_t dst2[len];
    memset(dst1, 0, len);
    memset(dst2, 0x7F, len);

    irk::sha2_256_digest(src, 0, dst1);
    ::SHA256(src, 0, dst2);
    EXPECT_TRUE(memcmp(dst1, dst2, len) == 0);

    irk::sha2_256_digest(src, 1, dst1);
    ::SHA256(src, 1, dst2);
    EXPECT_TRUE(memcmp(dst1, dst2, len) == 0);

    irk::sha2_256_digest(src, 63, dst1);
    ::SHA256(src, 63, dst2);
    EXPECT_TRUE(memcmp(dst1, dst2, len) == 0);

    irk::sha2_256_digest(src, 64, dst1);
    ::SHA256(src, 64, dst2);
    EXPECT_TRUE(memcmp(dst1, dst2, len) == 0);

    const int step = 33 + (int)(rnd() & 31);
    for (int cnt = 3; cnt < size; cnt += step)
    {
        irk::sha2_256_digest(src, cnt, dst1);
        ::SHA256(src, cnt, dst2);
        EXPECT_TRUE(memcmp(dst1, dst2, len) == 0);
    }
}

TEST(SHA, SHA2_224)
{
    std::random_device rdev;
    std::minstd_rand rnd(rdev());

    const int size = 1024;
    uint8_t* src = new uint8_t[size];
    gen_random_data(src, size, rnd);
    ON_EXIT(delete[] src; );

    const int len = 28;
    uint8_t dst1[len];
    uint8_t dst2[len];
    memset(dst1, 0, len);
    memset(dst2, 0x7F, len);

    irk::sha2_224_digest(src, 0, dst1);
    ::SHA224(src, 0, dst2);
    EXPECT_TRUE(memcmp(dst1, dst2, len) == 0);

    irk::sha2_224_digest(src, 1, dst1);
    ::SHA224(src, 1, dst2);
    EXPECT_TRUE(memcmp(dst1, dst2, len) == 0);

    irk::sha2_224_digest(src, 63, dst1);
    ::SHA224(src, 63, dst2);
    EXPECT_TRUE(memcmp(dst1, dst2, len) == 0);

    irk::sha2_224_digest(src, 65, dst1);
    ::SHA224(src, 65, dst2);
    EXPECT_TRUE(memcmp(dst1, dst2, len) == 0);

    const int step = 33 + (int)(rnd() & 31);
    for (int cnt = 7; cnt < size; cnt += step)
    {
        irk::sha2_224_digest(src, cnt, dst1);
        ::SHA224(src, cnt, dst2);
        EXPECT_TRUE(memcmp(dst1, dst2, len) == 0);
    }
}

TEST(SHA, SHA2_512)
{
    std::random_device rdev;
    std::minstd_rand rnd(rdev());

    const int size = 2048;
    uint8_t* src = new uint8_t[size];
    gen_random_data(src, size, rnd);
    ON_EXIT(delete[] src; );

    const int len = 64;
    uint8_t dst1[len];
    uint8_t dst2[len];
    memset(dst1, 0, len);
    memset(dst2, 0x7F, len);

    irk::sha2_512_digest(src, 0, dst1);
    ::SHA512(src, 0, dst2);
    EXPECT_TRUE(memcmp(dst1, dst2, len) == 0);

    irk::sha2_512_digest(src, 1, dst1);
    ::SHA512(src, 1, dst2);
    EXPECT_TRUE(memcmp(dst1, dst2, len) == 0);

    irk::sha2_512_digest(src, 127, dst1);
    ::SHA512(src, 127, dst2);
    EXPECT_TRUE(memcmp(dst1, dst2, len) == 0);

    irk::sha2_512_digest(src, 128, dst1);
    ::SHA512(src, 128, dst2);
    EXPECT_TRUE(memcmp(dst1, dst2, len) == 0);

    const int step = 33 + (int)(rnd() & 31);
    for (int cnt = 5; cnt < size; cnt += step)
    {
        irk::sha2_512_digest(src, cnt, dst1);
        ::SHA512(src, cnt, dst2);
        EXPECT_TRUE(memcmp(dst1, dst2, len) == 0);
    }
}

TEST(SHA, SHA2_384)
{
    std::random_device rdev;
    std::minstd_rand rnd(rdev());

    const int size = 2048;
    uint8_t* src = new uint8_t[size];
    gen_random_data(src, size, rnd);
    ON_EXIT(delete[] src; );

    const int len = 48;
    uint8_t dst1[len];
    uint8_t dst2[len];
    memset(dst1, 0, len);
    memset(dst2, 0x7F, len);

    irk::sha2_384_digest(src, 0, dst1);
    ::SHA384(src, 0, dst2);
    EXPECT_TRUE(memcmp(dst1, dst2, len) == 0);

    irk::sha2_384_digest(src, 1, dst1);
    ::SHA384(src, 1, dst2);
    EXPECT_TRUE(memcmp(dst1, dst2, len) == 0);

    irk::sha2_384_digest(src, 127, dst1);
    ::SHA384(src, 127, dst2);
    EXPECT_TRUE(memcmp(dst1, dst2, len) == 0);

    irk::sha2_384_digest(src, 129, dst1);
    ::SHA384(src, 129, dst2);
    EXPECT_TRUE(memcmp(dst1, dst2, len) == 0);

    const int step = 33 + (int)(rnd() & 31);
    for (int cnt = 5; cnt < size; cnt += step)
    {
        irk::sha2_384_digest(src, cnt, dst1);
        ::SHA384(src, cnt, dst2);
        EXPECT_TRUE(memcmp(dst1, dst2, len) == 0);
    }
}
