This directory contains Python scripts mainly used for estimating the theoretical memory usage of various Wagner's algorithm configurations and for running simplified proofs-of-concept (PoC) to validate the correctness of the index trimming and post-retrieval techniques.

- **`all_in_one_estimators.py`**: The main estimation tool. It calculates the theoretical memory consumption for different algorithm variants (TIV, CIV, CIP, Hybrid) across various parameters (n, k).
- **`second_run_of_index_trimming.py`**: PoC for the Improved Index Trimming technique.
- **`single_chain_algorithm.py, k_tree_algorithm.py`**: PoC for TIV, CIP algorithms.
- **`wagner_algorithmic_estimator.py`**: Helper classes and functions for estimating the complexity of Wagner's algorithm.

## Python Version and Dependencies

- **Python**: 3.10 or newer.
- Install dependencies from this directory:

```bash
python3 -m pip install -r requirements.txt
```

The pinned dependencies include:

- `rich`, `prettytable`: required by the PoC and estimator scripts in `python-poc/`.
- `psutil`: required by `merge_in_place.py`.
- `numpy`, `matplotlib`: required by plotting scripts in `../pics/`.

## Index Trimming PoC

To reproduce the results in Table~4 from the paper, run:

```bash
python second_run_of_index_trimming.py --n 200 --k 9 --nonce1 2f8355540e1a4ed472aa14eba5534647 --nonce2 46a9be3479c4a2da4f5ab2cb7fefe79a
```

**Note**: This will take a long time (about 1 hour) to run since our python implementation is poorly optimized, only for proof-of-concept.

## Index Pointer, Post Retrieval and External Memory

For proof-of-concept tests of index pointer, post retrieval and external memory usage, run:

```bash
python single_chain_algorithm.py
```

For more accurate benchmarking, refer to our optimized C++ implementation.


## All-in-One Estimators

To generate the data for Tables 1, 2 in the paper (Memory estimators):

```bash
# General All-in-One estimator
python all_in_one_estimators.py --mode all-in-one

# Specific modes
python all_in_one_estimators.py --mode civ
python all_in_one_estimators.py --mode hybrid
```
