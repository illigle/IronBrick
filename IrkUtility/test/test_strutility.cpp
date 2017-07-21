#include <random>
#include <limits.h>
#include "gtest/gtest.h"
#include "IrkMemUtility.h"
#include "IrkStringUtility.h"

using std::string;
using std::wstring;
using std::u16string;
using namespace irk;

TEST( StrUtility, tools )
{
    {
        const char* str = "abcxyz123456";
        const char* u8str = u8"abcxyz123456";
        const char16_t* u16str = u"abcxyz123456";
        const wchar_t* wstr = L"abcxyz123456";
        EXPECT_EQ( strlen(str), str_count(str) );
        EXPECT_EQ( strlen(str), str_count(u8str) );
        EXPECT_EQ( strlen(str), str_count(u16str) );
        EXPECT_EQ( strlen(str), str_count(wstr) );

        EXPECT_EQ( strlen(str), str_ncount(str,64) );
        EXPECT_EQ( strlen(str), str_ncount(u8str,64) );
        EXPECT_EQ( strlen(str), str_ncount(u16str,64) );
        EXPECT_EQ( strlen(str), str_ncount(wstr,64) );

        EXPECT_EQ( 5, str_ncount(str,5) );
        EXPECT_EQ( 4, str_ncount(u8str,4) );
        EXPECT_EQ( 0, str_ncount(u16str,0) );
        EXPECT_EQ( 0, str_ncount(wstr,0) );
    }
    {
        EXPECT_EQ( 0, str_compare("","") );
        EXPECT_EQ( 0, str_compare(L"",L"") );
        EXPECT_EQ( 0, str_compare(u"",u"") );
        EXPECT_EQ( 0, str_compare("abc","abc") );
        EXPECT_EQ( 0, str_compare(L"abc",L"abc") );
        EXPECT_EQ( 0, str_compare(u"abc",u"abc") );
        EXPECT_LT( str_compare("abc","abcd"), 0 );
        EXPECT_LT( str_compare(L"abc",L"abcd"), 0 );
        EXPECT_LT( str_compare(u"abc",u"abcd"), 0 );
        EXPECT_GT( str_compare("abcd","abc"), 0 );
        EXPECT_GT( str_compare(L"abcd",L"abc"), 0 );
        EXPECT_GT( str_compare(u"abcd",u"abc"), 0 );
        EXPECT_GT( str_compare("abx","abc"), 0 );
        EXPECT_GT( str_compare(L"abx",L"abc"), 0 );
        EXPECT_GT( str_compare(u"abx",u"abc"), 0 );
    }
    {
        const char* u8str = u8"123456中国";
        const char16_t* u16str = u"123456中国";
        const wchar_t* wstr = L"123456中国";
        EXPECT_GT( str_count(u8str), 8 );
        EXPECT_EQ( 8, str_u8count(u8str) );
        EXPECT_EQ( 8, str_count(u16str) );
        EXPECT_EQ( 8, str_count(wstr) );
    }
    {
        const char* str = "abcxyz123456";
        char buf[16];
        EXPECT_EQ(0, str_copy(buf, 0, str) );
        EXPECT_EQ(1, str_copy(buf, 1, str) );
        EXPECT_EQ(0, str_count(buf) );
        EXPECT_EQ(str_count(str), str_copy(buf, countof(buf), str) );
        EXPECT_EQ(str_count(str), str_count(buf) );

        const char16_t* ustr = u"abcxyz123456";
        char16_t ubuf[16];
        EXPECT_EQ(0, str_copy(ubuf, 0, ustr) );
        EXPECT_EQ(1, str_copy(ubuf, 1, ustr) );
        EXPECT_EQ(0, str_count(ubuf) );
        EXPECT_EQ(str_count(ustr), str_copy(ubuf, countof(ubuf), ustr) );
        EXPECT_EQ(str_count(ustr), str_count(ubuf) );

        const wchar_t* wstr = L"abcxyz123456";
        wchar_t wbuf[16];
        EXPECT_EQ(0, str_copy(wbuf, 0, wstr) );
        EXPECT_EQ(1, str_copy(wbuf, 1, wstr) );
        EXPECT_EQ(0, str_count(wbuf) );
        EXPECT_EQ(str_count(wstr), str_copy(wbuf, countof(wbuf), wstr) );
        EXPECT_EQ(str_count(wstr), str_count(wbuf) );
    }
    {
        char str[8] = "abcd";
        EXPECT_EQ( countof(str), str_cat(str, countof(str), "123456") );
        EXPECT_STREQ( str, "abcd123" );
        wchar_t wstr[8] = L"abcd";
        EXPECT_EQ( countof(str), str_cat(wstr, countof(wstr), L"123456") );
        EXPECT_STREQ( wstr, L"abcd123" );
        char16_t ustr[8] = u"abcd";
        EXPECT_EQ( countof(str), str_cat(ustr, countof(ustr), u"123456") );
        EXPECT_EQ( 0, ustr[7] );
    }
    {
        char buf[16];
        EXPECT_EQ( -1, str_format( buf, 0, "%d", 123 ) );
        EXPECT_EQ( -1, str_format( buf, 3, "%d", 123 ) );
        EXPECT_EQ( 3, str_format( buf, countof(buf), "%d", 123 ) );
        EXPECT_STREQ( "123", buf );
        wchar_t wbuf[16];
        EXPECT_EQ( -1, str_format( wbuf, 0, L"%d", 123 ) );
        EXPECT_EQ( -1, str_format( wbuf, 3, L"%d", 123 ) );
        EXPECT_EQ( 3, str_format( wbuf, countof(wbuf), L"%d", 123 ) );
        EXPECT_STREQ( L"123", wbuf );
    }
    {
        std::string str = str_splice( "http", ':', "//", "google.com" );
        EXPECT_STREQ( "http://google.com", str.c_str() );

        std::wstring disk = L"C:\\";
        std::wstring wstr = str_splice( disk, L"Windows", L"\\", L"cmd.exe" );
        EXPECT_STREQ( L"C:\\Windows\\cmd.exe", wstr.c_str() );

        std::u16string u16str = str_splice( u"123", u"456", std::u16string(u"789") );
        EXPECT_EQ( std::u16string(u"123456789"), u16str );
    }
    {
        std::string str = str_join( ",", "a", "b", "c", "d" );
        EXPECT_STREQ( "a,b,c,d", str.c_str() );
        str = str_join( "*-*", "abc" );
        EXPECT_STREQ( "abc", str.c_str() );

        std::u16string u16str = str_join( u"/", u"home", u"user1", u"work", u"test" );
        EXPECT_EQ( std::u16string(u"home/user1/work/test"), u16str );

        std::wstring user = L"username";
        std::wstring wstr = str_join( L":", user, std::wstring(L"password") );
        EXPECT_STREQ( L"username:password", wstr.c_str() );   
    }
}

