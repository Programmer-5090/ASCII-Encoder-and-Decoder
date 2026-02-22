#include "codec.h"
#include <iostream>
#include <cassert>
#include <print>

bool compareFrames(const std::vector<std::pair<char, rgb>>& frame1, 
                   const std::vector<std::pair<char, rgb>>& frame2) {
    if (frame1.size() != frame2.size()) {
        std::println("Frame size mismatch: {} vs {}", frame1.size(), frame2.size());
        return false;
    }
    
    for (size_t i = 0; i < frame1.size(); ++i) {
        if (frame1[i].first != frame2[i].first) {
            std::println("Char mismatch at pixel {}: '{}' vs '{}'", i, frame1[i].first, frame2[i].first);
            return false;
        }
        if (frame1[i].second != frame2[i].second) {
            std::println("RGB mismatch at pixel {}: [{},{},{}] vs [{},{},{}]", 
                        i, frame1[i].second[0], frame1[i].second[1], frame1[i].second[2],
                        frame2[i].second[0], frame2[i].second[1], frame2[i].second[2]);
            return false;
        }
    }
    
    return true;
}

bool compareVideos(const ASCIIVideo& video1, const ASCIIVideo& video2) {
    if (video1.size() != video2.size()) {
        std::println("Video size mismatch: {} vs {}", video1.size(), video2.size());
        return false;
    }
    
    for (const auto& [frameNum, frame] : video1) {
        if (video2.find(frameNum) == video2.end()) {
            std::println("Frame {} missing in decompressed video", frameNum);
            return false;
        }
        
        std::println("Comparing frame {}...", frameNum);
        if (!compareFrames(frame, video2.at(frameNum))) {
            std::println("Frame {} mismatch!", frameNum);
            return false;
        }
    }
    
    return true;
}

// Test Case 1: Simple video with minimal changes
void testSimpleVideo() {
    std::println("\n=== Test 1: Simple Video ===");
    
    ASCIIVideo video;
    
    // Frame 0: AAA (all red)
    video[0] = {
        {'A', {255, 0, 0}},
        {'A', {255, 0, 0}},
        {'A', {255, 0, 0}}
    };
    
    // Frame 1: ABA (middle changed to green B)
    video[1] = {
        {'A', {255, 0, 0}},
        {'B', {0, 255, 0}},
        {'A', {255, 0, 0}}
    };
    
    // Frame 2: ABC (last changed to blue C)
    video[2] = {
        {'A', {255, 0, 0}},
        {'B', {0, 255, 0}},
        {'C', {0, 0, 255}}
    };
    
    // Compress
    std::println("Compressing simple video...");
    compressASCIIVideo(video, "test_simple.bin");
    
    // Decompress
    std::println("Decompressing simple video...");
    ASCIIVideo decompressed = decompressASCIIVideo("test_simple.bin");
    
    // Verify
    if (compareVideos(video, decompressed)) {
        std::println("Test 1 PASSED: Simple video compressed and decompressed correctly!");
    } else {
        std::println("Test 1 FAILED: Decompressed video doesn't match original!");
    }
}

// Test Case 2: Video with no changes (all frames identical)
void testNoChanges() {
    std::println("\n=== Test 2: No Changes Video ===");
    
    ASCIIVideo video;
    
    // All frames identical
    for (int i = 0; i < 5; ++i) {
        video[i] = {
            {'X', {128, 128, 128}},
            {'Y', {64, 64, 64}},
            {'Z', {192, 192, 192}}
        };
    }
    
    // Compress
    std::println("Compressing no-change video...");
    compressASCIIVideo(video, "test_no_change.bin");
    
    // Decompress
    std::println("Decompressing no-change video...");
    ASCIIVideo decompressed = decompressASCIIVideo("test_no_change.bin");
    
    // Verify
    if (compareVideos(video, decompressed)) {
        std::println("Test 2 PASSED: No-change video compressed correctly (should have 0 changes per delta frame)!");
    } else {
        std::println("Test 2 FAILED: Decompressed video doesn't match original!");
    }
}

// Test Case 3: Large frame with many unique characters
void testLargeFrame() {
    std::println("\n=== Test 3: Large Frame ===");
    
    ASCIIVideo video;
    
    // Frame 0: 100 pixels with various characters
    std::vector<std::pair<char, rgb>> frame0;
    for (int i = 0; i < 100; ++i) {
        char ch = 'A' + (i % 26);  // Cycle through A-Z
        rgb color = {
            static_cast<unsigned int>((i * 2) % 256),
            static_cast<unsigned int>((i * 3) % 256),
            static_cast<unsigned int>((i * 5) % 256)
        };
        frame0.push_back({ch, color});
    }
    video[0] = frame0;
    
    // Frame 1: Change every 10th pixel
    std::vector<std::pair<char, rgb>> frame1 = frame0;
    for (int i = 0; i < 100; i += 10) {
        frame1[i] = {'*', {255, 255, 255}};
    }
    video[1] = frame1;
    
    // Compress
    std::println("Compressing large frame video...");
    compressASCIIVideo(video, "test_large.bin");
    
    // Decompress
    std::println("Decompressing large frame video...");
    ASCIIVideo decompressed = decompressASCIIVideo("test_large.bin");
    
    // Verify
    if (compareVideos(video, decompressed)) {
        std::println("Test 3 PASSED: Large frame compressed and decompressed correctly!");
    } else {
        std::println("Test 3 FAILED: Decompressed video doesn't match original!");
    }
}

// Test Case 4: Single frame video
void testSingleFrame() {
    std::println("\n=== Test 4: Single Frame ===");
    
    ASCIIVideo video;
    
    video[0] = {
        {'H', {255, 0, 0}},
        {'E', {0, 255, 0}},
        {'L', {0, 0, 255}},
        {'L', {255, 255, 0}},
        {'O', {255, 0, 255}}
    };
    
    // Compress
    std::println("Compressing single frame video...");
    compressASCIIVideo(video, "test_single.bin");
    
    // Decompress
    std::println("Decompressing single frame video...");
    ASCIIVideo decompressed = decompressASCIIVideo("test_single.bin");
    
    // Verify
    if (compareVideos(video, decompressed)) {
        std::println("Test 4 PASSED: Single frame compressed and decompressed correctly!");
    } else {
        std::println("Test 4 FAILED: Decompressed video doesn't match original!");
    }
}

// Test Case 5: Complete change (delta frame changes everything)
void testCompleteChange() {
    std::println("\n=== Test 5: Complete Change ===");
    
    ASCIIVideo video;
    
    // Frame 0
    video[0] = {
        {'A', {255, 0, 0}},
        {'A', {255, 0, 0}},
        {'A', {255, 0, 0}}
    };
    
    // Frame 1: Everything changes
    video[1] = {
        {'B', {0, 255, 0}},
        {'B', {0, 255, 0}},
        {'B', {0, 255, 0}}
    };
    
    // Compress
    std::println("Compressing complete change video...");
    compressASCIIVideo(video, "test_complete_change.bin");
    
    // Decompress
    std::println("Decompressing complete change video...");
    ASCIIVideo decompressed = decompressASCIIVideo("test_complete_change.bin");
    
    // Verify
    if (compareVideos(video, decompressed)) {
        std::println("Test 5 PASSED: Complete change compressed correctly!");
    } else {
        std::println("Test 5 FAILED: Decompressed video doesn't match original!");
    }
}

int main() {
    std::println("Starting Codec Tests...\n");
    
    try {
        testSimpleVideo();
        testNoChanges();
        testLargeFrame();
        testSingleFrame();
        testCompleteChange();
        
        std::println("\n=== All Tests Complete ===");
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
