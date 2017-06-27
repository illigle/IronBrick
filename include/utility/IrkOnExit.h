/*
* This Source Code Form is subject to the terms of the Mozilla Public License Version 2.0.
* If a copy of the MPL was not distributed with this file, 
* You can obtain one at http://mozilla.org/MPL/2.0/.

* Covered Software is provided on an ¡°as is¡± basis, 
* without warranty of any kind, either expressed, implied, or statutory,
* that the Covered Software is free of defects, merchantable, 
* fit for a particular purpose or non-infringing.
 
* Copyright (c) Wei Dongliang <illigle@163.com>.
*/

#ifndef _IRONBRICK_ONEXIT_H_
#define _IRONBRICK_ONEXIT_H_

namespace irk { namespace detail {

template<class Func>
class Scopeguard
{
public:
    Scopeguard( Func func ) : m_owner(true), m_func(func)
    {}
    Scopeguard( Scopeguard&& rhs ) : m_owner(rhs.m_owner), m_func(rhs.m_func)
    {
        rhs.m_owner = false;
    }
    ~Scopeguard()   
    {
        if( m_owner ) { m_func(); }
    }
private:
    Scopeguard(const Scopeguard&) = delete;
    Scopeguard& operator=(const Scopeguard&) = delete;
    bool m_owner;
    Func m_func;
};
template<class Func>
static inline Scopeguard<Func> makescopeguard( Func func )
{
    return Scopeguard<Func>( func );
}

}}  // namespace irk::detail

#define MACROJOIN(x,y)  MACROJOIN2(x,y)
#define MACROJOIN2(x,y) x##y

/* 
* using to make sure resources are released on scope exit
* @exps: any simple statements
* e.g.
* FILE* fp = fopen("123.txt", "w");
* ON_EXIT( fclose(fp) );
*/
#ifdef __COUNTER__
#define ON_EXIT(exps) auto MACROJOIN(_IrKs0gD, __COUNTER__) = irk::detail::makescopeguard( [&]{exps;} );
#else
#define ON_EXIT(exps) auto MACROJOIN(_IrKs0gD, __LINE__) = irk::detail::makescopeguard( [&]{exps;} );
#endif

#endif
