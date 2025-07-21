#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>

#include "../BLAKE3/c/blake3.h"
#include "../include/lookup.h"
#include "../include/pos.h"

int main(int argc, char* argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <filename> <hash> <num_prefix_bytes>\n", argv[0]);
        return 1;
    }

    const char* filename = argv[1];
    const char* hash_str = argv[2];
    int num_prefix_bytes = atoi(argv[3]);

    if (strlen(hash_str) != HASH_SIZE * 2) {
        fprintf(stderr, "Hash must be %d hex characters long.\n", HASH_SIZE * 2);
        return 1;
    }

    uint8_t hash[HASH_SIZE];
    if (!parse_hex_string(hash_str, hash, HASH_SIZE)) {
        fprintf(stderr, "Invalid hex characters in hash.\n");
        return 1;
    }

    uint16_t out_count;
    Record* records = read_bucket_by_hash(filename, hash, num_prefix_bytes, &out_count);
    if (!records) {
        fprintf(stderr, "Failed to read records by hash\n");
        return 1;
    }

    printf("Read %d records by hash:\n", out_count);
    free(records);
    return 0;
}

Record* read_bucket(const char* filename, size_t bucket_index, uint16_t* out_count) {
    size_t bucket_size = 2 + MAX_RECORDS_PER_BUCKET * sizeof(Record);
    size_t offset = bucket_index * bucket_size;

    FILE* file = fopen(filename, "rb");
    if (!file) {
        perror("Failed to open file");
        return NULL;
    }

    if (fseek(file, offset, SEEK_SET) != 0) {
        perror("Failed to seek to bucket");
        fclose(file);
        return NULL;
    }

    int start = fgetc(file);
    int end = fgetc(file);
    if (start == EOF || end == EOF) {
        perror("Failed to read record count");
        fclose(file);
        return NULL;
    }

    uint16_t count = (uint16_t)(start | (end << 8));
    if (out_count) *out_count = count;

    Record* records = malloc(sizeof(Record) * count);
    if (!records) {
        fprintf(stderr, "Memory allocation failed\n");
        fclose(file);
        return NULL;
    }

    if (fread(records, sizeof(Record), count, file) != count) {
        perror("Failed to read records");
        free(records);
        fclose(file);
        return NULL;
    }

    fclose(file);
    return records;
}

Record* read_bucket_by_hash(const char* filename, const uint8_t* hash, int num_prefix_bytes, uint16_t* out_count){
    uint32_t bucket_i = 0;
        for (int j = 0; j < num_prefix_bytes; j++) {
            bucket_i = (bucket_i << 8) | hash[j];
        }
    bucket_i = bucket_i % NUM_BUCKETS;

    Record* records = read_bucket(filename, bucket_i, out_count);
    if (!records) {
        fprintf(stderr, "Failed to read records from bucket %d\n", bucket_i);
        return NULL;
    }
    return records;
}

int hexchar_to_int(char c) {
    if ('0' <= c && c <= '9') return c - '0';
    if ('a' <= c && c <= 'f') return c - 'a' + 10;
    if ('A' <= c && c <= 'F') return c - 'A' + 10;
    return -1;
}

int parse_hex_string(const char* hex_str, uint8_t* out_bytes, size_t byte_len) {
    size_t len = strlen(hex_str);
    if (len != byte_len * 2) return 0;

    for (size_t i = 0; i < byte_len; ++i) {
        int high = hexchar_to_int(hex_str[i * 2]);
        int low  = hexchar_to_int(hex_str[i * 2 + 1]);
        if (high < 0 || low < 0) return 0;
        out_bytes[i] = (high << 4) | low;
    }
    return 1;
}