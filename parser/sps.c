
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bits.h"
#include "sps.h"

void
hexdump(const void *p, int len);

static int
parse_hrd(struct bits_t* bits, struct hrd_t* hrd)
{
    int index;

    hrd->cpb_cnt_minus1                                 = in_ueint(bits);
    hrd->bit_rate_scale                                 = in_uint(bits, 4);
    hrd->cpb_size_scale                                 = in_uint(bits, 4);
    for (index = 0; index <= hrd->cpb_cnt_minus1; index++ )
    {
        if (index > 31)
        {
            break;
        }
        hrd->bit_rate_value_minus1[index]               = in_ueint(bits);
        hrd->cpb_size_value_minus1[index]               = in_ueint(bits);
        hrd->cbr_flag[index]                            = in_uint(bits, 1);
    }
    hrd->initial_cpb_removal_delay_length_minus1        = in_uint(bits, 5);
    hrd->cpb_removal_delay_length_minus1                = in_uint(bits, 5);
    hrd->dpb_output_delay_length_minus1                 = in_uint(bits, 5);
    hrd->time_offset_length                             = in_uint(bits, 5);
    return 0;
}

static int
parse_vui(struct bits_t* bits, struct vui_t* vui)
{
    vui->aspect_ratio_info_present_flag                 = in_uint(bits, 1);
    if (vui->aspect_ratio_info_present_flag)
    {
        vui->aspect_ratio_idc                           = in_uint(bits, 8);
        if (vui->aspect_ratio_idc == 255)
        {
            /* Extended_SAR */
            vui->sar_width                              = in_uint(bits, 16);
            vui->sar_height                             = in_uint(bits, 16);
        }
    }

    vui->overscan_info_present_flag                     = in_uint(bits, 1);
    if (vui->overscan_info_present_flag)
    {
        vui->overscan_appropriate_flag                  = in_uint(bits, 1);
    }

    vui->video_signal_type_present_flag                 = in_uint(bits, 1);
    if (vui->video_signal_type_present_flag)
    {
        vui->video_format                               = in_uint(bits, 3);
        vui->video_full_range_flag                      = in_uint(bits, 1);
        vui->colour_description_present_flag            = in_uint(bits, 1);
        if (vui->colour_description_present_flag)
        {
            vui->colour_primaries                       = in_uint(bits, 8);
            vui->transfer_characteristics               = in_uint(bits, 8);
            vui->matrix_coefficients                    = in_uint(bits, 8);
        }
    }

    vui->chroma_loc_info_present_flag                   = in_uint(bits, 1);
    if (vui->chroma_loc_info_present_flag)
    {
        vui->chroma_sample_loc_type_top_field           = in_ueint(bits);
        vui->chroma_sample_loc_type_bottom_field        = in_ueint(bits);
    }
    
    vui->timing_info_present_flag                       = in_uint(bits, 1);
    if (vui->timing_info_present_flag)
    {
        vui->num_units_in_tick                          = in_uint(bits, 32);
        vui->time_scale                                 = in_uint(bits, 32);
        vui->fixed_frame_rate_flag                      = in_uint(bits, 1);
    }
    
    vui->nal_hrd_parameters_present_flag                = in_uint(bits, 1);
    if (vui->nal_hrd_parameters_present_flag)
    {
        parse_hrd(bits, &(vui->nal_hrd_parameters));
    }

    vui->vcl_hrd_parameters_present_flag                = in_uint(bits, 1);
    if (vui->vcl_hrd_parameters_present_flag)
    {
        parse_hrd(bits, &(vui->vcl_hrd_parameters));
    }

    if (vui->nal_hrd_parameters_present_flag ||
        vui->vcl_hrd_parameters_present_flag)
    {
        vui->low_delay_hrd_flag                         = in_uint(bits, 1);
    }

    vui->pic_struct_present_flag                        = in_uint(bits, 1);

    vui->bitstream_restriction_flag                     = in_uint(bits, 1);
    if (vui->bitstream_restriction_flag)
    {
        vui->motion_vectors_over_pic_boundaries_flag    = in_uint(bits, 1);
        vui->max_bytes_per_pic_denom                    = in_ueint(bits);
        vui->max_bits_per_mb_denom                      = in_ueint(bits);
        vui->log2_max_mv_length_horizontal              = in_ueint(bits);
        vui->log2_max_mv_length_vertical                = in_ueint(bits);
        vui->num_reorder_frames                         = in_ueint(bits);
        vui->max_dec_frame_buffering                    = in_ueint(bits);
    }

    return 0;
}

