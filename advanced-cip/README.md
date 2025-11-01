# `Equihash(200,9)` CIP Proof-of-Concept Benchmark Results

This document contains benchmark results for **post retrieval** and **external memory caching** implementations for `Equihash(200, 9)`, corresponding to *Table 6* in our paper.



---

## Benchmarks

**Benchmarks conducted on: 2025-11-01**
**System: Intel i7-13700K with 64GB RAM running Ubuntu 22.04.5 LTS on WSL2**

---

### System Requirements

* **CPU**: x86-64 processor with AVX2 support (required for BLAKE2b hashing)
* **OS**: Linux (Ubuntu 20.04 LTS or later recommended), NOT tested on MacOS or Windows natively.
* **Compiler**: GCC 9.0+ with C++20 support



###  Configuration

The benchmarks were executed using the script `./run.sh` with the following parameters:

* **Iterations**: 2000 different nonces tested for each algorithm
* **Starting Seed**: 0
* **Seed Range**: 0-1999
* **Execution Mode**: Single-threaded
* **USS Monitor Interval**: 0.01 seconds

---

### Benchmark Results



| Algorithm      | Sol/s | Peak RSS (kB) | Peak USS (kB) | Peak USS (MB) |
| -------------- | ----- | ------------- | ------------- | ------------- |
| CIP            | 2.05  | 162,304       | 159,604       | 155.86        |
| CIP-PR         | 0.41  | 61,264        | 58,456        | 57.09         |
| CIP-EM         | 1.21  | 61,408        | 58,748        | 57.37         |
| CIP-APR        | 0.68  | 66,312        | 63,420        | 61.93         |
| CIP-EMC        | 1.00  | 73,856        | 71,168        | 69.50         |
| Tromp-Baseline | 8.94  | 150,272       | N/A           | N/A           |



**Remarks**

* **Sol/s**:  Solutions per second
* **Peak RSS**: Peak Resident Set Size in kilobytes - total memory pages in RAM 
* **Peak USS**: Peak Unique Set Size in kilobytes - memory exclusively used by the process, more accurate than RSS 
* All benchmarks were conducted in **single-threaded mode** despite the 12-core CPU available
* The BLAKE2b implementation in Tromp's original code was **slightly modified** based on [this issue comment](https://github.com/Raptor3um/raptoreum/issues/48#issuecomment-969125200) to ensure successful compilation. **Configuration**: $2^{10}$ buckets, 145MB allocated, 4-way BLAKE2b
* System was running on WSL2, which may introduce some performance overhead compared to native Linux
* Total of 2000 iterations across seed range 0-1999 ensured statistical significance of results



## Detailed System Configuration

### CPU Information

* **Model**: 13th Gen Intel(R) Core(TM) i7-13700K
* **Architecture**: x86-64
* **Base Frequency**: ~3.4 GHz (average measured: 3417.6 MHz)
* **Cache**: 30 MB (L3)
* **Virtualization**: VT-x enabled
* **Features**: AVX2 support (required for BLAKE2b hashing)

### Memory Configuration

* **Total RAM**: 64 GB (62 GiB)
* **Swap Space**: 16 GB
* **Type**: DDR5，Samsung, Read/Write Speed 5600 MT/s

### External Storage Configuration

* **External Storage**: Windows filesystem mounted via WSL2
* **Filesystem**: NTFS (mounted via WSL2)
* **SSD**: Fanxiang 690Q with **read speeds up to** **5200 MB/s** **and write speeds up to** **4700 MB/s**

**Note:** Due to WSL2 virtual disk limitation, the actual read and write speeds may be slightly lower.

### Operating System

* **OS**: Ubuntu 22.04.5 LTS (running on WSL2)
* **Kernel**: Linux 6.6.87.2-microsoft-standard-WSL2
* **Compiler**: GCC 15.1.0 with C++20 support



---

## How to Reproduce

To reproduce these benchmarks on your system:

```bash
./run.sh
```

The script will automatically:
1. Create a build directory if it doesn't exist
2. Compile all implementations using CMake and GCC
3. Run all benchmark tests (CIP, CIP-PR, CIP-EM, CIP-APR, CIP-EMC, Tromp-Baseline)
4. Monitor peak USS (Unique Set Size) for accurate memory measurements
5. Generate a formatted results table



Compile (if `cmake` not available):

``` bash
g++ -I./include -I./third_party/kxsort -I./third_party/blake2-avx2 src/apr_main.cpp src/apr_alg.cpp third_party/blake2-avx2/blake2bip.c third_party/blake/blake2b.cpp -o apr -std=c++17 -O3 -march=native -mavx2 -funroll-loops -Wall -Wextra -Wno-unused-variable
```



