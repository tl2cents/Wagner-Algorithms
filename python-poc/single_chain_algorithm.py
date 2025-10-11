"""
Note: This is an optimized implementation of single-list algorithm for the loose generalized birthday problem
where 'loose' means the messages are selected from a single list which is applicable to the Equihash mining algorithm.
"""

import hashlib
from math import log2
import os
from collections import defaultdict
from io import BufferedWriter
import mmap
import time
import tracemalloc
from rich.console import Console
from rich.panel import Panel

algorithm_types = {
    'plain_ip': "Plain Index Pointer",
    'plain_iv': "Plain Index Vector",
    'ip_pr': "Index Pointer with Post Retrieval",
    'ip_em': "Index Pointer with External Memory",
    'iv_it': "Index Vector with Index Trimming", # Single Bit Index Case
    'iv_it_star': "Index Vector with Index Trimming (multiple runs)", # Single Bit Index Case
}



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

class single_chain_wagner_algorithm:
    """ 
    An optimized implementation of single-list algorithm. In this proof of concept, there are two kinds of implementations:
    1. Implementation of index pointers.
    2. Implementation of index vectors.

    Apart from these implementations, our work made the following new contributions:
    1. Handle trivial collisions near bound k = sqrt{n/2 + 1} to avoid list size blowup.
    2. Improved time-memory trade-offs for index vectors: index trimming and xor-removal.
    3. New time-memory trade-offs for index pointers: index-pointer post-retrieval or using external memory.
    """
    def __init__(self, n, k, hashfunc = None, nonce = b"nonce"):
        """ 
        Init a single-list solver for loose generalized birthday problem with parameters n, k. The underlying hash function is selected by `hashfunc` function.
        
        Loose generalized birthday problem: Given a hash function h with output length n, find k messages such that XOR of their hash values is 0.
        Args:
            n (int): the output bit length of hash function
            k (int): k messages to find, must be a power of 2
            hashfunc (function): the hash function to use: bytes* -> bytes
        """
        assert n % 8 == 0, "n should be a multiple of 8"
        if hashfunc is None:
            hashfunc = lambda x: blake2b(x, n // 8)
        assert len(hashfunc(b'')) == n // 8, "Hash function output length should be n/8 bytes"
        self.nonce = nonce
        self.n = n
        self.k = k
        self.lgk = int(log2(k))
        assert 2 ** self.lgk == k, "k should be a power of 2"
        assert n % (self.lgk + 1) == 0, "n should be a multiple of lg(k) + 1"
        self.hashfunc = hashfunc
        self.hash_size = n // 8
        self.ell = self.n // (1 + self.lgk)
        self.mask_bit = self.ell
        self.N = 2 ** (self.ell + 1)
        self.mask = (1 << self.mask_bit) - 1
        self.hash_list = None
        self.index_pointers = []
        self.current_idx = None
        self.ip_nbytes = (self.ell + 1 + 7) // 8 # number of bytes to store an index pointer
        self.algos = {
            "plain_iv": self.solve_index_vector,
            "plain_ip": self.solve_index_pointer,
            "ip_pr": self.solve_index_pointer_with_post_retrieval,
            "ip_em": self.solve_index_pointer_with_external_memory,
            "iv_it": self.solve_index_vector_with_single_bit_and_2_runs, 
            "iv_it_star": self.solve_index_vector_with_single_bit,
        }
        
    @staticmethod
    def new(n, k, hashfunc = None):
        return single_chain_wagner_algorithm(n, k, hashfunc)
    
    def _estimate_plain_iv(self) -> tuple[int, int]:
        """ 
        Estimate the time and memory complexity of plain single-list algorithm with index vector.
    
        Returns:
            (int, int): the estimated time complexity and memory complexity
        """
        T = self.lgk * 2 ** (self.mask_bit + 1)
        # M = 2*(self.n + self.lgk - self.mask_bit - 1) * 2 ** (self.mask_bit + 1)
        M = (2**(self.lgk - 1) * (self.ell + 1) + 2 * self.ell) * self.N
        return T, M
    
    def _estimate_plain_ip(self) -> tuple[int, int]:
        """ 
        Estimate the time and memory complexity of plain single-list algorithm with index pointer.

        Returns:
            (int, int): the estimated time complexity and memory complexity
        """
        T = self.lgk * 2 ** (self.mask_bit + 1)
        M = 2*(self.n + self.lgk - self.ell - 1) * self.N
        return T, M
    
    def _estimate_layer_memory(self, layer: int, layer_size: int, index_bit: int = None) -> float:
        """ 
        Estimate the memory complexity of a specific layer with index vector.

        Args:
            layer (int): the layer index
            layer_size (int): the size of the layer
            index_bit (int): the trimmed length of index

        Returns:
            float: the estimated memory complexity: log2(mem) (measured in bits) 
        """
        index_bit = index_bit if index_bit is not None else self.ell + 1
        if layer_size == 0:
            return 0
        return log2(layer_size * (2**layer * index_bit + self.n - self.mask_bit * layer))

    def compute_hash_item(self, i: int):
        """ 
        Compute the i-th item of messages.

        Args:
            i (int): the index of messages

        Returns:
            int: the hash value of the message
        """
        return int.from_bytes(self.hashfunc(self.compute_mess_item(i)), 'big')

    def compute_mess_item(self, i: int):
        """ 
        Generate the i-th message.
        
        Args:
            i (int): the index of messages

        Returns:
            bytes: the i-th message
        """
        return self.nonce + f"message-{i}".encode()

    def generate_hash_list(self, index_bit = None):
        """ 
        Generate hash list (the initial list in layer 0)

        Args:
            index_bit (int, optional): the number of bits to use for the index, if None, no index is included. Defaults to None.

        Returns:
            list[int] or list[tuple[int, int]]: the generated hash list with or without index
        """
        single_chain_size = self.N
        hash_list = []        
        if index_bit is None:
            for j in range(single_chain_size):
                hashval = self.compute_hash_item(j)
                hash_list.append(hashval)
        else:
            idx = 0
            mask = (1 << index_bit) - 1
            for j in range(single_chain_size):
                hashval = self.compute_hash_item(j)
                hash_list.append((hashval, [idx]))
                idx += 1
                idx &= mask
        return hash_list

    def get_current_index_vector(self, idx: int) -> set:
        """ 
        Get the index vector of the current index.

        Args:
            idx (int): the current index point

        Returns:
            list: the index vector of the current index
        """
        idx_vec = [idx]
        for i in range(len(self.index_pointers) - 1, -1, -1):
            index_list = self.index_pointers[i]
            tmp = []
            for idx in idx_vec:
                idx1, idx2 = index_list[idx]
                tmp.append(idx1)
                tmp.append(idx2)
            idx_vec = tmp
        return idx_vec    
    
    def hash_merge_index_pointer(self, L: list, mask_bit: int) -> tuple[list, list]:
        """ 
        Perform hash join on one list. Time/Memory complexity: O(|L|)

        Args:
            L (List): the list of messages
            mask_bit (int): the number of bits to collide

        Returns:
            List[int], List[tuple[int, int]]: the merged list and the list of index pointers.
        """
        hash_table = {}
        merged_list = []
        merged_index = []
        mask = (1 << mask_bit) - 1
        # before the last self-merge, we must discard full-zero values since with they are trivial solutions with overwhelming probability
        discard_zero = mask_bit != self.ell * 2

        for i, x1 in enumerate(L):
            # Way to reduce peak mem: split list L into multiple chunks and free chunks once each chunk is processed.
            # A better way is to use deque to free memory while enumerating
            # For simplicity, we ignore such optimizations since this is a proof-of-concept implementation.
            x_low = x1 & mask
            x_high = x1 >> mask_bit
            if x_low not in hash_table:
                hash_table[x_low] = [(x_high, i)]
            else:
                if discard_zero == True and any(x2_high ^ x_high == 0 for x2_high, j in hash_table[x_low]):
                    # we discard the zero value because it is a trivial solution in most cases and will not keep the new item in hash_table
                    continue
                if discard_zero == False and len(hash_table[x_low]) > 1:
                    # this is probably an invalid trivial solution since collisions on the same item thrice is almost impossible.
                    x1_high, idx1 = hash_table[x_low][0]
                    x2_high, idx2 = hash_table[x_low][1]
                    # remove the trivial collisions
                    idx = (idx2 , idx1)
                    if idx in merged_index:
                        assert x2_high ^ x1_high == 0
                        merged_list.remove(x2_high ^ x1_high)
                        merged_index.remove(idx)
                    # we will not keep the new item in hash_table
                    continue
                for x2_high, j in hash_table[x_low]:
                    # assert x2_high ^ x_high != 0 or (not discard_zero), f"{x2_high = }, {x_high = }"
                    merged_list.append(x2_high ^ x_high)
                    merged_index.append((i, j))
                hash_table[x_low].append((x_high, i))
        return merged_list, merged_index
    
    def hash_merge_index_vector(self, L: list, mask_bit: int, index_bit: int = None, check_table: dict = None) -> tuple[list, list]:
        """ 
        Perform hash join on one list. Time/Memory complexity: O(|L|)

        Args:
            L (List): the list of messages and their indices, i.e. [(m1, i1), (m2, i2), ...]
            mask_bit (int): the number of bits to collide
            index_bit (int): the number of constrained bits in the index
            check_table (dict): a table to check for valid indices with `index_bit` constrained bits.

        Returns:
            List[tuple(int, list)]: the merged list of messages and their indices
        """
        hash_table = {}
        merged_list = []
        mask = (1 << mask_bit) - 1
        # before the last self-merge, we must discard full-zero values since with they are trivial solutions with overwhelming probability
        discard_zero = mask_bit != self.ell * 2
        assert check_table is None or index_bit is not None, "If check_table is not None, index_bit must be specified"
        index_mask = (1 << index_bit) - 1 if index_bit is not None else None

        for x1, idx1 in L:
            # Way to reduce peak mem: split list L into multiple chunks and free chunks once each chunk is processed.
            # A better way is to use deque to free memory while enumerating
            # For simplicity, we ignore such optimizations since this is a proof-of-concept implementation.
            x_low = x1 & mask
            x_high = x1 >> mask_bit
            if x_low not in hash_table:
                hash_table[x_low] = [(x_high, idx1)]
            else:
                if discard_zero == True and any(x2_high ^ x_high == 0 for x2_high, _ in hash_table[x_low]):
                    # we discard the zero value because it is a trivial solution in most cases and will not keep the new item in hash_table
                    continue
                if discard_zero == False and len(hash_table[x_low]) > 1:
                    # this is probably an invalid solution since collisions on the same item thrice is almost impossible.
                    x1_high, idx1 = hash_table[x_low][0]
                    x2_high, idx2 = hash_table[x_low][1]
                    # remove the trivial collisions
                    idx = idx2 + idx1 # when add idx to list, the latter should come first, so idx = idx2 + idx1 instead of idx1 + idx2
                    if check_table is not None:
                        idx = tuple(i & index_mask for i in idx)
                    if (x2_high ^ x1_high, idx) in merged_list:
                        merged_list.remove((x2_high ^ x1_high, idx))
                    continue
                for x2_high, idx2 in hash_table[x_low]:
                    # assert x2_high ^ x_high != 0 or (not discard_zero), f"{x2_high = }, {x_high = }"
                    if check_table is None:
                        merged_list.append((x2_high ^ x_high, idx1 + idx2))
                    else:
                        idx = tuple(i & index_mask for i in idx1 + idx2)
                        if idx in check_table:
                            merged_list.append((x2_high ^ x_high, idx1 + idx2))
                hash_table[x_low].append((x_high, idx1))
        return merged_list
    
    def hash_merge_index_vector_efficient(self, L: list, mask_bit: int, index_bit: int = None, check_table: dict = None) -> tuple[list, list]:
        """ 
        Perform hash join on one list. Time/Memory complexity: O(|L|)

        Args:
            L (List): the list of messages and their indices, i.e. [(m1, i1), (m2, i2), ...]
            mask_bit (int): the number of bits to collide
            index_bit (int): the number of constrained bits in the index
            check_table (dict): a table to check for valid indices with `index_bit` constrained bits.

        Returns:
            List[tuple(int, list)]: the merged list of messages and their indices
        """
        hash_table = {}
        merged_list = []
        mask = (1 << mask_bit) - 1
        # before the last self-merge, we must discard full-zero values since with they are trivial solutions with overwhelming probability
        discard_zero = mask_bit != self.ell * 2
        assert check_table is None or index_bit is not None, "If check_table is not None, index_bit must be specified"
        index_mask = (1 << index_bit) - 1 if index_bit is not None else None

        for x1, idx1 in L:
            # Way to reduce peak mem: split list L into multiple chunks and free chunks once each chunk is processed.
            # A better way is to use deque to free memory while enumerating
            # For simplicity, we ignore such optimizations since this is a proof-of-concept implementation.
            x_low = x1 & mask
            x_high = x1 >> mask_bit
            if x_low not in hash_table:
                hash_table[x_low] = [(x_high, idx1)]
            else:
                if discard_zero == True and any(x2_high ^ x_high == 0 for x2_high, _ in hash_table[x_low]):
                    # we discard the zero value because it is a trivial solution in most cases and will not keep the new item in hash_table
                    continue
                if discard_zero == False and len(hash_table[x_low]) > 1:
                    # this is probably an invalid solution since collisions on the same item thrice is almost impossible.
                    x1_high, idx1 = hash_table[x_low][0]
                    x2_high, idx2 = hash_table[x_low][1]
                    # remove the trivial collisions
                    idx = idx2 + idx1 # when add idx to list, the latter should come first, so idx = idx2 + idx1 instead of idx1 + idx2
                    if check_table is not None:
                        idx = tuple(i & index_mask for i in idx)
                    if (x2_high ^ x1_high, idx) in merged_list:
                        merged_list.remove((x2_high ^ x1_high, idx))
                    continue
                for x2_high, idx2 in hash_table[x_low]:
                    # assert x2_high ^ x_high != 0 or (not discard_zero), f"{x2_high = }, {x_high = }"
                    if check_table is None:
                        merged_list.append((x2_high ^ x_high, idx1 + idx2))
                    else:
                        idx = tuple(i & index_mask for i in idx1 + idx2)
                        if idx in check_table:
                            merged_list.append((x2_high ^ x_high, idx1 + idx2))
                hash_table[x_low].append((x_high, idx1))
        return merged_list
    
    def _check_valid_index_vector(self, index_vector: tuple, verbose: bool = False) -> tuple[int, tuple]:
        """ 
        Check the validity of the index vector.

        Args:
            index_vector (tuple): the index vector to check.
            verbose (bool): whether to print verbose output.

        Returns:
            int: 0: trivial solution, 1: perfect solution, 2: secondary solution
            tuple: the real index vector (with duplicates removed)
        """
        set_weight = len(set(index_vector))
        if set_weight == self.k:
            k_prime = self.k
            if verbose: print(f"Perfect Solution: {k_prime = }")
            return 1, index_vector
        else:
            # we might get some duplicate solutions i.e., the same message is used twice
            # this solution is also valid if we also allow solutions to LGBP(n, k-2) or LGBP(n, k-2i).
            counter = defaultdict(int)
            for idx in index_vector:
                counter[idx] += 1 
                counter[idx] %= 2
            # number of 1s in the counter
            k_prime = sum(counter.values())
            if k_prime != 0:
                # not a trivial solution but still valid for LGBP(n, k_prime) i.e. LGBP(n, k-2i)
                real_index_vector = set([idx for idx, val in counter.items() if val == 1])
                if verbose: print(f"Secondary Solution: {k_prime = }")
                return 2, tuple(real_index_vector)
            else:
                # trivial solution
                return 0, tuple()

    def check_index_vectors(self, index_vectors: list[tuple], verbose: bool = False) -> list[tuple]:
        """ 
        Check the validity of a list of index vectors and return the solutions without without the trivial ones.

        Args:
            index_vectors (list): the list of index vectors to check.
            verbose (bool): whether to print verbose output.

        Returns:
            list: a list of tuples containing the real index vector (without trivial solutions)
        """
        sols = []
        num_perfect_solu = 0
        num_secondary_solu = 0
        num_trivial_solu = 0
        num_candidate_solu = len(index_vectors)
        for index_vector in index_vectors:
            solu_type, real_tuple = self._check_valid_index_vector(index_vector, verbose)
            if solu_type == 0:
                # trivial solution
                num_trivial_solu += 1
            elif solu_type == 1:
                # perfect solution
                num_perfect_solu += 1
                sols.append(set(real_tuple))
            else:
                # secondary solution
                num_secondary_solu += 1
                if set(real_tuple) not in sols:
                    sols.append(set(real_tuple))
        if verbose: print(f"The distribution of solutions:\n   {num_candidate_solu = } {num_perfect_solu = }, {num_secondary_solu = }, {num_trivial_solu = }")
        return sols

    def _retrieve_index_pointer_solution(self, current_ips: list, verbose: bool = False) -> list:
        """ 
        Retrieve the index pointer solution from the current state. This function can only be called after the final self-merge.

        Args:
            current_ips (list): the current index pointers.
            verbose (bool): whether to print verbose output.

        Returns:
            List[Tuple[int]]: the list of index vectors (final solutions).
        """
        index_vectors = [self.get_current_index_vector(ip1) + self.get_current_index_vector(ip2) for ip1, ip2 in current_ips]
        return self.check_index_vectors(index_vectors, verbose)
        
    def solve_index_pointer(self, verbose = False):
        """ 
        Solve the loose generalized birthday problem with index pointers.

        Args:
            verbose (bool): whether to print the debug information.

        Returns:
            List[tuple(int)]: the list of indexes of messages such that XOR of their hash values is 0
        """
        merged_list = self.generate_hash_list()
        if verbose: print(f"Layer 0, size of list :{len(merged_list)}")
        for i in range(self.lgk - 1):
            merged_list, current_idx = self.hash_merge_index_pointer(merged_list, self.ell)
            self.index_pointers.append(current_idx)
            if verbose: print(f"Layer {i + 1}, size of list :{len(merged_list)}")
        merged_list, current_idx = self.hash_merge_index_pointer(merged_list, self.ell * 2)
        if verbose: print(f"Layer {self.lgk}, size of list :{len(merged_list)}")
        sols = self._retrieve_index_pointer_solution(current_idx, verbose)
        self.index_pointers = [] # remove the index pointers
        return sols
    
    def _solve_index_vector(self, index_bit: int = None, check_tables = None, verbose = False):
        """
        Solve the loose generalized birthday problem with index vectors.

        Args:
            index_bit (int): the trimmed length of index.
            check_tables (list[dict]): the list of check tables for each layer, the check table is used to filter the valid index vectors in each layer.
            verbose (bool): whether to print the debug information
        
        Returns:
            List[tuple(int)]: the list of indexes of messages such that XOR of their hash values is 0
        """
        index_bit = index_bit if index_bit is not None else self.ell + 1
        if check_tables == None:
            # this indicates that we are working with the first run of the single-list algorithm.
            # typically, we can use index trimming techniques to reduce the size of the index and we will get a partial index solution.
            # such a partial index solution can build check tables for the second run.
            merged_list = self.generate_hash_list(index_bit)
            check_tables = [None] * self.lgk # check tables are None in the first run
            stored_index_bit = index_bit
        else:
            # this indicates that we are working with the second run of the single-list algorithm.
            # typically, if check_tables is not None, we store the full index and use the check tables to filter the valid index vectors.
            merged_list = self.generate_hash_list(self.ell + 1)
            stored_index_bit = self.ell + 1
        if verbose: print(f"Layer 0, the length of merged_list: {len(merged_list)}, theoretical memory usage 2^{self._estimate_layer_memory(0, len(merged_list), stored_index_bit)} bits")
        for i in range(self.lgk - 1):
            merged_list = self.hash_merge_index_vector(merged_list, self.mask_bit, index_bit, check_tables[i])
            if verbose: print(f"Layer {i + 1}, the length of merged_list: {len(merged_list)}, theoretical memory usage 2^{self._estimate_layer_memory(i + 1, len(merged_list), stored_index_bit)} bits")
        merged_list = self.hash_merge_index_vector(merged_list, self.mask_bit * 2, index_bit, check_tables[self.lgk - 1])
        if verbose: print(f"Layer {self.lgk}, the length of merged_list: {len(merged_list)}, theoretical memory usage 2^{self._estimate_layer_memory(self.lgk, len(merged_list), stored_index_bit)} bits")
        return [idx_vec for val, idx_vec in merged_list]
    
    def solve_index_vector(self, index_bit: int = None, verbose = False):
        """
        Solve the loose generalized birthday problem with index vectors. We also use the index trimming technique to reduce peak memory. 
        Our technique is more efficient than Equihash since we utilize a single list algorithm in the second run instead of a k-list algorithm. 
        Typically, the index bit can be trimmed to 1 bit compared to 8 bits in Equihash. 
        Our technique can also be combined with XOR-removal to further reduce peak memory.

        Args:
            index_bit (int, optional): the trimmed length of index. Defaults to None (full length).
            verbose (bool, optional): whether to print verbose output. Defaults to False.

        Returns:
            List[tuple(int)]: the list of index vectors (solutions).
        """
        index_bit = index_bit if index_bit is not None else self.ell + 1
        solu_candidates = self._solve_index_vector(index_bit, verbose = verbose)
        index_vectors = []
        if index_bit < self.ell + 1:
            for index_vector in solu_candidates:
                # we can also reserve the intermediate layer of height \hat{h}(k, t) to avoid multiple runs.
                check_tables = []
                if verbose: print(f"Second round run with constraint solution of {index_bit = }")
                # print(f"{index_vector = }")
                for h in range(1, self.lgk + 1):
                    l = 2**h
                    check_table = defaultdict(bool)
                    for i in range(0, len(index_vector), l):
                        check_table[tuple(index_vector[i:i+l])] = True
                    check_tables.append(check_table)
                index_vector = tuple(self._solve_index_vector(index_bit, check_tables, verbose)[0])
                index_vectors.append(index_vector)
        else:
            index_vectors = solu_candidates
        return self.check_index_vectors(index_vectors, verbose)

    def solve_index_vector_with_single_bit(self, verbose = False):
        return self.solve_index_vector(index_bit = 1, verbose = verbose)
        
    def solve_index_vector_with_single_bit_and_2_runs(self, verbose = False):
        """
        When k is near the bound sqrt{n/2 + 1}, there might be many candidates. Denote the peak memory of the first run as M.
            - Ways to reduce the number of candidates:
                1. Filter out the trivial solutions before the final self-merge.
                2. Detect the trivial solutions in the final self-merge: multiple solutions from the same hash item are trivial solutions in the final self-merge.
                3. Increase the index_bit slightly greater than ell + 1 in the final self-merge. We can filter out all the trivial solutions just as the previous rounds.
            Methods 1,2 remove almost all the trivial solutions (method 3 is optional), but they are not guaranteed to remove all the non-trivial solutions.
            
            - Ways to speed up the second runs:
                1. We can merge the constraints into one table if the estimated memory peak does not exceed M.
                2. We can reserve the intermediate layer of height h0 = hat{h}(k, t) to speed up the second runs if the estimated memory peak of reserving two low-height layers (h0, h0 + 1) does not exceed M.
        The proof-of-concept script will not cover optimal implementation details and just focus on the core idea of trade-off.
        
        Note:
            Usually, when k <= floor(sqrt{n/2 + 1}) - 1, there will not be many candidates in the last self-merge. And such cases can be handled more easily when single list algorithm is extended to field Z_q.
        """
        index_bit = 1
        solu_candidates = self._solve_index_vector(index_bit, verbose = verbose)
        index_vectors = []
        check_tables = [defaultdict(bool) for _ in range(self.lgk)]
        for index_vector in solu_candidates:
            # we take the first way: merge the constraints into one table. By the way, we can also reserve the intermediate layer of height \hat{h}(k, t) to avoid multiple runs.
            for h in range(1, self.lgk + 1):
                l = 2**h
                for i in range(0, len(index_vector), l):
                    check_tables[h-1][tuple(index_vector[i:i+l])] = True
        if verbose: print(f"Second round run with constraint solution of {index_bit = }")
        index_vectors = list(self._solve_index_vector(index_bit, check_tables, verbose))
        return self.check_index_vectors(index_vectors, verbose)
    
    def hash_merge_pure_vals(self, L: list, mask_bit: int):
        """
        Perform hash join on one list without index pointer or index vector. Time/Memory complexity: O(|L|)

        Args:
            L (List): the list of messages.
            mask_bit (int): the number of bits to collide
        
        Returns:
            List[int]: the merged list of messages.
        """
        hash_table = {}
        merged_list = []
        mask = (1 << mask_bit) - 1
        # before the last self-merge, we must discard full-zero values since with they are trivial solutions with overwhelming probability
        discard_zero = mask_bit != self.ell * 2
        # in our setting, the last self-merge will never be called in in pure-hash-join way.
        assert discard_zero, "Last self-merge should not be called in pure-hash-join way."
        for x1 in L:
            # Way to reduce peak mem: split list L into multiple chunks and free chunks once each chunk is processed.
            # A better way is to use deque to free memory while enumerating
            # For simplicity, we ignore such optimizations since this is a proof-of-concept implementation.
            x_low = x1 & mask
            x_high = x1 >> mask_bit
            if x_low not in hash_table:
                hash_table[x_low] = [x_high]
            else:
                # we will always drop full-zero values (discard_zero == True)
                if any(x2_high ^ x_high == 0 for x2_high in hash_table[x_low]):
                    # we discard the zero value because it is a trivial solution in most cases and will not keep the new item in hash_table
                    continue
                for x2_high in hash_table[x_low]:
                    merged_list.append(x2_high ^ x_high)
                hash_table[x_low].append(x_high)
        return merged_list
    
    def serialize_index_pointer(self, ip: tuple[int, int], writer: BufferedWriter):
        """ Serialize the index pointer to the writer.

        Args:
            ip (tuple[int, int]): the index pointer to serialize.
            writer (BufferedWriter): the writer to write the index pointer.
        """
        writer.write(ip[0].to_bytes(self.ip_nbytes, 'big'))
        writer.write(ip[1].to_bytes(self.ip_nbytes, 'big'))
        writer.flush()

    def hash_merge_external_ip(self, L: list, mask_bit: int, external_mem: BufferedWriter) -> tuple[list, list]:
        """ Perform hash join on one list. Time/Memory complexity: O(|L|)

        Args:
            L (List): the list of messages
            mask_bit (int): the number of bits to collide
            external_mem_path (str): the path to the external memory file for saving index pointer

        Returns:
            List[int]: the merged list.
        """
        
        hash_table = {}
        merged_list = []
        mask = (1 << mask_bit) - 1
        assert mask_bit != self.ell * 2, "Do not store index pointers in external memory in the last self-merge"
        
        for i, x1 in enumerate(L):
            # a better way is to use deque to free memory while enumerating
            x_low = x1 & mask
            x_high = x1 >> mask_bit
            if x_low not in hash_table:
                hash_table[x_low] = [(x_high, i)]
            else:
                # in this case, discard_zero == True 
                if any(x2_high ^ x_high == 0 for x2_high, j in hash_table[x_low]):
                    # we discard the zero value because it is a trivial solution in most cases and will not keep the new item in hash_table
                    continue
                for x2_high, j in hash_table[x_low]:
                    merged_list.append(x2_high ^ x_high)
                    self.serialize_index_pointer((i, j), external_mem)
                hash_table[x_low].append((x_high, i))
        return merged_list
        
    def hash_merge_pure_ips(self, L: list, mask_bit: int) -> tuple[list, list]:
        """ Perform hash join on one list but only returns index pointers. Time/Memory complexity: O(|L|)

        Args:
            L (List): the list of messages
            mask_bit (int): the number of bits to collide

        Returns:
            List[tuple[int, int]]: the merged list of index pointers
        """
        hash_table = {}
        merged_index = []
        mask = (1 << mask_bit) - 1
        # before the last self-merge, we must discard full-zero values since with they are trivial solutions with overwhelming probability
        discard_zero = mask_bit != self.ell * 2
        for i, x1 in enumerate(L):
            # Way to reduce peak mem: split list L into multiple chunks and free chunks once each chunk is processed.
            # A better way is to use deque to free memory while enumerating
            # For simplicity, we ignore such optimizations since this is a proof-of-concept implementation.
            x_low = x1 & mask
            x_high = x1 >> mask_bit
            if x_low not in hash_table:
                hash_table[x_low] = [(x_high, i)]
            else:
                if discard_zero == True and any(x2_high ^ x_high == 0 for x2_high, j in hash_table[x_low]):
                    # we discard the zero value because it is a trivial solution in most cases and will not keep the new item in hash_table
                    continue
                if discard_zero == False and len(hash_table[x_low]) > 1:
                    # this is probably an invalid trivial solution since collisions on the same item thrice is almost impossible in the last self-merge.
                    x1_high, idx1 = hash_table[x_low][0]
                    x2_high, idx2 = hash_table[x_low][1]
                    # remove the trivial collisions
                    idx = (idx2 , idx1)
                    if idx in merged_index:
                        assert x2_high ^ x1_high == 0
                        merged_index.remove(idx)
                    # we will not keep the new item in hash_table
                    continue
                for x2_high, j in hash_table[x_low]:
                    merged_index.append((i, j))
                hash_table[x_low].append((x_high, i))
        return merged_index
        
    def solve_index_pointer_with_post_retrieval(self, verbose: bool = False):
        """ Solve the loose generalized birthday problem with post-retrieval index pointer. We will run the reduced-round single-list algorithm k times to retrieve index pointer (switching height = k -1)
        
        Args:
            verbose (bool, optional): wether to print debug info. Defaults to False.
            
        Returns:
            List[tuple[int]]: the list of index vectors (solutions)

        Note:

            1. run k-round single-list algorithm to recover the last index pointer pair (p1, p2)
            2. run (k-1)-round single-list algorithm to recover index pointer of (p1, p2) ==> (p11, p12, p21, p22)
            .. ...
            k. run 1-round single-list algorithm to recover the real index vector solution: (i1, i2, ..., ik)
        """
        solutions = None
        ell = self.ell
        for n_round in range(self.lgk, 0, -1):
            # n_round - 1 self-merge: pure_hash_merge
            merged_list = self.generate_hash_list()
            if verbose: print(f"Post-retrieval round {self.lgk - n_round + 1}, {n_round = }")
            for _ in range(n_round - 1):
                merged_list = self.hash_merge_pure_vals(merged_list, ell)
            if n_round == self.lgk:
                # collide 2 * ell bits
                merged_index = self.hash_merge_pure_ips(merged_list, 2 * ell)
                solutions = merged_index
                del merged_list
            else:
                # collide ell bits
                merged_index = self.hash_merge_pure_ips(merged_list, ell)
                extended_solutions = []
                for solution in solutions:
                    # if verbose: print(f"Extending solution: {solution}, {len(merged_index) = }, {len(merged_list) = }")
                    solution = sum([list(merged_index[idx]) for idx in solution], [])
                    extended_solutions.append(solution)
                del merged_list, solutions, merged_index
                solutions = extended_solutions
        
        return self.check_index_vectors(solutions, verbose)

    def solve_index_pointer_with_external_memory(self, external_path: str = None, verbose: bool = False):
        """ Solve the loose generalized birthday problem with index pointer but cache index pointers in external memory.

        Args:
            external_path (str): the path to the external memory file. Defaults to None.
            verbose (bool, optional): wether to print debug info. Defaults to False.
        
        Note:

            1. run k-round single-list algorithm to recover the last index pointer pair (p1, p2) and store index_pointer lists: L1, L_2, ..., L_{k-1} in external memory
            2. retrieve (p11, p12, p21, p22) from L_{k-1} in external memory
            .. ...
            k. retrieve the real index vector solution: (i1, i2, ..., ik) from L_1 in external memory
        """
        if external_path is None:
            os.makedirs(os.path.dirname("./tmp"), exist_ok=True)
            external_path = f"./tmp/ip_{self.n}_{self.k}.bin"
        ell = self.ell
        # first run: recover (p1, p2) and store index_pointer lists in external memory
        merged_list = self.generate_hash_list()
        if verbose: print(f"Layer 0, size of list :{len(merged_list)}")
        tmp_writer = open(external_path, "wb")
        layer_sizes = []
        for _ in range(self.lgk - 1):
            merged_list = self.hash_merge_external_ip(merged_list, ell, tmp_writer)
            if verbose: print(f"Layer {_ + 1}, size of list :{len(merged_list)}")
            layer_sizes.append(len(merged_list))
        merged_index = self.hash_merge_pure_ips(merged_list, 2 * ell)
        if verbose: print(f"Layer {_ + 1}, size of list :{len(merged_index)}")
        del merged_list
        tmp_writer.close()
        with open(external_path, "rb") as f:
            ex_mem = mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ) # virtual memory mapping
            # retrieve index vectors in external memory
            solutions = merged_index
            for layer_i in range(self.lgk - 1, 0, -1):
                offset = sum(layer_sizes[:layer_i - 1]) * 2 * self.ip_nbytes
                new_solutions = []
                for solution in solutions:
                    new_solution = []
                    for idx in solution:
                        start = offset + 2 * self.ip_nbytes * idx
                        ip_data = ex_mem[start:start + 2 * self.ip_nbytes]
                        new_solution.append(int.from_bytes(ip_data[:self.ip_nbytes], 'big'))
                        new_solution.append(int.from_bytes(ip_data[self.ip_nbytes:], 'big'))
                    new_solutions.append(new_solution)
                solutions = new_solutions
            del ex_mem
        return self.check_index_vectors(solutions, verbose)

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
            for idx in indices:
                hashval ^= self.compute_hash_item(idx)
            assert hashval == 0, f"{hashval = }"
        print(f"All {len(result_indices)} solutions are correct!")

