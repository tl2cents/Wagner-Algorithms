""" 
Proof of concept implementations of in-place merge (colliding ell bits). This includes:
    - in_place_quick_sort: O(n log n) time complexity.
    - in_place_heap_sort: O(n log n) time complexity.
    - in_place_counting_sort (or bucket sort?): O(n) time complexity with O(k) space complexity where k is the range of input values.
The paper "Optimal In-Place Suffix Sorting" by Zhize Li, Jian Li, and Hongwei Huo provides a linear time in-place sorting algorithm for suffix arrays which perfectly fits our needs. See: https://link.springer.com/chapter/10.1007/978-3-030-00479-8_22 for details.
"""
import random
import tracemalloc
import os, psutil, threading, time, gc

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


def _merge_sorted_array_inplace(arr: list, ell: int):
    n = len(arr)
    write = 0
    i = 0
    mask = (1 << ell) - 1
    while i < n:
        key = arr[i] & mask
        start = i
        while i < n and (arr[i] & mask) == key:
            i += 1
        end = i
        group = []
        for i1 in range(start, end):
            for i2 in range(i1 + 1, end):
                group.append((arr[i1] ^ arr[i2]) >> ell)
        arr[write:write + len(group)] = group
        write += len(group)
        assert write <= i, f"Write new items into slots that have not been read, {write = }, {start = }, {end = }, {len(group) = }."
        del group
    del arr[write:]
    return arr

def merge_sorted_array_inplace(arr: list, ell: int):
    n = len(arr)
    write = 0
    i = 0
    mask = (1 << ell) - 1
    group = []
    while i < n:
        key = arr[i] & mask
        start = i
        while i < n and (arr[i] & mask) == key:
            i += 1
        end = i
        for i1 in range(start, end):
            for i2 in range(i1 + 1, end):
                group.append((arr[i1] ^ arr[i2]) >> ell)
        if len(group) + write <= i:
            arr[write:write + len(group)] = group
            write += len(group)
            group = []
        else:
            # not enough space to write in place
            arr[write:end] = group[:end - write]
            group = group[end - write:]
            write = end
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
    """In-place count using count sort implementation for ell-bit suffix."""
    mask = (1 << ell) - 1
    count_sort_partial_inplace(arr, 2**ell, key=lambda x: x & mask)
    return merge_sorted_array_inplace(arr, ell)

def merge_timsort_NOT_inplace(arr: list, ell: int):
    """Merge using python's built-in tim-sort implementation for ell-bit suffix."""
    mask = (1 << ell) - 1
    arr.sort(key=lambda x: x & mask)
    # Tim-sort is not in-place sort. It uses extra memory in C which will not be released until program ends
    return merge_sorted_array_inplace(arr, ell)

if __name__ == "__main__":
    random.seed(0x123456)
    # Equihash(128,7), Equihash(200,9)
    n, k = 200, 9
    # n, k = 128, 7
    ell = n // (k + 1)
    data = [random.getrandbits(n) for _ in range(2**(ell + 1))]
    print("Original Data length:", len(data))
    current_mem = get_mem_mb()
    print(f"Current memory: {current_mem:.3f} MB")
    data = monitor_memory(merge_qsort_inplace, data, ell)
    # monitor_memory(merge_hsort_inplace, data, ell)
    # monitor_memory(merge_csort_NOT_inplace, data, ell)
    # monitor_memory(merge_timsort_NOT_inplace, data, ell)
    print("Merged data length:", len(data))
    print("Merged data (first 10 elements):", [hex(x) for x in data[:10]])