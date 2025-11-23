## Index Trimming PoC

To reproduce the results in Table~5 from the paper, run:

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

To reproduce the results in Table 1, Table 2, Table 3 from the paper, run:

```bash
python all_in_one_estimators.py --mode all-in-one
python all_in_one_estimators.py --mode civ
python all_in_one_estimators.py --mode hybrid
# python all_in_one_estimators.py --mode cip
```
