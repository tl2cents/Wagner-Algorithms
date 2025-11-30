#include "core/equihash_base.h"
#include <iostream>
#include <iomanip>

int main()
{
    std::cout << "=== Index Vector (IV) Test ===" << std::endl;
    std::cout << std::endl;

    // Test Layer 1: 2^1 = 2 indices
    std::cout << "Layer 1 (2 indices):" << std::endl;
    IndexVector<4, 1> iv1;
    iv1.set_index(0, 100);
    iv1.set_index(1, 200);
    std::cout << "  IV1[0] = " << iv1.get_index(0) << std::endl;
    std::cout << "  IV1[1] = " << iv1.get_index(1) << std::endl;
    std::cout << "  Size: " << sizeof(iv1) << " bytes (expected: " << 4 * 2 << ")" << std::endl;
    std::cout << std::endl;

    // Test Layer 2: 2^2 = 4 indices
    std::cout << "Layer 2 (4 indices):" << std::endl;
    IndexVector<4, 2> iv2_left;
    iv2_left.set_index(0, 10);
    iv2_left.set_index(1, 20);
    iv2_left.set_index(2, 30);
    iv2_left.set_index(3, 40);
    
    IndexVector<4, 2> iv2_right;
    iv2_right.set_index(0, 50);
    iv2_right.set_index(1, 60);
    iv2_right.set_index(2, 70);
    iv2_right.set_index(3, 80);

    // Merge to create Layer 3 IV
    std::cout << "Merging two Layer 2 IVs to create Layer 3 IV:" << std::endl;
    auto iv3 = merge_iv(iv2_left, iv2_right);
    std::cout << "  IV3[0] = " << iv3.get_index(0) << " (from left[0])" << std::endl;
    std::cout << "  IV3[1] = " << iv3.get_index(1) << " (from left[1])" << std::endl;
    std::cout << "  IV3[2] = " << iv3.get_index(2) << " (from left[2])" << std::endl;
    std::cout << "  IV3[3] = " << iv3.get_index(3) << " (from left[3])" << std::endl;
    std::cout << "  IV3[4] = " << iv3.get_index(4) << " (from right[0])" << std::endl;
    std::cout << "  IV3[5] = " << iv3.get_index(5) << " (from right[1])" << std::endl;
    std::cout << "  IV3[6] = " << iv3.get_index(6) << " (from right[2])" << std::endl;
    std::cout << "  IV3[7] = " << iv3.get_index(7) << " (from right[3])" << std::endl;
    std::cout << "  Size: " << sizeof(iv3) << " bytes (expected: " << 4 * 8 << ")" << std::endl;
    std::cout << std::endl;

    // Test ItemIV
    std::cout << "ItemIV Test (Layer 2, XOR=10 bytes):" << std::endl;
    ItemIV<10, 4, 2> item_iv;
    for (int i = 0; i < 10; ++i)
    {
        item_iv.XOR[i] = i * 10;
    }
    item_iv.iv.set_index(0, 1000);
    item_iv.iv.set_index(1, 2000);
    
    std::cout << "  XOR bytes: ";
    for (int i = 0; i < 10; ++i)
    {
        std::cout << std::setw(3) << (int)item_iv.XOR[i] << " ";
    }
    std::cout << std::endl;
    std::cout << "  IV[0] = " << item_iv.iv.get_index(0) << std::endl;
    std::cout << "  IV[1] = " << item_iv.iv.get_index(1) << std::endl;
    std::cout << "  Total size: " << sizeof(item_iv) << " bytes (XOR: 10, IV: " << sizeof(item_iv.iv) << ")" << std::endl;
    std::cout << std::endl;

    // Test size progression
    std::cout << "Size progression for 4-byte indices:" << std::endl;
    std::cout << "  Layer 1: IndexVector<4,1> = " << sizeof(IndexVector<4, 1>) << " bytes (2 indices)" << std::endl;
    std::cout << "  Layer 2: IndexVector<4,2> = " << sizeof(IndexVector<4, 2>) << " bytes (4 indices)" << std::endl;
    std::cout << "  Layer 3: IndexVector<4,3> = " << sizeof(IndexVector<4, 3>) << " bytes (8 indices)" << std::endl;
    std::cout << "  Layer 4: IndexVector<4,4> = " << sizeof(IndexVector<4, 4>) << " bytes (16 indices)" << std::endl;
    std::cout << "  Layer 5: IndexVector<4,5> = " << sizeof(IndexVector<4, 5>) << " bytes (32 indices)" << std::endl;
    std::cout << std::endl;

    std::cout << "All tests completed successfully!" << std::endl;
    return 0;
}
