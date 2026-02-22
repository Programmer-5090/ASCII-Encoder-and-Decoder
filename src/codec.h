#ifndef CODEC_H
#define CODEC_H

#include <array>
#include <unordered_map>
#include <algorithm>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <print>
#include <bitset>


namespace fs = std::filesystem;

using rgb = std::array<unsigned int, 3>;
using ASCIIVideo = std::unordered_map<int, std::vector<std::pair<char, rgb>>>;


struct Node {
    char character;
    int freq;
    Node *l , *r;

    Node(char c, int f) : character(c), freq(f), l(nullptr), r(nullptr){}
};

inline Node* buildHuffmanTree(const std::unordered_map<char, int>& uniqueChars) {
    if (uniqueChars.empty()) {
        return nullptr;
    }
    
    std::vector<Node*> heap;
    for (auto& [character, freq] : uniqueChars) {
        heap.push_back(new Node(character, freq));
    }
    std::make_heap(heap.begin(), heap.end(), 
        [](Node* a, Node* b) { return a->freq > b->freq; });

    Node *l, *r, *top;

    while (heap.size() != 1) {
        std::pop_heap(heap.begin(), heap.end(), 
            [](Node* a, Node* b) { return a->freq > b->freq; });
        l = heap.back();
        heap.pop_back();
        
        std::pop_heap(heap.begin(), heap.end(), 
            [](Node* a, Node* b) { return a->freq > b->freq; });
        r = heap.back();
        heap.pop_back();
        
        top = new Node('\0', l->freq + r->freq);
        top->l = l;
        top->r = r;
        
        heap.push_back(top);
        std::push_heap(heap.begin(), heap.end(), 
            [](Node* a, Node* b) { return a->freq > b->freq; });
    }
    
    return heap[0];
}

// Serialize tree using pre-order traversal
inline void writeHuffmanTree(std::ofstream& out, Node* root) {
    if (!root) {
        return;
    }
    
    // Check if this is a leaf node
    bool isLeaf = (!root->l && !root->r);
    out.write(reinterpret_cast<const char*>(&isLeaf), sizeof(bool));
    
    if (isLeaf) {
        // Write the character for leaf nodes
        out.write(&root->character, sizeof(char));
    } else {
        // Recursively write left and right subtrees for internal nodes
        writeHuffmanTree(out, root->l);
        writeHuffmanTree(out, root->r);
    }
}

// Deserialize tree using pre-order traversal
inline Node* readHuffmanTree(std::ifstream& in) {
    bool isLeaf;
    in.read(reinterpret_cast<char*>(&isLeaf), sizeof(bool));
    
    if (isLeaf) {
        char character;
        in.read(&character, sizeof(char));
        return new Node(character, 0); // freq not needed for decompression
    } else {
        Node* root = new Node('\0', 0);
        root->l = readHuffmanTree(in);
        root->r = readHuffmanTree(in);
        return root;
    }
}

// Generate Huffman codes from tree
inline void generateCodes(Node* root, const std::string& code, 
                         std::unordered_map<char, std::string>& codes) {
    if (!root) return;
    
    // If leaf node, store the code
    if (!root->l && !root->r) {
        codes[root->character] = code.empty() ? "0" : code;
        return;
    }
    
    generateCodes(root->l, code + "0", codes);
    generateCodes(root->r, code + "1", codes);
}

inline char findCharFromCode(Node* root, const std::string& code) {
    if (!root->l && !root->r) {
        return root->character;
    }
    
    // Traverse tree to find character
    Node* current = root;
    for (char bit : code) {
        if (bit == '0') {
            current = current->l;
        } else {
            current = current->r;
        }
    }
    return current->character;
}

inline unsigned char stringToByte(const std::string& bits, int start) {
    unsigned char byte = 0;
    for (int i = 0; i < 8; i++) {
        if (start + i > static_cast<int>(bits.length())) {
            break;
        }
        if (bits[start + i] == '1') {
            int bit = 0b10000000 >> i;
            byte |= bit;
        }
    }
    return byte;
}

inline std::string byteToString(unsigned char byte) {
    std::string bits;
    bits.reserve(8);
    int byteMask = 0b10000000;
    for (int i = 0; i < 8; i++) {
        if ((byte & byteMask) != 0) {
            bits += '1';
        } else {
            bits += '0';
        }
        byteMask >>= 1;
    }
    return bits;
}

