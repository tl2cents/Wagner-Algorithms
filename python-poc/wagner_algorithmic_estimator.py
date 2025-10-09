"""
Concrete time/memory estimators for Wagner's algorithmic framework.
## K-Tree Algorithm
    - Index Vector Implementation.
    - Index Pointer Implementation.
    - Index Vector Implementation with index trimming (trimmed index bit length: 1). (Our work. Though it directly uses the technique from Equihash (single-list variant), we did not find documentation or implementation using this optimization)
## Single-List/Chain Algorithm
    - Index Vector Implementation.
    - Index Vector Implementation with index trimming and xor-removal. (Our Work: Further improve memory efficiency.)
    - Index Pointer Implementation.
    - Index Pointer with Post-Retrieval/External Memory. (Our Work)
"""

from math import ceil, log, floor, sqrt

def to_log2_complexity(complexity):
    """
    Convert the complexity to log2 scale.
    """
    if type(complexity) == list or type(complexity) == tuple:
        return [log(c, 2) for c in complexity]
    return log(complexity, 2)

class Wagner_Algorithmic_Framework:

    def __init__(self, n, k):
        """ Initialize the Wagner Algorithmic Framework for GBP(n, K = 2^k): find 2^k numbers from n-bit numbers such that their XOR is zero.

        Args:
            n (int): parameter n for GBP(n, 2^k).
            k (int): parameter k for GBP(n, 2^k).

        Note:
            RGBP: Regular Generalized Birthday Problem. Find x1 in L1, x2 in L2, ..., xK in LK such that x1 ⊕ x2 ⊕ ... ⊕ xK = 0. ==> k-Tree algorithm
            LGBP: Loose Generalized Birthday Problem. Find x1, x2, ..., xK in LK (single list) such that x1 ⊕ x2 ⊕ ... ⊕ xK = 0. ==> single-chain algorithm
        """
        self.n = n
        self.k = k
        assert n % (k + 1) == 0, "Invalid parameters"
        self.ell = n/(k + 1)
        self.N = 2**(self.ell + 1)
        self.k_tree_time_baseline = None
        self.single_list_time_baseline = None
    
    def k_tree_time_estimator(self):
        """
        Estimate the time complexity of the plain K-list algorithm. Generate the leafs while performing `merge` simultaneously. Not count the complexity of generating leaf lists since the practical case differs. 
        """
        if self.k_tree_time_baseline is None:
            self.k_tree_time_baseline = (2**self.k - 1) * 2**(1 + self.ell)
        return self.k_tree_time_baseline

    def k_tree_iv_estimator(self):
        """
        Estimate the memory and time complexity of the plain K-list algorithm with index vector.
        """
        return ((self.k**2 + 5*self.k + 2)/4 + 2**(self.k-1)) * self.ell * self.N, self.k_tree_time_estimator()

    def k_tree_ip_estimator(self):
        """
        Estimate the memory and time complexity of the plain K-list algorithm with index pointer.
        """
        return ((self.k**2 + self.k - 6)/4 + 2**self.k) * self.ell * self.N, self.k_tree_time_estimator()

    def k_tree_reduced_size(self, t: int = 0, verbose: bool = False):
        """
        Estimate the time and memory complexity of the K-list algorithm with initial list size reduced to N / 2^(t + 1). For t = 0, this is the standard K-list algorithm (2^ell = N/2).

        Args:
            t (int): The reduction factor 2^t for the initial list size.
            verbose (bool): Whether to print verbose output.

        Returns:
            tuple: A tuple containing the estimated memory and time complexity.
        """

        # def N_h(h): return 2 ** (self.ell - t * 2**h)  # list size at height h
        def N_h(h): return max(2 ** (self.ell - t * 2**h), 1)  # list size at height h, size >= 1 if solution exists
        
        # (when merging the last two leafs) the peak memory of k-list algorithm: each layer has one list except in layer 0 (2 lists) and layer k (0 lists)
        M = self.n * N_h(0) * 2  # memory of two list at height 0
        idx_len = self.ell
        T = 2**(self.k - 1) * 2 * N_h(0)  # time of merging leafs at height 0
        if verbose: print(f"List Size of Layer 0 (reduced factor {2**t}): 2^{to_log2_complexity(N_h(0)):.2f}")
        for i in range(1, self.k):
            if verbose: print(f"List Size of Layer {i} (reduced factor {2**t}): 2^{to_log2_complexity(N_h(i)):.2f}")
            M += ((2**i) * idx_len + self.n - i * self.ell) * N_h(i)
            T += 2**(self.k - 1 - i) * 2 * N_h(i)
        return M, T

    def _search_best_k_tree_iv_index_trimming(self, verbose: bool = False):
        """
        The index trimming technique can be applied in K-list algorithm while not increasing the time complexity significantly. 
        We search the best index trimming bit length for K-list algorithm. We only consider the depth of 2. Usually, the best trimming length is found to be 1.
        """
        # memory for storing XOR values
        if verbose: print("##########"*8)
        Mem_XOR = ((self.k**2 + 5*self.k + 2)/4) * self.ell * self.N
        # memory for storing index vectors
        def Mem_IDX(t): return 2**(self.k-1) * t * self.N
        def M_first_run(t): return Mem_XOR + Mem_IDX(t)
        for t in range(1, ceil(self.ell)):
            first_run_mem = M_first_run(t)
            second_run_mem, second_run_time = self.k_tree_reduced_size(t, verbose)
            if first_run_mem >= second_run_mem:
                if verbose: print(f"Trade-off for K-list algorithm: n = {self.n}, k = {self.k}, t = {t}, first_run_mem = 2^{to_log2_complexity(first_run_mem):.2f}, second_run_mem = 2^{to_log2_complexity(second_run_mem):.2f}, second_run_time = {second_run_time/self.k_tree_time_estimator():.2f} * T0")
                return t, first_run_mem, self.k_tree_time_estimator() + second_run_time

    def k_tree_iv_it_estimator(self, verbose: bool = False):
        """
        Estimate the memory complexity of the K-list algorithm with index vector and index trimming (trimmed index bit length: 1).
        """
        # return ((self.k**2 + 5*self.k + 2)/4 + 2**(self.k-1)) * self.ell * self.N
        trimming_length, mem, time =  self._search_best_k_tree_iv_index_trimming(verbose)
        return mem, time

    def single_list_time_estimator(self):
        """
        Estimate the time complexity of the single-list algorithm.
        """
        if self.single_list_time_baseline is None:
            self.single_list_time_baseline = self.k * self.N
        return self.single_list_time_baseline

    def single_list_iv_estimator(self):
        """
        Estimate the memory complexity of the plain single-list index vector algorithm.
        """
        # Peak memory at Layer k - 1 of size N. List item: 2^(k-1) * (ell + 1) for index vectors and 2*ell for the hash value.
        return (2**(self.k-1) * (self.ell + 1) + 2*self.ell) * self.N, self.single_list_time_estimator()
    
    def activating_height(self, t: int, verbose: bool = False):
        """
        Get the activating height with `t`-bit partial solution vector as a constraint.
        
        Args:
            t (int): the trimmed index bit length.
            verbose (int): wether to print debug info.
        
        ## Returns:
            - threshold_h (int): the activating height.
            - max_candidates (list[int]): number of candidates of t-bit index vector in each layer constrained by partial solution.
            - max_permutations (list[int]): number of permutations of t-bit index vector in each layer.
        """
        max_permutations = [2**t] # At layer 0, the total number of permutations considering t-bit partial index
        max_candidates = [2**self.k] # At layer 0, the total number of candidates considering partial solution: S = (i_1, i_2, ..., i_{2^k})
        threshold_h = None
        find = False
        for i in range(1, self.k):
            max_cand_i = 2**(self.k - i)
            if not find:
                # index vector with length 2^i, each index has 2^t choices
                max_perm_i = (2**t) ** (2**i) 
            else:
                # permutations will be limited, the number of index is 2 **(self.k - i + 1) ** 2
                max_perm_i = (max_cand_i * 2) ** 2
            max_candidates.append(max_cand_i)
            max_permutations.append(max_perm_i)
            if max_cand_i < max_perm_i and not find:
                # the constraint will be activated at height i
                # the list at threshold_h is of size O(N) and the subsequent list size will be << N.
                threshold_h = i
                find = True
                if verbose: print(f"Activated height for single-list algorithm(n = {self.n}, k = {self.k}, t = {t}): threshold_h = {threshold_h}")
        return threshold_h, max_candidates, max_permutations
    
    def civ_list_sizes_with_constraints(self, t: int, verbose: bool = False):
        max_permutations = [2**t] # At layer 0, the total number of permutations considering t-bit partial index
        max_candidates = [2**self.k] # At layer 0, the total number of candidates considering partial solution: S = (i_1, i_2, ..., i_{2^k})
        threshold_h = None
        find = False
        for i in range(1, self.k):
            max_cand_i = 2**(self.k - i)
            if not find:
                # index vector with length 2^i, each index has 2^t choices
                max_perm_i = (2**t) ** (2**i) 
            else:
                # permutations will be limited, the number of index is 2 **(self.k - i + 1) ** 2
                max_perm_i = (max_cand_i * 2) ** 2
            max_candidates.append(max_cand_i)
            max_permutations.append(max_perm_i)
            if max_cand_i < max_perm_i and not find:
                # the constraint will be activated at height i
                # the list at threshold_h is of size O(N) and the subsequent list size will be << N.
                threshold_h = i
                find = True
                if verbose: print(f"Activated height for single-list algorithm(n = {self.n}, k = {self.k}, t = {t}): threshold_h = {threshold_h}")
        layer_sizes = []
        current_size = self.N
        for i in range(self.k):
            layer_sizes.append(current_size)
            # constrains and self-merge 
            if i == self.k - 1:
                break
            if i < threshold_h - 1:
                assert max_candidates[i + 1] >= max_permutations[i + 1]
                current_size = current_size
            else:
                assert max_candidates[i + 1] < max_permutations[i + 1]
                current_size = max((max_candidates[i + 1] / max_permutations[i + 1]) * (current_size * (current_size - 1)/ 2) / 2**self.ell, 1)
        return threshold_h, layer_sizes
    
    def civ_concrete_parameters(self, trimmed_length: int, switching_height1: int, switching_height2: int, verbose: bool = False):
        """ Return the details of the given trade-off parameters.

        Args:
            trimmed_length (int): the trimmed index bit length.
            switching_height1 (int): switching height in the first run (h1).
            switching_height2 (int): switching height in the second run (h2).
        """
        # Note that the index can be omitted in layer 0 since it's sequential as 0, 1, 2, ...
        switching_height1 = 0 if switching_height1 is None else switching_height1
        switching_height2 = 0 if switching_height2 is None else switching_height2
        full_index_vector_size = lambda h: 2**h * (self.ell + 1) + self.n - h * self.ell if h != 0 else self.n
        trimmed_index_vector_size = lambda h: 2**h * trimmed_length + self.n - h * self.ell if h != 0 else self.n
        xor_removal_vector_size = lambda h: 2**h * (self.ell + 1) if h != 0 else 0
        # list item size in the first run
        list_item_size1 = lambda h: xor_removal_vector_size(h) if h < switching_height1 else trimmed_index_vector_size(h)
        list_item_size2 = lambda h: xor_removal_vector_size(h) if h < switching_height2 else full_index_vector_size(h)
        list_sizes1 = [self.N for _ in range(self.k)]
        threshold_h, list_sizes2 = self.civ_list_sizes_with_constraints(trimmed_length, verbose)
        # the peak memory is max(L0, L_{h2}, L_{k-1})
        list_mems1 = [list_sizes1[h] * list_item_size1(h) for h in range(self.k)]
        list_mems2 = [list_sizes2[h] * list_item_size2(h) for h in range(self.k)]
        peak_layer1, peak_mem1 = max(enumerate(list_mems1), key=lambda x: x[1])
        peak_layer2, peak_mem2 = max(enumerate(list_mems2), key=lambda x: x[1])
        runtime1 = sum([list_sizes1[h] * 2**h for h in range(1, switching_height1)] + list_sizes1[:])
        runtime2 = sum([list_sizes2[h] * 2**h for h in range(1, switching_height2)] + list_sizes2[:])
        runtime = runtime1 + runtime2
        assert peak_mem1 > peak_mem2, "Probably NOT an optimal trade-off."
        peak_mem = max(peak_mem1, peak_mem1)
        if verbose:
            print("###########" * 4 + f" Concrete Index Vector Trade-off GBP({self.n}, {self.k}) " + "##########"*4)
            print(f"Trimmed length: {trimmed_length}, Switching Height 1: {switching_height1}, Switching Height 2: {switching_height2}, Activated Height {threshold_h}.")
            print(f"Overall Peak Memory:  2^{to_log2_complexity(peak_mem):.2f}, Overall runtime {runtime/self.single_list_time_estimator():.2f} * T0.")
            print("    " + f"Peak Memory of first run 2^{to_log2_complexity(peak_mem1):.2f} at layer {peak_layer1}")
            print("    " + f"Runtime of first run {runtime1/self.single_list_time_estimator():.2f} * T0")
            print("    " + f"Peak Memory of second run 2^{to_log2_complexity(peak_mem2):.2f} at layer {peak_layer2}")
            print("    " + f"Runtime of second run {runtime2/self.single_list_time_estimator():.2f} * T0")
        
        results = {
            "(n,k)": (self.n, self.k),
            "trimmed_length": trimmed_length,
            "peak_mem": peak_mem,
            "runtime": runtime,
            "activating_height": threshold_h,
            "switching_height1": switching_height1,
            "switching_height2": switching_height2,
            "runtime1": runtime1,
            "runtime2": runtime2,
            "peak_mem1": peak_mem1,
            "peak_mem2": peak_mem2,
            "peak_layer1": peak_layer1,
            "peak_layer2": peak_layer2,            
        }
        return results

    def search_best_single_list_iv_it(self, xor_removal: bool = True, verbose: bool = False):
        """
        Search for the best index trimming bit length for single-list algorithm with limited XOR-removal. Make sure the time complexity is acceptable (approximately twofold~threefold).

        Args:
            xor_removal (bool, optional): If True, use limited XOR-removal. Defaults to True.
            verbose (bool, optional): If True, print detailed information. Defaults to False.
            
        ## Returns
            - first_run_mem (int): the total peak memory.
            - trimmed_length (int): the trimmed index bit length. 
            - threshold_h (int): the activated height in the second run.
            - switching_height1: switching height from XOR-removal to index trimming in the first run.
            - switching_height2: switching height from XOR-removal to index trimming in the second run.
            - runtime_overhead: the extra runtime overhead in the trade-off (limited in range 0~2*T0) where T0 is the time complexity of plain algorithm.
        
        Note:
            If `xor_removal` is enabled, we will use it in the first run. We will always use limited XOR-removal in the second run.
        """
        full_index_vector_size = lambda h: 2**h * (self.ell + 1) + self.n - h * self.ell if h != 0 else self.n
        trimmed_index_vector_size = lambda h, t: 2**h * t + self.n - h * self.ell if h != 0 else self.n
        xor_removal_vector_size = lambda h: 2**h * (self.ell + 1) if h != 0 else 0
        switching_height1 = None
        runtime_overhead1 = 0
        # The max XOR-removal height to limit the time penalty within twofold.
        max_xor_removal_depth = floor(log(self.k, 2))
        for trimmed_length in range(1, ceil(self.ell)):
            if verbose: print(f"Try {trimmed_length = } for single-list-index-vector algorithm with n = {self.n}, k = {self.k}")
            first_run_mem_layer_0 = self.n * self.N
            # first_run_mem_layer_k1 = (2**(self.k - 1) * trimmed_length + 2 * self.ell)  * self.N
            first_run_mem_layer_k1 = trimmed_index_vector_size(self.k - 1, trimmed_length) * self.N
            if first_run_mem_layer_0 > first_run_mem_layer_k1 and xor_removal:
                # If the memory usage at layer 0 (peak) is greater than at layer k-1, we can use xor removal to further reduce memory usage.
                # until layer max_xor_removal_depth, we use limited xor removal: peak = max(LayerMem(max_xor_removal_depth - 1), LayerMem(max_xor_removal_depth), first_run_mem_layer_k1)
                first_run_mem = first_run_mem_layer_0
                switching_height1 = None
                runtime_overhead1 = None
                for d in range(max_xor_removal_depth + 1): # or max_xor_removal_depth + 1
                    # first_run_mem_layer_d1 = (2**d * (self.ell + 1))  * self.N
                    # first_run_mem_layer_d2 = (2**(d + 1) * trimmed_length + self.n - (d + 1) * self.ell)  * self.N
                    first_run_mem_layer_d1 = xor_removal_vector_size(d) * self.N
                    first_run_mem_layer_d2 = trimmed_index_vector_size(d + 1, trimmed_length) * self.N
                    M1 = max(first_run_mem_layer_d1, first_run_mem_layer_d2, first_run_mem_layer_k1)
                    if M1 < first_run_mem_layer_k1:
                        # the memory usage of first run is lower bounded by `first_run_mem_layer_k1`
                        first_run_mem = first_run_mem_layer_k1
                        switching_height1 = d + 1
                        runtime_overhead1 = (2**(d+1) - 2) * self.N # recompute layer 1 ~ d: sum 2^1 + 2^2 + ... + 2^d
                        break
                    if M1 < first_run_mem:
                        first_run_mem = M1
                        switching_height1 = d + 1
                        runtime_overhead1 = (2**(d+1) - 2) * self.N # recompute layer 1 ~ d: sum 2^1 + 2^2 + ... + 2^d
                assert switching_height1 is not None
            else:
                first_run_mem = first_run_mem_layer_k1
            # now consider the memory of the second run 
            threshold_h, max_candidates, max_permutations = self.activating_height(trimmed_length, verbose)
            # calculate the layer sizes of each layer
            layer_sizes = []
            layer_mems = []
            current_size = self.N
            runtime_overhead2 = 0
            switching_height2 = None
            # if verbose: print(f"The second run with {trimmed_length}-bit partial solution.")
            for i in range(self.k):
                # if verbose: print(f"    List size of layer {i}, 2^{to_log2_complexity(current_size):.2f}")
                layer_sizes.append(current_size)
                # no xor-removal case.
                layer_mems.append(full_index_vector_size(i) * current_size)
                runtime_overhead2 += current_size
                # constrains and self-merge 
                if i == self.k - 1:
                    break
                if i < threshold_h - 1:
                    assert max_candidates[i + 1] >= max_permutations[i + 1]
                    current_size = current_size
                else:
                    assert max_candidates[i + 1] < max_permutations[i + 1]
                    current_size = max((max_candidates[i + 1] / max_permutations[i + 1]) * (current_size * (current_size - 1)/ 2) / 2**self.ell, 1)
            if max(layer_mems) < first_run_mem:
                runtime_overhead = runtime_overhead1 + runtime_overhead2
                if verbose:
                    print(f"Best Trade-off {trimmed_length = } for n = {self.n}, k = {self.k} with {xor_removal = }")
                    if xor_removal and switching_height1 is not None:
                        print(f"    XOR-Removal in the first run: switching height {switching_height1}, runtime overhead for XOR-removal: {runtime_overhead1/self.single_list_time_estimator():.2f} * T0")
                    else:
                        print(f"    No XOR-Removal in the first run!")
                    print(f"    Activated layer for constraints: {threshold_h}")
                    print(f"    Runtime overhead for the second run (no xor-removal) {runtime_overhead2/self.single_list_time_estimator():.2f} * T0")
                    print(f"    Total Peak Memory: 2^{to_log2_complexity(first_run_mem):.2f}")
                    print(f"    Total Runtime Overhead {runtime_overhead/self.single_list_time_estimator():.2f} * T0")
                return first_run_mem, trimmed_length, threshold_h, switching_height1, switching_height2, runtime_overhead
            else:
                for d in range(threshold_h + 2):
                    layer_d1 = (2**d * (self.ell + 1)) * layer_sizes[d]
                    layer_d2 = (2**(d + 1) * trimmed_length + self.n - (d + 1) * self.ell) * layer_sizes[d + 1]
                    M2 = max(layer_d1, layer_d2)
                    if M2 < first_run_mem:
                        runtime_overhead2 += sum([2**h * layer_sizes[h] for h in range(1, d + 1)]) # recompute layer 1 ~ d: sum 2^1 + 2^2 + ... + 2^d
                        runtime_overhead = runtime_overhead1 + runtime_overhead2
                        switching_height2 = d + 1
                        if verbose: 
                            print(f"Best Trade-off {trimmed_length = } for n = {self.n}, k = {self.k} with {xor_removal = }")
                            if xor_removal and switching_height1 is not None:
                                print(f"    XOR-Removal in the first run: switching height {switching_height1}, runtime overhead for XOR-removal: {runtime_overhead1/self.single_list_time_estimator():.2f} * T0")
                            else:
                                print(f"    No XOR-Removal in the first run!")
                            print(f"    Activated layer for constraints: {threshold_h}")
                            print(f"    XOR-Removal in the second run: switching height {switching_height2}, runtime overhead for second run: {runtime_overhead2/self.single_list_time_estimator():.2f} * T0")
                            print(f"    Total Peak Memory: 2^{to_log2_complexity(first_run_mem):.2f}")
                            print(f"    Total Runtime Overhead {runtime_overhead/self.single_list_time_estimator():.2f} * T0")
                        return first_run_mem, trimmed_length, threshold_h, switching_height1, switching_height2, runtime_overhead
                    
    def single_list_iv_it_estimator(self, verbose: bool = False):
        """
        Search for the best index trimming bit length for single-list algorithm with limited XOR-removal. Make sure the time complexity is acceptable (approximately twofold~threefold).
            
        ## Returns (verbose=False)
            - peak memory (int): the total peak memory.
            - total runtime: the total runtime in the trade-off (in range T0 ~ 3*T0) where T0 is the time complexity of plain algorithm.
            
        ## Returns (verbose=True)
            - detailed information dictionary of this trade-off.
        """
        if verbose: print("##########"*8)
        peak_mem, trimmed_length, threshold_h, switching_height1, switching_height2, runtime_overhead = self.search_best_single_list_iv_it(verbose=verbose)
        if verbose: 
            return self.civ_concrete_parameters(trimmed_length, switching_height1, switching_height2, verbose)
        else:
            return peak_mem, self.single_list_time_estimator() + runtime_overhead
    
    def single_list_ip_estimator(self, trade_type: str = "plain", verbose: bool = False):
        assert trade_type in ["plain", "post_retrieval", "external_memory"]
        if verbose: print("##########"*8)
        if trade_type == "plain":
            return 2*(self.n + self.k - self.ell - 1) * self.N, self.single_list_time_estimator()
        elif trade_type == "post_retrieval":
            result = self.search_best_ip_with_post_retrieval(verbose)
            return result["peak_mem"], result["runtime"]
        else:
            # use external memory to cache index pointers: in the case, we assume the complexity of writing index pointer from memory to external storage (i.e., SSD) is exactly 1 (equal to a hash call in this sense).
            return self.n * self.N, 2 * self.single_list_time_estimator()
    
    def search_best_ip_with_post_retrieval(self, verbose: bool = False):
        """ Search for the best height to switch from post-retrieval to index-pointer such that the peak memory is limited to nN.

        Args:
            verbose (bool, optional): whether to print debug information. Defaults to False.
        ## Returns:
            - switching_height (int): the best switching height.
            - total_time (int): the total time complexity with post-retrieval.
        """
        h_min = ceil((self.k - 1) // 2)
        if verbose: print(f"Search best IP-PR for n = {self.n}, k = {self.k}, h_min = {h_min}")
        for switching_height in range(h_min, self.k):
            # let h = switching_height, peak memory before switching height:
            # Post-retrieval: X0, X1, ..., X_h: peak memory O(nN) at X_0
            # Index-pointer: X_h, X_{h+1}, ..., X_{k-1}. Peak memory at layer k-1 : I_{h+1}, I_{h+2}, ..., I_{k-1}, X_{k-1}
            #                peak memory: M = (k - 1 - h) * 2 * (ell + 1) * N + 2 * ell * N
            M = (self.k - 1 - switching_height) * 2 * (self.ell + 1) * self.N + 2 * self.ell * self.N
            # another expression of M (just for verification):
            M_ = (self.n + (self.k - 1 - 2 * switching_height) * self.ell + 2 * (self.k - 1 - switching_height)) * self.N
            assert M == M_, "Check the peak memory calculation."
            if M <= self.n * self.N:
                # best trade-off with peak memory nN
                if verbose: print(f"Best Trade-off for IP-PR: switching height {switching_height}")
                runtime_overhead = switching_height * (switching_height+1) / 2 * self.N # recompute I_1 ~ I_h
                runtime = runtime_overhead + self.single_list_time_estimator()
                result = {
                    "(n,k)": (self.n, self.k),
                    "peak_mem": self.n * self.N,
                    "runtime": runtime,
                    "switching_height": switching_height,
                    }
                return result
    
    def search_best_hybrid_single_chian(self, h1_upper: int = 2, h2_min: int = None, max_gamma: float = 4.0, verbose: bool = False):
        """ Find the best parameters for hybrid_single_chian algorithm.

        Args:
            h1_upper(int, optional): the max layer height for applying XOR-Removal technique.
            h2_min(int, optional): the min starting height for storing index pointer.
            max_gamma(int, optional): the max time penalty factor.
            verbose (bool, optional): whether to print debug information. Defaults to False.
        """
        h2_min = ceil((self.k - 1) / 2) if h2_min is None else h2_min
        M0, T0 = self.single_list_ip_estimator()
        best_paras = None
        if verbose: print(f"Peak memory for plain cip{(self.n, self.k)}: {to_log2_complexity(M0):.2f}")
        for h1 in range(h1_upper + 1):
            # 0~h1: XOR removal + Index Vector 
            for h2 in range(max(h2_min, h1 + 1), self.k):
                # (h1 + 1) ~ h2: Index Removal (Post Retrieval)
                # (h2 + 1) ~ k-1: Index Pointer
                M1 = 2**(h1) * (self.ell + 1) * self.N
                M2 = (self.n - (h1 + 1) * self.ell) * self.N
                M3 = ((self.k - 1 - h2) * 2 * (self.ell + 1) + 2 * self.ell) * self.N
                peak_mem = max(M1, M2, M3)
                if peak_mem == M1:
                    peak_layer = h1
                elif peak_mem == M2:
                    peak_layer = h1 + 1
                else:
                    peak_layer = self.k - 1
                extra_runtime = sum([i for i in range(h1, h2+1)]) * self.N # post retrieval
                extra_runtime += (2**(h1 + 1) - 2) * self.N * (h2 + 1 - h1 + 1) # xor-removal
                if verbose: print(f"{h1 = }, {h2 = }, peak: {to_log2_complexity(peak_mem):.2f}, {peak_layer = }, extra runtime {extra_runtime/T0:.2f} * T0")
                if (extra_runtime/T0 + 1) <= max_gamma:
                    if best_paras is None or best_paras[0] > peak_mem:
                        best_paras = (peak_mem, extra_runtime, h1, h2, peak_layer)
        result = {
            '(n,k)': (self.n, self.k),
            'peak_mem': best_paras[0],
            'runtime': best_paras[1] + T0,
            'switching_height1': best_paras[2],
            'switching_height2': best_paras[3],
            'peak_layer': best_paras[4],
        }
        return result
    
def test():
    waf = Wagner_Algorithmic_Framework(200, 9)
    waf.k_tree_iv_it_estimator(verbose=True)
    waf.single_list_iv_it_estimator(verbose=True)
    waf = Wagner_Algorithmic_Framework(96, 3)
    waf.single_list_iv_it_estimator(verbose=True)
    waf = Wagner_Algorithmic_Framework(96, 5)
    waf.single_list_iv_it_estimator(verbose=True)
    waf = Wagner_Algorithmic_Framework(160, 9)
    waf.single_list_iv_it_estimator(verbose=True)
    waf = Wagner_Algorithmic_Framework(144, 5)
    waf.single_list_iv_it_estimator(verbose=True)
    waf = Wagner_Algorithmic_Framework(150, 5)
    waf.single_list_iv_it_estimator(verbose=True)
    waf = Wagner_Algorithmic_Framework(288, 8)
    waf.single_list_iv_it_estimator(verbose=True)
    waf = Wagner_Algorithmic_Framework(128, 7)
    waf.single_list_iv_it_estimator(verbose=True)

        
if __name__ == "__main__":
    # test()
    waf = Wagner_Algorithmic_Framework(200, 9)
    waf.search_best_hybrid_single_chian(verbose=True)
    waf = Wagner_Algorithmic_Framework(144, 5)
    waf.search_best_hybrid_single_chian(verbose=True)