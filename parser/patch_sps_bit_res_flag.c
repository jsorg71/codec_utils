
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "bits.h"
#include "sps.h"
#include "utils.h"

static int
sps_set_it_vui(struct bits_t* bits)
{
    int val;
    int val1;

    val = in_uint(bits, 1); // aspect_ratio_info_present_flag
    if (val)
    {
        val = in_uint(bits, 8); // aspect_ratio_idc
        if (val == 255)
        {
            /* Extended_SAR */
            in_uint(bits, 16); // sar_width
            in_uint(bits, 16); // sar_height
        }
    }

    val = in_uint(bits, 1); // overscan_info_present_flag
    if (val)
    {
        in_uint(bits, 1); // overscan_appropriate_flag
    }

    val = in_uint(bits, 1); // video_signal_type_present_flag
    if (val)
    {
        in_uint(bits, 3); // video_format
        in_uint(bits, 1); // video_full_range_flag
        val = in_uint(bits, 1); // colour_description_present_flag
        if (val)
        {
            in_uint(bits, 8); // colour_primaries
            in_uint(bits, 8); // transfer_characteristics
            in_uint(bits, 8); // matrix_coefficients
        }
    }

    val = in_uint(bits, 1); // chroma_loc_info_present_flag
    if (val)
    {
        in_ueint(bits); // chroma_sample_loc_type_top_field
        in_ueint(bits); // chroma_sample_loc_type_bottom_field
    }

    val = in_uint(bits, 1); // timing_info_present_flag
    if (val)
    {
        in_uint(bits, 32); // num_units_in_tick
        in_uint(bits, 32); // time_scale
        in_uint(bits, 1); // fixed_frame_rate_flag
    }

    val = in_uint(bits, 1); // nal_hrd_parameters_present_flag
    if (val)
    {
        //parse_hrd(bits, &(vui->nal_hrd_parameters));
    }

    val1 = in_uint(bits, 1); // vcl_hrd_parameters_present_flag
    if (val1)
    {
        //parse_hrd(bits, &(vui->vcl_hrd_parameters));
    }

    if (val || val1)
    {
        in_uint(bits, 1); // low_delay_hrd_flag
    }

    in_uint(bits, 1); // pic_struct_present_flag

    out_uint(bits, 1, 1); // bitstream_restriction_flag
    out_uint(bits, 1, 1); // motion_vectors_over_pic_boundaries_flag
    out_ueint(bits, 0); // max_bytes_per_pic_denom
    out_ueint(bits, 0); // max_bits_per_mb_denom
    out_ueint(bits, 11); // log2_max_mv_length_horizontal
    out_ueint(bits, 11); // log2_max_mv_length_vertical
    out_ueint(bits, 0); // num_reorder_frames
    out_ueint(bits, 1); // max_dec_frame_buffering

#if 0
    val = in_uint(bits, 1); // bitstream_restriction_flag
    printf("bitstream_restriction_flag %d left %d\n", val, bits->bits_left);
    if (val)
    {
        in_uint(bits, 1); // motion_vectors_over_pic_boundaries_flag
        in_ueint(bits); // max_bytes_per_pic_denom
        in_ueint(bits); // max_bits_per_mb_denom
        in_ueint(bits); // log2_max_mv_length_horizontal
        in_ueint(bits); // log2_max_mv_length_vertical
        in_ueint(bits); // num_reorder_frames
        in_ueint(bits); // max_dec_frame_buffering
    }
#endif
    return 0;
}

