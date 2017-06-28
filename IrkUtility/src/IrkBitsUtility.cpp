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
#include <new>
#include "IrkBitsUtility.h"

namespace irk {

// order 0 unsigned exp golomb code in range [0,254], return 255 if out of range
uint8_t BitsReader::read_ue8()
{
    // read at least 24 bits
    uint32_t data = BE_READ32( m_pCur ) << m_Offset;

    if( data & 0xFF000000 )
    {
        int msbi = msb_index_unzero( data );    // find MSB
        data >>= (msbi + msbi) - 31;
        m_Offset += 63 - (msbi + msbi);
        m_pCur += m_Offset >> 3;
        m_Offset &= 7;
        return static_cast<uint8_t>(data - 1);
    }

    // return invalid value if out of range
    return UINT8_MAX;
}

// order 0 unsigned exp golomb code in range [0,65534], return 65535 if out of range
uint16_t BitsReader::read_ue16()
{
    uint32_t data = this->peek32();

    if( data & 0xFFFF0000 )
    {
        int msbi = msb_index_unzero( data );    // find MSB
        data >>= (msbi + msbi) - 31;
        m_Offset += 63 - (msbi + msbi);
        m_pCur += m_Offset >> 3;
        m_Offset &= 7;
        return static_cast<uint16_t>(data - 1);
    }

    // return invalid value if out of range
    return UINT16_MAX;
}

// order 0 unsigned exp golomb code, return UINT32_MAX if failed
uint32_t BitsReader::read_ue32()
{
    // read 32 bits
    uint32_t data = this->peek32();
    if( data == 0 )
        return UINT32_MAX;  // return invalid value if out of range

    int msbi = msb_index_unzero( data );
    if( msbi >= 16 )        // in the 32 read bits
    {
        data >>= msbi + msbi - 31;
    }
    else    // more bits needed
    {
        int morebits = 31 - (msbi + msbi);
        uint32_t temp = BE_READ32( m_pCur + 4 );
        temp = (temp << m_Offset) | (m_pCur[8] >> (8 - m_Offset));
        data = (data << morebits) | (temp >> (32 - morebits));  
    }
    m_Offset += 63 - (msbi + msbi);
    m_pCur += m_Offset >> 3;
    m_Offset &= 7;
    return data - 1;
}

//======================================================================================================================
#undef BUF_PADDING
#define BUF_PADDING 8

static inline int get_writer_bufsize( int bitCapacity )
{
    if( bitCapacity < 0 || bitCapacity + BUF_PADDING * 8 <= 0 )
        throw std::bad_alloc();
    const int bufSize = 4 * ((bitCapacity + 31) >> 5);
    return bufSize > 64 ? bufSize : 64;     // at least 64 bytes
}

BitsWriter::BitsWriter( int bitCapacity ) : m_pBuf(nullptr), m_BitCapacity(0), m_BitCnt(0)
{
    int bufSize = get_writer_bufsize( bitCapacity );    // required buffer size
    m_pBuf = new uint8_t[bufSize + BUF_PADDING];
    memset( m_pBuf, 0, bufSize + BUF_PADDING );         // zero buffer
    m_BitCapacity = bufSize * 8;
}

// init/reset inner buffer, clear existing data
void BitsWriter::reset( int bitCapacity )
{
    if( bitCapacity == 0 )  // free inner buffer
    {
        delete[] m_pBuf;
        m_BitCapacity = 0;
        m_BitCnt = 0;
    }
    else
    {
        int bufSize = get_writer_bufsize( bitCapacity );    // required buffer size
        if( bufSize * 8 > m_BitCapacity )
        {
            delete[] m_pBuf;
            m_pBuf = new uint8_t[bufSize + BUF_PADDING];
            m_BitCapacity = bufSize * 8;
        }
        memset( m_pBuf, 0, (m_BitCapacity >> 3) + BUF_PADDING );    // zero buffer
        m_BitCnt = 0;
    }
}

// reserve inner buffer size, copy existing data to new buffer
void BitsWriter::reserve( int bitCapacity )
{
    int bufSize = get_writer_bufsize( bitCapacity );    // required buffer size
    if( bufSize * 8 > m_BitCapacity )
    {
        uint8_t* buf = new uint8_t[bufSize + BUF_PADDING];
        memset( buf, 0, bufSize + BUF_PADDING );    // zero buffer

        if( m_BitCnt > 0 )  // copy or clear existing data
            memcpy( buf, m_pBuf, (m_BitCnt + 7) >> 3 );
        delete[] m_pBuf;
        m_pBuf = buf;
        m_BitCapacity = bufSize * 8;
    }
}

// clear existing data
void BitsWriter::clear()
{
    m_BitCnt = 0;
    if( m_BitCapacity > 0 )
        memset( m_pBuf, 0, (m_BitCapacity >> 3) + BUF_PADDING );    // zero buffer
}

// write order 0 unsigned exp-golomb value, requires value < 0xFFFFFFFF
void BitsWriter::write_ue( uint32_t value )
{
    assert( value < UINT32_MAX );

    if( value == 0 )    // the most-happened case
    {
        if( m_BitCnt + 8 > m_BitCapacity )
            this->reserve( m_BitCapacity * 3 / 2 );

        int i = m_BitCnt >> 3;
        m_pBuf[i] |= 1u << ((m_BitCnt & 7) ^ 7);
        m_BitCnt++;
    }
    else if( value < 65535 )    // bit count < 32
    {
        int msbi = msb_index_unzero( value + 1 );
        assert( msbi > 0 && msbi < 16 );
        this->write_bits( value + 1, 2 * msbi + 1 );
    }
    else
    {
        int msbi = msb_index_unzero( value + 1 );
        assert( msbi >= 16 && msbi <= 31 );
        m_BitCnt += 2 * msbi - 31;      // write zeros
        this->write_bits( value + 1, 32 );
    }
}

// write order 0 signed exp-golomb value,
void BitsWriter::write_se( int32_t value )
{
    assert( value <= INT32_MAX && value > INT32_MIN );

    if( value == 0 )    // the most-happened case
    {
        if( m_BitCnt + 8 > m_BitCapacity )
            this->reserve( m_BitCapacity * 3 / 2 );

        int i = m_BitCnt >> 3;
        m_pBuf[i] |= 1u << ((m_BitCnt & 7) ^ 7);
        m_BitCnt++;
    }
    else
    {
        int32_t msk = value >> 31;          // sign mask: 0 or -1
        int32_t val = (value ^ msk) - msk;  // abs( value )
        uint32_t code = (val << 1) - msk;   // convert to unsigned code

        if( code < 65535 )                  // bit count < 32
        {
            int msbi = msb_index_unzero( code );
            assert( msbi > 0 && msbi < 16 );
            this->write_bits( code, 2 * msbi + 1 );
        }
        else
        {
            int msbi = msb_index_unzero( code );
            assert( msbi >= 16 && msbi <= 31 );
            m_BitCnt += 2 * msbi - 31;      // write zeros
            this->write_bits( code, 32 );
        }
    }
}

// append bits
void BitsWriter::append( const uint8_t* data, int bitCnt )
{
    if( bitCnt <= 0 )
        return;

    if( bitCnt > m_BitCapacity - m_BitCnt ) // buffer is not big enough
    {
        this->reserve( bitCnt + m_BitCapacity );
    }
    
    if( m_BitCnt & 7 )  // current write positon is not byte-aligned
    {
        const uint8_t* src = data;
        uint8_t* dst = m_pBuf + (m_BitCnt >> 3);
        const int rhs = (m_BitCnt & 7);
        const int lhs = 8 - rhs;            // empty bits in current write byte

        *dst |= (uint8_t)(src[0] >> rhs);   // make the first write byte aligned
        int bits = bitCnt - lhs;            // remain bits to write

        for( ; bits > rhs; bits -= 8 )
        {
            *(++dst) = (src[0] << lhs) | (src[1] >> rhs);
            ++src;
        }
        if( bits > 0 )
        {
            *(++dst) = (src[0] << lhs);
            *dst &= (uint8_t)(0xFF00 >> bits);
        }
        else
        {
            *dst &= (uint8_t)(0xFF00 >> (8 + bits));
        }
    }
    else    // current write positon is byte-aligned
    {
        uint8_t* dst = m_pBuf + (m_BitCnt >> 3);
        const int bytes = bitCnt >> 3;
        memcpy( dst, data, bytes );
        if( bitCnt & 7 )
            dst[bytes] = data[bytes] & (uint8_t)(0xFF00 >> (bitCnt & 7));
    }

    m_BitCnt += bitCnt;
}

} // namespace irk