TEST( StrUtility, int_str )
{
    constexpr int kCnt = 32;
    char str[kCnt];
    EXPECT_EQ( -1, int_to_str(123,str,0) );
    EXPECT_EQ( -1, int_to_str(123,str,3) );
    EXPECT_EQ( 3, int_to_str(123,str,kCnt) );
    EXPECT_EQ( 4, int_to_str(-123,str,kCnt) );
    EXPECT_EQ( -1, int_to_str(0xFF,str,0,16) );
    EXPECT_EQ( -1, int_to_str(0xFF,str,4,16) );
    EXPECT_EQ( 4, int_to_str(0xFF,str,kCnt,16) );
    EXPECT_EQ( 3, int_to_str(0xFF,str,kCnt,10) );
    int_to_str( INT_MIN, str, kCnt );
    EXPECT_STREQ( "-2147483648", str );
    int_to_str( INT_MAX, str, kCnt );
    EXPECT_STREQ( "2147483647", str );
    int_to_str( UINT_MAX, str, kCnt );
    EXPECT_STREQ( "4294967295", str );
    int_to_str( INT64_MIN, str, kCnt );
    EXPECT_STREQ( "-9223372036854775808", str );
    int_to_str( INT64_MAX, str, kCnt );
    EXPECT_STREQ( "9223372036854775807", str );
    int_to_str( UINT64_MAX, str, kCnt, 16 );
    EXPECT_STREQ( "0xffffffffffffffff", str );

    wchar_t wstr[kCnt];
    EXPECT_EQ( -1, int_to_str(123,wstr,0) );
    EXPECT_EQ( -1, int_to_str(123,wstr,3) );
    EXPECT_EQ( 3, int_to_str(123,wstr,kCnt) );
    EXPECT_EQ( 4, int_to_str(-123,wstr,kCnt) );
    EXPECT_EQ( -1, int_to_str(0xFF,wstr,0,16) );
    EXPECT_EQ( -1, int_to_str(0xFF,wstr,4,16) );
    EXPECT_EQ( 4, int_to_str(0xFF,wstr,kCnt,16) );
    EXPECT_EQ( 3, int_to_str(0xFF,wstr,kCnt,10) );
    int_to_str( INT_MIN, wstr, kCnt, 16 );
    EXPECT_STREQ( L"-0x80000000", wstr );
    int_to_str( INT_MAX, wstr, kCnt );
    EXPECT_STREQ( L"2147483647", wstr );
    int_to_str( UINT_MAX, wstr, kCnt );
    EXPECT_STREQ( L"4294967295", wstr );
    int_to_str( INT64_MIN, wstr, kCnt );
    EXPECT_STREQ( L"-9223372036854775808", wstr );
    int_to_str( INT64_MAX, wstr, kCnt, 16 );
    EXPECT_STREQ( L"0x7fffffffffffffff", wstr );
    int_to_str( UINT64_MAX, wstr, kCnt, 16 );
    EXPECT_STREQ( L"0xffffffffffffffff", wstr );

    char16_t ustr[kCnt];
    EXPECT_EQ( -1, int_to_str(123,ustr,0) );
    EXPECT_EQ( -1, int_to_str(123,ustr,3) );
    EXPECT_EQ( 3, int_to_str(123,ustr,kCnt) );
    EXPECT_EQ( 4, int_to_str(-123,ustr,kCnt) );
    EXPECT_EQ( -1, int_to_str(0xFF,ustr,0,16) );
    EXPECT_EQ( -1, int_to_str(0xFF,ustr,4,16) );
    EXPECT_EQ( 4, int_to_str(0xFF,ustr,kCnt,16) );
    EXPECT_EQ( 3, int_to_str(0xFF,ustr,kCnt,10) );
    EXPECT_EQ( u16string(u"-2147483648"), int_to_ustring( INT_MIN ) );
    EXPECT_EQ( u16string(u"0x7fffffff"), int_to_ustring( INT_MAX, 16 ) );
    EXPECT_EQ( u16string(u"-9223372036854775808"), int_to_ustring( INT64_MIN ) );
    EXPECT_EQ( u16string(u"9223372036854775807"), int_to_ustring( INT64_MAX ) );

    EXPECT_EQ( 100, str_to_int("100") );
    EXPECT_EQ( 200u, str_to_uint(L"200") );
    EXPECT_EQ( 1000, str_to_int64("1000") );
    EXPECT_EQ( 2000u, str_to_uint64(u"2000") );
    EXPECT_EQ( 0x100, str_to_int("0x100") );
    EXPECT_EQ( 0x200, str_to_uint("0X200") );
    EXPECT_EQ( 0x1000, str_to_int64("1000", 16) );
    EXPECT_EQ( 0x2000, str_to_uint64("2000", 16) );
    EXPECT_EQ( INT_MIN, str_to_int("-2147483648") );
    EXPECT_EQ( UINT_MAX, str_to_uint(L"0xffffffff") );
    EXPECT_EQ( INT64_MIN, str_to_int64("-9223372036854775808") );
    EXPECT_EQ( UINT64_MAX, str_to_uint64("ffffffffffffffff", 16) );

    std::random_device rnddev;
    std::mt19937 rng( rnddev() );
    {
        int data[kCnt];
        for( auto& v : data )
            v = (int)(rng() - INT32_MAX);

        std::string str;
        std::wstring wstr;
        std::u16string ustr;
        int val = 0;
        for( int i = 0; i < kCnt; i++ )
        {
            str = int_to_string( data[i], 16 );
            val = str_to_int( str, 16 );
            EXPECT_EQ( data[i], val );
            wstr = int_to_wstring( data[i] );
            val = str_to_int( wstr );
            EXPECT_EQ( data[i], val );
            ustr = int_to_ustring( data[i], 16 );
            val = str_to_int( ustr );
            EXPECT_EQ( data[i], val );
        }
    }
    {
        uint32_t data[kCnt];
        for( auto& v : data )
            v = rng();

        IntToStrCvt<char> int2str;
        IntToStrCvt<wchar_t> int2wstr;
        IntToStrCvt<char16_t> int2ustr;
        uint32_t val = 0;
        for( int i = 0; i < kCnt; i++ )
        {
            const char* str = int2str( data[i], 16 );
            val = str_to_uint( str, 16 );
            EXPECT_EQ( data[i], val );
            const wchar_t* wstr = int2wstr( data[i], 16 );
            val = str_to_uint( wstr );
            EXPECT_EQ( data[i], val );
            const char16_t* ustr = int2ustr( data[i] );
            val = str_to_uint( ustr );
            EXPECT_EQ( data[i], val );
        }
    }

    std::mt19937_64 rng64( rnddev() );
    {
        int64_t data[kCnt];
        for( auto& v : data )
            v = (int64_t)(rng64() - INT64_MAX);

        std::string str;
        std::wstring wstr;
        std::u16string ustr;
        int64_t val = 0;
        for( int i = 0; i < kCnt; i++ )
        {
            str = int_to_string( data[i], 16 );
            val = str_to_int64( str, 16 );
            EXPECT_EQ( data[i], val );
            wstr = int_to_wstring( data[i], 16 );
            val = str_to_int64( wstr );
            EXPECT_EQ( data[i], val );
            ustr = int_to_ustring( data[i] );
            val = str_to_int64( ustr );
            EXPECT_EQ( data[i], val );
        }
    }
    {
        uint64_t data[kCnt];
        for( auto& v : data )
            v = rng64();

        IntToStrCvt<char> int2str;
        IntToStrCvt<wchar_t> int2wstr;
        IntToStrCvt<char16_t> int2ustr;
        uint64_t val = 0;
        for( int i = 0; i < kCnt; i++ )
        {
            const char* str = int2str( data[i] );
            val = str_to_uint64( str );
            EXPECT_EQ( data[i], val );
            const wchar_t* wstr = int2wstr( data[i] );
            val = str_to_uint64( wstr );
            EXPECT_EQ( data[i], val );
            const char16_t* ustr = int2ustr( data[i], 16 );
            val = str_to_uint64( ustr );
            EXPECT_EQ( data[i], val );
        }
    }
}

