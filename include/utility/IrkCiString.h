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

#ifndef _IRONBRICK_CISTRING_H_
#define _IRONBRICK_CISTRING_H_

#include <ctype.h>
#include <wctype.h>
#include "IrkStringUtility.h"

// implement case-insensitive string

namespace irk {

template<typename CharT>
struct ci_char_traits;

template<>
struct ci_char_traits<char> : public std::char_traits<char>
{
    static bool eq(char c1, char c2) noexcept
    {
        return ::tolower(c1) == ::tolower(c2);
    }
    static bool lt(char c1, char c2) noexcept
    {
        return ::tolower(c1) < ::tolower(c2);
    }
    static int compare(const char* s1, const char* s2, size_t n) noexcept
    {
        return strnicmp(s1, s2, n);
    }
    static int compare(const char* s1, const char* s2) noexcept
    {
        return stricmp(s1, s2);
    }
    static const char* find(const char* str, size_t n, char ch) noexcept
    {
        char lc = ::tolower(ch);
        for (size_t i = 0; i < n; i++)
        {
            if (::tolower(str[i]) == lc)
                return str + i;
        }
        return nullptr;
    }
};

template<>
struct ci_char_traits<wchar_t> : public std::char_traits<wchar_t>
{
    static bool eq(wchar_t c1, wchar_t c2) noexcept
    {
        return ::towlower(c1) == ::towlower(c2);
    }
    static bool lt(wchar_t c1, wchar_t c2) noexcept
    {
        return ::towlower(c1) < ::towlower(c2);
    }
    static int compare(const wchar_t* s1, const wchar_t* s2, size_t n) noexcept
    {
        return wcsnicmp(s1, s2, n);
    }
    static int compare(const wchar_t* s1, const wchar_t* s2) noexcept
    {
        return wcsicmp(s1, s2);
    }
    static const wchar_t* find(const wchar_t* str, size_t n, wchar_t ch) noexcept
    {
        wchar_t lc = ::towlower(ch);
        for (size_t i = 0; i < n; i++)
        {
            if ((wchar_t)::towlower(str[i]) == lc)
                return str + i;
        }
        return nullptr;
    }
};

template<>
struct ci_char_traits<char16_t> : public std::char_traits<char16_t>
{
    static char16_t to_lower(char16_t ch) noexcept
    {
        return CTypeTraits<char16_t>::is_upper(ch) ? (ch | 0x20) : ch;
    }
    static bool eq(char16_t c1, char16_t c2) noexcept
    {
        return to_lower(c1) == to_lower(c2);
    }
    static bool lt(char16_t c1, char16_t c2) noexcept
    {
        return to_lower(c1) < to_lower(c2);
    }
    static int compare(const char16_t* s1, const char16_t* s2, size_t n) noexcept
    {
        for (size_t i = 0; i < n; i++)
        {
            char16_t c1 = to_lower(s1[i]);
            char16_t c2 = to_lower(s2[i]);
            if (c1 != c2)
                return c1 < c2 ? -1 : 1;
        }
        return 0;
    }
    static int compare(const char16_t* s1, const char16_t* s2) noexcept
    {
        while (1)
        {
            char16_t c1 = to_lower(*s1++);
            char16_t c2 = to_lower(*s2++);
            if (c1 != c2)
                return c1 < c2 ? -1 : 1;
            else if (c1 == 0)
                return 0;
        }
        return 0;
    }
    static const char16_t* find(const char16_t* str, size_t n, char16_t ch) noexcept
    {
        char16_t lc = to_lower(ch);
        for (size_t i = 0; i < n; i++)
        {
            if (to_lower(str[i]) == lc)
                return str + i;
        }
        return nullptr;
    }
};

// case-insensitive string
template<typename CharT>
using basic_ci_string = std::basic_string<CharT, ci_char_traits<CharT> >;

// case-insensitive comparision with std::string
template<typename CharT>
inline bool operator==(const basic_ci_string<CharT>& s1, const std::basic_string<CharT>& s2)
{
    return ci_char_traits<CharT>::compare(s1.c_str(), s2.c_str()) == 0;
}
template<typename CharT>
inline bool operator==(const std::basic_string<CharT>& s2, const basic_ci_string<CharT>& s1)
{
    return ci_char_traits<CharT>::compare(s1.c_str(), s2.c_str()) == 0;
}
template<typename CharT>
inline bool operator!=(const basic_ci_string<CharT>& s1, const std::basic_string<CharT>& s2)
{
    return ci_char_traits<CharT>::compare(s1.c_str(), s2.c_str()) != 0;
}
template<typename CharT>
inline bool operator!=(const std::basic_string<CharT>& s2, const basic_ci_string<CharT>& s1)
{
    return ci_char_traits<CharT>::compare(s1.c_str(), s2.c_str()) != 0;
}
template<typename CharT>
inline bool operator<(const basic_ci_string<CharT>& s1, const std::basic_string<CharT>& s2)
{
    return ci_char_traits<CharT>::compare(s1.c_str(), s2.c_str()) < 0;
}
template<typename CharT>
inline bool operator<(const std::basic_string<CharT>& s2, const basic_ci_string<CharT>& s1)
{
    return ci_char_traits<CharT>::compare(s1.c_str(), s2.c_str()) < 0;
}
template<typename CharT>
inline bool operator>(const basic_ci_string<CharT>& s1, const std::basic_string<CharT>& s2)
{
    return ci_char_traits<CharT>::compare(s1.c_str(), s2.c_str()) > 0;
}
template<typename CharT>
inline bool operator>(const std::basic_string<CharT>& s2, const basic_ci_string<CharT>& s1)
{
    return ci_char_traits<CharT>::compare(s1.c_str(), s2.c_str()) > 0;
}
template<typename CharT>
inline bool operator<=(const basic_ci_string<CharT>& s1, const std::basic_string<CharT>& s2)
{
    return ci_char_traits<CharT>::compare(s1.c_str(), s2.c_str()) <= 0;
}
template<typename CharT>
inline bool operator<=(const std::basic_string<CharT>& s2, const basic_ci_string<CharT>& s1)
{
    return ci_char_traits<CharT>::compare(s1.c_str(), s2.c_str()) <= 0;
}
template<typename CharT>
inline bool operator>=(const basic_ci_string<CharT>& s1, const std::basic_string<CharT>& s2)
{
    return ci_char_traits<CharT>::compare(s1.c_str(), s2.c_str()) >= 0;
}
template<typename CharT>
inline bool operator>=(const std::basic_string<CharT>& s2, const basic_ci_string<CharT>& s1)
{
    return ci_char_traits<CharT>::compare(s1.c_str(), s2.c_str()) >= 0;
}

// case-insensitive equal
template<typename StrT>
struct ci_equal_to
{
    bool operator()(const StrT& s1, const StrT& s2) const noexcept
    {
        typedef typename StrT::value_type CharT;
        return ci_char_traits<CharT>::compare(s1.c_str(), s2.c_str()) == 0;
    }
};
// case-insensitive less
template<typename StrT>
struct ci_less
{
    bool operator()(const StrT& s1, const StrT& s2) const noexcept
    {
        typedef typename StrT::value_type CharT;
        return ci_char_traits<CharT>::compare(s1.c_str(), s2.c_str()) < 0;
    }
};
// case-insensitive greater
template<typename StrT>
struct ci_greater
{
    bool operator()(const StrT& s1, const StrT& s2) const noexcept
    {
        typedef typename StrT::value_type CharT;
        return ci_char_traits<CharT>::compare(s1.c_str(), s2.c_str()) > 0;
    }
};

// case-insensitive hash
template<typename StrT>
struct ci_hash
{
    size_t operator()(const StrT& str) const
    {
        typedef typename StrT::value_type CharT;
        std::basic_string<CharT> lowered = get_lowered(str.c_str());
        return std::hash<std::basic_string<CharT>>()(lowered);
    }
};

// case-insensitive string with conversion between std string type
template<typename CharT>
class ext_ci_string : public basic_ci_string<CharT>
{
public:
    using std::basic_string<CharT, ci_char_traits<CharT> >::basic_string; // reusing standard constructor
    using stdstring = std::basic_string<CharT>;

