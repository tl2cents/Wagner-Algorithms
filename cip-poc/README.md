# `Equihash(200,9)` CIP Proof-of-Concept

This directory contains proof-of-concept implementations for **post retrieval** and **external memory caching** in `Equihash(200, 9)`, corresponding to *Table 6* in our paper.

---

## System Requirements

* **CPU**: x86-64 processor with AVX2 support (required for BLAKE2b hashing)
* **OS**: Linux
* **Compiler**: GCC with C++20 support

---

## How to Run Benchmarks

Use the provided script to build the solver and run the benchmark suite:

```bash
./run.sh
```

The script automatically compiles the implementations and executes the benchmark tests.

---

## Benchmark Results

* **CPU**: AMD Ryzen 7 PRO 6850HS with Radeon Graphics (16 threads) @ 4.785 GHz
  * benchmarks were conducted in single-threaded mode
* **OS**: Ubuntu 24.04.2 LTS x86_64

*Benchmark data to be updated.*

| Algorithm        | Sol/s | PeakRSS (kB) |
|------------------|-------|---------------|
| CIP              | 1.27  | 161,468       |
| CIP-PR           | 0.24  | 75,092        |
| CIP-EM           | 1.07  | 74,752        |
| Tromp-Baseline   | 4.03  | 138,752       |


*Note:* The BLAKE2b implementation in Tromp’s original code was **slightly modified** based on [this issue comment](https://github.com/Raptor3um/raptoreum/issues/48#issuecomment-969125200) to ensure successful compilation.