TEST( StrUtility, striped )
{
    {
        string str = " \n\t\f\vabc\r\n";
        string str2 = get_striped( str.c_str() );
        EXPECT_STREQ( "abc", str2.c_str() );
        str_strip( str );
        EXPECT_EQ( str2, str );

        str = " \n\t\f\vabc\r\n";
        str2 = get_lstriped( str.c_str() );
        EXPECT_STREQ( "abc\r\n", str2.c_str() );
        str_lstrip( str );
        EXPECT_EQ( str2, str );

        str = " \n\t\f\vabc\r\n";
        str2 = get_rstriped( str.c_str() );
        EXPECT_STREQ( " \n\t\f\vabc", str2.c_str() );
        str_rstrip( str );
        EXPECT_EQ( str2, str );

        str = "xyz123abc";
        str2 = get_striped( str.c_str() );
        EXPECT_EQ( str2, str );
        str_strip( str );
        EXPECT_EQ( str2, str );

        str = "\n \t\f\v\r\n\x20";
        str2 = get_striped( str.c_str() );
        EXPECT_TRUE( str2.empty() );
        str_strip( str );
        EXPECT_TRUE( str.empty() );

        str.clear();
        str2 = get_striped( str.c_str() );
        EXPECT_TRUE( str2.empty() );
        str_strip( str );
        EXPECT_TRUE( str.empty() );

        str = "***123*";
        str2 = get_striped( str.c_str(), '*' );
        EXPECT_TRUE( str2.compare("123")==0 );
        str_strip( str, '*' );
        EXPECT_EQ( str2, str );
        str2 = get_striped( "****", '*' );
        EXPECT_TRUE( str2.empty() );
    }
    {
        wstring str = L" \n\t\f\vXYZ\r\n";
        wstring str2 = get_striped( str.c_str() );
        EXPECT_STREQ( L"XYZ", str2.c_str() );
        str_strip( str );
        EXPECT_EQ( str2, str );

        str = L" \n\t\f\vXYZ\r\n";
        str2 = get_lstriped( str.c_str() );
        EXPECT_STREQ( L"XYZ\r\n", str2.c_str() );
        str_lstrip( str );
        EXPECT_EQ( str2, str );

        str = L" \n\t\f\vXYZ\r\n";
        str2 = get_rstriped( str.c_str() );
        EXPECT_STREQ( L" \n\t\f\vXYZ", str2.c_str() );
        str_rstrip( str );
        EXPECT_EQ( str2, str );

        str = L"xyz123abc";
        str2 = get_striped( str.c_str() );
        EXPECT_EQ( str2, str );
        str_strip( str );
        EXPECT_EQ( str2, str );

        str = L"\n\t\f  \v\r\n";
        str2 = get_striped( str.c_str() );
        EXPECT_TRUE( str2.empty() );
        str_strip( str );
        EXPECT_TRUE( str.empty() );

        str.clear();
        str2 = get_striped( str.c_str() );
        EXPECT_TRUE( str2.empty() );
        str_strip( str );
        EXPECT_TRUE( str.empty() );

        str = L"***123#";
        str2 = get_lstriped( str.c_str(), L'*' );
        EXPECT_TRUE( str2.compare(L"123#")==0 );
        str_lstrip( str, L'*' );
        EXPECT_EQ( str2, str );
        str2 = get_lstriped( L"****", '*' );
        EXPECT_TRUE( str2.empty() );
    }
    {
        u16string str = u" \n\t\f\v123\r\n";
        u16string str2 = get_striped( str.c_str() );
        EXPECT_EQ( 0, str2.compare( u"123" ) );
        str_strip( str );
        EXPECT_EQ( str2, str );

        str = u" \n\t\f\v123\r\n";
        str2 = get_lstriped( str.c_str() );
        EXPECT_EQ( 0, str2.compare( u"123\r\n" ) );
        str_lstrip( str );
        EXPECT_EQ( str2, str );

        str = u" \n\t\f\v123\r\n";
        str2 = get_rstriped( str.c_str() );
        EXPECT_EQ( 0, str2.compare( u" \n\t\f\v123" ) );
        str_rstrip( str );
        EXPECT_EQ( str2, str );

        str = u"xyz123abc";
        str2 = get_striped( str.c_str() );
        EXPECT_EQ( str2, str );
        str_strip( str );
        EXPECT_EQ( str2, str );

        str = u"\n\t\f \v\r \u0020\n";
        str2 = get_striped( str.c_str() );
        EXPECT_TRUE( str2.empty() );
        str_strip( str );
        EXPECT_TRUE( str.empty() );

        str.clear();
        str2 = get_striped( str.c_str() );
        EXPECT_TRUE( str2.empty() );
        str_strip( str );
        EXPECT_TRUE( str.empty() );

        str = u"*123****";
        str2 = get_rstriped( str.c_str(), u'*' );
        EXPECT_TRUE( str2.compare(u"*123")==0 );
        str_rstrip( str, u'*' );
        EXPECT_EQ( str2, str );
        str2 = get_rstriped( u"****", '*' );
        EXPECT_TRUE( str2.empty() );
    }
}

