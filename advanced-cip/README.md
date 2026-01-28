
## Quick Start Guide

- **Tools**: cmake, make, GCC 9+ or Clang 10+ (must support C++17)
- **Architecture**: x86-64 CPU with AVX2 support.

1. (Recommended) Build binaries using CMake:
    ```bash
    mkdir build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release
    make -j$(nproc)
    ```
2. Direct compilation example for Equihash(144,5):
    ``` bash
    g++ -I./include -I../third_party/kxsort -I../third_party/blake2-avx2 src/eq144_5/apr_main.cpp src/eq144_5/apr_alg.cpp ../third_party/blake2-avx2/blake2bip.c ../third_party/blake/blake2b.cpp -o apr144_5 -std=c++17 -O3 -march=native -mavx2 -funroll-loops -Wall -Wextra -Wno-unused-variable
    ```
3. (**Recommended**) We also provide automated bash scripts ([run_bench.sh](./run_bench.sh), [run_apr.sh](./run_apr.sh), [run_merge.sh](./run_merge.sh)) to run the experiments in the paper. These scripts will automatically build the binaries if they are not available. See [logs/README.md](./logs/README.md) for more details.

## Benchmarks

**System: Intel i7-13700K with 64GB RAM running Ubuntu 22.04.5 LTS on WSL2**

* **CPU**: x86-64 processor with AVX2 support (required for BLAKE2b hashing)
* **OS**: Linux (Ubuntu 20.04 LTS or later recommended), NOT tested on MacOS or Windows natively.
* **Compiler**: GCC 9.0+ with C++20 support

The results are logged in [logs/](./logs/) directory.

### Run In-Place Merge Benchmark

To reproduce the results in *Table~4* from the paper, run:

``` bash
./run_merge.sh 0 2000 std
./run_merge.sh 0 2000 kx
```

Benchmark results for the first merge in `Equihash(200,9)` using `kxsort` vs `std::sort` are as follows:

| Metric | kx (Avg) | kx (Max) | std (Avg) | std (Max) |
| :--- | :---: | :---: | :---: | :---: |
| Sort time (s) | 0.0591 | 0.0646 | 0.1265 | 0.1322 |
| Linear scan (s) | 0.0283 | 0.0338 | 0.0287 | 0.0313 |
| Temp buffer (items) | 65.86 | 120 | 65.86 | 120 |
| Peak USS (MB) | N/A | 51.59 | N/A | 51.58 |
| Peak RSS (MB) | N/A | 53.13 | N/A | 53.25 |

