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

#ifndef _IRONBRICK_RATIONAL_H_
#define _IRONBRICK_RATIONAL_H_

#include "IrkContract.h"

namespace irk {

// rational number
struct Rational
{
    int num;    // numerator
    int den;    // denominator
};

inline bool operator==(const Rational& x, const Rational& y)
{
    irk_expect( x.den != 0 && y.den != 0 );
    return (int64_t)x.num * y.den == (int64_t)y.num * x.den;
}

inline bool operator!=(const Rational& x, const Rational& y)
{
    irk_expect( x.den != 0 && y.den != 0 );
    return (int64_t)x.num * y.den != (int64_t)y.num * x.den;
}

inline bool operator<(const Rational& x, const Rational& y)
{
    irk_expect( x.den != 0 && y.den != 0 );
    int64_t diff = (int64_t)x.num * y.den - (int64_t)y.num * x.den;
    return (diff == 0 ? false : (diff ^ x.den ^ y.den) < 0);
}

inline bool operator<=(const Rational& x, const Rational& y)
{
    irk_expect( x.den != 0 && y.den != 0 );
    int64_t diff = (int64_t)x.num * y.den - (int64_t)y.num * x.den;
    return (diff == 0 ? true : (diff ^ x.den ^ y.den) < 0);
}

inline bool operator>(const Rational& x, const Rational& y)
{
    irk_expect( x.den != 0 && y.den != 0 );
    int64_t diff = (int64_t)x.num * y.den - (int64_t)y.num * x.den;
    return (diff == 0 ? false : (diff ^ x.den ^ y.den) >= 0);
}

inline bool operator>=(const Rational& x, const Rational& y)
{
    irk_expect( x.den != 0 && y.den != 0 );
    int64_t diff = (int64_t)x.num * y.den - (int64_t)y.num * x.den;
    return (diff == 0 ? true : (diff ^ x.den ^ y.den) >= 0);
}

// abs
inline Rational to_abs( const Rational& x )
{
    int numMsk = x.num >> 31;   // 0 or -1
    int denMsk = x.den >> 31;   // 0 or -1
    return {(x.num ^ numMsk) - numMsk, (x.den ^ denMsk) - denMsk};
}

// convert to double
inline double to_double( const Rational& x )
{
    irk_expect( x.den != 0 );
    return (double)x.num / x.den;
}

// convert to double
inline float to_float( const Rational& x )
{
    irk_expect( x.den != 0 );
    return (float)x.num / (float)x.den;
}

// calulate integer GCD(Greatest Common Divisor)
template<typename Ty>
int calc_GCD( Ty a, Ty b )
{
    while( b )
    {
        Ty t = b;
        b = a % b;
        a = t;
    }
    return a;
}

// normalize
inline void normalize( Rational& x )
{
    irk_expect( x.den != 0 );
    int gcd = calc_GCD( x.num, x.den );
    x.num /= gcd;
    x.den /= gcd;
}

} // namespace irk
#endif
