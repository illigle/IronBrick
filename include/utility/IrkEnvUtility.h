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

#ifndef _IRONBRICK_ENVUTILITY_H_
#define _IRONBRICK_ENVUTILITY_H_

#include <string>

namespace irk {

// get current process's full path in default local encoding, 
// dir separator is '/' or '\'(depending on OS),
// return empty string if failed
std::string get_proc_path();
// ditto, but in UTF-8 encoding, dir separator is '/'
std::string get_proc_path_u8();
// ditto, but in UTF-16 encoding, dir separator is '/'
std::u16string get_proc_path_u16();

// get directory current process resided in default local encoding,
// dir separator is '/' or '\'(depending on OS),
// return empty string if failed
std::string get_proc_dir();
// ditto, but in UTF-8 encoding, dir separator is '/'
std::string get_proc_dir_u8();
// ditto, but in UTF-16 encoding, dir separator is '/'
std::u16string get_proc_dir_u16();

// get current process's name
// return empty string if failed
std::string get_proc_name();
// ditto, but in UTF-8 encoding
std::string get_proc_name_u8();
// ditto, but in UTF-16 encoding
std::u16string get_proc_name_u16();

// get current user's home directory in default local encoding,
// dir separator is '/' or '\'(depending on OS),
// return empty string if failed
std::string get_home_dir();
// ditto, but in UTF-8 encoding, dir separator is '/'
std::string get_home_dir_u8();
// ditto, but in UTF-16 encoding, dir separator is '/'
std::u16string get_home_dir_u16();

// get current working directory in default local encoding,
// dir separator is '/' or '\'(depending on OS),
// return empty string if failed
std::string get_curr_dir();
// ditto, but in UTF-8 encoding, dir separator is '/'
std::string get_curr_dir_u8();
// ditto, but in UTF-16 encoding, dir separator is '/'
std::u16string get_curr_dir_u16();

// wchar_t version on Windows, dir separator is L'\'
#ifdef _WIN32
std::wstring get_proc_path_ws();
std::wstring get_proc_dir_ws();
std::wstring get_home_dir_ws();
std::wstring get_curr_dir_ws();
#endif

// get environment variable, return empty string if failed
std::string get_envar( const char* name );
std::u16string get_envar( const char16_t* name );

// modify/insert environment variable, return false if failed
// if value == NULL, remove the environment variable
// NOTE: It affects only the environment local to current process, has no effect on system environment variables
bool set_envar( const char* name, const char* value );
bool set_envar( const char16_t* name, const char16_t* value );

// remove environment variable, return false if failed
// NOTE: It affects only the environment local to current process, has no effect on system environment variables
bool remove_envar( const char* name );
bool remove_envar( const char16_t* name );

#ifdef _WIN32
std::wstring get_envar( const wchar_t* name );
bool set_envar( const wchar_t* name, const wchar_t* value );
bool remove_envar( const wchar_t* name );
#endif

}   // namespace irk
#endif
