#ifndef HASHVERIFY_H
#define HASHVERIFY_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "../include/pos.h"

int verify_hashes_file(const char* filename, bool verify_hashes, size_t num_from_head);
int verify_random_hashes(const char* filename, size_t count);
int verify_hash(const Record* record);


void print_record(const Record* record, size_t record_ct);
void print_tail_records(const char* filename, size_t record_ct);
void print_head_records(const char* filename, size_t record_ct);


#endif