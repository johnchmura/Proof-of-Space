#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>

#include <unistd.h>
#include <getopt.h>

#include <time.h>


#include "../BLAKE3/c/blake3.h"
#include "../include/lookup.h"
#include "../include/pos.h"

int main(int argc, char* argv[]) {
    srand(time(NULL));

    char* filename = "buckets.bin";
    int num_searches = 0;
    int prefix_bytes = 0;
    int num_seeks = 0;
    bool debug = false;
    int opt;
    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    while (( opt = getopt(argc, argv, "f:c:l:d:h")) != -1) {
        switch (opt) {
            case 'f':
                filename = optarg;
                break;
            case 'c':
                num_searches = atoi(optarg);
                break;
            case 'l':
                prefix_bytes = atoi(optarg);
                break;
            case 'd':
                debug = strcmp(optarg, "true") == 0 || strcmp(optarg, "1") == 0;
                break;
            case 'h':
                printf("Help:\n"
                       "  -f <filename>: Specify the output filename\n"
                       "  -c <num_searches>: Number of searches to perform\n"
                       "  -l <prefix_bytes>: Number of prefix bytes to use\n"
                       "  -h: Display this help message\n");
                return 0;
            default:
                printf("Help:\n"
                       "  -f <filename>: Specify the output filename\n"
                       "  -c <num_searches>: Number of searches to perform\n"
                       "  -l <prefix_bytes>: Number of prefix bytes to use\n"
                       "  -h: Display this help message\n");
                return 0;
        }
    }

    size_t filesize = calc_filesize(filename);

    if (debug) {
        printf("DEBUG=true\n");
        printf("FILENAME=%s\n", filename);
        printf("search_records=%d\n", num_searches);
        printf("prefixLength=%d\n", prefix_bytes);
        printf("FILESIZE_byte=%zu\n", filesize);
        printf("RECORD_SIZE=%zu\n", sizeof(Record));
        printf("HASH_SIZE=%d\n", HASH_SIZE);
        printf("NONCE_SIZE=%d\n", NONCE_SIZE);
    }

    printf("Searching for random hashes...\n");

    int total_found = 0;
    FILE* file = fopen(filename, "rb");

    blake3_hasher hasher;
    blake3_hasher_init(&hasher);

    for( int i = 0; i < num_searches; i++) {
        uint8_t hash[HASH_SIZE];

        uint8_t* random_hash = generate_random_hash(prefix_bytes);
        if (!random_hash) {
            fprintf(stderr, "Failed to generate random hash\n");
            continue;
        }
        memcpy(hash, random_hash, HASH_SIZE);
        free(random_hash);

        Record* record = search_records(file, hash, prefix_bytes, &num_seeks);
        if (record) {
            total_found++;
        }
        free(record);
    }

    fclose(file);
    
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    double elapsed_time_ms = (end_time.tv_sec - start_time.tv_sec) * 1000.0 + (end_time.tv_nsec - start_time.tv_nsec) / 1000000.0;
    double avg_lookup = elapsed_time_ms / num_searches;
    printf("Number of total lookups: %d\n", num_searches);
    printf("Number of records found: %d\n", total_found);
    printf("Number of total seeks: %d\n", num_seeks);
    printf("Time taken: %.4f ms/lookup\n", avg_lookup);
    printf("Throughput: %.2f lookups/s\n", num_searches / (elapsed_time_ms / 1000.0));

}

Record* search_records(FILE* file, const uint8_t* hash, int num_prefix_bytes, int* num_seeks) {
    uint16_t record_count = 0;
    Record* records = read_bucket_by_hash(file, hash, num_prefix_bytes, &record_count, num_seeks);
    if (!records) {
        fprintf(stderr, "Failed to read records from bucket by hash\n");
        return NULL;
    }
    if (record_count == 0) {
        free(records);
        return NULL;
    }

    int left = 0;
    int right = record_count - 1;
    while (left <= right) {
        int mid = left + (right - left) / 2;
        int cmp = memcmp(records[mid].hash, hash, HASH_SIZE);
        if (cmp == 0) {
            Record* found_record = malloc(sizeof(Record));
            if (!found_record) {
                fprintf(stderr, "Memory allocation failed\n");
                free(records);
                return NULL;
            }
            *found_record = records[mid];
            free(records);
            return found_record;
        } else if (cmp < 0) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }

    free(records);
    return NULL;
}

Record* read_bucket(FILE* file, size_t bucket_index, uint16_t* record_count, int* num_seeks) {
    size_t bucket_size = 2 + MAX_RECORDS_PER_BUCKET * sizeof(Record);
    size_t offset = bucket_index * bucket_size;

    if (fseek(file, offset, SEEK_SET) != 0) {
        perror("Failed to seek to bucket");
        return NULL;
    }

    if (num_seeks) {
        (*num_seeks)++;
    }

    int start = fgetc(file);
    int end = fgetc(file);
    if (start == EOF || end == EOF) {
        perror("Failed to read record count");
        fclose(file);
        return NULL;
    }

    uint16_t count = (uint16_t)(start | (end << 8));
    if (record_count) *record_count = count;

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

    return records;
}

Record* read_bucket_by_hash(FILE* file, const uint8_t* hash, int num_prefix_bytes, uint16_t* record_count, int* num_seeks) {
    uint32_t bucket_i = 0;
        for (int j = 0; j < num_prefix_bytes; j++) {
            bucket_i = (bucket_i << 8) | hash[j];
        }
    bucket_i = ((uint64_t)bucket_i * NUM_BUCKETS) >> (num_prefix_bytes * 8);

    return read_bucket(file, bucket_i, record_count, num_seeks);
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

uint8_t* generate_random_hash(int prefix_bytes) {
    if (prefix_bytes < 1 || prefix_bytes > HASH_SIZE) {
        fprintf(stderr, "Invalid prefix length: %d\n", prefix_bytes);
        return NULL;
    }

    uint8_t* hash = calloc(HASH_SIZE, sizeof(uint8_t));
    if (!hash) {
        fprintf(stderr, "Memory allocation failed for hash\n");
        return NULL;
    }

    for (int i = 0; i < prefix_bytes; i++) {
        hash[i] = rand() & 0xFF;
    }

    return hash;
}


size_t calc_filesize(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        perror("Failed to open file");
        return 0;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        perror("Failed to seek to end of file");
        fclose(file);
        return 0;
    }

    size_t filesize = ftell(file);
    fclose(file);
    return filesize;
}

void print_records(const Record* records, size_t count) {
    for (size_t i = 0; i < count; i++) {
        printf("Record %zu: ", i);
        for (size_t j = 0; j < HASH_SIZE; j++) {
            printf("%02x", records[i].hash[j]);
        }
        printf("\n");
    }
}

