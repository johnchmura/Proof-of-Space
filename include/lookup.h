#ifndef LOOKUP_H
#define LOOKUP_H

#include "pos.h"

int hexchar_to_int(char c);

Record* read_bucket(FILE* file, size_t bucket_index, uint16_t* out_count, int* num_seeks); //Seek a bucket
Record* read_bucket_by_hash(FILE* file, const uint8_t* hash, int num_prefix_bytes, uint16_t* out_count, int* num_seeks); //Find bucket index via a prefix
Record* search_records(FILE* file, const uint8_t* hash, int num_prefix_bytes, int* num_seeks); // Binary search through the combined buckets

int hexchar_to_int(char c); //Helper functions for testing
int parse_hex_string(const char* hex_str, uint8_t* out_bytes, size_t byte_len);

size_t calc_filesize(const char* filename);

uint8_t* generate_random_hash(int prefix_bytes); //Random prefixes to search for

void print_records(const Record* records, size_t count); 

#endif