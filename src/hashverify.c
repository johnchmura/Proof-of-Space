#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include <unistd.h>
#include <getopt.h>

#include <omp.h>

#include <time.h>

#include "../BLAKE3/c/blake3.h"
#include "../include/pos.h"
#include "../include/hashverify.h"

bool debug;
size_t num_unsorted;
int main(int argc, char* argv[]) {

    char* filename = "buckets.bin";
    int num_records_head = 0;
    int num_records_tail = 0;
    debug = false;
    bool verify_hashes = false;
    int num_valid_checks = 0;
    int opt;


    while (( opt = getopt(argc, argv, "f:p:r:v:d:b:h")) != -1) {
        switch (opt) {
            case 'f':
                filename = optarg;
                break;
            case 'p':
                num_records_head = atoi(optarg);
                break;
            case 'r':
                num_records_tail = atoi(optarg);
                break;
            case 'v':
                verify_hashes = strcmp(optarg, "true") == 0 || strcmp(optarg, "1") == 0;
                break;
            case 'd':
                debug = strcmp(optarg, "true") == 0 || strcmp(optarg, "1") == 0;
                break;
            case 'b':
                num_valid_checks = atoi(optarg);
                break;
            case 'h':
                printf("Help:\n"
                       "  -f <filename>: Specify the output filename\n"
                       "  -p <num_records>: Number of records to print from head\n"
                       "  -r <num_records>: Number of records to print from tail\n"
                       "  -v <bool>: Verify hashes from file, off with false, on with true\n"
                       "  -d <bool>: Enable debug mode\n"
                       "  -h: Display this help message\n");
                return 0;
            default:
                printf("Help:\n"
                       "  -f <filename>: Specify the output filename\n"
                       "  -p <num_records>: Number of records to print from head\n"
                       "  -r <num_records>: Number of records to print from tail\n"
                       "  -v <bool>: Verify hashes from file, off with false, on with true\n"
                       "  -d <bool>: Enable debug mode\n"
                       "  -h: Display this help message\n");
                return 0;
        }
    }
    
    if (debug) {
        printf("DEBUG=true\n");
        printf("FILENAME=%s\n", filename);
        printf("VERIFY_HASHES=%d\n", verify_hashes);
        printf("RECORD_SIZE=%zuB\n", sizeof(Record));
        printf("HASH_SIZE=%dB\n", HASH_SIZE);
        printf("NONCE_SIZE=%dB\n", NONCE_SIZE);
    }
    int failed = 0;
    uint64_t total_records = 0;
    if(num_records_head > 0){
        printf("Printing first %d of file %s...\n",num_records_head, filename);
        print_head_records(filename, num_records_head);
    }
    if (num_records_tail > 0 ){
        print_tail_records(filename, num_records_tail, total_records);
    }

    if(verify_hashes && ((total_records = verify_hashes_file(filename, MAX_RECORDS_PER_BUCKET, verify_hashes, num_records_head)) <= 0)){
        fprintf(stderr,"Something failed in verifying files");
        return 1;
    }

    if(verify_hashes && total_records > 0){
        printf("Total records: %zu\n",total_records);
        printf("Number of unsorted: %zu\n",num_unsorted);
    }

    if(num_valid_checks > 0){
        printf("Verifying random records against BLAKE3 hashes\n");

        failed = verify_random_hashes(filename, num_valid_checks);
        if(failed < 0){
            fprintf(stderr, "Hash verification against BLAKE3 failed\n");
            return -1;
        }
        printf("Number of total verifications: %d\n",num_valid_checks);
        printf("Number of verifications successful: %d\n",num_valid_checks - failed);
        printf("Number of verifications failed: %d\n",failed);
    }


    return 0;
}

