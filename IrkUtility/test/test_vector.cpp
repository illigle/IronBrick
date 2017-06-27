#include "gtest/gtest.h"
#include "IrkVector.h"

namespace {

struct Item
{
    Item() : x(0), y(1) {}
    Item( int a, int b ) : x(a), y(b) {}
    int x;
    int y;
};
}

TEST( Container, Vector )
{
    irk::Vector<int> vec(0, 16);
    EXPECT_TRUE( vec.empty() );
    EXPECT_EQ( 0, vec.size() );
    EXPECT_EQ( 0, vec.capacity() );

    vec.reserve( 100 );
    EXPECT_GE( vec.capacity(), 100 );
    vec.shrink( 30 );
    EXPECT_EQ( 30, vec.capacity() );

    vec.resize( 60 );
    EXPECT_FALSE( vec.empty() );
    EXPECT_EQ( 60, vec.size() );
    EXPECT_GE( vec.capacity(), 60 );
    vec.shrink( 10 );
    EXPECT_EQ( 60, vec.size() );
    EXPECT_GE( vec.capacity(), 60 );

    vec.push_back( 0xFF );
    EXPECT_EQ( 61, vec.size() );
    EXPECT_GE( vec.capacity(), 61 );

    vec.clear();
    EXPECT_TRUE( vec.empty() );
    EXPECT_EQ( 0, vec.size() );
    EXPECT_GE( vec.capacity(), 0 );
    vec.shrink( 0 );
    EXPECT_EQ( 0, vec.capacity() );

    int buf[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    vec.assign( buf, 8 );
    EXPECT_EQ( 8, vec.size() );
    EXPECT_GE( vec.capacity(), 8 );

    vec.push_back( buf, 8 );
    EXPECT_EQ( 16, vec.size() );
    EXPECT_GE( vec.capacity(), 16 );
    vec.pop_back( 1 );
    EXPECT_EQ( 7, vec.back() );

    vec.insert( 0, 100 );
    EXPECT_EQ( 100, vec[0] );
    vec.insert( 0, buf, 4 );
    EXPECT_EQ( 1, vec[0] );
    vec.pop_front( 4 );
    EXPECT_EQ( 100, vec[0] );
    vec.insert( 1, 200 );
    EXPECT_EQ( 200, vec[1] );
    vec.insert( vec.size(), -200 );
    EXPECT_EQ( -200, vec.back() );
    vec.insert( 0, -100 );
    EXPECT_EQ( -100, vec[0] );

    vec.remove( [](int x) {return (x >= 1 && x <=8);} );
    for( int x : vec )
        EXPECT_FALSE( (x >= 1 && x <= 8) );

    vec.erase( 1, vec.size() );
    EXPECT_EQ( 1, vec.size() );
    EXPECT_EQ( -100, vec[0] );

    vec.reset( 1024 );
    EXPECT_TRUE( vec.empty() );
    EXPECT_EQ( 0, vec.size() );
    EXPECT_EQ( 1024, vec.capacity() );

    vec.push_back( 1000 );
    EXPECT_EQ( 1, vec.size() );
    int* ptr = vec.alloc( 32 );
    irk::memset32( ptr, 500, 32 );
    vec.commit( 32 );
    EXPECT_EQ( 33, vec.size() );

    ptr = vec.take();
    EXPECT_TRUE( vec.empty() );
    EXPECT_EQ( 0, vec.size() );
    EXPECT_EQ( 0, vec.capacity() );
    irk::aligned_free( ptr );

    irk::Vector<int> vec2( 256 );
    vec = std::move( vec2 );
    EXPECT_EQ( 256, vec.capacity() );
    EXPECT_NE( nullptr, vec.data() );
}

TEST( Container, VecItem )
{
    irk::Vector<Item> vec( 16 );
    vec.resize( 4 );
    for( const auto& item : vec )
        EXPECT_TRUE( item.x == 0 && item.y == 1 );

    irk::Vector<Item> vec2( std::move(vec) );
    EXPECT_EQ( 0, vec.size() );
    EXPECT_EQ( 4, vec2.size() );
    for( const auto& item : vec2 )
        EXPECT_TRUE( item.x == 0 && item.y == 1 );

    vec2.reset(0);
    vec2.push_back( Item{11,22} );
    EXPECT_EQ( 1, vec2.size() );
    EXPECT_EQ( 11, vec2.back().x );
}