TEST( StrUtility, case_cvt )
{
    {
        string str = "AbcxYZ123<>:中国";
        string strL = "abcxyz123<>:中国";
        string strU = "ABCXYZ123<>:中国";
        EXPECT_EQ( strL, get_lowered(str.c_str()) );
        EXPECT_EQ( strU, get_uppered(str.c_str()) );
        str_tolower( str );
        EXPECT_EQ( strL, str );
        str_toupper( str );
        EXPECT_EQ( strU, str );
    }
    {
        wstring str = L"AbcxYZ123<>:中国";
        wstring strL = L"abcxyz123<>:中国";
        wstring strU = L"ABCXYZ123<>:中国";
        EXPECT_EQ( strL, get_lowered(str.c_str()) );
        EXPECT_EQ( strU, get_uppered(str.c_str()) );
        str_tolower( str );
        EXPECT_EQ( strL, str );
        str_toupper( str );
        EXPECT_EQ( strU, str );
    }
    {
        u16string str = u"AbcxYZ123<>:中国";
        u16string strL = u"abcxyz123<>:中国";
        u16string strU = u"ABCXYZ123<>:中国";
        EXPECT_EQ( strL, get_lowered(str.c_str()) );
        EXPECT_EQ( strU, get_uppered(str.c_str()) );
        str_tolower( str );
        EXPECT_EQ( strL, str );
        str_toupper( str );
        EXPECT_EQ( strU, str );
    }
}

