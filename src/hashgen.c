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

    int mb_per_batch = (sizeof(Bucket) * NUM_BUCKETS) / (1024 * 1024);

    double last_print_time = omp_get_wtime();

    if (mb_per_batch > memory_mb){
        printf("Too much memory per bucket dump: %d\n",mb_per_batch);
        return 1;
    }
    uint8_t nonce[NONCE_SIZE] = {0};
    size_t records_generated = 0;
    size_t records_per_batch = NUM_BUCKETS * MAX_RECORDS_PER_BUCKET;

    FILE* out = fopen(filename, "wb");
    if (!out) {
        perror("Failed to open output file");
        return 1;
    }
    fclose(out);

    Bucket* buckets = calloc(NUM_BUCKETS, sizeof(Bucket));

    if (!buckets) {
        fprintf(stderr, "Failed to allocate memory for buckets\n");
        return 1;
    }
    omp_set_num_threads(num_threads_hash);
    
    while (records_generated < NUM_RECORDS) {
        size_t this_batch = records_per_batch;

        if (NUM_RECORDS - records_generated < this_batch){
            this_batch = NUM_RECORDS - records_generated;
        }

        
        generate_records(nonce, num_prefix_bytes, buckets, this_batch, &last_print_time, records_generated);
        //omp_set_num_threads(num_threads_sort);
        //sort_records(buckets, NUM_BUCKETS);

        for (size_t i = 0; i < this_batch; ++i) {
            increment_nonce(nonce, NONCE_SIZE);
        }

        if (dump_buckets(buckets, NUM_BUCKETS, filename) != 0) {
            fprintf(stderr, "Failed to dump records\n");
            free(buckets);
            return 1;
        }
        records_generated += this_batch;
    }
    
        free(buckets);
        printf("Generated records successfully\n");
        return 0;
    }

