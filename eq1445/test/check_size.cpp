#include "core/equihash_base.h"
#include <iostream>

int main() {
    std::cout << "IndexVector<4, 1>: sizeof=" << sizeof(IndexVector<4, 1>) 
              << ", TotalBytes=" << IndexVector<4, 1>::TotalBytes << std::endl;
    std::cout << "IndexVector<4, 2>: sizeof=" << sizeof(IndexVector<4, 2>) 
              << ", TotalBytes=" << IndexVector<4, 2>::TotalBytes << std::endl;
    std::cout << "IndexVector<4, 3>: sizeof=" << sizeof(IndexVector<4, 3>) 
              << ", TotalBytes=" << IndexVector<4, 3>::TotalBytes << std::endl;
    
    // Check if they're equal
    if (sizeof(IndexVector<4, 1>) == IndexVector<4, 1>::TotalBytes) {
        std::cout << "✓ Size matches TotalBytes!" << std::endl;
    } else {
        std::cout << "✗ Size does NOT match TotalBytes" << std::endl;
    }
    return 0;
}
