# Wagner-Algorithms

New memory optimizations for Wagner's algorithms and Equihash accompanying the paper: "New Memory Optimizations of Wagner’s Algorithms via List Item Reduction". 

## New Memory Optimizations of Wagner's Algorithms

We propose two new techniques to optimize Wagner's algorithms:

- **Improved Index-Trimming Technique**: Reduces memory usage by trimming indexes for both the single-chain and $k$-tree algorithms implemented with index vectors.
- **Post-Retrival Technique**: Reduces memory usage by reconstructing the index pointers for single-chain algorithm implemented with index pointers. It's not memory-efficient for the $k$-tree algorithm.

All of these optimizations rely on our newly proposed in-place $`\textsf{merge}`$ framework. Theoretically, our techniques can reduce the peak memory usage of Wagner's algorithm by half (from $`2nN`$ to $`nN`$ bits) across most parameter settings, while incurring no more than a twofold time penalty. Under the hybrid framework, the memory footprint can be further reduced (below $`nN`$ bits) at the cost of additional computational overhead. 

<!-- 
For example, under the hybrid technique, the optimal parameter choices are as follows.
| (n,k)   | plain peak mem | peak mem | runtime     | switching height1 | switching height2 | peak layer |
|---------|---------------:|---------:|------------:|------------------:|------------------:|-----------:|
| (96, 5)  | 2^24.39       | 2^23.04 | 3.8 * T0   | 1                 | 3                 | 4          |
| (128,7) | 2^24.88       | 2^23.64 | 3.9 * T0   | 1                 | 4                 | 6          |
| (160,9) | 2^25.25       | 2^24.07 | 4.0 * T0   | 1                 | 5                 | 8          |
| (96, 3)  | 2^32.21       | 2^30.64 | 4.0 * T0   | 1                 | 2                 | 1          |
| (144,5) | 2^32.95       | 2^31.61 | 3.8 * T0   | 1                 | 3                 | 4          |
| (150,5) | 2^34.01       | 2^32.67 | 3.8 * T0   | 1                 | 3                 | 4          |
| (192,7) | 2^33.44       | 2^32.21 | 3.9 * T0   | 1                 | 4                 | 6          |
| (240,9) | 2^33.81       | 2^32.63 | 4.0 * T0   | 1                 | 5                 | 8          |
| (96, 2)  | 2^40.02       | 2^39.00 | 1.5 * T0   | 0                 | 1                 | 1          |
| (288,8) | 2^42.04       | 2^41.00 | 2.9 * T0   | 0                 | 5                 | 1          |
| (200,9) | 2^29.55       | 2^28.38 | 4.0 * T0   | 1                 | 5                 | 8          | 
-->


More estimators and resluts are available in [python-poc](./python-poc/). A lower–time-penalty implementation can be found in the next section.

## Implementations

Our implementation serves solely as a proof of concept and does not incorporate aggressive low-level optimizations. We also note that these optimizations may significantly impact the ASIC-resistance of existing blockchains that rely on $`\textsf{Equihash}`$. We therefore recommend that such blockchains reassess the memory bottlenecks of ASIC implementations across all $`\textsf{Equihash}`$ parameter settings. 

![apr-time-mem](./apr-time-mem.svg)

> Details are available in the directories [eq1445](./eq1445/).

### Equihash(144,5) Quick Benchmark

For the parameter setting $`\textsf{Equihash}(144, 5)`$, a subset of our optimization results is shown below (serving only as a proof of concept). Our implementations outperform Tromp's baseline implementation (CIP) in both time and memory usage.

| Algorithm      | Sol/s | Avg single run (s) | Total solutions | Peak USS (MB) |
| -------------- | -----:| ------------------: | ---------------:| --------------:|
| CIP            | 0.23  | 8.78               | 198             | 1732.92       |
| CIP-PR         | 0.08  | 25.83              | 198             | 707.20        |
| CIP-EM         | 0.21  | 9.27               | 198             | 707.53        |
| Tromp-Baseline | 0.22  | 8.32               | 190             | 2569.70       |

> Notes: "Avg single run (s)" is the average per-iteration runtime reported by the benchmark (for Tromp the total time was divided by 100 iterations to obtain the per-run average).



### Equihash(200,9) Quick Benchmark

For the parameter setting $`\textsf{Equihash}(200, 9)`$, a subset of our optimization results is shown below (serving only as a proof of concept).

| Algorithm      | Sol/s | Peak USS (MB) |
| -------------- | ----- | ------------- |
| CIP            | 2.12  | 155.82        |
| CIP-PR         | 0.44  | 58.24         |
| CIP-EM         | 1.93  | 58.67         |
| CIP-APR        | 0.74  | 63.11         |
| Tromp-Equi1    | 6.30  | 145.54        |

**Remark.** As the runner-up in the Zcash miner optimization contest (see https://zcashminers.org/submissions), Tromp’s implementation of $`\textsf{Equihash}(200,9)`$ incorporates numerous carefully engineered optimizations, including the choice of near-optimal bucket sizes, layer-specific tuning of $`\textsf{merge}`$ functions, and compact index-pointer representations. The winning implementation applied even more aggressive low-level optimizations, such as hand-crafted assembly and architecture-specific tuning.
In contrast, our work does not aim to produce a highly optimized or practically competitive $`\textsf{Equihash}`$ solver. This performance gap is therefore **an engineering issue rather than a conceptual one**. Our implementation is sufficient to demonstrate the effectiveness of the new algorithmic techniques proposed in this work, and the results clearly validate the improvements our methods bring.

> The current implementations of the sorting algorithm and the linear-scan procedure still have substantial room for optimization, which explains the noticeable performance gap between the standard $\textsf{CIP}$ implementation and Tromp’s implementation (CIP). Further details can be found in the directory [eq2009](./eq2009/).