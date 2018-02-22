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

#include "AvsHeaders.h"

namespace irk_avs_dec {

// q.v. 7.1.2.2, 错误的码流返回 false
bool parse_seq_header(AvsSeqHdr* hdr, const uint8_t* data, int size)
{
    assert(*(uint32_as*)data == 0xB0010000);
    if (size < 18)
        return false;
    AvsBitStream bitsm(data + 6, size - 6);

    hdr->profile = data[4];
    hdr->level = data[5];
    hdr->progressive_seq = bitsm.read1();
    hdr->width = bitsm.read_bits(14);
    hdr->height = bitsm.read_bits(14);
    hdr->chroma_format = bitsm.read_bits(2);
    hdr->sample_precision = bitsm.read_bits(3);
    hdr->aspect_ratio = bitsm.read_bits(4);
    hdr->frame_rate_code = bitsm.read_bits(4);
    hdr->bitrate = bitsm.read_bits(18);
    bitsm.skip_bits(1);     // marker_bit
    hdr->bitrate += bitsm.read_bits(12) << 18;
    hdr->low_delay = bitsm.read1();
    bitsm.skip_bits(1);     // marker_bit
    hdr->bbv_buffer_size = bitsm.read_bits(18);
    return true;
}

// q.v. 7.1.3.1, 错误的码流返回 false
bool parse_pic_header_I(AvsPicHdr* hdr, const AvsSeqHdr* seq, const uint8_t* data, int size)
{
    assert(*(uint32_as*)data == 0xB3010000);
    if (size < 8)
        return false;
    AvsBitStream bitsm(data + 4, size - 4);

    hdr->bbv_delay = bitsm.read_bits(16);
    if (seq->profile == 0x48)       // AVS+ broadcast profile
    {
        bitsm.skip_bits(1);         // marker_bit
        hdr->bbv_delay = (hdr->bbv_delay << 7) + bitsm.read_bits(7);
    }

    hdr->time_code_flag = bitsm.read1();
    if (hdr->time_code_flag)
        hdr->time_code = bitsm.read_bits(24);
    else
        hdr->time_code = 0;

    bitsm.skip_bits(1);             // marker_bit
    hdr->pic_type = PIC_TYPE_I;
    hdr->pic_distance = bitsm.read_bits(8);

    if (seq->low_delay)
        hdr->bbv_check_times = bitsm.read_ue16();
    else
        hdr->bbv_check_times = 0;

    hdr->progressive_frame = bitsm.read1();
    if (hdr->progressive_frame == 0)
        hdr->picture_structure = bitsm.read1();
    else
        hdr->picture_structure = 1;

    hdr->top_field_first = bitsm.read1();
    hdr->repeat_first_field = bitsm.read1();

    hdr->fixed_pic_qp = bitsm.read1();
    hdr->pic_qp = bitsm.read_bits(6);

    hdr->pic_ref_flag = 1;      // 第二场可能为 P 场, 其参考帧索引总是为 0
    hdr->no_fwd_ref_flag = 0;
    hdr->pb_field_enhanced_flag = 0;

    if (hdr->progressive_frame == 0 && hdr->picture_structure == 0)
        hdr->skip_mode_flag = bitsm.read1();
    else
        hdr->skip_mode_flag = 0;

    bitsm.skip_bits(4);         // reserved_bits

    // loop filter
    hdr->loop_filter_disable = bitsm.read1();
    hdr->loop_filter_param_flag = 0;
    hdr->alpha_c_offset = 0;
    hdr->beta_offset = 0;
    if (hdr->loop_filter_disable == 0)
    {
        hdr->loop_filter_param_flag = bitsm.read1();
        if (hdr->loop_filter_param_flag)
        {
            hdr->alpha_c_offset = bitsm.read_se8();
            hdr->beta_offset = bitsm.read_se8();
        }
        if (hdr->alpha_c_offset < -8 || hdr->alpha_c_offset > 8)
            return false;
        if (hdr->beta_offset < -8 || hdr->beta_offset > 8)
            return false;
    }

    // weighting quantization
    hdr->weight_quant_flag = 0;
    hdr->chroma_quant_param_disable = 1;
    hdr->chroma_quant_delta_cb = 0;
    hdr->chroma_quant_delta_cr = 0;
    hdr->weight_quant_index = 0;
    hdr->weight_quant_model = 0;
    *(uint64_as*)hdr->weight_quant_param_delta = 0;
    hdr->aec_enable = 0;

    // AVS+ broadcast profile
    if (seq->profile == 0x48)
    {
        if (bitsm.exhausted())
            return false;

        hdr->weight_quant_flag = bitsm.read1();
        if (hdr->weight_quant_flag)
        {
            bitsm.skip_bits(1);          // reserved_bits
            hdr->chroma_quant_param_disable = bitsm.read1();
            if (hdr->chroma_quant_param_disable == 0)
            {
                hdr->chroma_quant_delta_cb = bitsm.read_se8();
                hdr->chroma_quant_delta_cr = bitsm.read_se8();
                if (hdr->chroma_quant_delta_cb < -16 || hdr->chroma_quant_delta_cb > 16)
                    return false;
                if (hdr->chroma_quant_delta_cr < -16 || hdr->chroma_quant_delta_cr > 16)
                    return false;
            }

            hdr->weight_quant_index = bitsm.read_bits(2);
            hdr->weight_quant_model = bitsm.read_bits(2);
            if (hdr->weight_quant_index == 3 || hdr->weight_quant_model == 3)
                return false;
            if (hdr->weight_quant_index != 0)
            {
                for (int i = 0; i < 6; i++)
                {
                    int delta = bitsm.read_se16();
                    if (delta < -128 || delta > 127)
                        return false;
                    hdr->weight_quant_param_delta[i] = static_cast<int8_t>(delta);
                }
            }
        }

        // Advanced Entropy Coding
        hdr->aec_enable = bitsm.read1();
    }

    if (bitsm.exhausted())  // 越界
        return false;
    return true;
}

// q.v. 7.1.3.2, 错误的码流返回 false
bool parse_pic_header_PB(AvsPicHdr* hdr, const AvsSeqHdr* seq, const uint8_t* data, int size)
{
    assert(*(uint32_as*)data == 0xB6010000);
    if (size < 8)
        return false;
    AvsBitStream bitsm(data + 4, size - 4);

    hdr->bbv_delay = bitsm.read_bits(16);
    if (seq->profile == 0x48)       // AVS+ broadcast profile
    {
        bitsm.skip_bits(1);         // marker_bit
        hdr->bbv_delay = (hdr->bbv_delay << 7) + bitsm.read_bits(7);
    }

    hdr->time_code = 0;
    hdr->time_code_flag = 0;

    hdr->pic_type = 1 + bitsm.read_bits(2);
    if (hdr->pic_type != PIC_TYPE_P && hdr->pic_type != PIC_TYPE_B)
        return false;

    hdr->pic_distance = bitsm.read_bits(8);

    if (seq->low_delay)
        hdr->bbv_check_times = bitsm.read_ue16();
    else
        hdr->bbv_check_times = 0;

    hdr->progressive_frame = bitsm.read1();
    if (hdr->progressive_frame == 0)
    {
        hdr->picture_structure = bitsm.read1();
        if (hdr->picture_structure == 0)
            bitsm.skip_bits(1);     // advanced_pred_mode_disable, always 1
    }
    else
    {
        hdr->picture_structure = 1;
    }

    hdr->top_field_first = bitsm.read1();
    hdr->repeat_first_field = bitsm.read1();

    hdr->fixed_pic_qp = bitsm.read1();
    hdr->pic_qp = bitsm.read_bits(6);

    if (!(hdr->pic_type == PIC_TYPE_B && hdr->picture_structure == 1))
        hdr->pic_ref_flag = bitsm.read1();
    else
        hdr->pic_ref_flag = 1;      // 帧编码 B 帧参考帧索引总是为 0

    hdr->no_fwd_ref_flag = bitsm.read1();
    hdr->pb_field_enhanced_flag = bitsm.read1();

    bitsm.skip_bits(2);             // reserved_bits
    hdr->skip_mode_flag = bitsm.read1();

    // loop filter
    hdr->loop_filter_disable = bitsm.read1();
    hdr->loop_filter_param_flag = 0;
    hdr->alpha_c_offset = 0;
    hdr->beta_offset = 0;
    if (hdr->loop_filter_disable == 0)
    {
        hdr->loop_filter_param_flag = bitsm.read1();
        if (hdr->loop_filter_param_flag)
        {
            hdr->alpha_c_offset = bitsm.read_se8();
            hdr->beta_offset = bitsm.read_se8();
        }
        if (hdr->alpha_c_offset < -8 || hdr->alpha_c_offset > 8)
            return false;
        if (hdr->beta_offset < -8 || hdr->beta_offset > 8)
            return false;
    }

    // weighting quantization
    hdr->weight_quant_flag = 0;
    hdr->chroma_quant_param_disable = 1;
    hdr->chroma_quant_delta_cb = 0;
    hdr->chroma_quant_delta_cr = 0;
    hdr->weight_quant_index = 0;
    hdr->weight_quant_model = 0;
    *(uint64_as*)hdr->weight_quant_param_delta = 0;
    hdr->aec_enable = 0;

    // AVS+ broadcast profile
    if (seq->profile == 0x48)
    {
        if (bitsm.exhausted())
            return false;

        hdr->weight_quant_flag = bitsm.read1();
        if (hdr->weight_quant_flag)
        {
            bitsm.skip_bits(1);          // reserved_bits
            hdr->chroma_quant_param_disable = bitsm.read1();
            if (hdr->chroma_quant_param_disable == 0)
            {
                hdr->chroma_quant_delta_cb = bitsm.read_se8();
                hdr->chroma_quant_delta_cr = bitsm.read_se8();
                if (hdr->chroma_quant_delta_cb < -16 || hdr->chroma_quant_delta_cb > 16)
                    return false;
                if (hdr->chroma_quant_delta_cr < -16 || hdr->chroma_quant_delta_cr > 16)
                    return false;
            }

            hdr->weight_quant_index = bitsm.read_bits(2);
            hdr->weight_quant_model = bitsm.read_bits(2);
            if (hdr->weight_quant_index == 3 || hdr->weight_quant_model == 3)
                return false;
            if (hdr->weight_quant_index != 0)
            {
                for (int i = 0; i < 6; i++)
                {
                    int delta = bitsm.read_se16();
                    if (delta < -128 || delta > 127)
                        return false;
                    hdr->weight_quant_param_delta[i] = static_cast<int8_t>(delta);
                }
            }
        }

        // Advanced Entropy Coding
        hdr->aec_enable = bitsm.read1();
    }

    if (bitsm.exhausted())  // 越界
        return false;
    return true;
}

}   // namespace irk_avs_dec