def upper_bound(n):
    # for the given n, the upper bound of k for the single list algorithm
    return (n/2 + 1)**0.5

# (128, 7) bytes.fromhex("e11c0fbda860aa57d3d8d68b11be0ba5")
# (80, 4)  bytes.fromhex("d5f561bc83485b0a2732c8cb9f7f140e")

def run_single_chain_algorithm(n: int, k: int, nonce: bytes, algo_type: str, verbose: bool = False, trace_memory: bool = False):
    """
    Run single list algorithm tester with the given `algo_type` in ['plain_ip', 'plain_iv', 'ip_pr', 'ip_em', 'iv_it'].

    Args:
        n (int): parameter k for LGBP
        k (int): parameter n for LGBP
        nonce (bytes): random nonce
        algo_type (str): algorithm type, must be one of ['plain_ip', 'plain_iv', 'ip_pr', 'ip_em', 'iv_it','iv_it+']
        verbose (bool, optional): wether to print debug info. Defaults to False.
        trace_memory (bool, optional): wether to trace memory usage. Defaults to False. If True, the running time will be much longer than without it.
    """
    assert algo_type in algorithm_types
    print("==" * 10 + f"Run {algorithm_types[algo_type]}" + "==" * 10)
    print(f"{nonce.hex() = }")
    solver = single_chain_wagner_algorithm(n, 2**k, nonce=nonce)
    print(f"n = {n}, k = {k}, upper_k = {upper_bound(n)}, ell = {n/(k+1)}")
    st = time.time()
    if trace_memory:
        results = run_with_memory_trace(solver.algos[algo_type], verbose=verbose)
    else:
        results = solver.algos[algo_type](verbose=verbose)
    et = time.time()
    print(f"Time taken: {et - st:.4f} seconds")
    solver.verify_results(results)
    
