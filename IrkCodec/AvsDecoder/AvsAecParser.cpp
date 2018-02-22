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

#include "AvsAecParser.h"
#include "AvsDecUtility.h"

namespace irk_avs_dec {

using irk::msb_index_unzero;

static const int16_t s_lgPmpsAdd[4] = {197, 197, 95, 46};

static inline void update_ctx(AecDecCtx* pCtx, int binVal)
{
    assert(pCtx->lgPmps > 4 && pCtx->lgPmps < 1024);
    if (binVal != pCtx->mps)
    {
        assert(pCtx->cycNo >= 0 && pCtx->cycNo <= 3);
        int cycno = pCtx->cycNo;
        int lgPmps = pCtx->lgPmps + s_lgPmpsAdd[cycno];
        int32_t msk = (1023 - lgPmps) >> 31;            // lgPmps > 1023 ? -1 : 0
        pCtx->mps ^= (msk & 1);
        pCtx->cycNo += (cycno < 3);                     // MIN( cycno + 1, 3 )
        pCtx->lgPmps = lgPmps ^ (msk & 2047);
    }
    else
    {
        assert(pCtx->cycNo >= 0 && pCtx->cycNo <= 3);
        pCtx->cycNo += (pCtx->cycNo == 0);
        int tmp = pCtx->lgPmps >> (2 + pCtx->cycNo);
        pCtx->lgPmps -= (tmp + (tmp >> 2));
    }
}

void AvsAecParser::init()
{
    assert(m_pCur != nullptr && m_Offset == 0);
    m_rS1 = 0;
    m_rT1 = 255;
    m_ValueS = 0;
    m_ValueT = read_bits(9);
    while (m_ValueT < 0x100)
    {
        m_ValueT = (m_ValueT << 1) | read1();
        m_ValueS++;
    }
    m_ValueT &= 0xFF;

    for (int i = 0; i < 200; i++)
    {
        m_CtxAry[i].mps = 0;
        m_CtxAry[i].cycNo = 0;
        m_CtxAry[i].lgPmps = 1023;
    }
}

uint8_t AvsAecParser::dec_decision(int cIdx)
{
    assert(cIdx >= 0 && cIdx <= 323);
    AecDecCtx* pCtx = m_CtxAry + cIdx;
    uint8_t binVal = pCtx->mps;
    const int lgPmps2 = pCtx->lgPmps >> 2;
    assert(lgPmps2 > 0);

    int32_t rT2 = m_rT1 - lgPmps2;
    int32_t sMask = rT2 >> 31;          // m_rT1 >= lgPmps2 ? 0 : -1
    int32_t rS2 = m_rS1 - sMask;
    rT2 += (sMask & 256);

    int valueT = m_ValueT;
    if (rS2 > m_ValueS || (rS2 == m_ValueS && valueT >= rT2))
    {
        binVal ^= 1;

        if (rS2 == m_ValueS)
            valueT = valueT - rT2;
        else
            valueT = 256 + ((valueT << 1) | read1()) - rT2;

        int tRlps = (sMask & m_rT1) + lgPmps2;
        if (tRlps < 0x100)
        {
            int cnt = 8 - msb_index_unzero((uint32_t)tRlps);
            tRlps <<= cnt;
            valueT = (valueT << cnt) | read_bits(cnt);
        }
        assert(tRlps >= 0x100);
        m_rS1 = 0;
        m_rT1 = tRlps & 0xFF;

        int valueS = 0;
        while (valueT == 0)
        {
            valueS += 9;
            valueT = read_bits(9);
        }
        if (valueT < 0x100)
        {
            int cnt = 8 - msb_index_unzero((uint32_t)valueT);
            valueS += cnt;
            valueT = (valueT << cnt) | read_bits(cnt);
        }
        assert(valueT >= 0x100);
        m_ValueS = valueS;
        m_ValueT = valueT & 0xFF;

        // update_ctx()
        assert(pCtx->cycNo >= 0 && pCtx->cycNo <= 3);
        int cycno = pCtx->cycNo;
        int lgPmps = pCtx->lgPmps + s_lgPmpsAdd[cycno];
        int32_t mask = (1023 - lgPmps) >> 31;           // lgPmps > 1023 ? -1 : 0
        pCtx->mps ^= (mask & 1);
        pCtx->cycNo += (cycno < 3);                     // MIN( cycno + 1, 3 )
        pCtx->lgPmps = lgPmps ^ (mask & 2047);
    }
    else
    {
        m_rS1 = rS2;
        m_rT1 = rT2;

        // update_ctx()
        assert(pCtx->cycNo >= 0 && pCtx->cycNo <= 3);
        pCtx->cycNo += (pCtx->cycNo == 0);
        int tmp = lgPmps2 >> pCtx->cycNo;
        pCtx->lgPmps -= (tmp + (tmp >> 2));
    }

    assert(binVal == 0 || binVal == 1);
    return binVal;
}

uint8_t AvsAecParser::dec_decision2(int cIdx1, int cIdx2)
{
    assert(cIdx1 >= 0 && cIdx1 <= 323);
    assert(cIdx2 >= 0 && cIdx2 <= 323);
    AecDecCtx* pCtx1 = m_CtxAry + cIdx1;
    AecDecCtx* pCtx2 = m_CtxAry + cIdx2;

    int lgPmps2;
    uint8_t binVal;
    if (pCtx1->mps == pCtx2->mps)
    {
        binVal = pCtx1->mps;
        lgPmps2 = (pCtx1->lgPmps + pCtx2->lgPmps) >> 3;
    }
    else if (pCtx1->lgPmps < pCtx2->lgPmps)
    {
        binVal = pCtx1->mps;
        lgPmps2 = 1023 - ((pCtx2->lgPmps - pCtx1->lgPmps) >> 1);
        lgPmps2 >>= 2;
    }
    else
    {
        binVal = pCtx2->mps;
        lgPmps2 = 1023 - ((pCtx1->lgPmps - pCtx2->lgPmps) >> 1);
        lgPmps2 >>= 2;
    }
    assert(lgPmps2 > 0);

    int32_t rT2 = m_rT1 - lgPmps2;
    int32_t sMask = rT2 >> 31;          // m_rT1 >= lgPmps2 ? 0 : -1
    int32_t rS2 = m_rS1 - sMask;
    rT2 += (sMask & 256);

    int valueT = m_ValueT;
    if (rS2 > m_ValueS || (rS2 == m_ValueS && valueT >= rT2))
    {
        binVal ^= 1;

        if (rS2 == m_ValueS)
            valueT = valueT - rT2;
        else
            valueT = 256 + ((valueT << 1) | read1()) - rT2;

        int tRlps = (sMask & m_rT1) + lgPmps2;
        if (tRlps < 0x100)
        {
            int cnt = 8 - msb_index_unzero((uint32_t)tRlps);
            tRlps <<= cnt;
            valueT = (valueT << cnt) | read_bits(cnt);
        }
        assert(tRlps >= 0x100);
        m_rS1 = 0;
        m_rT1 = tRlps & 0xFF;

        int valueS = 0;
        while (valueT == 0)
        {
            valueS += 9;
            valueT = read_bits(9);
        }
        if (valueT < 0x100)
        {
            int cnt = 8 - msb_index_unzero((uint32_t)valueT);
            valueS += cnt;
            valueT = (valueT << cnt) | read_bits(cnt);
        }
        assert(valueT >= 0x100);
        m_ValueS = valueS;
        m_ValueT = valueT & 0xFF;
    }
    else
    {
        m_rS1 = rS2;
        m_rT1 = rT2;
    }

    assert(binVal == 0 || binVal == 1);
    update_ctx(pCtx1, binVal);
    update_ctx(pCtx2, binVal);

    return binVal;
}

int AvsAecParser::dec_zero_cnt(int cIdx, int maxCnt)
{
    assert(cIdx >= 0 && cIdx <= 323);
    AecDecCtx* pCtx = m_CtxAry + cIdx;
    int zeroCnt = 0;
    int rS1 = m_rS1;
    int rT1 = m_rT1;
    int valueS = m_ValueS;
    int valueT = m_ValueT;

    while (zeroCnt < maxCnt)
    {
        uint8_t binVal = pCtx->mps;
        const int lgPmps2 = pCtx->lgPmps >> 2;
        assert(lgPmps2 > 0);

        int32_t rT2 = rT1 - lgPmps2;
        int32_t sMask = rT2 >> 31;          // m_rT1 >= lgPmps2 ? 0 : -1
        int32_t rS2 = rS1 - sMask;
        rT2 += (sMask & 256);

        if (rS2 > valueS || (rS2 == valueS && valueT >= rT2))
        {
            binVal ^= 1;

            if (rS2 == valueS)
                valueT = valueT - rT2;
            else
                valueT = 256 + ((valueT << 1) | read1()) - rT2;

            int tRlps = (sMask & rT1) + lgPmps2;
            if (tRlps < 0x100)
            {
                int cnt = 8 - msb_index_unzero((uint32_t)tRlps);
                tRlps <<= cnt;
                valueT = (valueT << cnt) | read_bits(cnt);
            }
            assert(tRlps >= 0x100);
            rS1 = 0;
            rT1 = tRlps & 0xFF;

            valueS = 0;
            while (valueT == 0)
            {
                valueS += 9;
                valueT = read_bits(9);
            }
            if (valueT < 0x100)
            {
                int cnt = 8 - msb_index_unzero((uint32_t)valueT);
                valueS += cnt;
                valueT = (valueT << cnt) | read_bits(cnt);
            }
            assert(valueT >= 0x100);
            valueT &= 0xFF;

            // update_ctx()
            assert(pCtx->cycNo >= 0 && pCtx->cycNo <= 3);
            int cycno = pCtx->cycNo;
            int lgPmps = pCtx->lgPmps + s_lgPmpsAdd[cycno];
            int32_t mask = (1023 - lgPmps) >> 31;           // lgPmps > 1023 ? -1 : 0
            pCtx->mps ^= (mask & 1);
            pCtx->cycNo += (cycno < 3);                     // MIN( cycno + 1, 3 )
            pCtx->lgPmps = lgPmps ^ (mask & 2047);
        }
        else
        {
            rS1 = rS2;
            rT1 = rT2;

            // update_ctx()
            assert(pCtx->cycNo >= 0 && pCtx->cycNo <= 3);
            pCtx->cycNo += (pCtx->cycNo == 0);
            int tmp = lgPmps2 >> pCtx->cycNo;
            pCtx->lgPmps -= (tmp + (tmp >> 2));
        }

        if (binVal != 0)
            break;
        zeroCnt++;
    }

    assert(zeroCnt <= maxCnt);
    m_rS1 = rS1;
    m_rT1 = rT1;
    m_ValueS = valueS;
    m_ValueT = valueT;
    return zeroCnt;
}

uint8_t AvsAecParser::dec_bypass()
{
    int32_t rT2 = m_rT1 - 255;
    int32_t sMask = rT2 >> 31;          // m_rT1 >= 255 ? 0 : -1
    int32_t rS2 = m_rS1 - sMask;
    rT2 += (sMask & 256);

    int valueT = m_ValueT;
    if (rS2 > m_ValueS || (rS2 == m_ValueS && valueT >= rT2))
    {
        if (rS2 == m_ValueS)
            valueT = valueT - rT2;
        else
            valueT = 256 + ((valueT << 1) | read1()) - rT2;

        int tRlps = (sMask & m_rT1) + 255;
        if (tRlps < 0x100)
        {
            tRlps <<= 1;
            valueT = ((valueT << 1) | read1());
        }
        assert(tRlps >= 0x100);
        m_rS1 = 0;
        m_rT1 = tRlps & 0xFF;

        int valueS = 0;
        while (valueT == 0)
        {
            valueS += 9;
            valueT = read_bits(9);
        }
        if (valueT < 0x100)
        {
            int cnt = 8 - msb_index_unzero((uint32_t)valueT);
            valueS += cnt;
            valueT = (valueT << cnt) | read_bits(cnt);
        }
        assert(valueT >= 0x100);
        m_ValueS = valueS;
        m_ValueT = valueT & 0xFF;

        return 1;
    }
    else
    {
        m_rS1 = rS2;
        m_rT1 = rT2;
    }

    return 0;
}

uint8_t AvsAecParser::dec_stuffing_bit()
{
    int32_t rT2 = m_rT1 - 1;
    int32_t sMask = rT2 >> 31;      // m_rT1 >= 1 ? 0 : -1
    int32_t rS2 = m_rS1 - sMask;
    rT2 += (sMask & 256);

    if (rS2 > m_ValueS || (rS2 == m_ValueS && m_ValueT >= rT2))
    {
        int valueT;
        if (rS2 == m_ValueS)
            valueT = m_ValueT - rT2;
        else
            valueT = 256 + ((m_ValueT << 1) | read1()) - rT2;

        int tRlps = (sMask & m_rT1) + 1;
        if (tRlps < 0x100)
        {
            int cnt = 8 - msb_index_unzero((uint32_t)tRlps);
            tRlps <<= cnt;
            valueT = (valueT << cnt) | read_bits(cnt);
        }
        assert(tRlps >= 0x100);
        m_rS1 = 0;
        m_rT1 = tRlps & 0xFF;

        int valueS = 0;
        while (valueT == 0)
        {
            valueS += 9;
            valueT = read_bits(9);
        }
        if (valueT < 0x100)
        {
            int cnt = 8 - msb_index_unzero((uint32_t)valueT);
            valueS += cnt;
            valueT = (valueT << cnt) | read_bits(cnt);
        }
        assert(valueT >= 0x100);
        m_ValueS = valueS;
        m_ValueT = valueT & 0xFF;

        return 1;
    }
    else
    {
        m_rS1 = rS2;
        m_rT1 = rT2;
    }

    return 0;
}

// mb_type of P Slice
int AvsAecParser::dec_mb_type_P()
{
    if (dec_decision(4) != 0)
        return 0;
    else if (dec_decision(5) != 0)
        return 1;
    else if (dec_decision(6) != 0)
        return 2;
    else if (dec_decision(7) != 0)
        return 3;
    else if (dec_decision(8) != 0)
        return 4;
    else if (dec_decision(8) != 0)
        return 5;
    return 6;
}

// mb_type of B Slice
int AvsAecParser::dec_mb_type_B(int ctxInc)
{
    if (dec_decision(9 + ctxInc) == 0)
    {
        return 0;
    }
    for (int i = 1; i <= 7; i++)
    {
        if (dec_decision(11 + i) != 0)
            return i;
    }
    return 8 + dec_zero_cnt(18, 24);
}

void AvsAecParser::dec_mvd(int16_t* mvd, int16_t* mvdAbs)
{
    // mv_diff_x
    int ctxIdxX = 36 + (mvdAbs[0] >= 16) + (mvdAbs[0] >= 2);
    if (dec_decision(ctxIdxX) == 0)
    {
        mvdAbs[0] = 0;
        mvd[0] = 0;
    }
    else if (dec_decision(36 + 3) == 0)
    {
        int sign = dec_bypass();
        mvdAbs[0] = 1;
        mvd[0] = (int16_t)(1 - (sign << 1));
    }
    else if (dec_decision(36 + 4) == 0)
    {
        int sign = dec_bypass();
        mvdAbs[0] = 2;
        mvd[0] = (int16_t)(2 - (sign << 2));
    }
    else
    {
        int absMvd = 3 + dec_decision(36 + 5);
        int cnt = 0;
        int val = 1;
        while (dec_bypass() == 0 && cnt < 16)
        {
            cnt++;
        }
        while (cnt-- > 0)      // 0 阶哥伦布编码
        {
            val = (val << 1) | dec_bypass();
        }
        absMvd += (val - 1) * 2;

        int sign = dec_bypass();
        mvdAbs[0] = absMvd;
        mvd[0] = (int16_t)((absMvd ^ -sign) + sign);
    }

    // mv_diff_y
    int ctxIdxY = 42 + (mvdAbs[1] >= 16) + (mvdAbs[1] >= 2);
    if (dec_decision(ctxIdxY) == 0)
    {
        mvdAbs[1] = 0;
        mvd[1] = 0;
    }
    else if (dec_decision(42 + 3) == 0)
    {
        int sign = dec_bypass();
        mvdAbs[1] = 1;
        mvd[1] = (int16_t)(1 - (sign << 1));
    }
    else if (dec_decision(42 + 4) == 0)
    {
        int sign = dec_bypass();
        mvdAbs[1] = 2;
        mvd[1] = (int16_t)(2 - (sign << 2));
    }
    else
    {
        int absMvd = 3 + dec_decision(42 + 5);
        int cnt = 0;
        int val = 1;
        while (dec_bypass() == 0 && cnt < 16)
        {
            cnt++;
        }
        while (cnt-- > 0)      // 0 order golomb code
        {
            val = (val << 1) | dec_bypass();
        }
        absMvd += (val - 1) * 2;

        int sign = dec_bypass();
        mvdAbs[1] = (int16_t)absMvd;
        mvd[1] = (int16_t)((absMvd ^ -sign) + sign);
    }
}

// q.v. 8.4.4.2
bool AvsAecParser::dec_coeff_block(int16_t* coeff, int ctxIdxBase, int scale, uint8_t shift)
{
    static const int s_PriIdx3[8] = {0 - 1, 3 - 1, 6 - 1, 9 - 1, 9 - 1, 12 - 1, 12 - 1};
    static const int s_PriIdx4[8] = {0 + 46, 4 + 46, 8 + 46, 12 + 46, 12 + 46, 16 + 46, 16 + 46};

    int16_t levelAry[65];
    uint8_t runAry[65];
    int ctxIdxR = ctxIdxBase + 46;      // ctxIdx of coeffRun

    // parse coeffLevel
    if (dec_decision(ctxIdxBase) != 0)
    {
        levelAry[0] = 1;
    }
    else
    {
        ctxIdxR += 2;
        levelAry[0] = 2 + dec_zero_cnt(ctxIdxBase + 1, 16384);
    }
    int lMax = MIN(levelAry[0], 5);

    // parse coeffSign
    int sign = dec_bypass();
    levelAry[0] = (levelAry[0] ^ -sign) + sign;

    // parse coeffRun
    int run = dec_decision(ctxIdxR);
    if (run == 0)
        run = 2 + dec_zero_cnt(ctxIdxR + 1, 64);
    runAry[0] = run;

    int ctxIdxW = ctxIdxBase + 14;
    int ctxIdxL = ctxIdxBase + s_PriIdx3[lMax];
    int i = 1;
    int pos = run;
    while (1)
    {
        // parse EOB
        if (pos >= 64)
        {
            if (pos > 64 || dec_decision2(ctxIdxL, ctxIdxW + 31) == 0)   // bad stream
                return false;
            break;
        }
        else
        {
            if (dec_decision2(ctxIdxL, ctxIdxW + (pos >> 1)) != 0)       // EOB
                break;
        }

        // parse coeffLevel
        int ctxIdxR = ctxIdxBase + s_PriIdx4[lMax];     // ctxIdx of coeffRun
        if (dec_decision(ctxIdxL + 1) != 0)
        {
            levelAry[i] = 1;
        }
        else
        {
            ctxIdxR += 2;
            levelAry[i] = 2 + dec_zero_cnt(ctxIdxL + 2, 16384);
            if (levelAry[i] > lMax)
            {
                lMax = MIN(levelAry[i], 5);
                ctxIdxL = ctxIdxBase + s_PriIdx3[lMax];
            }
        }

        // parse coeffSign
        int signFlag = dec_bypass();
        levelAry[i] = (levelAry[i] ^ -signFlag) + signFlag;

        // parse coeffRun
        int run = dec_decision(ctxIdxR);
        if (run == 0)
            run = 2 + dec_zero_cnt(ctxIdxR + 1, 64);
        runAry[i] = run;

        i++;
        pos += run;
    }

    zero_block8x8(coeff);
    int rnd = 1 << (shift - 1);
    int k = -1;
    while (--i >= 0)
    {
        k += runAry[i];
        assert(k < 64);
        int idx = m_invScan[k];
        int tmp = ((levelAry[i] * m_weightQM[idx] >> 3) * scale) >> 4;
        coeff[idx] = (int16_t)((tmp + rnd) >> shift);
    }

    return true;
}

}   // namespace irk_avs_dec
