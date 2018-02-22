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

void dec_macroblock_I8x8(FrmDecContext* ctx, int mx, int my);
void dec_macroblock_PSkip(FrmDecContext* ctx, int mx, int my);
void dec_macroblock_BSkip(FrmDecContext* ctx, int mx, int my);
void dec_macroblock_I8x8_AEC(FrmDecContext* ctx, int mx, int my);
void dec_macroblock_BSkip_AEC(FrmDecContext* ctx, int mx, int my);

void loop_filterI_sse4(FrmDecContext* ctx, int my);
void loop_filterPB_sse4(FrmDecContext* ctx, int my);

// 重置宏块 context
static inline void reset_mbctx(MbContext* ctx)
{
    *(int16_as*)(ctx->ipMode) = -1;     // -1 表示不存在或者为帧间预测
    ctx->avail = 0;                     // 宏块不存在
    ctx->cipFlag = 0;
    ctx->skipFlag = 0;
    ctx->cbp = 0xFF;
    ctx->notBD8x8[0] = 0;
    ctx->notBD8x8[1] = 0;
    ctx->curQp = -127;                  // 宏块不存在
    ctx->lfBS = 0;
    *(int32_as*)(ctx->refIdxs) = -1;    // 不存在或为帧内预测
    ctx->mvs[0][0].i32 = 0;
    ctx->mvs[0][1].i32 = 0;
    ctx->mvs[1][0].i32 = 0;
    ctx->mvs[1][1].i32 = 0;
}

// 重置一行 MbContext, 针对 I Slice
static void reset_top_mbs_I(MbContext* ctxLine, int mbCnt)
{
    for (int i = 0; i < mbCnt; i++)
    {
        *(int16_as*)(ctxLine[i].ipMode) = -1;   // -1 表示不存在或者帧间预测
        ctxLine[i].avail = 0;                   // 表示宏块不存在
        ctxLine[i].cipFlag = 0;
        ctxLine[i].cbp = 0xFF;
        ctxLine[i].curQp = -127;                // 表示宏块不存在
    }
}

// 重置一行 MbContext, 针对 P Slice 和 B Slice
static void reset_top_mbs_PB(MbContext* ctxLine, int mbCnt)
{
    for (int i = 0; i < mbCnt; i++)
    {
        *(int16_as*)(ctxLine[i].ipMode) = -1;   // -1 表示不存在或者帧间预测
        ctxLine[i].avail = 0;                   // 表示宏块不存在
        ctxLine[i].cipFlag = 0;
        ctxLine[i].skipFlag = 0;
        ctxLine[i].cbp = 0xFF;
        *(int16_as*)(ctxLine[i].notBD8x8) = 0;
        ctxLine[i].curQp = -127;                // 表示宏块不存在
        *(int32_as*)(ctxLine[i].refIdxs) = -1;  // 表示不存在或为帧内预测
        ctxLine[i].mvs[0][0].i32 = 0;
        ctxLine[i].mvs[0][1].i32 = 0;
        ctxLine[i].mvs[1][0].i32 = 0;
        ctxLine[i].mvs[1][1].i32 = 0;
    }
}

// 填充参考帧左右边界, 方便进行运动补偿
static void padding_edge(FrmDecContext* ctx, int my)
{
    if (my < 0 || my >= ctx->mbRowCnt)
    {
        return;
    }

    // luma
    int width = ctx->picWidth;
    int pitch = ctx->picPitch[0];
    uint8_t* dstY = ctx->picPlane[0] + 16 * my * pitch;
    for (int i = 0; i < 8; i++)
    {
        *(uint32_as*)(dstY - 8) = *(uint32_as*)(dstY - 4) = dstY[0] * 0x01010101u;
        *(uint32_as*)(dstY + width) = *(uint32_as*)(dstY + width + 4) = dstY[width - 1] * 0x01010101u;
        dstY += pitch;
        *(uint32_as*)(dstY - 8) = *(uint32_as*)(dstY - 4) = dstY[0] * 0x01010101u;
        *(uint32_as*)(dstY + width) = *(uint32_as*)(dstY + width + 4) = dstY[width - 1] * 0x01010101u;
        dstY += pitch;
    }

    // chroma
    width = ctx->chromaWidth;
    pitch = ctx->picPitch[1];
    uint8_t* dstCb = ctx->picPlane[1] + 8 * my * pitch;
    uint8_t* dstCr = ctx->picPlane[2] + 8 * my * pitch;
    for (int i = 0; i < 4; i++)
    {
        *(uint32_as*)(dstCb - 8) = *(uint32_as*)(dstCb - 4) = dstCb[0] * 0x01010101u;
        *(uint32_as*)(dstCb + width) = *(uint32_as*)(dstCb + width + 4) = dstCb[width - 1] * 0x01010101u;
        dstCb += pitch;
        *(uint32_as*)(dstCb - 8) = *(uint32_as*)(dstCb - 4) = dstCb[0] * 0x01010101u;
        *(uint32_as*)(dstCb + width) = *(uint32_as*)(dstCb + width + 4) = dstCb[width - 1] * 0x01010101u;
        dstCb += pitch;
        *(uint32_as*)(dstCr - 8) = *(uint32_as*)(dstCr - 4) = dstCr[0] * 0x01010101u;
        *(uint32_as*)(dstCr + width) = *(uint32_as*)(dstCr + width + 4) = dstCr[width - 1] * 0x01010101u;
        dstCr += pitch;
        *(uint32_as*)(dstCr - 8) = *(uint32_as*)(dstCr - 4) = dstCr[0] * 0x01010101u;
        *(uint32_as*)(dstCr + width) = *(uint32_as*)(dstCr + width + 4) = dstCr[width - 1] * 0x01010101u;
        dstCr += pitch;
    }
}

