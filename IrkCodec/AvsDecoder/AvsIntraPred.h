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

#ifndef _AVS_INTRAPRED_H_
#define _AVS_INTRAPRED_H_

#include <stdint.h>

namespace irk_avs_dec {

static const int8_t s_PredIntraMode[6][8] =
{
    { 2, 2, 2, 2, 2, 2 },
    { 2, 0, 0, 0, 0, 0 },
    { 2, 0, 1, 1, 1, 1 },
    { 2, 0, 1, 2, 2, 2 },
    { 2, 0, 1, 2, 3, 3 },
    { 2, 0, 1, 2, 3, 4 },
};

// 根据左边和上边的块得到 predIntraPredMode, q.v. 标准 9.4.4
inline int8_t get_intra_pred_mode(int predA, int predB)
{
    assert(predA >= -1 && predA <= 4);
    assert(predB >= -1 && predB <= 4);
    return s_PredIntraMode[predA + 1][predB + 1];
}

// 用以标记周围 8x8 块是否可用
union NBUsable
{
    // flags[0]: 上边 8x8 块是否可用
    // flags[1]: 右上 8x8 块是否可用
    // flags[2]: 左边 8x8 块是否可用
    // flags[3]: 左下 8x8 块是否可用
    int8_t   flags[4];
    uint32_t u32;
};

// 垂直预测
void intra_pred_ver(uint8_t* dst, int pitch, NBUsable);

// 水平预测
void intra_pred_hor(uint8_t* dst, int pitch, NBUsable);

// DC 预测
void intra_pred_dc(uint8_t* dst, int pitch, NBUsable);

// down-left 预测
void intra_pred_downleft(uint8_t* dst, int pitch, NBUsable);

// down-right 预测
void intra_pred_downright(uint8_t* dst, int pitch, NBUsable);

// 色差分量 plane 预测
void intra_pred_plane(uint8_t* dst, int pitch, NBUsable);

}   // namespace irk_avs_dec
#endif