inline void writeBitString(std::ofstream& out, const std::string& bits) {
    int bitCount = bits.length();
    unsigned char remainder = bitCount % 8;

    out.write(reinterpret_cast<const char*>(&bitCount), sizeof(int));
    for (int i = 0; i < bitCount; i += 8) {
        unsigned char byte = stringToByte(bits, i);
        out.write(reinterpret_cast<const char*>(&byte), sizeof(byte));
    }
    out.write(reinterpret_cast<const char*>(&remainder), sizeof(unsigned char));
}

inline std::string readBitsAsString(std::ifstream& in, int bitCount) {
    std::string bits = "";

    unsigned char byte;
    int count = (bitCount + 7) / 8;  // Number of bytes to read
    while (count > 0 && in.read(reinterpret_cast<char*>(&byte), 1)) {
        bits += byteToString(byte);
        count--;
    }

    bits.resize(bitCount);
    return bits;
}

inline void compressFrame(std::ofstream& out, 
                         const std::vector<std::pair<char, rgb>>& frame,
                         const std::unordered_map<char, std::string>& huffmanCodes,
                         bool useDelta,
                         const std::vector<std::pair<char, rgb>>& prevFrame) {
    std::string bitString;

    if (!useDelta) {
        // Compress full frame
        int frameSize = static_cast<int>(frame.size());
        out.write(reinterpret_cast<const char*>(&frameSize), sizeof(int));

        for (const auto& [ch, color] : frame) {
            bitString.append(std::bitset<8>(huffmanCodes.at(ch).length()).to_string());
            bitString.append(huffmanCodes.at(ch));
            bitString.append(std::bitset<8>(color[0]).to_string());
            bitString.append(std::bitset<8>(color[1]).to_string());
            bitString.append(std::bitset<8>(color[2]).to_string());
        }

        writeBitString(out, bitString);
    } else {
        // Delta encoding: only write changes
        int numChanges = 0;

        for (size_t i = 0; i < frame.size(); ++i) {
            if (i >= prevFrame.size() ||
                frame[i].first != prevFrame[i].first ||
                frame[i].second != prevFrame[i].second) {
                const char ch = frame[i].first;
                const rgb& color = frame[i].second;
                bitString.append(std::bitset<32>(i).to_string());
                bitString.append(std::bitset<8>(huffmanCodes.at(ch).length()).to_string());
                bitString.append(huffmanCodes.at(ch));
                bitString.append(std::bitset<8>(color[0]).to_string());
                bitString.append(std::bitset<8>(color[1]).to_string());
                bitString.append(std::bitset<8>(color[2]).to_string());
                numChanges++;
            }
        }

        out.write(reinterpret_cast<const char*>(&numChanges), sizeof(int));
        writeBitString(out, bitString);
    }
}

static std::pair<char, rgb> parsePixel(Node* huffmanTree, const std::string& frameBits, int& start) {
    const int codeLen = std::stoi(frameBits.substr(start, 8), nullptr, 2);
    start += 8;

    const char character = findCharFromCode(huffmanTree, frameBits.substr(start, codeLen));
    start += codeLen;

    const rgb color = {
        static_cast<unsigned int>(std::stoi(frameBits.substr(start,    8), nullptr, 2)),
        static_cast<unsigned int>(std::stoi(frameBits.substr(start+8,  8), nullptr, 2)),
        static_cast<unsigned int>(std::stoi(frameBits.substr(start+16, 8), nullptr, 2))
    };
    start += 24;

    return {character, color};
}

