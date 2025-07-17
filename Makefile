CC = gcc
CFLAGS = -Wall -O2 -IBLAKE3/c

SRC = src/main.c src/pos.c \
      BLAKE3/c/blake3.c \
      BLAKE3/c/blake3_dispatch.c \
      BLAKE3/c/blake3_portable.c \
      BLAKE3/c/blake3_sse2_x86-64_unix.S \
      BLAKE3/c/blake3_sse41_x86-64_unix.S \
      BLAKE3/c/blake3_avx2_x86-64_unix.S \
      BLAKE3/c/blake3_avx512_x86-64_unix.S

OUT = pos_hash

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

run: $(OUT)
	./$(OUT)

clean:
	rm -f $(OUT)
