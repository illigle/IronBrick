#include <ctype.h>
#include <wctype.h>
#include <locale>
#include <limits.h>
#include "gtest/gtest.h"
#include "IrkCType.h"

using namespace irk;

TEST( CType, ascii )
{
    ::setlocale( LC_CTYPE, "C" );

    EXPECT_FALSE( CTypeTraits<char>::is_cntrl('\0') );  // WARNING: 0 is not deemed as control char

    for( int i = 1; i <= CHAR_MAX; i++ )
    {
        char c = static_cast<char>( i );

        if( ::isalpha(c) )
            EXPECT_TRUE( CTypeTraits<char>::is_alpha(c) );
        if( ::isalnum(c) )
            EXPECT_TRUE( CTypeTraits<char>::is_alnum(c) );
        if( ::islower(c) )
            EXPECT_TRUE( CTypeTraits<char>::is_lower(c) );
        if( ::isupper(c) )
            EXPECT_TRUE( CTypeTraits<char>::is_upper(c) );
        if( ::isdigit(c) )
            EXPECT_TRUE( CTypeTraits<char>::is_digit(c) );
        if( ::isxdigit(c) )
            EXPECT_TRUE( CTypeTraits<char>::is_xdigit(c) );
        if( ::isspace(c) )
            EXPECT_TRUE( CTypeTraits<char>::is_space(c) );
        if( ::iscntrl(c) )
            EXPECT_TRUE( CTypeTraits<char>::is_cntrl(c) );
        if( ::ispunct(c) )
            EXPECT_TRUE( CTypeTraits<char>::is_punct(c) );
        if( ::isspace(c) || ::iscntrl(c) )
            EXPECT_TRUE( CTypeTraits<char>::is_ctlws(c) );
    }

    for( int i = CHAR_MIN; i < 0; i++ )
    {
        char c = static_cast<char>( i );
        EXPECT_FALSE( CTypeTraits<char>::is_alpha(c) );
        EXPECT_FALSE( CTypeTraits<char>::is_alnum(c) );
        EXPECT_FALSE( CTypeTraits<char>::is_lower(c) );
        EXPECT_FALSE( CTypeTraits<char>::is_upper(c) );
        EXPECT_FALSE( CTypeTraits<char>::is_digit(c) );
        EXPECT_FALSE( CTypeTraits<char>::is_xdigit(c) );
        EXPECT_FALSE( CTypeTraits<char>::is_space(c) );
        EXPECT_FALSE( CTypeTraits<char>::is_cntrl(c) );
        EXPECT_FALSE( CTypeTraits<char>::is_punct(c) );
        EXPECT_FALSE( CTypeTraits<char>::is_ctlws(c) );
    }
}

TEST( CType, wchar )
{
    ::setlocale( LC_CTYPE, "C" );

    EXPECT_FALSE( CTypeTraits<wchar_t>::is_cntrl(L'\0') );  // WARNING: 0 is not deemed as control char

    for( int i = 1; i <= CHAR_MAX; i++ )
    {
        wchar_t c = static_cast<wchar_t>( i );

        if( ::iswalpha(c) )
            EXPECT_TRUE( CTypeTraits<wchar_t>::is_alpha(c) );
        if( ::iswalnum(c) )
            EXPECT_TRUE( CTypeTraits<wchar_t>::is_alnum(c) );
        if( ::iswlower(c) )
            EXPECT_TRUE( CTypeTraits<wchar_t>::is_lower(c) );
        if( ::iswupper(c) )
            EXPECT_TRUE( CTypeTraits<wchar_t>::is_upper(c) );
        if( ::iswdigit(c) )
            EXPECT_TRUE( CTypeTraits<wchar_t>::is_digit(c) );
        if( ::iswxdigit(c) )
            EXPECT_TRUE( CTypeTraits<wchar_t>::is_xdigit(c) );
        if( ::iswspace(c) )
            EXPECT_TRUE( CTypeTraits<wchar_t>::is_space(c) );
        if( ::iswcntrl(c) )
            EXPECT_TRUE( CTypeTraits<wchar_t>::is_cntrl(c) );
        if( ::iswpunct(c) )
            EXPECT_TRUE( CTypeTraits<wchar_t>::is_punct(c) );
        if( ::iswspace(c) || ::iscntrl(c) )
            EXPECT_TRUE( CTypeTraits<wchar_t>::is_ctlws(c) );
    }

    for( int i = CHAR_MAX + 1; i < 300; i++ )
    {
        wchar_t c = static_cast<wchar_t>( i );
        EXPECT_FALSE( CTypeTraits<wchar_t>::is_alpha(c) );
        EXPECT_FALSE( CTypeTraits<wchar_t>::is_alnum(c) );
        EXPECT_FALSE( CTypeTraits<wchar_t>::is_lower(c) );
        EXPECT_FALSE( CTypeTraits<wchar_t>::is_upper(c) );
        EXPECT_FALSE( CTypeTraits<wchar_t>::is_digit(c) );
        EXPECT_FALSE( CTypeTraits<wchar_t>::is_xdigit(c) );
        EXPECT_FALSE( CTypeTraits<wchar_t>::is_space(c) );
        EXPECT_FALSE( CTypeTraits<wchar_t>::is_cntrl(c) );
        EXPECT_FALSE( CTypeTraits<wchar_t>::is_punct(c) );
        EXPECT_FALSE( CTypeTraits<wchar_t>::is_ctlws(c) );
    }
}

