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

#include "AvsDecoder.h"

namespace irk_avs_dec {

extern void IDCT_8x8_add_sse4(const int16_t src[64], uint8_t* dst, int dstPitch);
extern void IDCT_8x8_add_c(const int16_t src[64], uint8_t* dst, int dstPitch);

// 标准表 42
const uint8_t s_CBPTab[64][2] =
{
    {63,  0}, {15, 15}, {31, 63}, {47, 31}, { 0, 16}, {14, 32}, {13, 47}, {11, 13},
    { 7, 14}, { 5, 11}, {10, 12}, { 8,  5}, {12, 10}, {61,  7}, { 4, 48}, {55,  3},
    { 1,  2}, { 2,  8}, {59,  4}, { 3,  1}, {62, 61}, { 9, 55}, { 6, 59}, {29, 62},
    {45, 29}, {51, 27}, {23, 23}, {39, 19}, {27, 30}, {46, 28}, {53,  9}, {30,  6},
    {43, 60}, {37, 21}, {60, 44}, {16, 26}, {21, 51}, {28, 35}, {19, 18}, {35, 20},
    {42, 24}, {26, 53}, {44, 17}, {32, 37}, {58, 39}, {24, 45}, {20, 58}, {17, 43},
    {18, 42}, {48, 46}, {22, 36}, {33, 33}, {25, 34}, {49, 40}, {40, 52}, {36, 49},
    {34, 50}, {50, 56}, {52, 25}, {54, 22}, {41, 54}, {56, 57}, {38, 41}, {57, 38},
};

// 帧内预测宏块解码
void dec_macroblock_I8x8(FrmDecContext* ctx, int mx, int my)
{
    AvsBitStream& bitsm = ctx->bitsm;
    MbContext* leftMb = &ctx->leftMb;
    MbContext* topMb = ctx->topLine + mx;
    MbContext* curMb = ctx->curLine + mx;

    // 解析 4 个 8x8 亮度块的帧内预测模式
    int8_t lumaPred[4];
    uint32_t tmp = bitsm.peek();
    uint32_t bitCnt = 4;
    // block 0
    int8_t predAB = get_intra_pred_mode(leftMb->ipMode[0], topMb->ipMode[0]); // 根据左边和上边的宏块预测
    if (tmp & 0x80000000)    // pred_mode_flag
    {
        lumaPred[0] = predAB;
        tmp <<= 1;
    }
    else
    {
        int8_t ipred = (tmp >> 29) & 3;   // intra_luma_pred_mode
        lumaPred[0] = ipred + (ipred >= predAB);
        tmp <<= 3;
        bitCnt += 2;
    }

    // block 1
    predAB = get_intra_pred_mode(lumaPred[0], topMb->ipMode[1]);
    if (tmp & 0x80000000)    // pred_mode_flag
    {
        lumaPred[1] = predAB;
        tmp <<= 1;
    }
    else
    {
        int8_t ipred = (tmp >> 29) & 3;   // intra_luma_pred_mode
        lumaPred[1] = ipred + (ipred >= predAB);
        tmp <<= 3;
        bitCnt += 2;
    }

    // block 2
    predAB = get_intra_pred_mode(leftMb->ipMode[1], lumaPred[0]);
    if (tmp & 0x80000000)     // pred_mode_flag
    {
        lumaPred[2] = predAB;
        tmp <<= 1;
    }
    else
    {
        int8_t ipred = (tmp >> 29) & 3;   // intra_luma_pred_mode
        lumaPred[2] = ipred + (ipred >= predAB);
        tmp <<= 3;
        bitCnt += 2;
    }

    // block 3
    predAB = get_intra_pred_mode(lumaPred[2], lumaPred[1]);
    if (tmp & 0x80000000)    // pred_mode_flag
    {
        lumaPred[3] = predAB;
    }
    else
    {
        int8_t ipred = (tmp >> 29) & 3;   // intra_luma_pred_mode
        lumaPred[3] = ipred + (ipred >= predAB);
        bitCnt += 2;
    }
    bitsm.skip_bits(bitCnt);
    assert(lumaPred[0] >= 0 && lumaPred[0] <= 4);
    assert(lumaPred[1] >= 0 && lumaPred[1] <= 4);
    assert(lumaPred[2] >= 0 && lumaPred[2] <= 4);
    assert(lumaPred[3] >= 0 && lumaPred[3] <= 4);

    // 解析色差分量帧内预测
    int chromaPred = bitsm.read_ue8();
    if (chromaPred > 3)
    {
        ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
        return;
    }

    // 解析 CBP
    int cbpIdx = 0;
    if (ctx->picHdr.pic_type == PIC_TYPE_B)
        cbpIdx = (ctx->mbTypeIdx < 24) ? bitsm.read_ue8() : (ctx->mbTypeIdx - 24);
    else
        cbpIdx = (ctx->mbTypeIdx < 5) ? bitsm.read_ue8() : (ctx->mbTypeIdx - 5);
    if (cbpIdx >= 64)
    {
        ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
        return;
    }
    const uint8_t cbpFlags = s_CBPTab[cbpIdx][0];

    // qp_delta
    if (cbpFlags && ctx->bFixedQp == 0)
    {
        int qpDelta = bitsm.read_se8();
        ctx->curQp += qpDelta;
        if (ctx->curQp & ~63)      // qp 超出了 [0,63] 的范围
        {
            ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
            return;
        }
    }

    AvsContext* avsCtx = ctx->avsCtx;
    AvsVlcParser* parser = ctx->vlcParser;
    const int dqScale = g_DequantScale[ctx->curQp];
    const int dqShift = g_DequantShift[ctx->curQp];
    const int lPitch = ctx->picPitch[0];
    uint8_t* luma = ctx->picPlane[0] + (my * lPitch + mx) * 16;
    int16_t* coeff = ctx->coeff;    // 存储 DCT 系数的临时内存
    NBUsable usable;

    // decode luma block 0
    usable.flags[0] = topMb[0].avail;
    usable.flags[1] = topMb[0].avail;
    usable.flags[2] = leftMb[0].avail;
    usable.flags[3] = leftMb[0].avail;
    (*avsCtx->pfnLumaIPred[lumaPred[0]])(luma, lPitch, usable);   // 帧内预测
    if (cbpFlags & 0x1)
    {
        if (!parser->dec_intra_coeff_block(coeff, bitsm, dqScale, dqShift))
        {
            ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
            return;
        }

        IDCT_8x8_add_sse4(coeff, luma, lPitch);
    }

    // decode luma block 1
    usable.flags[0] = topMb[0].avail;
    usable.flags[1] = topMb[1].avail;
    usable.flags[2] = 1;
    usable.flags[3] = 0;
    (*avsCtx->pfnLumaIPred[lumaPred[1]])(luma + 8, lPitch, usable);
    if (cbpFlags & 0x2)
    {
        if (!parser->dec_intra_coeff_block(coeff, bitsm, dqScale, dqShift))
        {
            ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
            return;
        }

        IDCT_8x8_add_sse4(coeff, luma + 8, lPitch);
    }

    // decode luma block 2
    luma += 8 * lPitch;
    usable.flags[0] = 1;
    usable.flags[1] = 1;
    usable.flags[2] = leftMb[0].avail;
    usable.flags[3] = 0;
    (*avsCtx->pfnLumaIPred[lumaPred[2]])(luma, lPitch, usable);
    if (cbpFlags & 0x4)
    {
        if (!parser->dec_intra_coeff_block(coeff, bitsm, dqScale, dqShift))
        {
            ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
            return;
        }

        IDCT_8x8_add_sse4(coeff, luma, lPitch);
    }

    // decode luma block 3
    usable.flags[0] = 1;
    usable.flags[1] = 0;
    usable.flags[2] = 1;
    usable.flags[3] = 0;
    (*avsCtx->pfnLumaIPred[lumaPred[3]])(luma + 8, lPitch, usable);
    if (cbpFlags & 0x8)
    {
        if (!parser->dec_intra_coeff_block(coeff, bitsm, dqScale, dqShift))
        {
            ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
            return;
        }

        IDCT_8x8_add_sse4(coeff, luma + 8, lPitch);
    }

    // decode Cb block
    const int cPitch = ctx->picPitch[1];
    uint8_t* dstCb = ctx->picPlane[1] + (my * cPitch + mx) * 8;
    usable.flags[0] = topMb[0].avail;
    usable.flags[1] = topMb[1].avail;
    usable.flags[2] = leftMb[0].avail;
    usable.flags[3] = 0;
    (*avsCtx->pfnCbCrIPred[chromaPred])(dstCb, cPitch, usable);   // 帧内预测
    if (cbpFlags & 0x10)
    {
        int qp = ctx->curQp + ctx->picHdr.chroma_quant_delta_cb;
        if (qp & ~63)
        {
            ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
            return;
        }
        qp = g_ChromaQp[qp];

        if (!parser->dec_chroma_coeff_block(coeff, bitsm, g_DequantScale[qp], g_DequantShift[qp]))
        {
            ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
            return;
        }

        IDCT_8x8_add_sse4(coeff, dstCb, cPitch);
    }

    // decode Cr block
    uint8_t* dstCr = ctx->picPlane[2] + (my * cPitch + mx) * 8;
    (*avsCtx->pfnCbCrIPred[chromaPred])(dstCr, cPitch, usable);   // 帧内预测
    if (cbpFlags & 0x20)
    {
        int qp = ctx->curQp + ctx->picHdr.chroma_quant_delta_cr;
        if (qp & ~63)
        {
            ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
            return;
        }
        qp = g_ChromaQp[qp];

        if (!parser->dec_chroma_coeff_block(coeff, bitsm, g_DequantScale[qp], g_DequantShift[qp]))
        {
            ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
            return;
        }

        IDCT_8x8_add_sse4(coeff, dstCr, cPitch);
    }

    // 当前宏块可供右侧和下一行宏块使用
    leftMb->avail = 1;
    leftMb->ipMode[0] = lumaPred[1];
    leftMb->ipMode[1] = lumaPred[3];
    curMb->avail = 1;
    curMb->ipMode[0] = lumaPred[2];
    curMb->ipMode[1] = lumaPred[3];

    // 设置环路滤波相关参数
    if (ctx->lfDisabled == 0)
    {
        curMb->leftQp = leftMb->curQp;
        curMb->topQp = topMb->curQp;
        curMb->curQp = ctx->curQp;
        curMb->lfBS = 0xAAAA;           // 8 个边界的 bs = 2
        leftMb->curQp = ctx->curQp;
    }

    // 针对 B_Direct 运动估计, 设置相关参数
    if (ctx->colMvs)
    {
        BDColMvs* colMv = ctx->colMvs + my * ctx->mbColCnt + mx;
        *(uint32_as*)(colMv->refIdxs) = 0xFFFFFFFF;
    }
}

//======================================================================================================================

// 计算宏块环路滤波强度
uint32_t calc_BS_P16x16(int refIdx, MvUnion curMv, const MbContext* leftMb, const MbContext* topMb);
uint32_t calc_BS_P16x8(const int8_t refIdxs[2], const MvUnion curMvs[2], const MbContext* leftMb, const MbContext* topMb);
uint32_t calc_BS_P8x16(const int8_t refIdxs[2], const MvUnion curMvs[2], const MbContext* leftMb, const MbContext* topMb);
uint32_t calc_BS_P8x8(const int8_t refIdxs[4], const MvUnion curMvs[4], const MbContext* leftMb, const MbContext* topMb);
uint32_t calc_BS_B16x16(const int8_t refIdxs[2], const MvUnion curMvs[2], const MbContext* leftMb, const MbContext* topMb);
uint32_t calc_BS_B16x8(const int8_t refIdxs[2][2], const MvUnion curMvs[2][2], const MbContext* leftMb, const MbContext* topMb);
uint32_t calc_BS_B8x16(const int8_t refIdxs[2][2], const MvUnion curMvs[2][2], const MbContext* leftMb, const MbContext* topMb);
uint32_t calc_BS_B8x8(const int8_t refIdxs[4][2], const MvUnion curMvs[4][2], const MbContext* leftMb, const MbContext* topMb);

// 残差解码并叠加
static bool dec_mb_residual(FrmDecContext* ctx, int mx, int my, uint32_t cbpFlags)
{
    AvsBitStream& bitsm = ctx->bitsm;
    AvsVlcParser* parser = ctx->vlcParser;
    int16_t* coeff = ctx->coeff;    // 存储 DCT 系数的临时内存

    const int dqScale = g_DequantScale[ctx->curQp];
    const int dqShift = g_DequantShift[ctx->curQp];
    const int lPitch = ctx->picPitch[0];
    uint8_t* luma = ctx->picPlane[0] + (my * lPitch + mx) * 16;

    // decode Luma block 0 
    if (cbpFlags & 0x1)
    {
        if (!parser->dec_inter_coeff_block(coeff, bitsm, dqScale, dqShift))
            return false;
        IDCT_8x8_add_sse4(coeff, luma, lPitch);
    }

    // decode Luma block 1
    if (cbpFlags & 0x2)
    {
        if (!parser->dec_inter_coeff_block(coeff, bitsm, dqScale, dqShift))
            return false;
        IDCT_8x8_add_sse4(coeff, luma + 8, lPitch);
    }

    // decode Luma block 2
    luma += 8 * lPitch;
    if (cbpFlags & 0x4)
    {
        if (!parser->dec_inter_coeff_block(coeff, bitsm, dqScale, dqShift))
            return false;
        IDCT_8x8_add_sse4(coeff, luma, lPitch);
    }

    // decode Luma block 3
    if (cbpFlags & 0x8)
    {
        if (!parser->dec_inter_coeff_block(coeff, bitsm, dqScale, dqShift))
            return false;
        IDCT_8x8_add_sse4(coeff, luma + 8, lPitch);
    }

    // decode Cb block
    if (cbpFlags & 0x10)
    {
        int qp = ctx->curQp + ctx->picHdr.chroma_quant_delta_cb;
        if (qp & ~63)
            return false;
        qp = g_ChromaQp[qp];

        if (!parser->dec_chroma_coeff_block(coeff, bitsm, g_DequantScale[qp], g_DequantShift[qp]))
            return false;

        const int cPitch = ctx->picPitch[1];
        uint8_t* dstCb = ctx->picPlane[1] + (my * cPitch + mx) * 8;
        IDCT_8x8_add_sse4(coeff, dstCb, cPitch);
    }

    // decode Cr block
    if (cbpFlags & 0x20)
    {
        int qp = ctx->curQp + ctx->picHdr.chroma_quant_delta_cr;
        if (qp & ~63)
            return false;
        qp = g_ChromaQp[qp];

        if (!parser->dec_chroma_coeff_block(coeff, bitsm, g_DequantScale[qp], g_DequantShift[qp]))
            return false;

        const int cPitch = ctx->picPitch[2];
        uint8_t* dstCr = ctx->picPlane[2] + (my * cPitch + mx) * 8;
        IDCT_8x8_add_sse4(coeff, dstCr, cPitch);
    }

    return true;
}

//======================================================================================================================
// P 宏块解码

static void MC_macroblock_P16x16(FrmDecContext* ctx, int mx, int my, int refIdx, const int16_t mvDiff[2], int wpFlag)
{
    // 解析运动矢量
    MbContext* leftMb = &ctx->leftMb;
    MbContext* topMb = ctx->topLine + mx;
    MbContext* curMb = ctx->curLine + mx;
    MvInfo nbs[3];
    nbs[0].refIdx = leftMb->refIdxs[0][0];
    nbs[0].mv = leftMb->mvs[0][0];
    nbs[0].denDist = ctx->denDist[nbs[0].refIdx];
    nbs[1].refIdx = topMb[0].refIdxs[0][0];
    nbs[1].mv = topMb[0].mvs[0][0];
    nbs[1].denDist = ctx->denDist[nbs[1].refIdx];
    if (topMb[1].avail)    // BlockC 存在
    {
        nbs[2].refIdx = topMb[1].refIdxs[0][0];
        nbs[2].mv = topMb[1].mvs[0][0];
    }
    else    // BlockC 不存在, 使用 BlockD 代替
    {
        nbs[2].refIdx = topMb[-1].refIdxs[1][0];
        nbs[2].mv = topMb[-1].mvs[1][0];
    }
    nbs[2].denDist = ctx->denDist[nbs[2].refIdx];
    MvUnion curMv = get_mv_pred(nbs, ctx->refDist[refIdx]);
    curMv.x += mvDiff[0];
    curMv.y += mvDiff[1];

    // 帧间预测   
    const int lPitch = ctx->picPitch[0];
    const int cPitch = ctx->picPitch[1];
    uint8_t* mbPos[3];
    mbPos[0] = ctx->picPlane[0] + (my * lPitch + mx) * 16;
    mbPos[1] = ctx->picPlane[1] + (my * cPitch + mx) * 8;
    mbPos[2] = ctx->picPlane[2] + (my * cPitch + mx) * 8;
    int x = (mx << 6) + curMv.x;
    int y = (my << 6) + curMv.y;
    const RefPicture* refPic = ctx->refPics + refIdx;
    luma_inter_pred_16x16(ctx, refPic, mbPos[0], lPitch, x, y);
    chroma_inter_pred_8x8(ctx, refPic, mbPos[1], mbPos[2], cPitch, x, y);
    if (wpFlag)        // 加权预测
    {
        weight_pred_16xN(mbPos[0], lPitch, ctx->lumaScale[refIdx], ctx->lumaDelta[refIdx], 16);
        weight_pred_8xN(mbPos[1], cPitch, ctx->cbcrScale[refIdx], ctx->cbcrDelta[refIdx], 8);
        weight_pred_8xN(mbPos[2], cPitch, ctx->cbcrScale[refIdx], ctx->cbcrDelta[refIdx], 8);
    }

    // 设置环路滤波相关参数
    if (ctx->lfDisabled == 0)
    {
        curMb->leftQp = leftMb->curQp;
        curMb->topQp = topMb->curQp;
        curMb->curQp = ctx->curQp;
        leftMb->curQp = ctx->curQp;
        curMb->lfBS = calc_BS_P16x16(refIdx, curMv, leftMb, topMb);
    }

    // 存储参考帧索引和运动矢量
    curMb->avail = 1;
    curMb->cipFlag = 0;
    curMb->skipFlag = 0;
    curMb->refIdxs[0][0] = refIdx;
    curMb->refIdxs[1][0] = refIdx;
    curMb->mvs[0][0] = curMv;
    curMb->mvs[1][0] = curMv;
    leftMb->avail = 1;
    leftMb->cipFlag = 0;
    leftMb->skipFlag = 0;
    leftMb->refIdxs[0][0] = refIdx;
    leftMb->refIdxs[1][0] = refIdx;
    leftMb->mvs[0][0] = curMv;
    leftMb->mvs[1][0] = curMv;

    // 针对 B_Direct 运动估计
    BDColMvs* colMv = ctx->colMvs + my * ctx->mbColCnt + mx;
    *(int32_as*)colMv->refIdxs = refIdx * 0x01010101;
    colMv->mvs[0] = colMv->mvs[1] =
        colMv->mvs[2] = colMv->mvs[3] = curMv;
}

static void MC_macroblock_P16x8(FrmDecContext* ctx, int mx, int my, const int8_t refIdxs[2], const int16_t mvDiff[2][2], int wpFlag)
{
    // 解析运动矢量
    MbContext* leftMb = &ctx->leftMb;
    MbContext* topMb = ctx->topLine + mx;
    MbContext* curMb = ctx->curLine + mx;
    MvUnion curMvs[2];

    if (topMb->refIdxs[0][0] == refIdxs[0])    // 如果 PFieldSkip=1, 与标准不符, 但码流表明如此处理正确
    {
        curMvs[0] = topMb->mvs[0][0];
    }
    else
    {
        MvInfo nbs[3];
        nbs[0].refIdx = leftMb->refIdxs[0][0];
        nbs[0].mv = leftMb->mvs[0][0];
        nbs[0].denDist = ctx->denDist[nbs[0].refIdx];
        nbs[1].refIdx = topMb[0].refIdxs[0][0];
        nbs[1].mv = topMb[0].mvs[0][0];
        nbs[1].denDist = ctx->denDist[nbs[1].refIdx];
        if (topMb[1].avail)    // BlockC 存在
        {
            nbs[2].refIdx = topMb[1].refIdxs[0][0];
            nbs[2].mv = topMb[1].mvs[0][0];
        }
        else    // BlockC 不存在, 使用 BlockD 代替
        {
            nbs[2].refIdx = topMb[-1].refIdxs[1][0];
            nbs[2].mv = topMb[-1].mvs[1][0];
        }
        nbs[2].denDist = ctx->denDist[nbs[2].refIdx];
        curMvs[0] = get_mv_pred(nbs, ctx->refDist[refIdxs[0]]);
    }
    curMvs[0].x += mvDiff[0][0];
    curMvs[0].y += mvDiff[0][1];

    // 帧间预测   
    const int lPitch = ctx->picPitch[0];
    const int cPitch = ctx->picPitch[1];
    uint8_t* mbPos[3];
    mbPos[0] = ctx->picPlane[0] + (my * lPitch + mx) * 16;
    mbPos[1] = ctx->picPlane[1] + (my * cPitch + mx) * 8;
    mbPos[2] = ctx->picPlane[2] + (my * cPitch + mx) * 8;
    int x = (mx << 6) + curMvs[0].x;
    int y = (my << 6) + curMvs[0].y;
    const RefPicture* refPic = ctx->refPics + refIdxs[0];
    luma_inter_pred_16x8(ctx, refPic, mbPos[0], lPitch, x, y);
    chroma_inter_pred_8x4(ctx, refPic, mbPos[1], mbPos[2], cPitch, x, y);
    if (wpFlag)    // 加权预测
    {
        int k = refIdxs[0];
        weight_pred_16xN(mbPos[0], lPitch, ctx->lumaScale[k], ctx->lumaDelta[k], 8);
        weight_pred_8xN(mbPos[1], cPitch, ctx->cbcrScale[k], ctx->cbcrDelta[k], 4);
        weight_pred_8xN(mbPos[2], cPitch, ctx->cbcrScale[k], ctx->cbcrDelta[k], 4);
    }

    //------------------------------------------------------

    // 解析运动矢量
    if (leftMb->refIdxs[1][0] == refIdxs[1])   // 如果 PFieldSkip=1, 与标准不符, 但码流表明如此处理正确
    {
        curMvs[1] = leftMb->mvs[1][0];
    }
    else
    {
        MvInfo nbs[3];
        nbs[0].refIdx = leftMb->refIdxs[1][0];
        nbs[0].mv = leftMb->mvs[1][0];
        nbs[0].denDist = ctx->denDist[nbs[0].refIdx];
        nbs[1].refIdx = refIdxs[0];
        nbs[1].mv = curMvs[0];
        nbs[1].denDist = ctx->denDist[nbs[1].refIdx];
        nbs[2].refIdx = leftMb->refIdxs[0][0];
        nbs[2].mv = leftMb->mvs[0][0];
        nbs[2].denDist = ctx->denDist[nbs[2].refIdx];
        curMvs[1] = get_mv_pred(nbs, ctx->refDist[refIdxs[1]]);
    }
    curMvs[1].x += mvDiff[1][0];
    curMvs[1].y += mvDiff[1][1];

    // 帧间预测   
    mbPos[0] += 8 * lPitch;
    mbPos[1] += 4 * cPitch;
    mbPos[2] += 4 * cPitch;
    x = (mx << 6) + curMvs[1].x;
    y = (my << 6) + 32 + curMvs[1].y;
    refPic = ctx->refPics + refIdxs[1];
    luma_inter_pred_16x8(ctx, refPic, mbPos[0], lPitch, x, y);
    chroma_inter_pred_8x4(ctx, refPic, mbPos[1], mbPos[2], cPitch, x, y);
    if (wpFlag)            // 加权预测
    {
        int k = refIdxs[1];
        weight_pred_16xN(mbPos[0], lPitch, ctx->lumaScale[k], ctx->lumaDelta[k], 8);
        weight_pred_8xN(mbPos[1], cPitch, ctx->cbcrScale[k], ctx->cbcrDelta[k], 4);
        weight_pred_8xN(mbPos[2], cPitch, ctx->cbcrScale[k], ctx->cbcrDelta[k], 4);
    }

    // 设置环路滤波相关参数
    if (ctx->lfDisabled == 0)
    {
        curMb->leftQp = leftMb->curQp;
        curMb->topQp = topMb->curQp;
        curMb->curQp = ctx->curQp;
        leftMb->curQp = ctx->curQp;
        curMb->lfBS = calc_BS_P16x8(refIdxs, curMvs, leftMb, topMb);
    }

    // 存储参考帧索引和运动矢量
    curMb->avail = 1;
    curMb->cipFlag = 0;
    curMb->skipFlag = 0;
    curMb->refIdxs[0][0] = refIdxs[1];
    curMb->refIdxs[1][0] = refIdxs[1];
    curMb->mvs[0][0] = curMvs[1];
    curMb->mvs[1][0] = curMvs[1];
    leftMb->avail = 1;
    leftMb->cipFlag = 0;
    leftMb->skipFlag = 0;
    leftMb->refIdxs[0][0] = refIdxs[0];
    leftMb->refIdxs[1][0] = refIdxs[1];
    leftMb->mvs[0][0] = curMvs[0];
    leftMb->mvs[1][0] = curMvs[1];

    // 针对 B_Direct 运动估计
    BDColMvs* colMv = ctx->colMvs + my * ctx->mbColCnt + mx;
    colMv->refIdxs[0] = colMv->refIdxs[1] = refIdxs[0];
    colMv->refIdxs[2] = colMv->refIdxs[3] = refIdxs[1];
    colMv->mvs[0] = colMv->mvs[1] = curMvs[0];
    colMv->mvs[2] = colMv->mvs[3] = curMvs[1];
}

static void MC_macroblock_P8x16(FrmDecContext* ctx, int mx, int my, const int8_t refIdxs[2], const int16_t mvDiff[2][2], int wpFlag)
{
    // 解析运动矢量
    MbContext* leftMb = &ctx->leftMb;
    MbContext* topMb = ctx->topLine + mx;
    MbContext* curMb = ctx->curLine + mx;
    MvUnion curMvs[2];

    if (leftMb->refIdxs[0][0] == refIdxs[0])   // 如果 PFieldSkip=1, 与标准不符, 但码流表明如此处理正确
    {
        curMvs[0] = leftMb->mvs[0][0];
    }
    else
    {
        MvInfo nbs[3];
        nbs[0].refIdx = leftMb->refIdxs[0][0];
        nbs[0].mv = leftMb->mvs[0][0];
        nbs[0].denDist = ctx->denDist[nbs[0].refIdx];
        nbs[1].refIdx = topMb->refIdxs[0][0];
        nbs[1].mv = topMb->mvs[0][0];
        nbs[1].denDist = ctx->denDist[nbs[1].refIdx];
        nbs[2].refIdx = topMb->refIdxs[1][0];
        nbs[2].mv = topMb->mvs[1][0];
        nbs[2].denDist = ctx->denDist[nbs[2].refIdx];
        curMvs[0] = get_mv_pred(nbs, ctx->refDist[refIdxs[0]]);
    }
    curMvs[0].x += mvDiff[0][0];
    curMvs[0].y += mvDiff[0][1];

    // 帧间预测   
    const int lPitch = ctx->picPitch[0];
    const int cPitch = ctx->picPitch[1];
    uint8_t* mbPos[3];
    mbPos[0] = ctx->picPlane[0] + (my * lPitch + mx) * 16;
    mbPos[1] = ctx->picPlane[1] + (my * cPitch + mx) * 8;
    mbPos[2] = ctx->picPlane[2] + (my * cPitch + mx) * 8;
    int x = (mx << 6) + curMvs[0].x;
    int y = (my << 6) + curMvs[0].y;
    const RefPicture* refPic = ctx->refPics + refIdxs[0];
    luma_inter_pred_8x16(ctx, refPic, mbPos[0], lPitch, x, y);
    chroma_inter_pred_4x8(ctx, refPic, mbPos[1], mbPos[2], cPitch, x, y);
    if (wpFlag)            // 加权预测
    {
        int k = refIdxs[0];
        weight_pred_8xN(mbPos[0], lPitch, ctx->lumaScale[k], ctx->lumaDelta[k], 16);
        weight_pred_4xN(mbPos[1], cPitch, ctx->cbcrScale[k], ctx->cbcrDelta[k], 8);
        weight_pred_4xN(mbPos[2], cPitch, ctx->cbcrScale[k], ctx->cbcrDelta[k], 8);
    }

    //---------------------------------------------------------

    // 解析运动矢量
    if (topMb[1].avail)    // BlockC 存在
    {
        if (topMb[1].refIdxs[0][0] == refIdxs[1]) // 如果 PFieldSkip=1, 与标准不符, 但码流表明如此处理正确
        {
            curMvs[1] = topMb[1].mvs[0][0];
        }
        else
        {
            MvInfo nbs[3];
            nbs[0].refIdx = refIdxs[0];
            nbs[0].mv = curMvs[0];
            nbs[0].denDist = ctx->denDist[nbs[0].refIdx];
            nbs[1].refIdx = topMb->refIdxs[1][0];
            nbs[1].mv = topMb->mvs[1][0];
            nbs[1].denDist = ctx->denDist[nbs[1].refIdx];
            nbs[2].refIdx = topMb[1].refIdxs[0][0];
            nbs[2].mv = topMb[1].mvs[0][0];
            nbs[2].denDist = ctx->denDist[nbs[2].refIdx];
            curMvs[1] = get_mv_pred(nbs, ctx->refDist[refIdxs[1]]);
        }
    }
    else    // BlockC 不存在, 根据标准使用 BlockD 代替
    {
        if (topMb->refIdxs[0][0] == refIdxs[1])  // 如果 PFieldSkip=1, 与标准不符, 但码流表明如此处理正确
        {
            curMvs[1] = topMb->mvs[0][0];
        }
        else
        {
            MvInfo nbs[3];
            nbs[0].refIdx = refIdxs[0];
            nbs[0].mv = curMvs[0];
            nbs[0].denDist = ctx->denDist[nbs[0].refIdx];
            nbs[1].refIdx = topMb->refIdxs[1][0];
            nbs[1].mv = topMb->mvs[1][0];
            nbs[1].denDist = ctx->denDist[nbs[1].refIdx];
            nbs[2].refIdx = topMb->refIdxs[0][0];
            nbs[2].mv = topMb->mvs[0][0];
            nbs[2].denDist = ctx->denDist[nbs[2].refIdx];
            curMvs[1] = get_mv_pred(nbs, ctx->refDist[refIdxs[1]]);
        }
    }
    curMvs[1].x += mvDiff[1][0];
    curMvs[1].y += mvDiff[1][1];

    // 帧间预测   
    x = (mx << 6) + 32 + curMvs[1].x;
    y = (my << 6) + curMvs[1].y;
    refPic = ctx->refPics + refIdxs[1];
    luma_inter_pred_8x16(ctx, refPic, mbPos[0] + 8, lPitch, x, y);
    chroma_inter_pred_4x8(ctx, refPic, mbPos[1] + 4, mbPos[2] + 4, cPitch, x, y);
    if (wpFlag)            // 加权预测
    {
        int k = refIdxs[1];
        weight_pred_8xN(mbPos[0] + 8, lPitch, ctx->lumaScale[k], ctx->lumaDelta[k], 16);
        weight_pred_4xN(mbPos[1] + 4, cPitch, ctx->cbcrScale[k], ctx->cbcrDelta[k], 8);
        weight_pred_4xN(mbPos[2] + 4, cPitch, ctx->cbcrScale[k], ctx->cbcrDelta[k], 8);
    }

    // 设置环路滤波相关参数
    if (ctx->lfDisabled == 0)
    {
        curMb->leftQp = leftMb->curQp;
        curMb->topQp = topMb->curQp;
        curMb->curQp = ctx->curQp;
        leftMb->curQp = ctx->curQp;
        curMb->lfBS = calc_BS_P8x16(refIdxs, curMvs, leftMb, topMb);
    }

    // 存储参考帧索引和运动矢量
    curMb->avail = 1;
    curMb->cipFlag = 0;
    curMb->skipFlag = 0;
    curMb->refIdxs[0][0] = refIdxs[0];
    curMb->refIdxs[1][0] = refIdxs[1];
    curMb->mvs[0][0] = curMvs[0];
    curMb->mvs[1][0] = curMvs[1];
    leftMb->avail = 1;
    leftMb->cipFlag = 0;
    leftMb->skipFlag = 0;
    leftMb->refIdxs[0][0] = refIdxs[1];
    leftMb->refIdxs[1][0] = refIdxs[1];
    leftMb->mvs[0][0] = curMvs[1];
    leftMb->mvs[1][0] = curMvs[1];

    // 针对 B_Direct 运动估计
    BDColMvs* colMv = ctx->colMvs + my * ctx->mbColCnt + mx;
    colMv->refIdxs[0] = colMv->refIdxs[2] = refIdxs[0];
    colMv->refIdxs[1] = colMv->refIdxs[3] = refIdxs[1];
    colMv->mvs[0] = colMv->mvs[2] = curMvs[0];
    colMv->mvs[1] = colMv->mvs[3] = curMvs[1];
}

static void MC_macroblock_P8x8(FrmDecContext* ctx, int mx, int my, const int8_t refIdxs[4], const int16_t mvDiff[4][2], int wpFlag)
{
    //------------------ block 0 ---------------------
    const int* rcpDist = ctx->denDist;
    MbContext* leftMb = &ctx->leftMb;
    MbContext* topMb = ctx->topLine + mx;
    MbContext* curMb = ctx->curLine + mx;
    MvUnion curMvs[4];
    MvInfo nbs[3];
    nbs[0].refIdx = leftMb->refIdxs[0][0];
    nbs[0].mv = leftMb->mvs[0][0];
    nbs[0].denDist = rcpDist[nbs[0].refIdx];
    nbs[1].refIdx = topMb->refIdxs[0][0];
    nbs[1].mv = topMb->mvs[0][0];
    nbs[1].denDist = rcpDist[nbs[1].refIdx];
    nbs[2].refIdx = topMb->refIdxs[1][0];
    nbs[2].mv = topMb->mvs[1][0];
    nbs[2].denDist = rcpDist[nbs[2].refIdx];
    curMvs[0] = get_mv_pred(nbs, ctx->refDist[refIdxs[0]]);
    curMvs[0].x += mvDiff[0][0];
    curMvs[0].y += mvDiff[0][1];

    // 帧间预测
    const int lPitch = ctx->picPitch[0];
    const int cPitch = ctx->picPitch[1];
    uint8_t* mbPos[3];
    mbPos[0] = ctx->picPlane[0] + (my * lPitch + mx) * 16;
    mbPos[1] = ctx->picPlane[1] + (my * cPitch + mx) * 8;
    mbPos[2] = ctx->picPlane[2] + (my * cPitch + mx) * 8;
    int x = (mx << 6) + curMvs[0].x;
    int y = (my << 6) + curMvs[0].y;
    const RefPicture* refPic = ctx->refPics + refIdxs[0];
    luma_inter_pred_8x8(ctx, refPic, mbPos[0], lPitch, x, y);
    chroma_inter_pred_4x4(ctx, refPic, mbPos[1], mbPos[2], cPitch, x, y);
    if (wpFlag)            // 加权预测
    {
        int k = refIdxs[0];
        weight_pred_8xN(mbPos[0], lPitch, ctx->lumaScale[k], ctx->lumaDelta[k], 8);
        weight_pred_4xN(mbPos[1], cPitch, ctx->cbcrScale[k], ctx->cbcrDelta[k], 4);
        weight_pred_4xN(mbPos[2], cPitch, ctx->cbcrScale[k], ctx->cbcrDelta[k], 4);
    }

    //------------------ block 1 ---------------------
    nbs[0].refIdx = refIdxs[0];
    nbs[0].mv = curMvs[0];
    nbs[0].denDist = rcpDist[nbs[0].refIdx];
    nbs[1].refIdx = topMb->refIdxs[1][0];
    nbs[1].mv = topMb->mvs[1][0];
    nbs[1].denDist = rcpDist[nbs[1].refIdx];
    if (topMb[1].avail)    // BlockC 存在
    {
        nbs[2].refIdx = topMb[1].refIdxs[0][0];
        nbs[2].mv = topMb[1].mvs[0][0];
    }
    else    // BlockC 不存在, 使用 BlockD 代替
    {
        nbs[2].refIdx = topMb->refIdxs[0][0];
        nbs[2].mv = topMb->mvs[0][0];
    }
    nbs[2].denDist = rcpDist[nbs[2].refIdx];
    curMvs[1] = get_mv_pred(nbs, ctx->refDist[refIdxs[1]]);
    curMvs[1].x += mvDiff[1][0];
    curMvs[1].y += mvDiff[1][1];

    // 帧间预测
    mbPos[0] += 8;
    mbPos[1] += 4;
    mbPos[2] += 4;
    x = (mx << 6) + 32 + curMvs[1].x;
    y = (my << 6) + curMvs[1].y;
    refPic = ctx->refPics + refIdxs[1];
    luma_inter_pred_8x8(ctx, refPic, mbPos[0], lPitch, x, y);
    chroma_inter_pred_4x4(ctx, refPic, mbPos[1], mbPos[2], cPitch, x, y);
    if (wpFlag)            // 加权预测
    {
        int k = refIdxs[1];
        weight_pred_8xN(mbPos[0], lPitch, ctx->lumaScale[k], ctx->lumaDelta[k], 8);
        weight_pred_4xN(mbPos[1], cPitch, ctx->cbcrScale[k], ctx->cbcrDelta[k], 4);
        weight_pred_4xN(mbPos[2], cPitch, ctx->cbcrScale[k], ctx->cbcrDelta[k], 4);
    }

    //------------------ block 2 ---------------------
    nbs[0].refIdx = leftMb->refIdxs[1][0];
    nbs[0].mv = leftMb->mvs[1][0];
    nbs[0].denDist = rcpDist[nbs[0].refIdx];
    nbs[1].refIdx = refIdxs[0];
    nbs[1].mv = curMvs[0];
    nbs[1].denDist = rcpDist[nbs[1].refIdx];
    nbs[2].refIdx = refIdxs[1];
    nbs[2].mv = curMvs[1];
    nbs[2].denDist = rcpDist[nbs[2].refIdx];
    curMvs[2] = get_mv_pred2(nbs, ctx->refDist[refIdxs[2]]);
    curMvs[2].x += mvDiff[2][0];
    curMvs[2].y += mvDiff[2][1];

    // 帧间预测
    mbPos[0] += 8 * lPitch - 8;
    mbPos[1] += 4 * cPitch - 4;
    mbPos[2] += 4 * cPitch - 4;
    x = (mx << 6) + curMvs[2].x;
    y = (my << 6) + 32 + curMvs[2].y;
    refPic = ctx->refPics + refIdxs[2];
    luma_inter_pred_8x8(ctx, refPic, mbPos[0], lPitch, x, y);
    chroma_inter_pred_4x4(ctx, refPic, mbPos[1], mbPos[2], cPitch, x, y);
    if (wpFlag)            // 加权预测
    {
        int k = refIdxs[2];
        weight_pred_8xN(mbPos[0], lPitch, ctx->lumaScale[k], ctx->lumaDelta[k], 8);
        weight_pred_4xN(mbPos[1], cPitch, ctx->cbcrScale[k], ctx->cbcrDelta[k], 4);
        weight_pred_4xN(mbPos[2], cPitch, ctx->cbcrScale[k], ctx->cbcrDelta[k], 4);
    }

    //------------------ block 3 ---------------------
    nbs[0].refIdx = refIdxs[2];
    nbs[0].mv = curMvs[2];
    nbs[0].denDist = rcpDist[nbs[0].refIdx];
    nbs[1].refIdx = refIdxs[1];
    nbs[1].mv = curMvs[1];
    nbs[1].denDist = rcpDist[nbs[1].refIdx];
    nbs[2].refIdx = refIdxs[0];
    nbs[2].mv = curMvs[0];
    nbs[2].denDist = rcpDist[nbs[2].refIdx];
    curMvs[3] = get_mv_pred2(nbs, ctx->refDist[refIdxs[3]]);
    curMvs[3].x += mvDiff[3][0];
    curMvs[3].y += mvDiff[3][1];

    // 帧间预测
    mbPos[0] += 8;
    mbPos[1] += 4;
    mbPos[2] += 4;
    x = (mx << 6) + 32 + curMvs[3].x;
    y = (my << 6) + 32 + curMvs[3].y;
    refPic = ctx->refPics + refIdxs[3];
    luma_inter_pred_8x8(ctx, refPic, mbPos[0], lPitch, x, y);
    chroma_inter_pred_4x4(ctx, refPic, mbPos[1], mbPos[2], cPitch, x, y);
    if (wpFlag)            // 加权预测
    {
        int k = refIdxs[3];
        weight_pred_8xN(mbPos[0], lPitch, ctx->lumaScale[k], ctx->lumaDelta[k], 8);
        weight_pred_4xN(mbPos[1], cPitch, ctx->cbcrScale[k], ctx->cbcrDelta[k], 4);
        weight_pred_4xN(mbPos[2], cPitch, ctx->cbcrScale[k], ctx->cbcrDelta[k], 4);
    }

    // 设置环路滤波相关参数
    if (ctx->lfDisabled == 0)
    {
        curMb->leftQp = leftMb->curQp;
        curMb->topQp = topMb->curQp;
        curMb->curQp = ctx->curQp;
        leftMb->curQp = ctx->curQp;
        curMb->lfBS = calc_BS_P8x8(refIdxs, curMvs, leftMb, topMb);
    }

    // 存储参考帧索引和运动矢量
    curMb->avail = 1;
    curMb->cipFlag = 0;
    curMb->skipFlag = 0;
    curMb->refIdxs[0][0] = refIdxs[2];
    curMb->refIdxs[1][0] = refIdxs[3];
    curMb->mvs[0][0] = curMvs[2];
    curMb->mvs[1][0] = curMvs[3];
    leftMb->avail = 1;
    leftMb->cipFlag = 0;
    leftMb->skipFlag = 0;
    leftMb->refIdxs[0][0] = refIdxs[1];
    leftMb->refIdxs[1][0] = refIdxs[3];
    leftMb->mvs[0][0] = curMvs[1];
    leftMb->mvs[1][0] = curMvs[3];

    // 针对 B_Direct 运动估计
    BDColMvs* colMv = ctx->colMvs + my * ctx->mbColCnt + mx;
    colMv->refIdxs[0] = refIdxs[0];
    colMv->refIdxs[1] = refIdxs[1];
    colMv->refIdxs[2] = refIdxs[2];
    colMv->refIdxs[3] = refIdxs[3];
    colMv->mvs[0] = curMvs[0];
    colMv->mvs[1] = curMvs[1];
    colMv->mvs[2] = curMvs[2];
    colMv->mvs[3] = curMvs[3];
}

// P-Skip 宏块解码
void dec_macroblock_PSkip(FrmDecContext* ctx, int mx, int my)
{
    MbContext* leftMb = &ctx->leftMb;
    MbContext* topMb = ctx->topLine + mx;
    MbContext* curMb = ctx->curLine + mx;
    const int8_t refIdx = ctx->pFieldSkip;      // 0 or 1
    assert(refIdx == 0 || refIdx == 1);

    // 计算运动矢量
    MvUnion curMv;
    if ((leftMb->avail & topMb->avail) == 0 ||
        (leftMb->refIdxs[0][0] == refIdx && leftMb->mvs[0][0].i32 == 0) ||
        (topMb->refIdxs[0][0] == refIdx && topMb->mvs[0][0].i32 == 0))
    {
        curMv.i32 = 0;
    }
    else
    {
        // 运动矢量预测
        MvInfo nbs[3];
        nbs[0].refIdx = leftMb->refIdxs[0][0];
        nbs[0].mv = leftMb->mvs[0][0];
        nbs[0].denDist = ctx->denDist[nbs[0].refIdx];
        nbs[1].refIdx = topMb[0].refIdxs[0][0];
        nbs[1].mv = topMb[0].mvs[0][0];
        nbs[1].denDist = ctx->denDist[nbs[1].refIdx];
        if (topMb[1].avail)    // BlockC 存在
        {
            nbs[2].refIdx = topMb[1].refIdxs[0][0];
            nbs[2].mv = topMb[1].mvs[0][0];
        }
        else    // BlockC 不存在, 使用 BlockD 代替
        {
            nbs[2].refIdx = topMb[-1].refIdxs[1][0];
            nbs[2].mv = topMb[-1].mvs[1][0];
        }
        nbs[2].denDist = ctx->denDist[nbs[2].refIdx];
        curMv = get_mv_pred(nbs, ctx->refDist[refIdx]);
    }

    // 亮度分量帧间预测
    const RefPicture* refPic = ctx->refPics + refIdx;
    const int lPitch = ctx->picPitch[0];
    uint8_t* luma = ctx->picPlane[0] + (my * lPitch + mx) * 16;
    int x = (mx << 6) + curMv.x;
    int y = (my << 6) + curMv.y;
    luma_inter_pred_16x16(ctx, refPic, luma, lPitch, x, y);

    // 色差分量帧间预测
    const int cPitch = ctx->picPitch[1];
    uint8_t* dstCb = ctx->picPlane[1] + (my * cPitch + mx) * 8;
    uint8_t* dstCr = ctx->picPlane[2] + (my * cPitch + mx) * 8;
    chroma_inter_pred_8x8(ctx, refPic, dstCb, dstCr, cPitch, x, y);

    // 加权预测
    if (ctx->sliceWPFlag && !ctx->mbWPFlag)
    {
        weight_pred_16xN(luma, lPitch, ctx->lumaScale[refIdx], ctx->lumaDelta[refIdx], 16);
        weight_pred_8xN(dstCb, cPitch, ctx->cbcrScale[refIdx], ctx->cbcrDelta[refIdx], 8);
        weight_pred_8xN(dstCr, cPitch, ctx->cbcrScale[refIdx], ctx->cbcrDelta[refIdx], 8);
    }

    // 设置环路滤波相关参数
    if (ctx->lfDisabled == 0)
    {
        curMb->leftQp = leftMb->curQp;
        curMb->topQp = topMb->curQp;
        curMb->curQp = ctx->curQp;
        leftMb->curQp = ctx->curQp;
        curMb->lfBS = calc_BS_P16x16(refIdx, curMv, leftMb, topMb);
    }

    // 存储参考帧索引和运动矢量等信息
    curMb->avail = 1;
    curMb->cipFlag = 0;
    curMb->skipFlag = 1;
    curMb->cbp = 0;
    curMb->refIdxs[0][0] = refIdx;
    curMb->refIdxs[1][0] = refIdx;
    curMb->mvs[0][0] = curMv;
    curMb->mvs[1][0] = curMv;
    leftMb->avail = 1;
    leftMb->cipFlag = 0;
    leftMb->skipFlag = 1;
    leftMb->cbp = 0;
    leftMb->refIdxs[0][0] = refIdx;
    leftMb->refIdxs[1][0] = refIdx;
    leftMb->mvs[0][0] = curMv;
    leftMb->mvs[1][0] = curMv;

    // 针对高级熵编码
    ctx->prevQpDelta = 0;
    *(uint64_as*)(ctx->mvdA[0]) = 0;

    // 针对 B_Direct 运动估计
    BDColMvs* colMv = ctx->colMvs + my * ctx->mbColCnt + mx;
    *(int32_as*)colMv->refIdxs = refIdx * 0x01010101;
    colMv->mvs[0] = colMv->mvs[1] =
        colMv->mvs[2] = colMv->mvs[3] = curMv;
}

void dec_macroblock_P16x16(FrmDecContext* ctx, int mx, int my)
{
    AvsBitStream& bitsm = ctx->bitsm;

    // 解析参考帧索引
    int refIdx = 0;
    if (ctx->picHdr.pic_ref_flag == 0)
    {
        if (ctx->frameCoding)   // 帧编码
            refIdx = bitsm.read1();
        else
            refIdx = bitsm.read_bits(2);
    }

    // 解析 mv_diff
    int16_t mvDiff[2];
    mvDiff[0] = bitsm.read_se16();
    mvDiff[1] = bitsm.read_se16();

    // 解析 weighting_prediction
    const uint8_t wpFlag = ctx->mbWPFlag ? bitsm.read1() : ctx->sliceWPFlag;

    // 解析 CBP
    int cbpIdx = bitsm.read_ue8();
    if (cbpIdx >= 64)
    {
        ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
        return;
    }
    const uint32_t cbpFlags = s_CBPTab[cbpIdx][1];

    // qp_delta
    if (cbpFlags && ctx->bFixedQp == 0)
    {
        int qpDelta = bitsm.read_se8();
        ctx->curQp += qpDelta;
        if (ctx->curQp & ~63)      // qp 超出了 [0,63] 的范围
        {
            ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
            return;
        }
    }

    // 运动补偿
    MC_macroblock_P16x16(ctx, mx, my, refIdx, mvDiff, wpFlag);

    // 叠加残差
    if (!dec_mb_residual(ctx, mx, my, cbpFlags))
    {
        ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
        return;
    }
}

void dec_macroblock_P16x8(FrmDecContext* ctx, int mx, int my)
{
    AvsBitStream& bitsm = ctx->bitsm;

    // 解析参考帧索引
    int8_t refIdxs[2];
    if (ctx->picHdr.pic_ref_flag == 0)
    {
        if (ctx->frameCoding)  // 帧编码
        {
            uint32_t tmp = bitsm.read_bits(2);
            refIdxs[0] = (tmp >> 1) & 1;
            refIdxs[1] = tmp & 1;
        }
        else
        {
            uint32_t tmp = bitsm.read_bits(4);
            refIdxs[0] = (tmp >> 2) & 3;
            refIdxs[1] = tmp & 3;
        }
    }
    else
    {
        refIdxs[0] = 0;
        refIdxs[1] = 0;
    }

    // 解析 mv_diff
    int16_t mvDiff[2][2];
    mvDiff[0][0] = bitsm.read_se16();
    mvDiff[0][1] = bitsm.read_se16();
    mvDiff[1][0] = bitsm.read_se16();
    mvDiff[1][1] = bitsm.read_se16();

    // 解析 weighting_prediction
    const uint8_t wpFlag = ctx->mbWPFlag ? bitsm.read1() : ctx->sliceWPFlag;

    // 解析 CBP
    int cbpIdx = bitsm.read_ue8();
    if (cbpIdx >= 64)
    {
        ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
        return;
    }
    const uint32_t cbpFlags = s_CBPTab[cbpIdx][1];

    // qp_delta
    if (cbpFlags && ctx->bFixedQp == 0)
    {
        int qpDelta = bitsm.read_se8();
        ctx->curQp += qpDelta;
        if (ctx->curQp & ~63)      // qp 超出了 [0,63] 的范围
        {
            ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
            return;
        }
    }

    // 运动补偿
    MC_macroblock_P16x8(ctx, mx, my, refIdxs, mvDiff, wpFlag);

    // 叠加残差
    if (!dec_mb_residual(ctx, mx, my, cbpFlags))
    {
        ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
        return;
    }
}

void dec_macroblock_P8x16(FrmDecContext* ctx, int mx, int my)
{
    AvsBitStream& bitsm = ctx->bitsm;

    // 解析参考帧索引
    int8_t refIdxs[2];
    if (ctx->picHdr.pic_ref_flag == 0)
    {
        if (ctx->frameCoding)  // 帧编码
        {
            uint32_t tmp = bitsm.read_bits(2);
            refIdxs[0] = (tmp >> 1) & 1;
            refIdxs[1] = tmp & 1;
        }
        else
        {
            uint32_t tmp = bitsm.read_bits(4);
            refIdxs[0] = (tmp >> 2) & 3;
            refIdxs[1] = tmp & 3;
        }
    }
    else
    {
        refIdxs[0] = 0;
        refIdxs[1] = 0;
    }

    // 解析 mv_diff
    int16_t mvDiff[2][2];
    mvDiff[0][0] = bitsm.read_se16();
    mvDiff[0][1] = bitsm.read_se16();
    mvDiff[1][0] = bitsm.read_se16();
    mvDiff[1][1] = bitsm.read_se16();

    // 解析 weighting_prediction
    const uint8_t wpFlag = ctx->mbWPFlag ? bitsm.read1() : ctx->sliceWPFlag;

    // 解析 CBP
    int cbpIdx = bitsm.read_ue8();
    if (cbpIdx >= 64)
    {
        ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
        return;
    }
    const uint32_t cbpFlags = s_CBPTab[cbpIdx][1];

    // qp_delta
    if (cbpFlags && ctx->bFixedQp == 0)
    {
        int qpDelta = bitsm.read_se8();
        ctx->curQp += qpDelta;
        if (ctx->curQp & ~63)      // qp 超出了 [0,63] 的范围
        {
            ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
            return;
        }
    }

    // 运动补偿
    MC_macroblock_P8x16(ctx, mx, my, refIdxs, mvDiff, wpFlag);

    // 叠加残差
    if (!dec_mb_residual(ctx, mx, my, cbpFlags))
    {
        ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
        return;
    }
}

void dec_macroblock_P8x8(FrmDecContext* ctx, int mx, int my)
{
    AvsBitStream& bitsm = ctx->bitsm;

    // 解析参考帧索引
    int8_t refIdxs[4];
    if (ctx->picHdr.pic_ref_flag == 0)
    {
        if (ctx->frameCoding)  // 帧编码
        {
            uint32_t tmp = bitsm.read_bits(4);
            refIdxs[0] = (tmp >> 3) & 1;
            refIdxs[1] = (tmp >> 2) & 1;
            refIdxs[2] = (tmp >> 1) & 1;
            refIdxs[3] = tmp & 1;
        }
        else
        {
            uint32_t tmp = bitsm.read_bits(8);
            refIdxs[0] = (tmp >> 6) & 3;
            refIdxs[1] = (tmp >> 4) & 3;
            refIdxs[2] = (tmp >> 2) & 3;
            refIdxs[3] = tmp & 3;
        }
    }
    else
    {
        refIdxs[0] = 0;
        refIdxs[1] = 0;
        refIdxs[2] = 0;
        refIdxs[3] = 0;
    }

    // 解析 mv_diff
    int16_t mvDiff[4][2];
    mvDiff[0][0] = bitsm.read_se16();
    mvDiff[0][1] = bitsm.read_se16();
    mvDiff[1][0] = bitsm.read_se16();
    mvDiff[1][1] = bitsm.read_se16();
    mvDiff[2][0] = bitsm.read_se16();
    mvDiff[2][1] = bitsm.read_se16();
    mvDiff[3][0] = bitsm.read_se16();
    mvDiff[3][1] = bitsm.read_se16();

    // 解析 weighting_prediction
    const uint8_t wpFlag = ctx->mbWPFlag ? bitsm.read1() : ctx->sliceWPFlag;

    // 解析 CBP
    int cbpIdx = bitsm.read_ue8();
    if (cbpIdx >= 64)
    {
        ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
        return;
    }
    const uint32_t cbpFlags = s_CBPTab[cbpIdx][1];

    // qp_delta
    if (cbpFlags && ctx->bFixedQp == 0)
    {
        int qpDelta = bitsm.read_se8();
        ctx->curQp += qpDelta;
        if (ctx->curQp & ~63)      // qp 超出了 [0,63] 的范围
        {
            ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
            return;
        }
    }

    // 运动补偿
    MC_macroblock_P8x8(ctx, mx, my, refIdxs, mvDiff, wpFlag);

    // 叠加残差
    if (!dec_mb_residual(ctx, mx, my, cbpFlags))
    {
        ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
        return;
    }
}

//======================================================================================================================
// B 宏块解码

// 前向预测
#define PRED_FWD    0x1
// 后向预测
#define PRED_BCK    0x2
// 双向预测
#define PRED_SYM    (PRED_FWD | PRED_BCK)

// B 帧宏块预测方向
const uint8_t g_BPredFlags[24][2] =
{
    {       0,        0}, {       0,        0}, {PRED_FWD, PRED_FWD}, {PRED_BCK, PRED_BCK},
    {PRED_SYM, PRED_SYM}, {PRED_FWD, PRED_FWD}, {PRED_FWD, PRED_FWD}, {PRED_BCK, PRED_BCK},
    {PRED_BCK, PRED_BCK}, {PRED_FWD, PRED_BCK}, {PRED_FWD, PRED_BCK}, {PRED_BCK, PRED_FWD},
    {PRED_BCK, PRED_FWD}, {PRED_FWD, PRED_SYM}, {PRED_FWD, PRED_SYM}, {PRED_BCK, PRED_SYM},
    {PRED_BCK, PRED_SYM}, {PRED_SYM, PRED_FWD}, {PRED_SYM, PRED_FWD}, {PRED_SYM, PRED_BCK},
    {PRED_SYM, PRED_BCK}, {PRED_SYM, PRED_SYM}, {PRED_SYM, PRED_SYM},
};

// 16x8, 8x16, 8x8 Block 解码信息
struct BlkDecArgv
{
    int         wpFlag;
    int         predFlag;
    int         bx;
    int         by;
    uint8_t*    mbDst[3];
};

// 后向预测 MV 缩放
static inline MvUnion sym_mv_scale(MvUnion src, int scale)
{
    MvUnion mv;
    mv.x = -(int16_t)((src.x * scale + 256) >> 9);
    mv.y = -(int16_t)((src.y * scale + 256) >> 9);
    return mv;
}

// B_Direct 前向 MV 缩放
static inline MvUnion BD_forward_scale(MvUnion mvRef, int dist, int den)
{
    MvUnion mv;
    int sign = (int)mvRef.x >> 31;
    int absv = (mvRef.x ^ sign) - sign;
    mv.x = (((den * (1 + absv * dist) - 1) >> 14) ^ sign) - sign;
    sign = (int)mvRef.y >> 31;
    absv = (mvRef.y ^ sign) - sign;
    mv.y = (((den * (1 + absv * dist) - 1) >> 14) ^ sign) - sign;
    return mv;
}

// B_Direct 后向 MV 缩放
static inline MvUnion BD_back_scale(MvUnion mvRef, int dist, int den)
{
    MvUnion mv;
    int sign = (int)mvRef.x >> 31;
    int absv = (mvRef.x ^ sign) - sign;
    sign = ~sign;
    mv.x = (((den * (1 + absv * dist) - 1) >> 14) ^ sign) - sign;
    sign = (int)mvRef.y >> 31;
    absv = (mvRef.y ^ sign) - sign;
    sign = ~sign;
    mv.y = (((den * (1 + absv * dist) - 1) >> 14) ^ sign) - sign;
    return mv;
}

// 计算宏块级运动矢量预测值, [dir]: 预测方向索引
static MvUnion get_mb_mv_pred(const FrmDecContext* ctx, const MbContext* leftMb, const MbContext* topMb, int refIdx, int dir)
{
    MvInfo nbs[3];
    nbs[0].refIdx = leftMb->refIdxs[0][dir];
    nbs[0].mv = leftMb->mvs[0][dir];
    nbs[0].denDist = ctx->denDist[nbs[0].refIdx];
    nbs[1].refIdx = topMb[0].refIdxs[0][dir];
    nbs[1].mv = topMb[0].mvs[0][dir];
    nbs[1].denDist = ctx->denDist[nbs[1].refIdx];
    if (topMb[1].avail)    // BlockC 存在
    {
        nbs[2].refIdx = topMb[1].refIdxs[0][dir];
        nbs[2].mv = topMb[1].mvs[0][dir];
    }
    else    // BlockC 不存在, 使用 BlockD 代替
    {
        nbs[2].refIdx = topMb[-1].refIdxs[1][dir];
        nbs[2].mv = topMb[-1].mvs[1][dir];
    }
    nbs[2].denDist = ctx->denDist[nbs[2].refIdx];

    MvUnion mvPred = get_mv_pred(nbs, ctx->refDist[refIdx]);
    return mvPred;
}

// 检查参考帧 B_Direct ColMV 数据是否就绪
static inline void check_BD_col_mv(FrmDecContext* ctx, const DecFrame* colFrame, int my)
{
    if (ctx->curFrame == colFrame) // 同一帧的第二场, 第一场数据总是已解码
    {
        assert(ctx->fieldIdx == 1);
        return;
    }

    // 等待参考帧, +1 是为了让参考帧多解码一些
    assert(colFrame->decState);
    ctx->refRequest.m_refLine = (my + 1) * 16 * (2 - ctx->frameCoding);  // 可能存在帧场自适应编码, 转化为帧的刻度
    colFrame->decState->wait_request(&ctx->refRequest);
}

// 计算 BDirect 运动矢量, 针对 BfieldEnhanced == 1 的情形
static void get_BD_mv_BFE(FrmDecContext* ctx, int mx, int my, int blkIdx, int8_t refIdxs[2], MvUnion curMvs[2])
{
    // 根据标准, 参考帧也应是场编码, 并且 top_field_first 保持一致
    // 对应位置总是选择后向参考帧的第一场

    const DecFrame* colFrame = ctx->refFrames[0];
    const BDColMvs* colMv = colFrame->colMvs + my * ctx->mbColCnt + mx;
    int colRefIdx = colMv->refIdxs[blkIdx];
    if (colRefIdx < 0)    // I MacroBlock
    {
        const MbContext* leftMb = &ctx->leftMb;
        const MbContext* topMb = ctx->topLine + mx;
        refIdxs[0] = 0;
        refIdxs[1] = 1;
        curMvs[0] = get_mb_mv_pred(ctx, leftMb, topMb, 0, 0);
        curMvs[1] = get_mb_mv_pred(ctx, leftMb, topMb, 1, 1);
        return;
    }

    if (ctx->fieldIdx == 0 && colRefIdx != 0)
        refIdxs[0] = 2;
    else
        refIdxs[0] = 0;
    refIdxs[1] = 1;          // 总是选择标准中标识为 0 的后向参考场

    MvUnion mvRef = colMv->mvs[blkIdx];
    if ((colRefIdx & 1) == 0)          // mvRef 指向顶底不同的场
    {
        if (colFrame->topfield_first)  // 后向参考为顶场
            mvRef.y += 2;
        else
            mvRef.y -= 2;
    }

    int distDen = colFrame->denDistBD[0][colRefIdx];
    curMvs[0] = BD_forward_scale(mvRef, ctx->refDist[refIdxs[0]], distDen);
    curMvs[1] = BD_back_scale(mvRef, ctx->refDist[refIdxs[1]], distDen);

    if (ctx->fieldIdx == 0)
    {
        if (ctx->curFrame->topfield_first)     // 当前为顶场
        {
            curMvs[0].y -= 2 - refIdxs[0];
        }
        else    // 当前为底场
        {
            curMvs[0].y += 2 - refIdxs[0];
        }
    }
    else
    {
        if (ctx->curFrame->topfield_first)     // 当前为底场
        {
            curMvs[0].y += refIdxs[0];
            curMvs[1].y += 2;
        }
        else    // 当前为顶场
        {
            curMvs[0].y -= refIdxs[0];
            curMvs[1].y -= 2;
        }
    }
}

// 计算 BDirect 运动矢量
static void get_BD_mv(FrmDecContext* ctx, int mx, int my, int blkIdx, int8_t refIdxs[2], MvUnion curMvs[2])
{
    const MbContext* leftMb = &ctx->leftMb;
    const MbContext* topMb = ctx->topLine + mx;
    const DecFrame* curFrame = ctx->curFrame;
    const DecFrame* colFrame = ctx->refFrames[0];   // 后向参考帧

    // 检查参考帧 B_Direct ColMV 数据是否就绪
    check_BD_col_mv(ctx, colFrame, my);

    if (curFrame->frameCoding != 0)        // 当前为帧编码
    {
        refIdxs[0] = 0;
        refIdxs[1] = 1;

        if (colFrame->frameCoding != 0)    // 后向参考图像为帧编码
        {
            const BDColMvs* colMv = colFrame->colMvs + my * ctx->mbColCnt + mx;
            int colRefIdx = colMv->refIdxs[blkIdx];
            if (colRefIdx < 0)    // I MacroBlock
            {
                curMvs[0] = get_mb_mv_pred(ctx, leftMb, topMb, 0, 0);
                curMvs[1] = get_mb_mv_pred(ctx, leftMb, topMb, 1, 1);
            }
            else
            {
                int distDen = colFrame->denDistBD[0][colRefIdx];
                MvUnion mvRef = colMv->mvs[blkIdx];
                curMvs[0] = BD_forward_scale(mvRef, ctx->refDist[0], distDen);
                curMvs[1] = BD_back_scale(mvRef, ctx->refDist[1], distDen);
            }
        }
        else    // 后向参考帧为场编码
        {
            const int topIdx = colFrame->topfield_first ^ 1;    // 顶场的解码顺序索引          
            const BDColMvs* colMv = colFrame->colMvs;
            colMv += topIdx * ((ctx->mbRowCnt + 1) >> 1) * ctx->mbColCnt;
            colMv += (my >> 1) * ctx->mbColCnt + mx;
            int colRefIdx = colMv->refIdxs[blkIdx];
            if (colRefIdx < 0)     // I MacroBlock
            {
                curMvs[0] = get_mb_mv_pred(ctx, leftMb, topMb, 0, 0);
                curMvs[1] = get_mb_mv_pred(ctx, leftMb, topMb, 1, 1);
            }
            else
            {
                int distDen = colFrame->denDistBD[topIdx][colRefIdx];
                MvUnion mvRef = colMv->mvs[blkIdx];
                mvRef.y *= 2;
                curMvs[0] = BD_forward_scale(mvRef, ctx->refDist[0], distDen);
                curMvs[1] = BD_back_scale(mvRef, ctx->refDist[1], distDen);
            }
        }
    }
    else    // 当前为场编码
    {
        if (ctx->bFieldEnhance != 0)       // AVS+ 扩展功能
        {
            get_BD_mv_BFE(ctx, mx, my, blkIdx, refIdxs, curMvs);
            return;
        }
        if (colFrame->frameCoding == 0)    // 后向参考图像为场编码
        {
            // 得到对应位置所在参考场解码顺序索引
            const int decIdx = curFrame->topfield_first ^ colFrame->topfield_first ^ ctx->fieldIdx;
            const BDColMvs* colMv = colFrame->colMvs + (decIdx * ctx->mbRowCnt + my) * ctx->mbColCnt + mx;
            int colRefIdx = colMv->refIdxs[blkIdx];
            if (colRefIdx < 0)     // I MacroBlock
            {
                refIdxs[0] = 0;
                refIdxs[1] = 1;
                curMvs[0] = get_mb_mv_pred(ctx, leftMb, topMb, 0, 0);
                curMvs[1] = get_mb_mv_pred(ctx, leftMb, topMb, 1, 1);
            }
            else
            {
                // fldIdx == colRefIdx: mvRef 指向的参考场与前向参考场 0 相同
                assert(decIdx == 0 || decIdx == 1);
                refIdxs[0] = (decIdx == colRefIdx) ? 0 : 2;
                refIdxs[1] = (ctx->fieldIdx << 1) + 1;
                int distDen = colFrame->denDistBD[decIdx][colRefIdx];
                MvUnion mvRef = colMv->mvs[blkIdx];
                curMvs[0] = BD_forward_scale(mvRef, ctx->refDist[refIdxs[0]], distDen);
                curMvs[1] = BD_back_scale(mvRef, ctx->refDist[refIdxs[1]], distDen);
            }
        }
        else    // 后向参考图像为帧编码
        {
            const BDColMvs* colMv = colFrame->colMvs + 2 * my * ctx->mbColCnt + mx;
            int colRefIdx = colMv->refIdxs[blkIdx];
            if (colRefIdx < 0)     // I MacroBlock
            {
                refIdxs[0] = 0;
                refIdxs[1] = 1;
                curMvs[0] = get_mb_mv_pred(ctx, leftMb, topMb, 0, 0);
                curMvs[1] = get_mb_mv_pred(ctx, leftMb, topMb, 1, 1);
            }
            else
            {
                refIdxs[0] = 2;      // 标准在此处描述不清...
                refIdxs[1] = (ctx->fieldIdx << 1) + 1;
                int distDen = colFrame->denDistBD[0][colRefIdx];
                MvUnion mvRef = colMv->mvs[blkIdx];
                mvRef.y /= 2;
                curMvs[0] = BD_forward_scale(mvRef, ctx->refDist[refIdxs[0]], distDen);
                curMvs[1] = BD_back_scale(mvRef, ctx->refDist[refIdxs[1]], distDen);
            }
        }
    }
}

// BDirect_16x16 帧间预测
static void MC_BDirect_16x16(FrmDecContext* ctx, int mx, int my, int wpFlag)
{
    const int lPitch = ctx->picPitch[0];
    const int cPitch = ctx->picPitch[1];
    uint8_t* mbPos[3] =
    {
        ctx->picPlane[0] + (my * lPitch + mx) * 16,
        ctx->picPlane[1] + (my * cPitch + mx) * 8,
        ctx->picPlane[2] + (my * cPitch + mx) * 8,
    };
    uint8_t* mcBuf[3] = {ctx->mcBuff, ctx->mcBuff + 256, ctx->mcBuff + 512};
    uint8_t* blkDst[3];
    RefPicture* refPic = nullptr;
    int8_t refIdxs[4][2];
    MvUnion curMvs[4][2];

    // 4 个 8x8 块依次处理
    for (int i = 0; i < 4; i++)
    {
        // 8x8 块的目标位置
        int blkX = (mx << 6) + (i & 1) * 32;
        int blkY = (my << 6) + (i & 2) * 16;
        blkDst[0] = mbPos[0] + (i & 2) * 4 * lPitch + (i & 1) * 8;
        blkDst[1] = mbPos[1] + (i & 2) * 2 * cPitch + (i & 1) * 4;
        blkDst[2] = mbPos[2] + (i & 2) * 2 * cPitch + (i & 1) * 4;

        // 得到 B_Direct 运动矢量
        get_BD_mv(ctx, mx, my, i, refIdxs[i], curMvs[i]);

        // 前向预测
        int x = blkX + curMvs[i][0].x;
        int y = blkY + curMvs[i][0].y;
        refPic = ctx->refPics + refIdxs[i][0];
        luma_inter_pred_8x8(ctx, refPic, blkDst[0], lPitch, x, y);
        chroma_inter_pred_4x4(ctx, refPic, blkDst[1], blkDst[2], cPitch, x, y);
        if (wpFlag)    // 加权预测
        {
            int k = refIdxs[i][0];
            weight_pred_8xN(blkDst[0], lPitch, ctx->lumaScale[k], ctx->lumaDelta[k], 8);
            weight_pred_4xN(blkDst[1], cPitch, ctx->cbcrScale[k], ctx->cbcrDelta[k], 4);
            weight_pred_4xN(blkDst[2], cPitch, ctx->cbcrScale[k], ctx->cbcrDelta[k], 4);
        }

        // 后向预测
        x = blkX + curMvs[i][1].x;
        y = blkY + curMvs[i][1].y;
        refPic = ctx->refPics + refIdxs[i][1];
        luma_inter_pred_8x8(ctx, refPic, mcBuf[0], 16, x, y);
        chroma_inter_pred_4x4(ctx, refPic, mcBuf[1], mcBuf[2], 8, x, y);
        if (wpFlag)    // 加权预测
        {
            int k = refIdxs[i][1];
            weight_pred_8xN(mcBuf[0], 16, ctx->lumaScale[k], ctx->lumaDelta[k], 8);
            weight_pred_4xN(mcBuf[1], 8, ctx->cbcrScale[k], ctx->cbcrDelta[k], 4);
            weight_pred_4xN(mcBuf[2], 8, ctx->cbcrScale[k], ctx->cbcrDelta[k], 4);
        }

        // 取前后预测平均值
        MC_avg_8xN(mcBuf[0], 16, blkDst[0], lPitch, 8);
        MC_avg_4xN(mcBuf[1], 8, blkDst[1], cPitch, 4);
        MC_avg_4xN(mcBuf[2], 8, blkDst[2], cPitch, 4);
    }

    // 设置环路滤波相关参数
    MbContext* leftMb = &ctx->leftMb;
    MbContext* topMb = ctx->topLine + mx;
    MbContext* curMb = ctx->curLine + mx;
    if (ctx->lfDisabled == 0)
    {
        curMb->leftQp = leftMb->curQp;
        curMb->topQp = topMb->curQp;
        curMb->curQp = ctx->curQp;
        leftMb->curQp = ctx->curQp;
        curMb->lfBS = calc_BS_B8x8(refIdxs, curMvs, leftMb, topMb);
    }

    // 存储参考帧索引和运动矢量  
    curMb->avail = 1;
    curMb->cipFlag = 0;
    curMb->skipFlag = 1;
    curMb->refIdxs[0][0] = refIdxs[2][0];
    curMb->refIdxs[0][1] = refIdxs[2][1];
    curMb->refIdxs[1][0] = refIdxs[3][0];
    curMb->refIdxs[1][1] = refIdxs[3][1];
    curMb->mvs[0][0] = curMvs[2][0];
    curMb->mvs[0][1] = curMvs[2][1];
    curMb->mvs[1][0] = curMvs[3][0];
    curMb->mvs[1][1] = curMvs[3][1];

    leftMb->avail = 1;
    leftMb->cipFlag = 0;
    leftMb->skipFlag = 1;
    leftMb->refIdxs[0][0] = refIdxs[1][0];
    leftMb->refIdxs[0][1] = refIdxs[1][1];
    leftMb->refIdxs[1][0] = refIdxs[3][0];
    leftMb->refIdxs[1][1] = refIdxs[3][1];
    leftMb->mvs[0][0] = curMvs[1][0];
    leftMb->mvs[0][1] = curMvs[1][1];
    leftMb->mvs[1][0] = curMvs[3][0];
    leftMb->mvs[1][1] = curMvs[3][1];
}

static void MC_macroblock_B16x16(FrmDecContext* ctx, int mx, int my, int rawIdx, const int16_t mvDiff[2], int wpFlag)
{
    MbContext* leftMb = &ctx->leftMb;
    MbContext* topMb = ctx->topLine + mx;
    MbContext* curMb = ctx->curLine + mx;
    int8_t refIdxs[2];
    MvUnion curMvs[2];

    // 解析运动矢量
    const uint8_t predFlag = g_BPredFlags[ctx->mbTypeIdx][0];
    int dir = (predFlag & 1) ^ 1;               // 运动方向索引
    refIdxs[dir] = (rawIdx << 1) | dir;         // 变换后的参考帧索引
    curMvs[dir] = get_mb_mv_pred(ctx, leftMb, topMb, refIdxs[dir], dir);
    curMvs[dir].x += mvDiff[0];
    curMvs[dir].y += mvDiff[1];

    // 帧间预测   
    const int lPitch = ctx->picPitch[0];
    const int cPitch = ctx->picPitch[1];
    uint8_t* mbPos[3];
    mbPos[0] = ctx->picPlane[0] + (my * lPitch + mx) * 16;
    mbPos[1] = ctx->picPlane[1] + (my * cPitch + mx) * 8;
    mbPos[2] = ctx->picPlane[2] + (my * cPitch + mx) * 8;
    int x = (mx << 6) + curMvs[dir].x;
    int y = (my << 6) + curMvs[dir].y;
    const RefPicture* refPic = ctx->refPics + refIdxs[dir];
    luma_inter_pred_16x16(ctx, refPic, mbPos[0], lPitch, x, y);
    chroma_inter_pred_8x8(ctx, refPic, mbPos[1], mbPos[2], cPitch, x, y);
    if (wpFlag)        // 加权预测
    {
        int k = refIdxs[dir];
        weight_pred_16xN(mbPos[0], lPitch, ctx->lumaScale[k], ctx->lumaDelta[k], 16);
        weight_pred_8xN(mbPos[1], cPitch, ctx->cbcrScale[k], ctx->cbcrDelta[k], 8);
        weight_pred_8xN(mbPos[2], cPitch, ctx->cbcrScale[k], ctx->cbcrDelta[k], 8);
    }

    if (predFlag == PRED_SYM)      // 双向预测
    {
        assert(dir == 0 && (refIdxs[0] & 1) == 0);
        refIdxs[1] = refIdxs[0] ^ ctx->backIdxXor;
        curMvs[1] = sym_mv_scale(curMvs[0], ctx->backMvScale[refIdxs[0]]);

        uint8_t* mcBuf[3] = {ctx->mcBuff, ctx->mcBuff + 256, ctx->mcBuff + 512};
        int x = (mx << 6) + curMvs[1].x;
        int y = (my << 6) + curMvs[1].y;
        const RefPicture* refPic = ctx->refPics + refIdxs[1];
        luma_inter_pred_16x16(ctx, refPic, mcBuf[0], 16, x, y);
        chroma_inter_pred_8x8(ctx, refPic, mcBuf[1], mcBuf[2], 16, x, y);
        if (wpFlag)    // 加权预测
        {
            int k = refIdxs[1];
            weight_pred_16xN(mcBuf[0], 16, ctx->lumaScale[k], ctx->lumaDelta[k], 16);
            weight_pred_8xN(mcBuf[1], 16, ctx->cbcrScale[k], ctx->cbcrDelta[k], 8);
            weight_pred_8xN(mcBuf[2], 16, ctx->cbcrScale[k], ctx->cbcrDelta[k], 8);
        }

        // 取前后预测平均值
        MC_avg_16xN(mcBuf[0], 16, mbPos[0], lPitch, 16);
        MC_avg_8xN(mcBuf[1], 16, mbPos[1], cPitch, 8);
        MC_avg_8xN(mcBuf[2], 16, mbPos[2], cPitch, 8);
    }
    else
    {
        // 这个预测方向没有运动矢量
        refIdxs[dir ^ 1] = -1;
        curMvs[dir ^ 1].i32 = 0;
    }

    // 设置环路滤波相关参数
    if (ctx->lfDisabled == 0)
    {
        curMb->leftQp = leftMb->curQp;
        curMb->topQp = topMb->curQp;
        curMb->curQp = ctx->curQp;
        leftMb->curQp = ctx->curQp;
        curMb->lfBS = calc_BS_B16x16(refIdxs, curMvs, leftMb, topMb);
    }

    // 保存运动矢量信息
    curMb->avail = 1;
    curMb->cipFlag = 0;
    curMb->skipFlag = 0;
    curMb->refIdxs[0][0] = refIdxs[0];
    curMb->refIdxs[0][1] = refIdxs[1];
    curMb->refIdxs[1][0] = refIdxs[0];
    curMb->refIdxs[1][1] = refIdxs[1];
    curMb->mvs[0][0] = curMvs[0];
    curMb->mvs[0][1] = curMvs[1];
    curMb->mvs[1][0] = curMvs[0];
    curMb->mvs[1][1] = curMvs[1];
    leftMb->avail = 1;
    leftMb->cipFlag = 0;
    leftMb->skipFlag = 0;
    leftMb->refIdxs[0][0] = refIdxs[0];
    leftMb->refIdxs[0][1] = refIdxs[1];
    leftMb->refIdxs[1][0] = refIdxs[0];
    leftMb->refIdxs[1][1] = refIdxs[1];
    leftMb->mvs[0][0] = curMvs[0];
    leftMb->mvs[0][1] = curMvs[1];
    leftMb->mvs[1][0] = curMvs[0];
    leftMb->mvs[1][1] = curMvs[1];
}

static void MC_blockB_16x8(FrmDecContext* ctx, const BlkDecArgv& blk, int8_t refIdxs[2], MvUnion curMvs[2])
{
    const int dir = (blk.predFlag & 1) ^ 1;     // 预测方向索引
    const int lPitch = ctx->picPitch[0];
    const int cPitch = ctx->picPitch[1];
    int x = blk.bx + curMvs[dir].x;
    int y = blk.by + curMvs[dir].y;
    const RefPicture* refPic = ctx->refPics + refIdxs[dir];
    luma_inter_pred_16x8(ctx, refPic, blk.mbDst[0], lPitch, x, y);
    chroma_inter_pred_8x4(ctx, refPic, blk.mbDst[1], blk.mbDst[2], cPitch, x, y);
    if (blk.wpFlag)           // 加权预测
    {
        int k = refIdxs[dir];
        weight_pred_16xN(blk.mbDst[0], lPitch, ctx->lumaScale[k], ctx->lumaDelta[k], 8);
        weight_pred_8xN(blk.mbDst[1], cPitch, ctx->cbcrScale[k], ctx->cbcrDelta[k], 4);
        weight_pred_8xN(blk.mbDst[2], cPitch, ctx->cbcrScale[k], ctx->cbcrDelta[k], 4);
    }

    // 双向预测
    if (blk.predFlag == PRED_SYM)
    {
        assert(dir == 0 && (refIdxs[0] & 1) == 0);
        refIdxs[1] = refIdxs[0] ^ ctx->backIdxXor;
        curMvs[1] = sym_mv_scale(curMvs[0], ctx->backMvScale[refIdxs[0]]);

        uint8_t* mcBuf[3] = {ctx->mcBuff, ctx->mcBuff + 256, ctx->mcBuff + 512};
        int x = blk.bx + curMvs[1].x;
        int y = blk.by + curMvs[1].y;
        const RefPicture* refPic = ctx->refPics + refIdxs[1];
        luma_inter_pred_16x8(ctx, refPic, mcBuf[0], 16, x, y);
        chroma_inter_pred_8x4(ctx, refPic, mcBuf[1], mcBuf[2], 16, x, y);
        if (blk.wpFlag)        // 加权预测
        {
            int k = refIdxs[1];
            weight_pred_16xN(mcBuf[0], 16, ctx->lumaScale[k], ctx->lumaDelta[k], 8);
            weight_pred_8xN(mcBuf[1], 16, ctx->cbcrScale[k], ctx->cbcrDelta[k], 4);
            weight_pred_8xN(mcBuf[2], 16, ctx->cbcrScale[k], ctx->cbcrDelta[k], 4);
        }

        // 取前后预测平均值
        MC_avg_16xN(mcBuf[0], 16, blk.mbDst[0], lPitch, 8);
        MC_avg_8xN(mcBuf[1], 16, blk.mbDst[1], cPitch, 4);
        MC_avg_8xN(mcBuf[2], 16, blk.mbDst[2], cPitch, 4);
    }
    else
    {
        // 这个预测方向没有运动矢量
        refIdxs[dir ^ 1] = -1;
        curMvs[dir ^ 1].i32 = 0;
    }
}

static void MC_blockB_8x16(FrmDecContext* ctx, const BlkDecArgv& blk, int8_t refIdxs[2], MvUnion curMvs[2])
{
    const int dir = (blk.predFlag & 1) ^ 1;     // 预测方向索引
    const int lPitch = ctx->picPitch[0];
    const int cPitch = ctx->picPitch[1];
    int x = blk.bx + curMvs[dir].x;
    int y = blk.by + curMvs[dir].y;
    const RefPicture* refPic = ctx->refPics + refIdxs[dir];
    luma_inter_pred_8x16(ctx, refPic, blk.mbDst[0], lPitch, x, y);
    chroma_inter_pred_4x8(ctx, refPic, blk.mbDst[1], blk.mbDst[2], cPitch, x, y);
    if (blk.wpFlag)           // 加权预测
    {
        int k = refIdxs[dir];
        weight_pred_8xN(blk.mbDst[0], lPitch, ctx->lumaScale[k], ctx->lumaDelta[k], 16);
        weight_pred_4xN(blk.mbDst[1], cPitch, ctx->cbcrScale[k], ctx->cbcrDelta[k], 8);
        weight_pred_4xN(blk.mbDst[2], cPitch, ctx->cbcrScale[k], ctx->cbcrDelta[k], 8);
    }

    // 双向预测
    if (blk.predFlag == PRED_SYM)
    {
        assert(dir == 0 && (refIdxs[0] & 1) == 0);
        refIdxs[1] = refIdxs[0] ^ ctx->backIdxXor;
        curMvs[1] = sym_mv_scale(curMvs[0], ctx->backMvScale[refIdxs[0]]);

        uint8_t* mcBuf[3] = {ctx->mcBuff, ctx->mcBuff + 256, ctx->mcBuff + 512};
        int x = blk.bx + curMvs[1].x;
        int y = blk.by + curMvs[1].y;
        const RefPicture* refPic = ctx->refPics + refIdxs[1];
        luma_inter_pred_8x16(ctx, refPic, mcBuf[0], 16, x, y);
        chroma_inter_pred_4x8(ctx, refPic, mcBuf[1], mcBuf[2], 8, x, y);
        if (blk.wpFlag)        // 加权预测
        {
            int k = refIdxs[1];
            weight_pred_8xN(mcBuf[0], 16, ctx->lumaScale[k], ctx->lumaDelta[k], 16);
            weight_pred_4xN(mcBuf[1], 8, ctx->cbcrScale[k], ctx->cbcrDelta[k], 8);
            weight_pred_4xN(mcBuf[2], 8, ctx->cbcrScale[k], ctx->cbcrDelta[k], 8);
        }

        // 取前后预测平均值
        MC_avg_8xN(mcBuf[0], 16, blk.mbDst[0], lPitch, 16);
        MC_avg_4xN(mcBuf[1], 8, blk.mbDst[1], cPitch, 8);
        MC_avg_4xN(mcBuf[2], 8, blk.mbDst[2], cPitch, 8);
    }
    else
    {
        // 这个预测方向没有运动矢量
        refIdxs[dir ^ 1] = -1;
        curMvs[dir ^ 1].i32 = 0;
    }
}

// B_8x8 块帧间预测
static void MC_blockB_8x8(FrmDecContext* ctx, const BlkDecArgv& blk, int8_t refIdxs[2], MvUnion curMvs[2])
{
    const int dir = (blk.predFlag & 1) ^ 1;     // 预测方向索引
    const int lPitch = ctx->picPitch[0];
    const int cPitch = ctx->picPitch[1];
    int x = blk.bx + curMvs[dir].x;
    int y = blk.by + curMvs[dir].y;
    const RefPicture* refPic = ctx->refPics + refIdxs[dir];
    luma_inter_pred_8x8(ctx, refPic, blk.mbDst[0], lPitch, x, y);
    chroma_inter_pred_4x4(ctx, refPic, blk.mbDst[1], blk.mbDst[2], cPitch, x, y);
    if (blk.wpFlag)           // 加权预测
    {
        int k = refIdxs[dir];
        weight_pred_8xN(blk.mbDst[0], lPitch, ctx->lumaScale[k], ctx->lumaDelta[k], 8);
        weight_pred_4xN(blk.mbDst[1], cPitch, ctx->cbcrScale[k], ctx->cbcrDelta[k], 4);
        weight_pred_4xN(blk.mbDst[2], cPitch, ctx->cbcrScale[k], ctx->cbcrDelta[k], 4);
    }

    // 双向预测
    if (blk.predFlag == PRED_SYM)
    {
        assert(dir == 0 && (refIdxs[0] & 1) == 0);
        refIdxs[1] = refIdxs[0] ^ ctx->backIdxXor;
        curMvs[1] = sym_mv_scale(curMvs[0], ctx->backMvScale[refIdxs[0]]);

        uint8_t* mcBuf[3] = {ctx->mcBuff, ctx->mcBuff + 256, ctx->mcBuff + 512};
        int x = blk.bx + curMvs[1].x;
        int y = blk.by + curMvs[1].y;
        const RefPicture* refPic = ctx->refPics + refIdxs[1];
        luma_inter_pred_8x8(ctx, refPic, mcBuf[0], 16, x, y);
        chroma_inter_pred_4x4(ctx, refPic, mcBuf[1], mcBuf[2], 8, x, y);
        if (blk.wpFlag)        // 加权预测
        {
            int k = refIdxs[1];
            weight_pred_8xN(mcBuf[0], 16, ctx->lumaScale[k], ctx->lumaDelta[k], 8);
            weight_pred_4xN(mcBuf[1], 8, ctx->cbcrScale[k], ctx->cbcrDelta[k], 4);
            weight_pred_4xN(mcBuf[2], 8, ctx->cbcrScale[k], ctx->cbcrDelta[k], 4);
        }

        // 取前后预测平均值
        MC_avg_8xN(mcBuf[0], 16, blk.mbDst[0], lPitch, 8);
        MC_avg_4xN(mcBuf[1], 8, blk.mbDst[1], cPitch, 4);
        MC_avg_4xN(mcBuf[2], 8, blk.mbDst[2], cPitch, 4);
    }
    else
    {
        // 这个预测方向没有运动矢量
        refIdxs[dir ^ 1] = -1;
        curMvs[dir ^ 1].i32 = 0;
    }
}

// SB_Direct_8x8 解码
static void MC_BDirect_8x8(FrmDecContext* ctx, const BlkDecArgv& blk, int8_t refIdxs[2], MvUnion curMvs[2])
{
    // 得到运动矢量
    const int mx = blk.bx >> 6;
    const int my = blk.by >> 6;
    const int blkIdx = ((blk.bx >> 5) & 1) | ((blk.by >> 4) & 2);
    get_BD_mv(ctx, mx, my, blkIdx, refIdxs, curMvs);

    const int lPitch = ctx->picPitch[0];
    const int cPitch = ctx->picPitch[1];
    int x = blk.bx + curMvs[0].x;
    int y = blk.by + curMvs[0].y;
    const RefPicture* refPic = ctx->refPics + refIdxs[0];
    luma_inter_pred_8x8(ctx, refPic, blk.mbDst[0], lPitch, x, y);
    chroma_inter_pred_4x4(ctx, refPic, blk.mbDst[1], blk.mbDst[2], cPitch, x, y);
    if (blk.wpFlag)    // 加权预测
    {
        int k = refIdxs[0];
        weight_pred_8xN(blk.mbDst[0], lPitch, ctx->lumaScale[k], ctx->lumaDelta[k], 8);
        weight_pred_4xN(blk.mbDst[1], cPitch, ctx->cbcrScale[k], ctx->cbcrDelta[k], 4);
        weight_pred_4xN(blk.mbDst[2], cPitch, ctx->cbcrScale[k], ctx->cbcrDelta[k], 4);
    }

    uint8_t* mcBuf[3] = {ctx->mcBuff, ctx->mcBuff + 256, ctx->mcBuff + 512};
    x = blk.bx + curMvs[1].x;
    y = blk.by + curMvs[1].y;
    refPic = ctx->refPics + refIdxs[1];
    luma_inter_pred_8x8(ctx, refPic, mcBuf[0], 16, x, y);
    chroma_inter_pred_4x4(ctx, refPic, mcBuf[1], mcBuf[2], 8, x, y);
    if (blk.wpFlag)    // 加权预测
    {
        int k = refIdxs[1];
        weight_pred_8xN(mcBuf[0], 16, ctx->lumaScale[k], ctx->lumaDelta[k], 8);
        weight_pred_4xN(mcBuf[1], 8, ctx->cbcrScale[k], ctx->cbcrDelta[k], 4);
        weight_pred_4xN(mcBuf[2], 8, ctx->cbcrScale[k], ctx->cbcrDelta[k], 4);
    }

    // 取前后预测平均值
    MC_avg_8xN(mcBuf[0], 16, blk.mbDst[0], lPitch, 8);
    MC_avg_4xN(mcBuf[1], 8, blk.mbDst[1], cPitch, 4);
    MC_avg_4xN(mcBuf[2], 8, blk.mbDst[2], cPitch, 4);
}

// B-Skip 宏块解码
void dec_macroblock_BSkip(FrmDecContext* ctx, int mx, int my)
{
    const int wpFlag = (ctx->sliceWPFlag && !ctx->mbWPFlag);
    MC_BDirect_16x16(ctx, mx, my, wpFlag);
}

// BDirect_16x16 宏块解码
void dec_macroblock_BDirect(FrmDecContext* ctx, int mx, int my)
{
    // 解析 weighting_prediction
    AvsBitStream& bitsm = ctx->bitsm;
    const int wpFlag = ctx->mbWPFlag ? bitsm.read1() : ctx->sliceWPFlag;

    // 解析 CBP
    int cbpIdx = bitsm.read_ue8();
    if (cbpIdx >= 64)
    {
        ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
        return;
    }
    const uint32_t cbpFlags = s_CBPTab[cbpIdx][1];

    // qp_delta
    if (cbpFlags && ctx->bFixedQp == 0)
    {
        ctx->curQp += bitsm.read_se8();
        if (ctx->curQp & ~63)      // qp 超出了 [0,63] 的范围
        {
            ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
            return;
        }
    }

    // B_Direct 运动预测
    MC_BDirect_16x16(ctx, mx, my, wpFlag);

    // 叠加残差
    if (!dec_mb_residual(ctx, mx, my, cbpFlags))
    {
        ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
        return;
    }
}

// B_16x16 宏块解码
void dec_macroblock_B16x16(FrmDecContext* ctx, int mx, int my)
{
    // 解析参考帧索引
    AvsBitStream& bitsm = ctx->bitsm;
    int rawIdx = ctx->picHdr.pic_ref_flag ? 0 : bitsm.read1();

    // 解析 mv_diff
    int16_t mvDiff[2];
    mvDiff[0] = bitsm.read_se16();
    mvDiff[1] = bitsm.read_se16();

    // 解析 weighting_prediction
    const int wpFlag = ctx->mbWPFlag ? bitsm.read1() : ctx->sliceWPFlag;

    // 解析 CBP
    int cbpIdx = bitsm.read_ue8();
    if (cbpIdx >= 64)
    {
        ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
        return;
    }
    const uint32_t cbpFlags = s_CBPTab[cbpIdx][1];

    // qp_delta
    if (cbpFlags && ctx->bFixedQp == 0)
    {
        int qpDelta = bitsm.read_se8();
        ctx->curQp += qpDelta;
        if (ctx->curQp & ~63)              // qp 超出了 [0,63] 的范围
        {
            ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
            return;
        }
    }

    // 运动补偿
    MC_macroblock_B16x16(ctx, mx, my, rawIdx, mvDiff, wpFlag);

    // 叠加残差
    if (!dec_mb_residual(ctx, mx, my, cbpFlags))
    {
        ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
        return;
    }
}

// B_16x8 宏块解码
void dec_macroblock_B16x8(FrmDecContext* ctx, int mx, int my)
{
    AvsBitStream& bitsm = ctx->bitsm;
    BlkDecArgv blkArg;
    int8_t rawIdxs[2] = {0};
    int16_t mvDiff[2][2];

    // 解析参考帧索引    
    if (ctx->picHdr.pic_ref_flag == 0)
    {
        rawIdxs[0] = bitsm.read1();
        rawIdxs[1] = bitsm.read1();
    }

    // 解析 mv_diff  
    mvDiff[0][0] = bitsm.read_se16();
    mvDiff[0][1] = bitsm.read_se16();
    mvDiff[1][0] = bitsm.read_se16();
    mvDiff[1][1] = bitsm.read_se16();

    // 解析 weighting_prediction
    blkArg.wpFlag = ctx->mbWPFlag ? bitsm.read1() : ctx->sliceWPFlag;

    // 解析 CBP
    int cbpIdx = bitsm.read_ue8();
    if (cbpIdx >= 64)
    {
        ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
        return;
    }
    const uint32_t cbpFlags = s_CBPTab[cbpIdx][1];

    // qp_delta
    if (cbpFlags && ctx->bFixedQp == 0)
    {
        int qpDelta = bitsm.read_se8();
        ctx->curQp += qpDelta;
        if (ctx->curQp & ~63)              // qp 超出了 [0,63] 的范围
        {
            ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
            return;
        }
    }

    MbContext* leftMb = &ctx->leftMb;
    MbContext* topMb = ctx->topLine + mx;
    MbContext* curMb = ctx->curLine + mx;
    int8_t refIdxs[2][2];
    MvUnion curMvs[2][2];

    //------------------------------ block 0 ------------------------------
    uint8_t predFlags[2] = {g_BPredFlags[ctx->mbTypeIdx][0], g_BPredFlags[ctx->mbTypeIdx][1]};
    int dir = (predFlags[0] & 1) ^ 1;                       // 运动方向索引
    int idx = (predFlags[0] & 1) < (predFlags[1] & 1);      // 标准说所有前向参考先解析, 莫名奇妙
    refIdxs[0][dir] = (rawIdxs[idx] << 1) | dir;            // 变换后的参考帧索引

    if (topMb->refIdxs[0][dir] == refIdxs[0][dir])
    {
        curMvs[0][dir] = topMb->mvs[0][dir];
    }
    else
    {
        MvInfo nbs[3];
        nbs[0].refIdx = leftMb->refIdxs[0][dir];
        nbs[0].mv = leftMb->mvs[0][dir];
        nbs[0].denDist = ctx->denDist[nbs[0].refIdx];
        nbs[1].refIdx = topMb[0].refIdxs[0][dir];
        nbs[1].mv = topMb[0].mvs[0][dir];
        nbs[1].denDist = ctx->denDist[nbs[1].refIdx];
        if (topMb[1].avail)    // BlockC 存在
        {
            nbs[2].refIdx = topMb[1].refIdxs[0][dir];
            nbs[2].mv = topMb[1].mvs[0][dir];
        }
        else    // BlockC 不存在, 使用 BlockD 代替
        {
            nbs[2].refIdx = topMb[-1].refIdxs[1][dir];
            nbs[2].mv = topMb[-1].mvs[1][dir];
        }
        nbs[2].denDist = ctx->denDist[nbs[2].refIdx];
        curMvs[0][dir] = get_mv_pred(nbs, ctx->refDist[refIdxs[0][dir]]);
    }
    curMvs[0][dir].x += mvDiff[idx][0];
    curMvs[0][dir].y += mvDiff[idx][1];

    // 帧间预测
    const int lPitch = ctx->picPitch[0];
    const int cPitch = ctx->picPitch[1];

    blkArg.predFlag = predFlags[0];
    blkArg.bx = mx << 6;
    blkArg.by = my << 6;
    blkArg.mbDst[0] = ctx->picPlane[0] + (my * lPitch + mx) * 16;
    blkArg.mbDst[1] = ctx->picPlane[1] + (my * cPitch + mx) * 8;
    blkArg.mbDst[2] = ctx->picPlane[2] + (my * cPitch + mx) * 8;
    MC_blockB_16x8(ctx, blkArg, refIdxs[0], curMvs[0]);

    //------------------------------ block 1 ------------------------------
    dir = (predFlags[1] & 1) ^ 1;
    idx ^= 1;
    refIdxs[1][dir] = (rawIdxs[idx] << 1) | dir;

    // 解析运动矢量
    if (leftMb->refIdxs[1][dir] == refIdxs[1][dir])
    {
        curMvs[1][dir] = leftMb->mvs[1][dir];
    }
    else
    {
        MvInfo nbs[3];
        nbs[0].refIdx = leftMb->refIdxs[1][dir];
        nbs[0].mv = leftMb->mvs[1][dir];
        nbs[0].denDist = ctx->denDist[nbs[0].refIdx];
        nbs[1].refIdx = refIdxs[0][dir];
        nbs[1].mv = curMvs[0][dir];
        nbs[1].denDist = ctx->denDist[nbs[1].refIdx];
        nbs[2].refIdx = leftMb->refIdxs[0][dir];
        nbs[2].mv = leftMb->mvs[0][dir];
        nbs[2].denDist = ctx->denDist[nbs[2].refIdx];
        curMvs[1][dir] = get_mv_pred(nbs, ctx->refDist[refIdxs[1][dir]]);
    }
    curMvs[1][dir].x += mvDiff[idx][0];
    curMvs[1][dir].y += mvDiff[idx][1];

    // 帧间预测
    blkArg.predFlag = predFlags[1];
    blkArg.by += 32;
    blkArg.mbDst[0] += 8 * lPitch;
    blkArg.mbDst[1] += 4 * cPitch;
    blkArg.mbDst[2] += 4 * cPitch;
    MC_blockB_16x8(ctx, blkArg, refIdxs[1], curMvs[1]);

    // 设置环路滤波相关参数
    if (ctx->lfDisabled == 0)
    {
        curMb->leftQp = leftMb->curQp;
        curMb->topQp = topMb->curQp;
        curMb->curQp = ctx->curQp;
        leftMb->curQp = ctx->curQp;
        curMb->lfBS = calc_BS_B16x8(refIdxs, curMvs, leftMb, topMb);
    }

    // 存储参考帧索引和运动矢量
    curMb->avail = 1;
    curMb->refIdxs[0][0] = refIdxs[1][0];
    curMb->refIdxs[0][1] = refIdxs[1][1];
    curMb->refIdxs[1][0] = refIdxs[1][0];
    curMb->refIdxs[1][1] = refIdxs[1][1];
    curMb->mvs[0][0] = curMvs[1][0];
    curMb->mvs[0][1] = curMvs[1][1];
    curMb->mvs[1][0] = curMvs[1][0];
    curMb->mvs[1][1] = curMvs[1][1];
    leftMb->avail = 1;
    leftMb->refIdxs[0][0] = refIdxs[0][0];
    leftMb->refIdxs[0][1] = refIdxs[0][1];
    leftMb->refIdxs[1][0] = refIdxs[1][0];
    leftMb->refIdxs[1][1] = refIdxs[1][1];
    leftMb->mvs[0][0] = curMvs[0][0];
    leftMb->mvs[0][1] = curMvs[0][1];
    leftMb->mvs[1][0] = curMvs[1][0];
    leftMb->mvs[1][1] = curMvs[1][1];

    // 叠加残差
    if (!dec_mb_residual(ctx, mx, my, cbpFlags))
    {
        ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
        return;
    }
}

// B_8x16 宏块解码
void dec_macroblock_B8x16(FrmDecContext* ctx, int mx, int my)
{
    AvsBitStream& bitsm = ctx->bitsm;
    BlkDecArgv blkArg;
    int8_t rawIdxs[2] = {0};
    int16_t mvDiff[2][2];

    // 解析参考帧索引
    if (ctx->picHdr.pic_ref_flag == 0)
    {
        rawIdxs[0] = bitsm.read1();
        rawIdxs[1] = bitsm.read1();
    }

    // 解析 mv_diff
    mvDiff[0][0] = bitsm.read_se16();
    mvDiff[0][1] = bitsm.read_se16();
    mvDiff[1][0] = bitsm.read_se16();
    mvDiff[1][1] = bitsm.read_se16();

    // 解析 weighting_prediction
    blkArg.wpFlag = ctx->mbWPFlag ? bitsm.read1() : ctx->sliceWPFlag;

    // 解析 CBP
    int cbpIdx = bitsm.read_ue8();
    if (cbpIdx >= 64)
    {
        ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
        return;
    }
    const uint32_t cbpFlags = s_CBPTab[cbpIdx][1];

    // qp_delta
    if (cbpFlags && ctx->bFixedQp == 0)
    {
        ctx->curQp += bitsm.read_se8();
        if (ctx->curQp & ~63)              // qp 超出了 [0,63] 的范围
        {
            ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
            return;
        }
    }

    MbContext* leftMb = &ctx->leftMb;
    MbContext* topMb = ctx->topLine + mx;
    MbContext* curMb = ctx->curLine + mx;
    int8_t refIdxs[2][2];
    MvUnion curMvs[2][2];

    //------------------------------ block 0 ------------------------------
    uint8_t predFlags[2] = {g_BPredFlags[ctx->mbTypeIdx][0], g_BPredFlags[ctx->mbTypeIdx][1]};
    int dir = (predFlags[0] & 1) ^ 1;                       // 运动方向索引
    int idx = (predFlags[0] & 1) < (predFlags[1] & 1);      // 标准说所有前向参考先解析, 莫名奇妙
    refIdxs[0][dir] = (rawIdxs[idx] << 1) | dir;            // 变换后的参考帧索引

    if (leftMb->refIdxs[0][dir] == refIdxs[0][dir])
    {
        curMvs[0][dir] = leftMb->mvs[0][dir];
    }
    else
    {
        MvInfo nbs[3];
        nbs[0].refIdx = leftMb->refIdxs[0][dir];
        nbs[0].mv = leftMb->mvs[0][dir];
        nbs[0].denDist = ctx->denDist[nbs[0].refIdx];
        nbs[1].refIdx = topMb->refIdxs[0][dir];
        nbs[1].mv = topMb->mvs[0][dir];
        nbs[1].denDist = ctx->denDist[nbs[1].refIdx];
        nbs[2].refIdx = topMb->refIdxs[1][dir];
        nbs[2].mv = topMb->mvs[1][dir];
        nbs[2].denDist = ctx->denDist[nbs[2].refIdx];
        curMvs[0][dir] = get_mv_pred(nbs, ctx->refDist[refIdxs[0][dir]]);
    }
    curMvs[0][dir].x += mvDiff[idx][0];
    curMvs[0][dir].y += mvDiff[idx][1];

    // 帧间预测
    blkArg.predFlag = predFlags[0];
    blkArg.bx = mx << 6;
    blkArg.by = my << 6;
    blkArg.mbDst[0] = ctx->picPlane[0] + (my * ctx->picPitch[0] + mx) * 16;
    blkArg.mbDst[1] = ctx->picPlane[1] + (my * ctx->picPitch[1] + mx) * 8;
    blkArg.mbDst[2] = ctx->picPlane[2] + (my * ctx->picPitch[1] + mx) * 8;
    MC_blockB_8x16(ctx, blkArg, refIdxs[0], curMvs[0]);

    //------------------------------ block 1 ------------------------------
    dir = (predFlags[1] & 1) ^ 1;
    idx ^= 1;
    refIdxs[1][dir] = (rawIdxs[idx] << 1) | dir;

    // 解析运动矢量
    if (topMb[1].avail)    // BlockC 存在
    {
        if (topMb[1].refIdxs[0][dir] == refIdxs[1][dir])
        {
            curMvs[1][dir] = topMb[1].mvs[0][dir];
        }
        else
        {
            MvInfo nbs[3];
            nbs[0].refIdx = refIdxs[0][dir];
            nbs[0].mv = curMvs[0][dir];
            nbs[0].denDist = ctx->denDist[nbs[0].refIdx];
            nbs[1].refIdx = topMb->refIdxs[1][dir];
            nbs[1].mv = topMb->mvs[1][dir];
            nbs[1].denDist = ctx->denDist[nbs[1].refIdx];
            nbs[2].refIdx = topMb[1].refIdxs[0][dir];
            nbs[2].mv = topMb[1].mvs[0][dir];
            nbs[2].denDist = ctx->denDist[nbs[2].refIdx];
            curMvs[1][dir] = get_mv_pred(nbs, ctx->refDist[refIdxs[1][dir]]);
        }
    }
    else    // BlockC 不存在, 根据标准使用 BlockD 代替
    {
        if (topMb->refIdxs[0][dir] == refIdxs[1][dir])
        {
            curMvs[1][dir] = topMb->mvs[0][dir];
        }
        else
        {
            MvInfo nbs[3];
            nbs[0].refIdx = refIdxs[0][dir];
            nbs[0].mv = curMvs[0][dir];
            nbs[0].denDist = ctx->denDist[nbs[0].refIdx];
            nbs[1].refIdx = topMb->refIdxs[1][dir];
            nbs[1].mv = topMb->mvs[1][dir];
            nbs[1].denDist = ctx->denDist[nbs[1].refIdx];
            nbs[2].refIdx = topMb->refIdxs[0][dir];
            nbs[2].mv = topMb->mvs[0][dir];
            nbs[2].denDist = ctx->denDist[nbs[2].refIdx];
            curMvs[1][dir] = get_mv_pred(nbs, ctx->refDist[refIdxs[1][dir]]);
        }
    }
    curMvs[1][dir].x += mvDiff[idx][0];
    curMvs[1][dir].y += mvDiff[idx][1];

    // 帧间预测
    blkArg.predFlag = predFlags[1];
    blkArg.bx += 32;
    blkArg.mbDst[0] += 8;
    blkArg.mbDst[1] += 4;
    blkArg.mbDst[2] += 4;
    MC_blockB_8x16(ctx, blkArg, refIdxs[1], curMvs[1]);

    // 设置环路滤波相关参数
    if (ctx->lfDisabled == 0)
    {
        curMb->leftQp = leftMb->curQp;
        curMb->topQp = topMb->curQp;
        curMb->curQp = ctx->curQp;
        leftMb->curQp = ctx->curQp;
        curMb->lfBS = calc_BS_B8x16(refIdxs, curMvs, leftMb, topMb);
    }

    // 存储参考帧索引和运动矢量
    curMb->avail = 1;
    curMb->refIdxs[0][0] = refIdxs[0][0];
    curMb->refIdxs[0][1] = refIdxs[0][1];
    curMb->refIdxs[1][0] = refIdxs[1][0];
    curMb->refIdxs[1][1] = refIdxs[1][1];
    curMb->mvs[0][0] = curMvs[0][0];
    curMb->mvs[0][1] = curMvs[0][1];
    curMb->mvs[1][0] = curMvs[1][0];
    curMb->mvs[1][1] = curMvs[1][1];
    leftMb->avail = 1;
    leftMb->refIdxs[0][0] = refIdxs[1][0];
    leftMb->refIdxs[0][1] = refIdxs[1][1];
    leftMb->refIdxs[1][0] = refIdxs[1][0];
    leftMb->refIdxs[1][1] = refIdxs[1][1];
    leftMb->mvs[0][0] = curMvs[1][0];
    leftMb->mvs[0][1] = curMvs[1][1];
    leftMb->mvs[1][0] = curMvs[1][0];
    leftMb->mvs[1][1] = curMvs[1][1];

    // 叠加残差
    if (!dec_mb_residual(ctx, mx, my, cbpFlags))
    {
        ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
        return;
    }
}

// B_8x8 宏块解码
void dec_macroblock_B8x8(FrmDecContext* ctx, int mx, int my)
{
    AvsBitStream& bitsm = ctx->bitsm;
    uint8_t partType[4];
    int8_t rawIdxs[4] = {0};
    int16_t mvDiff[4][2] = {0};
    BlkDecArgv blkArg;

    // mb_part_type
    uint8_t tmp = bitsm.read_bits(8);
    partType[0] = (tmp >> 6) & 3;
    partType[1] = (tmp >> 4) & 3;
    partType[2] = (tmp >> 2) & 3;
    partType[3] = (tmp >> 0) & 3;

    // 非 SB_Direct_8x8 子宏块数目
    const int mvNum = (partType[0] != 0) + (partType[1] != 0) + (partType[2] != 0) + (partType[3] != 0);

    // 解析参考帧索引
    int8_t tmpIdxs[4] = {0};
    if (ctx->picHdr.pic_ref_flag == 0)
    {
        for (int i = 0; i < mvNum; i++)
            tmpIdxs[i] = bitsm.read1();
    }

    // 解析 mv_diff
    int16_t tmpDiff[4][2];
    for (int i = 0; i < mvNum; i++)
    {
        tmpDiff[i][0] = bitsm.read_se16();
        tmpDiff[i][1] = bitsm.read_se16();
    }

    // 这个愚蠢的标准说先解析全部前向参考, 再解析全部后向参考
    int idx = 0;
    for (int i = 0; i < 4; i++)
    {
        if (partType[i] & PRED_FWD)
        {
            rawIdxs[i] = tmpIdxs[idx];
            mvDiff[i][0] = tmpDiff[idx][0];
            mvDiff[i][1] = tmpDiff[idx][1];
            idx++;
        }
    }
    for (int i = 0; i < 4; i++)
    {
        if (partType[i] == PRED_BCK)
        {
            rawIdxs[i] = tmpIdxs[idx];
            mvDiff[i][0] = tmpDiff[idx][0];
            mvDiff[i][1] = tmpDiff[idx][1];
            idx++;
        }
    }
    assert(mvNum == idx);

    // 解析 weighting_prediction
    blkArg.wpFlag = ctx->mbWPFlag ? bitsm.read1() : ctx->sliceWPFlag;

    // 解析 CBP
    int cbpIdx = bitsm.read_ue8();
    if (cbpIdx >= 64)
    {
        ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
        return;
    }
    const uint32_t cbpFlags = s_CBPTab[cbpIdx][1];

    // qp_delta
    if (cbpFlags && ctx->bFixedQp == 0)
    {
        int qpDelta = bitsm.read_se8();
        ctx->curQp += qpDelta;
        if (ctx->curQp & ~63)              // qp 超出了 [0,63] 的范围
        {
            ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
            return;
        }
    }

    const int lPitch = ctx->picPitch[0];
    const int cPitch = ctx->picPitch[1];
    MbContext* leftMb = &ctx->leftMb;
    MbContext* topMb = ctx->topLine + mx;
    MbContext* curMb = ctx->curLine + mx;
    const int* distDen = ctx->denDist;
    MvInfo nbs[3];
    int8_t refIdxs[4][2];
    MvUnion curMvs[4][2];

    //------------------------------- block 0 --------------------------------
    blkArg.predFlag = partType[0];
    blkArg.bx = mx << 6;
    blkArg.by = my << 6;
    blkArg.mbDst[0] = ctx->picPlane[0] + (my * lPitch + mx) * 16;
    blkArg.mbDst[1] = ctx->picPlane[1] + (my * cPitch + mx) * 8;
    blkArg.mbDst[2] = ctx->picPlane[2] + (my * cPitch + mx) * 8;
    if (partType[0] != 0)
    {
        const int dir = (partType[0] & 1) ^ 1;          // 预测方向索引
        refIdxs[0][dir] = (rawIdxs[0] << 1) | dir;      // 变换后的参考帧索引
        nbs[0].refIdx = leftMb->refIdxs[0][dir];
        nbs[0].mv = leftMb->mvs[0][dir];
        nbs[0].denDist = distDen[nbs[0].refIdx];
        nbs[1].refIdx = topMb->refIdxs[0][dir];
        nbs[1].mv = topMb->mvs[0][dir];
        nbs[1].denDist = distDen[nbs[1].refIdx];
        nbs[2].refIdx = topMb->refIdxs[1][dir];
        nbs[2].mv = topMb->mvs[1][dir];
        nbs[2].denDist = distDen[nbs[2].refIdx];
        curMvs[0][dir] = get_mv_pred(nbs, ctx->refDist[refIdxs[0][dir]]);
        curMvs[0][dir].x += mvDiff[0][0];
        curMvs[0][dir].y += mvDiff[0][1];

        MC_blockB_8x8(ctx, blkArg, refIdxs[0], curMvs[0]);
    }
    else    // SB_Direct_8x8
    {
        MC_BDirect_8x8(ctx, blkArg, refIdxs[0], curMvs[0]);
    }

    //------------------------------- block 1 --------------------------------
    blkArg.predFlag = partType[1];
    blkArg.bx += 32;
    blkArg.mbDst[0] += 8;
    blkArg.mbDst[1] += 4;
    blkArg.mbDst[2] += 4;
    if (partType[1] != 0)
    {
        const int dir = (partType[1] & 1) ^ 1;          // 预测方向索引
        refIdxs[1][dir] = (rawIdxs[1] << 1) | dir;      // 变换后的参考帧索引
        nbs[0].refIdx = refIdxs[0][dir];
        nbs[0].mv = curMvs[0][dir];
        nbs[0].denDist = distDen[nbs[0].refIdx];
        nbs[1].refIdx = topMb->refIdxs[1][dir];
        nbs[1].mv = topMb->mvs[1][dir];
        nbs[1].denDist = distDen[nbs[1].refIdx];
        if (topMb[1].avail)    // BlockC 存在
        {
            nbs[2].refIdx = topMb[1].refIdxs[0][dir];
            nbs[2].mv = topMb[1].mvs[0][dir];
        }
        else    // BlockC 不存在, 使用 BlockD 代替
        {
            nbs[2].refIdx = topMb->refIdxs[0][dir];
            nbs[2].mv = topMb->mvs[0][dir];
        }
        nbs[2].denDist = distDen[nbs[2].refIdx];
        curMvs[1][dir] = get_mv_pred(nbs, ctx->refDist[refIdxs[1][dir]]);
        curMvs[1][dir].x += mvDiff[1][0];
        curMvs[1][dir].y += mvDiff[1][1];

        MC_blockB_8x8(ctx, blkArg, refIdxs[1], curMvs[1]);
    }
    else    // SB_Direct_8x8
    {
        MC_BDirect_8x8(ctx, blkArg, refIdxs[1], curMvs[1]);
    }

    //------------------------------- block 2 --------------------------------
    blkArg.predFlag = partType[2];
    blkArg.bx -= 32;
    blkArg.by += 32;
    blkArg.mbDst[0] += 8 * lPitch - 8;
    blkArg.mbDst[1] += 4 * cPitch - 4;
    blkArg.mbDst[2] += 4 * cPitch - 4;
    if (partType[2] != 0)
    {
        const int dir = (partType[2] & 1) ^ 1;          // 预测方向索引
        refIdxs[2][dir] = (rawIdxs[2] << 1) | dir;      // 变换后的参考帧索引
        nbs[0].refIdx = leftMb->refIdxs[1][dir];
        nbs[0].mv = leftMb->mvs[1][dir];
        nbs[0].denDist = distDen[nbs[0].refIdx];
        nbs[1].refIdx = refIdxs[0][dir];
        nbs[1].mv = curMvs[0][dir];
        nbs[1].denDist = distDen[nbs[1].refIdx];
        nbs[2].refIdx = refIdxs[1][dir];
        nbs[2].mv = curMvs[1][dir];
        nbs[2].denDist = distDen[nbs[2].refIdx];
        curMvs[2][dir] = get_mv_pred(nbs, ctx->refDist[refIdxs[2][dir]]);
        curMvs[2][dir].x += mvDiff[2][0];
        curMvs[2][dir].y += mvDiff[2][1];

        MC_blockB_8x8(ctx, blkArg, refIdxs[2], curMvs[2]);
    }
    else    // SB_Direct_8x8
    {
        MC_BDirect_8x8(ctx, blkArg, refIdxs[2], curMvs[2]);
    }

    //------------------------------- block 3 --------------------------------
    blkArg.predFlag = partType[3];
    blkArg.bx += 32;
    blkArg.mbDst[0] += 8;
    blkArg.mbDst[1] += 4;
    blkArg.mbDst[2] += 4;
    if (partType[3] != 0)
    {
        const int dir = (partType[3] & 1) ^ 1;          // 预测方向索引
        refIdxs[3][dir] = (rawIdxs[3] << 1) | dir;      // 变换后的参考帧索引
        nbs[0].refIdx = refIdxs[2][dir];
        nbs[0].mv = curMvs[2][dir];
        nbs[0].denDist = distDen[nbs[0].refIdx];
        nbs[1].refIdx = refIdxs[1][dir];
        nbs[1].mv = curMvs[1][dir];
        nbs[1].denDist = distDen[nbs[1].refIdx];
        nbs[2].refIdx = refIdxs[0][dir];
        nbs[2].mv = curMvs[0][dir];
        nbs[2].denDist = distDen[nbs[2].refIdx];
        curMvs[3][dir] = get_mv_pred(nbs, ctx->refDist[refIdxs[3][dir]]);
        curMvs[3][dir].x += mvDiff[3][0];
        curMvs[3][dir].y += mvDiff[3][1];

        MC_blockB_8x8(ctx, blkArg, refIdxs[3], curMvs[3]);
    }
    else    // SB_Direct_8x8
    {
        MC_BDirect_8x8(ctx, blkArg, refIdxs[3], curMvs[3]);
    }

    // 设置环路滤波相关参数
    if (ctx->lfDisabled == 0)
    {
        curMb->leftQp = leftMb->curQp;
        curMb->topQp = topMb->curQp;
        curMb->curQp = ctx->curQp;
        leftMb->curQp = ctx->curQp;
        curMb->lfBS = calc_BS_B8x8(refIdxs, curMvs, leftMb, topMb);
    }

    // 存储参考帧索引和运动矢量
    curMb->avail = 1;
    curMb->refIdxs[0][0] = refIdxs[2][0];
    curMb->refIdxs[0][1] = refIdxs[2][1];
    curMb->refIdxs[1][0] = refIdxs[3][0];
    curMb->refIdxs[1][1] = refIdxs[3][1];
    curMb->mvs[0][0] = curMvs[2][0];
    curMb->mvs[0][1] = curMvs[2][1];
    curMb->mvs[1][0] = curMvs[3][0];
    curMb->mvs[1][1] = curMvs[3][1];
    leftMb->avail = 1;
    leftMb->refIdxs[0][0] = refIdxs[1][0];
    leftMb->refIdxs[0][1] = refIdxs[1][1];
    leftMb->refIdxs[1][0] = refIdxs[3][0];
    leftMb->refIdxs[1][1] = refIdxs[3][1];
    leftMb->mvs[0][0] = curMvs[1][0];
    leftMb->mvs[0][1] = curMvs[1][1];
    leftMb->mvs[1][0] = curMvs[3][0];
    leftMb->mvs[1][1] = curMvs[3][1];

    // 叠加残差
    if (!dec_mb_residual(ctx, mx, my, cbpFlags))
    {
        ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
        return;
    }
}

//======================================================================================================================
// 以下为高级熵编码模式

// 解析 qp_delta
static inline bool parse_qp_AEC(FrmDecContext* ctx)
{
    AvsAecParser* parser = ctx->aecParser;
    int qpDelta = 0;
    int ctxInc = (ctx->prevQpDelta != 0);
    if (parser->dec_decision(54 + ctxInc) == 0)
    {
        if (parser->dec_decision(56) != 0)
        {
            qpDelta = 1;
        }
        else
        {
            qpDelta = 2 + parser->dec_zero_cnt(57, 64);
            int sign = 2 * (qpDelta & 1) - 1;
            qpDelta = (qpDelta + 1) >> 1;
            qpDelta *= sign;
        }
    }
    ctx->prevQpDelta = qpDelta;
    ctx->curQp += qpDelta;
    if (ctx->curQp & ~63)      // qp 超出了 [0,63] 的范围
    {
        ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
        return false;
    }

    return true;
}

// 解析 DCT 系数并与帧间预测结果叠加
static bool dec_mb_residual_AEC(FrmDecContext* ctx, int mx, int my, uint32_t cbpFlags)
{
    AvsAecParser* parser = ctx->aecParser;
    const int dqScale = g_DequantScale[ctx->curQp];
    const int dqShift = g_DequantShift[ctx->curQp];
    int16_t* coeff = ctx->coeff;        // 存储 DCT 系数的临时内存

    // decode Luma block 0
    const int lPitch = ctx->picPitch[0];
    uint8_t* luma = ctx->picPlane[0] + (my * lPitch + mx) * 16;
    if (cbpFlags & 0x1)
    {
        if (!parser->dec_coeff_block(coeff, 58, dqScale, dqShift))
            return false;
        IDCT_8x8_add_sse4(coeff, luma, lPitch);
    }

    // decode Luma block 1
    if (cbpFlags & 0x2)
    {
        if (!parser->dec_coeff_block(coeff, 58, dqScale, dqShift))
            return false;
        IDCT_8x8_add_sse4(coeff, luma + 8, lPitch);
    }

    // decode Luma block 2
    luma += 8 * lPitch;
    if (cbpFlags & 0x4)
    {
        if (!parser->dec_coeff_block(coeff, 58, dqScale, dqShift))
            return false;
        IDCT_8x8_add_sse4(coeff, luma, lPitch);
    }

    // decode Luma block 3
    if (cbpFlags & 0x8)
    {
        if (!parser->dec_coeff_block(coeff, 58, dqScale, dqShift))
            return false;
        IDCT_8x8_add_sse4(coeff, luma + 8, lPitch);
    }

    // decode Cb block
    const int cPitch = ctx->picPitch[1];
    if (cbpFlags & 0x10)
    {
        int qp = ctx->curQp + ctx->picHdr.chroma_quant_delta_cb;
        if (qp & ~63)
            return false;
        qp = g_ChromaQp[qp];

        if (!parser->dec_coeff_block(coeff, 124, g_DequantScale[qp], g_DequantShift[qp]))
            return false;
        const int cPitch = ctx->picPitch[1];
        uint8_t* dstCb = ctx->picPlane[1] + (my * cPitch + mx) * 8;
        IDCT_8x8_add_sse4(coeff, dstCb, cPitch);
    }

    // decode Cr block
    if (cbpFlags & 0x20)
    {
        int qp = ctx->curQp + ctx->picHdr.chroma_quant_delta_cr;
        if (qp & ~63)
            return false;
        qp = g_ChromaQp[qp];

        if (!parser->dec_coeff_block(coeff, 124, g_DequantScale[qp], g_DequantShift[qp]))
            return false;
        const int cPitch = ctx->picPitch[2];
        uint8_t* dstCr = ctx->picPlane[2] + (my * cPitch + mx) * 8;
        IDCT_8x8_add_sse4(coeff, dstCr, cPitch);
    }

    return true;
}

// 帧内预测宏块解码
void dec_macroblock_I8x8_AEC(FrmDecContext* ctx, int mx, int my)
{
    AvsAecParser* parser = ctx->aecParser;
    MbContext* leftMb = &ctx->leftMb;
    MbContext* topMb = ctx->topLine + mx;
    MbContext* curMb = ctx->curLine + mx;

    // 解析 4 个 8x8 亮度块的帧内预测模式
    int8_t lumaPred[4];
    // block 0
    int8_t predAB = get_intra_pred_mode(leftMb->ipMode[0], topMb->ipMode[0]);  // 根据左边和上边的宏块预测
    if (parser->dec_decision(22) != 0)     // intra_luma_pred_mode == 0
    {
        lumaPred[0] = predAB;
    }
    else
    {
        int8_t ipred = 0;
        if (parser->dec_decision(23) != 0)
            ipred = 1;
        else if (parser->dec_decision(24) != 0)
            ipred = 2;
        else if (parser->dec_decision(25) != 0)
            ipred = 3;
        lumaPred[0] = ipred + (ipred >= predAB);
    }

    // block 1
    predAB = get_intra_pred_mode(lumaPred[0], topMb->ipMode[1]);
    if (parser->dec_decision(22) != 0)     // intra_luma_pred_mode == 0
    {
        lumaPred[1] = predAB;
    }
    else
    {
        int8_t ipred = 0;
        if (parser->dec_decision(23) != 0)
            ipred = 1;
        else if (parser->dec_decision(24) != 0)
            ipred = 2;
        else if (parser->dec_decision(25) != 0)
            ipred = 3;
        lumaPred[1] = ipred + (ipred >= predAB);
    }

    // block 2
    predAB = get_intra_pred_mode(leftMb->ipMode[1], lumaPred[0]);
    if (parser->dec_decision(22) != 0)     // intra_luma_pred_mode == 0
    {
        lumaPred[2] = predAB;
    }
    else
    {
        int8_t ipred = 0;
        if (parser->dec_decision(23) != 0)
            ipred = 1;
        else if (parser->dec_decision(24) != 0)
            ipred = 2;
        else if (parser->dec_decision(25) != 0)
            ipred = 3;
        lumaPred[2] = ipred + (ipred >= predAB);
    }

    // block 3
    predAB = get_intra_pred_mode(lumaPred[2], lumaPred[1]);
    if (parser->dec_decision(22) != 0)     // intra_luma_pred_mode == 0
    {
        lumaPred[3] = predAB;
    }
    else
    {
        int8_t ipred = 0;
        if (parser->dec_decision(23) != 0)
            ipred = 1;
        else if (parser->dec_decision(24) != 0)
            ipred = 2;
        else if (parser->dec_decision(25) != 0)
            ipred = 3;
        lumaPred[3] = ipred + (ipred >= predAB);
    }
    assert(lumaPred[0] >= 0 && lumaPred[0] <= 4);
    assert(lumaPred[1] >= 0 && lumaPred[1] <= 4);
    assert(lumaPred[2] >= 0 && lumaPred[2] <= 4);
    assert(lumaPred[3] >= 0 && lumaPred[3] <= 4);

    // 解析色差分量帧内预测
    int ctxInc = leftMb->cipFlag + topMb->cipFlag;
    const uint8_t chromaPred = parser->dec_intra_chroma_pred_mode(ctxInc);
    leftMb->cipFlag = curMb->cipFlag = (chromaPred != 0);

    // 解析 CBP
    const uint8_t cbpFlags = parser->dec_cbp(leftMb->cbp, topMb->cbp);
    leftMb->cbp = curMb->cbp = cbpFlags;

    // qp_delta
    if (cbpFlags && ctx->bFixedQp == 0)
    {
        if (!parse_qp_AEC(ctx))
            return;
    }
    else
    {
        ctx->prevQpDelta = 0;
    }

    AvsContext* avsCtx = ctx->avsCtx;
    NBUsable usable;
    const int lPitch = ctx->picPitch[0];
    uint8_t* luma = ctx->picPlane[0] + (my * lPitch + mx) * 16;
    int16_t* coeff = ctx->coeff;            // 存储 DCT 系数的临时内存
    int dqScale = g_DequantScale[ctx->curQp];
    int dqShift = g_DequantShift[ctx->curQp];

    // decode luma block 0
    usable.flags[0] = topMb[0].avail;
    usable.flags[1] = topMb[0].avail;
    usable.flags[2] = leftMb[0].avail;
    usable.flags[3] = leftMb[0].avail;
    (*avsCtx->pfnLumaIPred[lumaPred[0]])(luma, lPitch, usable);    // 帧内预测
    if (cbpFlags & 0x1)
    {
        if (!parser->dec_coeff_block(coeff, 58, dqScale, dqShift))
        {
            ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
            return;
        }
        IDCT_8x8_add_sse4(coeff, luma, lPitch);
    }

    // decode luma block 1
    usable.flags[0] = topMb[0].avail;
    usable.flags[1] = topMb[1].avail;
    usable.flags[2] = 1;
    usable.flags[3] = 0;
    (*avsCtx->pfnLumaIPred[lumaPred[1]])(luma + 8, lPitch, usable);
    if (cbpFlags & 0x2)
    {
        if (!parser->dec_coeff_block(coeff, 58, dqScale, dqShift))
        {
            ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
            return;
        }
        IDCT_8x8_add_sse4(coeff, luma + 8, lPitch);
    }

    // decode luma block 2
    luma += 8 * lPitch;
    usable.flags[0] = 1;
    usable.flags[1] = 1;
    usable.flags[2] = leftMb[0].avail;
    usable.flags[3] = 0;
    (*avsCtx->pfnLumaIPred[lumaPred[2]])(luma, lPitch, usable);
    if (cbpFlags & 0x4)
    {
        if (!parser->dec_coeff_block(coeff, 58, dqScale, dqShift))
        {
            ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
            return;
        }
        IDCT_8x8_add_sse4(coeff, luma, lPitch);
    }

    // decode luma block 3
    usable.flags[0] = 1;
    usable.flags[1] = 0;
    usable.flags[2] = 1;
    usable.flags[3] = 0;
    (*avsCtx->pfnLumaIPred[lumaPred[3]])(luma + 8, lPitch, usable);
    if (cbpFlags & 0x8)
    {
        if (!parser->dec_coeff_block(coeff, 58, dqScale, dqShift))
        {
            ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
            return;
        }
        IDCT_8x8_add_sse4(coeff, luma + 8, lPitch);
    }

    // decode Cb block
    const int cPitch = ctx->picPitch[1];
    uint8_t* dstCb = ctx->picPlane[1] + (my * cPitch + mx) * 8;
    usable.flags[0] = topMb[0].avail;
    usable.flags[1] = topMb[1].avail;
    usable.flags[2] = leftMb[0].avail;
    usable.flags[3] = 0;
    (*avsCtx->pfnCbCrIPred[chromaPred])(dstCb, cPitch, usable);  // 帧内预测
    if (cbpFlags & 0x10)
    {
        int qp = ctx->curQp + ctx->picHdr.chroma_quant_delta_cb;
        if (qp & ~63)
        {
            ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
            return;
        }
        qp = g_ChromaQp[qp];

        if (!parser->dec_coeff_block(coeff, 124, g_DequantScale[qp], g_DequantShift[qp]))
        {
            ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
            return;
        }

        IDCT_8x8_add_sse4(coeff, dstCb, cPitch);
    }

    // decode Cr block
    uint8_t* dstCr = ctx->picPlane[2] + (my * cPitch + mx) * 8;
    (*avsCtx->pfnCbCrIPred[chromaPred])(dstCr, cPitch, usable);      // 帧内预测
    if (cbpFlags & 0x20)
    {
        int qp = ctx->curQp + ctx->picHdr.chroma_quant_delta_cr;
        if (qp & ~63)
        {
            ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
            return;
        }
        qp = g_ChromaQp[qp];

        if (!parser->dec_coeff_block(coeff, 124, g_DequantScale[qp], g_DequantShift[qp]))
        {
            ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
            return;
        }

        IDCT_8x8_add_sse4(coeff, dstCr, cPitch);
    }

    // 当前宏块可供右侧和下一行宏块使用
    leftMb->avail = 1;
    leftMb->ipMode[0] = lumaPred[1];
    leftMb->ipMode[1] = lumaPred[3];
    curMb->avail = 1;
    curMb->ipMode[0] = lumaPred[2];
    curMb->ipMode[1] = lumaPred[3];

    // 设置环路滤波相关参数
    if (ctx->lfDisabled == 0)
    {
        curMb->leftQp = leftMb->curQp;
        curMb->topQp = topMb->curQp;
        curMb->curQp = ctx->curQp;
        curMb->lfBS = 0xAAAA;           // 8 个边界的 bs = 2
        leftMb->curQp = ctx->curQp;
    }

    // 针对 B_Direct 运动估计, 设置相关参数
    if (ctx->colMvs)
    {
        BDColMvs* colMv = ctx->colMvs + my * ctx->mbColCnt + mx;
        *(uint32_as*)(colMv->refIdxs) = 0xFFFFFFFF;
    }
}

//======================================================================================================================

// 针对 PFieldSkip == 1, 标准很奇葩
#define INC_REFIDX(mbc, i)  (mbc->skipFlag ? 0 : (mbc->refIdxs[i][0] > 0))

void dec_macroblock_P16x16_AEC(FrmDecContext* ctx, int mx, int my)
{
    MbContext* leftMb = &ctx->leftMb;
    MbContext* topMb = ctx->topLine + mx;
    MbContext* curMb = ctx->curLine + mx;
    AvsAecParser* parser = ctx->aecParser;

    // 解析参考帧索引
    int refIdx = 0;
    if (ctx->picHdr.pic_ref_flag == 0)
    {
        int ctxInc = INC_REFIDX(leftMb, 0) + 2 * INC_REFIDX(topMb, 0);
        refIdx = parser->dec_ref_idx_P(ctxInc);
        if (refIdx > ctx->maxRefIdx)   // 码流错误
        {
            ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
            return;
        }
    }

    // 解析 mv_diff
    int16_t mvDiff[2];
    parser->dec_mvd(mvDiff, ctx->mvdA[0]);
    *(uint32_t*)(ctx->mvdA[1]) = *(uint32_t*)(ctx->mvdA[0]);

    // 解析 weighting_prediction
    uint8_t wpFlag = ctx->sliceWPFlag;
    if (ctx->mbWPFlag)
        wpFlag = parser->dec_decision(190);

    // 解析 CBP
    const uint8_t cbpFlags = parser->dec_cbp(leftMb->cbp, topMb->cbp);
    leftMb->cbp = curMb->cbp = cbpFlags;

    // qp_delta
    if (cbpFlags && ctx->bFixedQp == 0)
    {
        if (!parse_qp_AEC(ctx))
            return;
    }
    else
    {
        ctx->prevQpDelta = 0;
    }

    // 运动补偿
    MC_macroblock_P16x16(ctx, mx, my, refIdx, mvDiff, wpFlag);

    // 叠加残差
    if (!dec_mb_residual_AEC(ctx, mx, my, cbpFlags))
    {
        ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
        return;
    }
}

void dec_macroblock_P16x8_AEC(FrmDecContext* ctx, int mx, int my)
{
    MbContext* leftMb = &ctx->leftMb;
    MbContext* topMb = ctx->topLine + mx;
    MbContext* curMb = ctx->curLine + mx;
    AvsAecParser* parser = ctx->aecParser;

    // 解析参考帧索引
    int8_t refIdxs[2];
    if (ctx->picHdr.pic_ref_flag == 0)
    {
        int ctxInc = INC_REFIDX(leftMb, 0) + 2 * INC_REFIDX(topMb, 0);
        refIdxs[0] = parser->dec_ref_idx_P(ctxInc);
        ctxInc = INC_REFIDX(leftMb, 1) + 2 * (refIdxs[0] > 0);
        refIdxs[1] = parser->dec_ref_idx_P(ctxInc);
        if (refIdxs[0] > ctx->maxRefIdx || refIdxs[1] > ctx->maxRefIdx)   // 码流错误
        {
            ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
            return;
        }
    }
    else
    {
        refIdxs[0] = 0;
        refIdxs[1] = 0;
    }

    // 解析 mv_diff
    int16_t mvDiff[2][2];
    parser->dec_mvd(mvDiff[0], ctx->mvdA[0]);
    parser->dec_mvd(mvDiff[1], ctx->mvdA[1]);

    // 解析 weighting_prediction
    uint8_t wpFlag = ctx->sliceWPFlag;
    if (ctx->mbWPFlag)
        wpFlag = parser->dec_decision(190);

    // 解析 CBP
    const uint8_t cbpFlags = parser->dec_cbp(leftMb->cbp, topMb->cbp);
    leftMb->cbp = curMb->cbp = cbpFlags;

    // qp_delta
    if (cbpFlags && ctx->bFixedQp == 0)
    {
        if (!parse_qp_AEC(ctx))
            return;
    }
    else
    {
        ctx->prevQpDelta = 0;
    }

    // 运动补偿
    MC_macroblock_P16x8(ctx, mx, my, refIdxs, mvDiff, wpFlag);

    // 叠加残差
    if (!dec_mb_residual_AEC(ctx, mx, my, cbpFlags))
    {
        ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
        return;
    }
}

void dec_macroblock_P8x16_AEC(FrmDecContext* ctx, int mx, int my)
{
    MbContext* leftMb = &ctx->leftMb;
    MbContext* topMb = ctx->topLine + mx;
    MbContext* curMb = ctx->curLine + mx;
    AvsAecParser* parser = ctx->aecParser;

    // 解析参考帧索引
    int8_t refIdxs[2];
    if (ctx->picHdr.pic_ref_flag == 0)
    {
        int ctxInc = INC_REFIDX(leftMb, 0) + 2 * INC_REFIDX(topMb, 0);
        refIdxs[0] = parser->dec_ref_idx_P(ctxInc);
        ctxInc = (refIdxs[0] > 0) + 2 * INC_REFIDX(topMb, 1);
        refIdxs[1] = parser->dec_ref_idx_P(ctxInc);
        if (refIdxs[0] > ctx->maxRefIdx || refIdxs[1] > ctx->maxRefIdx)   // 码流错误
        {
            ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
            return;
        }
    }
    else
    {
        refIdxs[0] = 0;
        refIdxs[1] = 0;
    }

    // 解析 mv_diff
    int16_t mvDiff[2][2];
    parser->dec_mvd(mvDiff[0], ctx->mvdA[0]);
    parser->dec_mvd(mvDiff[1], ctx->mvdA[0]);
    *(uint32_t*)(ctx->mvdA[1]) = *(uint32_t*)(ctx->mvdA[0]);

    // 解析 weighting_prediction
    uint8_t wpFlag = ctx->sliceWPFlag;
    if (ctx->mbWPFlag)
        wpFlag = parser->dec_decision(190);

    // 解析 CBP
    const uint8_t cbpFlags = parser->dec_cbp(leftMb->cbp, topMb->cbp);
    leftMb->cbp = curMb->cbp = cbpFlags;

    // qp_delta
    if (cbpFlags && ctx->bFixedQp == 0)
    {
        if (!parse_qp_AEC(ctx))
            return;
    }
    else
    {
        ctx->prevQpDelta = 0;
    }

    // 运动补偿
    MC_macroblock_P8x16(ctx, mx, my, refIdxs, mvDiff, wpFlag);

    // 叠加残差
    if (!dec_mb_residual_AEC(ctx, mx, my, cbpFlags))
    {
        ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
        return;
    }
}

void dec_macroblock_P8x8_AEC(FrmDecContext* ctx, int mx, int my)
{
    MbContext* leftMb = &ctx->leftMb;
    MbContext* topMb = ctx->topLine + mx;
    MbContext* curMb = ctx->curLine + mx;
    AvsAecParser* parser = ctx->aecParser;

    // 解析参考帧索引
    int8_t refIdxs[4];
    if (ctx->picHdr.pic_ref_flag == 0)
    {
        int ctxInc = INC_REFIDX(leftMb, 0) + 2 * INC_REFIDX(topMb, 0);
        refIdxs[0] = parser->dec_ref_idx_P(ctxInc);
        ctxInc = (refIdxs[0] > 0) + 2 * INC_REFIDX(topMb, 1);
        refIdxs[1] = parser->dec_ref_idx_P(ctxInc);
        ctxInc = INC_REFIDX(leftMb, 1) + 2 * (refIdxs[0] > 0);
        refIdxs[2] = parser->dec_ref_idx_P(ctxInc);
        ctxInc = (refIdxs[2] > 0) + 2 * (refIdxs[1] > 0);
        refIdxs[3] = parser->dec_ref_idx_P(ctxInc);

        if (refIdxs[0] > ctx->maxRefIdx || refIdxs[1] > ctx->maxRefIdx ||
            refIdxs[2] > ctx->maxRefIdx || refIdxs[3] > ctx->maxRefIdx)    // 码流错误
        {
            ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
            return;
        }
    }
    else
    {
        refIdxs[0] = 0;
        refIdxs[1] = 0;
        refIdxs[2] = 0;
        refIdxs[3] = 0;
    }

    // 解析 mv_diff
    int16_t mvDiff[4][2];
    parser->dec_mvd(mvDiff[0], ctx->mvdA[0]);
    parser->dec_mvd(mvDiff[1], ctx->mvdA[0]);
    parser->dec_mvd(mvDiff[2], ctx->mvdA[1]);
    parser->dec_mvd(mvDiff[3], ctx->mvdA[1]);

    // 解析 weighting_prediction
    uint8_t wpFlag = ctx->sliceWPFlag;
    if (ctx->mbWPFlag)
        wpFlag = parser->dec_decision(190);

    // 解析 CBP
    const uint8_t cbpFlags = parser->dec_cbp(leftMb->cbp, topMb->cbp);
    leftMb->cbp = curMb->cbp = cbpFlags;

    // qp_delta
    if (cbpFlags && ctx->bFixedQp == 0)
    {
        if (!parse_qp_AEC(ctx))
            return;
    }
    else
    {
        ctx->prevQpDelta = 0;
    }

    // 运动补偿
    MC_macroblock_P8x8(ctx, mx, my, refIdxs, mvDiff, wpFlag);

    // 叠加残差
    if (!dec_mb_residual_AEC(ctx, mx, my, cbpFlags))
    {
        ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
        return;
    }
}

#undef INC_REFIDX

//======================================================================================================================

void dec_macroblock_BSkip_AEC(FrmDecContext* ctx, int mx, int my)
{
    const int wpFlag = (ctx->sliceWPFlag && !ctx->mbWPFlag);
    MC_BDirect_16x16(ctx, mx, my, wpFlag);

    ctx->leftMb.cbp = 0;
    *(int16_t*)(ctx->leftMb.notBD8x8) = 0;

    ctx->curLine[mx].cbp = 0;
    *(int16_t*)(ctx->curLine[mx].notBD8x8) = 0;

    ctx->prevQpDelta = 0;
    *(uint64_t*)(ctx->mvdA[0]) = 0; // mv_diff_x, mv_diff_y = 0
}

void dec_macroblock_BDirect_AEC(FrmDecContext* ctx, int mx, int my)
{
    // 解析 weighting_prediction
    AvsAecParser* parser = ctx->aecParser;
    int wpFlag = ctx->sliceWPFlag;
    if (ctx->mbWPFlag)
        wpFlag = parser->dec_decision(190);

    // 解析 CBP
    MbContext* leftMb = &ctx->leftMb;
    MbContext* topMb = ctx->topLine + mx;
    MbContext* curMb = ctx->curLine + mx;
    const uint8_t cbpFlags = parser->dec_cbp(leftMb->cbp, topMb->cbp);
    leftMb->cbp = curMb->cbp = cbpFlags;

    // qp_delta
    if (cbpFlags && ctx->bFixedQp == 0)
    {
        if (!parse_qp_AEC(ctx))
            return;
    }
    else
    {
        ctx->prevQpDelta = 0;
    }

    *(int16_t*)(leftMb->notBD8x8) = 0;
    *(int16_t*)(curMb->notBD8x8) = 0;
    *(uint64_t*)(ctx->mvdA[0]) = 0;         // mv_diff_x, mv_diff_y = 0

    // B_Direct 运动预测
    MC_BDirect_16x16(ctx, mx, my, wpFlag);

    // 叠加残差
    if (!dec_mb_residual_AEC(ctx, mx, my, cbpFlags))
    {
        ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
        return;
    }
}

// 解析参考帧索引的 ctxIdxInc, 标准完全没有说清楚 !!
#define INC_REF_L0( mbc, i ) (mbc->notBD8x8[i] ? (mbc->refIdxs[i][0] > 1) : 0)
#define INC_REF_L1( mbc, i ) (mbc->notBD8x8[i] ? (mbc->refIdxs[i][0] < 0 && mbc->refIdxs[i][1] > 1) : 0)

// B_16x16 宏块解码
void dec_macroblock_B16x16_AEC(FrmDecContext* ctx, int mx, int my)
{
    MbContext* leftMb = &ctx->leftMb;
    MbContext* topMb = ctx->topLine + mx;
    MbContext* curMb = ctx->curLine + mx;
    AvsAecParser* parser = ctx->aecParser;
    const uint8_t predFlag = g_BPredFlags[ctx->mbTypeIdx][0];   // 预测方向

    // 解析参考帧索引, 标准在这里不清不楚 !!
    int refIdx = 0;
    if (ctx->picHdr.pic_ref_flag == 0)
    {
        int ctxInc = 0;
        if (predFlag & PRED_FWD)
        {
            ctxInc = INC_REF_L0(leftMb, 0);
            ctxInc += 2 * INC_REF_L0(topMb, 0);
        }
        else
        {
            ctxInc = INC_REF_L1(leftMb, 0);
            ctxInc += 2 * INC_REF_L1(topMb, 0);
        }
        refIdx = parser->dec_ref_idx_B(ctxInc);
    }

    // 解析 mv_diff
    if (predFlag & PRED_FWD)   // 当前为前向或双向预测宏块
    {
        if (leftMb->refIdxs[0][0] < 0)         // 左侧不是前向或双向预测宏块
            *(uint32_t*)(ctx->mvdA[0]) = 0;
    }
    else    // 当前为后向预测宏块
    {
        if (leftMb->refIdxs[0][0] >= 0 || leftMb->refIdxs[0][1] < 0)   // 左侧不是后向预测宏块
            *(uint32_t*)(ctx->mvdA[0]) = 0;
    }
    int16_t mvDiff[2];
    parser->dec_mvd(mvDiff, ctx->mvdA[0]);
    *(uint32_t*)(ctx->mvdA[1]) = *(uint32_t*)(ctx->mvdA[0]);

    // 解析 weighting_prediction
    int wpFlag = ctx->sliceWPFlag;
    if (ctx->mbWPFlag)
        wpFlag = parser->dec_decision(190);

    // 解析 CBP
    const uint8_t cbpFlags = parser->dec_cbp(leftMb->cbp, topMb->cbp);
    leftMb->cbp = curMb->cbp = cbpFlags;

    // qp_delta
    if (cbpFlags && ctx->bFixedQp == 0)
    {
        if (!parse_qp_AEC(ctx))
            return;
    }
    else
    {
        ctx->prevQpDelta = 0;
    }

    // 运动补偿
    MC_macroblock_B16x16(ctx, mx, my, refIdx, mvDiff, wpFlag);
    leftMb->skipFlag = 0;
    leftMb->notBD8x8[0] = leftMb->notBD8x8[1] = 1;
    curMb->skipFlag = 0;
    curMb->notBD8x8[0] = curMb->notBD8x8[1] = 1;

    // 叠加残差
    if (cbpFlags)
    {
        if (!dec_mb_residual_AEC(ctx, mx, my, cbpFlags))
        {
            ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
            return;
        }
    }
}

// B_16x8 宏块解码
void dec_macroblock_B16x8_AEC(FrmDecContext* ctx, int mx, int my)
{
    MbContext* leftMb = &ctx->leftMb;
    MbContext* topMb = ctx->topLine + mx;
    MbContext* curMb = ctx->curLine + mx;
    AvsAecParser* parser = ctx->aecParser;
    const uint8_t predFlags[2] = {g_BPredFlags[ctx->mbTypeIdx][0], g_BPredFlags[ctx->mbTypeIdx][1]};

    // 解析参考帧索引, 标准在这里不清不楚
    int8_t rawIdxs[2] = {0};
    if (ctx->picHdr.pic_ref_flag == 0)
    {
        if (predFlags[0] & PRED_FWD)
        {
            int ctxInc = INC_REF_L0(leftMb, 0);
            ctxInc += 2 * INC_REF_L0(topMb, 0);
            rawIdxs[0] = parser->dec_ref_idx_B(ctxInc);

            if (predFlags[1] & PRED_FWD)
            {
                ctxInc = INC_REF_L0(leftMb, 1);
                ctxInc += 2 * rawIdxs[0];
                rawIdxs[1] = parser->dec_ref_idx_B(ctxInc);
            }
            else
            {
                ctxInc = INC_REF_L1(leftMb, 1);
                rawIdxs[1] = parser->dec_ref_idx_B(ctxInc);
            }
        }
        else
        {
            if (predFlags[1] == PRED_BCK)
            {
                int ctxInc = INC_REF_L1(leftMb, 0);
                ctxInc += 2 * INC_REF_L1(topMb, 0);
                rawIdxs[0] = parser->dec_ref_idx_B(ctxInc);

                ctxInc = INC_REF_L1(leftMb, 1);
                ctxInc += 2 * rawIdxs[0];
                rawIdxs[1] = parser->dec_ref_idx_B(ctxInc);
            }
            else
            {
                // 根据标准应先解析前向参考索引
                int ctxInc = INC_REF_L0(leftMb, 1);
                rawIdxs[1] = parser->dec_ref_idx_B(ctxInc);

                ctxInc = INC_REF_L1(leftMb, 0);
                ctxInc += 2 * INC_REF_L1(topMb, 0);
                rawIdxs[0] = parser->dec_ref_idx_B(ctxInc);
            }
        }
    }

    // 解析 mv_diff
    int16_t mvDiff[2][2];
    int idx0 = (predFlags[0] & 1) < (predFlags[1] & 1);
    int idx1 = idx0 ^ 1;
    if (predFlags[idx0] & PRED_FWD)            // 前向或双向预测宏块
    {
        if (leftMb->refIdxs[idx0][0] < 0)      // 左侧不是前向或双向预测宏块
            *(uint32_t*)(ctx->mvdA[idx0]) = 0;
    }
    else    // 后向预测宏块
    {
        if (leftMb->refIdxs[idx0][0] >= 0 || leftMb->refIdxs[idx0][1] < 0)   // 左侧不是后向预测宏块
            *(uint32_t*)(ctx->mvdA[idx0]) = 0;
    }
    if (predFlags[idx1] & PRED_FWD)
    {
        if (leftMb->refIdxs[idx1][0] < 0)
            *(uint32_t*)(ctx->mvdA[idx1]) = 0;
    }
    else
    {
        if (leftMb->refIdxs[idx1][0] >= 0 || leftMb->refIdxs[idx1][1] < 0)
            *(uint32_t*)(ctx->mvdA[idx1]) = 0;
    }
    parser->dec_mvd(mvDiff[idx0], ctx->mvdA[idx0]);
    parser->dec_mvd(mvDiff[idx1], ctx->mvdA[idx1]);

    // 解析 weighting_prediction
    BlkDecArgv blkArg;
    if (ctx->mbWPFlag)
        blkArg.wpFlag = parser->dec_decision(190);
    else
        blkArg.wpFlag = ctx->sliceWPFlag;

    // 解析 CBP
    const uint8_t cbpFlags = parser->dec_cbp(leftMb->cbp, topMb->cbp);
    leftMb->cbp = curMb->cbp = cbpFlags;

    // qp_delta
    if (cbpFlags && ctx->bFixedQp == 0)
    {
        if (!parse_qp_AEC(ctx))
            return;
    }
    else
    {
        ctx->prevQpDelta = 0;
    }

    int8_t refIdxs[2][2];
    MvUnion curMvs[2][2];

    //------------------------------ block 0 ------------------------------
    int dir = (predFlags[0] & 1) ^ 1;                       // 运动方向索引
    refIdxs[0][dir] = (rawIdxs[0] << 1) | dir;              // 变换后的参考帧索引

    if (topMb->refIdxs[0][dir] == refIdxs[0][dir])
    {
        curMvs[0][dir] = topMb->mvs[0][dir];
    }
    else
    {
        MvInfo nbs[3];
        nbs[0].refIdx = leftMb->refIdxs[0][dir];
        nbs[0].mv = leftMb->mvs[0][dir];
        nbs[0].denDist = ctx->denDist[nbs[0].refIdx];
        nbs[1].refIdx = topMb[0].refIdxs[0][dir];
        nbs[1].mv = topMb[0].mvs[0][dir];
        nbs[1].denDist = ctx->denDist[nbs[1].refIdx];
        if (topMb[1].avail)    // BlockC 存在
        {
            nbs[2].refIdx = topMb[1].refIdxs[0][dir];
            nbs[2].mv = topMb[1].mvs[0][dir];
        }
        else    // BlockC 不存在, 使用 BlockD 代替
        {
            nbs[2].refIdx = topMb[-1].refIdxs[1][dir];
            nbs[2].mv = topMb[-1].mvs[1][dir];
        }
        nbs[2].denDist = ctx->denDist[nbs[2].refIdx];
        curMvs[0][dir] = get_mv_pred(nbs, ctx->refDist[refIdxs[0][dir]]);
    }
    curMvs[0][dir].x += mvDiff[0][0];
    curMvs[0][dir].y += mvDiff[0][1];

    // 帧间预测
    const int lPitch = ctx->picPitch[0];
    const int cPitch = ctx->picPitch[1];
    blkArg.predFlag = predFlags[0];
    blkArg.bx = mx << 6;
    blkArg.by = my << 6;
    blkArg.mbDst[0] = ctx->picPlane[0] + (my * lPitch + mx) * 16;
    blkArg.mbDst[1] = ctx->picPlane[1] + (my * cPitch + mx) * 8;
    blkArg.mbDst[2] = ctx->picPlane[2] + (my * cPitch + mx) * 8;
    MC_blockB_16x8(ctx, blkArg, refIdxs[0], curMvs[0]);

    //------------------------------ block 1 ------------------------------
    dir = (predFlags[1] & 1) ^ 1;
    refIdxs[1][dir] = (rawIdxs[1] << 1) | dir;

    // 解析运动矢量
    if (leftMb->refIdxs[1][dir] == refIdxs[1][dir])
    {
        curMvs[1][dir] = leftMb->mvs[1][dir];
    }
    else
    {
        MvInfo nbs[3];
        nbs[0].refIdx = leftMb->refIdxs[1][dir];
        nbs[0].mv = leftMb->mvs[1][dir];
        nbs[0].denDist = ctx->denDist[nbs[0].refIdx];
        nbs[1].refIdx = refIdxs[0][dir];
        nbs[1].mv = curMvs[0][dir];
        nbs[1].denDist = ctx->denDist[nbs[1].refIdx];
        nbs[2].refIdx = leftMb->refIdxs[0][dir];
        nbs[2].mv = leftMb->mvs[0][dir];
        nbs[2].denDist = ctx->denDist[nbs[2].refIdx];
        curMvs[1][dir] = get_mv_pred(nbs, ctx->refDist[refIdxs[1][dir]]);
    }
    curMvs[1][dir].x += mvDiff[1][0];
    curMvs[1][dir].y += mvDiff[1][1];

    // 帧间预测
    blkArg.predFlag = predFlags[1];
    blkArg.by += 32;
    blkArg.mbDst[0] += 8 * lPitch;
    blkArg.mbDst[1] += 4 * cPitch;
    blkArg.mbDst[2] += 4 * cPitch;
    MC_blockB_16x8(ctx, blkArg, refIdxs[1], curMvs[1]);

    // 设置环路滤波相关参数
    if (ctx->lfDisabled == 0)
    {
        curMb->leftQp = leftMb->curQp;
        curMb->topQp = topMb->curQp;
        curMb->curQp = ctx->curQp;
        leftMb->curQp = ctx->curQp;
        curMb->lfBS = calc_BS_B16x8(refIdxs, curMvs, leftMb, topMb);
    }

    // 存储参考帧索引和运动矢量
    curMb->avail = 1;
    curMb->cipFlag = 0;
    curMb->skipFlag = 0;
    curMb->notBD8x8[0] = curMb->notBD8x8[1] = 1;
    curMb->refIdxs[0][0] = refIdxs[1][0];
    curMb->refIdxs[0][1] = refIdxs[1][1];
    curMb->refIdxs[1][0] = refIdxs[1][0];
    curMb->refIdxs[1][1] = refIdxs[1][1];
    curMb->mvs[0][0] = curMvs[1][0];
    curMb->mvs[0][1] = curMvs[1][1];
    curMb->mvs[1][0] = curMvs[1][0];
    curMb->mvs[1][1] = curMvs[1][1];
    leftMb->avail = 1;
    leftMb->cipFlag = 0;
    leftMb->skipFlag = 0;
    leftMb->notBD8x8[0] = leftMb->notBD8x8[1] = 1;
    leftMb->refIdxs[0][0] = refIdxs[0][0];
    leftMb->refIdxs[0][1] = refIdxs[0][1];
    leftMb->refIdxs[1][0] = refIdxs[1][0];
    leftMb->refIdxs[1][1] = refIdxs[1][1];
    leftMb->mvs[0][0] = curMvs[0][0];
    leftMb->mvs[0][1] = curMvs[0][1];
    leftMb->mvs[1][0] = curMvs[1][0];
    leftMb->mvs[1][1] = curMvs[1][1];

    // 叠加残差
    if (!dec_mb_residual_AEC(ctx, mx, my, cbpFlags))
    {
        ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
        return;
    }
}

// B_8x16 宏块解码
void dec_macroblock_B8x16_AEC(FrmDecContext* ctx, int mx, int my)
{
    MbContext* leftMb = &ctx->leftMb;
    MbContext* topMb = ctx->topLine + mx;
    MbContext* curMb = ctx->curLine + mx;
    AvsAecParser* parser = ctx->aecParser;
    const uint8_t predFlags[2] = {g_BPredFlags[ctx->mbTypeIdx][0], g_BPredFlags[ctx->mbTypeIdx][1]};

    // 解析参考帧索引, 标准在这里不清不楚, 莫名奇妙 !!
    int8_t rawIdxs[2] = {0};
    if (ctx->picHdr.pic_ref_flag == 0)
    {
        if (predFlags[0] & PRED_FWD)
        {
            int ctxInc = INC_REF_L0(leftMb, 0);
            ctxInc += 2 * INC_REF_L0(topMb, 0);
            rawIdxs[0] = parser->dec_ref_idx_B(ctxInc);

            if (predFlags[1] & PRED_FWD)
            {
                ctxInc = 2 * INC_REF_L0(topMb, 1);
                ctxInc += rawIdxs[0];
                rawIdxs[1] = parser->dec_ref_idx_B(ctxInc);
            }
            else
            {
                ctxInc = 2 * INC_REF_L1(topMb, 1);
                rawIdxs[1] = parser->dec_ref_idx_B(ctxInc);
            }
        }
        else
        {
            if (predFlags[1] == PRED_BCK)
            {
                int ctxInc = INC_REF_L1(leftMb, 0);
                ctxInc += 2 * INC_REF_L1(topMb, 0);
                rawIdxs[0] = parser->dec_ref_idx_B(ctxInc);

                ctxInc = 2 * INC_REF_L1(topMb, 1);
                ctxInc += rawIdxs[0];
                rawIdxs[1] = parser->dec_ref_idx_B(ctxInc);
            }
            else
            {
                // 根据标准应先解析前向参考索引
                int ctxInc = 2 * INC_REF_L0(topMb, 1);
                rawIdxs[1] = parser->dec_ref_idx_B(ctxInc);

                ctxInc = INC_REF_L1(leftMb, 0);
                ctxInc += 2 * INC_REF_L1(topMb, 0);
                rawIdxs[0] = parser->dec_ref_idx_B(ctxInc);
            }
        }
    }

    // 解析 mv_diff
    int16_t mvDiff[2][2];
    if (predFlags[0] & PRED_FWD)
    {
        if (leftMb->refIdxs[0][0] < 0)         // 左侧不是前向或者双向预测块
            *(uint32_t*)(ctx->mvdA[0]) = 0;
        parser->dec_mvd(mvDiff[0], ctx->mvdA[0]);

        if (predFlags[1] == PRED_BCK)          // 第二块是后向预测块
            *(uint32_t*)(ctx->mvdA[0]) = 0;
        parser->dec_mvd(mvDiff[1], ctx->mvdA[0]);
    }
    else
    {
        if (predFlags[1] == PRED_BCK)      // 都是后向预测块
        {
            if (leftMb->refIdxs[0][0] >= 0 || leftMb->refIdxs[0][1] < 0)
                *(uint32_t*)(ctx->mvdA[0]) = 0;
            parser->dec_mvd(mvDiff[0], ctx->mvdA[0]);
            parser->dec_mvd(mvDiff[1], ctx->mvdA[0]);
        }
        else
        {
            int16_t tmpMvdA[2] = {0};
            if (leftMb->refIdxs[0][0] < 0 && leftMb->refIdxs[0][1] >= 0)   // 左侧为后向预测块
                *(uint32_t*)tmpMvdA = *(uint32_t*)(ctx->mvdA[0]);

            // 根据标准应先解析前向运动矢量
            *(uint32_t*)(ctx->mvdA[0]) = 0;
            parser->dec_mvd(mvDiff[1], ctx->mvdA[0]);  // 前向
            parser->dec_mvd(mvDiff[0], tmpMvdA);       // 后向
        }
    }
    *(uint32_t*)(ctx->mvdA[1]) = *(uint32_t*)(ctx->mvdA[0]);

    // 解析 weighting_prediction
    BlkDecArgv blkArg;
    if (ctx->mbWPFlag)
        blkArg.wpFlag = parser->dec_decision(190);
    else
        blkArg.wpFlag = ctx->sliceWPFlag;

    // 解析 CBP
    const uint8_t cbpFlags = parser->dec_cbp(leftMb->cbp, topMb->cbp);
    leftMb->cbp = curMb->cbp = cbpFlags;

    // qp_delta
    if (cbpFlags && ctx->bFixedQp == 0)
    {
        if (!parse_qp_AEC(ctx))
            return;
    }
    else
    {
        ctx->prevQpDelta = 0;
    }

    int8_t refIdxs[2][2];
    MvUnion curMvs[2][2];

    //------------------------------ block 0 ------------------------------
    int dir = (predFlags[0] & 1) ^ 1;                       // 运动方向索引
    refIdxs[0][dir] = (rawIdxs[0] << 1) | dir;              // 变换后的参考帧索引

    if (leftMb->refIdxs[0][dir] == refIdxs[0][dir])
    {
        curMvs[0][dir] = leftMb->mvs[0][dir];
    }
    else
    {
        MvInfo nbs[3];
        nbs[0].refIdx = leftMb->refIdxs[0][dir];
        nbs[0].mv = leftMb->mvs[0][dir];
        nbs[0].denDist = ctx->denDist[nbs[0].refIdx];
        nbs[1].refIdx = topMb->refIdxs[0][dir];
        nbs[1].mv = topMb->mvs[0][dir];
        nbs[1].denDist = ctx->denDist[nbs[1].refIdx];
        nbs[2].refIdx = topMb->refIdxs[1][dir];
        nbs[2].mv = topMb->mvs[1][dir];
        nbs[2].denDist = ctx->denDist[nbs[2].refIdx];
        curMvs[0][dir] = get_mv_pred(nbs, ctx->refDist[refIdxs[0][dir]]);
    }
    curMvs[0][dir].x += mvDiff[0][0];
    curMvs[0][dir].y += mvDiff[0][1];

    // 帧间预测
    blkArg.predFlag = predFlags[0];
    blkArg.bx = mx << 6;
    blkArg.by = my << 6;
    blkArg.mbDst[0] = ctx->picPlane[0] + (my * ctx->picPitch[0] + mx) * 16;
    blkArg.mbDst[1] = ctx->picPlane[1] + (my * ctx->picPitch[1] + mx) * 8;
    blkArg.mbDst[2] = ctx->picPlane[2] + (my * ctx->picPitch[1] + mx) * 8;
    MC_blockB_8x16(ctx, blkArg, refIdxs[0], curMvs[0]);

    //------------------------------ block 1 ------------------------------
    dir = (predFlags[1] & 1) ^ 1;
    refIdxs[1][dir] = (rawIdxs[1] << 1) | dir;

    // 解析运动矢量
    if (topMb[1].avail)    // BlockC 存在
    {
        if (topMb[1].refIdxs[0][dir] == refIdxs[1][dir])
        {
            curMvs[1][dir] = topMb[1].mvs[0][dir];
        }
        else
        {
            MvInfo nbs[3];
            nbs[0].refIdx = refIdxs[0][dir];
            nbs[0].mv = curMvs[0][dir];
            nbs[0].denDist = ctx->denDist[nbs[0].refIdx];
            nbs[1].refIdx = topMb->refIdxs[1][dir];
            nbs[1].mv = topMb->mvs[1][dir];
            nbs[1].denDist = ctx->denDist[nbs[1].refIdx];
            nbs[2].refIdx = topMb[1].refIdxs[0][dir];
            nbs[2].mv = topMb[1].mvs[0][dir];
            nbs[2].denDist = ctx->denDist[nbs[2].refIdx];
            curMvs[1][dir] = get_mv_pred(nbs, ctx->refDist[refIdxs[1][dir]]);
        }
    }
    else    // BlockC 不存在, 根据标准使用 BlockD 代替
    {
        if (topMb->refIdxs[0][dir] == refIdxs[1][dir])
        {
            curMvs[1][dir] = topMb->mvs[0][dir];
        }
        else
        {
            MvInfo nbs[3];
            nbs[0].refIdx = refIdxs[0][dir];
            nbs[0].mv = curMvs[0][dir];
            nbs[0].denDist = ctx->denDist[nbs[0].refIdx];
            nbs[1].refIdx = topMb->refIdxs[1][dir];
            nbs[1].mv = topMb->mvs[1][dir];
            nbs[1].denDist = ctx->denDist[nbs[1].refIdx];
            nbs[2].refIdx = topMb->refIdxs[0][dir];
            nbs[2].mv = topMb->mvs[0][dir];
            nbs[2].denDist = ctx->denDist[nbs[2].refIdx];
            curMvs[1][dir] = get_mv_pred(nbs, ctx->refDist[refIdxs[1][dir]]);
        }
    }
    curMvs[1][dir].x += mvDiff[1][0];
    curMvs[1][dir].y += mvDiff[1][1];

    // 帧间预测
    blkArg.predFlag = predFlags[1];
    blkArg.bx += 32;
    blkArg.mbDst[0] += 8;
    blkArg.mbDst[1] += 4;
    blkArg.mbDst[2] += 4;
    MC_blockB_8x16(ctx, blkArg, refIdxs[1], curMvs[1]);

    // 设置环路滤波相关参数
    if (ctx->lfDisabled == 0)
    {
        curMb->leftQp = leftMb->curQp;
        curMb->topQp = topMb->curQp;
        curMb->curQp = ctx->curQp;
        leftMb->curQp = ctx->curQp;
        curMb->lfBS = calc_BS_B8x16(refIdxs, curMvs, leftMb, topMb);
    }

    // 存储参考帧索引和运动矢量
    curMb->avail = 1;
    curMb->cipFlag = 0;
    curMb->skipFlag = 0;
    curMb->notBD8x8[0] = curMb->notBD8x8[1] = 1;
    curMb->refIdxs[0][0] = refIdxs[0][0];
    curMb->refIdxs[0][1] = refIdxs[0][1];
    curMb->refIdxs[1][0] = refIdxs[1][0];
    curMb->refIdxs[1][1] = refIdxs[1][1];
    curMb->mvs[0][0] = curMvs[0][0];
    curMb->mvs[0][1] = curMvs[0][1];
    curMb->mvs[1][0] = curMvs[1][0];
    curMb->mvs[1][1] = curMvs[1][1];
    leftMb->avail = 1;
    leftMb->cipFlag = 0;
    leftMb->skipFlag = 0;
    leftMb->notBD8x8[0] = leftMb->notBD8x8[1] = 1;
    leftMb->refIdxs[0][0] = refIdxs[1][0];
    leftMb->refIdxs[0][1] = refIdxs[1][1];
    leftMb->refIdxs[1][0] = refIdxs[1][0];
    leftMb->refIdxs[1][1] = refIdxs[1][1];
    leftMb->mvs[0][0] = curMvs[1][0];
    leftMb->mvs[0][1] = curMvs[1][1];
    leftMb->mvs[1][0] = curMvs[1][0];
    leftMb->mvs[1][1] = curMvs[1][1];

    // 叠加残差
    if (!dec_mb_residual_AEC(ctx, mx, my, cbpFlags))
    {
        ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
        return;
    }
}

// B_8x8 宏块解码
void dec_macroblock_B8x8_AEC(FrmDecContext* ctx, int mx, int my)
{
    MbContext* leftMb = &ctx->leftMb;
    MbContext* topMb = ctx->topLine + mx;
    MbContext* curMb = ctx->curLine + mx;
    AvsAecParser* parser = ctx->aecParser;

    // 解析 mb_part_type
    uint8_t partType[4];
    partType[0] = parser->dec_mb_part_type();
    partType[1] = parser->dec_mb_part_type();
    partType[2] = parser->dec_mb_part_type();
    partType[3] = parser->dec_mb_part_type();

    // 解析参考帧索引
    int8_t rawIdxs[4] = {0};
    if (ctx->picHdr.pic_ref_flag == 0)
    {
        // 这个愚蠢的标准说先解析全部前向参考, 再解析全部后向参考
        if (partType[0] & PRED_FWD)
        {
            int ctxInc = INC_REF_L0(leftMb, 0);
            ctxInc += 2 * INC_REF_L0(topMb, 0);
            rawIdxs[0] = parser->dec_ref_idx_B(ctxInc);
        }
        if (partType[1] & PRED_FWD)
        {
            int ctxInc = rawIdxs[0] + 2 * INC_REF_L0(topMb, 1);
            rawIdxs[1] = parser->dec_ref_idx_B(ctxInc);
        }
        if (partType[2] & PRED_FWD)
        {
            int ctxInc = INC_REF_L0(leftMb, 1) + 2 * rawIdxs[0];
            rawIdxs[2] = parser->dec_ref_idx_B(ctxInc);
        }
        if (partType[3] & PRED_FWD)
        {
            int ctxInc = rawIdxs[2] + 2 * rawIdxs[1];
            rawIdxs[3] = parser->dec_ref_idx_B(ctxInc);
        }
        if (partType[0] == PRED_BCK)
        {
            int ctxInc = INC_REF_L1(leftMb, 0);
            ctxInc += 2 * INC_REF_L1(topMb, 0);
            rawIdxs[0] = parser->dec_ref_idx_B(ctxInc);
        }
        if (partType[1] == PRED_BCK)
        {
            int ctxInc = (partType[0] == PRED_BCK) ? rawIdxs[0] : 0;
            ctxInc += 2 * INC_REF_L1(topMb, 1);
            rawIdxs[1] = parser->dec_ref_idx_B(ctxInc);
        }
        if (partType[2] == PRED_BCK)
        {
            int ctxInc = INC_REF_L1(leftMb, 1);
            if (partType[0] == PRED_BCK)
                ctxInc += 2 * rawIdxs[0];
            rawIdxs[2] = parser->dec_ref_idx_B(ctxInc);
        }
        if (partType[3] == PRED_BCK)
        {
            int ctxInc = (partType[2] == PRED_BCK) ? rawIdxs[2] : 0;
            if (partType[1] == PRED_BCK)
                ctxInc += 2 * rawIdxs[1];
            rawIdxs[3] = parser->dec_ref_idx_B(ctxInc);
        }
    }

    // 解析 mv_diff
    int16_t mvDiff[4][2];
    int16_t tmpMvdA[2][2];
    *(uint32_t*)tmpMvdA[0] = *(uint32_t*)ctx->mvdA[0];
    *(uint32_t*)tmpMvdA[1] = *(uint32_t*)ctx->mvdA[1];
    if (partType[0] & PRED_FWD)
    {
        if (leftMb->refIdxs[0][0] < 0)
            *(uint32_t*)ctx->mvdA[0] = 0;
        parser->dec_mvd(mvDiff[0], ctx->mvdA[0]);
    }
    if (partType[1] & PRED_FWD)
    {
        if ((partType[0] & PRED_FWD) == 0)
            *(uint32_t*)ctx->mvdA[0] = 0;
        parser->dec_mvd(mvDiff[1], ctx->mvdA[0]);
    }
    if (partType[2] & PRED_FWD)
    {
        if (leftMb->refIdxs[1][0] < 0)
            *(uint32_t*)ctx->mvdA[1] = 0;
        parser->dec_mvd(mvDiff[2], ctx->mvdA[1]);
    }
    if (partType[3] & PRED_FWD)
    {
        if ((partType[2] & PRED_FWD) == 0)
            *(uint32_t*)ctx->mvdA[1] = 0;
        parser->dec_mvd(mvDiff[3], ctx->mvdA[1]);
    }
    if (partType[0] == PRED_BCK)
    {
        if (leftMb->refIdxs[0][0] >= 0 || leftMb->refIdxs[0][1] < 0)
            *(uint32_t*)tmpMvdA[0] = 0;
        parser->dec_mvd(mvDiff[0], tmpMvdA[0]);
    }
    if (partType[1] == PRED_BCK)
    {
        if (partType[0] == PRED_BCK)
            *(uint32_t*)ctx->mvdA[0] = *(uint32_t*)tmpMvdA[0];
        else
            *(uint32_t*)ctx->mvdA[0] = 0;
        parser->dec_mvd(mvDiff[1], ctx->mvdA[0]);
    }
    if (partType[2] == PRED_BCK)
    {
        if (leftMb->refIdxs[1][0] >= 0 || leftMb->refIdxs[1][1] < 0)
            *(uint32_t*)tmpMvdA[1] = 0;
        parser->dec_mvd(mvDiff[2], tmpMvdA[1]);
    }
    if (partType[3] == PRED_BCK)
    {
        if (partType[2] == PRED_BCK)
            *(uint32_t*)ctx->mvdA[1] = *(uint32_t*)tmpMvdA[1];
        else
            *(uint32_t*)ctx->mvdA[1] = 0;
        parser->dec_mvd(mvDiff[3], ctx->mvdA[1]);
    }

    // 解析 weighting_prediction
    BlkDecArgv blkArg;
    if (ctx->mbWPFlag)
        blkArg.wpFlag = parser->dec_decision(190);
    else
        blkArg.wpFlag = ctx->sliceWPFlag;

    // 解析 CBP
    const uint8_t cbpFlags = parser->dec_cbp(leftMb->cbp, topMb->cbp);
    leftMb->cbp = curMb->cbp = cbpFlags;

    // qp_delta
    if (cbpFlags && ctx->bFixedQp == 0)
    {
        if (!parse_qp_AEC(ctx))
            return;
    }
    else
    {
        ctx->prevQpDelta = 0;
    }

    const int lPitch = ctx->picPitch[0];
    const int cPitch = ctx->picPitch[1];
    const int* distDen = ctx->denDist;
    MvInfo nbs[3];
    int8_t refIdxs[4][2];
    MvUnion curMvs[4][2];

    //------------------------------- block 0 --------------------------------
    blkArg.predFlag = partType[0];
    blkArg.bx = mx << 6;
    blkArg.by = my << 6;
    blkArg.mbDst[0] = ctx->picPlane[0] + (my * lPitch + mx) * 16;
    blkArg.mbDst[1] = ctx->picPlane[1] + (my * cPitch + mx) * 8;
    blkArg.mbDst[2] = ctx->picPlane[2] + (my * cPitch + mx) * 8;
    if (partType[0] != 0)
    {
        const int dir = (partType[0] & 1) ^ 1;          // 预测方向索引
        refIdxs[0][dir] = (rawIdxs[0] << 1) | dir;      // 变换后的参考帧索引
        nbs[0].refIdx = leftMb->refIdxs[0][dir];
        nbs[0].mv = leftMb->mvs[0][dir];
        nbs[0].denDist = distDen[nbs[0].refIdx];
        nbs[1].refIdx = topMb->refIdxs[0][dir];
        nbs[1].mv = topMb->mvs[0][dir];
        nbs[1].denDist = distDen[nbs[1].refIdx];
        nbs[2].refIdx = topMb->refIdxs[1][dir];
        nbs[2].mv = topMb->mvs[1][dir];
        nbs[2].denDist = distDen[nbs[2].refIdx];
        curMvs[0][dir] = get_mv_pred(nbs, ctx->refDist[refIdxs[0][dir]]);
        curMvs[0][dir].x += mvDiff[0][0];
        curMvs[0][dir].y += mvDiff[0][1];

        MC_blockB_8x8(ctx, blkArg, refIdxs[0], curMvs[0]);
    }
    else    // SB_Direct_8x8
    {
        MC_BDirect_8x8(ctx, blkArg, refIdxs[0], curMvs[0]);
    }

    //------------------------------- block 1 --------------------------------
    blkArg.predFlag = partType[1];
    blkArg.bx += 32;
    blkArg.mbDst[0] += 8;
    blkArg.mbDst[1] += 4;
    blkArg.mbDst[2] += 4;
    if (partType[1] != 0)
    {
        const int dir = (partType[1] & 1) ^ 1;          // 预测方向索引
        refIdxs[1][dir] = (rawIdxs[1] << 1) | dir;      // 变换后的参考帧索引
        nbs[0].refIdx = refIdxs[0][dir];
        nbs[0].mv = curMvs[0][dir];
        nbs[0].denDist = distDen[nbs[0].refIdx];
        nbs[1].refIdx = topMb->refIdxs[1][dir];
        nbs[1].mv = topMb->mvs[1][dir];
        nbs[1].denDist = distDen[nbs[1].refIdx];
        if (topMb[1].avail)    // BlockC 存在
        {
            nbs[2].refIdx = topMb[1].refIdxs[0][dir];
            nbs[2].mv = topMb[1].mvs[0][dir];
        }
        else    // BlockC 不存在, 使用 BlockD 代替
        {
            nbs[2].refIdx = topMb->refIdxs[0][dir];
            nbs[2].mv = topMb->mvs[0][dir];
        }
        nbs[2].denDist = distDen[nbs[2].refIdx];
        curMvs[1][dir] = get_mv_pred(nbs, ctx->refDist[refIdxs[1][dir]]);
        curMvs[1][dir].x += mvDiff[1][0];
        curMvs[1][dir].y += mvDiff[1][1];

        MC_blockB_8x8(ctx, blkArg, refIdxs[1], curMvs[1]);
        leftMb->notBD8x8[0] = 1;
    }
    else    // SB_Direct_8x8
    {
        MC_BDirect_8x8(ctx, blkArg, refIdxs[1], curMvs[1]);
        leftMb->notBD8x8[0] = 0;
        *(uint32_t*)(ctx->mvdA[0]) = 0;
    }

    //------------------------------- block 2 --------------------------------
    blkArg.predFlag = partType[2];
    blkArg.bx -= 32;
    blkArg.by += 32;
    blkArg.mbDst[0] += 8 * lPitch - 8;
    blkArg.mbDst[1] += 4 * cPitch - 4;
    blkArg.mbDst[2] += 4 * cPitch - 4;
    if (partType[2] != 0)
    {
        const int dir = (partType[2] & 1) ^ 1;          // 预测方向索引
        refIdxs[2][dir] = (rawIdxs[2] << 1) | dir;      // 变换后的参考帧索引
        nbs[0].refIdx = leftMb->refIdxs[1][dir];
        nbs[0].mv = leftMb->mvs[1][dir];
        nbs[0].denDist = distDen[nbs[0].refIdx];
        nbs[1].refIdx = refIdxs[0][dir];
        nbs[1].mv = curMvs[0][dir];
        nbs[1].denDist = distDen[nbs[1].refIdx];
        nbs[2].refIdx = refIdxs[1][dir];
        nbs[2].mv = curMvs[1][dir];
        nbs[2].denDist = distDen[nbs[2].refIdx];
        curMvs[2][dir] = get_mv_pred(nbs, ctx->refDist[refIdxs[2][dir]]);
        curMvs[2][dir].x += mvDiff[2][0];
        curMvs[2][dir].y += mvDiff[2][1];

        MC_blockB_8x8(ctx, blkArg, refIdxs[2], curMvs[2]);
        curMb->notBD8x8[0] = 1;
    }
    else    // SB_Direct_8x8
    {
        MC_BDirect_8x8(ctx, blkArg, refIdxs[2], curMvs[2]);
        curMb->notBD8x8[0] = 0;
    }

    //------------------------------- block 3 --------------------------------
    blkArg.predFlag = partType[3];
    blkArg.bx += 32;
    blkArg.mbDst[0] += 8;
    blkArg.mbDst[1] += 4;
    blkArg.mbDst[2] += 4;
    if (partType[3] != 0)
    {
        const int dir = (partType[3] & 1) ^ 1;          // 预测方向索引
        refIdxs[3][dir] = (rawIdxs[3] << 1) | dir;      // 变换后的参考帧索引
        nbs[0].refIdx = refIdxs[2][dir];
        nbs[0].mv = curMvs[2][dir];
        nbs[0].denDist = distDen[nbs[0].refIdx];
        nbs[1].refIdx = refIdxs[1][dir];
        nbs[1].mv = curMvs[1][dir];
        nbs[1].denDist = distDen[nbs[1].refIdx];
        nbs[2].refIdx = refIdxs[0][dir];
        nbs[2].mv = curMvs[0][dir];
        nbs[2].denDist = distDen[nbs[2].refIdx];
        curMvs[3][dir] = get_mv_pred(nbs, ctx->refDist[refIdxs[3][dir]]);
        curMvs[3][dir].x += mvDiff[3][0];
        curMvs[3][dir].y += mvDiff[3][1];

        MC_blockB_8x8(ctx, blkArg, refIdxs[3], curMvs[3]);
        leftMb->notBD8x8[1] = 1;
        curMb->notBD8x8[1] = 1;
    }
    else    // SB_Direct_8x8
    {
        MC_BDirect_8x8(ctx, blkArg, refIdxs[3], curMvs[3]);
        leftMb->notBD8x8[1] = 0;
        curMb->notBD8x8[1] = 0;
        *(uint32_t*)(ctx->mvdA[1]) = 0;
    }

    // 设置环路滤波相关参数
    if (ctx->lfDisabled == 0)
    {
        curMb->leftQp = leftMb->curQp;
        curMb->topQp = topMb->curQp;
        curMb->curQp = ctx->curQp;
        leftMb->curQp = ctx->curQp;
        curMb->lfBS = calc_BS_B8x8(refIdxs, curMvs, leftMb, topMb);
    }

    // 存储参考帧索引和运动矢量
    curMb->avail = 1;
    curMb->cipFlag = 0;
    curMb->skipFlag = 0;
    curMb->refIdxs[0][0] = refIdxs[2][0];
    curMb->refIdxs[0][1] = refIdxs[2][1];
    curMb->refIdxs[1][0] = refIdxs[3][0];
    curMb->refIdxs[1][1] = refIdxs[3][1];
    curMb->mvs[0][0] = curMvs[2][0];
    curMb->mvs[0][1] = curMvs[2][1];
    curMb->mvs[1][0] = curMvs[3][0];
    curMb->mvs[1][1] = curMvs[3][1];
    leftMb->avail = 1;
    leftMb->cipFlag = 0;
    leftMb->skipFlag = 0;
    leftMb->refIdxs[0][0] = refIdxs[1][0];
    leftMb->refIdxs[0][1] = refIdxs[1][1];
    leftMb->refIdxs[1][0] = refIdxs[3][0];
    leftMb->refIdxs[1][1] = refIdxs[3][1];
    leftMb->mvs[0][0] = curMvs[1][0];
    leftMb->mvs[0][1] = curMvs[1][1];
    leftMb->mvs[1][0] = curMvs[3][0];
    leftMb->mvs[1][1] = curMvs[3][1];

    // 叠加残差
    if (!dec_mb_residual_AEC(ctx, mx, my, cbpFlags))
    {
        ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
        return;
    }
}

}   // namespace irk_avs_dec
