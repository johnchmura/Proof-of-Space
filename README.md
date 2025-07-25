USAGE: 

Make File:

make: Compiles all source files to be run via ./vault, ./hashgen, ./hashverify
make clean: Clears out the compiled files
K=N: Compiles all source files and sets the number of records to the the value 2^N.
B=N: Sets number of buckets to 2^N
R=N: Sets number of records per bucket to N

Example usage - make K=32 B=17 R=11 (4GB of memory) (64GB filesize)

These are the only ways to change these numbers without going and changing values in the code.

Running the compiled files:

./hashgen, ./hashverify, ./vault


./hashgen commands:

  -f <filename>: Specify the output filename (default: buckets.bin)
  -d <bool>: Enable debug mode
  -m <memory_mb>: Set memory size in MB (default: 16MB)
  -s <file_size_mb>: Set file size in MB (default: 1024MB)
  -t <num_threads_hash>: Set number of threads for hashing (default: 1)
  -o <num_threads_sort>: Set number of threads for sorting (default: 1)
  -i <num_threads_write>: Set number of threads for writing (default: 1)
  -k <bool> Specify if you want to run in memory only
  -h: Display this help message

./hashverify commands:

  -f <filename>: Specify the output filename  
  -p <num_records>: Number of records to print from head  
  -r <num_records>: Number of records to print from tail  
  -v <bool>: Verify hashes from file, off with false, on with true  
  -d <bool>: Enable debug mode  
  -h: Display this help message  

./vault commands:

  -f <filename>: Specify the output filename  
  -c <num_searches>: Number of searches to perform  
  -l <prefix_bytes>: Number of prefix bytes to use  
  -h: Display this help message  
