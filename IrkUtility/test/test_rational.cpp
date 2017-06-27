#include "gtest/gtest.h"
#include "IrkRational.h"

using namespace irk;

TEST( Rational, Compare )
{
    Rational r1 = { 1, 2 };
    Rational r2 = { 3, 6 };
    Rational r3 = { 2, 3 };
    Rational r4 = { -2, -3 };
    Rational r5 = { -2, 5 };
    Rational r6 = { 2, -5 };

    EXPECT_EQ( r1, r2 );
    EXPECT_EQ( r3, r4 );
    EXPECT_EQ( r5, r6 );
    EXPECT_NE( r1, r3 );
    EXPECT_NE( r3, r5 );
    EXPECT_NE( r5, r1 );
    EXPECT_LT( r1, r3 );
    EXPECT_GT( r4, r2 );
    EXPECT_LT( r5, r1 );
    EXPECT_GT( r4, r6 );
    EXPECT_LT( (Rational{-1,2}), (Rational{1,3}) );
    EXPECT_LT( (Rational{-1,2}), (Rational{-1,3}) );
    EXPECT_LT( (Rational{-1,2}), (Rational{1,-3}) );
    EXPECT_LT( (Rational{-1,2}), (Rational{-1,-3}) );
    EXPECT_LT( (Rational{1,-2}), (Rational{1,3}) );
    EXPECT_LT( (Rational{1,-2}), (Rational{-1,3}) );
    EXPECT_LT( (Rational{1,-2}), (Rational{1,-3}) );
    EXPECT_LT( (Rational{1,-2}), (Rational{-1,-3}) );
    EXPECT_GT( (Rational{-1,-2}), (Rational{1,3}) );
    EXPECT_GT( (Rational{-1,-2}), (Rational{-1,3}) );
    EXPECT_GT( (Rational{-1,-2}), (Rational{1,-3}) );
    EXPECT_GT( (Rational{-1,-2}), (Rational{-1,-3}) );
}

TEST( Rational, Abs )
{
    Rational r1{ 1, 2 };
    Rational r2{ -3, 2 };
    Rational r3{ 2, -3 };
    Rational r4{ -2, -3 };
    EXPECT_EQ( (Rational{1,2}), to_abs( r1 ) );
    EXPECT_EQ( (Rational{3,2}), to_abs( r2 ) );
    EXPECT_EQ( (Rational{2,3}), to_abs( r3 ) );
    EXPECT_EQ( (Rational{2,3}), to_abs( r4 ) );
}

TEST( Rational, Normalize )
{
    Rational r1 = { 1, 2 };
    Rational r2 = { -3, -6 };
    Rational r3 = { -8, 4 };
    Rational r4 = { -117, 117 };

    normalize( r1 );
    normalize( r2 );
    normalize( r3 );
    normalize( r4 );
    EXPECT_EQ( r1, r2 );
    EXPECT_EQ( (Rational{2,-1}), r3 );
    EXPECT_EQ( (Rational{-1,1}), r4 );
}

TEST( Rational, ToDouble )
{
    Rational r1{ 1, 2 };
    Rational r2{ -3, 2 };
    Rational r3{ -2, -3 };
    Rational r4{ -2, 5 };
    EXPECT_DOUBLE_EQ( 0.5,      to_double(r1) );
    EXPECT_DOUBLE_EQ( -1.5f,    to_float(r2) );
    EXPECT_DOUBLE_EQ( 2.0/3.0,  to_double(r3) );
    EXPECT_DOUBLE_EQ( -2.0/5.0, to_double(r4) );
}
