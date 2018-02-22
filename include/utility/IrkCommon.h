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

#ifndef _IRONBRICK_COMMON_H_
#define _IRONBRICK_COMMON_H_

// disable VC CRT deprecation warning caused by security enhancement
#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#include <stdint.h>
#include <stddef.h>
#include <assert.h>

#if defined(_MSC_VER) && defined(_M_X64)        /* VC */
#define IRK_ARCH_X64 1
#elif defined(__clang__) && defined(__x86_64__)
#define IRK_ARCH_X64 1
#elif defined(__GNUC__) && defined(__x86_64__)
#define IRK_ARCH_X64 1
#endif

#ifdef _MSC_VER                                 /* VC */
#define IRK_FCINLINE    __forceinline
#define IRK_NOINLINE    __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#define IRK_FCINLINE    inline __attribute__((always_inline))
#define IRK_NOINLINE    __attribute__((noinline))
#else
#define IRK_FCINLINE    inline
#define IRK_NOINLINE
#endif

#if defined(_MSC_VER) && !defined(_M_X64)       /* VC */
#define IRK_FASTCALL    __fastcall
#elif defined(__clang__) && !defined(__x86_64__)
#define IRK_FASTCALL    __fastcall
#elif defined(__GNUC__) && !defined(__x86_64__)
#define IRK_FASTCALL    __attribute__((fastcall))
#else
#define IRK_FASTCALL
#endif

// unreachable hint
#ifdef _MSC_VER
#define IRK_UNREACHABLE()   __assume(0)
#elif defined(__GNUC__) || defined(__clang__)
#define IRK_UNREACHABLE()   __builtin_unreachable()
#else
#define IRK_UNREACHABLE()   assert(0)
#endif

// DLL export, import
#ifdef _MSC_VER
#define IRK_EXPORT  __declspec(dllexport)
#define IRK_IMPORT  __declspec(dllimport)
#define IRK_HIDDEN 
#elif defined(__GNUC__) || defined(__clang__)
#define IRK_EXPORT  __attribute__((visibility("default")))
#define IRK_IMPORT  __attribute__((visibility("default")))
#define IRK_HIDDEN  __attribute__((visibility("hidden")))
#else
#define IRK_EXPORT 
#define IRK_IMPORT 
#define IRK_HIDDEN 
#endif

//======================================================================================================================
#ifdef __cplusplus

// require C++14 features
#ifdef _MSC_VER                 /* VC */
#if _MSC_VER < 1900
#error "require Visual Stdio 2015 or above"
#endif
#elif __cplusplus < 201402
#error "require C++14 or above"
#endif

class IrkNocopy     // object which can NOT be copied
{
public:
    IrkNocopy(const IrkNocopy&) = delete;
    IrkNocopy& operator=(const IrkNocopy&) = delete;
protected:
    IrkNocopy() = default;
};

#endif      /* __cplusplus */

#endif      /* _IRONBRICK_COMMON_H_ */
