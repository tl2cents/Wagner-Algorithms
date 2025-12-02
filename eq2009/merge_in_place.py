""" 
Proof of concept implementations of in-place merge (colliding ell bits). The underlying sorting algorithms include:
    - in_place_quick_sort: O(n log n) time complexity.
    - in_place_heap_sort: O(n log n) time complexity.
    - in_place_counting_sort (or bucket/radix sort): O(n) time complexity with O(n + k) space complexity where k is the range of input values.
    - inplace_radix_sort: almost O(n) time complexity with O(1) space complexity.

## More references and advanced implementations.
    Technique Blog:
    - https://duvanenko.tech.blog/2022/04/09/in-place-binary-radix-sort/
    - https://duvanenko.tech.blog/2022/04/10/in-place-n-bit-radix-sort/
    - https://github.com/DragonSpit/ParallelAlgorithms
    - https://github.com/voutcn/kxsort
    - Wiki: https://en.wikipedia.org/wiki/Radix_sort#In-place_MSD_radix_sort_implementations
  
    Paper:
    - Fast In-place Integer Radix Sorting: https://link.springer.com/chapter/10.1007/11428862_107
    - Theoretically-Efficient and Practical Parallel In-Place Radix Sorting: https://jshun.csail.mit.edu/RegionsSort.pdf
    - PARADIS: an efficient parallel algorithm for in-place radix sort: https://dl.acm.org/doi/10.14778/2824032.2824050
"""
import random
import tracemalloc
import os, psutil, threading, time, gc
import argparse


def get_mem_mb():
    process = psutil.Process(os.getpid())
    return process.memory_info().rss / 1024 / 1024

def monitor_memory(func, *args, interval=0.01, **kwargs):
    """Measure the memory while running func. The memory already used before calling func will also be counted."""
    gc.collect()
    start_mem = get_mem_mb()
    peak_mem = start_mem
    stop_flag = False
    func_name = func.__name__
    print(f"Measuring memory for function: {func_name}")
    def sampler():
        nonlocal peak_mem
        while not stop_flag:
            m = get_mem_mb()
            if m > peak_mem:
                peak_mem = m
            time.sleep(interval)
    t = threading.Thread(target=sampler, daemon=True)
    t.start()
    result = func(*args, **kwargs)
    stop_flag = True
    t.join()
    gc.collect()
    end_mem = get_mem_mb()
    print(f"Before {func_name}: {start_mem:.3f} MB")
    print(f"Peak while running {func_name}: {peak_mem:.3f} MB")
    print(f"After {func_name}: {end_mem:.3f} MB")
    return result


def monitor_extra_memory(func, *args, **kwargs):
    """Measure the extra memory used by func using tracemalloc. The memory already used before calling func is will not be counted."""
    func_name = func.__name__
    print(f"Measuring extra allocated memory for function: {func_name}")
    gc.collect()
    tracemalloc.start()
    result = func(*args, **kwargs)
    current, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    gc.collect()
    end_current = current
    print(f"Extra allocated peak memory while running {func_name}: {peak / 1024 / 1024:.3f} MB")
    print(f"After {func_name}, extra allocated memory: {end_current / 1024 / 1024:.3f} MB")
    return result

def quick_sort_inplace(arr, key=lambda x: x):
    """In-place quick sort implementation for ell-bit suffix."""
    if len(arr) <= 1:
        return arr

    def partition(low, high):
        pivot = key(arr[high])
        i = low - 1
        for j in range(low, high):
            if key(arr[j]) < pivot:
                i += 1
                arr[i], arr[j] = arr[j], arr[i]
        arr[i + 1], arr[high] = arr[high], arr[i + 1]
        return i + 1

    def quick_sort_recursive(low, high):
        if low < high:
            pi = partition(low, high)
            quick_sort_recursive(low, pi - 1)
            quick_sort_recursive(pi + 1, high)

    quick_sort_recursive(0, len(arr) - 1)
    