**Note**: The script [merge_in_place.py](./merge_in_place.py) clearly shows the core idea of the in-place merge implementation. We also add some notes and useful links that may further improve the performance. Note that for ease of implementation, our practical version uses a templated [kx-sort](https://github.com/voutcn/kxsort) with a fixed bucket size of 256. It is only about 2× faster than `std::sort`, which means there is still significant room for optimization.

### Run Benchmark Comparisons

> For Equihash(144, 5):

``` bash
$./run_bench.sh --nk 144_5 --seed 0 --iters 100 
```

| Algorithm      | Sol/s | Avg RunTime (s) | Total solutions | Peak USS (MB) |
| -------------- | ----: | --------------: | --------------: | ------------: |
| CIP            |  0.23 |            8.45 |             198 |       1453.12 |
| CIP-PR         |  0.08 |           24.94 |             198 |        706.79 |
| CIP-EM         |  0.22 |            9.05 |             198 |        707.54 |
| CIP-APR        |  0.10 |           19.77 |             198 |        713.68 |
| Tromp-Baseline |  0.20 |            9.11 |             190 |       2569.69 |

> For Equihash(200, 9):

``` bash
$./run_bench.sh --nk 200_9 --seed 0 --iters 1000 
```

| Algorithm      | Sol/s | Avg RunTime (s) | Total solutions | Peak USS (MB) |
| -------------- | ----: | --------------: | --------------: | ------------: |
| CIP            |  2.06 |            0.91 |            1875 |        123.94 |
| CIP-PR         |  0.43 |            4.41 |            1875 |         58.24 |
| CIP-EM         |  1.89 |            0.99 |            1875 |         58.43 |
| CIP-APR        |  0.71 |            2.66 |            1875 |         62.98 |
| Tromp-Baseline |  6.12 |            0.29 |            1827 |        144.86 |


**Remarks**

* **Sol/s**:  Solutions per second
* **Peak RSS**: Peak Resident Set Size in kilobytes - total memory pages in RAM 
* **Peak USS**: Peak Unique Set Size in kilobytes - memory exclusively used by the process, more accurate than RSS 
* **CIP-APR** refers to the advanced post-retrieval technique with optimal switching height: 3 for Equihash(144,5) and 5 for Equihash(200,9).
* **CIP-EM** requires external memory which in practice may be highly advantageous for ASIC implementations.
* **Tromp-Baseline**: see https://github.com/tromp/equihash
* The BLAKE2b implementation in Tromp's original code was **slightly modified** based on [this issue comment](https://github.com/Raptor3um/raptoreum/issues/48#issuecomment-969125200) to ensure successful compilation. **Configuration**: $2^{10}$ buckets, 145MB allocated, 4-way BLAKE2b

### Run Advanced Post-Retrieval (APR) Benchmarks

> Run Advanced Post-Retrieval of Equihash(200, 9):
``` bash
./run_apr_curve.sh --nk 200_9 --seed 0 --iters 1000
```

| Height | Avg RunTime (s) | Solutions | Sol/s | Peak USS (MB) |
| -----: | --------------: | --------: | ----: | ------------: |
|    h=0 |            0.91 |      1875 |  2.05 |         124.3 |
|    h=1 |            1.13 |      1875 |  1.66 |         112.3 |
|    h=2 |            1.41 |      1875 |  1.33 |         100.2 |
|    h=3 |            1.74 |      1875 |  1.08 |          88.1 |
|    h=4 |            2.14 |      1875 |  0.88 |          75.8 |
|    h=5 |            2.61 |      1875 |  0.72 |          63.4 |
|    h=6 |            3.17 |      1875 |  0.59 |          61.2 |
|    h=7 |            3.77 |      1875 |  0.50 |          61.3 |
|    h=8 |            4.41 |      1875 |  0.43 |          58.5 |


> Run Advanced Post-Retrieval of Equihash(144, 5):
``` bash
./run_apr_curve.sh --nk 144_5 --seed 0 --iters 100
```

| Height | Avg RunTime (s) | Solutions | Sol/s | Peak USS (MB) |
| -----: | --------------: | --------: | ----: | ------------: |
|    h=0 |            8.42 |       198 |  0.24 |        1453.1 |
|    h=1 |           11.05 |       198 |  0.18 |        1196.8 |
|    h=2 |           14.82 |       198 |  0.13 |         940.5 |
|    h=3 |           19.37 |       198 |  0.10 |         713.7 |
|    h=4 |           24.74 |       198 |  0.08 |         706.8 |


### Detailed System Configuration

* **Model**: 13th Gen Intel(R) Core(TM) i7-13700K
* **Base Frequency**: ~3.4 GHz (average measured: 3417.6 MHz)
* **Memory**: DDR5, Samsung, Read/Write Speed 5600 MT/s
* **Filesystem**: NTFS (mounted via Ubuntu 22.04.5 LTS, WSL2)
* **SSD**: Fanxiang 690Q with **read speeds up to** **5200 MB/s** **and write speeds up to** **4700 MB/s**

**Note:** Due to WSL2 virtual disk limitation, the actual read and write speeds may be slightly lower.


## Remarks

1. As the runner-up in the Zcash miner optimization contest (see https://zcashminers.org/submissions), [Tromp’s implementation](https://github.com/tromp/equihash) of $`\textsf{Equihash}(200,9)`$ incorporates numerous carefully engineered optimizations, including the choice of near-optimal bucket sizes, layer-specific tuning of $`\textsf{merge}`$ functions, and compact index-pointer representations. The winning implementation applied even more aggressive low-level optimizations, such as hand-crafted assembly and architecture-specific tuning.
In contrast, our work does not aim to produce a highly optimized or practically competitive $`\textsf{Equihash}`$ solver. This performance gap is therefore **an engineering issue rather than a conceptual one**. Our implementation is sufficient to demonstrate the effectiveness of the new algorithmic techniques proposed in this work, and the results clearly validate the improvements our methods bring.

1. In theory, beyond the optimal switching height (e.g., h = 6, 7, 8 for Equihash(200,9)), the peak memory should no longer decrease. However, in practice we observe small fluctuations in the peak memory, which are attributable to C++ lazy memory allocation and system-dependent compiler optimizations. To date, this phenomenon has only been observed in our WSL2 testing environment. In our implementation, the maximum list size at each layer is set to $N = 2,200,000$, which is slightly larger than $2^{21} = 2,097,152$. As a result, there is inherent unused capacity. For example, Layer~0 always contains exactly $2^{21}$ elements, and the remaining allocated space is never allocated due to C++ lazy memory allocation (if we do not access it later). Specifically (see [src/eq200_9/apr_main.cpp#L278](./src/eq200_9/apr_main.cpp#L278)), we use `std::memset(base, 0, total_mem) ` to prevent lazy allocation. However, this statement may be optimized away by the compiler, causing untouched memory pages not to be physically allocated. Combined with our memory layout (see [src/eq144_5/apr_alg.cpp#L356](./src/eq144_5/apr_alg.cpp#L356)), this results in the apparent memory variation seen in Fig.~1.
