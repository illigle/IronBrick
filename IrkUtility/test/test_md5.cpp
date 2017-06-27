#include <random>
#include <openssl/md5.h>
#include "gtest/gtest.h"
#include "IrkOnExit.h"
#include "IrkCryptoHash.h"

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

TEST( MD5, MD5 )
{
    std::random_device rdev;
    std::minstd_rand rnd( rdev() );

    const int size = 1024;
    uint8_t* src = new uint8_t[size];
    gen_random_data( src, size, rnd );
    ON_EXIT( delete[] src; );

    uint8_t dst1[16];
    uint8_t dst2[16];
    memset( dst1, 0, 16 );
    memset( dst2, 0x7F, 16 );

    irk::md5_digest( src, 0, dst1 );
    ::MD5( src, 0, dst2 );
    EXPECT_TRUE( memcmp(dst1, dst2, 16)==0 );

    irk::md5_digest( src, 1, dst1 );
    ::MD5( src, 1, dst2 );
    EXPECT_TRUE( memcmp(dst1, dst2, 16)==0 );

    irk::md5_digest( src, 63, dst1 );
    ::MD5( src, 63, dst2 );
    EXPECT_TRUE( memcmp(dst1, dst2, 16)==0 );

    irk::md5_digest( src, 64, dst1 );
    ::MD5( src, 64, dst2 );
    EXPECT_TRUE( memcmp(dst1, dst2, 16)==0 );

    const int step = 33 + (int)(rnd() & 31);
    for( int cnt = 2; cnt < size; cnt += step )
    {
        irk::md5_digest( src, cnt, dst1 );
        ::MD5( src, cnt, dst2 );
        EXPECT_TRUE( memcmp(dst1, dst2, 16)==0 );
    }
}
