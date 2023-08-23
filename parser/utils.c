
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void
hexdump(const void *p, int len)
{
    unsigned char *line;
    int i;
    int thisline;
    int offset;

    line = (unsigned char *)p;
    offset = 0;

    while (offset < len)
    {
        printf("%04x ", offset);
        thisline = len - offset;

        if (thisline > 16)
        {
            thisline = 16;
        }

        for (i = 0; i < thisline; i++)
        {
            printf("%02x ", line[i]);
        }

        for (; i < 16; i++)
        {
            printf("   ");
        }

        for (i = 0; i < thisline; i++)
        {
            printf("%c", (line[i] >= 0x20 && line[i] < 0x7f) ? line[i] : '.');
        }

        printf("\n");
        offset += thisline;
        line += thisline;
    }
}

int
parse_start_code(const char* data, const char* end_data)
{
    int bytes = (int)(end_data - data);
    if (bytes > 2 && data[0] == 0 && data[1] == 0 && data[2] == 1)
    {
        return 3;
    }
    if (bytes > 3 && data[0] == 0 && data[1] == 0 && data[2] == 0 && data[3] == 1)
    {
        return 4;
    }
    return 0;
}

int
get_nal_bytes(const char* data, const char* end_data)
{
    int bytes = 0;
    while (data < end_data)
    {
        if (end_data - data > 2 && data[0] == 0 && data[1] == 0 && data[2] == 1)
        {
            return bytes;
        }
        if (end_data - data > 3 && data[0] == 0 && data[1] == 0 && data[2] == 0 && data[3] == 1)
        {
            return bytes;
        }
        data++;
        bytes++;
    }
    return bytes;
}

int
rbsp_to_nal(const char* rbsp_buf, int rbsp_size, char* nal_buf, int* nal_size)
{
    int i;
    int j;
    int count;
    int lnal_size;

    j = 0;
    count = 0;
    lnal_size = *nal_size;
    for (i = 0; i < rbsp_size ;)
    {
        if (j >= lnal_size)
        {
            /* error, not enough space */
            return -1;
        }
        if ((count == 2) && !(rbsp_buf[i] & 0xFC)) /* HACK 0xFC */
        {
            nal_buf[j] = 0x03;
            j++;
            count = 0;
            continue;
        }
        nal_buf[j] = rbsp_buf[i];
        if (rbsp_buf[i] == 0x00)
        {
            count++;
        }
        else
        {
            count = 0;
        }
        i++;
        j++;
    }
    *nal_size = j;
    return j;
}

int
nal_to_rbsp(const char* nal_buf, int* nal_size, char* rbsp_buf, int* rbsp_size)
{
    int i;
    int j;
    int count;
    int lnal_size;
    int lrbsp_size;

    j = 0;
    count = 0;
    lnal_size = *nal_size;
    lrbsp_size = *rbsp_size;
    for (i = 0; i < lnal_size; i++)
    { 
        /* in NAL unit, 0x000000, 0x000001 or 0x000002 shall not occur at any
           byte-aligned position */
        if ((count == 2) && ((unsigned char)nal_buf[i] < 0x03))
        {
            return -1;
        }
        if ((count == 2) && (nal_buf[i] == 0x03))
        {
            /* check the 4th byte after 0x000003, except when cabac_zero_word
               is used, in which case the last three bytes of this NAL unit
               must be 0x000003 */
            if ((i < lnal_size - 1) && (nal_buf[i + 1] > 0x03))
            {
                return -1;
            }
            /* if cabac_zero_word is used, the final byte of this NAL
               unit(0x03) is discarded, and the last two bytes of RBSP
               must be 0x0000 */
            if (i == lnal_size - 1)
            {
                break;
            }
            i++;
            count = 0;
        }
        if (j >= lrbsp_size)
        {
            /* error, not enough space */
            return -1;
        }
        rbsp_buf[j] = nal_buf[i];
        if (nal_buf[i] == 0x00)
        {
            count++;
        }
        else
        {
            count = 0;
        }
        j++;
    }
    *nal_size = i;
    *rbsp_size = j;
    return j;
}

