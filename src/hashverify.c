#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include <unistd.h>
#include <getopt.h>

#include <time.h>

#include "../BLAKE3/c/blake3.h"
#include "../include/pos.h"
#include "../include/hashverify.h"

int main(int argc, char* argv[]) {

    char* filename = "buckets.bin";
    int num_records_head = 0;
    int num_records_tail = 0;
    bool debug = false;
    bool verify_hashes = false;
    int opt;

    while (( opt = getopt(argc, argv, "f:p:r:v:d:h")) != -1) {
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

    uint64_t total_records = 0;

    if((total_records = verify_hashes_file(filename, MAX_RECORDS_PER_BUCKET, verify_hashes, num_records_head)) <= 0){
        return 1;
    }

    printf("Total records: %zu\n", total_records);

    printf("Number of unsorted hashes: %lu\n",total_records);

    if (num_records_tail > 0 ){
        print_tail_records(filename, num_records_tail, total_records);
    }

    return 0;
}


int verify_hashes_file(const char* filename, size_t bucket_batch_size, bool verify_hashes, size_t num_from_head) {

    FILE* file = fopen(filename, "rb");
    if (file == NULL) {
        perror("Failed to open the file");
        return 0;
    }
    
    const size_t record_size = NONCE_SIZE + HASH_SIZE;
    const size_t bucket_size = 2 + MAX_RECORDS_PER_BUCKET * record_size;
    const size_t records_per_full_file = NUM_BUCKETS * MAX_RECORDS_PER_BUCKET;

    size_t num_unsorted = 0;

    Record prev;
    bool has_prev = false;

    size_t print_head_left = num_from_head;
    uint64_t total_records = 0;
    size_t batch_idx = 0;

    while (1) {
        size_t bucket_offset = batch_idx * NUM_BUCKETS;
        bool found_any = false;

        for (int i = 0; i < NUM_BUCKETS; i += bucket_batch_size) {
            size_t global_bucket_index = bucket_offset + i;
            size_t current_batch_size = (i + bucket_batch_size < NUM_BUCKETS) ? bucket_batch_size : (NUM_BUCKETS - i);
            size_t num_bytes = current_batch_size * bucket_size;

            if (fseek(file, global_bucket_index * bucket_size, SEEK_SET) != 0) {
                perror("Failed to seek.");
                fclose(file);
                return 0;
            }

            uint8_t* buffer = malloc(num_bytes);
            if (buffer == NULL) {
                fprintf(stderr, "Memory allocation failed.\n");
                fclose(file);
                return 0;
            }

            size_t bytes_read = fread(buffer, 1, num_bytes, file);
            if (bytes_read == 0) {
                // EOF reached
                free(buffer);
                fclose(file);
                return total_records;
            }

            if (bytes_read != num_bytes) {
                if (feof(file)) {
                    num_bytes = bytes_read;
                } else {
                    perror("Failed to read records from file.");
                    free(buffer);
                    fclose(file);
                    return 0;
                }
            }

            for (size_t j = 0; j < current_batch_size; j++) {
                size_t local_offset = j * bucket_size;
                if (local_offset + 2 > num_bytes) break;

                uint8_t* bucket_pt = buffer + local_offset;
                uint16_t record_count = bucket_pt[0] | (bucket_pt[1] << 8);

                if (record_count == 0) continue;
                if (local_offset + 2 + record_count * record_size > num_bytes) break;

                Record* records = (Record*)(bucket_pt + 2);
                found_any = true;

                for (int r = 0; r < record_count; r++) {
                    if (print_head_left > 0) {
                        print_record(&records[r], total_records);
                        print_head_left--;
                    }

                    if (verify_hashes) {
                        if (!verify_hash(&records[r])) {
                            fprintf(stderr, "Hash verification failed for record %d in bucket %zu.\n", r, global_bucket_index + j);
                            free(buffer);
                            fclose(file);
                            return 0;
                        }
                    }

                    if (has_prev && r == 0 && memcmp(prev.hash, records[r].hash, HASH_SIZE) > 0) {
                        num_unsorted++;
                    }

                    if (r > 0 && memcmp(records[r - 1].hash, records[r].hash, HASH_SIZE) > 0) {
                        num_unsorted++;
                    }

                    total_records++;
                }

                if (record_count > 0) {
                    prev = records[record_count - 1];
                    has_prev = true;
                }
            }

            free(buffer);
        }

        if (!found_any) {
            break;
        }

        batch_idx++;
    }

    fclose(file);
    return total_records;
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

void print_tail_records(const char* filename, size_t record_ct, size_t total_records) {
    if (record_ct == 0) return;

    FILE* file = fopen(filename, "rb");
    if (!file) {
        perror("Failed to open file for tail printing");
        return;
    }

    const size_t bucket_size = sizeof(Bucket);
    const size_t record_size = sizeof(Record);

    for (int64_t i = NUM_BUCKETS - 1; i >= 0 && record_ct > 0; i--) {
        if (fseek(file, i * bucket_size, SEEK_SET) != 0) {
            perror("Failed to seek to bucket");
            break;
        }

        uint8_t header[2];
        if (fread(header, 1, 2, file) != 2) {
            perror("Failed to read record count");
            break;
        }
        uint16_t record_count = header[0] | (header[1] << 8);
        if (record_count == 0) continue;

        Record *records = malloc(record_count * sizeof(Record));
        if (fread(records, record_size, record_count, file) != record_count) {
            perror("Failed to read records from bucket");
            break;
        }

        for (int r = record_count - 1; r >= 0 && record_ct > 0; r--) {
            size_t byte_offset = i * bucket_size + 2 + r * record_size;
            print_record(&records[r], byte_offset);
            record_ct--;
        }
        free(records);

    }

    fclose(file);
}


