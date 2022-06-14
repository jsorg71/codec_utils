
#ifndef _BITS_H_
#define _BITS_H_

struct bits_t
{
    char* data;
    char* end_data;
    unsigned long long byte_data;
    int bits_left;
    int error;
};

int
in_uint(struct bits_t* bits, int num_bits);
int
in_ueint(struct bits_t* bits);
int
in_seint(struct bits_t* bits);

int
out_uint(struct bits_t* bits, int val, int num_bits);
int
out_ueint(struct bits_t* bits, int val);
int
out_seint(struct bits_t* bits, int val);

#endif
