
#ifndef _SPS_H_
#define _SPS_H_

struct hrd_t
{
    int cpb_cnt_minus1;
    int bit_rate_scale;
    int cpb_size_scale;
    int bit_rate_value_minus1[32];
    int cpb_size_value_minus1[32];
    int cbr_flag[32];
    int initial_cpb_removal_delay_length_minus1;
    int cpb_removal_delay_length_minus1;
    int dpb_output_delay_length_minus1;
    int time_offset_length;
};

struct vui_t
{
    int aspect_ratio_info_present_flag;         /* u(1) */
    int aspect_ratio_idc;                       /* u(8) */
    int sar_width;                              /* u(16) */
    int sar_height;                             /* u(16) */
    int overscan_info_present_flag;             /* u(1) */
    int overscan_appropriate_flag;              /* u(1) */
    int video_signal_type_present_flag;         /* u(1) */
    int video_format;                           /* u(3) */
    int video_full_range_flag;                  /* u(1) */
    int colour_description_present_flag;        /* u(1) */
    int colour_primaries;                       /* u(8) */
    int transfer_characteristics;               /* u(8) */
    int matrix_coefficients;                    /* u(8) */
    int chroma_loc_info_present_flag;           /* u(1) */
    int chroma_sample_loc_type_top_field;       /* ue(v) */
    int chroma_sample_loc_type_bottom_field;    /* ue(v) */
    int timing_info_present_flag;               /* u(1) */
    int num_units_in_tick;                      /* u(32) */
    int time_scale;                             /* u(32) */
    int fixed_frame_rate_flag;                  /* u(1) */
    int nal_hrd_parameters_present_flag;        /* u(1) */
    struct hrd_t nal_hrd_parameters;
    int vcl_hrd_parameters_present_flag;        /* u(1) */
    struct hrd_t vcl_hrd_parameters;
    int low_delay_hrd_flag;                     /* u(1) */
    int pic_struct_present_flag;                /* u(1) */
    int bitstream_restriction_flag;             /* u(1) */
    int motion_vectors_over_pic_boundaries_flag;/* u(1) */
    int max_bytes_per_pic_denom;                /* ue(v) */
    int max_bits_per_mb_denom;                  /* ue(v) */
    int log2_max_mv_length_horizontal;          /* ue(v) */
    int log2_max_mv_length_vertical;            /* ue(v) */
    int num_reorder_frames;                     /* ue(v) */
    int max_dec_frame_buffering;                /* ue(v) */
};

struct sps_t
{
    int forbidden_zero_bit;                     /* u(1) */
    int nal_ref_idc;                            /* u(2) */
    int nal_unit_type;                          /* u(5) */
    int profile_idc;                            /* u(8) */
    int constraint_set0_flag;                   /* u(1) */
    int constraint_set1_flag;                   /* u(1) */
    int constraint_set2_flag;                   /* u(1) */
    int constraint_set3_flag;                   /* u(1) */
    int reserved_zero_4bits;                    /* u(4) */
    int level_idc;                              /* u(8) */
    int seq_parameter_set_id;                   /* ue(v) */
    int log2_max_frame_num_minus4;              /* ue(v) */
    int pic_order_cnt_type;                     /* ue(v) */
    int log2_max_pic_order_cnt_lsb_minus4;      /* ue(v) */
    int delta_pic_order_always_zero_flag;       /* u(1) */
    int offset_for_non_ref_pic;                 /* se(v) */
    int offset_for_top_to_bottom_field;         /* se(v) */
    int num_ref_frames_in_pic_order_cnt_cycle;  /* ue(v) */
    int num_ref_frames;                         /* ue(v) */
    int gaps_in_frame_num_value_allowed_flag;   /* u(1) */
    int pic_width_in_mbs_minus_1;               /* ue(v) */
    int pic_height_in_map_units_minus_1;        /* ue(v) */
    int frame_mbs_only_flag;                    /* u(1) */
    int mb_adaptive_frame_field_flag;           /* u(1) */
    int direct_8x8_inference_flag;              /* u(1) */
    int frame_cropping_flag;                    /* u(1) */
    int frame_crop_left_offset;                 /* ue(v) */
    int frame_crop_right_offset;                /* ue(v) */
    int frame_crop_top_offset;                  /* ue(v) */
    int frame_crop_bottom_offset;               /* ue(v) */
    int vui_prameters_present_flag;             /* u(1) */

    struct vui_t vui;

    int width;
    int height;
};

int
parse_sps(struct bits_t* bits, struct sps_t* sps);

#endif
