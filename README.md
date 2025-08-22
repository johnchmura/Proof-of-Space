# Proof-of-Space Generator (C + OpenMP + BLAKE3)

This project implements a parallel proof-of-space generator in C, using BLAKE3 hashing and OpenMP for multithreading. It generates, sorts, and verifies large datasets with bucketed storage and prefix-based lookup support.

Key Features:

* Multithreaded hashing, sorting, and writing (scales across CPU cores)
* Bucketed storage with fixed-size padding for fast lookups
* Prefix search via `vault` for hash verification
* Configurable memory, file size, and bucket layout from the command line

Installation:

1. Clone BLAKE3 into the project directory:

   * HTTPS: [https://github.com/BLAKE3-team/BLAKE3.git](https://github.com/BLAKE3-team/BLAKE3.git)
   * SSH: [git@github.com](mailto:git@github.com)\:BLAKE3-team/BLAKE3.git
   * GitHub CLI: gh repo clone BLAKE3-team/BLAKE3

2. Clone this repo:

```
git clone <your_repo_url>
cd <your_repo_name>
```

3. Build using make (example):

```
make K=32 B=17 R=11  # 2^32 records, 2^17 buckets, 11 records per bucket
```

This creates three executables:

* `hashgen` – generate, hash, and sort records
* `hashverify` – verify and inspect files
* `vault` – prefix-based search

`make clean` removes compiled files.

Usage:

hashgen – Generate Proof-of-Space File:

```
./hashgen -f buckets.bin -m 4096 -s 65536 -t 8 -o 8 -i 4
```

Options:

* `-f <filename>`: Output filename (default: buckets.bin)
* `-m <memory_mb>`: Memory in MB (default: 16)
* `-s <file_size_mb>`: File size in MB (default: 1024)
* `-t <threads>`: Threads for hashing (default: 1)
* `-o <threads>`: Threads for sorting (default: 1)
* `-i <threads>`: Threads for writing (default: 1)
* `-k`: In-memory only mode (off by default)
* `-d`: Debug mode (off by default)
* `-h`: Show help

hashverify – Inspect and Verify Files:

```
./hashverify -f buckets.bin -v true -p 10
```

Options:

* `-f <filename>`: Target file
* `-p <num_records>`: Print first N records
* `-r <num_records>`: Print last N records
* `-v <bool>`: Verify hashes (true/false)
* `-d`: Debug mode
* `-h`: Help

vault – Prefix Search:

```
./vault -f buckets.bin -c 1000 -l 4
```

Options:

* `-f <filename>`: Target file
* `-c <num_searches>`: Number of random searches
* `-l <prefix_bytes>`: Prefix length in bytes
* `-h`: Help

Example Workflow:

```
make K=32 B=17 R=11
./hashgen -f buckets.bin -m 4096 -s 65536 -t 16 -o 16 -i 8
./hashverify -f buckets.bin -v true -p 5
./vault -f buckets.bin -c 100 -l 3
```

Generates a 64GB file with 2^32 records using 16 threads, verifies results, and performs prefix searches.

Performance:

* \~6–8× speedup with multithreading vs single-threaded
* Handles terabyte-scale datasets efficiently
* Scales near-linearly with CPU cores
