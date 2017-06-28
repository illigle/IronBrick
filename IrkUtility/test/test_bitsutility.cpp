#include <random>
#include <limits.h>
#include "gtest/gtest.h"
#include "IrkBitsUtility.h"

using namespace irk;

TEST( BitsUtility, Bits )
{
    for( int k = 0; k < 31; k++ )
    {
        uint32_t val = 1u << k;
        EXPECT_EQ( 1, bit_get(val, k) );

        bit_reset( &val, k );
        EXPECT_EQ( 0, val );

        bit_set( &val, k );
        EXPECT_EQ( (1u << k), val );

        bit_flip( &val, k );
        EXPECT_EQ( 0, val );

        EXPECT_EQ( 0, bit_get_set(&val, k) );
        EXPECT_EQ( (1u << k), val );

        EXPECT_EQ( 1, bit_get_reset(&val, k) );
        EXPECT_EQ( 0, val );

        EXPECT_EQ( 0, bit_get_flip(&val, k) );
        EXPECT_EQ( (1u << k), val );
    }

    EXPECT_EQ( 32, msb_index(0u) );
    EXPECT_EQ( 32, lsb_index(0u) );
    EXPECT_EQ( 31, msb_index(0xffffffff) );
    EXPECT_EQ( 0, lsb_index(0xffffffff) );
    EXPECT_EQ( 1, bit_mincnt(0u) );
    EXPECT_EQ( 32, bit_mincnt(0xffffffff) );

    for( int k = 0; k < 31; k++ )
    {
        uint32_t val = 1u << k;
        EXPECT_EQ( k, msb_index(val) );
        EXPECT_EQ( k, lsb_index(val) );
        EXPECT_EQ( k, msb_index_unzero(val) );
        EXPECT_EQ( k, lsb_index_unzero(val) );
        EXPECT_EQ( k + 1, bit_mincnt(val) );
    }

    uint8_t buf[4];
    BE_WRITE16( buf, 0xABCD );
    EXPECT_EQ( 0xABCD, BE_READ16(buf) );
    EXPECT_EQ( buf[0], 0xAB );
    EXPECT_EQ( buf[1], 0xCD );

    BE_WRITE32( buf, 0x1234ABCD );
    EXPECT_EQ( 0x1234ABCD, BE_READ32(buf) );
    EXPECT_EQ( 0x1234, BE_READ16(buf) );
    EXPECT_EQ( 0xABCD, BE_READ16(buf+2) );
    EXPECT_EQ( buf[0], 0x12 );
    EXPECT_EQ( buf[1], 0x34 );
    EXPECT_EQ( buf[2], 0xAB );
    EXPECT_EQ( buf[3], 0xCD );

#if defined(_WIN64) || defined(__x86_64__)
    for( int k = 0; k < 63; k++ )
    {
        uint64_t val = 1ull << k;
        EXPECT_EQ( 1, bit_get(val, k) );

        bit_reset( &val, k );
        EXPECT_EQ( 0, val );

        bit_set( &val, k );
        EXPECT_EQ( (1ull << k), val );

        bit_flip( &val, k );
        EXPECT_EQ( 0, val );

        EXPECT_EQ( 0, bit_get_set(&val, k) );
        EXPECT_EQ( (1ull << k), val );

        EXPECT_EQ( 1, bit_get_reset(&val, k) );
        EXPECT_EQ( 0, val );

        EXPECT_EQ( 0, bit_get_flip(&val, k) );
        EXPECT_EQ( (1ull << k), val );
    }
    
    EXPECT_EQ( 64, msb_index(UINT64_C(0)) );
    EXPECT_EQ( 64, lsb_index(UINT64_C(0)) );
    EXPECT_EQ( 63, msb_index(UINT64_C(0xffffffffffffffff)) );
    EXPECT_EQ( 0, lsb_index(UINT64_C(0xffffffffffffffff)) );
    EXPECT_EQ( 1, bit_mincnt(UINT64_C(0)) );
    EXPECT_EQ( 64, bit_mincnt(UINT64_C(0xffffffffffffffff)) );

    for( int k = 0; k < 63; k++ )
    {
        uint64_t val = 1ull << k;
        EXPECT_EQ( k, msb_index(val) );
        EXPECT_EQ( k, lsb_index(val) );
        EXPECT_EQ( k, msb_index_unzero(val) );
        EXPECT_EQ( k, lsb_index_unzero(val) );
        EXPECT_EQ( k + 1, bit_mincnt(val) );
    }

    uint8_t buf2[8];
    BE_WRITE64( buf2, UINT64_C(0x0102030405060708) );
    EXPECT_EQ( UINT64_C(0x0102030405060708), BE_READ64(buf2) );
    EXPECT_EQ( 0x01020304, BE_READ32(buf2) );
    EXPECT_EQ( 0x05060708, BE_READ32(buf2+4) );
    EXPECT_EQ( buf2[0], 0x1 );
    EXPECT_EQ( buf2[1], 0x2 );
    EXPECT_EQ( buf2[2], 0x3 );
    EXPECT_EQ( buf2[3], 0x4 );
    EXPECT_EQ( buf2[4], 0x5 );
    EXPECT_EQ( buf2[5], 0x6 );
    EXPECT_EQ( buf2[6], 0x7 );
    EXPECT_EQ( buf2[7], 0x8 );
#endif
}