//======================================================================================================================

// I 帧或者 I 场解码
void decode_slice_I(FrmDecContext* ctx, const uint8_t* data, int size)
{
    assert(ctx->curFrame);
    assert((*(uint32_t*)data & 0xFFFFFF) == 0x010000);
    int mx = 0;
    int my = data[3];
    if (my >= ctx->mbRowCnt)
    {
        if (ctx->frameCoding == 0)     // 场编码视频
            my -= ctx->mbRowCnt;

        if (my >= ctx->mbRowCnt)       // 码流错误
        {
            ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
            return;
        }
    }

    AvsBitStream& bitsm = ctx->bitsm;
    bitsm.set_buffer(data + 4, size - 4);

    // 重置边界
    ctx->topLine = ctx->topMbBuf[my & 1] + 1;
    ctx->curLine = ctx->topMbBuf[(my & 1) ^ 1] + 1;
    reset_top_mbs_I(ctx->topLine - 1, ctx->mbColCnt + 2);
    reset_top_mbs_I(ctx->curLine - 1, ctx->mbColCnt + 2);
    *(int16_as*)ctx->leftMb.ipMode = -1;     // 左边界无法做帧内预测参考
    ctx->leftMb.avail = 0;
    ctx->leftMb.curQp = -127;

    // 初始量化参数
    if (ctx->picHdr.fixed_pic_qp)
    {
        ctx->bFixedQp = 1;
        ctx->curQp = ctx->picHdr.pic_qp;
    }
    else
    {
        ctx->bFixedQp = bitsm.read1();
        ctx->curQp = bitsm.read_bits(6);
    }

    // 解码进度, 用以并行解码
    assert(ctx->curFrame->decState);
    DecodingState* decState = ctx->curFrame->decState;
    int mbHeight = 0;                   // 按帧计算的宏块高度
    int lfDelay = 0;                    // 环路滤波引起的延时
    if (ctx->avsCtx->threadCnt > 1)     // 并行解码
    {
        if (ctx->frameCoding == 0)      // 场编码视频
        {
            if (ctx->fieldIdx != 0)     // 第二场才发布解码进度        
                mbHeight = 32;
        }
        else
        {
            mbHeight = 16;
        }
        if (ctx->lfDisabled == 0)
        {
            lfDelay = 2;
        }
    }

    const int lfMy = ctx->lfDisabled ? INT32_MAX : my + 1;  // 环路滤波开始宏块行
    ctx->errCode = 0;
    ctx->mbTypeIdx = 0;         // I Macroblock with CBP

    // 宏块逐一解码
    while (1)
    {
        // 宏块解码
        dec_macroblock_I8x8(ctx, mx, my);

        // 检测到解码错误后立即结束当前 slice 的解码
        if (ctx->errCode)
            return;

        if (++mx == ctx->mbColCnt)  // 到达当前宏块行末尾
        {
            if (my >= lfMy)
            {
                loop_filterI_sse4(ctx, my - 1);     // 环路滤波
                padding_edge(ctx, my - 2);          // 填充边界方便后续帧间预测
            }
            else
            {
                padding_edge(ctx, my);      // 填充边界方便后续帧间预测
            }

            mx = 0;
            my++;
            if (my >= ctx->mbRowCnt || bitsm.is_end_of_slice())    // 查看是否已到 slice 末尾
                break;

            ctx->topLine = ctx->curLine;
            ctx->curLine = ctx->topMbBuf[(my & 1) ^ 1] + 1;

            // 重置左边界
            *(int16_as*)ctx->leftMb.ipMode = -1;     // 左边界无法做帧内预测参考
            ctx->leftMb.avail = 0;
            ctx->leftMb.curQp = -127;

            // 更新解码进度
            if (mbHeight > 0 && my > lfDelay)
            {
                decState->update_state((my - lfDelay) * mbHeight);
            }
        }
    }

    if (my >= lfMy)
    {
        loop_filterI_sse4(ctx, my - 1);     // 最后一行环路滤波
        padding_edge(ctx, my - 2);          // 填充边界方便后续帧间预测
        padding_edge(ctx, my - 1);
    }

    // 更新解码进度
    if (mbHeight > 0)
    {
        decState->update_state(my * mbHeight);
    }
}