TEST( CType, utf16 )
{
    ::setlocale( LC_CTYPE, "C" );

    EXPECT_FALSE( CTypeTraits<char16_t>::is_cntrl(u'\0') ); // WARNING: 0 is not deemed as control char

    for( char16_t c = u'a'; c <= u'z'; c++ )
    {
        EXPECT_TRUE( CTypeTraits<char16_t>::is_lower(c) );
        EXPECT_TRUE( CTypeTraits<char16_t>::is_alpha(c) );
        EXPECT_TRUE( CTypeTraits<char16_t>::is_alnum(c) );
        EXPECT_FALSE( CTypeTraits<char16_t>::is_upper(c) );
        EXPECT_FALSE( CTypeTraits<char16_t>::is_digit(c) );
        EXPECT_FALSE( CTypeTraits<char16_t>::is_space(c) );
        EXPECT_FALSE( CTypeTraits<char16_t>::is_cntrl(c) );
        EXPECT_FALSE( CTypeTraits<char16_t>::is_punct(c) );
        EXPECT_FALSE( CTypeTraits<char16_t>::is_ctlws(c) );
    }
    for( char16_t c = u'A'; c <= u'Z'; c++ )
    {
        EXPECT_TRUE( CTypeTraits<char16_t>::is_upper(c) );
        EXPECT_TRUE( CTypeTraits<char16_t>::is_alpha(c) );
        EXPECT_TRUE( CTypeTraits<char16_t>::is_alnum(c) );
        EXPECT_FALSE( CTypeTraits<char16_t>::is_lower(c) );
        EXPECT_FALSE( CTypeTraits<char16_t>::is_digit(c) );
        EXPECT_FALSE( CTypeTraits<char16_t>::is_space(c) );
        EXPECT_FALSE( CTypeTraits<char16_t>::is_cntrl(c) );
        EXPECT_FALSE( CTypeTraits<char16_t>::is_punct(c) );
        EXPECT_FALSE( CTypeTraits<char16_t>::is_ctlws(c) );
    }
    for( char16_t c = u'0'; c <= u'9'; c++ )
    {
        EXPECT_TRUE( CTypeTraits<char16_t>::is_digit(c) );
        EXPECT_TRUE( CTypeTraits<char16_t>::is_xdigit(c) );
        EXPECT_TRUE( CTypeTraits<char16_t>::is_alnum(c) );
        EXPECT_FALSE( CTypeTraits<char16_t>::is_alpha(c) );   
        EXPECT_FALSE( CTypeTraits<char16_t>::is_lower(c) );
        EXPECT_FALSE( CTypeTraits<char16_t>::is_upper(c) ); 
        EXPECT_FALSE( CTypeTraits<char16_t>::is_space(c) );
        EXPECT_FALSE( CTypeTraits<char16_t>::is_cntrl(c) );
        EXPECT_FALSE( CTypeTraits<char16_t>::is_punct(c) );
        EXPECT_FALSE( CTypeTraits<char16_t>::is_ctlws(c) );
    }
    for( char16_t c = u'a'; c <= u'f'; c++ )
    {
        EXPECT_TRUE( CTypeTraits<char16_t>::is_xdigit(c) );
    }
    for( char16_t c = u'A'; c <= u'F'; c++ )
    {
        EXPECT_TRUE( CTypeTraits<char16_t>::is_xdigit(c) );
    }
    for( char16_t c = 1; c < 0x20; c++ )
    {
        EXPECT_TRUE( CTypeTraits<char16_t>::is_cntrl(c) );
    }
    for( char16_t c : { u' ', u'\t', u'\n', u'\r' } )
    {
        EXPECT_TRUE( CTypeTraits<char16_t>::is_space(c) );
    }

    for( int i = CHAR_MAX + 1; i < 300; i++ )
    {
        char16_t c = static_cast<char16_t>( i );
        EXPECT_FALSE( CTypeTraits<char16_t>::is_alpha(c) );
        EXPECT_FALSE( CTypeTraits<char16_t>::is_alnum(c) );
        EXPECT_FALSE( CTypeTraits<char16_t>::is_lower(c) );
        EXPECT_FALSE( CTypeTraits<char16_t>::is_upper(c) );
        EXPECT_FALSE( CTypeTraits<char16_t>::is_digit(c) );
        EXPECT_FALSE( CTypeTraits<char16_t>::is_xdigit(c) );
        EXPECT_FALSE( CTypeTraits<char16_t>::is_space(c) );
        EXPECT_FALSE( CTypeTraits<char16_t>::is_cntrl(c) );
        EXPECT_FALSE( CTypeTraits<char16_t>::is_punct(c) );
        EXPECT_FALSE( CTypeTraits<char16_t>::is_ctlws(c) );
    }
}
