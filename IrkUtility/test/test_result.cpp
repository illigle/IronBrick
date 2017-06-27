#include <string>
#include <memory>
#include <vector>
#include <system_error>
#include "gtest/gtest.h"
#include "IrkResult.h"

using namespace std;
using namespace irk;

TEST( Result, base )
{
    Result<string,int> r;
    EXPECT_FALSE( r.has_value() );
    EXPECT_FALSE( r.has_error() );
    EXPECT_EQ( nullptr, r.try_get_error() );
    EXPECT_EQ( nullptr, r.try_get_value() );

    // set error
    r.set_error( -1 );
    EXPECT_FALSE( r.has_value() );
    EXPECT_TRUE( r.has_error() );
    EXPECT_EQ( -1, r.error() );
    r = 100;
    EXPECT_EQ( 100, r.error() );
    EXPECT_THROW( r.value(), ResultBadAccess );
    EXPECT_NE( nullptr, r.try_get_error() );
    EXPECT_EQ( nullptr, r.try_get_value() );

    // set value
    r.set_value( "123" );
    EXPECT_TRUE( r.has_value() );
    EXPECT_FALSE( r.has_error() );
    EXPECT_STREQ( "123", r.value().c_str() );
    r = string("abc");
    EXPECT_EQ( string("abc"), r.value() );
    EXPECT_THROW( r.error(), ResultBadAccess );
    EXPECT_NE( nullptr, r.try_get_value() );
    EXPECT_EQ( nullptr, r.try_get_error() );

    // reset
    r.reset();
    EXPECT_FALSE( r.has_value() );
    EXPECT_FALSE( r.has_error() );
    EXPECT_THROW( r.value(), ResultBadAccess );
    EXPECT_THROW( r.error(), ResultBadAccess );

    {
        Result<string,int> r2("value");
        EXPECT_TRUE( r2.has_value() );
        EXPECT_STREQ( "value", r2.value().c_str() );
    }
    {
        Result<string,int> r3(-5);
        EXPECT_TRUE( r3.has_error() );
        EXPECT_EQ( -5, r3.error() );
    }
    {
        auto r4 = make_success_result<string,int>( "success" );
        EXPECT_TRUE( r4.has_value() );
        auto pv = r4.try_get_value();
        ASSERT_NE( nullptr, pv );
        EXPECT_EQ( string("success"), *pv );
    }
    {
        auto r5 = make_error_result<string,int>( -1 );
        EXPECT_TRUE( r5.has_error() );
        auto pe = r5.try_get_error();
        ASSERT_NE( nullptr, pe );
        EXPECT_EQ( -1, *pe );
    }
    {
        Result<vector<int>,error_condition> r6;
        r6.emplace_value( 4, -100 );
        EXPECT_EQ( 4u, r6.value().size() );
        r6.emplace_error( std::errc::bad_file_descriptor );
        EXPECT_EQ( std::errc::bad_file_descriptor, r6.error() );
    }
}

TEST( Result, copy_move_assign )
{
    Result<string,int> nil;
    Result<string,int> r2;
    EXPECT_EQ(nil, r2);

    auto r3 = make_success_result<string,int>( "123" );
    EXPECT_NE(nil, r3);
    EXPECT_NE(r2, r3);

    Result<string,int> r4( r3 );    // copy
    EXPECT_EQ(r3, r4);
    EXPECT_EQ(string("123"), r3.value());
    EXPECT_EQ(string("123"), r4.value());
    r4 = -1;    // set error
    EXPECT_NE(r3, r4);

    Result<string,int> r5( std::move(r3) ); // move
    EXPECT_EQ(string("123"), r5.value());
    EXPECT_EQ( nil, r3 );       // r3 moved

    r3 = r4;                    // assign
    EXPECT_EQ(r3, r4);
    EXPECT_EQ(-1, r3.error());
    EXPECT_EQ(-1, r4.error());

    r2 = std::move(r5);         // move assignment
    EXPECT_EQ(string("123"), r2.value());
    EXPECT_NE(r2, r5);
    EXPECT_EQ(nil, r5);         // r5 moved
    EXPECT_FALSE( r5.has_value() );
}

TEST( Result, insane )
{
    Result<unique_ptr<int[]>,error_code> r;     // store move-only data type
    r.set_value( std::make_unique<int[]>(8) );
    unique_ptr<int[]> ubuf = std::move( r.value() );
    EXPECT_FALSE( r.value() );
    EXPECT_TRUE( ubuf );

    r.set_error( std::make_error_code(std::errc::invalid_argument) );
    EXPECT_FALSE( r.has_value() );
    EXPECT_TRUE( r.has_error() );
}

TEST( Optional, base )
{
    Optional<string> r;
    EXPECT_FALSE( r.has_value() );
    EXPECT_EQ( nullptr, r.try_get_value() );
    EXPECT_THROW( r.value(), ResultBadAccess );

    // set value
    r.set_value( "123" );
    EXPECT_TRUE( r.has_value() );
    EXPECT_STREQ( "123", r.value().c_str() );
    r = string("abc");
    EXPECT_EQ( string("abc"), r.value() );
    EXPECT_NE( nullptr, r.try_get_value() );

    // reset
    r.reset();
    EXPECT_FALSE( r.has_value() );
    EXPECT_THROW( r.value(), ResultBadAccess );

    {
        Optional<string> r2("value");
        EXPECT_TRUE( r2.has_value() );
        EXPECT_STREQ( "value", r2.value().c_str() );
    }
    {
        auto r3 = make_optional_result<string>( "success" );
        EXPECT_TRUE( r3.has_value() );
        auto pv = r3.try_get_value();
        ASSERT_NE( nullptr, pv );
        EXPECT_EQ( string("success"), *pv );
    }
    {
        Optional<vector<int>> r6;
        r6.emplace_value( 4, -100 );
        EXPECT_EQ( 4u, r6.value().size() );
    }
}

TEST( Optional, copy_move_assign )
{
    Optional<string> nil;
    Optional<string> r2;
    EXPECT_EQ(nil, r2);

    auto r3 = make_optional_result<string>( "123" );
    EXPECT_NE(nil, r3);
    EXPECT_NE(r2, r3);

    Optional<string> r4( r3 );    // copy
    EXPECT_EQ(r3, r4);
    EXPECT_EQ(string("123"), r3.value());
    EXPECT_EQ(string("123"), r4.value());

    Optional<string> r5( std::move(r3) );   // move
    EXPECT_EQ(string("123"), r5.value());
    EXPECT_EQ( nil, r3 );   // r3 moved

    r4 = string("abc");
    r3 = r4;                // assign
    EXPECT_EQ(r3, r4);
    EXPECT_EQ(string("abc"), r3.value());

    r2 = std::move(r5);     // move assignment
    EXPECT_EQ(string("123"), r2.value());
    EXPECT_NE(r2, r5);
    EXPECT_EQ(nil, r5);     // r5 moved
    EXPECT_FALSE( r5.has_value() );
}

TEST( Optional, insane )
{
    Optional<unique_ptr<int[]>> r;     // store move-only data type
    r.set_value( std::make_unique<int[]>(8) );
    unique_ptr<int[]> ubuf = std::move( r.value() );
    EXPECT_FALSE( r.value() );
    EXPECT_TRUE( ubuf );
}
