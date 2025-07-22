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

void generate_records(const uint8_t* starting_nonce, int num_prefix_bytes, Bucket* buckets) {
    blake3_hasher hasher;
    uint8_t* nonce = malloc(NONCE_SIZE);
    size_t records_batch = NUM_BUCKETS * MAX_RECORDS_PER_BUCKET;

    if (nonce == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        return;
    }

    for (size_t i = 0; i < NUM_BUCKETS; i++) {
        buckets[i].record_count = 0;
    }

    memcpy(nonce, starting_nonce, NONCE_SIZE);
    blake3_hasher_init(&hasher);

    for (int i = 0; i < records_batch; i++) {
        uint8_t hash[HASH_SIZE];
        Record record;

        blake3_hasher_reset(&hasher);
        blake3_hasher_update(&hasher, nonce, NONCE_SIZE);
        blake3_hasher_finalize(&hasher, hash, HASH_SIZE);

        uint32_t prefix = 0;
        for (int j = 0; j < num_prefix_bytes; j++) {
            prefix = (prefix << 8) | hash[j];
        }

        uint32_t bucket_i = ((uint64_t)prefix * NUM_BUCKETS) >> (num_prefix_bytes * 8);
        if (bucket_i >= NUM_BUCKETS) bucket_i = NUM_BUCKETS - 1;

        Bucket* bucket = &buckets[bucket_i];
        if (bucket->record_count < MAX_RECORDS_PER_BUCKET) {
            memcpy(record.hash, hash, HASH_SIZE);
            memcpy(record.nonce, nonce, NONCE_SIZE);
            bucket->records[bucket->record_count++] = record;
        }

        increment_nonce(nonce, NONCE_SIZE);
    }

    free(nonce);
}


int dump_buckets(Bucket* buckets, size_t num_buckets, const char* filename) {
    FILE* file = fopen(filename, "ab");
    
    if (!file) {
        perror("Failed to open the file.");
        return -1;
    }
    
    for (size_t i = 0; i < num_buckets; i++) {
        Bucket* bucket = &buckets[i];

        uint16_t count = bucket->record_count;

        fputc(count & 0xFF, file); // Write the count of records in each bucket (byte by byte)
        fputc((count >> 8) & 0xFF, file);

        size_t written = fwrite(bucket->records, sizeof(Record), count, file);

        if (written != count) {
            perror("Failed to write records to file.");
            fclose(file);
            return -1;
        }

        size_t padding = MAX_RECORDS_PER_BUCKET - count;
        Record empty_record = {0};

        for (size_t j = 0; j < padding; j++) {
            if( fwrite(&empty_record, sizeof(Record), 1, file) != 1) { // Write empty records to fill the bucket
                perror("Failed to write padding records to file.");
                fclose(file);
                return -1;
            }
        }
    }

    fclose(file);
    return 0;
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
    return (bits + 7) / 8; // round up to needed bytes
}