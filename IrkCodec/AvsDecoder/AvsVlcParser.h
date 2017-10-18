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

#ifndef _AVS_VLCPARSER_H_
#define _AVS_VLCPARSER_H_

#include "AvsBitstream.h"

namespace irk_avs_dec {

class AvsVlcParser : IrkNocopy
{
public:
    AvsVlcParser() : m_invScan(nullptr), m_weightQM(nullptr) {}

    // 设置逆扫描和量化矩阵
    void set_scan_quant_matrix( const uint8_t* invScan, const uint8_t* wqm )
    {
        m_invScan = invScan;
        m_weightQM = wqm;
    }

    // 解析 8x8 系数块
    bool dec_intra_coeff_block( int16_t* coeff, AvsBitStream& bitsm, int scale, uint8_t shift );
    bool dec_inter_coeff_block( int16_t* coeff, AvsBitStream& bitsm, int scale, uint8_t shift );
    bool dec_chroma_coeff_block( int16_t* coeff, AvsBitStream& bitsm, int scale, uint8_t shift );

private:
    const uint8_t*  m_invScan;
    const uint8_t*  m_weightQM;  
};

}   // namespace irk_avs_dec
#endif
