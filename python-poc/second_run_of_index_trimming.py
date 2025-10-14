from single_chain_algorithm import run_single_chain_algorithm
from wagner_algorithmic_estimator import Wagner_Algorithmic_Framework
from k_tree_algorithm import k_tree_wagner_algorithm, run_k_tree_algorithm
import os
from math import log2
import argparse


def theoretic_estimator(n, k, t = 1):
    waf = Wagner_Algorithmic_Framework(n, k)
    threshold_h, single_chain_layer_sizes = waf.civ_list_sizes_with_constraints(t)
    k_tree_layer_sizes = waf.kiv_list_sizes_with_constraints(t)
    return single_chain_layer_sizes, k_tree_layer_sizes



def run_random_case(n=200, k=9, nonce1=None, nonce2=None):
    assert n % (k + 1) == 0, "n must be divisible by k + 1"
    single_chain_layer_sizes, k_tree_layer_sizes = theoretic_estimator(n, k, 1)
    print("&".join([f"~$2^{{{log2(sz):.2f}}}$~" for sz in single_chain_layer_sizes]) + "\\\\")
    print("&".join([f"~$2^{{{log2(sz):.2f}}}$~" for sz in k_tree_layer_sizes]) + "\\\\")

    if nonce1 is None:
        nonce1 = os.urandom(16)
    else:
        nonce1 = bytes.fromhex(nonce1)
    print(f"nonce1.hex() = '{nonce1.hex()}'")
    run_k_tree_algorithm(n, k, nonce1, index_bit_length=1, verbose=True, trace_memory=False)

    if nonce2 is None:
        nonce2 = os.urandom(16)
    else:
        nonce2 = bytes.fromhex(nonce2)
    print(f"nonce2.hex() = '{nonce2.hex()}'")
    run_single_chain_algorithm(n, k, nonce2, 'iv_it_star', verbose=True, trace_memory=False)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Run Wagner algorithms with optional parameters. Note: This will take a long time for large (n, k) to run since our python implementation is poorly optimized, only for proof-of-concept.")
    parser.add_argument('--n', type=int, default=200, help='Parameter n (default: 200)')
    parser.add_argument('--k', type=int, default=9, help='Parameter k (default: 9)')
    parser.add_argument('--nonce1', type=str, default=None, help='Hex string for nonce1 (default: random)')
    parser.add_argument('--nonce2', type=str, default=None, help='Hex string for nonce2 (default: random)')
    args = parser.parse_args()
    run_random_case(n=args.n, k=args.k, nonce1=args.nonce1, nonce2=args.nonce2)
    
    
"""
Note: This will take a long time to run since our python implementation is poorly optimized, only for proof-of-concept.
paper case: $ python second_run_of_index_trimming.py --n 200 --k 9 --nonce1 2f8355540e1a4ed472aa14eba5534647 --nonce2 46a9be3479c4a2da4f5ab2cb7fefe79a

## Some logs
nonce.hex() = '2f8355540e1a4ed472aa14eba5534647'
...
Merge two lists at depth 0, the length of merged list: 262703
Merge two lists at depth 1, the length of merged list: 66120
Merge two lists at depth 2, the length of merged list: 4145
Merge two lists at depth 3, the length of merged list: 16
Merge two lists at depth 4, the length of merged list: 1
Merge two lists at depth 5, the length of merged list: 1
Merge two lists at depth 6, the length of merged list: 1
Merge two lists at depth 7, the length of merged list: 1
Merge two lists at depth 8, the length of merged list: 1
Time elapsed: 3537.42 seconds
All 2 solutions are correct!
nonce.hex() = '46a9be3479c4a2da4f5ab2cb7fefe79a'
====================Run Index Vector with Index Trimming (multiple runs)====================
nonce.hex() = '46a9be3479c4a2da4f5ab2cb7fefe79a'
n = 200, k = 9, upper_k = 10.04987562112089, ell = 20.0
Layer 0, the length of merged_list: 2097152, theoretical memory usage 2^28.651051691178928 bits
Layer 1, the length of merged_list: 2095505, theoretical memory usage 2^28.506661173351983 bits
Layer 2, the length of merged_list: 2094274, theoretical memory usage 2^28.355570780576333 bits
Layer 3, the length of merged_list: 2092259, theoretical memory usage 2^28.206083388300474 bits
Layer 4, the length of merged_list: 2086164, theoretical memory usage 2^28.079883987744964 bits
Layer 5, the length of merged_list: 2073079, theoretical memory usage 2^28.02773778377761 bits
Layer 6, the length of merged_list: 2051294, theoretical memory usage 2^28.13802785046574 bits
Layer 7, the length of merged_list: 2004397, theoretical memory usage 2^28.489325704588204 bits
Layer 8, the length of merged_list: 1917393, theoretical memory usage 2^29.08016800541846 bits
Layer 9, the length of merged_list: 1, theoretical memory usage 2^9.055282435501189 bits
Second round run with constraint solution of index_bit = 1
Layer 0, the length of merged_list: 2097152, theoretical memory usage 2^28.78790255939143 bits
Layer 1, the length of merged_list: 2095505, theoretical memory usage 2^28.793282399503394 bits
Layer 2, the length of merged_list: 2094274, theoretical memory usage 2^28.928756113521136 bits
Layer 3, the length of merged_list: 481890, theoretical memory usage 2^27.145130878255475 bits
Layer 4, the length of merged_list: 1078, theoretical memory usage 2^18.907031476917247 bits
Layer 5, the length of merged_list: 16, theoretical memory usage 2^13.59245703726808 bits
Layer 6, the length of merged_list: 8, theoretical memory usage 2^13.475733430966399 bits
Layer 7, the length of merged_list: 4, theoretical memory usage 2^13.4241662888181 bits
Layer 8, the length of merged_list: 2, theoretical memory usage 2^13.403012023574997 bits
Layer 9, the length of merged_list: 1, theoretical memory usage 2^13.394998514501673 bits
Perfect Solution: k_prime = 512
The distribution of solutions:
   num_candidate_solu = 1 num_perfect_solu = 1, num_secondary_solu = 0, num_trivial_solu = 0
Time taken: 109.1777 seconds
All 1 solutions are correct!
"""