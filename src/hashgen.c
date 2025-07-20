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

int main(int argc, char* argv[]) {

    char* filename = "buckets.bin";
    int num_records_head = 0;
    int num_records_tail = 0;
    bool debug = false;
    bool verify_order = false;
    int num_hashes_verify = 0;
    int memory_mb = 16;
    int file_size_mb = 1024;
    int num_threads_hash = 1;
    int num_threads_sort = 1;
    int num_threads_write = 1;
    int opt;

    while (( opt = getopt(argc, argv, "f:p:r:d:v:b:m:s:t:o:i:h")) != -1) {
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
            case 'd':
                debug = strcmp(optarg, "true") == 0 || strcmp(optarg, "1") == 0;
                break;
            case 'v':
                verify_order = strcmp(optarg, "true") == 0 || strcmp(optarg, "1") == 0;
                break;
            case 'b':
                num_hashes_verify = atoi(optarg);
                break;
            case 'm':
                memory_mb = atoi(optarg);
                break;
            case 's':
                file_size_mb = atoi(optarg);
                break;
            case 't':
                num_threads_hash = atoi(optarg);
                break;
            case 'o':
                num_threads_sort = atoi(optarg);
                break;
            case 'i':
                num_threads_write = atoi(optarg);
                break;
            case 'h':
                printf("Help:\n"
                       "  -f <filename>: Specify the output filename (default: buckets.bin)\n"
                       "  -p <num_records>: Number of records to print from head\n"
                       "  -r <num_records>: Number of records to print from tail\n"
                       "  -d <bool>: Enable debug mode\n"
                       "  -v <bool>: Verify order of records\n"
                       "  -b <bool>: Verify hashes of records\n"
                       "  -m <memory_mb>: Set memory size in MB (default: 16MB)\n"
                       "  -s <file_size_mb>: Set file size in MB (default: 1024MB)\n"
                       "  -t <num_threads_hash>: Set number of threads for hashing (default: 1)\n"
                       "  -o <num_threads_sort>: Set number of threads for sorting (default: 1)\n"
                       "  -i <num_threads_write>: Set number of threads for writing (default: 1)\n"
                       "  -h: Display this help message\n");
                return 0;
            default:
                printf("Help:\n"
                       "  -f <filename>: Specify the output filename (default: buckets.bin)\n"
                       "  -p <num_records>: Number of records to print from head\n"
                       "  -r <num_records>: Number of records to print from tail\n"
                       "  -d <bool>: Enable debug mode\n"
                       "  -v <bool>: Verify order of records\n"
                       "  -b <bool>: Verify hashes of records\n"
                       "  -m <memory_mb>: Set memory size in MB (default: 16MB)\n"
                       "  -s <file_size_mb>: Set file size in MB (default: 1024MB)\n"
                       "  -t <num_threads_hash>: Set number of threads for hashing (default: 1)\n"
                       "  -o <num_threads_sort>: Set number of threads for sorting (default: 1)\n"
                       "  -i <num_threads_write>: Set number of threads for writing (default: 1)\n"
                       "  -h: Display this help message\n");
                return 0;
        }
    }
    
    if (debug) {
        printf("NUM_THREADS_HASH=%d\n", num_threads_hash);
        printf("NUM_THREADS_SORT=%d\n", num_threads_sort);
        printf("NUM_THREADS_WRITE=%d\n", num_threads_write);
        printf("FILENAME=%s\n", filename);
        printf("MEMORY_SIZE=%dMB\n", memory_mb);
        printf("FILESIZE=%dMB\n", file_size_mb);
        printf("RECORD_SIZE=%zuB\n", sizeof(Record));
        printf("HASH_SIZE=%dB\n", HASH_SIZE);
        printf("NONCE_SIZE=%dB\n", NONCE_SIZE);
        printf("BUCKETS=%d\n", NUM_BUCKETS);
    }

    int num_prefix_bytes = calc_prefix_bytes(NUM_BUCKETS);
    Bucket* buckets = generate_records(num_prefix_bytes);

    printf("Hashes and nonces successfully generated. Starting sorting...\n");

    sort_records(buckets, NUM_BUCKETS);

    printf("Sorting completed. Verifying records...\n");
    
    if (buckets == NULL) {
        fprintf(stderr, "Failed to generate records\n");
        return 1;
    }

    printf("Generated records successfully\n");

    int total_records = 0;

    for (int i = 0; i < NUM_BUCKETS; i++) {
        //printf("Bucket %d:\n", i + 1);
        total_records += buckets[i].record_count;
        for (int j = 0; j < buckets[i].record_count - 1; j++) {
            if (memcmp(buckets[i].records[j].hash, buckets[i].records[j + 1].hash, HASH_SIZE) > 0) {
                printf("Records in bucket %d are not sorted by hash!\n", i + 1);
                break;
            }
        }
        //printf("\n");
    }

    printf("Total prefix bytes needed: %d\n", calc_prefix_bytes(NUM_BUCKETS));
    printf("Total records generated: %d\n", total_records);

    printf("Dumping buckets to file...\n");
    if (dump_buckets(buckets, NUM_BUCKETS, "buckets.bin") != 0) {
        fprintf(stderr, "Failed to dump buckets to file.\n");
    }
    printf("Buckets dumped successfully.\n");

    free(buckets);
    return 0;
}

