# Inplace Merge Benchmark

To reproduce the results in *Table 5* from the paper, run:

```
./run_merge.sh 0 2000 std
./run_merge.sh 0 2000 kx
```

Benchmark results for the first merge in `Equihash(200,9)` using `kxsort` vs `std::sort` are as follows:

| Metric | kx (Avg) | kx (Max) | std (Avg) | std (Max) |
| :--- | :---: | :---: | :---: | :---: |
| Sort time (s) | 0.0593 | 0.0651 | 0.1256 | 0.1358 |
| Linear scan (s) | 0.0265 | 0.0329 | 0.0268 | 0.0333 |
| Temp buffer (items) | 66.16 | 120 | 66.16 | 120 |
| Peak USS (MB) | N/A | 51.55 | N/A | 51.59 |
| Peak RSS (MB) | N/A | 53.00 | N/A | 53.25 |

> The benchmarks were conducted on an Intel i7-13700K CPU with 64GB RAM running Ubuntu 22.04.5 LTS on WSL2 covering seeds from 0 to 1999. More details about the system configuration can be found in the next section.

In [merge_in_place.py](./merge_in_place.py), we provided a Python implementation of in-place merge, along with many optimization insights. Note that for ease of implementation, our practical version uses a templated [kx-sort](https://github.com/voutcn/kxsort) with a fixed bucket size of 256. It is only about 2× faster than `std::sort`, which means there is still significant room for optimization. For more efficient sorting algorithms, please refer to the links included in [merge_in_place.py](./merge_in_place.py).


# `Equihash(200,9)` CIP Proof-of-Concept Benchmark Results

This document contains benchmark results for **post retrieval** and **external memory caching** implementations for `Equihash(200, 9)`, corresponding to *Table 6* in our paper.

---

## Benchmarks

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


> Running with plain BLAKE2b:
``` bash
./run.sh --iters 2000 --seed 0
```


| Algorithm      | Sol/s | Peak RSS (kB) | Peak USS (kB) | Peak USS (MB) |
| -------------- | ----- | ------------- | ------------- | ------------- |
| CIP            | 2.12  | 162,304       | 159,560       | 155.82        |
| CIP-PR         | 0.44  | 61,184        | 59,640        | 58.24         |
| CIP-EM         | 1.93  | 61,440        | 60,080        | 58.67         |
| CIP-APR        | 0.74  | 66,048        | 64,628        | 63.11         |
| Tromp-Equi1    | 6.30  | 150,528       | 149,036       | 145.54        |


> Running with 4-way BLAKE2b:
``` bash
./run.sh -x4 --iters 2000 --seed 0
```
| Algorithm      | Sol/s | Peak RSS (kB) | Peak USS (kB) | Peak USS (MB) |
| -------------- | ----- | ------------- | ------------- | ------------- |
| CIP            | 2.27  | 162,304       | 160,952       | 157.18        |
| CIP-PR         | 0.51  | 61,056        | 59,660        | 58.26         |
| CIP-EM         | 2.06  | 61,576        | 58,748        | 57.37         |
| CIP-APR        | 0.88  | 66,048        | 64,568        | 63.05         |
| Tromp-Equix41  | 9.01  | 150,400       | 147,700       | 144.24        |


**Remarks**

* **Sol/s**:  Solutions per second
* **Peak RSS**: Peak Resident Set Size in kilobytes - total memory pages in RAM 
* **Peak USS**: Peak Unique Set Size in kilobytes - memory exclusively used by the process, more accurate than RSS 
* The BLAKE2b implementation in Tromp's original code was **slightly modified** based on [this issue comment](https://github.com/Raptor3um/raptoreum/issues/48#issuecomment-969125200) to ensure successful compilation. **Configuration**: $2^{10}$ buckets, 145MB allocated, 4-way BLAKE2b

> We note that the performance bottleneck of our current implementation lies in the templated sorting routine. As a result, the optimization of 4-way BLAKE2b provides only limited performance gains in our setting. In contrast, Tromp’s implementation employs a fully customized and highly optimized sorting algorithm, for which the 4-way BLAKE2b optimization yields a substantial improvement in benchmark performance.


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
* **Compiler**: GCC 15.1.0 with C++20 support

---

## How to Reproduce

To reproduce these benchmarks on your system. On Debian/Ubuntu you can install common packages with:

```bash
sudo apt update && sudo apt install -y build-essential cmake
```

The 4-way BLAKE2b SIMD build (`-x4` / `equix41`) requires a CPU with AVX2 support and a compiler that can emit AVX2 code (use `-mavx2` or `-march=native`).

```bash
./run.sh (-x4) --iters 2000 --seed 0
```

For `cip-apr`, you can sweep different switching heights with `--switch-h=N` (0 maps to plain CIP, 8 maps to PR; default 5).

Plain compile (if `cmake` not available):

``` bash
g++ -I./include -I./third_party/kxsort -I./third_party/blake2-avx2 src/apr_main.cpp src/apr_alg.cpp third_party/blake2-avx2/blake2bip.c third_party/blake/blake2b.cpp -o apr -std=c++17 -O3 -march=native -mavx2 -funroll-loops -Wall -Wextra -Wno-unused-variable
```
