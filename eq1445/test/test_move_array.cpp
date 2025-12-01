#include "eq144_5/equihash_144_5.h"
#include "core/sort.h"
extern bool g_verbose;
#include "core/util.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <cstring>

// Define g_verbose as required by util.h
bool g_verbose = false;

struct TestItem {
    uint8_t XOR[4];
    uint8_t index[4];
};

int main() {
    const size_t N = 100;
    const size_t BUF_SIZE = 1024 * 1024;
    uint8_t* buffer = new uint8_t[BUF_SIZE];
    
    // Initialize layer at start of buffer
    LayerVec<TestItem> vec = init_layer<TestItem>(buffer, BUF_SIZE);
    vec.resize(N);
    
    // Fill with data
    for(size_t i=0; i<N; ++i) {
        uint32_t val = (uint32_t)i;
        std::memcpy(vec[i].XOR, &val, 4);
        std::memcpy(vec[i].index, &val, 4);
    }
    
    // Verify initial data
    for(size_t i=0; i<N; ++i) {
        uint32_t val;
        std::memcpy(&val, vec[i].XOR, 4);
        assert(val == i);
    }
    
    std::cout << "Initial check passed." << std::endl;
    
    // Move by offset (overlapping)
    // Shift right by 10 items (80 bytes)
    size_t offset = 10 * sizeof(TestItem); 
    size_t new_cap = BUF_SIZE - offset;
    
    LayerVec<TestItem> new_vec = move_array(vec, offset, new_cap);
    
    // Verify new vector
    assert(new_vec.size() == N);
    assert(reinterpret_cast<uint8_t*>(new_vec.data()) == buffer + offset);
    
    for(size_t i=0; i<N; ++i) {
        uint32_t val;
        std::memcpy(&val, new_vec[i].XOR, 4);
        assert(val == i);
    }
    
    std::cout << "Move check passed." << std::endl;
    
    // Test default capacity
    uint8_t* buffer2 = new uint8_t[1024];
    LayerVec<TestItem> vec2 = init_layer<TestItem>(buffer2, 1024);
    vec2.resize(10);
    for(size_t i=0; i<10; ++i) {
        uint32_t val = (uint32_t)i;
        std::memcpy(vec2[i].XOR, &val, 4);
    }

    // Move by 1 item size
    size_t offset2 = sizeof(TestItem);
    // Call without new_capacity_bytes
    LayerVec<TestItem> new_vec2 = move_array(vec2, offset2);

    assert(new_vec2.size() == 10);
    assert(new_vec2.capacity() == 10);
    
    std::cout << "Default capacity check passed." << std::endl;

    delete[] buffer2;
    delete[] buffer;
    return 0;
}
