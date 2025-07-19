#ifndef POS_H
#define POS_H

#include <stdint.h>
#include <math.h>

#define HASH_SIZE 10
#define NONCE_SIZE 6
#define K 10
#define NUM_RECORDS (1 << K)

#define MAX_MEM 1024 * 1024 * 1024

#define NUM_BUCKETS 67100 // 67100 buckets at 1000 records is about a GB of memory
#define MAX_RECORDS_PER_BUCKET 1000

typedef struct { //total 16 bytes 
    uint8_t hash[HASH_SIZE]; // hash value as byte array 
    uint8_t nonce[NONCE_SIZE]; // Nonce value as byte array 
} Record;

typedef struct {
    Record records[MAX_RECORDS_PER_BUCKET];
    uint16_t record_count;
} Bucket;

Bucket* generate_records(int num_prefix_bytes);

int dump_buckets(Bucket* buckets, size_t num_buckets, const char* filename);
void print_records(const Record* records, uint16_t count);

int verify_record(const Record* record);

void increment_nonce(uint8_t *nonce, size_t nonce_size);

int compare_records(const void* a, const void* b);
void sort_records(Bucket* buckets, size_t num_buckets);

int calc_prefix_bytes(size_t num_buckets);

#endif