#ifndef LOOKUP_H
#define LOOKUP_H

#include "pos.h"

int hexchar_to_int(char c);

Record* read_bucket(FILE* file, size_t bucket_index, uint16_t* out_count, int* num_seeks);
Record* read_bucket_by_hash(FILE* file, const uint8_t* hash, int num_prefix_bytes, uint16_t* out_count, int* num_seeks);
Record* search_records(FILE* file, const uint8_t* hash, int num_prefix_bytes, int* num_seeks);

int hexchar_to_int(char c);
int parse_hex_string(const char* hex_str, uint8_t* out_bytes, size_t byte_len);

size_t calc_filesize(const char* filename);

uint8_t* generate_random_hash(int prefix_bytes);

void print_records(const Record* records, size_t count);

#endif