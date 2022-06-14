
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bits.h"

static const int g_len_table[256] =
{
    1,
    1,
    2, 2,
    3, 3, 3, 3,
    4, 4, 4, 4, 4, 4, 4, 4,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8
};

int
in_uint(struct bits_t* bits, int num_bits)
{
    int rv;
    int shift_pos;
    unsigned long long mask;

    if (bits->error)
    {
        return 0;
    }
    if (num_bits > 32)
    {
        bits->error = 1;
        return 0;
    }
    while (bits->bits_left < num_bits)
    {
        if (bits->data >= bits->end_data)
        {
            bits->error = 1;
            return 0;
        }
        bits->byte_data <<= 8;
        bits->byte_data |= (unsigned char)(bits->data[0]);
        bits->data++;
        bits->bits_left += 8;
    }
    shift_pos = bits->bits_left - num_bits;
    bits->bits_left -= num_bits;
    mask = 1;
    mask = (mask << num_bits) - 1;
    rv = (bits->byte_data >> shift_pos) & mask;
    return rv;
}

int
in_ueint(struct bits_t* bits)
{
    int rv;
    int zero_count;

    zero_count = 0;
    rv = in_uint(bits, 1);
    while (rv == 0)
    {
        if (bits->error)
        {
            return 0;
        }
        zero_count++;
        rv = in_uint(bits, 1);
    }
    rv = ((rv << zero_count) | in_uint(bits, zero_count)) - 1;
    return rv;
}

int
in_seint(struct bits_t* bits)
{
    int rv;

    rv = in_ueint(bits);
    rv = rv & 1 ? -(rv / 2) : (rv + 1) / 2;
    return rv;
}

int
out_uint(struct bits_t* bits, int val, int num_bits)
{
    int index;
    int shift_pos;
    int bits_write_back;
    unsigned long long byte_data;
    unsigned long long mask;

    if (bits->error)
    {
        return 0;
    }
    if (num_bits > 32)
    {
        bits->error = 1;
        return 0;
    }
    while (bits->bits_left < num_bits)
    {
        if (bits->data >= bits->end_data)
        {
            bits->error = 1;
            return 0;
        }
        bits->byte_data <<= 8;
        bits->byte_data |= (unsigned char)(bits->data[0]);
        bits->data++;
        bits->bits_left += 8;
    }
    shift_pos = bits->bits_left - num_bits;
    bits_write_back = bits->bits_left;
    bits->bits_left -= num_bits;
    mask = 1;
    mask = (mask << num_bits) - 1;
    bits->byte_data &= ~(mask << shift_pos);
    bits->byte_data |= (val & mask) << shift_pos;
    index = -1;
    byte_data = bits->byte_data;
    while (bits_write_back > 0)
    {
        bits->data[index] = byte_data;
        byte_data >>= 8;
        bits_write_back -= 8;
        index--;
    }
    return 0;
}

int
out_ueint(struct bits_t* bits, int val)
{
    int len;

    if (val == 0)
    {
        out_uint(bits, 1, 1);
    }
    else
    {
        val++;
        if (val >= 0x01000000)
        {
            len = 24 + g_len_table[val >> 24];
        }
        else if(val >= 0x00010000)
        {
            len = 16 + g_len_table[val >> 16];
        }
        else if(val >= 0x00000100)
        {
            len =  8 + g_len_table[val >> 8];
        }
        else 
        {
            len = g_len_table[val];
        }
        out_uint(bits, val, 2 * len - 1);
    }
    return 0;
}

int
out_seint(struct bits_t* bits, int val)
{
    if (val <= 0)
    {
        out_ueint(bits, -val * 2);
    }
    else
    {
        out_ueint(bits, val * 2 - 1);
    }
    return 0;
}

