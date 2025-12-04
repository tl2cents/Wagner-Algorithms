## Benchmarks

**System: Intel i7-13700K with 64GB RAM running Ubuntu 22.04.5 LTS on WSL2**

* **CPU**: x86-64 processor with AVX2 support (required for BLAKE2b hashing)
* **OS**: Linux (Ubuntu 20.04 LTS or later recommended), NOT tested on MacOS or Windows natively.
* **Compiler**: GCC 9.0+ with C++20 support



> Running Benchmark Comparisons of Equihash(200, 9):
``` bash
$./run.sh --iters 100 --seed 0 --variant 144_5
```

| Algorithm      | Sol/s | Avg RunTime (s) | Total solutions | Peak RSS (kB) | Peak USS (MB) |
| -------------- | ----: | --------------: | --------------: | ------------: | ------------: |
| CIP            |  0.23 |            8.50 |             198 |       1776052 |       1732.97 |
| CIP-PR         |  0.08 |           25.24 |             198 |        725236 |        706.78 |
| CIP-EM         |  0.22 |            9.17 |             198 |        726052 |        707.56 |
| Tromp-Baseline |  0.20 |            9.20 |             190 |       2633344 |       2569.71 |

> Running Advanced Post-Retrieval of Equihash(200, 9):
``` bash
./run_apr_curve.sh --nk 200_9 --seed 0 --iters 1000
```

| Height | Avg RunTime (s) | Solutions | Sol/s | Peak USS (MB) |
| -----: | --------------: | --------: | ----: | ------------: |
|    h=0 |            0.93 |      1875 |  2.02 |         123.9 |
|    h=1 |            1.15 |      1875 |  1.62 |         110.6 |
|    h=2 |            1.45 |      1875 |  1.30 |          99.8 |
|    h=3 |            1.79 |      1875 |  1.04 |          86.5 |
|    h=4 |            2.22 |      1875 |  0.85 |          74.2 |
|    h=5 |            2.70 |      1875 |  0.69 |          61.8 |
|    h=6 |            3.27 |      1875 |  0.57 |          60.8 |
|    h=7 |            3.89 |      1875 |  0.48 |          59.6 |
|    h=8 |            4.55 |      1875 |  0.41 |          56.9 |


> Running Advanced Post-Retrieval of Equihash(144, 5):
``` bash
./run_apr_curve.sh --nk 144_5 --seed 0 --iters 300
```

| Height | Avg RunTime (s) | Solutions | Sol/s | Peak USS (MB) |
| -----: | --------------: | --------: | ----: | ------------: |
|    h=0 |            8.66 |       617 |  0.24 |        1453.5 |
|    h=1 |           11.52 |       617 |  0.18 |        1197.2 |
|    h=2 |           15.48 |       617 |  0.13 |         939.5 |
|    h=3 |           20.35 |       617 |  0.10 |         713.2 |
|    h=4 |           25.87 |       617 |  0.08 |         706.8 |


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