// P 帧或者 P 场解码
void decode_slice_P(FrmDecContext* ctx, const uint8_t* data, int size)
{
    assert(ctx->curFrame);
    assert((*(uint32_t*)data & 0xFFFFFF) == 0x010000);
    int mx = 0;
    int my = data[3];
    if (my >= ctx->mbRowCnt)
    {
        if (ctx->frameCoding == 0)      // 场编码视频
            my -= ctx->mbRowCnt;

        if (my >= ctx->mbRowCnt)        // 码流错误
        {
            ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
            return;
        }
    }

    AvsBitStream& bitsm = ctx->bitsm;
    bitsm.set_buffer(data + 4, size - 4);

    // 重置边界
    ctx->topLine = ctx->topMbBuf[my & 1] + 1;
    ctx->curLine = ctx->topMbBuf[(my & 1) ^ 1] + 1;
    reset_top_mbs_PB(ctx->topLine - 1, ctx->mbColCnt + 2);
    reset_top_mbs_PB(ctx->curLine - 1, ctx->mbColCnt + 2);
    reset_mbctx(&ctx->leftMb);

    // 初始量化参数
    if (ctx->picHdr.fixed_pic_qp)
    {
        ctx->bFixedQp = 1;
        ctx->curQp = ctx->picHdr.pic_qp;
    }
    else
    {
        ctx->bFixedQp = bitsm.read1();
        ctx->curQp = bitsm.read_bits(6);
    }

    // 解析加权预测相关参数
    ctx->sliceWPFlag = bitsm.read1();   // slice_weight_flag
    ctx->mbWPFlag = 0;
    if (ctx->sliceWPFlag)
    {
        int numRef = 1;
        if (ctx->picHdr.pic_type == PIC_TYPE_P)
        {
            numRef = 4 - ctx->frameCoding * 2;
        }
        for (int k = 0; k < numRef; k++)
        {
            uint32_t temp = bitsm.read_bits(17) >> 1;
            ctx->lumaScale[k] = (uint8_t)(temp >> 8);
            ctx->lumaDelta[k] = (int8_t)(temp & 0xFF);
            temp = bitsm.read_bits(17) >> 1;
            ctx->cbcrScale[k] = (uint8_t)(temp >> 8);
            ctx->cbcrDelta[k] = (int8_t)(temp & 0xFF);
        }
        ctx->mbWPFlag = bitsm.read1();
    }

    // 解码进度, 用以并行解码
    assert(ctx->curFrame->decState);
    DecodingState* decState = ctx->curFrame->decState;
    int mbHeight = 0;                   // 按帧计算的宏块高度
    int lfDelay = 0;                    // 环路滤波引起的延时
    if (ctx->avsCtx->threadCnt > 1)     // 并行解码
    {
        if (ctx->frameCoding == 0)      // 场编码视频
        {
            if (ctx->fieldIdx != 0)     // 第二场才发布解码进度        
                mbHeight = 32;
        }
        else
        {
            mbHeight = 16;
        }
        if (ctx->lfDisabled == 0)
        {
            lfDelay = 2;
        }
    }

    const int lfMy = ctx->lfDisabled ? INT32_MAX : my + 1;  // 环路滤波开始宏块行
    const int skipModeFlag = ctx->picHdr.skip_mode_flag;
    PFN_DecodeMB* pfnDecMb = ctx->avsCtx->pfnDecMbP;
    ctx->errCode = 0;

    // 宏块逐一解码
    while (1)
    {
        if (skipModeFlag)
        {
            int skipCnt = bitsm.read_ue16();     // mb_skip_run

            // P-Skip 宏块解析
            while (skipCnt-- > 0)
            {
                dec_macroblock_PSkip(ctx, mx, my);
                *(int16_as*)(ctx->curLine[mx].ipMode) = -1;
                *(int16_as*)(ctx->leftMb.ipMode) = -1;

                if (++mx == ctx->mbColCnt)     // 到达当前宏块行末尾
                {
                    // 环路滤波
                    if (my >= lfMy)
                    {
                        loop_filterPB_sse4(ctx, my - 1);
                        padding_edge(ctx, my - 2);      // 填充边界方便后续帧间预测
                    }
                    else
                    {
                        padding_edge(ctx, my);          // 填充边界方便后续帧间预测
                    }

                    mx = 0;
                    my++;
                    if (my >= ctx->mbRowCnt)            // 是否已到图像末尾
                        break;

                    ctx->topLine = ctx->curLine;
                    ctx->curLine = ctx->topMbBuf[(my & 1) ^ 1] + 1;
                    reset_mbctx(&ctx->leftMb);     // 重置左边界

                    // 更新解码进度
                    if (mbHeight > 0 && my > lfDelay)
                    {
                        decState->update_state((my - lfDelay) * mbHeight);
                    }
                }
            }

            // 查看是否已到图像末尾
            if (my >= ctx->mbRowCnt || bitsm.is_end_of_slice())
                break;

            ctx->mbTypeIdx = bitsm.read_ue8() + 1;
        }
        else
        {
            ctx->mbTypeIdx = bitsm.read_ue8();
        }

        if (ctx->mbTypeIdx >= 5)   // I_8x8
        {
            // I 宏块解码
            dec_macroblock_I8x8(ctx, mx, my);

            *(int32_as*)(ctx->leftMb.refIdxs) = -1;
            ctx->leftMb.mvs[0][0].i32 = 0;
            ctx->leftMb.mvs[1][0].i32 = 0;
            MbContext* curMb = ctx->curLine + mx;
            *(int32_as*)(curMb->refIdxs) = -1;
            curMb->mvs[0][0].i32 = 0;
            curMb->mvs[1][0].i32 = 0;
        }
        else
        {
            // P 宏块解码
            (*pfnDecMb[ctx->mbTypeIdx])(ctx, mx, my);
            *(int16_as*)(ctx->curLine[mx].ipMode) = -1;
            *(int16_as*)(ctx->leftMb.ipMode) = -1;
        }

        // 检测到解码错误后立即结束当前 slice 的解码, 避免解码器崩溃
        if (ctx->errCode != 0)
            return;

        if (++mx == ctx->mbColCnt)         // 下一宏块到达当前宏块行末尾
        {
            // 环路滤波
            if (my >= lfMy)
            {
                loop_filterPB_sse4(ctx, my - 1);
                padding_edge(ctx, my - 2);     // 填充边界方便后续帧间预测
            }
            else
            {
                padding_edge(ctx, my);         // 填充边界方便后续帧间预测
            }

            mx = 0;
            my++;
            if (my >= ctx->mbRowCnt || bitsm.is_end_of_slice())   // 是否已到图像末尾
                break;

            ctx->topLine = ctx->curLine;
            ctx->curLine = ctx->topMbBuf[(my & 1) ^ 1] + 1;
            reset_mbctx(&ctx->leftMb);     // 重置左边界

            // 更新解码进度
            if (mbHeight > 0 && my > lfDelay)
            {
                decState->update_state((my - lfDelay) * mbHeight);
            }
        }
    }

    if (my >= lfMy)
    {
        loop_filterPB_sse4(ctx, my - 1);    // 最后一行环路滤波
        padding_edge(ctx, my - 2);          // 填充边界方便后续帧间预测
        padding_edge(ctx, my - 1);
    }

    // 更新解码进度
    if (mbHeight > 0)
    {
        decState->update_state(my * mbHeight);
    }
}

