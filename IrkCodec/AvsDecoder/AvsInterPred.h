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

#ifndef _AVS_INTERPRED_H_
#define _AVS_INTERPRED_H_

#include <stdint.h>

namespace irk_avs_dec {

// 解码后的一帧数据
struct DecFrame;

// 单帧解码 context
struct FrmDecContext;

// 运动矢量
union MvUnion
{
    struct { int16_t x; int16_t y; };
    int32_t i32;
};

struct MvInfo
{
    int     refIdx;         // 参考帧索引
    MvUnion mv;             // 运动矢量
    int     denDist;        // 512 / BlockDistance  
};

// 针对 B_Direct 运动估计模式, 存储参考帧的运动信息
struct BDColMvs
{
    int8_t  refIdxs[4];     // 8x8 块的参考帧索引
    MvUnion mvs[4];         // 8x8 块的运动矢量
};

// 参考帧/场
struct RefPicture
{
    DecFrame*   pframe;
    uint8_t*    plane[3];
};

// 定义长方形区域 [x1, x2) x [y1, y2)
struct Rect
{
    int     x1;
    int     y1;
    int     x2;
    int     y2;
};

// 得到运动矢量预测
// abcMVS: 周边 block 的参考帧, 运动矢量等信息
// refDist: 当前 block 参考距离
MvUnion get_mv_pred(const MvInfo abcMVS[3], int refDist);

// 同上
// 针对周边 block 至少两个参考帧索引 >= 0 的情形
MvUnion get_mv_pred2(const MvInfo abcMVS[3], int refDist);

// 16x16 亮度分量帧间预测
void luma_inter_pred_16x16(FrmDecContext*, const RefPicture*, uint8_t* dst, int dstPitch, int x, int y);

// 16x8 亮度分量帧间预测
void luma_inter_pred_16x8(FrmDecContext*, const RefPicture*, uint8_t* dst, int dstPitch, int x, int y);

// 8x16 亮度分量帧间预测
void luma_inter_pred_8x16(FrmDecContext*, const RefPicture*, uint8_t* dst, int dstPitch, int x, int y);

// 8x8 亮度分量帧间预测
void luma_inter_pred_8x8(FrmDecContext*, const RefPicture*, uint8_t* dst, int dstPitch, int x, int y);

// 8x8 色差分量帧间预测
void chroma_inter_pred_8x8(FrmDecContext*, const RefPicture*,
    uint8_t* dstCb, uint8_t* dstCr, int dstPitch, int x, int y);

// 8x4 色差分量帧间预测
void chroma_inter_pred_8x4(FrmDecContext*, const RefPicture*,
    uint8_t* dstCb, uint8_t* dstCr, int dstPitch, int x, int y);

// 4x8 色差分量帧间预测
void chroma_inter_pred_4x8(FrmDecContext*, const RefPicture*,
    uint8_t* dstCb, uint8_t* dstCr, int dstPitch, int x, int y);

// 4x4 色差分量帧间预测
void chroma_inter_pred_4x4(FrmDecContext*, const RefPicture*,
    uint8_t* dstCb, uint8_t* dstCr, int dstPitch, int x, int y);

// 加权预测, 16x16, 16x8
void weight_pred_16xN(uint8_t* dst, int pitch, int scale, int delta, int N);

// 加权预测, 8x16, 8x8, 8x4
void weight_pred_8xN(uint8_t* dst, int pitch, int scale, int delta, int N);

// 加权预测, 4x8, 4x4
void weight_pred_4xN(uint8_t* dst, int pitch, int scale, int delta, int N);

// 取平均, 16x16, 16x8
void MC_avg_16xN(const uint8_t* src, int srcPitch, uint8_t* dst, int dstPitch, int N);

// 取平均, 8x16, 8x8, 8x4
void MC_avg_8xN(const uint8_t* src, int srcPitch, uint8_t* dst, int dstPitch, int N);

// 取平均, 4x8, 4x4
void MC_avg_4xN(const uint8_t* src, int srcPitch, uint8_t* dst, int dstPitch, int N);

}   // namespace irk_avs_dec
#endif
