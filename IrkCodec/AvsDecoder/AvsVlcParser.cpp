﻿/*
* This Source Code Form is subject to the terms of the Mozilla Public License Version 2.0.
* If a copy of the MPL was not distributed with this file, 
* You can obtain one at http://mozilla.org/MPL/2.0/.

* Covered Software is provided on an "as is" basis, 
* without warranty of any kind, either expressed, implied, or statutory,
* that the Covered Software is free of defects, merchantable, 
* fit for a particular purpose or non-infringing.
 
* Copyright (c) Wei Dongliang <illigle@163.com>.
*/

#include "AvsVlcParser.h"
#include "AvsDecUtility.h"

namespace irk_avs_dec {

struct VLCMap
{
    int8_t  levelRunInc[60][3];     // level + run + next table index inc
    int8_t  refAbsLevel[26];        // RefAbsLevel
    int8_t  order;                  // exp golomb order
    int8_t  maxRun;                 // q.v. Table D.20
};

// 帧内编码亮度宏块 VLC 表格
static const struct VLCMap s_IntraVlcTab[7] = 
{
    {
        {
            {1,  1, 1}, { -1,  1, 1}, {1,  2,  1}, { -1,  2,  1}, {1,  3,  1}, { -1,  3, 1},
            {1,  4, 1}, { -1,  4, 1}, {1,  5,  1}, { -1,  5,  1}, {1,  6,  1}, { -1,  6, 1},
            {1,  7, 1}, { -1,  7, 1}, {1,  8,  1}, { -1,  8,  1}, {1,  9,  1}, { -1,  9, 1},
            {1, 10, 1}, { -1, 10, 1}, {1, 11,  1}, { -1, 11,  1}, {2,  1,  2}, { -2,  1, 2},
            {1, 12, 1}, { -1, 12, 1}, {1, 13,  1}, { -1, 13,  1}, {1, 14,  1}, { -1, 14, 1},
            {1, 15, 1}, { -1, 15, 1}, {2,  2,  2}, { -2,  2,  2}, {1, 16,  1}, { -1, 16, 1},
            {1, 17, 1}, { -1, 17, 1}, {3,  1,  3}, { -3,  1,  3}, {1, 18,  1}, { -1, 18, 1},
            {1, 19, 1}, { -1, 19, 1}, {2,  3,  2}, { -2,  3,  2}, {1, 20,  1}, { -1, 20, 1},
            {1, 21, 1}, { -1, 21, 1}, {2,  4,  2}, { -2,  4,  2}, {1, 22,  1}, { -1, 22, 1},
            {2,  5, 2}, { -2,  5, 2}, {1, 23,  1}, { -1, 23,  1},
        },
        { 4, 3, 3, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
        2,
        22,
    },
    {
        {
            {1,  1,   0}, {-1, 1,  0}, {1,    2,  0}, {-1,  2, 0}, {2,   1,  1}, {-2, 1,  1},
            {1,  3,   0}, {-1, 3,  0}, {0,    0,  0}, {1,  4,  0}, {-1,  4,  0}, {1,  5,  0},
            {-1,  5,  0}, {1,  6,  0}, { -1,  6,  0}, {3,  1,  2}, {-3,  1,  2}, {2,  2,  1},
            {-2,  2,  1}, {1,  7,  0}, { -1,  7,  0}, {1,  8,  0}, {-1,  8,  0}, {1,  9,  0},
            {-1,  9,  0}, {2,  3,  1}, { -2,  3,  1}, {4,  1,  2}, {-4,  1,  2}, {1, 10,  0},
            {-1, 10,  0}, {1, 11,  0}, { -1, 11,  0}, {2,  4,  1}, {-2,  4,  1}, {3,  2,  2},
            {-3,  2,  2}, {1, 12,  0}, { -1, 12,  0}, {2,  5,  1}, {-2,  5,  1}, {5,  1,  3},
            {-5,  1,  3}, {1, 13,  0}, { -1, 13,  0}, {2,  6,  1}, {-2,  6,  1}, {1, 14,  0},
            {-1, 14,  0}, {2,  7,  1}, { -2,  7,  1}, {2,  8,  1}, {-2,  8,  1}, {3,  3,  2},
            {-3,  3,  2}, {6,  1,  3}, { -6,  1,  3}, {1, 15,  0}, {-1, 15,  0}
        },
        { 7, 4, 4, 3, 3, 3, 3, 3, 2, 2, 2, 2, 2, 2, 2 },
        2,
        14,
    },
    {
        {
            {1,  1,  0}, {-1, 1,  0}, {2,  1,  0}, {-2, 1,  0}, {1,  2,  0}, {-1, 2,  0},
            {3,  1,  1}, {-3, 1,  1}, {0,  0,  0}, {1,  3,  0}, {-1,  3, 0}, {2,  2,  0},
            {-2, 2,  0}, {4,  1,  1}, {-4,  1, 1}, {1,  4,  0}, {-1,  4, 0}, {5,  1,  2},
            {-5, 1,  2}, {1,  5,  0}, {-1,  5, 0}, {3,  2,  1}, {-3,  2, 1}, {2,  3,  0},
            {-2, 3,  0}, {1,  6,  0}, {-1,  6, 0}, {6,  1,  2}, {-6,  1, 2}, {2,  4,  0},
            {-2, 4,  0}, {1,  7,  0}, {-1,  7, 0}, {4,  2,  1}, {-4,  2, 1}, {7,  1,  2},
            {-7, 1,  2}, {3,  3,  1}, {-3,  3, 1}, {2,  5,  0}, {-2,  5, 0}, {1,  8,  0},
            {-1, 8,  0}, {2,  6,  0}, {-2,  6, 0}, {8,  1,  3}, {-8,  1, 3}, {1,  9,  0},
            {-1, 9,  0}, {5,  2,  2}, {-5,  2, 2}, {3,  4,  1}, {-3,  4, 1}, {2,  7,  0},
            {-2, 7,  0}, {9,  1,  3}, {-9,  1, 3}, {1, 10,  0}, {-1, 10, 0}
        },
        { 10, 6, 4, 4, 3, 3, 3, 2, 2, 2 },
        2,
        9,
    },
    {
        {
            {1,  1,  0},  {-1, 1,  0}, {2,   1,  0}, {-2,  1,  0}, {3,   1,  0}, {-3, 1,  0},
            {1,  2,  0},  {-1, 2,  0}, {0,   0,  0}, {4,   1,  0}, {-4,  1,  0}, {5,  1,  1},
            {-5, 1,  1},  {2,  2,  0}, {-2,  2,  0}, {1,   3,  0}, {-1,  3,  0}, {6,  1,  1},
            {-6, 1,  1},  {3,  2,  0}, {-3,  2,  0}, {7,   1,  1}, {-7,  1,  1}, {1,  4,  0},
            {-1, 4,  0},  {8,  1,  2}, {-8,  1,  2}, {2,   3,  0}, {-2,  3,  0}, {4,  2,  0},
            {-4, 2,  0},  {1,  5,  0}, {-1,  5,  0}, {9,   1,  2}, {-9,  1,  2}, {5,  2,  1},
            {-5, 2,  1},  {2,  4,  0}, {-2,  4,  0}, {10,  1,  2}, {-10, 1,  2}, {3,  3,  0},
            {-3, 3,  0},  {1,  6,  0}, {-1,  6,  0}, {11,  1,  3}, {-11, 1,  3}, {6,  2,  1},
            {-6, 2,  1},  {1,  7,  0}, {-1,  7,  0}, {2,   5,  0}, {-2,  5,  0}, {3,  4,  0},
            {-3, 4,  0},  {12, 1,  3}, {-12, 1,  3}, {4,   3,  0}, {-4,  3,  0}
        },
        { 13, 7, 5, 4, 3, 2, 2 },
        2,
        6,
    },
    {
        {
            {1,   1,  0}, {-1, 1,  0}, {2,   1,  0}, {-2, 1,  0}, {3,   1,  0}, {-3,  1,  0},
            {0,   0,  0}, {4,  1,  0}, {-4,  1,  0}, {5,  1,  0}, {-5,  1,  0}, {6,   1,  0},
            {-6,  1,  0}, {1,  2,  0}, {-1,  2,  0}, {7,  1,  0}, {-7,  1,  0}, {8,   1,  1},
            {-8,  1,  1}, {2,  2,  0}, {-2,  2,  0}, {9,  1,  1}, {-9,  1,  1}, {10,  1,  1},
            {-10, 1,  1}, {1,  3,  0}, {-1,  3,  0}, {3,  2,  0}, {-3,  2,  0}, {11,  1,  2},
            {-11, 1,  2}, {4,  2,  0}, {-4,  2,  0}, {12, 1,  2}, {-12, 1,  2}, {13,  1,  2},
            {-13, 1,  2}, {5,  2,  0}, {-5,  2,  0}, {1,  4,  0}, {-1,  4,  0}, {2,   3,  0},
            {-2,  3,  0}, {14, 1,  2}, {-14, 1,  2}, {6,  2,  0}, {-6,  2,  0}, {15,  1,  2},
            {-15, 1,  2}, {16, 1,  2}, {-16, 1,  2}, {3,  3,  0}, {-3,  3,  0}, {1,   5,  0},
            {-1,  5,  0}, {7,  2,  0}, {-7,  2,  0}, {17, 1,  2}, {-17, 1,  2}
        },
        { 18, 8, 4, 2, 2 },
        2, 
        4,
    },
    {
        {
            {0,   0,  0}, {1,   1,  0}, {-1,   1,  0}, {2,  1,  0}, {-2,  1,  0}, {3,  1,  0},
            {-3,  1,  0}, {4,   1,  0}, {-4,   1,  0}, {5,  1,  0}, {-5,  1,  0}, {6,  1,  0},
            {-6,  1,  0}, {7,   1,  0}, {-7,   1,  0}, {8,  1,  0}, {-8,  1,  0}, {9,  1,  0},
            {-9,  1,  0}, {10,  1,  0}, {-10,  1,  0}, {1,  2,  0}, {-1,  2,  0}, {11, 1,  1},
            {-11, 1,  1}, {12,  1,  1}, {-12,  1,  1}, {13, 1,  1}, {-13, 1,  1}, {2,  2,  0},
            {-2,  2,  0}, {14,  1,  1}, {-14,  1,  1}, {15, 1,  1}, {-15, 1,  1}, {3,  2,  0},
            {-3,  2,  0}, {16,  1,  1}, {-16,  1,  1}, {1,  3,  0}, {-1,  3,  0}, {17, 1,  1},
            {-17, 1,  1}, {4,   2,  0}, {-4,   2,  0}, {18, 1,  1}, {-18, 1,  1}, {5,  2,  0},
            {-5,  2,  0}, {19,  1,  1}, {-19,  1,  1}, {20, 1,  1}, {-20, 1,  1}, {6,  2,  0},
            {-6,  2,  0}, {21,  1,  1}, {-21,  1,  1}, {2,  3,  0}, {-2,  3,  0}
        },
        { 22, 7, 3 },
        2,
        2,
    },
    {
        {
            {0,   0,  0}, {1,  1,  0}, {-1,  1,  0}, {2,  1,  0}, {-2,  1, 0}, {3,  1, 0},
            {-3,  1,  0}, {4,  1,  0}, {-4,  1,  0}, {5,  1,  0}, {-5,  1, 0}, {6,  1, 0},
            {-6,  1,  0}, {7,  1,  0}, {-7,  1,  0}, {8,  1,  0}, {-8,  1, 0}, {9,  1, 0},
            {-9,  1,  0}, {10, 1,  0}, {-10, 1,  0}, {11, 1,  0}, {-11, 1, 0}, {12, 1, 0},
            {-12, 1,  0}, {13, 1,  0}, {-13, 1,  0}, {14, 1,  0}, {-14, 1, 0}, {15, 1, 0},
            {-15, 1,  0}, {16, 1,  0}, {-16, 1,  0}, {1,  2,  0}, {-1,  2, 0}, {17, 1, 0},
            {-17, 1,  0}, {18, 1,  0}, {-18, 1,  0}, {19, 1,  0}, {-19, 1, 0}, {20, 1, 0},
            {-20, 1,  0}, {21, 1,  0}, {-21, 1,  0}, {2,  2,  0}, {-2,  2, 0}, {22, 1, 0},
            {-22, 1,  0}, {23, 1,  0}, {-23, 1,  0}, {24, 1,  0}, {-24, 1, 0}, {25, 1, 0},
            {-25, 1,  0}, {3,  2,  0}, {-3,  2,  0}, {26, 1,  0}, {-26, 1, 0}
        },
        { 27, 4 },
        2,
        1,
    }
};

// 根据当前 level 下一个 intra vlc map 索引
static const uint8_t s_IntraNextIdx[16] = { 1, 1, 2, 3, 3, 4, 4, 4, 5, 5, 5, 6, 6, 6, 6, 6 };

// 帧间编码亮度宏块 VLC 表格
static const struct VLCMap s_InterVlcTab[7] = 
{
    {
        {
            { 1,  1,  1}, {-1,  1,  1}, { 1,  2,  1}, {-1,  2,  1}, { 1,  3,  1}, {-1,  3,  1},
            { 1,  4,  1}, {-1,  4,  1}, { 1,  5,  1}, {-1,  5,  1}, { 1,  6,  1}, {-1,  6,  1},
            { 1,  7,  1}, {-1,  7,  1}, { 1,  8,  1}, {-1,  8,  1}, { 1,  9,  1}, {-1,  9,  1},
            { 1, 10,  1}, {-1, 10,  1}, { 1, 11,  1}, {-1, 11,  1}, { 1, 12,  1}, {-1, 12,  1},
            { 1, 13,  1}, {-1, 13,  1}, { 2,  1,  2}, {-2,  1,  2}, { 1, 14,  1}, {-1, 14,  1},
            { 1, 15,  1}, {-1, 15,  1}, { 1, 16,  1}, {-1, 16,  1}, { 1, 17,  1}, {-1, 17,  1},
            { 1, 18,  1}, {-1, 18,  1}, { 1, 19,  1}, {-1, 19,  1}, { 3,  1,  3}, {-3,  1,  3},
            { 1, 20,  1}, {-1, 20,  1}, { 1, 21,  1}, {-1, 21,  1}, { 2,  2,  2}, {-2,  2,  2},
            { 1, 22,  1}, {-1, 22,  1}, { 1, 23,  1}, {-1, 23,  1}, { 1, 24,  1}, {-1, 24,  1},
            { 1, 25,  1}, {-1, 25,  1}, { 1, 26,  1}, {-1, 26,  1},
        },
        { 4, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
        3,
        25
    },
    {
        {
            { 1,  1,  0}, {-1,  1,  0}, { 0,  0,  0}, { 1,  2,  0}, {-1,  2,  0}, { 1,  3,  0},
            {-1,  3,  0}, { 1,  4,  0}, {-1,  4,  0}, { 1,  5,  0}, {-1,  5,  0}, { 1,  6,  0},
            {-1,  6,  0}, { 2,  1,  1}, {-2,  1,  1}, { 1,  7,  0}, {-1,  7,  0}, { 1,  8,  0},
            {-1,  8,  0}, { 1,  9,  0}, {-1,  9,  0}, { 1, 10,  0}, {-1, 10,  0}, { 2,  2,  1},
            {-2,  2,  1}, { 1, 11,  0}, {-1, 11,  0}, { 1, 12,  0}, {-1, 12,  0}, { 3,  1,  2},
            {-3,  1,  2}, { 1, 13,  0}, {-1, 13,  0}, { 1, 14,  0}, {-1, 14,  0}, { 2,  3,  1},
            {-2,  3,  1}, { 1, 15,  0}, {-1, 15,  0}, { 2,  4,  1}, {-2,  4,  1}, { 1, 16,  0},
            {-1, 16,  0}, { 2,  5,  1}, {-2,  5,  1}, { 1, 17,  0}, {-1, 17,  0}, { 4,  1,  3},
            {-4,  1,  3}, { 2,  6,  1}, {-2,  6,  1}, { 1, 18,  0}, {-1, 18,  0}, { 1, 19,  0},
            {-1, 19,  0}, { 2,  7,  1}, {-2,  7,  1}, { 3,  2,  2}, {-3,  2,  2}
        },
        { 5, 4, 3, 3, 3, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
        2,
        18
    },
    {
        {
            { 1,  1,  0}, {-1,  1,  0}, {0,   0,  0}, { 1,  2,  0}, {-1,  2,  0}, { 2,  1,  0},
            {-2,  1,  0}, { 1,  3,  0}, {-1,  3,  0}, { 1,  4,  0}, {-1,  4,  0}, { 3,  1,  1},
            {-3,  1,  1}, { 2,  2,  0}, {-2,  2,  0}, { 1,  5,  0}, {-1,  5,  0}, { 1,  6,  0},
            {-1,  6,  0}, { 1,  7,  0}, {-1,  7,  0}, { 2,  3,  0}, {-2,  3,  0}, { 4,  1,  2},
            {-4,  1,  2}, { 1,  8,  0}, {-1,  8,  0}, { 3,  2,  1}, {-3,  2,  1}, { 2,  4,  0},
            {-2,  4,  0}, { 1,  9,  0}, {-1,  9,  0}, { 1, 10,  0}, {-1, 10,  0}, { 5,  1,  2},
            {-5,  1,  2}, { 2,  5,  0}, {-2,  5,  0}, { 1, 11,  0}, {-1, 11,  0}, { 2,  6,  0},
            {-2,  6,  0}, { 1, 12,  0}, {-1, 12,  0}, { 3,  3,  1}, {-3,  3,  1}, { 6,  1,  2},
            {-6,  1,  2}, { 4,  2,  2}, {-4,  2,  2}, { 1, 13,  0}, {-1, 13,  0}, { 2,  7,  0},
            {-2,  7,  0}, { 3,  4,  1}, {-3,  4,  1}, { 1, 14,  0}, {-1, 14,  0}
        },
        { 7, 5, 4, 4, 3, 3, 3, 2, 2, 2, 2, 2, 2, 2 },
        2,
        13
    },
    {
        {
            { 1,  1,  0}, {-1,  1,  0}, { 0,  0,  0}, { 2,  1,  0}, {-2,  1,  0}, { 1,  2,  0},
            {-1,  2,  0}, { 3,  1,  0}, {-3,  1,  0}, { 1,  3,  0}, {-1,  3,  0}, { 2,  2,  0},
            {-2,  2,  0}, { 4,  1,  1}, {-4,  1,  1}, { 1,  4,  0}, {-1,  4,  0}, { 5,  1,  1},
            {-5,  1,  1}, { 1,  5,  0}, {-1,  5,  0}, { 3,  2,  0}, {-3,  2,  0}, { 2,  3,  0},
            {-2,  3,  0}, { 1,  6,  0}, {-1,  6,  0}, { 6,  1,  1}, {-6,  1,  1}, { 2,  4,  0},
            {-2,  4,  0}, { 1,  7,  0}, {-1,  7,  0}, { 4,  2,  1}, {-4,  2,  1}, { 7,  1,  2},
            {-7,  1,  2}, { 3,  3,  0}, {-3,  3,  0}, { 1,  8,  0}, {-1,  8,  0}, { 2,  5,  0},
            {-2,  5,  0}, { 8,  1,  2}, {-8,  1,  2}, { 1,  9,  0}, {-1,  9,  0}, { 3,  4,  0},
            {-3,  4,  0}, { 2,  6,  0}, {-2,  6,  0}, { 5,  2,  1}, {-5,  2,  1}, { 1, 10,  0},
            {-1, 10,  0}, { 9,  1,  2}, {-9,  1,  2}, { 4,  3,  1}, {-4,  3,  1}
        },
        { 10, 6, 5, 4, 3, 3, 2, 2, 2, 2 },
        2,
        9
    },
    {
        {
            { 1,  1,  0}, {-1,  1,  0}, { 0,  0,  0}, { 2,  1,  0}, {-2,  1,  0}, { 3,  1,  0},
            {-3,  1,  0}, { 1,  2,  0}, {-1,  2,  0}, { 4,  1,  0}, {-4,  1,  0}, { 5,  1,  0},
            {-5,  1,  0}, { 2,  2,  0}, {-2,  2,  0}, { 1,  3,  0}, {-1,  3,  0}, { 6,  1,  0},
            {-6,  1,  0}, { 3,  2,  0}, {-3,  2,  0}, { 7,  1,  1}, {-7,  1,  1}, { 1,  4,  0},
            {-1,  4,  0}, { 8,  1,  1}, {-8,  1,  1}, { 2,  3,  0}, {-2,  3,  0}, { 4,  2,  0},
            {-4,  2,  0}, { 1,  5,  0}, {-1,  5,  0}, { 9,  1,  1}, {-9,  1,  1}, { 5,  2,  0},
            {-5,  2,  0}, { 2,  4,  0}, {-2,  4,  0}, { 1,  6,  0}, {-1,  6,  0}, {10,  1,  2},
            {-10, 1,  2}, { 3,  3,  0}, {-3,  3,  0}, {11,  1,  2}, {-11, 1,  2}, { 1,  7,  0},
            {-1,  7,  0}, { 6,  2,  0}, {-6,  2,  0}, { 3,  4,  0}, {-3,  4,  0}, { 2,  5,  0},
            {-2,  5,  0}, {12,  1,  2}, {-12, 1,  2}, { 4,  3,  0}, {-4,  3,  0}
        },
        { 13, 7, 5, 4, 3, 2, 2 },
        2,
        6
    },
    {
        {
            {  0,  0,  0}, { 1,  1,  0}, { -1,  1,  0}, { 2,  1,  0}, { -2,  1,  0}, { 3,  1,  0},
            { -3,  1,  0}, { 4,  1,  0}, { -4,  1,  0}, { 5,  1,  0}, { -5,  1,  0}, { 1,  2,  0},
            { -1,  2,  0}, { 6,  1,  0}, { -6,  1,  0}, { 7,  1,  0}, { -7,  1,  0}, { 8,  1,  0},
            { -8,  1,  0}, { 2,  2,  0}, { -2,  2,  0}, { 9,  1,  0}, { -9,  1,  0}, { 1,  3,  0},
            { -1,  3,  0}, {10,  1,  1}, {-10,  1,  1}, { 3,  2,  0}, { -3,  2,  0}, {11,  1,  1},
            {-11,  1,  1}, { 4,  2,  0}, { -4,  2,  0}, {12,  1,  1}, {-12,  1,  1}, { 1,  4,  0},
            { -1,  4,  0}, { 2,  3,  0}, { -2,  3,  0}, {13,  1,  1}, {-13,  1,  1}, { 5,  2,  0},
            { -5,  2,  0}, {14,  1,  1}, {-14,  1,  1}, { 6,  2,  0}, { -6,  2,  0}, { 1,  5,  0},
            { -1,  5,  0}, {15,  1,  1}, {-15,  1,  1}, { 3,  3,  0}, { -3,  3,  0}, {16,  1,  1},
            {-16,  1,  1}, { 2,  4,  0}, { -2,  4,  0}, { 7,  2,  0}, { -7,  2,  0}
        },
        { 17, 8, 4, 3, 2 },
        2,
        4 
    },
    {
        {
            {  0,  0,  0}, { 1,  1,  0}, { -1,  1,  0}, { 2,  1,  0}, { -2,  1,  0}, {  3,  1,  0},
            { -3,  1,  0}, { 4,  1,  0}, { -4,  1,  0}, { 5,  1,  0}, { -5,  1,  0}, {  6,  1,  0},
            { -6,  1,  0}, { 7,  1,  0}, { -7,  1,  0}, { 1,  2,  0}, { -1,  2,  0}, {  8,  1,  0},
            { -8,  1,  0}, { 9,  1,  0}, { -9,  1,  0}, {10,  1,  0}, {-10,  1,  0}, { 11,  1,  0},
            {-11,  1,  0}, {12,  1,  0}, {-12,  1,  0}, { 2,  2,  0}, { -2,  2,  0}, { 13,  1,  0},
            {-13,  1,  0}, { 1,  3,  0}, { -1,  3,  0}, {14,  1,  0}, {-14,  1,  0}, { 15,  1,  0},
            {-15,  1,  0}, { 3,  2,  0}, { -3,  2,  0}, {16,  1,  0}, {-16,  1,  0}, { 17,  1,  0},
            {-17,  1,  0}, {18,  1,  0}, {-18,  1,  0}, { 4,  2,  0}, { -4,  2,  0}, { 19,  1,  0},
            {-19,  1,  0}, {20,  1,  0}, {-20,  1,  0}, { 2,  3,  0}, { -2,  3,  0}, {  1,  4,  0},
            { -1,  4,  0}, { 5,  2,  0}, { -5,  2,  0}, {21,  1,  0}, {-21,  1,  0}
        },
        { 22, 6, 3, 2 },
        2,
        3
    }
};

// 根据当前 level 下一个 inter vlc map 索引
static const uint8_t s_InterNextIdx[16] = {1, 1, 2, 3, 4, 4, 4, 5, 5, 5, 6, 6, 6, 6, 6, 6};

// 色差分量 VLC 表格
static const struct VLCMap s_ChromaVlcTab[5] = 
{
    {
        {
            { 1,  1,  1}, {-1,  1,  1}, { 1,  2,  1}, {-1,  2,  1}, { 1,  3,  1}, {-1,  3,  1},
            { 1,  4,  1}, {-1,  4,  1}, { 1,  5,  1}, {-1,  5,  1}, { 1,  6,  1}, {-1,  6,  1},
            { 1,  7,  1}, {-1,  7,  1}, { 2,  1,  2}, {-2,  1,  2}, { 1,  8,  1}, {-1,  8,  1},
            { 1,  9,  1}, {-1,  9,  1}, { 1, 10,  1}, {-1, 10,  1}, { 1, 11,  1}, {-1, 11,  1},
            { 1, 12,  1}, {-1, 12,  1}, { 1, 13,  1}, {-1, 13,  1}, { 1, 14,  1}, {-1, 14,  1},
            { 1, 15,  1}, {-1, 15,  1}, { 3,  1,  3}, {-3,  1,  3}, { 1, 16,  1}, {-1, 16,  1},
            { 1, 17,  1}, {-1, 17,  1}, { 1, 18,  1}, {-1, 18,  1}, { 1, 19,  1}, {-1, 19,  1},
            { 1, 20,  1}, {-1, 20,  1}, { 1, 21,  1}, {-1, 21,  1}, { 1, 22,  1}, {-1, 22,  1},
            { 2,  2,  2}, {-2,  2,  2}, { 1, 23,  1}, {-1, 23,  1}, { 1, 24,  1}, {-1, 24,  1},
            { 1, 25,  1}, {-1, 25,  1}, { 4,  1,  3}, {-4,  1,  3},
        },
        { 5, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
        2,
        24
    },
    {
        {
            { 0,  0,  0}, { 1,  1,  0}, {-1,  1,  0}, { 1,  2,  0}, {-1,  2,  0}, { 2,  1,  1},
            {-2,  1,  1}, { 1,  3,  0}, {-1,  3,  0}, { 1,  4,  0}, {-1,  4,  0}, { 1,  5,  0},
            {-1,  5,  0}, { 1,  6,  0}, {-1,  6,  0}, { 3,  1,  2}, {-3,  1,  2}, { 1,  7,  0},
            {-1,  7,  0}, { 1,  8,  0}, {-1,  8,  0}, { 2,  2,  1}, {-2,  2,  1}, { 1,  9,  0},
            {-1,  9,  0}, { 1, 10,  0}, {-1, 10,  0}, { 1, 11,  0}, {-1, 11,  0}, { 4,  1,  2},
            {-4,  1,  2}, { 1, 12,  0}, {-1, 12,  0}, { 1, 13,  0}, {-1, 13,  0}, { 1, 14,  0},
            {-1, 14,  0}, { 2,  3,  1}, {-2,  3,  1}, { 1, 15,  0}, {-1, 15,  0}, { 2,  4,  1},
            {-2,  4,  1}, { 5,  1,  3}, {-5,  1,  3}, { 3,  2,  2}, {-3,  2,  2}, { 1, 16,  0},
            {-1, 16,  0}, { 1, 17,  0}, {-1, 17,  0}, { 1, 18,  0}, {-1, 18,  0}, { 2,  5,  1},
            {-2,  5,  1}, { 1, 19,  0}, {-1, 19,  0}, { 1, 20,  0}, {-1, 20,  0}
        },
        { 6, 4, 3, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
        0,
        19
    },
    {
        {
            { 1,  1,  0}, {-1,  1,  0}, { 0,  0,  0}, { 2,  1,  0}, {-2,  1,  0}, { 1,  2,  0},
            {-1,  2,  0}, { 3,  1,  1}, {-3,  1,  1}, { 1,  3,  0}, {-1,  3,  0}, { 4,  1,  1},
            {-4,  1,  1}, { 2,  2,  0}, {-2,  2,  0}, { 1,  4,  0}, {-1,  4,  0}, { 5,  1,  2},
            {-5,  1,  2}, { 1,  5,  0}, {-1,  5,  0}, { 3,  2,  1}, {-3,  2,  1}, { 2,  3,  0},
            {-2,  3,  0}, { 1,  6,  0}, {-1,  6,  0}, { 6,  1,  2}, {-6,  1,  2}, { 1,  7,  0},
            {-1,  7,  0}, { 2,  4,  0}, {-2,  4,  0}, { 7,  1,  2}, {-7,  1,  2}, { 1,  8,  0},
            {-1,  8,  0}, { 4,  2,  1}, {-4,  2,  1}, { 1,  9,  0}, {-1,  9,  0}, { 3,  3,  1},
            {-3,  3,  1}, { 2,  5,  0}, {-2,  5,  0}, { 2,  6,  0}, {-2,  6,  0}, { 8,  1,  2},
            {-8,  1,  2}, { 1, 10,  0}, {-1, 10,  0}, { 1, 11,  0}, {-1, 11,  0}, { 9,  1,  2},
            {-9,  1,  2}, { 5,  2,  2}, {-5,  2,  2}, { 3,  4,  1}, {-3,  4,  1},
        },
        { 10, 6, 4, 4, 3, 3, 2, 2, 2, 2, 2 },
        1,
        10
    },
    {
        {
            { 0,  0,  0}, { 1,  1,  0}, {-1,  1,  0}, { 2,  1,  0}, {-2,  1,  0}, { 3,  1,  0},
            {-3,  1,  0}, { 4,  1,  0}, {-4,  1,  0}, { 1,  2,  0}, {-1,  2,  0}, { 5,  1,  1},
            {-5,  1,  1}, { 2,  2,  0}, {-2,  2,  0}, { 6,  1,  1}, {-6,  1,  1}, { 1,  3,  0},
            {-1,  3,  0}, { 7,  1,  1}, {-7,  1,  1}, { 3,  2,  0}, {-3,  2,  0}, { 8,  1,  1},
            {-8,  1,  1}, { 1,  4,  0}, {-1,  4,  0}, { 2,  3,  0}, {-2,  3,  0}, { 9,  1,  1},
            {-9,  1,  1}, { 4,  2,  0}, {-4,  2,  0}, { 1,  5,  0}, {-1,  5,  0}, {10,  1,  1},
            {-10, 1,  1}, { 3,  3,  0}, {-3,  3,  0}, { 5,  2,  1}, {-5,  2,  1}, { 2,  4,  0},
            {-2,  4,  0}, {11,  1,  1}, {-11, 1,  1}, { 1,  6,  0}, {-1,  6,  0}, {12,  1,  1},
            {-12, 1,  1}, { 1,  7,  0}, {-1,  7,  0}, { 6,  2,  1}, {-6,  2,  1}, {13,  1,  1},
            {-13, 1,  1}, { 2,  5,  0}, {-2,  5,  0}, { 1,  8,  0}, {-1,  8,  0},
        },
        { 14, 7, 4, 3, 3, 2, 2, 2 },
        1,
        7
    },
    {
        {
            { 0,   0,  0}, { 1,  1,  0}, { -1,  1,  0}, { 2,  1,  0}, { -2,  1,  0}, { 3,  1,  0},
            { -3,  1,  0}, { 4,  1,  0}, { -4,  1,  0}, { 5,  1,  0}, { -5,  1,  0}, { 6,  1,  0},
            { -6,  1,  0}, { 7,  1,  0}, { -7,  1,  0}, { 8,  1,  0}, { -8,  1,  0}, { 1,  2,  0},
            { -1,  2,  0}, { 9,  1,  0}, { -9,  1,  0}, {10,  1,  0}, {-10,  1,  0}, {11,  1,  0},
            {-11,  1,  0}, { 2,  2,  0}, { -2,  2,  0}, {12,  1,  0}, {-12,  1,  0}, {13,  1,  0},
            {-13,  1,  0}, { 3,  2,  0}, { -3,  2,  0}, {14,  1,  0}, {-14,  1,  0}, { 1,  3,  0},
            { -1,  3,  0}, {15,  1,  0}, {-15,  1,  0}, { 4,  2,  0}, { -4,  2,  0}, {16,  1,  0},
            {-16,  1,  0}, {17,  1,  0}, {-17,  1,  0}, { 5,  2,  0}, { -5,  2,  0}, { 1,  4,  0},
            { -1,  4,  0}, { 2,  3,  0}, { -2,  3,  0}, {18,  1,  0}, {-18,  1,  0}, { 6,  2,  0},
            { -6,  2,  0}, {19,  1,  0}, {-19,  1,  0}, { 1,  5,  0}, { -1,  5,  0},
        },
        { 20, 7, 3, 2, 2 },
        0,
        4
    }
};

//======================================================================================================================
bool AvsVlcParser::dec_intra_coeff_block( int16_t* coeff, AvsBitStream& bitsm, int scale, uint8_t shift )
{
    int16_t levelAry[65];   // 最多 64 个系数 + EOB
    uint8_t runAry[65];
    const VLCMap* vlc = s_IntraVlcTab;
    int i = 0;
    for( ; i < 65; i++ )
    {
        int code = bitsm.read_egk( vlc->order );
        if( code >= 59 )
        {
            int run = (code - 59) >> 1;
            int msk = -(code & 1);                  // -1 or 0
            int diff = bitsm.read_egk( 1 );         // escape_level_diff
            int level = diff + (run > vlc->maxRun ? 1 : vlc->refAbsLevel[run]);
            levelAry[i] = (level ^ msk) - msk;
            runAry[i] = run + 1;
            if( level > 10 )
            {
                vlc = s_IntraVlcTab + 6;
            }
            else
            {
                const VLCMap* nextVlc = s_IntraVlcTab + s_IntraNextIdx[level];
                if( nextVlc > vlc )     // 只前进不后退
                    vlc = nextVlc;
            }
        }
        else
        {
            int level = vlc->levelRunInc[code][0];
            if( level == 0 )        // EOB
                break;
            levelAry[i] = level;
            runAry[i] = vlc->levelRunInc[code][1];
            vlc += vlc->levelRunInc[code][2];
        }
    }
    if( i >= 65 )           // 码流错误
        return false;
    
    zero_block8x8( coeff );
    int rnd = 1 << (shift - 1);
    int k = -1;   
    while( --i >= 0 )
    {
        k += runAry[i];
        if( k >= 64 )       // 码流错误
            return false;
        int idx = m_invScan[k];
        int tmp = ((levelAry[i] * m_weightQM[idx] >> 3) * scale) >> 4;
        coeff[idx] = (int16_t)((tmp + rnd) >> shift);
    }

    return true;
}

bool AvsVlcParser::dec_inter_coeff_block( int16_t* coeff, AvsBitStream& bitsm, int scale, uint8_t shift )
{
    int16_t levelAry[65];   // 最多 64 个系数 + EOB
    uint8_t runAry[65];
    const VLCMap* vlc = s_InterVlcTab;
    int i = 0;
    for( ; i < 65; i++ )
    {
        int code = bitsm.read_egk( vlc->order );
        if( code >= 59 )
        {
            int run = (code - 59) >> 1;
            int msk = -(code & 1);              // -1 or 0
            int diff = bitsm.read_egk( 0 );     // escape_level_diff
            int level = diff + (run > vlc->maxRun ? 1 : vlc->refAbsLevel[run]);
            levelAry[i] = (level ^ msk) - msk;
            runAry[i] = run + 1;
            if( level > 9 )
            {
                vlc = s_InterVlcTab + 6;
            }
            else
            {
                const VLCMap* tmp = s_InterVlcTab + s_InterNextIdx[level];
                if( tmp > vlc )     // 只前进不后退
                    vlc = tmp;
            }
        }
        else
        {
            int level = vlc->levelRunInc[code][0];
            if( level == 0 )        // EOB
                break;
            levelAry[i] = level;
            runAry[i] = vlc->levelRunInc[code][1];
            vlc += vlc->levelRunInc[code][2];
        }
    }
    if( i >= 65 )           // 码流错误
        return false;

    zero_block8x8( coeff );
    int rnd = 1 << (shift - 1);
    int k = -1;   
    while( --i >= 0 )
    {
        k += runAry[i];
        if( k >= 64 )       // 码流错误
            return false;
        int idx = m_invScan[k];
        int tmp = ((levelAry[i] * m_weightQM[idx] >> 3) * scale) >> 4;
        coeff[idx] = (int16_t)((tmp + rnd) >> shift);
    }

    return true;
}

bool AvsVlcParser::dec_chroma_coeff_block( int16_t* coeff, AvsBitStream& bitsm, int scale, uint8_t shift )
{
    // 根据当前 level 下一个 chroma vlc map 索引
    static const uint8_t s_ChromaNextIdx[8] = {1, 1, 2, 3, 3, 4, 4, 4};

    int16_t levelAry[65];   // 最多 64 个系数 + EOB
    uint8_t runAry[65];
    const VLCMap* vlc = s_ChromaVlcTab;
    int i = 0;
    for( ; i < 65; i++ )
    {
        int code = bitsm.read_egk( vlc->order );
        if( code >= 59 )
        {
            int run = (code - 59) >> 1;
            int msk = -(code & 1);          // -1 or 0
            int diff = bitsm.read_egk( 0 );      // escape_level_diff
            int level = diff + (run > vlc->maxRun ? 1 : vlc->refAbsLevel[run]);
            levelAry[i] = (level ^ msk) - msk;
            runAry[i] = run + 1;
            if( level > 4 )
            {
                vlc = s_ChromaVlcTab + 4;
            }
            else
            {
                const VLCMap* tmp = s_ChromaVlcTab + s_ChromaNextIdx[level];
                if( tmp > vlc )     // 只前进不后退
                    vlc = tmp;
            }
        }
        else
        {
            int level = vlc->levelRunInc[code][0];
            if( level == 0 )        // EOB
                break;
            levelAry[i] = level;
            runAry[i] = vlc->levelRunInc[code][1];
            vlc += vlc->levelRunInc[code][2];
        }
    }
    if( i >= 65 )           // 码流错误
        return false;

    zero_block8x8( coeff );
    int rnd = 1 << (shift - 1);
    int k = -1;   
    while( --i >= 0 )
    {
        k += runAry[i];
        if( k >= 64 )       // 码流错误
            return false;
        int idx = m_invScan[k];
        int tmp = ((levelAry[i] * m_weightQM[idx] >> 3) * scale) >> 4;
        coeff[idx] = (int16_t)((tmp + rnd) >> shift);
    }

    return true;
}

}   // namespace irk_avs_dec