TEST( BitsUtility, Reader )
{
    // get random data
    std::random_device rnddev;
    std::mt19937 rng( rnddev() );
    uint8_t buf[64];
    for( auto& v : buf )
        v = static_cast<uint8_t>( rng() );

    BitsReader reader;
    reader.set_buffer( buf, sizeof(buf) );
    EXPECT_EQ( buf, reader.buffer() );
    EXPECT_EQ( sizeof(buf), reader.buffer_size() );

    EXPECT_EQ( BE_READ32(buf) >> 8, reader.read_bits(24) );
    EXPECT_EQ( 24, reader.offset() );
    EXPECT_EQ( bit_get((uint32_t)buf[3], 7), reader.read1() );
    reader.make_byte_aligned();
    EXPECT_EQ( 32, reader.offset() );

    EXPECT_EQ( BE_READ32(buf+4), reader.read_long_bits(32) );
    EXPECT_EQ( BE_READ32(buf+8), reader.read32() );
    reader.skip_bits( 1 );
    EXPECT_EQ( (buf[12] >> 4) & 0x7, reader.read_bits(3) );
    EXPECT_EQ( buf[12] & 0xF, reader.peek_bits(4) );
    EXPECT_EQ( buf[12] & 0xF, reader.read_bits(4) );

    reader.set_offset( 32 );
    uint32_t vv = BE_READ32( buf + 4 );
    for( int i = 0; i < 32; i++ )
    {
        EXPECT_EQ( bit_get(vv, 31-i), reader.read1() );
    }

    reader.skip_bits( 8 );
    EXPECT_EQ( buf[9], reader.read8() );
    reader.read_bits(3);
    EXPECT_EQ( (uint8_t)(buf[10]<<3) | (uint8_t)(buf[11]>>5), reader.peek8() );
}

TEST( BitsUtility, Writer )
{
    // get random data
    std::random_device rnddev;
    std::mt19937 rng( rnddev() );
    uint8_t buf[64];
    for( auto& v : buf )
        v = static_cast<uint8_t>( rng() );

    BitsReader reader( buf, sizeof(buf) );
    BitsWriter writer;

    int bits = 0;
    for( int k = 1; k <= 20; k++ )
    {
        uint32_t v = reader.read_bits( k );
        writer.write_bits( v, k );
        bits += k;
    }
    while( bits & 7 )
    {
        writer.write1( reader.read1() );
        bits++;
    }
    EXPECT_EQ( bits, reader.offset() );
    EXPECT_EQ( bits, writer.bits() );
    EXPECT_EQ( 0, memcmp(buf, writer.buffer(), bits/8) );

    // test append
    do
    {
        reader.set_buffer( buf, sizeof(buf) );
        writer.reset( 128 * 8 );
        const int len1 = rng() & 15;
        const int len2 = rng() & 31;

        for( int k = 1; k <= len1; k++ )
        {
            uint32_t v = reader.read_bits( k );
            writer.write_bits( v, k );
        }
        writer.append( buf, len2 );
        writer.write1( 0 );
        writer.make_byte_aligned( 1 );

        BitsReader wreader( writer.buffer(), writer.bits()/8 );
        reader.set_offset(0);
        for( int k = 1; k <= len1; k++ )
        {
            EXPECT_EQ( reader.read_bits(k), wreader.read_bits(k) );
        }

        reader.set_offset(0);
        for( int k = 1; k <= len2; k++ )    // test appended data
        {
            EXPECT_EQ( reader.read1(), wreader.read1() );
        }
        EXPECT_EQ( 0, wreader.read1() );
    }
    while(0);
}

