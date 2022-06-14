
#ifndef _UTILS_H_
#define _UTILS_H_

void
hexdump(const void *p, int len);
int
parse_start_code(const char* data, const char* end_data);
int
get_nal_bytes(const char* data, const char* end_data);
int
rbsp_to_nal(const char* rbsp_buf, int rbsp_size, char* nal_buf, int* nal_size);
int
nal_to_rbsp(const char* nal_buf, int* nal_size, char* rbsp_buf, int* rbsp_size);

#endif