static int
sps_set_it(const char* data, int data_bytes, char* new_sps)
{
    char rbsp[64];
    int ldata_bytes;
    int rbsp_bytes;
    int nal_bytes;
    int index;
    int val;
    struct bits_t lbits;
    struct bits_t* bits;

    //hexdump(data, data_bytes);
    ldata_bytes = data_bytes;
    rbsp_bytes = 64;
    nal_to_rbsp(data, &ldata_bytes, rbsp, &rbsp_bytes);
    //hexdump(rbsp, rbsp_bytes);

    bits = &lbits;
    memset(&lbits, 0, sizeof(lbits));
    lbits.data = rbsp;
    lbits.data_bytes = 64;

    in_uint(bits, 1); // forbidden_zero_bit
    in_uint(bits, 2); // nal_ref_idc
    in_uint(bits, 5); // nal_unit_type
    in_uint(bits, 8); // profile_idc
    in_uint(bits, 1); // constraint_set0_flag
    in_uint(bits, 1); // constraint_set1_flag
    in_uint(bits, 1); // constraint_set2_flag
    in_uint(bits, 1); // constraint_set3_flag
    in_uint(bits, 4); // reserved_zero_4bits
    in_uint(bits, 8); // level_idc
    in_ueint(bits); // seq_parameter_set_id
    /* some alpha fields can be here */
    in_ueint(bits); // log2_max_frame_num_minus4
    val = in_ueint(bits); // pic_order_cnt_type
    if (val == 0)
    {
        in_ueint(bits); // log2_max_pic_order_cnt_lsb_minus4
    }
    else if (val == 1)
    {
        in_uint(bits, 1); // delta_pic_order_always_zero_flag
        in_seint(bits); // offset_for_non_ref_pic
        in_seint(bits); // offset_for_top_to_bottom_field
        val = in_ueint(bits); // num_ref_frames_in_pic_order_cnt_cycle
        for (index = 0; index < val; index++)
        {
            in_ueint(bits);
        }
    }
    in_ueint(bits); // num_ref_frames
    in_uint(bits, 1); // gaps_in_frame_num_value_allowed_flag
    in_ueint(bits); // pic_width_in_mbs_minus_1
    in_ueint(bits); // pic_height_in_map_units_minus_1
    val = in_uint(bits, 1); // frame_mbs_only_flag
    if (!val)
    {
        in_uint(bits, 1); // mb_adaptive_frame_field_flag
    }
    in_uint(bits, 1); // direct_8x8_inference_flag
    val = in_uint(bits, 1); // frame_cropping_flag
    if (val)
    {
        in_ueint(bits); // frame_crop_left_offset
        in_ueint(bits); // frame_crop_right_offset
        in_ueint(bits); // frame_crop_top_offset
        in_ueint(bits); // frame_crop_bottom_offset
    }
    val = in_uint(bits, 1); // vui_prameters_present_flag
    printf("vui_prameters_present_flag %d\n", val);
    if (val)
    {
        sps_set_it_vui(bits);
    }

    out_uint(bits, 1, 1); // stop bit
    while (bits->bits_left > 0)
    {
        out_uint(bits, 0, 1); // align bits
    }

    rbsp_bytes = bits->offset;
    printf("rbsp_bytes %d\n", rbsp_bytes);

    //memcpy(new_sps, data, data_bytes);
    //rbsp_bytes = data_bytes;

    nal_bytes = 64;
    rbsp_to_nal(rbsp, rbsp_bytes, new_sps, &nal_bytes);

    //hexdump(new_sps, nal_bytes);

    return nal_bytes;
}

int
main(int argc, char** argv)
{
    int fd;
    int out_fd;
    int data_bytes;
    int nal_bytes;
    int start_code_bytes;
    int total_out_bytes;
    int new_sps_bytes;
    char* alloc_data;
    char* data;
    char* end_data;
    char new_sps[64];
#if 0
    struct bits_t lbits;
    memset(&lbits, 0, sizeof(lbits));
    lbits.data = new_sps;
    lbits.data_bytes = 5;
    memset(new_sps, 0, sizeof(new_sps));
    hexdump(new_sps, 64);
    in_uint(&lbits, 1);
    out_uint(&lbits, 0x11223344, 32);
    printf("error %d\n", lbits.error);
    hexdump(new_sps, 64);
    memset(&lbits, 0, sizeof(lbits));
    lbits.data = new_sps;
    lbits.data_bytes = 5;
    in_uint(&lbits, 1);
    printf("0x%8.8x\n", in_uint(&lbits, 32));
    printf("error %d\n", lbits.error);
    return 0;
#endif
    if (argc < 3)
    {
        printf("error\n");
        return 1;
    }
    fd = open(argv[1], O_RDONLY);
    if (fd == -1)
    {
        printf("error\n");
        return 1;
    }
    out_fd = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd == -1)
    {
        printf("error\n");
        return 1;
    }
    total_out_bytes = 0;
    data_bytes = 1024 * 1024;
    alloc_data = (char*)malloc(data_bytes);
    data = alloc_data;
    data_bytes = read(fd, data, data_bytes);
    close(fd);
    printf("data_bytes in %d\n", data_bytes);
    end_data = data + data_bytes;
    //hexdump(data, 32);
    start_code_bytes = parse_start_code(data, end_data);
    write(out_fd, data, start_code_bytes);
    total_out_bytes += start_code_bytes;
    data += start_code_bytes;
    while ((nal_bytes = get_nal_bytes(data, end_data)) > 0)
    {
        //printf("nal_bytes %d\n", nal_bytes);
        if ((data[0] & 0x1F) == 7)
        {
            new_sps_bytes = sps_set_it(data, nal_bytes, new_sps);
            write(out_fd, new_sps, new_sps_bytes);
            total_out_bytes += new_sps_bytes;
        }
        else
        {
            write(out_fd, data, nal_bytes);
            total_out_bytes += nal_bytes;
        }
        printf("nal_bytes %d nal type %x\n", nal_bytes, data[0] & 0x1F);
        data += nal_bytes;
        start_code_bytes = parse_start_code(data, end_data);
        if (start_code_bytes < 1)
        {
            break;
        }
        write(out_fd, data, start_code_bytes);
        total_out_bytes += start_code_bytes;
        data += start_code_bytes;
    }
    free(alloc_data);
    close(out_fd);
    printf("total_out_bytes %d\n", total_out_bytes);
    return 0;
}
