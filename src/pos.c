#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include "../BLAKE3/c/blake3.h"
#include "../include/pos.h"


void increment_nonce(uint8_t *nonce, size_t nonce_size){
    for (size_t i = 0; i < nonce_size; i++) {
        if (++nonce[i] != 0) {
            break;
        }
    }
}

Bucket* generate_records(int num_prefix_bytes){

    blake3_hasher hasher;
    Bucket* buckets = calloc(NUM_BUCKETS, sizeof(Bucket));
    uint8_t* nonce = malloc(NONCE_SIZE);

    if (buckets == NULL || nonce == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        return NULL;
    }

    for (int i=0; i < NUM_RECORDS; i++) {
        uint8_t hash[HASH_SIZE];
        Record record;
        
        blake3_hasher_init(&hasher);
        blake3_hasher_update(&hasher, nonce, NONCE_SIZE);

        blake3_hasher_finalize(&hasher, hash, HASH_SIZE);

        uint32_t bucket_i = 0;
        for (int j = 0; j < num_prefix_bytes; j++) {
            bucket_i = (bucket_i << 8) | hash[j];
        }
        bucket_i = bucket_i % NUM_BUCKETS;

        Bucket* bucket = &buckets[bucket_i];

        if (bucket->record_count < MAX_RECORDS_PER_BUCKET) {
            memcpy(record.hash, hash, HASH_SIZE);
            memcpy(record.nonce, nonce, NONCE_SIZE);
            bucket->records[bucket->record_count++] = record;
        }

        increment_nonce(nonce, NONCE_SIZE);
    }


    free(nonce);
    return buckets;
}

int compare_records(const void* a, const void* b) {
    const Record* record_a = (const Record*)a;
    const Record* record_b = (const Record*)b;
    return memcmp(record_a->hash, record_b->hash, HASH_SIZE);
}

void sort_records(Bucket* buckets, size_t num_buckets) {
    for (size_t i = 0; i < num_buckets; i++) {
        Bucket* bucket = &buckets[i];
        qsort(bucket->records, bucket->record_count, sizeof(Record), compare_records);
    }
}

int calc_prefix_bytes(size_t num_buckets){
    int bits = 0;
    size_t buckets = num_buckets - 1;
    while (buckets > 0) {
        bits++;
        buckets >>= 1;
    }
    return (bits + 7) / 8; // Go up if we need more bytes to allow more buckets
}