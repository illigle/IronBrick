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

#ifndef _AVS_AECPARSER_H_
#define _AVS_AECPARSER_H_

#include "AvsBitstream.h"

namespace irk_avs_dec {

struct AecDecCtx
{
    int8_t  mps;
    int8_t  cycNo;
    int16_t lgPmps;
};

class AvsAecParser : IrkNocopy
{
public:
    AvsAecParser() : m_pBuf(nullptr), m_pCur(nullptr), m_pEnd(nullptr), m_invScan(nullptr), m_weightQM(nullptr) {}

    // 设置逆扫描和量化矩阵
    void set_scan_quant_matrix(const uint8_t* invScan, const uint8_t* wqm)
    {
        m_invScan = invScan;
        m_weightQM = wqm;
    }

    // 设置比特流
    void set_buffer(const uint8_t* buff, uint32_t size)
    {
        assert(size > 0 && size < 500000000);
        m_pBuf = buff;
        m_pEnd = buff + size;
        m_pCur = buff;
        m_Offset = 0;
    }

    // 解析器初始化
    void init();

    // 读取下一个 bit
    uint8_t read1()
    {
        uint8_t value = m_pCur[0] >> (m_Offset ^ 7);
        m_Offset += 1;
        m_pCur += m_Offset >> 3;
        m_Offset &= 7;
        return (value & 1);
    }

    // 读取指定位数, 要求 bitCnt > 0 && bitCnt <= 25
    uint32_t read_bits(uint32_t bitCnt)
    {
        assert(bitCnt > 0 && bitCnt <= 25);
        uint32_t value = BE_READ32(m_pCur);
        value = (value << m_Offset) >> (32 - bitCnt);
        m_Offset += bitCnt;
        m_pCur += m_Offset >> 3;
        m_Offset &= 7;
        return value;
    }

    // 当前读取位置是否字节对其
    bool byte_aligned() const
    {
        return m_Offset == 0;
    }

    // 使下次读取从字节对齐处开始
    void make_byte_aligned()
    {
        m_pCur += (m_Offset + 7) >> 3;
        m_Offset = 0;
    }

    // 实现标准定义 is_end_of_slice()
    bool is_end_of_slice() const
    {
        if (m_pCur < m_pEnd - 1)   // 快速返回
            return false;
        if ((m_pCur >= m_pEnd) || (((m_pCur[0] << m_Offset) & 0xFF) == 0x80))
            return true;
        return false;
    }

    // decode_decision, contextWeighting == 0
    uint8_t dec_decision(int cIdx);

    // decode_decision, contextWeighting == 1
    uint8_t dec_decision2(int cIdx1, int cIdx2);

    // decode_decision until '1', return '0' count
    int dec_zero_cnt(int cIdx, int maxCnt);

    // decode_bypass
    uint8_t dec_bypass();

    // decode_aec_stuffing_bit
    uint8_t dec_stuffing_bit();

    // intra_chroma_pred_mode()
    uint8_t dec_intra_chroma_pred_mode(int ctxInc);

    // cbp
    uint8_t dec_cbp(uint8_t leftCbp, uint8_t topCbp);

    // mb_skip_run
    int dec_mb_skip_run();

    // mb_type of P Slice
    int dec_mb_type_P();

    // mb_type of B Slice
    int dec_mb_type_B(int ctxInc);

    // mb_part_type of B Slice
    uint8_t dec_mb_part_type();

    // mb_reference_index
    int dec_ref_idx_P(int ctxInc);
    int dec_ref_idx_B(int ctxInc);

    // mv_diff_x, mv_diff_y
    void dec_mvd(int16_t* mvd, int16_t* mvdAbs);

    // 8x8 coefficient block
    bool dec_coeff_block(int16_t* coeff, int ctxIdxBase, int scale, uint8_t shift);

private:
    const uint8_t*  m_pBuf;
    const uint8_t*  m_pEnd;
    const uint8_t*  m_pCur;         // current read position
    uint32_t        m_Offset;       // [0, 7]
    int32_t         m_rS1;
    int32_t         m_rT1;
    int32_t         m_ValueS;
    int32_t         m_ValueT;
    AecDecCtx       m_CtxAry[200];
    const uint8_t*  m_invScan;
    const uint8_t*  m_weightQM;
};

// intra_chroma_pred_mode()
inline uint8_t AvsAecParser::dec_intra_chroma_pred_mode(int ctxInc)
{
    if (dec_decision(26 + ctxInc) == 0)
        return 0;
    else if (dec_decision(29) == 0)
        return 1;
    else if (dec_decision(29) == 0)
        return 2;
    return 3;
}

inline uint8_t AvsAecParser::dec_cbp(uint8_t leftCbp, uint8_t topCbp)
{
    // luma
    int ctxInc = ((leftCbp >> 1) & 1) + ((topCbp >> 1) & 2);
    uint8_t cbp = dec_decision(51 - ctxInc);
    ctxInc = cbp + ((topCbp >> 2) & 2);
    cbp |= dec_decision(51 - ctxInc) << 1;
    ctxInc = ((leftCbp >> 3) & 1) + ((cbp << 1) & 2);
    cbp |= dec_decision(51 - ctxInc) << 2;
    ctxInc = ((cbp >> 2) & 1) + (cbp & 2);
    cbp |= dec_decision(51 - ctxInc) << 3;

    // chroma
    if (dec_decision(52) != 0)
    {
        if (dec_decision(53) != 0)
        {
            cbp += 48;
        }
        else
        {
            int k = dec_decision(53);
            cbp += 16 + 16 * k;
        }
    }
    return cbp;
}

inline int AvsAecParser::dec_mb_skip_run()
{
    if (dec_decision(0) != 0)
        return 0;
    else if (dec_decision(1) != 0)
        return 1;
    else if (dec_decision(2) != 0)
        return 2;
    return 3 + dec_zero_cnt(3, 16384);
}

inline uint8_t AvsAecParser::dec_mb_part_type()
{
    int x = dec_decision(19);
    return x * 2 + dec_decision(20 + x);
}

inline int AvsAecParser::dec_ref_idx_P(int ctxInc)
{
    if (dec_decision(30 + ctxInc) != 0)
        return 0;
    else if (dec_decision(34) != 0)
        return 1;
    else if (dec_decision(35) != 0)
        return 2;
    else if (dec_decision(35) != 0)
        return 3;
    return 4;       // bad stream
}

inline int AvsAecParser::dec_ref_idx_B(int ctxInc)
{
    return dec_decision(30 + ctxInc) ^ 1;
}

}   // namespace irk_avs_dec
#endif