ASCIIVideo decompressASCIIVideo(const std::string& inPathStr) {
    ASCIIVideo video;
    fs::path inPath(inPathStr);
    if (inPath.extension() != ".bin" || !fs::exists(inPath)) {
        std::cerr << "This file does not exist or is not a bin file: " << inPathStr << '\n';
        return video;
    }

    std::println("Start Decompressing from: {}", inPath.string());

    try {
        std::ifstream packedBits(inPath, std::ios::binary);
        if (!packedBits.is_open()) {
            std::cerr << "Failed to open file: " << inPath.string() << '\n';
            return video;
        }

        int numframes;
        packedBits.read(reinterpret_cast<char*>(&numframes), sizeof(int));
        std::println("Number of frames: {}", numframes);

        Node* huffmanTree = readHuffmanTree(packedBits);
        if (!huffmanTree) {
            std::cerr << "Failed to read Huffman tree\n";
            return video;
        }
        std::println("Huffman tree loaded");

        for (int i = 0; i < numframes; i++) {
            std::println("Decompressing frame {}/{}", i, numframes - 1);

            if (i == 0) {
                // Full frame
                int frameSize, bitCount;
                packedBits.read(reinterpret_cast<char*>(&frameSize), sizeof(int));
                packedBits.read(reinterpret_cast<char*>(&bitCount), sizeof(int));
                std::println("  Frame 0: frameSize={}, bitCount={}", frameSize, bitCount);

                const std::string frameBits = readBitsAsString(packedBits, bitCount);
                unsigned char remainder;
                packedBits.read(reinterpret_cast<char*>(&remainder), sizeof(unsigned char));

                int start = 0;
                std::vector<std::pair<char, rgb>> frame;
                frame.reserve(frameSize);
                for (int p = 0; p < frameSize; ++p) {
                    frame.push_back(parsePixel(huffmanTree, frameBits, start));
                }
                std::println("  Frame 0 reconstructed with {} pixels", frame.size());
                video[0] = std::move(frame);
            } else {
                // Delta frame
                int numChanges, bitCount;
                packedBits.read(reinterpret_cast<char*>(&numChanges), sizeof(int));
                packedBits.read(reinterpret_cast<char*>(&bitCount), sizeof(int));
                std::println("  Delta frame: {} changes, {} bits", numChanges, bitCount);

                const std::string frameBits = readBitsAsString(packedBits, bitCount);
                unsigned char remainder;
                packedBits.read(reinterpret_cast<char*>(&remainder), sizeof(unsigned char));

                int start = 0;
                std::vector<std::pair<char, rgb>> frame = video[i - 1];
                std::println("  Previous frame size: {}", frame.size());

                for (int c = 0; c < numChanges; c++) {
                    // Parse changed pixel
                    const int index = std::stoi(frameBits.substr(start, 32), nullptr, 2);
                    start += 32;

                    if (index >= static_cast<int>(frame.size())) {
                        std::cerr << "ERROR: Index " << index << " out of bounds (frame size: " << frame.size() << ")\n";
                        throw std::out_of_range("Index out of bounds");
                    }

                    frame[index] = parsePixel(huffmanTree, frameBits, start);
                }
                video[i] = std::move(frame);
            }
        }

        std::println("Video decompressed successfully: {} frames", video.size());
        packedBits.close();
        
    } catch (const std::exception& e) {
        std::cerr << "Exception during decompression: " << e.what() << '\n';
        return ASCIIVideo{};
    }

    return video;
}

inline void compressASCIIVideo(const ASCIIVideo& video, const std::string& outPathStr) {
    fs::path outPath(outPathStr);
    fs::path parentDir = outPath.parent_path();

    if (!outPath.has_extension()) {
        outPath.replace_extension(".bin");
    }

    try {
        if (!parentDir.empty() && !fs::exists(parentDir)) {
            fs::create_directories(parentDir);
            std::println("Directories created: {}", parentDir.string());
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error creating directories: " << e.what() << '\n';
        return;
    }

    std::unordered_map<char, int> charFreq;
    for (const auto& [frameNum, pixels] : video) {
        for (const auto& [ch, color] : pixels) {
            charFreq[ch]++;
        }
    }

    Node* huffmanTree = buildHuffmanTree(charFreq);
    std::unordered_map<char, std::string> huffmanCodes;
    generateCodes(huffmanTree, "", huffmanCodes);

    std::ofstream outFile(outPath, std::ios::binary);

    const int numFrames = static_cast<int>(video.size());
    outFile.write(reinterpret_cast<const char*>(&numFrames), sizeof(numFrames));
    writeHuffmanTree(outFile, huffmanTree);

    std::vector<std::pair<char, rgb>> prevFrame;
    for (int i = 0; i < numFrames; ++i) {
        const auto& frame = video.at(i);
        compressFrame(outFile, frame, huffmanCodes, i != 0, prevFrame);
        prevFrame = frame;
    }

    std::println("Video compressed to: {}", outPath.string());
}

#endif // CODEC_H