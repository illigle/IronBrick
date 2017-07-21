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

#ifndef _IRONBRICK_STRUTILITY_H_
#define _IRONBRICK_STRUTILITY_H_

#include <stdio.h>
#include <stdarg.h>
#include <wchar.h>
#include <string.h>
#include <string>
#include <utility>
#include "IrkCType.h"

// string case-insensitive compare
#ifdef _MSC_VER
#define stricmp     _stricmp
#define strnicmp    _strnicmp
#define wcsicmp     _wcsicmp
#define wcsnicmp    _wcsnicmp
#else
#define stricmp     strcasecmp
#define strnicmp    strncasecmp
#define wcsicmp     wcscasecmp
#define wcsnicmp    wcsncasecmp
#endif

namespace irk {

// strlen: return character count of null terminated string
inline size_t str_count( const char* str )
{
    return ::strlen( str );
}
inline size_t str_count( const wchar_t* str )
{
    return ::wcslen( str );
}
extern size_t str_count( const char16_t* str );

// strnlen: returns character count of null terminated string but at most maxlen.
inline size_t str_ncount( const char* str, size_t maxlen )
{
    return ::strnlen( str, maxlen );
}
inline size_t str_ncount( const wchar_t* str, size_t maxlen )
{
    return ::wcsnlen( str, maxlen );
}
extern size_t str_ncount( const char16_t* str, size_t maxlen );

// string comparison, return <0, 0, >0
inline int str_compare( const char* str1, const char* str2 )
{
    return ::strcmp( str1, str2 );
}
inline int str_compare( const wchar_t* str1, const wchar_t* str2 )
{
    return ::wcscmp( str1, str2 );
}
extern int str_compare( const char16_t* str1, const char16_t* str2 );

// string copy, the behaviour is like c11 strncpy_s(buf, size, src, size - 1)
// the dest buf will always be null terminated
// return actual character number copied(excluding terminal null), return "size" if dest buf is too small
size_t str_copy( char* buf, size_t size, const char* src );
size_t str_copy( wchar_t* buf, size_t size, const wchar_t* src );
size_t str_copy( char16_t* buf, size_t size, const char16_t* src );

// string concatenation, like strncat, but the result string will always be null terminated
// "dst" shall not be NULL and shall be null terminated
// return length of the result string(excluding terminal null), return "size" if dest buf is too small
size_t str_cat( char* dst, size_t size, const char* src );
size_t str_cat( wchar_t* dst, size_t size, const wchar_t* src );
size_t str_cat( char16_t* dst, size_t size, const char16_t* src );

// hash string to 32 bits unsigned value, using FNV-1a string hash algorithm
uint32_t str_hash( const char* str );
uint32_t str_hash( const wchar_t* str );
uint32_t str_hash( const char16_t* str );

// hash string to "bits" bits unsigned value
inline uint32_t str_hash( const char* str, uint32_t bits )
{
    assert( bits > 1 && bits < 32 );
    uint32_t hv32 = str_hash( str );
    return ((hv32 >> bits) ^ hv32) & ((1 << bits) - 1);
}
inline uint32_t str_hash( const wchar_t* str, uint32_t bits )
{
    assert( bits > 1 && bits < 32 );
    uint32_t hv32 = str_hash( str );
    return ((hv32 >> bits) ^ hv32) & ((1 << bits) - 1);
}
inline uint32_t str_hash( const char16_t* str, uint32_t bits )
{
    assert( bits > 1 && bits < 32 );
    uint32_t hv32 = str_hash( str );
    return ((hv32 >> bits) ^ hv32) & ((1 << bits) - 1);
}

// string format, the behaviour is like snprintf/swprintf
// the result buf will always be null terminated
// return actual character number printed(excluding terminal null) if succeeded
// return -1 if failed(insufficient buffer size or format error)
inline int str_format( char* buf, size_t size, const char* fmt, ... )
{
    if( size > 0 )
    {
        va_list args;
        va_start( args, fmt );
#if defined(_MSC_VER) && _MSC_VER < 1900
        int cnt = _vsnprintf_s( buf, size, _TRUNCATE, fmt, args );
#else
        int cnt = vsnprintf( buf, size, fmt, args );
#endif
        va_end( args );
        if( cnt >= 0 && (size_t)cnt < size )    // succeeded
            return cnt;
        buf[size-1] = 0;    // failed
    }
    return -1;
}
inline int str_format( wchar_t* buf, size_t size, const wchar_t* fmt, ... )
{
    if( size > 0 )
    {
        va_list args;
        va_start( args, fmt );
        int cnt = vswprintf( buf, size, fmt, args );
        va_end( args );
        if( cnt >= 0 && (size_t)cnt < size )    // succeeded
            return cnt;
        buf[size-1] = 0;    // failed
    }
    return -1;
}

//======================================================================================================================

// convert string to integer
// return 0 if no digital character can be found
// if "radix" is 16, parsed as hex number, otherwise parsed as decimal
// NOTE: if string begin with 0x or 0X, always parsed as hex number
// WARNIG: if overflow, result is undefined
int32_t str_to_int( const char* str, int radix = 10 );
int32_t str_to_int( const wchar_t* str, int radix = 10 );
int32_t str_to_int( const char16_t* str, int radix = 10 );
uint32_t str_to_uint( const char* str, int radix = 10 );
uint32_t str_to_uint( const wchar_t* str, int radix = 10 );
uint32_t str_to_uint( const char16_t* str, int radix = 10 );
int64_t str_to_int64( const char* str, int radix = 10 );
int64_t str_to_int64( const wchar_t* str, int radix = 10 );
int64_t str_to_int64( const char16_t* str, int radix = 10 );
uint64_t str_to_uint64( const char* str, int radix = 10 );
uint64_t str_to_uint64( const wchar_t* str, int radix = 10 );
uint64_t str_to_uint64( const char16_t* str, int radix = 10 );

inline int32_t str_to_int( const std::string& str, int radix = 10 )
{
    return str_to_int( str.c_str(), radix );
}
inline int32_t str_to_int( const std::wstring& str, int radix = 10 )
{
    return str_to_int( str.c_str(), radix );
}
inline int32_t str_to_int( const std::u16string& str, int radix = 10 )
{
    return str_to_int( str.c_str(), radix );
}
inline uint32_t str_to_uint( const std::string& str, int radix = 10 )
{
    return str_to_uint( str.c_str(), radix );
}
inline uint32_t str_to_uint( const std::wstring& str, int radix = 10 )
{
    return str_to_uint( str.c_str(), radix );
}
inline uint32_t str_to_uint( const std::u16string& str, int radix = 10 )
{
    return str_to_uint( str.c_str(), radix );
}
inline int64_t str_to_int64( const std::string& str, int radix = 10 )
{
    return str_to_int64( str.c_str(), radix );
}
inline int64_t str_to_int64( const std::wstring& str, int radix = 10 )
{
    return str_to_int64( str.c_str(), radix );
}
inline int64_t str_to_int64( const std::u16string& str, int radix = 10 )
{
    return str_to_int64( str.c_str(), radix );
}
inline uint64_t str_to_uint64( const std::string& str, int radix = 10 )
{
    return str_to_uint64( str.c_str(), radix );
}
inline uint64_t str_to_uint64( const std::wstring& str, int radix = 10 )
{
    return str_to_uint64( str.c_str(), radix );
}
inline uint64_t str_to_uint64( const std::u16string& str, int radix = 10 )
{
    return str_to_uint64( str.c_str(), radix );
}

// convert integer to string, support decimal and hexadecimal output format
// return character number written(excludeing terminal null)
// return -1 if output buffer's size is insufficient
// NOTE: if "radix" is 16, output string begins with 0x
int int_to_str( int32_t val, char* buf,     int size, int radix = 10 );
int int_to_str( int32_t val, wchar_t* buf,  int size, int radix = 10 );
int int_to_str( int32_t val, char16_t* buf, int size, int radix = 10 );
int int_to_str( uint32_t val, char* buf,    int size, int radix = 10 );
int int_to_str( uint32_t val, wchar_t* buf, int size, int radix = 10 );
int int_to_str( uint32_t val, char16_t* buf, int size, int radix = 10 );
int int_to_str( int64_t val, char* buf,     int size, int radix = 10 );
int int_to_str( int64_t val, wchar_t* buf,  int size, int radix = 10 );
int int_to_str( int64_t val, char16_t* buf, int size, int radix = 10 );
int int_to_str( uint64_t val, char* buf,    int size, int radix = 10 );
int int_to_str( uint64_t val, wchar_t* buf, int size, int radix = 10 );
int int_to_str( uint64_t val, char16_t* buf, int size, int radix = 10 );

std::string int_to_string( int32_t val, int radix = 10 );
std::string int_to_string( uint32_t val, int radix = 10 );
std::string int_to_string( int64_t val, int radix = 10 );
std::string int_to_string( uint64_t val, int radix = 10 );
std::wstring int_to_wstring( int32_t val, int radix = 10 );
std::wstring int_to_wstring( uint32_t val, int radix = 10 );
std::wstring int_to_wstring( int64_t val, int radix = 10 );
std::wstring int_to_wstring( uint64_t val, int radix = 10 );
std::u16string int_to_ustring( int32_t val, int radix = 10 );
std::u16string int_to_ustring( uint32_t val, int radix = 10 );
std::u16string int_to_ustring( int64_t val, int radix = 10 );
std::u16string int_to_ustring( uint64_t val, int radix = 10 );

// integer to string converter, return inner buffer
// WARNIG: can not used more than 2 times in single statement
template<typename CharT>
class IntToStrCvt
{
public:
    IntToStrCvt() : m_Idx(0) {}
    const CharT* operator()( int32_t val, int radix = 10 );
    const CharT* operator()( uint32_t val, int radix = 10 );
    const CharT* operator()( int64_t val, int radix = 10 );
    const CharT* operator()( uint64_t val, int radix = 10 );
private:
    int     m_Idx;
    CharT   m_Buff[2][32];
};
extern template class IntToStrCvt<char>;
extern template class IntToStrCvt<wchar_t>;
extern template class IntToStrCvt<char16_t>;

//======================================================================================================================

// return UTF-8 character count in the string, 
// return -1 if the string contains invalid UTF-8 character
extern int str_u8count( const std::string& u8str );
extern int str_u8count( const char* u8str );

// Convert UTF-16 to UTF-8
// "size": The max number of characters can be stored(including terminal null)
// if "size" == 0, this function will return buffer size needed
// return The number of UTF-8 characters written, or -1 if failed
// may be failed if out buffer is insufficient or invalid character is encountered
extern int str_utf16to8( const char16_t* str, char* buf, int size );
inline int str_utf16to8( const std::u16string& str, char* buf, int size )
{
    return str_utf16to8( str.c_str(), buf, size );
}
extern int str_utf16to8( const char16_t* str, std::string& dst );
inline int str_utf16to8( const std::u16string& str, std::string& dst )
{
    return str_utf16to8( str.c_str(), dst );
}

// Convert UTF-8 to UTF-16
// "size": The max number of UTF-16 characters can be stored(including terminal null)
// if "size" == 0, this function will return buffer size needed
// return The number of UTF-16 characters written, or -1 if failed
// may be failed if out buffer is insufficient or invalid character is encountered
extern int str_utf8to16( const char* u8str, char16_t* buf, int size );
inline int str_utf8to16( const std::string& u8str, char16_t* buf, int size )
{
    return str_utf8to16( u8str.c_str(), buf, size );
}
extern int str_utf8to16( const char* u8str, std::u16string& dst );
inline int str_utf8to16( const std::string& u8str, std::u16string& dst )
{
    return str_utf8to16( u8str.c_str(), dst );
}

// under windows, wchar_t == char16_t
#ifdef _WIN32
extern int str_utf16to8( const wchar_t* str, char* buf, int size );
inline int str_utf16to8( const std::wstring& str, char* buf, int size )
{
    return str_utf16to8( str.c_str(), buf, size );
}
extern int str_utf16to8( const wchar_t* str, std::string& dst );
inline int str_utf16to8( const std::wstring& str, std::string& dst )
{
    return str_utf16to8( str.c_str(), dst );
}

extern int str_utf8to16( const char* u8str, wchar_t* buf, int size );
inline int str_utf8to16( const std::string& u8str, wchar_t* buf, int size )
{
    return str_utf8to16( u8str.c_str(), buf, size );
}
extern int str_utf8to16( const char* u8str, std::wstring& dst );
inline int str_utf8to16( const std::string& u8str, std::wstring& dst )
{
    return str_utf8to16( u8str.c_str(), dst );
}
#endif

//======================================================================================================================
// common string utility

// return a string with leading and tailing whitespace(or specified character) removed
std::string get_striped( const char* );
std::wstring get_striped( const wchar_t* );
std::u16string get_striped( const char16_t* );
std::string get_striped( const char*, char charToStrip );
std::wstring get_striped( const wchar_t*, wchar_t charToStrip );
std::u16string get_striped( const char16_t*, char16_t charToStrip );

// remove leading and tailing whitespace(or specified character)
void str_strip( std::string& );
void str_strip( std::wstring& );
void str_strip( std::u16string& );
void str_strip( std::string&, char charToStrip );
void str_strip( std::wstring&, wchar_t charToStrip );
void str_strip( std::u16string&, char16_t charToStrip );

// return a string with leading(left) whitespace(or specified character) removed
std::string get_lstriped( const char* );
std::wstring get_lstriped( const wchar_t* );
std::u16string get_lstriped( const char16_t* );
std::string get_lstriped( const char*, char charToStrip );
std::wstring get_lstriped( const wchar_t*, wchar_t charToStrip );
std::u16string get_lstriped( const char16_t*, char16_t charToStrip );

// remove leading whitespace(or specified character)
void str_lstrip( std::string& );
void str_lstrip( std::wstring& );
void str_lstrip( std::u16string& );
void str_lstrip( std::string&, char charToStrip );
void str_lstrip( std::wstring&, wchar_t charToStrip );
void str_lstrip( std::u16string&, char16_t charToStrip );

// return a string with tailing(right) whitespace(or specified character) removed
std::string get_rstriped( const char* );
std::wstring get_rstriped( const wchar_t* );
std::u16string get_rstriped( const char16_t* );
std::string get_rstriped( const char*, char charToStrip );
std::wstring get_rstriped( const wchar_t*, wchar_t charToStrip );
std::u16string get_rstriped( const char16_t*, char16_t charToStrip );

// remove tailing(right) whitespace(or specified character)
void str_rstrip( std::string& );
void str_rstrip( std::wstring& );
void str_rstrip( std::u16string& );
void str_rstrip( std::string&, char charToStrip );
void str_rstrip( std::wstring&, wchar_t charToStrip );
void str_rstrip( std::u16string&, char16_t charToStrip );

// return a string with lower-case letter
std::string get_lowered( const char* );
std::wstring get_lowered( const wchar_t* );
std::u16string get_lowered( const char16_t* );

// convert to lower-case string
void str_tolower( std::string& );
void str_tolower( std::wstring& );
void str_tolower( std::u16string& );

// return a string with upper-case letter
std::string get_uppered( const char* );
std::wstring get_uppered( const wchar_t* );
std::u16string get_uppered( const char16_t* );

// convert to upper-case string
void str_toupper( std::string& );
void str_toupper( std::wstring& );
void str_toupper( std::u16string& );

// splice a list of strings
template<typename CharT, typename ...Ts>
inline std::basic_string<CharT> str_splice( const std::basic_string<CharT>& s1, Ts&& ...others )
{
    std::basic_string<CharT> str( s1 );
#if __cplusplus >= 201703L
    ((str += std::forward<Ts>(others)), ...);
#else
    char dummy_[] = { ((void)(str += std::forward<Ts>(others)), '0')... };
    (void)dummy_;
#endif
    return str;
}
template<typename CharT, typename ...Ts>
inline std::basic_string<CharT> str_splice( std::basic_string<CharT>&& s1, Ts&& ...others )
{
    std::basic_string<CharT> str( std::move(s1) );
#if __cplusplus >= 201703L
    ((str += std::forward<Ts>(others)), ...);
#else
    char dummy_[] = { ((void)(str += std::forward<Ts>(others)), '0')... };
    (void)dummy_;
#endif
    return str;
}
template<typename CharT, typename ...Ts>
inline std::basic_string<CharT> str_splice( const CharT* s1, Ts&& ...others )
{
    std::basic_string<CharT> str( s1 );
#if __cplusplus >= 201703L
    ((str += std::forward<Ts>(others)), ...);
#else
    char dummy_[] = { ((void)(str += std::forward<Ts>(others)), '0')... };
    (void)dummy_;
#endif
    return str;
}

// join a list of string with the delimit.
// str_join("/", "a", "b", "c") => "a/b/c"
template<typename CharT, typename T1, typename ...Ts>
inline std::basic_string<CharT> str_join( const CharT* delimit, T1&& s1, Ts&& ...others )
{
    std::basic_string<CharT> str( std::forward<T1>(s1) );
#if __cplusplus >= 201703L
    ((str+=delimit, str+=std::forward<Ts>(others)), ...);
#else
    constexpr size_t len = sizeof...(others);
    char dummy_[len+1] = { ((void)(str+=delimit, str+=std::forward<Ts>(others)), '0')... };
    (void)dummy_;
#endif
    return str;
}
template<typename CharT, typename T1, typename ...Ts>
inline std::basic_string<CharT> str_join( const std::basic_string<CharT>& delimit, T1&& s1, Ts&& ...others )
{
    std::basic_string<CharT> str( std::forward<T1>(s1) );
#if __cplusplus >= 201703L
    ((str+=delimit, str+=std::forward<Ts>(others)), ...);
#else
    constexpr size_t len = sizeof...(others);
    char dummy_[len+1] = { ((void)(str+=delimit, str+=std::forward<Ts>(others)), '0')... };
    (void)dummy_;
#endif
    return str;
}

}   // namespace irk
#endif