int
parse_sps(struct bits_t* bits, struct sps_t* sps)
{
    int index;
    int count;

    sps->forbidden_zero_bit                             = in_uint(bits, 1);
    sps->nal_ref_idc                                    = in_uint(bits, 2);
    sps->nal_unit_type                                  = in_uint(bits, 5);
    sps->profile_idc                                    = in_uint(bits, 8);
    sps->constraint_set0_flag                           = in_uint(bits, 1);
    sps->constraint_set1_flag                           = in_uint(bits, 1);
    sps->constraint_set2_flag                           = in_uint(bits, 1);
    sps->constraint_set3_flag                           = in_uint(bits, 1);
    sps->reserved_zero_4bits                            = in_uint(bits, 4);
    sps->level_idc                                      = in_uint(bits, 8);
    sps->seq_parameter_set_id                           = in_ueint(bits);
    /* some alpha fields can be here */
    sps->log2_max_frame_num_minus4                      = in_ueint(bits);
    sps->pic_order_cnt_type                             = in_ueint(bits);
    if (sps->pic_order_cnt_type == 0)
    {
        sps->log2_max_pic_order_cnt_lsb_minus4          = in_ueint(bits);
    }
    else if (sps->pic_order_cnt_type == 1)
    {
        sps->delta_pic_order_always_zero_flag           = in_uint(bits, 1);
        sps->offset_for_non_ref_pic                     = in_seint(bits);
        sps->offset_for_top_to_bottom_field             = in_seint(bits);
        sps->num_ref_frames_in_pic_order_cnt_cycle      = in_ueint(bits);
        count = sps->num_ref_frames_in_pic_order_cnt_cycle;
        for (index = 0; index < count; index++)
        {
            in_ueint(bits);
        }
    }
    sps->num_ref_frames                                 = in_ueint(bits);
    sps->gaps_in_frame_num_value_allowed_flag           = in_uint(bits, 1);
    sps->pic_width_in_mbs_minus_1                       = in_ueint(bits);
    sps->pic_height_in_map_units_minus_1                = in_ueint(bits);
    sps->frame_mbs_only_flag                            = in_uint(bits, 1);
    if (!sps->frame_mbs_only_flag)
    {
        sps->mb_adaptive_frame_field_flag               = in_uint(bits, 1);
    }
    sps->direct_8x8_inference_flag                      = in_uint(bits, 1);
    sps->frame_cropping_flag                            = in_uint(bits, 1);
    if (sps->frame_cropping_flag)
    {
        sps->frame_crop_left_offset                     = in_ueint(bits);
        sps->frame_crop_right_offset                    = in_ueint(bits);
        sps->frame_crop_top_offset                      = in_ueint(bits);
        sps->frame_crop_bottom_offset                   = in_ueint(bits);
    }
    sps->vui_prameters_present_flag                     = in_uint(bits, 1);
    if (sps->vui_prameters_present_flag)
    {
        //hexdump(bits->data, bits->end_data - bits->data);
        parse_vui(bits, &(sps->vui));
    }

    sps->width = 16 * (sps->pic_width_in_mbs_minus_1 + 1);
    sps->height = 16 * (2 - sps->frame_mbs_only_flag) *
                  (sps->pic_height_in_map_units_minus_1 + 1);

    return 0;
}

