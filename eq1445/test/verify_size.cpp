#include "core/equihash_base.h"
#include <iostream>

template<std::size_t IB, std::size_t L>
void check() {
    using IV = IndexVector<IB, L>;
    bool match = sizeof(IV) == IV::TotalBytes;
    std::cout << "IndexVector<" << IB << ", " << L << ">: "
              << "sizeof=" << sizeof(IV) 
              << ", TotalBytes=" << IV::TotalBytes
              << " " << (match ? "✓" : "✗") << std::endl;
}

int main() {
    std::cout << "=== Verification: sizeof == TotalBytes ===" << std::endl;
    
    // Test with 4-byte indices
    check<4, 0>();
    check<4, 1>();
    check<4, 2>();
    check<4, 3>();
    check<4, 4>();
    check<4, 5>();
    
    std::cout << std::endl;
    
    // Test with 8-byte indices
    check<8, 0>();
    check<8, 1>();
    check<8, 2>();
    check<8, 3>();
    
    return 0;
}
