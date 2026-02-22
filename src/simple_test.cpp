#include "codec.h"
#include <iostream>

int main() {
    // Testing bit packing/unpacking functions directly
    std::println("Testing bit manipulation functions...\n");
    
    // Test 1: stringToByte and byteToString
    std::string testBits = "10110010";
    unsigned char byte = stringToByte(testBits, 0);
    std::println("Input:  '{}'", testBits);
    std::println("Byte:   0x{:02X} ({})", byte, (int)byte);
    std::string recovered = byteToString(byte);
    std::println("Output: '{}'", recovered);
    std::println("Match: {}\n", testBits == recovered ? "YES" : "NO");
    
    // Test 2: Write and read bit string
    std::ofstream out("test_bits.bin", std::ios::binary);
    std::string originalBits = "101011001110101010110011101010101100111010";
    std::println("Original bits ({} bits): '{}'", originalBits.length(), originalBits);
    writeBitString(out, originalBits);
    out.close();
    
    std::ifstream in("test_bits.bin", std::ios::binary);
    int bitCount;
    in.read(reinterpret_cast<char*>(&bitCount), sizeof(int));
    std::println("Read bitCount: {}", bitCount);
    
    std::string recoveredBits = readBitsAsString(in, bitCount);
    std::println("Recovered bits ({} bits): '{}'", recoveredBits.length(), recoveredBits);
    std::println("Match: {}", originalBits == recoveredBits ? "YES" : "NO");
    
    return 0;
}
