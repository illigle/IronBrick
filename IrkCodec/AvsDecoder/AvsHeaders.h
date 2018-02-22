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

#ifndef _AVS_HEADERS_H_
#define _AVS_HEADERS_H_

#include "AvsBitstream.h"

#define PIC_TYPE_I  1
#define PIC_TYPE_P  2
#define PIC_TYPE_B  3

namespace irk_avs_dec {

// sequence header
struct AvsSeqHdr
{
    uint8_t     profile;
    uint8_t     level;
    uint8_t     progressive_seq;
    uint8_t     chroma_format;
    uint16_t    width;
    uint16_t    height;
    uint8_t     sample_precision;
    uint8_t     aspect_ratio;
    uint8_t     frame_rate_code;
    uint8_t     low_delay;
    uint32_t    bitrate;
    uint32_t    bbv_buffer_size;
};

// picture header
struct AvsPicHdr
{
    uint32_t    bbv_delay;
    uint32_t    time_code;
    uint8_t     time_code_flag;
    uint8_t     pic_type;
    uint8_t     pic_distance;
    uint16_t    bbv_check_times;
    uint8_t     progressive_frame;
    uint8_t     picture_structure;
    uint8_t     top_field_first;
    uint8_t     repeat_first_field;
    uint8_t     fixed_pic_qp;
    uint8_t     pic_qp;
    uint8_t     pic_ref_flag;
    uint8_t     no_fwd_ref_flag;
    uint8_t     pb_field_enhanced_flag;
    uint8_t     skip_mode_flag;
    uint8_t     loop_filter_disable;
    uint8_t     loop_filter_param_flag;
    int8_t      alpha_c_offset;
    int8_t      beta_offset;
    uint8_t     weight_quant_flag;
    uint8_t     chroma_quant_param_disable;
    int8_t      chroma_quant_delta_cb;
    int8_t      chroma_quant_delta_cr;
    uint8_t     weight_quant_index;
    uint8_t     weight_quant_model;
    int8_t      weight_quant_param_delta[8];
    uint8_t     aec_enable;
};

bool parse_seq_header(AvsSeqHdr* hdr, const uint8_t* data, int size);
bool parse_pic_header_I(AvsPicHdr* hdr, const AvsSeqHdr* seq, const uint8_t* data, int size);
bool parse_pic_header_PB(AvsPicHdr* hdr, const AvsSeqHdr* seq, const uint8_t* data, int size);

}   // namespace irk_avs_dec
#endif
