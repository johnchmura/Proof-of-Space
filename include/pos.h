#ifndef POS_H
#define POS_H

#include <stdint.h>

#define HASH_SIZE 10
#define NONCE_SIZE 6
#define NUM_RECORDS 1000

#define NUM_BUCKETS 5
#define MAX_RECORDS_PER_BUCKET 10

typedef struct { 
    uint8_t hash[HASH_SIZE]; // hash value as byte array 
    uint8_t nonce[NONCE_SIZE]; // Nonce value as byte array 
} Record;

typedef struct {
    Record records[MAX_RECORDS_PER_BUCKET];
    size_t record_count;
} Bucket;

Bucket* generate_records();

void increment_nonce(uint8_t *nonce, size_t nonce_size);


#endif