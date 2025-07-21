CC = gcc
CFLAGS = -Wall -O2 -IBLAKE3/c

HASH_SRC = src/hashgen.c src/pos.c \
      BLAKE3/c/blake3.c \
      BLAKE3/c/blake3_dispatch.c \
      BLAKE3/c/blake3_portable.c \
      BLAKE3/c/blake3_sse2_x86-64_unix.S \
      BLAKE3/c/blake3_sse41_x86-64_unix.S \
      BLAKE3/c/blake3_avx2_x86-64_unix.S \
      BLAKE3/c/blake3_avx512_x86-64_unix.S

LOOKUP_SRC = src/lookup.c src/pos.c \
      BLAKE3/c/blake3.c \
      BLAKE3/c/blake3_dispatch.c \
      BLAKE3/c/blake3_portable.c \
      BLAKE3/c/blake3_sse2_x86-64_unix.S \
      BLAKE3/c/blake3_sse41_x86-64_unix.S \
      BLAKE3/c/blake3_avx2_x86-64_unix.S \
      BLAKE3/c/blake3_avx512_x86-64_unix.S

HASH_VERIFY_SRC = src/hashverify.c src/pos.c \
      BLAKE3/c/blake3.c \
      BLAKE3/c/blake3_dispatch.c \
      BLAKE3/c/blake3_portable.c \
      BLAKE3/c/blake3_sse2_x86-64_unix.S \
      BLAKE3/c/blake3_sse41_x86-64_unix.S \
      BLAKE3/c/blake3_avx2_x86-64_unix.S \
      BLAKE3/c/blake3_avx512_x86-64_unix.S

HASH_OUT = hashgen

HASH_VERIFY_OUT = hashverify

LOOKUP_OUT = lookup_build


all: $(HASH_OUT) $(HASH_VERIFY_OUT) $(LOOKUP_OUT)

$(HASH_OUT): $(HASH_SRC)
	$(CC) $(CFLAGS) -o $@ $^

$(HASH_VERIFY_OUT): $(HASH_VERIFY_SRC)
	$(CC) $(CFLAGS) -o $@ $^

$(LOOKUP_OUT): $(LOOKUP_SRC)
	$(CC) $(CFLAGS) -o $@ $^

run-hashgen: $(HASH_OUT)
	./$(HASH_OUT)

run-hashverify: $(HASH_VERIFY_OUT)
	./$(HASH_VERIFY_OUT)

run-lookup: $(LOOKUP_OUT)
	./$(LOOKUP_OUT)

clean:
	rm -f $(HASH_OUT) $(HASH_VERIFY_OUT) $(LOOKUP_OUT)
