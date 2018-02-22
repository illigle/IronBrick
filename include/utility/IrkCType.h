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

#ifndef _IRONBRICK_CTYPE_H_
#define _IRONBRICK_CTYPE_H_

#include "IrkCommon.h"

//=============== Character type of "C" local ===============

constexpr uint8_t kCTypeCtrl = 0x1;     // 0x1-0x1F, 0x7F
constexpr uint8_t kCTypeSpace = 0x2;    // 0x9-0xD 0x20
constexpr uint8_t kCTypeDigit = 0x4;    // 0-9
constexpr uint8_t kCTypeHexChr = 0x8;   // 0-9, a-f, A-F
constexpr uint8_t kCTypeLower = 0x10;   // a-z
constexpr uint8_t kCTypeUpper = 0x20;   // A-Z
constexpr uint8_t kCTypePunct = 0x40;   // punctuation

// ctype table
constexpr uint8_t s_IrkCTypeTab[256] =
{
    0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x03, 0x03, 0x03, 0x03, 0x03, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x02, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
    0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
    0x40, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x40, 0x40, 0x40, 0x40, 0x40,
    0x40, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x40, 0x40, 0x40, 0x40, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

#define IRK_CTYPE_IS(chr, msk)  ((s_IrkCTypeTab[(uint8_t)(chr)] & (msk)) != 0)
// is control character(0x1-0x1F, 0x7F)
#define IS_CNTRL(chr)           IRK_CTYPE_IS(chr, kCTypeCtrl)
// is whitespace(0x9-0xD 0x20)
#define IS_SPACE(chr)           IRK_CTYPE_IS(chr, kCTypeSpace)
// is control character or whitespace
#define IS_CTLWS(chr)           IRK_CTYPE_IS(chr, (kCTypeCtrl | kCTypeSpace))
// is decimal digit(0-9)
#define IS_DIGIT(chr)           IRK_CTYPE_IS(chr, kCTypeDigit)
// is hex digit(0-9, a-f, A-F)
#define IS_XDIGIT(chr)          IRK_CTYPE_IS(chr, kCTypeHexChr)
// is lowercase letter(a-z)
#define IS_LOWER(chr)           IRK_CTYPE_IS(chr, kCTypeLower)
// is uppercase letter(A-Z)
#define IS_UPPER(chr)           IRK_CTYPE_IS(chr, kCTypeUpper)
// is alphabet(a-z, A-Z)
#define IS_ALPHA(chr)           IRK_CTYPE_IS(chr, (kCTypeLower | kCTypeUpper))
// is alphabet or digit(a-z, A-Z, 0-9)
#define IS_ALNUM(chr)           IRK_CTYPE_IS(chr, (kCTypeDigit | kCTypeLower | kCTypeUpper))
// is punctuation
#define IS_PUNCT(chr)           IRK_CTYPE_IS(chr, kCTypePunct)

// ditto, but can be used for wide character(wchar_t, char16_t)
#define IRK_WCTYPE_IS(chr, msk) (((chr) & 0x7F)==(chr) && (s_IrkCTypeTab[(uint8_t)(chr)] & (msk)) != 0)
#define IS_WCNTRL(chr)          IRK_WCTYPE_IS(chr, kCTypeCtrl)
#define IS_WSPACE(chr)          IRK_WCTYPE_IS(chr, kCTypeSpace)
#define IS_WCTLWS(chr)          IRK_WCTYPE_IS(chr, (kCTypeCtrl | kCTypeSpace))
#define IS_WDIGIT(chr)          IRK_WCTYPE_IS(chr, kCTypeDigit)
#define IS_WXDIGIT(chr)         IRK_WCTYPE_IS(chr, kCTypeHexChr)
#define IS_WLOWER(chr)          IRK_WCTYPE_IS(chr, kCTypeLower)
#define IS_WUPPER(chr)          IRK_WCTYPE_IS(chr, kCTypeUpper)
#define IS_WALPHA(chr)          IRK_WCTYPE_IS(chr, (kCTypeLower | kCTypeUpper))
#define IS_WALNUM(chr)          IRK_WCTYPE_IS(chr, (kCTypeDigit | kCTypeLower | kCTypeUpper))
#define IS_WPUNCT(chr)          IRK_WCTYPE_IS(chr, kCTypePunct)

namespace irk {

template<typename CharT>
struct CTypeTraits
{
    static bool is_cntrl(CharT ch)  { return IS_WCNTRL(ch); }   // WARNING: 0 is not deemed as control char
    static bool is_space(CharT ch)  { return IS_WSPACE(ch); }
    static bool is_ctlws(CharT ch)  { return IS_WCTLWS(ch); }
    static bool is_digit(CharT ch)  { return IS_WDIGIT(ch); }
    static bool is_xdigit(CharT ch) { return IS_WXDIGIT(ch); }
    static bool is_lower(CharT ch)  { return IS_WLOWER(ch); }
    static bool is_upper(CharT ch)  { return IS_WUPPER(ch); }
    static bool is_alpha(CharT ch)  { return IS_WALPHA(ch); }
    static bool is_alnum(CharT ch)  { return IS_WALNUM(ch); }
    static bool is_punct(CharT ch)  { return IS_WPUNCT(ch); }
};
template<>
struct CTypeTraits<char>
{
    static bool is_cntrl(char ch)   { return IS_CNTRL(ch); }    // WARNING: 0 is not deemed as control char
    static bool is_space(char ch)   { return IS_SPACE(ch); }
    static bool is_ctlws(char ch)   { return IS_CTLWS(ch); }
    static bool is_digit(char ch)   { return IS_DIGIT(ch); }
    static bool is_xdigit(char ch)  { return IS_XDIGIT(ch); }
    static bool is_lower(char ch)   { return IS_LOWER(ch); }
    static bool is_upper(char ch)   { return IS_UPPER(ch); }
    static bool is_alpha(char ch)   { return IS_ALPHA(ch); }
    static bool is_alnum(char ch)   { return IS_ALNUM(ch); }
    static bool is_punct(char ch)   { return IS_PUNCT(ch); }
};

}   // namespace irk
#endif