TEST( StrUtility, utf_8_16 )
{
    const string u8str = u8"AbcxYZ123<>:中国";
    const u16string u16str = u"AbcxYZ123<>:中国";

    char u8buf[64];
    EXPECT_EQ( -1, str_utf16to8(u16str, u8buf, 1) );    // buffer too small
    EXPECT_EQ( (int)u16str.length(), str_utf16to8(u16str, u8buf, 64) );
    EXPECT_EQ( u8str.length(), ::strlen(u8buf) );
    EXPECT_STREQ( u8str.c_str(), u8buf );

    char16_t u16buf[64];
    EXPECT_EQ( -1, str_utf8to16(u8str, u16buf, 1) );    // buffer too small
    EXPECT_EQ( str_u8count(u8str.c_str()), str_utf8to16(u8str, u16buf, 64) );
    EXPECT_EQ( u16str.length(), str_count(u16buf) );
    EXPECT_EQ( 0, memcmp(u16str.c_str(), u16buf, u16str.length()) );

    string str2;
    u16string ustr2;
    str_utf16to8( u16str, str2 );
    str_utf8to16( u8str, ustr2 );
    EXPECT_EQ( u8str, str2 );
    EXPECT_EQ( u16str, ustr2 );

#ifdef _WIN32
    wstring wstr = L"AbcxYZ123<>:中国";
    wstring wstr2;
    str_utf16to8( wstr, str2 );
    str_utf8to16( u8str, wstr2 );
    EXPECT_EQ( u8str, str2 );
    EXPECT_EQ( wstr, wstr2 );
#endif
}