def pretty_print_box(header: str, content: str):
    console = Console()
    console.print(
        Panel.fit(
            f"[red]{content}[/red]",
            title=f"[red]⚠ {header}[/red]",
            border_style="red"
        )
    )

def paper_index_pointer_tests():
    n, k = (128, 7) 
    nonce = bytes.fromhex("e11c0fbda860aa57d3d8d68b11be0ba5")
    run_single_chain_algorithm(n, k, nonce, 'plain_ip', verbose=False, trace_memory=False)
    run_single_chain_algorithm(n, k, nonce, 'ip_pr', verbose=False, trace_memory=False)
    run_single_chain_algorithm(n, k, nonce, 'ip_em', verbose=False, trace_memory=False)
    run_single_chain_algorithm(n, k, nonce, 'plain_ip', verbose=False, trace_memory=True)
    run_single_chain_algorithm(n, k, nonce, 'ip_pr', verbose=False, trace_memory=True)
    run_single_chain_algorithm(n, k, nonce, 'ip_em', verbose=False, trace_memory=True)

def paper_index_vector_tests():
    n, k = (128, 7) 
    nonce = bytes.fromhex("e11c0fbda860aa57d3d8d68b11be0ba5")
    remarks = "Our proof-of-concept implementation is not optimized for index vector storage in Python. Since indices are represented as int objects, a 1-bit value and an 8-bit value occupy the same memory. As a result, the observed peak memory reduction from index trimming is less significant than the theoretical expectation. Here, our implementation only aims to validate the theoretical correctness rather than optimize performance."
    pretty_print_box("Remarks", remarks)
    run_single_chain_algorithm(n, k, nonce, 'plain_iv', verbose=True, trace_memory=False)
    run_single_chain_algorithm(n, k, nonce, 'iv_it', verbose=True, trace_memory=False)
    # The peak memory observed in our proof-of-concept implementation remains nearly the same, since all indices are stored as `int` in python. 
    # run_single_chain_algorithm(n, k, nonce, 'plain_iv', verbose=False, trace_memory=True)
    # run_single_chain_algorithm(n, k, nonce, 'iv_it', verbose=False, trace_memory=True)
    # run_single_chain_algorithm(n, k, nonce, 'iv_it+', verbose=False, trace_memory=True)
    


if __name__ == "__main__":
    # paper_index_pointer_tests()
    paper_index_vector_tests()