int verify_hashes_file(const char* filename, size_t _unused, bool verify_hashes, size_t num_from_head) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        perror("Failed to open the file");
        return 0;
    }

    size_t total_records = 0;
    num_unsorted = 0;

    const size_t record_size = sizeof(Record);

    double start_time = omp_get_wtime();
    double last_print_time = start_time;
    static int print_count = 0;

    for (size_t bucket = 0; bucket < NUM_BUCKETS; bucket++) {
        uint8_t header[2];
        if (fread(header, 1, 2, file) != 2) {
            fprintf(stderr, "Failed to read record count header for bucket %zu\n", bucket);
            break;
        }

        uint16_t record_count = header[0] | (header[1] << 8);
        if (record_count == 0) continue;

        Record* records = malloc(record_count * record_size);
        if (!records) {
            fprintf(stderr, "Memory allocation failed\n");
            break;
        }

        if (fread(records, record_size, record_count, file) != record_count) {
            fprintf(stderr, "Failed to read %u records from bucket %zu\n", record_count, bucket);
            free(records);
            break;
        }

        for (size_t i = 0; i < record_count; i++) {

            if (i > 0 && memcmp(records[i - 1].hash, records[i].hash, HASH_SIZE) > 0) {
                num_unsorted++;
            }

        }

        total_records += record_count;
        free(records);

        double now = omp_get_wtime();
        double interval = now - last_print_time;
        if (debug && (interval >= PRINT_TIME || bucket == NUM_BUCKETS - 1)) {
            double elapsed = now - start_time;
            double percent = 100.0 * total_records / NUM_RECORDS;
            double eta = elapsed * (NUM_RECORDS - total_records) / (total_records + 1e-5);

            print_count++;
            printf("[%.3f][VERIFY]: %.2f%% completed, ETA %.1f seconds\n",elapsed, percent, eta);
            fflush(stdout);
            last_print_time = now;
        }
    }

    fclose(file);
    return total_records;
}

int verify_random_hashes(const char* filename, size_t count) {
    if (count == 0) return -1;

    FILE* file = fopen(filename, "rb");
    if (!file) {
        perror("Failed to open file for hash verification");
        return -1;
    }

    const size_t record_size = sizeof(Record);
    size_t* bucket_offsets = malloc(NUM_BUCKETS * sizeof(size_t));
    uint16_t* bucket_counts = calloc(NUM_BUCKETS, sizeof(uint16_t));
    size_t offset = 0, total_records = 0;

    for (size_t i = 0; i < NUM_BUCKETS; i++) {
        bucket_offsets[i] = offset;

        if (fseek(file, offset, SEEK_SET) != 0) {
            perror("seek");
            free(bucket_offsets); free(bucket_counts); fclose(file);
            return -1;
        }

        uint8_t header[2];
        if (fread(header, 1, 2, file) != 2) {
            perror("header read");
            free(bucket_offsets); free(bucket_counts); fclose(file);
            return -1;
        }

        uint16_t count_in_bucket = header[0] | (header[1] << 8);
        bucket_counts[i] = count_in_bucket;
        offset += 2 + count_in_bucket * record_size;
        total_records += count_in_bucket;
    }

    if (total_records == 0) {
        printf("No records to verify.\n");
        free(bucket_offsets); free(bucket_counts); fclose(file);
        return -1;
    }

    if (count > total_records) count = total_records;

    srand(time(NULL));
    size_t* indices = malloc(count * sizeof(size_t));
    for (size_t i = 0; i < count; i++) {
        indices[i] = rand() % total_records;
    }
    qsort(indices, count, sizeof(size_t), (__compar_fn_t)memcmp);

    size_t current_global = 0, current_index = 0;
    size_t verified = 0, failed = 0;

    for (size_t b = 0; b < NUM_BUCKETS && current_index < count; b++) {
        uint16_t bucket_count = bucket_counts[b];
        if (bucket_count == 0) continue;

        if (fseek(file, bucket_offsets[b] + 2, SEEK_SET) != 0) {
            perror("Failed to seek to bucket data");
            break;
        }

        Record* records = malloc(bucket_count * record_size);
        if (!records) {
            fprintf(stderr, "Memory allocation failed\n");
            break;
        }

        if (fread(records, record_size, bucket_count, file) != bucket_count) {
            perror("Failed to read bucket data");
            free(records);
            break;
        }

        for (size_t i = 0; i < bucket_count && current_index < count; i++) {
            if (current_global == indices[current_index]) {
                if (!verify_hash(&records[i])) {
                    failed++;
                }
                verified++;
                current_index++;
            }
            current_global++;
        }

        free(records);
    }

    free(indices);
    free(bucket_offsets);
    free(bucket_counts);
    fclose(file);

    return failed;
}


