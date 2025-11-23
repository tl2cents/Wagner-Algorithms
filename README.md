# Wagner-Algorithms

New memory optimizations for Wagner's algorithms and Equihash accompanies the paper: "New Memory Optimizations of Wagner’s Algorithms via List Item Reduction". Wagner’s single-chain algorithm and the $k$-tree algorithm can be used to solve the generalized birthday problem and its regular variant, respectively. We define them as follows.

**Definition. *Loose Generalized Birthday Problem.*** $\textsf{LGBP}(n, K)$: Given a list $L$ containing random $n$-bit values, the loose generalized birthday problem involves finding $x_1, \ldots, x_K \in L$ such that 

$$
x_1 \oplus x_2 \oplus \cdots \oplus x_K = 0.
$$

**Definition. *Regular Generalized Birthday Problem.*** $\textsf{RGBP}(n, K)$: Given $K$ lists $L_1, \ldots, L_K$ containing random $n$-bit values, the strict generalized birthday problem involves finding $x_1 \in L_1, \ldots, x_K \in L_K$ such that 

$$
x_1 \oplus x_2 \oplus \cdots \oplus x_K = 0.
$$

> **Remark.** $\textsf{LGBP}(n, K)$ is also known as $\textsf{Equihash}$, which is a popular memory-hard proof-of-work based on the single-chain algorithm used in various cryptocurrencies.



## New Memory Optimizations of Wagner's Algorithms

We propose two new techniques to optimize Wagner's algorithms:

- **Improved Index-Trimming Technique**: Reduces memory usage by trimming indexes for both the single-chain and $k$-tree algorithms implemented with index vectors.
- **Post-Retrival Technique**: Reduces memory usage by reconstructing the index pointers for single-chain algorithm implemented with index pointers. It's not memory-efficient for the $k$-tree algorithm.

All of these optimizations rely on our newly proposed in-place $`\textsf{merge}`$ framework. Theoretically, our techniques can reduce the peak memory usage of Wagner's algorithm by half (from $`2nN`$ to $`nN`$ bits) across most parameter settings, while incurring no more than a twofold time penalty. Under the hybrid framework, the memory footprint can be further reduced (below $`nN`$ bits) at the cost of additional computational overhead.

For all optimization techniques, we provide Python-based proof-of-concept implementations to validate their theoretical correctness. In the subdirectory [python-poc](./python-poc/), we include concrete estimators that compute optimal parameter choices for Wagner’s algorithm under various memory constraints and parameter settings.  For example, under the index-trimming technique, the optimal parameter choices are as follows.

| (n,k)   | trimmed_length | peak_mem  | runtime      | switching_height1 | peak_mem1  | runtime1     | peak_layer1 | switching_height2 | peak_mem2  | runtime2     | peak_layer2 | activating_height |
|----------|----------------|-----------|--------------|-------------------:|-----------:|--------------:|------------:|-------------------:|-----------:|--------------:|------------:|------------------:|
| (96, 5)  | 1              | 2^23.09   | 2.30 * T0    | 2                  | 2^23.09    | 1.40 * T0    | 2           | 2                  | 2^23.04    | 0.90 * T0    | 2           | 2                 |
| (128,7) | 1              | 2^23.58   | 3.15 * T0    | 3                  | 2^23.58    | 1.86 * T0    | 6           | 3                  | 2^23.09    | 1.29 * T0    | 2           | 3                 |
| (160,9) | 1              | 2^25.17   | 1.36 * T0    | 0                  | 2^25.17    | 1.00 * T0    | 8           | 0                  | 2^24.61    | 0.36 * T0    | 2           | 3                 |
| (96, 3)  | 1              | 2^30.70   | 3.04 * T0    | 2                  | 2^30.70    | 1.67 * T0    | 2           | 2                  | 2^30.64    | 1.37 * T0    | 1           | 2                 |
| (144,5) | 1              | 2^31.64   | 2.30 * T0    | 2                  | 2^31.64    | 1.40 * T0    | 2           | 2                  | 2^31.61    | 0.90 * T0    | 2           | 2                 |
| (150,5) | 1              | 2^32.70   | 2.30 * T0    | 2                  | 2^32.70    | 1.40 * T0    | 2           | 2                  | 2^32.67    | 0.90 * T0    | 2           | 2                 |
| (192,7) | 1              | 2^32.00   | 3.15 * T0    | 3                  | 2^32.00    | 1.86 * T0    | 3           | 3                  | 2^31.64    | 1.29 * T0    | 2           | 3                 |
| (240,9) | 1              | 2^33.25   | 1.36 * T0    | 0                  | 2^33.25    | 1.00 * T0    | 8           | 0                  | 2^33.19    | 0.36 * T0    | 2           | 3                 |
| (96, 2)  | 1              | 2^39.04   | 1.75 * T0    | 1                  | 2^39.04    | 1.00 * T0    | 1           | 1                  | 2^39.02    | 0.75 * T0    | 1           | 1                 |
| (288,8) | 1              | 2^40.64   | 2.89 * T0    | 3                  | 2^40.64    | 1.75 * T0    | 3           | 3                  | 2^40.04    | 1.14 * T0    | 2           | 3                 |
| (200,9) | 1              | 2^29.21   | 1.36 * T0    | 0                  | 2^29.21    | 1.00 * T0    | 8           | 0                  | 2^28.93    | 0.36 * T0    | 2           | 3                 |


More resluts are available in [python-poc](./python-poc/).


## Implementations

Our implementation serves solely as a proof of concept and does not incorporate aggressive low-level optimizations. We also note that these optimizations may significantly impact the ASIC-resistance of existing blockchains that rely on $`\textsf{Equihash}`$. We therefore recommend that such blockchains reassess the memory bottlenecks of ASIC implementations across all $`\textsf{Equihash}`$ parameter settings. For the parameter setting \textsf{Equihash}$(200, 9)$, a subset of our optimization results is shown below (serving only as a proof of concept).


| Algorithm      | Sol/s | Peak RSS (kB) | Peak USS (kB) | Peak USS (MB) |
| -------------- | ----- | ------------- | ------------- | ------------- |
| CIP            | 2.27  | 162,304       | 160,952       | 157.18        |
| CIP-PR         | 0.51  | 61,056        | 59,660        | 58.26         |
| CIP-EM         | 2.06  | 61,576        | 58,748        | 57.37         |
| CIP-APR        | 0.88  | 66,048        | 64,568        | 63.05         |
| Tromp-Equix41  | 9.01  | 150,400       | 147,700       | 144.24        |


> The current implementations of the sorting algorithm and the linear-scan procedure still have substantial room for optimization, which explains the noticeable performance gap between the standard $\textsf{CIP}$ implementation and Tromp’s implementation. Further details can be found in the directory [advanced-cip](./advanced-cip/).