// B 帧或者 B 场解码
void decode_slice_B(FrmDecContext* ctx, const uint8_t* data, int size)
{
    assert(ctx->curFrame);
    assert((*(uint32_t*)data & 0xFFFFFF) == 0x010000);
    int mx = 0;
    int my = data[3];
    if (my >= ctx->mbRowCnt)
    {
        if (ctx->frameCoding == 0)      // 场编码视频
            my -= ctx->mbRowCnt;
        if (my >= ctx->mbRowCnt)        // 码流错误
        {
            ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
            return;
        }
    }

    AvsBitStream& bitsm = ctx->bitsm;
    bitsm.set_buffer(data + 4, size - 4);

    // 重置边界
    ctx->topLine = ctx->topMbBuf[my & 1] + 1;
    ctx->curLine = ctx->topMbBuf[(my & 1) ^ 1] + 1;
    reset_top_mbs_PB(ctx->topLine - 1, ctx->mbColCnt + 2);
    reset_top_mbs_PB(ctx->curLine - 1, ctx->mbColCnt + 2);
    reset_mbctx(&ctx->leftMb);

    // 初始量化参数
    if (ctx->picHdr.fixed_pic_qp)
    {
        ctx->bFixedQp = 1;
        ctx->curQp = ctx->picHdr.pic_qp;
    }
    else
    {
        ctx->bFixedQp = bitsm.read1();
        ctx->curQp = bitsm.read_bits(6);
    }

    ctx->sliceWPFlag = bitsm.read1();  // slice_weight_flag
    ctx->mbWPFlag = 0;
    if (ctx->sliceWPFlag)
    {
        const int numRef = 4 - ctx->frameCoding * 2;
        for (int k = 0; k < numRef; k++)
        {
            uint32_t temp = bitsm.read_bits(17) >> 1;
            ctx->lumaScale[k] = (uint8_t)(temp >> 8);
            ctx->lumaDelta[k] = (int8_t)(temp & 0xFF);
            temp = bitsm.read_bits(17) >> 1;
            ctx->cbcrScale[k] = (uint8_t)(temp >> 8);
            ctx->cbcrDelta[k] = (int8_t)(temp & 0xFF);
        }
        ctx->mbWPFlag = bitsm.read1();
    }

    const int lfMy = ctx->lfDisabled ? INT32_MAX : my + 1;  // 环路滤波开始宏块行
    const int skipModeFlag = ctx->picHdr.skip_mode_flag;
    PFN_DecodeMB* pfnDecMb = ctx->avsCtx->pfnDecMbB;
    ctx->errCode = 0;

    // 宏块逐一解码
    while (1)
    {
        if (skipModeFlag)
        {
            int skipCnt = bitsm.read_ue16();    // mb_skip_run

            // B-Skip 宏块解析
            while (skipCnt-- > 0)
            {
                dec_macroblock_BSkip(ctx, mx, my);
                *(int16_as*)(ctx->curLine[mx].ipMode) = -1;
                *(int16_as*)(ctx->leftMb.ipMode) = -1;

                if (++mx == ctx->mbColCnt)      // 到达当前宏块行末尾
                {
                    // 环路滤波
                    if (my >= lfMy)
                        loop_filterPB_sse4(ctx, my - 1);

                    mx = 0;
                    my++;
                    if (my >= ctx->mbRowCnt)    // 是否已到图像末尾
                        break;

                    ctx->topLine = ctx->curLine;
                    ctx->curLine = ctx->topMbBuf[(my & 1) ^ 1] + 1;
                    reset_mbctx(&ctx->leftMb);  // 重置左边界
                }
            }

            // 查看是否已到图像末尾
            if (my >= ctx->mbRowCnt || bitsm.is_end_of_slice())
                break;

            ctx->mbTypeIdx = bitsm.read_ue8() + 1;
        }
        else
        {
            ctx->mbTypeIdx = bitsm.read_ue8();
        }

        if (ctx->mbTypeIdx >= 24)   // I_8x8
        {
            dec_macroblock_I8x8(ctx, mx, my);

            *(int32_as*)(ctx->leftMb.refIdxs) = -1;
            ctx->leftMb.mvs[0][0].i32 = 0;
            ctx->leftMb.mvs[0][1].i32 = 0;
            ctx->leftMb.mvs[1][0].i32 = 0;
            ctx->leftMb.mvs[1][1].i32 = 0;
            MbContext* mbCtx = ctx->curLine + mx;
            *(int32_as*)mbCtx->refIdxs = -1;
            mbCtx->mvs[0][0].i32 = 0;
            mbCtx->mvs[0][1].i32 = 0;
            mbCtx->mvs[1][0].i32 = 0;
            mbCtx->mvs[1][1].i32 = 0;
        }
        else
        {
            // B 宏块解码
            (*pfnDecMb[ctx->mbTypeIdx])(ctx, mx, my);
            *(int16_as*)(ctx->curLine[mx].ipMode) = -1;
            *(int16_as*)(ctx->leftMb.ipMode) = -1;
        }

        // 检测到解码错误后立即结束当前 slice 的解码
        if (ctx->errCode != 0)
            return;

        if (++mx == ctx->mbColCnt)     // 到达当前宏块行末尾
        {
            // 环路滤波
            if (my >= lfMy)
                loop_filterPB_sse4(ctx, my - 1);

            mx = 0;
            my++;
            if (my >= ctx->mbRowCnt || bitsm.is_end_of_slice())   // 是否已到图像末尾
                break;

            ctx->topLine = ctx->curLine;
            ctx->curLine = ctx->topMbBuf[(my & 1) ^ 1] + 1;
            reset_mbctx(&ctx->leftMb);      // 重置左边界
        }
    }

    if (my >= lfMy)
    {
        loop_filterPB_sse4(ctx, my - 1);    // 最后一行环路滤波
    }
}

