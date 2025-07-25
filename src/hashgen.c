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
    bool debug = false;
    int memory_mb = 16;
    int file_size_mb = 1024;
    int num_threads_hash = 1;
    int num_threads_sort = 1;
    int num_threads_write = 1;
    bool in_memory = false;
    int opt;

    while (( opt = getopt(argc, argv, "f:d:m:s:t:o:i:k:h")) != -1) {
        switch (opt) {
            case 'f':
                filename = optarg;
                break;
            case 'd':
                debug = strcmp(optarg, "true") == 0 || strcmp(optarg, "1") == 0;
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
            case 'k':
                in_memory = strcmp(optarg, "true") == 0 || strcmp(optarg, "1") == 0;
                break;
            case 'h':
                printf("Help:\n"
                       "  -f <filename>: Specify the output filename (default: buckets.bin)\n"
                       "  -d <bool>: Enable debug mode\n"
                       "  -m <memory_mb>: Set memory size in MB (default: 1024MB)\n"
                       "  -s <file_size_mb>: Set file size in MB (default: 1024MB)\n"
                       "  -t <num_threads_hash>: Set number of threads for hashing (default: 1)\n"
                       "  -o <num_threads_sort>: Set number of threads for sorting (default: 1)\n"
                       "  -i <num_threads_write>: Set number of threads for writing (default: 1)\n"
                       "  -k <bool> enable sorting and storing all in memory\n"
                       "  -h: Display this help message\n");
                return 0;
            default:
                printf("Help:\n"
                       "  -f <filename>: Specify the output filename (default: buckets.bin)\n"
                       "  -d <bool>: Enable debug mode\n"
                       "  -m <memory_mb>: Set memory size in MB (default: 16MB)\n"
                       "  -s <file_size_mb>: Set file size in MB (default: 1024MB)\n"
                       "  -t <num_threads_hash>: Set number of threads for hashing (default: 1)\n"
                       "  -o <num_threads_sort>: Set number of threads for sorting (default: 1)\n"
                       "  -i <num_threads_write>: Set number of threads for writing (default: 1)\n"
                       "  -k <bool> enable sorting and storing all in memory\n"
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
        printf("BUCKETS=%lld\n", NUM_BUCKETS);
    }

    int num_prefix_bytes = calc_prefix_bytes(NUM_BUCKETS);

    int mb_per_batch = (sizeof(Bucket) * NUM_BUCKETS) / (1024 * 1024);

    double start_time = omp_get_wtime();
    double last_print_time = omp_get_wtime();
    if ((mb_per_batch > memory_mb) || (in_memory && NUM_BATCHES > 1)){ //Check if your dumps use too much memory
        printf("Too much memory per bucket dump or too little bucket space for in memory: %d\n",mb_per_batch);
        return 1;
    }
    
    uint8_t nonce[NONCE_SIZE] = {0};
    size_t records_generated = 0;
    size_t records_per_batch = NUM_BUCKETS * MAX_RECORDS_PER_BUCKET;

    FILE* out = fopen(TEMP_FILE, "wb");
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
    
    while (records_generated < NUM_RECORDS) { // Keep generating records until we hit the amount we were going for 
        size_t this_batch = records_per_batch;

        if (NUM_RECORDS - records_generated < this_batch){
            this_batch = NUM_RECORDS - records_generated;
        }

        
        generate_records(nonce, num_prefix_bytes, buckets, this_batch, &last_print_time, records_generated, debug);

        for (size_t i = 0; i < this_batch; ++i) { //Keep the nonce updated
            increment_nonce(nonce, NONCE_SIZE);
        }

        if (!in_memory){
            if (dump_buckets(buckets, NUM_BUCKETS, TEMP_FILE) != 0) {
                fprintf(stderr, "Failed to dump records\n");
                free(buckets);
                return 1;
            }
        }
        records_generated += this_batch;
    }

        if (in_memory) {
            sort_buckets_in_memory(buckets, filename);
            free(buckets);
        }
        else{
            free(buckets);
            merge_and_sort_buckets(TEMP_FILE,filename,num_threads_sort);
        }
    
        FILE* out_final = fopen(filename, "rb+");
        if (out_final) {
            fflush(out_final);
            fsync(fileno(out_final));
            fclose(out_final);
        }

        double total_time = omp_get_wtime() - start_time;
        double mhps = (NUM_RECORDS / 1e6) / total_time;
        double mbps = ((NUM_RECORDS * sizeof(Record)) / 1e6) / total_time;

        printf("Completed %d MB file %s in %.2f seconds : %.2f MH/s %.2f MB/s\n", file_size_mb, filename, total_time, mhps, mbps);
        return 0;
    }

