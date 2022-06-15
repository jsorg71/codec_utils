/*
slice_type  name of slice
0           P
1           B
2           I
3           SP
4           SI
5           P
6           B
7           I
8           SP
9           SI
*/

/* https://chromium.googlesource.com/external/webrtc/+/HEAD/common_video/h264/sps_parser.cc */
/* https://en.wikipedia.org/wiki/Exponential-Golomb_coding */
/* https://www.cardinalpeak.com/blog/the-h-264-sequence-parameter-set */
/* https://github.com/alb423/sps_pps_parser */
/* https://blog.birost.com/a?ID=01450-72ff90f9-a465-464a-91c7-d4671f46cece */
/* https://base64.guru/converter/encode/hex */
/* https://mradionov.github.io/h264-bitstream-viewer/ */
/* https://github.com/cplussharp/graph-studio-next/blob/master/src/H264StructReader.cpp#L32 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "bits.h"
#include "sps.h"
#include "utils.h"

static int
get_next_frame(int fd, char* data, int* bytes, int* width, int* height)
{
    struct _header
    {
        char text[4];
        int width;
        int height;
        int bytes_follow;
    } header;

    if (read(fd, &header, sizeof(header)) != sizeof(header))
    {
        return 1;
    }
    if (strncmp(header.text, "BEEF", 4) != 0)
    {
        printf("not BEEF file\n");
        return 2;
    }
    if (header.bytes_follow > *bytes)
    {
        return 3;
    }
    if (read(fd, data, header.bytes_follow) != header.bytes_follow)
    {
        return 4;
    }
    printf("new frame width %d height %d bytes_follow %d\n", header.width, header.height, header.bytes_follow);
    *bytes = header.bytes_follow; 
    *width = header.width;
    *height = header.height;
    return 0;
}

static int
process_sps(char* data, int bytes)
{
    struct sps_t sps;
    struct bits_t bits;

    bits_init(&bits, data, bytes);
    memset(&sps, 0, sizeof(sps));
    parse_sps(&bits, &sps);

    printf("    bits.error                              %d\n", bits.error);
    printf("    bytes left                              %d\n", (int)(bits.data_bytes - bits.offset));
    printf("    forbidden_zero_bit                      %d\n", sps.forbidden_zero_bit);
    printf("    nal_ref_idc                             %d\n", sps.nal_ref_idc);
    printf("    nal_unit_type                           %d\n", sps.nal_unit_type);
    printf("    profile_idc                             %d\n", sps.profile_idc);
    printf("    constraint_set0_flag                    %d\n", sps.constraint_set0_flag);
    printf("    constraint_set1_flag                    %d\n", sps.constraint_set1_flag);
    printf("    constraint_set2_flag                    %d\n", sps.constraint_set2_flag);
    printf("    constraint_set3_flag                    %d\n", sps.constraint_set3_flag);
    printf("    reserved_zero_4bits                     %d\n", sps.reserved_zero_4bits);
    printf("    level_idc                               %d\n", sps.level_idc);
    printf("    seq_parameter_set_id                    %d\n", sps.seq_parameter_set_id);
    printf("    log2_max_frame_num_minus4               %d\n", sps.log2_max_frame_num_minus4);
    printf("    pic_order_cnt_type                      %d\n", sps.pic_order_cnt_type);
    printf("    log2_max_pic_order_cnt_lsb_minus4       %d\n", sps.log2_max_pic_order_cnt_lsb_minus4);
    printf("    delta_pic_order_always_zero_flag        %d\n", sps.delta_pic_order_always_zero_flag);
    printf("    offset_for_non_ref_pic                  %d\n", sps.offset_for_non_ref_pic);
    printf("    offset_for_top_to_bottom_field          %d\n", sps.offset_for_top_to_bottom_field);
    printf("    num_ref_frames_in_pic_order_cnt_cycle   %d\n", sps.num_ref_frames_in_pic_order_cnt_cycle);
    printf("    num_ref_frames                          %d\n", sps.num_ref_frames);
    printf("    gaps_in_frame_num_value_allowed_flag    %d\n", sps.gaps_in_frame_num_value_allowed_flag);
    printf("    pic_width_in_mbs_minus_1                %d\n", sps.pic_width_in_mbs_minus_1);
    printf("    pic_height_in_map_units_minus_1         %d\n", sps.pic_height_in_map_units_minus_1);
    printf("    frame_mbs_only_flag                     %d\n", sps.frame_mbs_only_flag);
    printf("    mb_adaptive_frame_field_flag            %d\n", sps.mb_adaptive_frame_field_flag);
    printf("    direct_8x8_inference_flag               %d\n", sps.direct_8x8_inference_flag);
    printf("    frame_cropping_flag                     %d\n", sps.frame_cropping_flag);
    printf("    frame_crop_left_offset                  %d\n", sps.frame_crop_left_offset);
    printf("    frame_crop_right_offset                 %d\n", sps.frame_crop_right_offset);
    printf("    frame_crop_top_offset                   %d\n", sps.frame_crop_top_offset);
    printf("    frame_crop_bottom_offset                %d\n", sps.frame_crop_bottom_offset);
    printf("    vui_prameters_present_flag              %d\n", sps.vui_prameters_present_flag);
    printf("    aspect_ratio_info_present_flag          %d\n", sps.vui.aspect_ratio_info_present_flag);
    printf("    aspect_ratio_idc                        %d\n", sps.vui.aspect_ratio_idc);
    printf("    sar_width                               %d\n", sps.vui.sar_width);
    printf("    sar_height                              %d\n", sps.vui.sar_height);
    printf("    overscan_info_present_flag              %d\n", sps.vui.overscan_info_present_flag);
    printf("    overscan_appropriate_flag               %d\n", sps.vui.overscan_appropriate_flag);
    printf("    video_signal_type_present_flag          %d\n", sps.vui.video_signal_type_present_flag);
    printf("    video_format                            %d\n", sps.vui.video_format);
    printf("    video_full_range_flag                   %d\n", sps.vui.video_full_range_flag);
    printf("    colour_description_present_flag         %d\n", sps.vui.colour_description_present_flag);
    printf("    colour_primaries                        %d\n", sps.vui.colour_primaries);
    printf("    transfer_characteristics                %d\n", sps.vui.transfer_characteristics);
    printf("    matrix_coefficients                     %d\n", sps.vui.matrix_coefficients);
    printf("    chroma_loc_info_present_flag            %d\n", sps.vui.chroma_loc_info_present_flag);
    printf("    chroma_sample_loc_type_top_field        %d\n", sps.vui.chroma_sample_loc_type_top_field);
    printf("    chroma_sample_loc_type_bottom_field     %d\n", sps.vui.chroma_sample_loc_type_bottom_field);
    printf("    timing_info_present_flag                %d\n", sps.vui.timing_info_present_flag);
    printf("    num_units_in_tick                       %d\n", sps.vui.num_units_in_tick);
    printf("    time_scale                              %d\n", sps.vui.time_scale);
    printf("    fixed_frame_rate_flag                   %d\n", sps.vui.fixed_frame_rate_flag);
    printf("    nal_hrd_parameters_present_flag         %d\n", sps.vui.nal_hrd_parameters_present_flag);
    printf("    vcl_hrd_parameters_present_flag         %d\n", sps.vui.vcl_hrd_parameters_present_flag);
    printf("    low_delay_hrd_flag                      %d\n", sps.vui.low_delay_hrd_flag);
    printf("    pic_struct_present_flag                 %d\n", sps.vui.pic_struct_present_flag);
    printf("    bitstream_restriction_flag              %d\n", sps.vui.bitstream_restriction_flag);
    printf("    motion_vectors_over_pic_boundaries_flag %d\n", sps.vui.motion_vectors_over_pic_boundaries_flag);
    printf("    max_bytes_per_pic_denom                 %d\n", sps.vui.max_bytes_per_pic_denom);
    printf("    max_bits_per_mb_denom                   %d\n", sps.vui.max_bits_per_mb_denom);
    printf("    log2_max_mv_length_horizontal           %d\n", sps.vui.log2_max_mv_length_horizontal);
    printf("    log2_max_mv_length_vertical             %d\n", sps.vui.log2_max_mv_length_vertical);
    printf("    num_reorder_frames                      %d\n", sps.vui.num_reorder_frames);
    printf("    max_dec_frame_buffering                 %d\n", sps.vui.max_dec_frame_buffering);
    printf("    width                                   %d\n", sps.width);
    printf("    height                                  %d\n", sps.height);
    return 0;
}

static int
process_pps(const char* data, int bytes)
{
    return 0;
}

char test[] =
{
    0x67, 0x42, 0xc0, 0x20, 0xda, 0x01, 0x0c, 0x1d,
    0xf9, 0x78, 0x40, 0x00, 0x00, 0x03, 0x00, 0x40,
    0x00, 0x00, 0x0c, 0x23, 0xc6, 0x0c, 0xa8 };

int
main(int argc, char** argv)
{
    int fd;
    char* alloc_data;
    char* data;
    char* end_data;
    int data_bytes;
    int width;
    int height;
    int nal_unit_type;
    int start_code_bytes;
    int nal_bytes;
    char* rbsp;
    int lnal_bytes;
    int rbsp_bytes;

    if (argc < 2)
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
    data_bytes = 1024 * 1024;
    alloc_data = (char*)malloc(data_bytes);
    while (get_next_frame(fd, alloc_data, &data_bytes, &width, &height) == 0)
    {
        data = alloc_data;
        end_data = data + data_bytes;
        while (data < end_data)
        {
            start_code_bytes = parse_start_code(data, end_data);
            if (start_code_bytes == 0)
            {
                printf("bad start code\n");
                break;
            }
            data += start_code_bytes;
            nal_bytes = get_nal_bytes(data, end_data);

            lnal_bytes = nal_bytes;
            rbsp_bytes = lnal_bytes + 16;
            rbsp = (char*)malloc(rbsp_bytes);
            if (rbsp != NULL)
            {
                if (nal_to_rbsp(data, &lnal_bytes, rbsp, &rbsp_bytes) != -1)
                {
                    printf("  start_code_bytes %d rbsp_bytes %d\n", start_code_bytes, rbsp_bytes);
                    nal_unit_type = rbsp[0] & 0x1F;
                    printf("  nal_unit_type 0x%2.2x\n", nal_unit_type);
                    switch (nal_unit_type)
                    {
                        case 1: /* Coded slice of a non-IDR picture */
                            hexdump(data, 32);
                            break;
                        case 5: /* Coded slice of an IDR picture */
                            hexdump(data, 32);
                            break;
                        case 7: /* Sequence parameter set */
                            hexdump(rbsp, rbsp_bytes);
                            process_sps(rbsp, rbsp_bytes);
                            break;
                        case 8: /* Picture parameter set */
                            hexdump(rbsp, rbsp_bytes);
                            process_pps(rbsp, rbsp_bytes);
                            break;
                        case 9: /* Access unit delimiter */
                            hexdump(rbsp, rbsp_bytes);
                            break;
                        default:
                            break;
                    }
                }
                free(rbsp);
            }
            data += nal_bytes;
        }
        data_bytes = 1024 * 1024;
    }
    free(alloc_data);
    close(fd);

    printf("end test\n");
#if 0
    lnal_bytes = sizeof(test);
    rbsp_bytes = lnal_bytes + 16;
    rbsp = (char*)malloc(rbsp_bytes);
    if (rbsp != NULL)
    {
        if (nal_to_rbsp(test, &lnal_bytes, rbsp, &rbsp_bytes) != -1)
        {
            hexdump(test, sizeof(test));
            hexdump(rbsp, sizeof(test));
            process_sps(rbsp, rbsp_bytes);
        }
        free(rbsp);
    }
#endif
    return 0;
}