//======================================================================================================================
// 以下为高级熵编码

// I 帧或者 I 场解码
void decode_slice_I_AEC(FrmDecContext* ctx, const uint8_t* data, int size)
{
    assert(ctx->curFrame);
    assert((*(uint32_t*)data & 0xFFFFFF) == 0x010000);
    int mx = 0;
    int my = data[3];
    if (my >= ctx->mbRowCnt)
    {
        if (ctx->frameCoding == 0)      // 场编码视频
            my -= ctx->mbRowCnt;
        if (my >= ctx->mbRowCnt)        // 码流错误
        {
            ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
            return;
        }
    }

    // 重置边界
    ctx->topLine = ctx->topMbBuf[my & 1] + 1;
    ctx->curLine = ctx->topMbBuf[(my & 1) ^ 1] + 1;
    reset_top_mbs_I(ctx->topLine - 1, ctx->mbColCnt + 2);
    reset_top_mbs_I(ctx->curLine - 1, ctx->mbColCnt + 2);
    *(int16_as*)ctx->leftMb.ipMode = -1;    // 左边界无法做帧内预测参考
    ctx->leftMb.avail = 0;
    ctx->leftMb.cipFlag = 0;
    ctx->leftMb.cbp = 0xFF;
    ctx->leftMb.curQp = -127;

    AvsAecParser* parser = ctx->aecParser;  // 高级熵编码解析器
    parser->set_buffer(data + 4, size - 4);

    // 初始量化参数
    if (ctx->picHdr.fixed_pic_qp)
    {
        ctx->bFixedQp = 1;
        ctx->curQp = ctx->picHdr.pic_qp;
    }
    else
    {
        ctx->bFixedQp = parser->read1();
        ctx->prevQpDelta = 0;
        ctx->curQp = parser->read_bits(6);
    }

    // 解码进度, 用以并行解码
    assert(ctx->curFrame->decState);
    DecodingState* decState = ctx->curFrame->decState;
    int mbHeight = 0;                   // 按帧计算的宏块高度
    int lfDelay = 0;                    // 环路滤波引起的延时
    if (ctx->avsCtx->threadCnt > 1)     // 并行解码
    {
        if (ctx->frameCoding == 0)      // 场编码视频
        {
            if (ctx->fieldIdx != 0)     // 第二场才发布解码进度        
                mbHeight = 32;
        }
        else
        {
            mbHeight = 16;
        }
        if (ctx->lfDisabled == 0)
        {
            lfDelay = 2;
        }
    }

    // 高级熵编码器初始化
    parser->make_byte_aligned();
    parser->init();

    const int lfMy = ctx->lfDisabled ? INT32_MAX : my + 1;  // 环路滤波开始宏块行
    ctx->errCode = 0;
    ctx->mbTypeIdx = 0;         // I Macroblock with CBP

    // 宏块逐一解码
    while (1)
    {
        // 宏块解码
        dec_macroblock_I8x8_AEC(ctx, mx, my);

        // 检测到解码错误后立即结束当前 slice 的解码
        if (ctx->errCode)
            return;

        // aec_mb_stuffing_bit
        if (parser->dec_stuffing_bit() != 0)
            break;

        if (++mx == ctx->mbColCnt)                  // 到达当前宏块行末尾
        {
            if (my >= lfMy)
            {
                loop_filterI_sse4(ctx, my - 1);     // 环路滤波
                padding_edge(ctx, my - 2);          // 填充边界方便后续帧间预测
            }
            else
            {
                padding_edge(ctx, my);              // 填充边界方便后续帧间预测
            }

            mx = 0;
            my++;
            if (my >= ctx->mbRowCnt || parser->is_end_of_slice())  // 查看是否已到 slice 末尾
                break;

            ctx->topLine = ctx->curLine;
            ctx->curLine = ctx->topMbBuf[(my & 1) ^ 1] + 1;

            // 重置左边界
            *(int16_as*)ctx->leftMb.ipMode = -1;    // 左边界无法做帧内预测参考
            ctx->leftMb.avail = 0;
            ctx->leftMb.cipFlag = 0;
            ctx->leftMb.cbp = 0xFF;
            ctx->leftMb.curQp = -127;

            // 更新解码进度
            if (mbHeight > 0 && my > lfDelay)
            {
                decState->update_state((my - lfDelay) * mbHeight);
            }
        }
    }

    if (my >= lfMy)
    {
        loop_filterI_sse4(ctx, my - 1);     // 最后一行环路滤波
        padding_edge(ctx, my - 2);          // 填充边界方便后续帧间预测
        padding_edge(ctx, my - 1);
    }
    padding_edge(ctx, my);

    // 更新解码进度
    if (mbHeight > 0)
    {
        decState->update_state(my * mbHeight);
    }
}

