#ifndef HASHVERIFY_H
#define HASHVERIFY_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "../include/pos.h"

ssize_t verify_hashes_file(const char* filename, bool verify_hashes); // Go through buckets and make sure they are in order
int verify_random_hashes(const char* filename, size_t count); // Generate random index values. Sort them and then go through the file once checking if they line up with the BLAKE3 hashes
int verify_hash(const Record* record); // check a nonce against the BLAKE3 hash


void print_record(const Record* record, size_t record_ct); //Print records
void print_tail_records(const char* filename, size_t record_ct);
void print_head_records(const char* filename, size_t record_ct);


#endif