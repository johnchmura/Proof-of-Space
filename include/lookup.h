#ifndef LOOKUP_H
#define LOOKUP_H

#include "pos.h"

int hexchar_to_int(char c);

Record* read_bucket(const char* filename, size_t bucket_index, uint16_t* out_count);
Record* read_bucket_by_hash(const char* filename, const uint8_t* hash, int num_prefix_bytes, uint16_t* out_count);

int hexchar_to_int(char c);
int parse_hex_string(const char* hex_str, uint8_t* out_bytes, size_t byte_len);

#endif