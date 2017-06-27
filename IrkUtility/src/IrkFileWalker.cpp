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

#include <string.h>
#include "IrkFileWalker.h"

namespace irk {

#ifdef _MSC_VER
#pragma intrinsic( strlen )
#else
#define strlen __builtin_strlen
#endif

// get next line, NULL is returned to indicate an error or an end-of-file condition
// if pLen != NULL, *pLen = line's length
// NOTE: if the line's length is more than 64M, NULL is returned
// NOTE: The newline character is discarded
const char* FileWalker::next_line( int* pLen )
{
    if( pLen )
        *pLen = 0;
    if( !m_File )
        return nullptr;

    // try read one line
    if( !::fgets( m_Buf, BUFSZ, m_File ) )
        return nullptr;

    // get length
    int len = static_cast<int>( strlen(m_Buf) );
    if( len == 0 )
        return nullptr;

    if( m_Buf[len-1] == '\n' )  // Done!
    {
        if( pLen )
            *pLen = len - 1;
        m_Buf[len-1] = 0;       // discard newline character
        return m_Buf;
    }
    else
    {
        if( feof( m_File ) )    // end of file
        {
            if( pLen )
                *pLen = len;
            return m_Buf;
        }
        else // this line is very long
        {
            m_LongLine.assign( m_Buf, len );
            return this->getlongline( pLen );
        }
    }
}

const char* FileWalker::getlongline( int* pLen )
{
    assert( m_File );
    assert( m_LongLine.back() != '\n' );

    while( 1 )
    {
        if( !::fgets( m_Buf, BUFSZ, m_File ) )
            break;

        m_LongLine += m_Buf;
        if( m_LongLine.back() == '\n' )     // done!
        {
            m_LongLine.pop_back();          // discard newline character
            break;
        }
        else if( m_LongLine.length() > INSANELINESZ )   // insane text file
        {
            m_LongLine.clear();
            m_LongLine.shrink_to_fit();
            if( pLen )
                *pLen = 0;
            return nullptr;
        }
    }

    if( pLen )
        *pLen = static_cast<int>( m_LongLine.length() );
    return m_LongLine.c_str();
}

} // namespace irk