int verify_hash(const Record* record) {
    uint8_t test_hash[HASH_SIZE];
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);
    blake3_hasher_update(&hasher, record->nonce, NONCE_SIZE);
    blake3_hasher_finalize(&hasher, test_hash, HASH_SIZE);
    return memcmp(test_hash, record->hash, HASH_SIZE) == 0;
}

void print_record(const Record* record, size_t record_ct) {
    if (record == NULL) {
        fprintf(stderr, "Record is NULL\n");
        return;
    }

    printf("[%zu] ", record_ct*16);

    printf("Hash: ");
    for (int i = 0; i < HASH_SIZE; i++) {
        printf("%02x", record->hash[i]);
    }
    printf(" : ");
    for (int i = 0; i < NONCE_SIZE; i++) {
        printf("%02x", record->nonce[i]);
    }
    
    uint64_t nonce_val = 0;
    for (int i = NONCE_SIZE - 1; i >= 0; i--) {
        nonce_val = (nonce_val << 8) | record->nonce[i];
    }
    printf(" : %lu", nonce_val);

    printf("\n");
}

void print_head_records(const char* filename, size_t record_ct) {
    if (record_ct == 0) return;

    FILE* file = fopen(filename, "rb");
    if (!file) {
        perror("Failed to open file for head printing");
        return;
    }

    const size_t record_size = sizeof(Record);
    size_t printed = 0;

    for (size_t i = 0; i < NUM_BUCKETS && printed < record_ct; i++) {
        uint8_t header[2];
        if (fread(header, 1, 2, file) != 2) {
            perror("Failed to read record count header");
            break;
        }

        uint16_t record_count = header[0] | (header[1] << 8);
        if (record_count == 0) {
            fseek(file, record_count * record_size, SEEK_CUR);
            continue;
        }

        Record* records = malloc(record_count * record_size);
        if (!records) {
            fprintf(stderr, "Memory allocation failed\n");
            break;
        }

        if (fread(records, record_size, record_count, file) != record_count) {
            perror("Failed to read records from bucket");
            free(records);
            break;
        }

        for (size_t j = 0; j < record_count && printed < record_ct; j++) {
            print_record(&records[j], printed);
            printed++;
        }

        free(records);
    }

    fclose(file);
}

void print_tail_records(const char* filename, size_t record_ct, size_t total_records) {
    if (record_ct == 0 || total_records == 0) return;
    if (record_ct > total_records) record_ct = total_records;

    FILE* file = fopen(filename, "rb");
    if (!file) {
        perror("Failed to open file for tail printing");
        return;
    }

    const size_t record_size = sizeof(Record);
    size_t printed = 0;
    size_t current_index = 0;

    for (size_t i = 0; i < NUM_BUCKETS; i++) {
        uint8_t header[2];
        if (fread(header, 1, 2, file) != 2) {
            perror("Failed to read record count");
            break;
        }

        uint16_t record_count = header[0] | (header[1] << 8);
        if (record_count == 0) {
            // Skip empty bucket
            fseek(file, record_count * record_size, SEEK_CUR);
            continue;
        }

        Record* records = malloc(record_count * record_size);
        if (!records) {
            fprintf(stderr, "Memory allocation failed\n");
            break;
        }

        if (fread(records, record_size, record_count, file) != record_count) {
            perror("Failed to read records");
            free(records);
            break;
        }

        for (size_t j = 0; j < record_count; j++) {
            size_t global_idx = current_index + j;
            if (global_idx >= total_records - record_ct) {
                print_record(&records[j], global_idx);
                printed++;
                if (printed >= record_ct) break;
            }
        }

        current_index += record_count;
        free(records);

        if (printed >= record_ct) break;
    }

    fclose(file);
}