def heap_sort_inplace(arr, key=lambda x: x):
    n = len(arr)

    def heapify(n, i):
        largest = i
        l = 2*i + 1
        r = 2*i + 2

        if l < n and key(arr[l]) > key(arr[largest]):
            largest = l
        if r < n and key(arr[r]) > key(arr[largest]):
            largest = r
        if largest != i:
            arr[i], arr[largest] = arr[largest], arr[i]
            heapify(n, largest)

    # Build max heap
    for i in range(n // 2 - 1, -1, -1):
        heapify(n, i)

    # Extract elements
    for i in range(n-1, 0, -1):
        arr[i], arr[0] = arr[0], arr[i]
        heapify(i, 0)
        
        
def count_sort_partial_inplace(arr, kmax, key=lambda x: x):
    """ Count Sort in linear time and memory O(n + k_max).
    from https://stackoverflow.com/questions/15682100/sorting-in-linear-time-and-in-place. 
    """
    n = len(arr)
    counts = [0] * kmax

    # 1. counting
    for val in arr:
        counts[key(val)] += 1

    # 2. prefix sum
    total = 0
    for val in range(kmax):
        tmp = counts[val]
        counts[val] = total
        total += tmp

    # 3. in-place rearrangement
    for i in range(n - 1, -1, -1):
        val = arr[i]
        j = counts[key(val)]
        if j < i:
            while True:
                counts[key(val)] += 1
                # swap
                tmp = val
                val = arr[j]
                arr[j] = tmp
                j = counts[key(val)]
                if not (j < i):
                    break
            arr[i] = val


def radix_sort_inplace(arr, n_lsb):
    """ Radix sort for n_lsb bits. Use buckets of size 2^8
    """
    if n_lsb % 8 == 0:
        initial_shift = n_lsb - 8
    else:
        initial_shift = n_lsb  - n_lsb % 8
    key_mask = (1 << n_lsb) - 1
    key = lambda x: x & key_mask
    radix_bits = 8
    radix_size = 1 << radix_bits
    radix_mask = radix_size - 1
    
    def sort_recursive(start, end, bit_shift):
        
        if start >= end - 1 or bit_shift < 0:
            return
        counts = [0] * radix_size
        for i in range(start, end):
            digit = (key(arr[i]) >> bit_shift) & radix_mask
            counts[digit] += 1

        offsets = [0] * radix_size
        offsets[0] = start
        for i in range(1, radix_size):
            offsets[i] = offsets[i-1] + counts[i-1]
        
        bucket_starts = list(offsets)

        for b in range(radix_size): 
            while offsets[b] < bucket_starts[b] + counts[b]:
                current_pos = offsets[b]
                current_elem = arr[current_pos]
                elem_digit = (key(current_elem) >> bit_shift) & radix_mask

                if elem_digit == b:
                    offsets[b] += 1
                else:
                    dest_pos = offsets[elem_digit]
                    arr[current_pos], arr[dest_pos] = arr[dest_pos], arr[current_pos]
                    offsets[elem_digit] += 1
        
        next_bit_shift = bit_shift - radix_bits
        for b in range(radix_size):
            sort_recursive(bucket_starts[b], bucket_starts[b] + counts[b], next_bit_shift)

    sort_recursive(0, len(arr), initial_shift)

def merge_sorted_array_inplace(arr: list, ell: int):
    n = len(arr)
    write = 0
    i = 0
    mask = (1 << ell) - 1
    group = []
    max_group_length = 0
    while i < n:
        key = arr[i] & mask
        start = i
        while i < n and (arr[i] & mask) == key:
            i += 1
        end = i
        for i1 in range(start, end):
            for i2 in range(i1 + 1, end):
                group.append((arr[i1] ^ arr[i2]) >> ell)
        if len(group) > max_group_length:
            max_group_length = len(group)
        if len(group) + write <= i:
            arr[write:write + len(group)] = group
            write += len(group)
            group = []
        else:
            # not enough space to write in place
            arr[write:end] = group[:end - write]
            group = group[end - write:]
            write = end
    print(f"Max temporary array length in merge: {max_group_length}")
    if len(group) > 0:
        assert write == n
        arr.extend(group)
    else:
        del arr[write:]
    return arr

def merge_qsort_inplace(arr: list, ell: int):
    """In-place merge using quick sort implementation for ell-bit suffix."""
    mask = (1 << ell) - 1
    quick_sort_inplace(arr, key=lambda x: x & mask)
    return merge_sorted_array_inplace(arr, ell)

def merge_hsort_inplace(arr: list, ell: int):
    """In-place heap using heap sort implementation for ell-bit suffix."""
    mask = (1 << ell) - 1
    quick_sort_inplace(arr, key=lambda x: x & mask)
    return merge_sorted_array_inplace(arr, ell)

def merge_csort_NOT_inplace(arr: list, ell: int):
    """Merge using count sort implementation for ell-bit suffix."""
    mask = (1 << ell) - 1
    count_sort_partial_inplace(arr, 2**ell, key=lambda x: x & mask)
    return merge_sorted_array_inplace(arr, ell)

def merge_rsort_inplace(arr: list, ell: int):
    """Merge using radix sort implementation for ell-bit suffix."""
    radix_sort_inplace(arr, ell)
    return merge_sorted_array_inplace(arr, ell)

def merge_timsort_NOT_inplace(arr: list, ell: int):
    """Merge using python's built-in tim-sort implementation for ell-bit suffix."""
    mask = (1 << ell) - 1
    arr.sort(key=lambda x: x & mask)
    # Tim-sort is not in-place sort. It uses extra memory in C which will not be released until program ends
    return merge_sorted_array_inplace(arr, ell)

def run_random_case(n:int =200, k:int =9, seed: str = None, sorting_method: str ='rsort'):
    seed = int(seed, 16) if seed is not None else 0xdeadbeef
    random.seed(0xdeadbeef)
    assert n % (k + 1) == 0, "n must be divisible by k + 1"
    ell = n // (k + 1)
    data = [random.getrandbits(n) for _ in range(2**(ell + 1))]
    print("Original Data length:", len(data))
    current_mem = get_mem_mb()
    print(f"Current memory: {current_mem:.3f} MB")
    if sorting_method == 'qsort':
        monitor_memory(merge_qsort_inplace, data, ell)
    elif sorting_method == 'hsort':
        monitor_memory(merge_hsort_inplace, data, ell)
    elif sorting_method == 'csort':
        monitor_memory(merge_csort_NOT_inplace, data, ell)
    elif sorting_method == 'tsort':
        monitor_memory(merge_timsort_NOT_inplace, data, ell)
    elif sorting_method == 'rsort':
        monitor_memory(merge_rsort_inplace, data, ell)
    else:
        raise ValueError(f"Unknown sorting method: {sorting_method}")
    # check is sorted
    print("Merged data length:", len(data))
    
    
if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Run Python PoC for In-place Merge.")
    parser.add_argument('--n', type=int, default=200, help='Parameter n (default: 200)')
    parser.add_argument('--k', type=int, default=9, help='Parameter k (default: 9)')
    parser.add_argument('--seed', type=str, default=None, help='Hex string for seed (default: 0xdeadbeef)')
    parser.add_argument('--algo', type=str, default='rsort', choices=['qsort', 'hsort', 'csort', 'rsort', 'tsort'],
                        help='Sorting algorithm: qsort, hsort, csort, rsort, tsort (default: rsort)')
    args = parser.parse_args()
    
    run_random_case(n=args.n, k=args.k, seed=args.seed, sorting_method=args.algo)