#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include <omp.h>

#include "../BLAKE3/c/blake3.h"
#include "../include/pos.h"

static size_t total_bucket_flushes = 0;

void increment_nonce(uint8_t *nonce, size_t nonce_size){
    for (size_t i = 0; i < nonce_size; i++) {
        if (++nonce[i] != 0) {
            break;
        }
    }
}

void generate_records(const uint8_t* starting_nonce, int num_prefix_bytes, Bucket* buckets, size_t records_batch, double* last_print, size_t records_generated) {
    omp_lock_t bucket_locks[NUM_BUCKETS];

    size_t total_flushes = NUM_BATCHES * NUM_BUCKETS;
    static double start_time = 0;
    if (start_time == 0) start_time = omp_get_wtime();
    static int print_count = 0;

    for (int i = 0; i < NUM_BUCKETS; i++) {
        omp_init_lock(&bucket_locks[i]);
        buckets[i].record_count = 0;
    }

    #pragma omp parallel
    {
        blake3_hasher hasher;
        uint8_t local_nonce[NONCE_SIZE];
        memcpy(local_nonce, starting_nonce, NONCE_SIZE);

        int tid = omp_get_thread_num();
        int nthreads = omp_get_num_threads();

        size_t start = (records_batch * tid) / nthreads;
        size_t end   = (records_batch * (tid + 1)) / nthreads;

        for (size_t i = 0; i < start; ++i) {
            increment_nonce(local_nonce, NONCE_SIZE);
        }

        for (size_t i = start; i < end; i++) {
            uint8_t hash[HASH_SIZE];
            Record record;

            blake3_hasher_init(&hasher);
            blake3_hasher_update(&hasher, local_nonce, NONCE_SIZE);
            blake3_hasher_finalize(&hasher, hash, HASH_SIZE);

            uint32_t prefix = 0;
            for (int j = 0; j < num_prefix_bytes; j++) {
                prefix = (prefix << 8) | hash[j];
            }

            uint32_t bucket_i = ((uint64_t)prefix * NUM_BUCKETS) >> (num_prefix_bytes * 8);
            if (bucket_i >= NUM_BUCKETS) bucket_i = NUM_BUCKETS - 1;

            memcpy(record.hash, hash, HASH_SIZE);
            memcpy(record.nonce, local_nonce, NONCE_SIZE);

            omp_set_lock(&bucket_locks[bucket_i]);
            if (buckets[bucket_i].record_count < MAX_RECORDS_PER_BUCKET) {
                buckets[bucket_i].records[buckets[bucket_i].record_count++] = record;
            }
            omp_unset_lock(&bucket_locks[bucket_i]);

            increment_nonce(local_nonce, NONCE_SIZE);
            double now = omp_get_wtime();
            if (tid == 0) {
                double now = omp_get_wtime();
                if (now - *last_print >= 0.5) {
                    double elapsed = now - start_time;
                    double percent = (100.0 * (records_generated + i - start)) / NUM_RECORDS;
                    double eta = elapsed * (NUM_RECORDS - (records_generated + i - start)) / ((records_generated + i - start) + 1e-5);
                    double mb_done = total_flushes * MAX_RECORDS_PER_BUCKET * sizeof(Record) / 1e6;
                    double mb_per_sec = mb_done / elapsed;

                    print_count++;
                    printf("[%d][HASHGEN]: %.2f%% completed, ETA %.1f seconds, %zu/%zu flushes, %.1f MB/sec\n",
                        print_count, percent, eta, total_flushes, total_flushes, mb_per_sec);
                    fflush(stdout);
                    *last_print = now;
                }
            }
        }
    }

    for (int i = 0; i < NUM_BUCKETS; i++) {
        omp_destroy_lock(&bucket_locks[i]);
    }
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

    #pragma omp atomic
    total_bucket_flushes += num_buckets;

    return 0;
}


int compare_records(const void* a, const void* b) {
    const Record* record_a = (const Record*)a;
    const Record* record_b = (const Record*)b;
    return memcmp(record_a->hash, record_b->hash, HASH_SIZE);
}

void sort_records(Bucket* buckets, size_t num_buckets) {
    static int flush_print_every = 1600;
    static double start_time = 0;
    static size_t total_flushes = 0;

    if (start_time == 0) {
        start_time = omp_get_wtime();
    }

    #pragma omp parallel
    {
        size_t local_flushes = 0;

        #pragma omp for schedule(dynamic)
        for (size_t i = 0; i < num_buckets; i++) {
            Bucket* bucket = &buckets[i];
            if (bucket->record_count > 1) {
                qsort(bucket->records, bucket->record_count, sizeof(Record), compare_records);
            }

            #pragma omp atomic
            total_flushes++;

            local_flushes++;
            if (local_flushes % flush_print_every == 0) {
                double now = omp_get_wtime();
                double elapsed = now - start_time;
                double percent = (100.0 * total_flushes) / num_buckets;
                double remaining = elapsed * (num_buckets - total_flushes) / (total_flushes + 1e-5);
                double mb_done = total_flushes * (MAX_RECORDS_PER_BUCKET * sizeof(Record)) / 1e6;
                printf("[SORT]: %.2f%% completed, ETA %.1f seconds, %zu/%zu flushes, %.1f MB/sec\n",
                    percent, remaining, total_flushes, num_buckets, mb_done / elapsed);
                fflush(stdout);
            }
        }
    }

    double total_time = omp_get_wtime() - start_time;
    double mb = total_flushes * (MAX_RECORDS_PER_BUCKET * sizeof(Record)) / 1e6;
    printf("[SORT]: 100.00%% completed, Total time: %.2fs, Throughput: %.2f MB/sec\n",
        total_time, mb / total_time);
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