// P 帧或者 P 场解码
void decode_slice_P_AEC(FrmDecContext* ctx, const uint8_t* data, int size)
{
    assert(ctx->curFrame);
    assert((*(uint32_t*)data & 0xFFFFFF) == 0x010000);
    int mx = 0;
    int my = data[3];
    if (my >= ctx->mbRowCnt)
    {
        if (ctx->frameCoding == 0)      // 场编码视频
            my -= ctx->mbRowCnt;
        if (my >= ctx->mbRowCnt)        // 码流错误
        {
            ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
            return;
        }
    }

    // 重置边界
    ctx->topLine = ctx->topMbBuf[my & 1] + 1;
    ctx->curLine = ctx->topMbBuf[(my & 1) ^ 1] + 1;
    reset_top_mbs_PB(ctx->topLine - 1, ctx->mbColCnt + 2);
    reset_top_mbs_PB(ctx->curLine - 1, ctx->mbColCnt + 2);
    reset_mbctx(&ctx->leftMb);
    *(uint64_as*)(ctx->mvdA[0]) = 0;

    AvsAecParser* parser = ctx->aecParser;  // 高级熵编码解析器
    parser->set_buffer(data + 4, size - 4);

    // 初始量化参数
    if (ctx->picHdr.fixed_pic_qp)
    {
        ctx->bFixedQp = 1;
        ctx->curQp = ctx->picHdr.pic_qp;
    }
    else
    {
        ctx->bFixedQp = parser->read1();
        ctx->prevQpDelta = 0;
        ctx->curQp = parser->read_bits(6);
    }

    // 解析加权预测相关参数
    ctx->sliceWPFlag = parser->read1();     // slice_weight_flag
    ctx->mbWPFlag = 0;
    if (ctx->sliceWPFlag)
    {
        int numRef = 1;
        if (ctx->picHdr.pic_type == PIC_TYPE_P)
        {
            numRef = 4 - ctx->frameCoding * 2;
        }
        for (int k = 0; k < numRef; k++)
        {
            uint32_t temp = parser->read_bits(17) >> 1;
            ctx->lumaScale[k] = (uint8_t)(temp >> 8);
            ctx->lumaDelta[k] = (int8_t)(temp & 0xFF);
            temp = parser->read_bits(17) >> 1;
            ctx->cbcrScale[k] = (uint8_t)(temp >> 8);
            ctx->cbcrDelta[k] = (int8_t)(temp & 0xFF);
        }
        ctx->mbWPFlag = parser->read1();
    }

    // 解码进度, 用以并行解码
    assert(ctx->curFrame->decState);
    DecodingState* decState = ctx->curFrame->decState;
    int mbHeight = 0;                   // 按帧计算的宏块高度
    int lfDelay = 0;                    // 环路滤波引起的延时
    if (ctx->avsCtx->threadCnt > 1)     // 并行解码
    {
        if (ctx->frameCoding == 0)      // 场编码视频
        {
            if (ctx->fieldIdx != 0)     // 第二场才发布解码进度        
                mbHeight = 32;
        }
        else
        {
            mbHeight = 16;
        }
        if (ctx->lfDisabled == 0)
        {
            lfDelay = 2;
        }
    }

    // 高级熵编码器初始化
    parser->make_byte_aligned();
    parser->init();

    const int lfMy = ctx->lfDisabled ? INT32_MAX : my + 1;  // 环路滤波开始宏块行
    const int skipModeFlag = ctx->picHdr.skip_mode_flag;
    PFN_DecodeMB* pfnDecMb = ctx->avsCtx->pfnDecMbP_AEC;
    ctx->errCode = 0;

    // 宏块逐一解码
    while (1)
    {
        if (skipModeFlag)
        {
            // mb_skip_run
            int skipCnt = parser->dec_mb_skip_run();
            if (skipCnt > 0)
            {
                while (skipCnt-- > 0)   // P-Skip 宏块解析
                {
                    dec_macroblock_PSkip(ctx, mx, my);
                    *(int16_as*)(ctx->curLine[mx].ipMode) = -1;
                    *(int16_as*)(ctx->leftMb.ipMode) = -1;

                    if (++mx == ctx->mbColCnt)          // 到达当前宏块行末尾
                    {
                        // 环路滤波
                        if (my >= lfMy)
                        {
                            loop_filterPB_sse4(ctx, my - 1);
                            padding_edge(ctx, my - 2);  // 填充边界方便后续帧间预测
                        }
                        else
                        {
                            padding_edge(ctx, my);      // 填充边界方便后续帧间预测
                        }

                        mx = 0;
                        my++;
                        if (my >= ctx->mbRowCnt)        // 是否已到图像末尾
                            break;

                        ctx->topLine = ctx->curLine;
                        ctx->curLine = ctx->topMbBuf[(my & 1) ^ 1] + 1;
                        reset_mbctx(&ctx->leftMb);         // 重置左边界
                        *(uint64_as*)(ctx->mvdA[0]) = 0;

                        // 更新解码进度
                        if (mbHeight > 0 && my > lfDelay)
                        {
                            decState->update_state((my - lfDelay) * mbHeight);
                        }
                    }
                }

                // aec_mb_stuffing_bit
                if (parser->dec_stuffing_bit() != 0)
                    break;

                // 查看是否已到图像末尾
                if (my >= ctx->mbRowCnt || parser->is_end_of_slice())
                    break;
            }

            // 解析下一宏块类型
            ctx->mbTypeIdx = parser->dec_mb_type_P();
            if (ctx->mbTypeIdx > 4)     // 码流错误
            {
                ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
                return;
            }
            else if (ctx->mbTypeIdx == 0)
            {
                ctx->mbTypeIdx = 5;
            }
        }
        else
        {
            // 解析下一宏块类型
            ctx->mbTypeIdx = parser->dec_mb_type_P();
            if (ctx->mbTypeIdx > 5)     // 码流错误
            {
                ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
                return;
            }
            else if (ctx->mbTypeIdx == 0)
            {
                ctx->mbTypeIdx = 5;
            }
            else
            {
                ctx->mbTypeIdx -= 1;
            }
        }

        if (ctx->mbTypeIdx == 5)   // I_8x8
        {
            // I 宏块解码
            dec_macroblock_I8x8_AEC(ctx, mx, my);

            ctx->leftMb.skipFlag = 0;
            *(int32_as*)(ctx->leftMb.refIdxs) = -1;
            ctx->leftMb.mvs[0][0].i32 = 0;
            ctx->leftMb.mvs[1][0].i32 = 0;
            MbContext* curMb = ctx->curLine + mx;
            curMb->skipFlag = 0;
            *(int32_as*)(curMb->refIdxs) = -1;
            curMb->mvs[0][0].i32 = 0;
            curMb->mvs[1][0].i32 = 0;
            *(uint64_as*)(ctx->mvdA[0]) = 0;
        }
        else
        {
            // P 宏块解码
            (*pfnDecMb[ctx->mbTypeIdx])(ctx, mx, my);

            MbContext* curMb = ctx->curLine + mx;
            *(int16_as*)(curMb->ipMode) = -1;
            curMb->cipFlag = 0;
            *(int16_as*)(ctx->leftMb.ipMode) = -1;
            ctx->leftMb.cipFlag = 0;
        }

        // 检测到解码错误后立即结束当前 slice 的解码, 避免解码器崩溃
        if (ctx->errCode != 0)
            return;

        // aec_mb_stuffing_bit
        if (parser->dec_stuffing_bit() != 0)
            break;

        if (++mx == ctx->mbColCnt)          // 下一宏块到达当前宏块行末尾
        {
            // 环路滤波
            if (my >= lfMy)
            {
                loop_filterPB_sse4(ctx, my - 1);
                padding_edge(ctx, my - 2);  // 填充边界方便后续帧间预测
            }
            else
            {
                padding_edge(ctx, my);      // 填充边界方便后续帧间预测
            }

            mx = 0;
            my++;
            if (my >= ctx->mbRowCnt || parser->is_end_of_slice())   // 是否已到图像末尾
                break;

            ctx->topLine = ctx->curLine;
            ctx->curLine = ctx->topMbBuf[(my & 1) ^ 1] + 1;
            reset_mbctx(&ctx->leftMb);      // 重置左边界
            *(uint64_as*)(ctx->mvdA[0]) = 0;

            // 更新解码进度
            if (mbHeight > 0 && my > lfDelay)
            {
                decState->update_state((my - lfDelay) * mbHeight);
            }
        }
    }

    if (my >= lfMy)
    {
        loop_filterPB_sse4(ctx, my - 1);    // 最后一行环路滤波
        padding_edge(ctx, my - 2);          // 填充边界方便后续帧间预测
        padding_edge(ctx, my - 1);
    }
    padding_edge(ctx, my);

    // 更新解码进度
    if (mbHeight > 0)
    {
        decState->update_state(my * mbHeight);
    }
}