    // convertion between std string type
    explicit ext_ci_string(const stdstring& str) : basic_ci_string<CharT>(str.data(), str.length())
    {}
    operator stdstring() const
    {
        return stdstring(this->data(), this->length());
    }
    stdstring stdstr() const
    {
        return stdstring(this->data(), this->length());
    }
    ext_ci_string& operator=(const stdstring& str)
    {
        this->assign(str.data(), str.length());
        return *this;
    }
    ext_ci_string& operator=(const CharT* cstr)
    {
        this->assign(cstr);
        return *this;
    }
};

using ci_string = ext_ci_string<char>;
using ci_wstring = ext_ci_string<wchar_t>;
using ci_u16string = ext_ci_string<char16_t>;

}   // namespace irk

//======================================================================================================================
namespace std {

template<typename CharT>
struct hash<irk::basic_ci_string<CharT>>
{
    size_t operator()(const irk::basic_ci_string<CharT>& str) const
    {
        std::basic_string<CharT> lowered = irk::get_lowered(str.c_str());
        return std::hash<std::basic_string<CharT>>()(lowered);
    }
};

template<typename CharT>
struct hash<irk::ext_ci_string<CharT>>
{
    size_t operator()(const irk::ext_ci_string<CharT>& str) const
    {
        std::basic_string<CharT> lowered = irk::get_lowered(str.c_str());
        return std::hash<std::basic_string<CharT>>()(lowered);
    }
};

}

#endif
