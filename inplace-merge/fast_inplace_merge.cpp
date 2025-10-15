#include <iostream>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <numeric>
#include <random>
#include <chrono>
#include <cstring> 
#include <cassert>
#include "./kxsort/kxsort.h"
#include <cstddef>
#include <cstdlib>
#include <new>
#include <type_traits>
#define MAX_LIST_SIZE 2100000 // slightly greater than 1<<21 = 2097152

const int MASK_KEY = (1 << 20) - 1; // 20-bit key max value

// --- Data Structure Definition ---

/**
 * @brief Structure representing a data item 25 bytes
 */
struct DataItem0 {
    uint_fast8_t data[25];
};

inline uint32_t getKey0(const DataItem0 &item) {
    // 20 lsb 
    uint32_t low_4_bytes;
    std::memcpy(&low_4_bytes, item.data, sizeof(uint32_t)); 
    return low_4_bytes & 0x000FFFFF;
}

/**
 * @brief Merge two DataItem0s by XORing: (i1 ^ i2) >> 20
 * * i1 and i2 are 25 bytes. We simplify the result to be another DataItem0 (25 bytes).
 */
inline DataItem0 merge_item0(const DataItem0 &i1, const DataItem0 &i2) {
    DataItem0 result;
    uint_fast8_t xor_result[25];
    std::transform(i1.data, i1.data + 25, i2.data, xor_result, 
                   [](uint_fast8_t a, uint_fast8_t b) { return a ^ b; });
    const uint_fast8_t* src = xor_result + 2; 
    
    for (int i = 0; i < 22; ++i) {
        uint_fast8_t low_part = src[i] >> 4;
        uint_fast8_t high_part = src[i + 1] << 4;
        result.data[i] = low_part | high_part;
    }
    result.data[22] = src[22] >> 4;
    // result.data[23] = 0;
    // result.data[24] = 0;
    return result;
}

// ---  Comparison-Based Sort ---

/**
 * @brief Sorts the array using the highly optimized C++ standard library sort (IntroSort).
 * @param arr The vector of DataItems to sort.
 */
void stdSort(std::vector<DataItem0>& arr) {
    // std::sort uses a custom comparison function (lambda) to sort the DataItem objects
    std::sort(arr.begin(), arr.end(), [](const DataItem0& a, const DataItem0& b) {
        return getKey0(a) < getKey0(b);
    });
}

// --- Third Party, In-place Radix Sort. ---

struct RadixTraitsItem0 {
    static const int nBytes = 3; // 20 bits = 3 bytes
    uint32_t kth_byte(const DataItem0 &x, int k) {
        uint32_t key = getKey0(x);
    	return (key >> (k * 8)) & 0xFF;
    }
    bool compare(const DataItem0 &x, const DataItem0 &y) {
    	return getKey0(x) < getKey0(y);
    }
};

/**
 * @brief Sorts the array using the optimized in-place radix sort.
 * @param arr The vector of DataItems to sort.
 */
void kxSort(std::vector<DataItem0>& arr) {
    // std::sort uses a custom comparison function (lambda) to sort the DataItem objects
    kx::radix_sort(arr.begin(), arr.end(), RadixTraitsItem0());
}

void verify_sorted(const std::vector<DataItem0>& arr) {
    for (size_t i = 1; i < arr.size(); ++i) {
        if (getKey0(arr[i-1]) > getKey0(arr[i])) {
            std::cout << "Verification failed: array is not sorted at index " << i << std::endl;
            // assert error when not sorted
            assert(0);
        }
    }
    std::cout << "Verification passed: array is sorted." << std::endl;
    
}

