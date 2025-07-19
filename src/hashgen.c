#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include "../BLAKE3/c/blake3.h"
#include "../include/pos.h"

int main() {
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