// B 帧或者 B 场解码
void decode_slice_B_AEC(FrmDecContext* ctx, const uint8_t* data, int size)
{
    assert(ctx->curFrame);
    assert((*(uint32_t*)data & 0xFFFFFF) == 0x010000);
    int mx = 0;
    int my = data[3];
    if (my >= ctx->mbRowCnt)
    {
        if (ctx->frameCoding == 0)      // 场编码视频
            my -= ctx->mbRowCnt;
        if (my >= ctx->mbRowCnt)        // 码流错误
        {
            ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
            return;
        }
    }

    // 重置边界
    ctx->topLine = ctx->topMbBuf[my & 1] + 1;
    ctx->curLine = ctx->topMbBuf[(my & 1) ^ 1] + 1;
    reset_top_mbs_PB(ctx->topLine - 1, ctx->mbColCnt + 2);
    reset_top_mbs_PB(ctx->curLine - 1, ctx->mbColCnt + 2);
    reset_mbctx(&ctx->leftMb);
    *(uint64_as*)(ctx->mvdA[0]) = 0;

    AvsAecParser* parser = ctx->aecParser;  // 高级熵编码解析器
    parser->set_buffer(data + 4, size - 4);

    // 初始量化参数
    if (ctx->picHdr.fixed_pic_qp)
    {
        ctx->bFixedQp = 1;
        ctx->curQp = ctx->picHdr.pic_qp;
    }
    else
    {
        ctx->bFixedQp = parser->read1();
        ctx->prevQpDelta = 0;
        ctx->curQp = parser->read_bits(6);
    }

    ctx->sliceWPFlag = parser->read1(); // slice_weight_flag
    ctx->mbWPFlag = 0;
    if (ctx->sliceWPFlag)
    {
        const int numRef = 4 - ctx->frameCoding * 2;
        for (int k = 0; k < numRef; k++)
        {
            uint32_t temp = parser->read_bits(17) >> 1;
            ctx->lumaScale[k] = (uint8_t)(temp >> 8);
            ctx->lumaDelta[k] = (int8_t)(temp & 0xFF);
            temp = parser->read_bits(17) >> 1;
            ctx->cbcrScale[k] = (uint8_t)(temp >> 8);
            ctx->cbcrDelta[k] = (int8_t)(temp & 0xFF);
        }
        ctx->mbWPFlag = parser->read1();
    }

    // 高级熵编码器初始化
    parser->make_byte_aligned();
    parser->init();

    const int lfMy = ctx->lfDisabled ? INT32_MAX : my + 1;  // 环路滤波开始宏块行
    const int skipModeFlag = ctx->picHdr.skip_mode_flag;
    PFN_DecodeMB* pfnDecMb = ctx->avsCtx->pfnDecMbB_AEC;
    MbContext* leftMb = &ctx->leftMb;
    ctx->errCode = 0;

    // 宏块逐一解码
    while (1)
    {
        if (skipModeFlag)
        {
            int skipCnt = parser->dec_mb_skip_run();    // mb_skip_run
            if (skipCnt > 0)
            {
                while (skipCnt-- > 0)       // B-Skip 宏块解析
                {
                    dec_macroblock_BSkip_AEC(ctx, mx, my);
                    *(int16_as*)(ctx->curLine[mx].ipMode) = -1;
                    *(int16_as*)(leftMb->ipMode) = -1;

                    if (++mx == ctx->mbColCnt)      // 到达当前宏块行末尾
                    {
                        // 环路滤波
                        if (my >= lfMy)
                            loop_filterPB_sse4(ctx, my - 1);

                        mx = 0;
                        my++;
                        if (my >= ctx->mbRowCnt)    // 是否已到图像末尾
                            break;

                        ctx->topLine = ctx->curLine;
                        ctx->curLine = ctx->topMbBuf[(my & 1) ^ 1] + 1;
                        reset_mbctx(leftMb);        // 重置左边界
                        *(uint64_as*)(ctx->mvdA[0]) = 0;
                    }
                }

                // aec_mb_stuffing_bit
                if (parser->dec_stuffing_bit() != 0)
                    break;

                // 查看是否已到图像末尾
                if (my >= ctx->mbRowCnt || parser->is_end_of_slice())
                    break;
            }

            // 解析下一宏块类型
            int ctxInc = (leftMb->avail & (leftMb->skipFlag ^ 1));
            ctxInc += (ctx->topLine[mx].avail & (ctx->topLine[mx].skipFlag ^ 1));
            ctx->mbTypeIdx = parser->dec_mb_type_B(ctxInc) + 1;
            if (ctx->mbTypeIdx > 24)    // 码流错误
            {
                ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
                return;
            }
        }
        else
        {
            // 解析下一宏块类型
            int ctxInc = (leftMb->avail & (leftMb->skipFlag ^ 1));
            ctxInc += (ctx->topLine[mx].avail & (ctx->topLine[mx].skipFlag ^ 1));
            ctx->mbTypeIdx = parser->dec_mb_type_B(ctxInc);
            if (ctx->mbTypeIdx > 24)    // 码流错误
            {
                ctx->errCode = IRK_AVS_DEC_BAD_STREAM;
                return;
            }
        }

        if (ctx->mbTypeIdx == 24)  // I_8x8
        {
            dec_macroblock_I8x8_AEC(ctx, mx, my);

            leftMb->skipFlag = 0;
            *(int16_as*)(leftMb->notBD8x8) = 0;
            *(int32_as*)leftMb->refIdxs = -1;
            leftMb->mvs[0][0].i32 = 0;
            leftMb->mvs[0][1].i32 = 0;
            leftMb->mvs[1][0].i32 = 0;
            leftMb->mvs[1][1].i32 = 0;
            MbContext* curMb = ctx->curLine + mx;
            curMb->skipFlag = 0;
            *(int32_as*)curMb->refIdxs = -1;
            *(int16_as*)(curMb->notBD8x8) = 0;
            curMb->mvs[0][0].i32 = 0;
            curMb->mvs[0][1].i32 = 0;
            curMb->mvs[1][0].i32 = 0;
            curMb->mvs[1][1].i32 = 0;
            *(uint64_as*)(ctx->mvdA[0]) = 0;
        }
        else
        {
            // B 宏块解码
            (*pfnDecMb[ctx->mbTypeIdx])(ctx, mx, my);
            *(int16_as*)(ctx->curLine[mx].ipMode) = -1;
            *(int16_as*)(leftMb->ipMode) = -1;
        }

        // 检测到解码错误后立即结束当前 slice 的解码
        if (ctx->errCode != 0)
            return;

        // aec_mb_stuffing_bit
        if (parser->dec_stuffing_bit() != 0)
            break;

        if (++mx == ctx->mbColCnt)  // 到达当前宏块行末尾
        {
            // 环路滤波
            if (my >= lfMy)
                loop_filterPB_sse4(ctx, my - 1);

            mx = 0;
            my++;
            if (my >= ctx->mbRowCnt || parser->is_end_of_slice())   // 是否已到图像末尾
                break;

            ctx->topLine = ctx->curLine;
            ctx->curLine = ctx->topMbBuf[(my & 1) ^ 1] + 1;
            reset_mbctx(leftMb);            // 重置左边界
            *(uint64_as*)(ctx->mvdA[0]) = 0;
        }
    }

    if (my >= lfMy)
    {
        loop_filterPB_sse4(ctx, my - 1);    // 最后一行环路滤波
    }
}

}   // namespace irk_avs_dec
