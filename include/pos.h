#ifndef POS_H
#define POS_H

#include <stdint.h>
#include <math.h>

#define HASH_SIZE 10
#define NONCE_SIZE 6
#define K 27ULL
#define NUM_RECORDS (1ULL << K)
#define TEMP_FILE "temp.bin"

#define NUM_BUCKETS 67100 // 67100 buckets at 1000 records is about a GB of memory
#define MAX_RECORDS_PER_BUCKET 1000

#define PRINT_TIME 0.5

#define NUM_BATCHES ((size_t)(NUM_RECORDS + NUM_BUCKETS * MAX_RECORDS_PER_BUCKET - 1) / (NUM_BUCKETS * MAX_RECORDS_PER_BUCKET))

typedef struct { //total 16 bytes 
    uint8_t hash[HASH_SIZE]; // hash value as byte array 
    uint8_t nonce[NONCE_SIZE]; // Nonce value as byte array 
} Record;

typedef struct {
    Record records[MAX_RECORDS_PER_BUCKET];
    uint16_t record_count;
} Bucket;

void generate_records(const uint8_t* starting_nonce, int num_prefix_bytes, Bucket* buckets, size_t records_batch, double* last_print, size_t records_generated);

int dump_buckets(Bucket* buckets, size_t num_buckets, const char* filename);

void increment_nonce(uint8_t *nonce, size_t nonce_size);

int compare_records(const void* a, const void* b);
void sort_records(Bucket* buckets, size_t num_buckets);
void merge_and_sort_buckets(const char* input_file, const char* output_file, int num_threads_sort);

int calc_max_records_per_bucket(size_t memory_mb);
int calc_prefix_bytes(size_t num_buckets);

#endif