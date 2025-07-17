#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include "../BLAKE3/c/blake3.h"
#include "../include/pos.h"

int verify_record(const Record* record) {
    uint8_t test_hash[HASH_SIZE];
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);
    blake3_hasher_update(&hasher, record->nonce, NONCE_SIZE);
    blake3_hasher_finalize(&hasher, test_hash, HASH_SIZE);

    return memcmp(test_hash, record->hash, HASH_SIZE) == 0;
}

int main() {
    Record* records = generate_records();
    if (records == NULL) {
        fprintf(stderr, "Failed to generate records\n");
        return 1;
    }

    printf("Generated records successfully\n");

    for (int i = 0; i < NUM_RECORDS; i++) {
        printf("Record %d: ", i + 1);
        for (int j = 0; j < HASH_SIZE; j++) {
            printf("%02x", records[i].hash[j]);
        }
        printf(" Nonce: ");
        for (int j = 0; j < NONCE_SIZE; j++) {
            printf("%02x", records[i].nonce[j]);
        }

        if (!verify_record(&records[i])) {
            printf("INVALID HASH!");
        }

        printf("\n");
    }

    printf("Total bytes stored: %u\n", HASH_SIZE * NUM_RECORDS);
    free(records);
    return 0;
}