void merge_inplace(std::vector<DataItem0>& arr, void (*sort_func)(std::vector<DataItem0>&), const std::string& sort_name) {
    if (arr.empty()) {
        return;
    }

    // --- Sorting Step ---
    std::cout << "Starting In-Place Sorting with " << sort_name << "..." << std::endl;
    auto start_time = std::chrono::high_resolution_clock::now();
    sort_func(arr);
    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;
    std::cout << sort_name << " finished in " << elapsed.count() << " seconds." << std::endl;
    verify_sorted(arr);
    std::cout << "Starting Merging Sorted Array ..." << std::endl;
    start_time = std::chrono::high_resolution_clock::now();

    const size_t original_size = arr.size();
    uint32_t current_key;
    size_t final_size = 0;
    for (size_t i = 0; i < original_size; ) {
        size_t group_start = i;
        current_key = getKey0(arr[group_start]);
        while (i < original_size && getKey0(arr[i]) == current_key) {
            i++;
        }
        size_t group_size = i - group_start;
        if (group_size > 1) {
            // The number of pairs in a group of size N is N * (N - 1) / 2
            final_size += group_size * (group_size - 1) / 2;
        }
    }

    size_t write_ptr = final_size; // Points to one past the last element to write
    size_t read_i = original_size; // Points to one past the last element to read
    std::cout << "Resized array from " << original_size << " to " << final_size << " elements." << std::endl;
    if (final_size > original_size){
        // simple resizing by `arr.resize(final_size)` will double the peak memory, so we do not resize here.
        // In practiacl implementaion of Equihash, we can
        // (1) initial array size is slightly greater than 1<<21 to handle this cases
        // (2) The list size is fixed at 1<<21 and extra elements are discarded. This operation generally does not affect the overall success probability.
        // here we choose the second way
        std::cout << "We choose to discard  " << final_size - original_size << "elements." << std::endl;
        write_ptr = original_size;
    }
    else{
        write_ptr = original_size;
    }
    // This temporary vector is still needed to hold the merged items for one group.
    // Since its max size is small (~2000), this is an acceptable trade-off.
    std::vector<DataItem0> group_merged_items;
    size_t max_group_size = 0;
    size_t actual_writes = 0;

    while (read_i > 0) {
        // Find the boundaries of the current group from the right
        size_t group_end = read_i;
        uint32_t key = getKey0(arr[group_end - 1]);
        size_t group_start = group_end - 1;
        while (group_start > 0 && getKey0(arr[group_start - 1]) == key) {
            group_start--;
        }

        size_t current_group_size = group_end - group_start;
        if (current_group_size > 1) {
            for (size_t i1 = group_start; i1 < group_end; ++i1) {
                for (size_t i2 = i1 + 1; i2 < group_end; ++i2) {
                    group_merged_items.push_back(merge_item0(arr[i1], arr[i2]));
                }
            }
        }
        // Move the read pointer to the start of the processed group
        read_i = group_start;
        if (group_merged_items.size() > max_group_size) {
            max_group_size = group_merged_items.size();
        }

        // Copy the newly generated group into the correct position from the back
        size_t num_free_slots = write_ptr - read_i;
        
        if (!group_merged_items.empty()) {
            size_t num_items = group_merged_items.size();
            if (num_free_slots >= num_items){
                std::copy(group_merged_items.begin(), group_merged_items.end(), arr.begin() + write_ptr - num_items);
                write_ptr -= num_items;
                group_merged_items.clear();
                actual_writes += num_items;
                // assert(group_merged_items.size() == 0);
            }
            else{
                // write only num_free_slots
                std::copy(group_merged_items.end() - num_free_slots, group_merged_items.end(), arr.begin() + write_ptr - num_free_slots);
                write_ptr -= num_free_slots;
                group_merged_items.erase(group_merged_items.end() - num_free_slots, group_merged_items.end());
                actual_writes += num_free_slots;
                // assert(group_merged_items.size() == num_items - num_free_slots);
            }
        }
    }
    std::cout << "group_merged_items size: " << group_merged_items.size() << std::endl;
    end_time = std::chrono::high_resolution_clock::now();
    elapsed = end_time - start_time;
    std::cout << "Merging Sorted Array finished in " << elapsed.count() << " seconds." << std::endl;
    std::cout << "Max group size during merge: " << max_group_size << std::endl;
    std::cout << "Number of merged items: " << actual_writes << std::endl;
}

void test_inplace_merge(int mt_seed, int array_size, void (*sort_func)(std::vector<DataItem0>&), const std::string& sort_name) {
    // Setup random number generation
    std::cout << "-------------------------------------------------------------------------------" << std::endl;
    std::cout << "Testing " << sort_name << " with array size " << array_size << " and seed " << mt_seed << std::endl;
    std::mt19937 rng(mt_seed); // Mersenne Twister with a fixed seed
    std::uniform_int_distribution<uint_fast8_t> payload_dist;

    // Create the array
    std::vector<DataItem0> array(array_size);
    for (size_t i = 0; i < array_size; ++i) {
        for (int j = 0; j < 25; j++) {
            array[i].data[j] = payload_dist(rng);
        }
    }

    std::cout << "Starting Merge..." << std::endl;
    auto start_time = std::chrono::high_resolution_clock::now();
    // merge_inplace(array, sort_func, sort_name);
    merge_inplace(array, sort_func, sort_name);
    std::cout << "Array size after merge: " << array.size() << std::endl;
    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;
    std::cout << "Merge finished in " << elapsed.count() << " seconds." << std::endl;
}

int main(int argc, char* argv[]){
    int seed = 10; // Default seed
    int array_size = 1 << 21; // Default size 2^21
    std::string sort_name = "kx";
    if (argc >= 2) {
        seed = std::stoi(argv[1]);
        if (argc >= 3) {
            sort_name = argv[2];
        }
    }
    // usage
    std::cout << "Usage: " << argv[0] << " [seed] [sort_name]" << std::endl;
    if (sort_name == "kx") {
        test_inplace_merge(seed, array_size, kxSort, "kx:radix_sort");
        return 0;
    } else if (sort_name == "std") {
        test_inplace_merge(seed, array_size, stdSort, "std::sort");
        return 0;
    }
    return 0;
}