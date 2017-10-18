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

#ifndef _AVS_BITSTREAM_H_
#define _AVS_BITSTREAM_H_

#include "IrkBitsUtility.h"

namespace irk_avs_dec {

// AVS+ 码流解析读取器
class AvsBitStream : public irk::BitsReader
{
public:
    AvsBitStream() : irk::BitsReader() {}
    AvsBitStream( const uint8_t* buff, size_t size, size_t offset=0 ) : irk::BitsReader(buff, size, offset) {}

    // 特殊函数, 查看后续 25~32 位数据
    uint32_t peek()
    {
        uint32_t value = BE_READ32( m_pCur );
        return value << m_Offset;
    }

    // 判断是否已到 slice 末尾
    bool is_end_of_slice() const
    {
        if( m_pCur < m_pEnd - 1 )
            return false;
        if( (m_pCur >= m_pEnd) || (((m_pCur[0] << m_Offset) & 0xFF) == 0x80) )
            return true;
        return false;
    }

    // order k unsigned exp golomb code
    // 根据 AVS 标准, 比特位深为 8 时正确的 EGK 编码长度不可能超过 25 比特
    uint16_t read_egk( int k )
    {
        uint32_t data = BE_READ32( m_pCur );
        data <<= m_Offset;
        int midx = irk::msb_index_unzero( data | 0x80000 );
        midx += midx - k;
        data >>= midx - 31;
        m_Offset += 63 - midx;
        m_pCur += m_Offset >> 3;
        m_Offset &= 7;
        return static_cast<uint16_t>(data - (1 << k));
    }
};

// 这个类专门用以处理 AVS 伪起始码
class AvsBitWriter : IrkNocopy
{
public:
    AvsBitWriter() : m_pBuf(nullptr), m_pCur(nullptr), m_Offset(0) {}

    // 初始化, 外界传递内存, 本类不负责内存管理, 不检查是否越界
    void setup( uint8_t* buf )
    { 
        m_pBuf = m_pCur = buf;

        // 根据标准丢弃后 2 个比特: 0x2 -> 0x0
        assert( buf[0] == 0x2 );
        m_Offset = 6;
        buf[0] = 0;
    }

    void write_6bits( uint8_t byte )
    {
        byte &= 0xFC;
        m_pCur[0] |= byte >> m_Offset;
        m_pCur[1] = byte << (8 - m_Offset);
        m_pCur += (m_Offset + 6) >> 3;
        m_Offset = (m_Offset + 6) & 7;
    }

    void write_byte( uint8_t byte )
    {
        m_pCur[0] |= byte >> m_Offset;
        m_pCur[1] = byte << (8 - m_Offset);
        m_pCur++;
    }

    void make_byte_aligned()
    {
        m_pCur += (m_Offset + 7) >> 3;
        m_Offset = 0;
    }

    int get_size() const
    {
        return static_cast<int>(m_pCur - m_pBuf);
    }

private:
    uint8_t*    m_pBuf;
    uint8_t*    m_pCur;
    uint32_t    m_Offset;   // [0, 7]
};

}   // namespace irk_avs_dec
#endif
