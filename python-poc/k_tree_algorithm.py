"""
Note: This is a basic implementation of Wagner's k-list algorithm for the strict generalized birthday problem
where 'strict' means the the messages must come from k distinct lists.
"""

import hashlib
from math import log2
import os
from collections import deque
import time
from collections import defaultdict
import tracemalloc
from rich.console import Console
from rich.panel import Panel

def run_with_memory_trace(func, *args, **kwargs):
    func_name = func.__name__
    print(f"Measuring memory for function: {func_name}")
    tracemalloc.start()
    result = func(*args, **kwargs)
    current, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    print(f"Current memory usage: {current / 1024 / 1024:.2f} MB")
    print(f"Peak memory usage: {peak / 1024 / 1024:.2f} MB")
    return result

def blake2b(data : bytes, digest_byte_size : int = 64) -> bytes:
    return hashlib.blake2b(data, digest_size = digest_byte_size).digest()

class k_tree_wagner_algorithm:
    
    def __init__(self, n, k, nonce: bytes = None, hashfunc = None):
        """
        Init a k-list solver for strict generalized birthday problem with parameters n, k. The underlying hash function is selected by `hashfunc` function.
    
        Args:
            n (int): the output bit length of hash function
            k (int): k messages to find, must be a power of 2
            nonce (bytes): the nonce used to generate the hash values
            hashfunc (function): the hash function to use
            
        Note:
            Strict generalized birthday problem: Given a hash function h with output length n, find k messages from pre-defined k distinct lists such that XOR of their hash values is 0.
        """
        assert n % 8 == 0, "n should be a multiple of 8"
        nonce = os.urandom(16) if nonce is None else nonce
        assert len(nonce) == 16, "Nonce should be 16 bytes"
        if hashfunc is None:
            hashfunc = lambda x: blake2b(x, n // 8)
        assert len(hashfunc(b'')) == n // 8, "Hash function output length should be n/8 bytes"
        self.n = n
        self.k = k
        self.nonce = nonce
        self.hashfunc = hashfunc
        self.hash_size = n // 8
        self.lgk = int(log2(k))
        assert 2 ** self.lgk == k, "k should be a power of 2"
        assert n % (self.lgk + 1) == 0, "n should be divisible by lg(k) + 1"
        self.ell = self.n // (self.lgk + 1)
        self.mask_bit = self.ell
        self.mask = (1 << self.mask_bit) - 1
        
    @staticmethod
    def new(n, k, nonce = None, hashfunc = None):
        """
        Create a new instance of k_tree_wagner_algorithm.

        Args:
            n (int): The output bit length of the hash function.
            k (int): The number of messages to find.
            nonce (bytes, optional): The nonce used to generate the hash values. Defaults to None (os.urandom(16)).
            hashfunc (function, optional): The hash function to use. Defaults to (blake2b).

        Returns:
            k_tree_wagner_algorithm: A new instance of k_tree_wagner_algorithm.
        """
        return k_tree_wagner_algorithm(n, k, nonce, hashfunc)
        
    def hash_merge(self, L1: list, L2: list, mask_bit: int) -> list:
        """
        Perform hash merge on two lists of messages:
            Time complexity: O(n)
            Memory complexity: O(n)
        
        Args:
            L1 (List): the first list of messages
            L2 (List): the second list of messages
            mask_bit (int): the number of bits to collide
        
        Returns:
            List[tuple[int, list[int]]]: list of (hash_value, index vector)
        """
        hash_table = defaultdict(list)
        mask = (1 << mask_bit) - 1
        
        for x1, idx1 in L1:
            hash_table[x1 & mask].append((x1, idx1))
        merged_list = []
        for x2, idx2 in L2:
            if x2 & mask in hash_table:
                colls = hash_table[x2 & mask]
                for x1, idx1 in colls:
                    merge_x = (x1 ^ x2) >> mask_bit
                    merge_idxs = idx1 + idx2
                    merged_list.append((merge_x, merge_idxs))
        return merged_list
    
    def compute_item(self, i: int, j: int):
        """ Compute the j-th item of the i-th list of messages.

        Args:
            i (int): the index of the list of messages
            j (int): the index of the message in the list

        Returns:
            int: the hash value of the message
        """
        message = self.nonce + f"{i}-{j}".encode()
        return int.from_bytes(self.hashfunc(message), 'big')
    
    def compute_hash_list_on_the_fly(self, i: int, index_bit_length: int = None, index_value: int = None):
        """ Compute the i-th hash list on the fly.

        Args:
            i (int): the index of the list of messages
            index_bit_length (int): the index bit length used in the first run
            index_value (int): the index value of the solution found in the first run
        
        Returns:
            List[tuple[int, int]]: the list of (hash_value, index)
            
        Note:
            In the second run, set the value of `index_bit_length` and `index_value`. This will only return a list of size 2**(ell - index_bit_length).
        """
        if index_bit_length is not None:
            index_mask = (1 << index_bit_length) - 1
            if index_value is not None:
                # this is for the second run, so we save the full index.
                return [(self.compute_item(i, j), [j]) for j in range(index_value, 2 ** self.mask_bit, 2 ** index_bit_length)]
            else:
                # this is for the first run, so we need to save all the indexes with bit length trimmed to `index_bit_length`
                return [(self.compute_item(i, j), [j & index_mask]) for j in range(2 ** self.mask_bit)]
        else:
            # index_bit_length is None, return the full index
            return [(self.compute_item(i, j), [j]) for j in range(2 ** self.mask_bit)]
        
    def _solve(self, index_bit_length: int = None, index_vals: list[int] = None, verbose = False):
        """ Solve the generalized birthday problem on the fly.
        
        Args:
            index_bit_length (int): the index bit length used in the first run
            index_vals (list[int]): the index values of the solution found in the first run (only used in the second run)
            verbose (bool): whether to print the debug information
        
        Returns:
            List[(int, list)]: the list of indexes of messages such that XOR of their hash values is 0
            
        """
        # generate the binary tree root using post-order traversal
        index_vals = index_vals if index_vals is not None else [None] * self.k
        stack = deque([(self.compute_hash_list_on_the_fly(0, index_bit_length, index_vals[0]), 0)])
        
        for i in range(1, self.k):
            # check the top of the stack depth is the same as the current depth
            current_depth = 0
            merged_list = self.compute_hash_list_on_the_fly(i, index_bit_length, index_vals[i])
            while len(stack) != 0 and stack[-1][1] == current_depth:
                # pop the top of the stack
                top_hash_list, _ = stack.pop()
                # merge the top of the stack with the current hash_list
                if current_depth == self.lgk - 1:
                    merged_list = self.hash_merge(top_hash_list, merged_list, self.mask_bit * 2)
                else:
                    merged_list = self.hash_merge(top_hash_list, merged_list, self.mask_bit)
                if verbose: print(f"Merge two lists at depth {current_depth}, the length of merged list: {len(merged_list)}")
                current_depth += 1
            # push the current hash_list to the stack
            stack.append((merged_list, current_depth))
        # the root of the binary tree
        root_hash_list, depth = stack.pop()
        assert depth == self.lgk, f"The depth of the binary tree should be {self.lgk}"
        return [index_vec for _, index_vec in root_hash_list]
    
    def solve(self, index_bit_length: int = None, verbose = False):
        """ Solve the generalized birthday problem with generated message lists.
        
        Args:
            index_bit (int): The index bit length used in the first run (if this value is set, trade-off will be used).
            verbose (bool): whether to print the debug information
        
        Returns:
            List[(int, list)]: the list of indexes of messages such that XOR of their hash values is 0
        """
        if index_bit_length is None:
            # no trade-off, use the full-length index
            return self._solve(verbose = verbose)
        else:
            # trade-off first run with trimmed index
            index_vectors = self._solve(index_bit_length, verbose = verbose)
            # second run with reduced size
            # note: we can combine the index vectors to reduce the running times if there are multiple candidates in the first run (not necessary)
            solutions = []
            if verbose: print(f"Second run with trimmed index bit length {index_bit_length}")
            for index_vector in index_vectors:
                solutions += self._solve(index_bit_length, index_vector, verbose = verbose)
        return solutions
    
    def verify_results(self, result_indices: list):
        """
        Verify the results of the algorithm: Check if the XOR of the hash values of the messages in each solution is zero.

        Args:
            result_indices (list[list[int]]): The list of index vectors representing the solutions.
        """
        if len(result_indices) == 0:
            print("No solution found!")
            return
        for indices in result_indices:
            hashval = 0
            for i, idx in enumerate(indices):
                hashval ^= self.compute_item(i, idx)
            assert hashval == 0, f"{hashval = }"
        print(f"All {len(result_indices)} solutions are correct!")
    

def run_k_tree_algorithm(n: int, k: int, nonce: bytes, index_bit_length: int = None, verbose: bool = True, trace_memory: bool = False):
    print("##" * 10 + f"Run k-list Wagner Algorithm" + "##" * 10)
    print(f"Parameters: n = {n}, k = {k}, index_bit_length = {index_bit_length}")
    print(f"Input nonce: {nonce.hex()}")
    solver = k_tree_wagner_algorithm.new(n, 2**k, nonce)
    st = time.time()
    if trace_memory:
        solutions = run_with_memory_trace(solver.solve, index_bit_length = index_bit_length, verbose=verbose)
    else:
        solutions = solver.solve(index_bit_length, verbose=verbose)
    et = time.time()
    print(f"Time elapsed: {et - st:.2f} seconds")
    solver.verify_results(solutions)

def pretty_print_box(header: str, content: str):
    console = Console()
    console.print(
        Panel.fit(
            f"[red]{content}[/red]",
            title=f"[red]⚠ {header}[/red]",
            border_style="red"
        )
    )

def k_tree_tests():
    n, k = 128, 7
    nonce = bytes.fromhex("58c3d5db02bd1617dc3e7844950d6f7c")
    remarks = "Our proof-of-concept implementation is not optimized for index vector storage in Python. Since indices are represented as int objects, a 1-bit value and an 8-bit value occupy the same memory. As a result, the observed peak memory reduction from index trimming is less significant than the theoretical expectation. Here, our implementation only aims to validate the theoretical correctness rather than optimized performance."
    pretty_print_box("Remarks", remarks)
    run_k_tree_algorithm(n, k, nonce, index_bit_length=1, verbose=False, trace_memory=True)
    run_k_tree_algorithm(n, k, nonce, verbose=False, trace_memory=True)

if __name__ == "__main__":
    k_tree_tests()