TEST( BitsUtility, ExpGolomb )
{
    // get random data
    std::random_device rnddev;
    std::mt19937 rng( rnddev() );
    BitsWriter writer;
    BitsReader reader;
    const int kCnt = 16;

    // 8 bit
    do
    {
        std::uniform_int_distribution<uint32_t> dist(0, 254);
        uint8_t buf[kCnt];
        for( auto& v : buf )
            v = static_cast<uint8_t>( dist(rng) );

        writer.clear();
        for( int i = 0; i < kCnt; i++ )
        {
            writer.write_ue( buf[i] );
        }
        writer.make_byte_aligned();  
        reader.set_buffer( writer.buffer(), writer.bits() / 8 );
        for( int i = 0; i < kCnt; i++ )
        {
            EXPECT_EQ( buf[i], reader.read_ue8() );
        }

        writer.clear();
        for( int i = 0; i < kCnt; i++ )
        {
            writer.write_se( (int)(buf[i] - 127) );
        }
        writer.make_byte_aligned();  
        reader.set_buffer( writer.buffer(), writer.bits() / 8 );
        for( int i = 0; i < kCnt; i++ )
        {
            EXPECT_EQ( (int)(buf[i] - 127), reader.read_se8() );
        }
    }
    while(0);

    // 16 bit
    do
    {
        std::uniform_int_distribution<uint32_t> dist(0, UINT16_MAX-1);
        uint16_t buf[kCnt];
        for( auto& v : buf )
            v = static_cast<uint16_t>( dist(rng) );

        writer.clear();
        for( int i = 0; i < kCnt; i++ )
        {
            writer.write_ue( buf[i] );
        }
        writer.make_byte_aligned();  
        reader.set_buffer( writer.buffer(), writer.bits() / 8 );
        for( int i = 0; i < kCnt; i++ )
        {
            EXPECT_EQ( buf[i], reader.read_ue16() );
        }

        writer.clear();
        for( int i = 0; i < kCnt; i++ )
        {
            writer.write_se( (int)(buf[i] - INT16_MAX) );
        }
        writer.make_byte_aligned();  
        reader.set_buffer( writer.buffer(), writer.bits() / 8 );
        for( int i = 0; i < kCnt; i++ )
        {
            EXPECT_EQ( (int)(buf[i] - INT16_MAX), reader.read_se16() );
        }
    }
    while(0);

    // 32 bit
    do
    {
        std::uniform_int_distribution<uint32_t> dist(0, UINT32_MAX-1);
        uint32_t buf[kCnt];
        for( auto& v : buf )
            v = dist(rng);

        writer.clear();
        for( int i = 0; i < kCnt; i++ )
        {
            writer.write_ue( buf[i] );
        }
        writer.make_byte_aligned();  
        reader.set_buffer( writer.buffer(), writer.bits() / 8 );
        for( int i = 0; i < kCnt; i++ )
        {
            EXPECT_EQ( buf[i], reader.read_ue32() );
        }

        writer.clear();
        for( int i = 0; i < kCnt; i++ )
        {
            writer.write_se( (int)(buf[i] - INT32_MAX) );
        }
        writer.make_byte_aligned();  
        reader.set_buffer( writer.buffer(), writer.bits() / 8 );
        for( int i = 0; i < kCnt; i++ )
        {
            EXPECT_EQ( (int)(buf[i] - INT32_MAX), reader.read_se32() );
        }
    }
    while(0);
}
