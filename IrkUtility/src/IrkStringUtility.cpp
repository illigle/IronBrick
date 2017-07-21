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

#include <type_traits>
#include "IrkStringUtility.h"
#include "IrkContract.h"

namespace irk {

size_t str_count( const char16_t* str )
{
    size_t cnt = 0;
    while( str[cnt] != 0 )
        cnt++;
    return cnt;
}

size_t str_ncount( const char16_t* str, size_t maxlen )
{
    size_t cnt = 0;
    while( str[cnt] != 0 )
    {
        if( ++cnt >= maxlen )
            return maxlen;
    }
    return cnt;
}

int str_compare( const char16_t* s1, const char16_t* s2 )
{
    for( ; *s1 == *s2; *s1++, *s2++ )
    {
        if( *s1 == 0 )
            return 0;
    }
    return (*s1 > *s2) ? 1 : -1;
}

// string copy, the behaviour is like c11 strncpy_s(buf, size, src, size - 1)
size_t str_copy( char* buf, size_t size, const char* src )
{
    if( size == 0 ) // invalid buffer size
        return 0;

    for( size_t idx = 0; idx < size; idx++ )
    {
        if( src[idx] == 0 )
        {
            buf[idx] = 0;
            return idx;
        }
        buf[idx] = src[idx];
    }

    buf[size-1] = 0;    // always add terminal null char
    return size;
}
size_t str_copy( wchar_t* buf, size_t size, const wchar_t* src )
{
    if( size == 0 ) // invalid buffer size
        return 0;

    for( size_t idx = 0; idx < size; idx++ )
    {
        if( src[idx] == 0 )
        {
            buf[idx] = 0;
            return idx;
        }
        buf[idx] = src[idx];
    }

    buf[size-1] = 0;    // always add terminal null char
    return size;
}
size_t str_copy( char16_t* buf, size_t size, const char16_t* src )
{
    if( size == 0 ) // invalid buffer size
        return 0;

    for( size_t idx = 0; idx < size; idx++ )
    {
        if( src[idx] == 0 )
        {
            buf[idx] = 0;
            return idx;
        }
        buf[idx] = src[idx];
    }

    buf[size-1] = 0;    // always add terminal null char
    return size;
}

// string concatenation, like strncat, but the result string will always be null terminated
size_t str_cat( char* dst, size_t size, const char* src )
{
    if( size == 0 ) // invalid buffer size
        return 0;

    size_t srcIdx = 0;
    size_t dstIdx = ::strnlen( dst, size );
    while( dstIdx < size )
    {
        if( src[srcIdx] == 0 )
        {
            dst[dstIdx] = 0;
            return dstIdx;
        }
        dst[dstIdx++] = src[srcIdx++];
    }

    dst[size-1] = 0;    // always add terminal null char
    return size;
}
size_t str_cat( wchar_t* dst, size_t size, const wchar_t* src )
{
    if( size == 0 ) // invalid buffer size
        return 0;

    size_t srcIdx = 0;
    size_t dstIdx = ::wcsnlen( dst, size );
    while( dstIdx < size )
    {
        if( src[srcIdx] == 0 )
        {
            dst[dstIdx] = 0;
            return dstIdx;
        }
        dst[dstIdx++] = src[srcIdx++];
    }

    dst[size-1] = 0;    // always add terminal null char
    return size;
}
size_t str_cat( char16_t* dst, size_t size, const char16_t* src )
{
    if( size == 0 ) // invalid buffer size
        return 0;

    size_t srcIdx = 0;
    size_t dstIdx = str_ncount( dst, size );
    while( dstIdx < size )
    {
        if( src[srcIdx] == 0 )
        {
            dst[dstIdx] = 0;
            return dstIdx;
        }
        dst[dstIdx++] = src[srcIdx++];
    }

    dst[size-1] = 0;    // always add terminal null char
    return size;
}

//======================================================================================================================
// hash string to 32 bits unsigned value, using FNV-1a string hash algorithm

uint32_t str_hash( const char* str )
{
    uint32_t hval = 0x811c9dc5;
    uint32_t ch = *str;
    while( ch != 0 )
    {
        hval = (hval ^ ch) * 0x01000193;
        ch = *(++str);
    }
    return hval;
}
uint32_t str_hash( const wchar_t* str )
{
    uint32_t hval = 0x811c9dc5;
    uint32_t ch = *str;
    while( ch != 0 )
    {
        hval = (hval ^ (ch >> 8)) * 0x01000193;
        hval = (hval ^ (ch & 0xFF)) * 0x01000193;
        ch = *(++str);
    }
    return hval;
}
uint32_t str_hash( const char16_t* str )
{
    uint32_t hval = 0x811c9dc5;
    uint32_t ch = *str;
    while( ch != 0 )
    {
        hval = (hval ^ (ch >> 8)) * 0x01000193;
        hval = (hval ^ (ch & 0xFF)) * 0x01000193;
        ch = *(++str);
    }
    return hval;
}

//======================================================================================================================
// convert string to integer
// if string begin with 0x or 0X, it is parsed as hex number, otherwise parsed as decimal
// return 0 if no digital character can be found
// NOTE: if overflow, result is undefined !

// digital value table, 16 means invalid
static const uint8_t s_DigitVals[256] =
{
    16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,
    16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,
    16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 16,16,16,16,16,16,
    16,10,11,12,13,14,15,16,16,16,16,16,16,16,16,16,
    16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,
    16,10,11,12,13,14,15,16,16,16,16,16,16,16,16,16,
    16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,
    16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,
    16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,
    16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,
    16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,
    16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,
    16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,
    16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,
    16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,
};

// calculate integer value from ASCII string, much faster
template<typename IntTy>
static IntTy calc_strval( const char* s, int radix )
{
    IntTy val = 0;
    if( *s == '0' && ((s[1] | 0x20) == 'x') )   // hex
    {
        radix = 16;
        s += 2;
    }

    if( radix == 16 )
    {
        const uint8_t* us = (const uint8_t*)s;
        uint8_t dgt = s_DigitVals[*us];
        while( dgt < 16 )
        {
            val = (val << 4) + dgt;
            dgt = s_DigitVals[*++us];
        }
    }
    else    // decimal
    {
        const uint8_t* us = (const uint8_t*)s;
        uint8_t dgt = s_DigitVals[*us];
        while( dgt < 10 )
        {
            val = val * 10 + dgt;
            dgt = s_DigitVals[*++us];
        }
    }
    return val;
}

// calculate integer value from wide string
template<typename IntTy, typename CharT>
static IntTy calc_strval( const CharT* s, int radix )
{
    static_assert( sizeof(CharT) > 1, "wide string only" );

    IntTy val = 0;
    if( *s == '0' && ((s[1] | 0x20) == 'x') )   // hex
    {
        radix = 16;
        s += 2;
    }

    if( radix == 16 )
    {
        while( CTypeTraits<CharT>::is_xdigit(*s) )
        {
            val = (val << 4) + s_DigitVals[*s];
            s++;
        }
    }
    else    // decimal
    {
        while( CTypeTraits<CharT>::is_digit(*s) )
        {
            val = val * 10 + *s - '0';
            s++;
        }
    }
    return val;
}

template<typename IntTy, typename CharT>
static inline IntTy calc_signed( const CharT* str, int radix )
{
    static_assert( std::is_signed<IntTy>::value, "" );

    // skip leading space
    while( CTypeTraits<CharT>::is_space( *str ) )
        str++;

    // check sign
    IntTy signFlag = 0;
    if( *str == '-' )
    {
        signFlag = -1;
        str++;
    }

    IntTy val = calc_strval<IntTy>( str, radix );
    return (val ^ signFlag) - signFlag;
}

template<typename IntTy, typename CharT>
static inline IntTy calc_unsigned( const CharT* str, int radix )
{
    static_assert( std::is_unsigned<IntTy>::value, "" );

    // skip leading space
    while( CTypeTraits<CharT>::is_space( *str ) )
        str++;

    return calc_strval<IntTy>( str, radix );
}

int32_t str_to_int( const char* str, int radix )
{
    return calc_signed<int,char>( str, radix );
}
int32_t str_to_int( const wchar_t* str, int radix )
{
    return calc_signed<int,wchar_t>( str, radix );
}
int32_t str_to_int( const char16_t* str, int radix )
{
    return calc_signed<int,char16_t>( str, radix );
}

uint32_t str_to_uint( const char* str, int radix )
{
    return calc_unsigned<uint32_t,char>( str, radix );
}
uint32_t str_to_uint( const wchar_t* str, int radix )
{
    return calc_unsigned<uint32_t,wchar_t>( str, radix );
}
uint32_t str_to_uint( const char16_t* str, int radix )
{
    return calc_unsigned<uint32_t,char16_t>( str, radix );
}

int64_t str_to_int64( const char* str, int radix )
{
    return calc_signed<int64_t,char>( str, radix );
}
int64_t str_to_int64( const wchar_t* str, int radix )
{
    return calc_signed<int64_t,wchar_t>( str, radix );
}
int64_t str_to_int64( const char16_t* str, int radix )
{
    return calc_signed<int64_t,char16_t>( str, radix );
}

uint64_t str_to_uint64( const char* str, int radix )
{
    return calc_unsigned<uint64_t,char>( str, radix );
}
uint64_t str_to_uint64( const wchar_t* str, int radix )
{
    return calc_unsigned<uint64_t,wchar_t>( str, radix );
}
uint64_t str_to_uint64( const char16_t* str, int radix )
{
    return calc_unsigned<uint64_t,char16_t>( str, radix );
}

//======================================================================================================================
// convert integer to string, support decimal and hexadecimal output format
// return character number written(excludeing terminal null)
// return -1 if output buffer's size is insufficient

// inner integer to string convertion, output in reverse order
template<typename UIntTy,typename CharT>
static int reverse_uint2str( UIntTy v, int radix, CharT* buff, int i )
{
    static constexpr char s_HexChar[] = "0123456789abcdef";
    static_assert( std::is_unsigned<UIntTy>::value, "unsigned int" );

    buff[i] = 0;        // always add terminal null character

    if( radix == 10 )
    {
        while( v >= 10 )
        {
            UIntTy d = v / 10;
            buff[--i] = static_cast<CharT>( s_HexChar[(unsigned)(v - d*10)] );
            v = d;
        }
        buff[--i] = static_cast<CharT>( s_HexChar[v] );
    }
    else    // hexadecimal
    {
        while( v > 15 )
        {
            buff[--i] = static_cast<CharT>( s_HexChar[(unsigned)(v & 0xF)] );
            v >>= 4;
        }
        buff[--i] = static_cast<CharT>( s_HexChar[v] );
        buff[--i] = static_cast<CharT>( 'x' );
        buff[--i] = static_cast<CharT>( '0' );
    }

    return i;
}

template<typename IntTy, typename CharT>
static int impl_sint2str( IntTy val, CharT* dst, int size, int radix )
{
    static_assert( std::is_signed<IntTy>::value, "signed int" );
    irk_expect( radix == 10 || radix == 16 );

    // convert into inner buffer
    typedef typename std::make_unsigned<IntTy>::type UIntTy;
    CharT buff[32];
    int i = 31;
    if( val >= 0 )
    {
        i = reverse_uint2str<UIntTy,CharT>( val, radix, buff, 31 );
    }
    else
    {
        UIntTy uval = ~(UIntTy)val + 1;     // work around gcc 5.4 bug
        i = reverse_uint2str<UIntTy,CharT>( uval, radix, buff, 31 );
        buff[--i] = '-';    // add sign char
    }
    assert( i >= 0 );

    // copy inner result
    int cnt = 31 - i;
    if( cnt < size )
    {
        for( int k = 0; i < 32; k++, i++ )
            dst[k] = buff[i];
        return cnt;
    }

    // output buffer's size too small
    if( size > 0 )
        dst[0] = 0;
    return -1;
}
template<typename IntTy, typename CharT>
static inline std::basic_string<CharT> impl_sint2str( IntTy val, int radix )
{
    static_assert( std::is_signed<IntTy>::value, "signed int" );
    irk_expect( radix == 10 || radix == 16 );

    // convert into inner buffer
    typedef typename std::make_unsigned<IntTy>::type UIntTy;
    CharT buff[32];
    int i = 31;
    if( val >= 0 )
    {
        i = reverse_uint2str<UIntTy,CharT>( val, radix, buff, 31 );
    }
    else
    {
        UIntTy uval = ~(UIntTy)val + 1; // work around gcc 5.4 bug
        i = reverse_uint2str<UIntTy,CharT>( uval, radix, buff, 31 );
        buff[--i] = '-';    // add sign char
    }
    assert( i >= 0 );

    return std::basic_string<CharT>( buff + i );
}

template<typename UIntTy, typename CharT>
static int impl_uint2str( UIntTy val, CharT* dst, int size, int radix )
{
    static_assert( std::is_unsigned<UIntTy>::value, "unsigned int" );
    irk_expect( radix == 10 || radix == 16 );

    // convert into inner buffer
    CharT buff[32];
    int i = reverse_uint2str<UIntTy,CharT>( val, radix, buff, 31 );
    assert( i >= 0 );

    // copy inner result
    int cnt = 31 - i;
    if( cnt < size )
    {
        for( int k = 0; i < 32; k++, i++ )
            dst[k] = buff[i];
        return cnt;
    }

    // output buffer's size too small
    if( size > 0 )
        dst[0] = 0;
    return -1;
}
template<typename UIntTy, typename CharT>
static inline std::basic_string<CharT> impl_uint2str( UIntTy val, int radix )
{
    static_assert( std::is_unsigned<UIntTy>::value, "unsigned int" );
    irk_expect( radix == 10 || radix == 16 );

    // convert into inner buffer
    CharT buff[32];
    int i = reverse_uint2str<UIntTy,CharT>( val, radix, buff, 31 );
    assert( i >= 0 );
    
    return std::basic_string<CharT>( buff + i );
}

// convert int32 to string
int int_to_str( int32_t val, char* buf, int size, int radix )
{
    return impl_sint2str<int32_t,char>( val, buf, size, radix );
}
int int_to_str( int32_t val, wchar_t* buf, int size, int radix )
{
    return impl_sint2str<int32_t,wchar_t>( val, buf, size, radix );
}
int int_to_str( int32_t val, char16_t* buf, int size, int radix )
{
    return impl_sint2str<int32_t,char16_t>( val, buf, size, radix );
}
std::string int_to_string( int32_t val, int radix )
{
    return impl_sint2str<int32_t,char>( val, radix );
}
std::wstring int_to_wstring( int32_t val, int radix )
{
    return impl_sint2str<int32_t,wchar_t>( val, radix );
}
std::u16string int_to_ustring( int32_t val, int radix )
{
    return impl_sint2str<int32_t,char16_t>( val, radix );
}

// convert uint32 to string
int int_to_str( uint32_t val, char* buf, int size, int radix )
{
    return impl_uint2str<uint32_t,char>( val, buf, size, radix );
}
int int_to_str( uint32_t val, wchar_t* buf, int size, int radix )
{
    return impl_uint2str<uint32_t,wchar_t>( val, buf, size, radix );
}
int int_to_str( uint32_t val, char16_t* buf, int size, int radix )
{
    return impl_uint2str<uint32_t,char16_t>( val, buf, size, radix );
}
std::string int_to_string( uint32_t val, int radix )
{
    return impl_uint2str<uint32_t,char>( val, radix );
}
std::wstring int_to_wstring( uint32_t val, int radix )
{
    return impl_uint2str<uint32_t,wchar_t>( val, radix );
}
std::u16string int_to_ustring( uint32_t val, int radix )
{
    return impl_uint2str<uint32_t,char16_t>( val, radix );
}

// convert int64 to string
int int_to_str( int64_t val, char* buf, int size, int radix )
{
    return impl_sint2str<int64_t,char>( val, buf, size, radix );
}
int int_to_str( int64_t val, wchar_t* buf, int size, int radix )
{
    return impl_sint2str<int64_t,wchar_t>( val, buf, size, radix );
}
int int_to_str( int64_t val, char16_t* buf, int size, int radix )
{
    return impl_sint2str<int64_t,char16_t>( val, buf, size, radix );
}
std::string int_to_string( int64_t val, int radix )
{
    return impl_sint2str<int64_t,char>( val, radix );
}
std::wstring int_to_wstring( int64_t val, int radix )
{
    return impl_sint2str<int64_t,wchar_t>( val, radix );
}
std::u16string int_to_ustring( int64_t val, int radix )
{
    return impl_sint2str<int64_t,char16_t>( val, radix );
}

// convert uint64 to string
int int_to_str( uint64_t val, char* buf, int size, int radix )
{
    return impl_uint2str<uint64_t,char>( val, buf, size, radix );
}
int int_to_str( uint64_t val, wchar_t* buf, int size, int radix )
{
    return impl_uint2str<uint64_t,wchar_t>( val, buf, size, radix );
}
int int_to_str( uint64_t val, char16_t* buf, int size, int radix )
{
    return impl_uint2str<uint64_t,char16_t>( val, buf, size, radix );
}
std::string int_to_string( uint64_t val, int radix )
{
    return impl_uint2str<uint64_t,char>( val, radix );
}
std::wstring int_to_wstring( uint64_t val, int radix )
{
    return impl_uint2str<uint64_t,wchar_t>( val, radix );
}
std::u16string int_to_ustring( uint64_t val, int radix )
{
    return impl_uint2str<uint64_t,char16_t>( val, radix );
}

// integer to string converter, return inner buffer
// WARNIG: can not used more than 2 times in single statement !
template<typename CharT>
const CharT* IntToStrCvt<CharT>::operator()( int32_t val, int radix )
{
    CharT* buff = m_Buff[m_Idx];
    m_Idx ^= 1;

    int i = 31;
    if( val >= 0 )
    {
        i = reverse_uint2str<uint32_t,CharT>( val, radix, buff, 31 );
    }
    else
    {
        uint32_t uval = ~(uint32_t)val + 1;
        i = reverse_uint2str<uint32_t,CharT>( uval, radix, buff, 31 );
        buff[--i] = '-';    // add sign char
    }
    assert( i >= 0 );
    return buff + i;
}

template<typename CharT>
const CharT* IntToStrCvt<CharT>::operator()( uint32_t val, int radix )
{
    CharT* buff = m_Buff[m_Idx];
    m_Idx ^= 1;

    int i = reverse_uint2str<uint32_t,CharT>( val, radix, buff, 31 );
    assert( i >= 0 );
    return buff + i;
}

template<typename CharT>
const CharT* IntToStrCvt<CharT>::operator()( int64_t val, int radix )
{
    CharT* buff = m_Buff[m_Idx];
    m_Idx ^= 1;

    int i = 31;
    if( val >= 0 )
    {
        i = reverse_uint2str<uint64_t,CharT>( val, radix, buff, 31 );
    }
    else
    {
        uint64_t uval = ~(uint64_t)val + 1;
        i = reverse_uint2str<uint64_t,CharT>( uval, radix, buff, 31 );
        buff[--i] = '-';    // add sign char
    }
    assert( i >= 0 );
    return buff + i;
}

template<typename CharT>
const CharT* IntToStrCvt<CharT>::operator()( uint64_t val, int radix )
{
    CharT* buff = m_Buff[m_Idx];
    m_Idx ^= 1;

    int i = reverse_uint2str<uint64_t,CharT>( val, radix, buff, 31 );
    assert( i >= 0 );
    return buff + i;
}

// explicit instantiation
template class IntToStrCvt<char>;
template class IntToStrCvt<wchar_t>;
template class IntToStrCvt<char16_t>;

//======================================================================================================================
// convert between UTF-16 and UTF-8 string, q.v. standard RTF-3926

// max UTF-16 character number
#define UTF16_MAX   0x10FFFF

// bytes of single UTF-8 character, 0: invalid UTF-8 character
static constexpr uint8_t s_Utf8ByteCnt[32] =
{
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 3, 3, 4, 0,
};

// return UTF-8 character count in the string
int str_u8count( const std::string& u8str )
{
    const int len = (int)u8str.length();
    int chCnt = 0, i = 0;  
    while( i < len )
    {
        uint8_t ch = u8str[i];
        int byteCnt = s_Utf8ByteCnt[ch >> 3];
        if( byteCnt == 0 )  // invalid utf-8 char
            return -1;
        i += byteCnt;
        chCnt++;
    }
    if( i > len )   // invalid utf-8 string
    {
        return -1;
    }
    return chCnt;
}

// return UTF-8 character count in the string
int str_u8count( const char* u8str )
{
    const int len = (int)strlen( u8str );
    int chCnt = 0, i = 0;
    while( i < len )
    {
        uint8_t ch = u8str[i];
        int byteCnt = s_Utf8ByteCnt[ch >> 3];
        if( byteCnt == 0 )  // invalid utf-8 char
            return -1;
        i += byteCnt;
        chCnt++;
    }
    if( i > len )   // invalid utf-8 string
    {
        return -1;
    }
    return chCnt;
}

// get UTF-16 character's "unicode character number" and byte count of the according UTF-8 character
// return > UTF16_MAX if character is invalid
static inline uint32_t get_chnum_and_bytes( const char16_t*& src, uint8_t& byteCnt )
{
    uint32_t ch = *src++;

    // get unicode character number
    if( ch >= 0xD800 && ch <= 0xDBFF )  // 4 bytes UTF-16
    {
        if( (uint16_t)*src >= 0xDC00 && (uint16_t)*src <= 0xDFFF )
        {
            ch = (ch - 0xD800) << 10;
            ch += ((uint16_t)*src - 0xDC00) + 0x10000;
            src++;
        }
        else    // invalid character
        {
            return INT32_MAX;
        }
    }
    else if( ch >= 0xDC00 && ch <= 0xDFFF ) // invalid character
    {
        return INT32_MAX;
    }

    // get byte count of according UTF-8 character
    if( ch < 0x80 )
        byteCnt = 1;
    else if( ch < 0x800 )
        byteCnt = 2;
    else if( ch < 0x10000 )
        byteCnt = 3;
    else
        byteCnt = 4;

    return ch;
}

// get UTF-8 buffer size needed
static int calc_u8_bufsize( const char16_t* str )
{
    uint8_t byteCnt = 0;
    int size = 0;
    while( *str )
    {
        if( (uint16_t)*str < 0x80 ) // ASCII
            size += 1;
        else if( get_chnum_and_bytes( str, byteCnt ) <= UTF16_MAX )
            size += byteCnt;
        else    // invalid char
            break;
    }
    return size + 4;
}

// Convert UTF-16 to UTF-8
// Return the number of characters written, or -1 if failed
int str_utf16to8( const char16_t* str, char* buff, int size )
{
    if( size <= 0 )     // compute output buffer size needed
    {
        return calc_u8_bufsize( str );
    }

    uint8_t byteCnt = 0;
    uint8_t* out = (uint8_t*)buff;
    int k = 0, u8Cnt = 0;
    while( 1 )
    {
        if( (uint16_t)*str < 0x80 )     // ASCII
        {
            if( *str == 0 )             // Done!
            {
                out[k] = 0;
                return u8Cnt;
            }
            else if( k + 1 >= size )    // insufficient buffer
                break;

            out[k++] = (uint8_t)( *str++ );
            u8Cnt++;
        }
        else
        {
            uint32_t ch = get_chnum_and_bytes( str, byteCnt );
            if( ch > UTF16_MAX )        // invalid char
                break;
            if( k + byteCnt >= size )   // insufficient buffer
                break;

            switch( byteCnt )
            {
            case 4:
                out[k+0] = (uint8_t)( 0xF0 | (ch >> 18) );
                out[k+1] = (uint8_t)( 0x80 | ((ch >> 12) & 0x3F) );
                out[k+2] = (uint8_t)( 0x80 | ((ch >> 6) & 0x3F) );
                out[k+3] = (uint8_t)( 0x80 | (ch & 0x3F) );
                k += 4;
                break;
            case 3:
                out[k+0] = (uint8_t)( 0xE0 | (ch >> 12) );
                out[k+1] = (uint8_t)( 0x80 | ((ch >> 6) & 0x3F) );
                out[k+2] = (uint8_t)( 0x80 | (ch & 0x3F) );
                k += 3;
                break;
            case 2:
                out[k+0] = (uint8_t)( 0xC0 | (ch >> 6) );
                out[k+1] = (uint8_t)( 0x80 | (ch & 0x3F) );
                k += 2;
                break;
            case 1:
                out[k++] = (uint8_t)( ch );
                break;
            default:
                IRK_UNREACHABLE();
            }
            u8Cnt++;
        }
    }

    out[k] = 0;
    return -1;
}

// Convert UTF-16 to UTF-8
// Return the number of characters written, or -1 if failed
int str_utf16to8( const char16_t* str, std::string& dst )
{
    dst.clear();

    uint8_t byteCnt = 0;
    int u8Cnt = 0;
    while( 1 )
    {
        if( (uint16_t)*str < 0x80 )     // ASCII
        {
            if( *str == 0 )             // Done
                return u8Cnt;

            dst.push_back( (char)str[0] );
            str++;
            u8Cnt++;
        }
        else
        {
            uint32_t ch = get_chnum_and_bytes( str, byteCnt );
            if( ch > UTF16_MAX )        // invalid char
                break;

            switch( byteCnt )
            {
            case 4:
                dst.push_back( (char)(0xF0 | (ch >> 18)) );
                dst.push_back( (char)(0x80 | ((ch >> 12) & 0x3F)) );
                dst.push_back( (char)(0x80 | ((ch >> 6) & 0x3F)) );
                dst.push_back( (char)(0x80 | (ch & 0x3F)) );
                break;
            case 3:
                dst.push_back( (char)(0xE0 | (ch >> 12)) );
                dst.push_back( (char)(0x80 | ((ch >> 6) & 0x3F)) );
                dst.push_back( (char)(0x80 | (ch & 0x3F)) );
                break;
            case 2:
                dst.push_back( (char)(0xC0 | (ch >> 6)) );
                dst.push_back( (char)(0x80 | (ch & 0x3F)) );
                break;
            case 1:
                dst.push_back( (char)ch );
                break;
            default:
                IRK_UNREACHABLE();
            }
            u8Cnt++;
        }
    }

    return -1;
}

// get UTF-8 character's "unicode character number"
// return > UTF16_MAX if UTF-8 character is invalid
static inline uint32_t calc_u8_chnum( const uint8_t* us, uint8_t bytes )
{
    // used to remove UTF-8 encoding prefix, q.v. RTF-3926
    static const uint32_t s_Utf8Offset[6] =
    {
        0, 0, 0x3080, 0xE2080, 0x3C82080
    };

    uint32_t ch = *us++;
    switch( bytes )
    {
    case 4:
        if( (*us & 0xC0) != 0x80 )      // invalid UTF8-Tail
            return INT32_MAX;
        ch <<= 6;
        ch += *us++;
    case 3:
        if( (*us & 0xC0) != 0x80 )      // invalid UTF8-Tail
            return INT32_MAX;
        ch <<= 6;
        ch += *us++;
    case 2:
        if( (*us & 0xC0) != 0x80 )      // invalid UTF8-Tail
            return INT32_MAX;
        ch <<= 6;
        ch += *us;
        break;
    case 1:
        return ch;
    case 0:                 // invalid UTF8 char
        return INT32_MAX;
    default:
        IRK_UNREACHABLE();
    }

    // remove UTF-8 encoding prefix
    return ch - s_Utf8Offset[bytes];
}

// get UTF-16 buffer size needed
static int calc_u16_bufsize( const char* u8str )
{
    const uint8_t* us = (const uint8_t*)u8str;
    int i = 0, k = 0;
    while( us[i] != 0 )
    {
        k++;
        if( us[i] < 0x80 )  // ASCII
        {
            i++;
        }
        else
        {
            // get byte count and {unicode character number}
            uint8_t byteCnt = s_Utf8ByteCnt[us[i]>>3];
            uint32_t ch = calc_u8_chnum( us + i, byteCnt );
            if( ch > UTF16_MAX )    // invalid char
                break;
            if( ch > 0xFFFF )       // 4 bytes UTF-16
                k++;
            i += byteCnt;
        }
    }
    return k + 1;
}

// Convert UTF-8 to UTF-16
// Return The number of UTF-16 characters written, or -1 if failed
int str_utf8to16( const char* u8str, char16_t* buff, int size )
{
    if( size <= 0 )     // get buffer size needed
    {
        return calc_u16_bufsize( u8str );
    }

    const uint8_t* us = (const uint8_t*)u8str;
    int i = 0, k = 0;
    while( 1 )
    {
        if( us[i] < 0x80 )          // ASCII
        {
            if( us[i] == 0 )        // Done!
            {
                buff[k] = 0;
                return k;
            }
            if( k + 1 >= size )     // insufficient buffer
            {
                break;
            }
            buff[k++] = us[i++];
        }
        else
        {
            // get byte count and {unicode character number}
            uint8_t byteCnt = s_Utf8ByteCnt[us[i]>>3];
            uint32_t ch = calc_u8_chnum( us + i, byteCnt );
            i += byteCnt;

            if( ch <= 0xFFFF )              // two bytes UTF-16
            {
                // 0xD800 ~ 0xDFFF: reserved region for 4-bytes UTF-16
                if( ch >= 0xD800 && ch <= 0xDFFF )
                    break;
                if( k + 1 >= size )         // insufficient buffer
                    break;
                buff[k++] = (char16_t)ch;
            }
            else if( ch <= UTF16_MAX )      // valid UTF-16
            {
                if( k + 2 >= size )         // insufficient buffer
                    break;

                // encode as 4-bytes UTF-16
                ch -= 0x10000;
                buff[k++] = (char16_t)((ch >> 10) + 0xD800);
                buff[k++] = (char16_t)((ch & 0x3FF) + 0xDC00);
            }
            else    // invalid UTF-8 char
            {
                break;
            }
        }
    }

    buff[k] = 0;
    return -1;
}

// Convert UTF-8 to UTF-16
// Return The number of UTF-16 characters written, or -1 if failed
int str_utf8to16( const char* u8str, std::u16string& dst )
{
    dst.clear();

    const uint8_t* us = (const uint8_t*)u8str;
    int i = 0;
    while( 1 )
    {
        if( us[i] < 0x80 )      // ASCII
        {
            if( us[i] == 0 )    // Done!
                return (int)dst.length();

            dst.push_back( (char16_t)us[i] );
            i++;
        }
        else
        {
            // get byte count and {unicode character number}
            uint8_t byteCnt = s_Utf8ByteCnt[us[i]>>3];
            uint32_t ch = calc_u8_chnum( us + i, byteCnt );
            i += byteCnt;

            if( ch <= 0xFFFF )          // two bytes UTF-16
            {
                // 0xD800 ~ 0xDFFF: reserved region for 4-bytes UTF-16
                if( ch >= 0xD800 && ch <= 0xDFFF )
                    break;
                dst.push_back( (char16_t)ch );
            }
            else if( ch <= UTF16_MAX )  // valid UTF-16
            {
                // encode as 4-bytes UTF-16
                ch -= 0x10000;
                dst.push_back( (char16_t)((ch >> 10) + 0xD800) );
                dst.push_back( (char16_t)((ch & 0x3FF) + 0xDC00) );
            }
            else    // invalid UTF-8 char
            {
                break;
            }
        }
    }

    return -1;
}

#ifdef _WIN32

// under windows, wchar_t == char16_t
static_assert( sizeof(wchar_t) == sizeof(char16_t), "" );

int str_utf16to8( const wchar_t* str, char* buf, int size )
{
    return str_utf16to8( (const char16_t*)str, buf, size );
}
int str_utf16to8( const wchar_t* str, std::string& dst )
{
    return str_utf16to8( (const char16_t*)str, dst );
}

int str_utf8to16( const char* u8str, wchar_t* buff, int size )
{
    return str_utf8to16( u8str, (char16_t*)buff, size );
}
int str_utf8to16( const char* u8str, std::wstring& dst )
{
    dst.clear();

    const uint8_t* us = (const uint8_t*)u8str;
    int i = 0;
    while( 1 )
    {
        if( us[i] < 0x80 )      // ASCII
        {
            if( us[i] == 0 )    // Done!
                return (int)dst.length();
            dst.push_back( (wchar_t)us[i] );
            i++;
        }
        else
        {
            // get UTF-8 byte count and {unicode character number}
            uint8_t byteCnt = s_Utf8ByteCnt[us[i]>>3];
            uint32_t ch = calc_u8_chnum( us + i, byteCnt );
            i += byteCnt;

            if( ch <= 0xFFFF )          // two bytes UTF-16
            {
                // 0xD800 ~ 0xDFFF: reserved region for 4-bytes UTF-16
                if( ch >= 0xD800 && ch <= 0xDFFF )
                    break;
                dst.push_back( (wchar_t)ch );
            }
            else if( ch <= UTF16_MAX )  // valid UTF-16
            {
                // encode as 4-bytes UTF-16
                ch -= 0x10000;
                dst.push_back( (wchar_t)((ch >> 10) + 0xD800) );
                dst.push_back( (wchar_t)((ch & 0x3FF) + 0xDC00) );
            }
            else    // invalid UTF-8 char
            {
                break;
            }
        }
    }

    return -1;
}

#endif

//======================================================================================================================

// return a string with leading and tailing whitespace removed
template<typename CharT>
static std::basic_string<CharT> impl_get_striped( const CharT* text )
{
    std::basic_string<CharT> str;

    // remove leading whitespace
    for( const CharT* s = text; *s != 0; s++ )
    {
        if( !irk::CTypeTraits<CharT>::is_space( *s ) )
        {
            str.assign( s );
            break;
        }
    }
    // remove tailing whitespace
    while( !str.empty() && irk::CTypeTraits<CharT>::is_space( str.back() ) )
    {
        str.pop_back();
    }

    return str;
}
// return a string with leading and tailing specified character removed
template<typename CharT>
static std::basic_string<CharT> impl_get_striped( const CharT* text, CharT cc )
{
    std::basic_string<CharT> str;

    // remove leading specified character
    for( const CharT* s = text; *s != 0; s++ )
    {
        if( *s != cc )
        {
            str.assign( s );
            break;
        }
    }
    // remove tailing specified character
    while( !str.empty() && str.back() == cc )
    {
        str.pop_back();
    }

    return str;
}

// remove leading and tailing whitespace
template<typename CharT>
static void impl_strip( std::basic_string<CharT>& str )
{
    // remove leading whitespace
    int len = static_cast<int>( str.length() );
    int i = 0;
    for( ; i < len; i++ )
    {
        if( !irk::CTypeTraits<CharT>::is_space( str[i] ) )
            break;
    }
    if( i > 0 )
    {
        str.erase( 0, i );
        len -= i;
    }

    // remove tailing whitespace
    for( i = len - 1; i >= 0; i-- )
    {
        if( !irk::CTypeTraits<CharT>::is_space( str[i] ) )
            break;
    }
    if( i + 1 < len )
    {
        str.erase( i + 1 );
    }
}
// remove leading and tailing specified character
template<typename CharT>
static void impl_strip( std::basic_string<CharT>& str, CharT cc )
{
    // remove leading specified character
    int len = static_cast<int>( str.length() );
    int i = 0;
    for( ; i < len; i++ )
    {
        if( str[i] != cc )
            break;
    }
    if( i > 0 )
    {
        str.erase( 0, i );
        len -= i;
    }

    // remove tailing specified character
    for( i = len - 1; i >= 0; i-- )
    {
        if( str[i] != cc )
            break;
    }
    if( i + 1 < len )
    {
        str.erase( i + 1 );
    }
}

// return a string with leading(left) whitespace removed
template<typename CharT>
static std::basic_string<CharT> impl_get_lstriped( const CharT* text )
{
    const CharT* s = text;
    for( ; *s != 0; s++ )
    {
        if( !irk::CTypeTraits<CharT>::is_space( *s ) )
            break;
    }
    return std::basic_string<CharT>( s );
}
// return a string with leading(left) specified character removed
template<typename CharT>
static std::basic_string<CharT> impl_get_lstriped( const CharT* text, CharT cc )
{
    const CharT* s = text;
    for( ; *s != 0; s++ )
    {
        if( *s != cc )
            break;
    }
    return std::basic_string<CharT>( s );
}

// remove leading whitespace
template<typename CharT>
static void impl_lstrip( std::basic_string<CharT>& str )
{
    int len = static_cast<int>( str.length() );
    int i = 0;
    for( ; i < len; i++ )
    {
        if( !irk::CTypeTraits<CharT>::is_space( str[i] ) )
            break;
    }
    if( i > 0 )
    {
        str.erase( 0, i );
    }
}
// remove leading specified character
template<typename CharT>
static void impl_lstrip( std::basic_string<CharT>& str, CharT cc )
{
    int len = static_cast<int>( str.length() );
    int i = 0;
    for( ; i < len; i++ )
    {
        if( str[i] != cc )
            break;
    }
    if( i > 0 )
    {
        str.erase( 0, i );
    }
}

// return a string with tailing(right) whitespace removed
template<typename CharT>
static std::basic_string<CharT> impl_get_rstriped( const CharT* text )
{
    std::basic_string<CharT> str( text );
    int len = static_cast<int>( str.length() );
    int i = len - 1;
    for( ; i >= 0; i-- )
    {
        if( !irk::CTypeTraits<CharT>::is_space( str[i] ) )
            break;
    }
    if( i + 1 < len )
    {
        str.erase( i + 1 );
    }
    return str;
}
// return a string with tailing(right) specified character removed
template<typename CharT>
static std::basic_string<CharT> impl_get_rstriped( const CharT* text, CharT cc )
{
    std::basic_string<CharT> str( text );
    int len = static_cast<int>( str.length() );
    int i = len - 1;
    for( ; i >= 0; i-- )
    {
        if( str[i] != cc )
            break;
    }
    if( i + 1 < len )
    {
        str.erase( i + 1 );
    }
    return str;
}

// remove tailing(right) whitespace
template<typename CharT>
static void impl_rstrip( std::basic_string<CharT>& str )
{
    int len = static_cast<int>( str.length() );
    int i = len - 1;
    for( ; i >= 0; i-- )
    {
        if( !irk::CTypeTraits<CharT>::is_space( str[i] ) )
            break;
    }
    if( i + 1 < len )
    {
        str.erase( i + 1 );
    }
}
// remove tailing(right) specified character
template<typename CharT>
static void impl_rstrip( std::basic_string<CharT>& str, CharT cc )
{
    int len = static_cast<int>( str.length() );
    int i = len - 1;
    for( ; i >= 0; i-- )
    {
        if( str[i] != cc )
            break;
    }
    if( i + 1 < len )
    {
        str.erase( i + 1 );
    }
}

// return a string with lower-case letter
template<typename CharT>
static std::basic_string<CharT> impl_get_lowered( const CharT* text )
{
    std::basic_string<CharT> str( text );
    for( auto& ch : str )
    {
        if( irk::CTypeTraits<CharT>::is_upper(ch) )
            ch |= 0x20;
    }
    return str;
}

// convert to lower-case string
template<typename CharT>
static void impl_tolower( std::basic_string<CharT>& str )
{
    for( auto& ch : str )
    {
        if( irk::CTypeTraits<CharT>::is_upper(ch) )
            ch |= 0x20;
    }
}

// return a string with upper-case letter
template<typename CharT>
static std::basic_string<CharT> impl_get_uppered( const CharT* text )
{
    std::basic_string<CharT> str( text );
    for( auto& ch : str )
    {
        if( irk::CTypeTraits<CharT>::is_lower(ch) )
            ch -= 0x20;
    }
    return str;
}

// convert to upper-case string
template<typename CharT>
static void impl_toupper( std::basic_string<CharT>& str )
{
    for( auto& ch : str )
    {
        if( irk::CTypeTraits<CharT>::is_lower(ch) )
            ch -= 0x20;
    }
}

// return a string with leading and tailing whitespace removed
std::string get_striped( const char* str )
{
    return impl_get_striped( str );
}
std::wstring get_striped( const wchar_t* str )
{
    return impl_get_striped( str );
}
std::u16string get_striped( const char16_t* str )
{
    return impl_get_striped( str );
}
// return a string with leading and tailing specified character removed
std::string get_striped( const char* str, char cc )
{
    return impl_get_striped( str, cc );
}
std::wstring get_striped( const wchar_t* str, wchar_t cc )
{
    return impl_get_striped( str, cc );
}
std::u16string get_striped( const char16_t* str, char16_t cc )
{
    return impl_get_striped( str, cc );
}

// remove leading and tailing whitespace
void str_strip( std::string& str )
{
    impl_strip( str );
}
void str_strip( std::wstring& str )
{
    impl_strip( str );
}
void str_strip( std::u16string& str )
{
    impl_strip( str );
}
// remove leading and tailing specified character
void str_strip( std::string& str, char cc )
{
    impl_strip( str, cc );
}
void str_strip( std::wstring& str, wchar_t cc )
{
    impl_strip( str, cc );
}
void str_strip( std::u16string& str, char16_t cc )
{
    impl_strip( str, cc );
}

// return a string with leading(left) whitespace removed
std::string get_lstriped( const char* str )
{
    return impl_get_lstriped( str );
}
std::wstring get_lstriped( const wchar_t* str )
{
    return impl_get_lstriped( str );
}
std::u16string get_lstriped( const char16_t* str )
{
    return impl_get_lstriped( str );
}
// return a string with leading(left) specified character removed
std::string get_lstriped( const char* str, char cc )
{
    return impl_get_lstriped( str, cc );
}
std::wstring get_lstriped( const wchar_t* str, wchar_t cc )
{
    return impl_get_lstriped( str, cc );
}
std::u16string get_lstriped( const char16_t* str, char16_t cc )
{
    return impl_get_lstriped( str, cc );
}

// remove leading whitespace
void str_lstrip( std::string& str )
{
    impl_lstrip( str );
}
void str_lstrip( std::wstring& str )
{
    impl_lstrip( str );
}
void str_lstrip( std::u16string& str )
{
    impl_lstrip( str );
}
// remove leading specified character
void str_lstrip( std::string& str, char cc )
{
    impl_lstrip( str, cc );
}
void str_lstrip( std::wstring& str, wchar_t cc )
{
    impl_lstrip( str, cc );
}
void str_lstrip( std::u16string& str, char16_t cc )
{
    impl_lstrip( str, cc );
}

// return a string with tailing(right) whitespace removed
std::string get_rstriped( const char* str )
{
    return impl_get_rstriped( str );
}
std::wstring get_rstriped( const wchar_t* str )
{
    return impl_get_rstriped( str );
}
std::u16string get_rstriped( const char16_t* str )
{
    return impl_get_rstriped( str );
}
// return a string with tailing(right) specified character removed
std::string get_rstriped( const char* str, char cc )
{
    return impl_get_rstriped( str, cc );
}
std::wstring get_rstriped( const wchar_t* str, wchar_t cc )
{
    return impl_get_rstriped( str, cc );
}
std::u16string get_rstriped( const char16_t* str, char16_t cc )
{
    return impl_get_rstriped( str, cc );
}

// remove tailing(right) whitespace
void str_rstrip( std::string& str )
{
    impl_rstrip( str );
}
void str_rstrip( std::wstring& str )
{
    impl_rstrip( str );
}
void str_rstrip( std::u16string& str )
{
    impl_rstrip( str );
}
// remove tailing(right) specified character
void str_rstrip( std::string& str, char cc )
{
    impl_rstrip( str, cc );
}
void str_rstrip( std::wstring& str, wchar_t cc )
{
    impl_rstrip( str, cc );
}
void str_rstrip( std::u16string& str, char16_t cc )
{
    impl_rstrip( str, cc );
}

// return a string with lower-case letter
std::string get_lowered( const char* str )
{
    return impl_get_lowered( str );
}
std::wstring get_lowered( const wchar_t* str )
{
    return impl_get_lowered( str );
}
std::u16string get_lowered( const char16_t* str )
{
    return impl_get_lowered( str );
}

// convert to lower-case string
void str_tolower( std::string& str )
{
    impl_tolower( str );
}
void str_tolower( std::wstring& str )
{
    impl_tolower( str );
}
void str_tolower( std::u16string& str )
{
    impl_tolower( str );
}

// return a string with upper-case letter
std::string get_uppered( const char* str )
{
    return impl_get_uppered( str );
}
std::wstring get_uppered( const wchar_t* str )
{
    return impl_get_uppered( str );
}
std::u16string get_uppered( const char16_t* str )
{
    return impl_get_uppered( str );
}

// convert to upper-case string
void str_toupper( std::string& str )
{
    impl_toupper( str );
}
void str_toupper( std::wstring& str )
{
    impl_toupper( str );
}
void str_toupper( std::u16string& str )
{
    impl_toupper( str );
}

}   // namespace irk
