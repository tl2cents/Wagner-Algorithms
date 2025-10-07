import random
import sys
import random
import os, psutil, threading, time, gc

def get_mem_mb():
    process = psutil.Process(os.getpid())
    return process.memory_info().rss / 1024 / 1024

def monitor_memory(func, *args, interval=0.001, **kwargs):
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

# 设置更高的递归深度限制，因为对于大数据集可能会需要
sys.setrecursionlimit(3000) 

# --- 用户提供的初始代码 ---
random.seed(10)
# 为了快速验证，我们减小数组大小。
# 原始大小: 2**21。如果机器内存足够，可以改回去。
array_size = 2**21 
# array_size = 2**15 # 使用一个较小的尺寸进行演示
array = [random.getrandbits(200) for i in range(array_size)]
mask = (1 << 20) - 1
key = lambda x: x & mask

# --- 原地基数排序 (美国国旗排序) 实现 ---

def american_flag_sort(arr):
    """
    对给定的数组进行原地基数排序。
    该实现假设键是整数。
    """
    # 我们将 20 位的键分为 3 个部分: 8, 8, 4 位
    # 也可以分为 2 部分: 10, 10 位
    radix_bits = 8
    radix_size = 1 << radix_bits
    radix_mask = radix_size - 1

    def sort_recursive(start, end, bit_shift):
        """
        递归地对数组的一个切片进行排序。
        
        Args:
            start: 当前处理的子数组的起始索引。
            end: 当前处理的子数组的结束索引（不包含）。
            bit_shift: 当前要比较的位的偏移量。
        """
        if start >= end - 1 or bit_shift < 0:
            return

        # 1. 统计当前"位"上每个值的频率
        counts = [0] * radix_size
        for i in range(start, end):
            # 提取当前处理的 "位" (digit)
            digit = (key(arr[i]) >> bit_shift) & radix_mask
            counts[digit] += 1

        # 2. 计算每个桶的起始位置 (offsets)
        offsets = [0] * radix_size
        offsets[0] = start
        for i in range(1, radix_size):
            offsets[i] = offsets[i-1] + counts[i-1]
        
        # 记录每个桶的原始起始位置，用于后续的递归调用
        bucket_starts = list(offsets)

        # 3. 将元素交换到正确的桶中 (原地置换)
        # 这是算法最核心和复杂的部分
        for b in range(radix_size): # 遍历每个桶
            # offsets[b] 是桶 b 的下一个写入位置
            # bucket_starts[b] + counts[b] 是桶 b 的结束位置
            while offsets[b] < bucket_starts[b] + counts[b]:
                current_pos = offsets[b]
                current_elem = arr[current_pos]
                elem_digit = (key(current_elem) >> bit_shift) & radix_mask

                if elem_digit == b:
                    # 元素已在正确的桶中，处理下一个位置
                    offsets[b] += 1
                else:
                    # 元素位置错误，将其与它应该在的桶的第一个空位交换
                    dest_pos = offsets[elem_digit]
                    arr[current_pos], arr[dest_pos] = arr[dest_pos], arr[current_pos]
                    # 目标桶的一个位置已被填充
                    offsets[elem_digit] += 1
        
        # 4. 递归地对每个桶内部进行排序
        next_bit_shift = bit_shift - radix_bits
        for b in range(radix_size):
            sort_recursive(bucket_starts[b], bucket_starts[b] + counts[b], next_bit_shift)

    # 初始调用，从最高位开始
    # 20 位键，按 8-8-4 划分，最高位在 bit 16 附近
    initial_shift = 16 
    sort_recursive(0, len(arr), initial_shift)


# --- 运行和验证 ---
print("开始排序...")
monitor_memory(american_flag_sort, array)
print("排序完成。")

# 验证排序结果
print("开始验证排序结果...")
is_sorted = True
for i in range(len(array) - 1):
    if key(array[i]) > key(array[i+1]):
        print(f"排序错误在索引 {i}: key({array[i]}) > key({array[i+1]})")
        is_sorted = False
        break

if is_sorted:
    print("验证成功：数组已根据指定的键正确排序。")
else:
    print("验证失败：数组未正确排序。")

print("\n排序后前 10 个元素")
for i in range(min(10, len(array))):
    print(f"元素 {i}  {array[i]}")
key_array = [key(i) for i in array]
del array 
# verify
random.seed(10)
array = [random.getrandbits(200) for i in range(array_size)]
array.sort(key=key)
assert key_array == [key(i) for i in array]
print("OK")