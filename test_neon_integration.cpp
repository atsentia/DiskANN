#include <iostream>
#include <vector>
#include <chrono>
#include <random>

// Test our ARM64 NEON integration
#include "include/arm64/distance_neon.h"

int main() {
    std::cout << "Testing ARM64 NEON Integration" << std::endl;
    
#if defined(__aarch64__) || defined(_M_ARM64)
    std::cout << "✅ ARM64 architecture detected" << std::endl;
#else
    std::cout << "❌ Not ARM64 architecture" << std::endl;
    return 1;
#endif

    // Test data
    const size_t dim = 768;
    const size_t num_tests = 1000;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    
    std::vector<float> a(dim), b(dim);
    for (size_t i = 0; i < dim; i++) {
        a[i] = dist(gen);
        b[i] = dist(gen);
    }
    
    // Test NEON L2 distance
    auto start = std::chrono::high_resolution_clock::now();
    float neon_result = 0;
    for (size_t i = 0; i < num_tests; i++) {
        neon_result = diskann::neon::l2_distance_neon(a.data(), b.data(), dim);
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    auto neon_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Test scalar L2 distance
    start = std::chrono::high_resolution_clock::now();
    float scalar_result = 0;
    for (size_t i = 0; i < num_tests; i++) {
        float sum = 0;
        for (size_t j = 0; j < dim; j++) {
            float diff = a[j] - b[j];
            sum += diff * diff;
        }
        scalar_result = std::sqrt(sum);
    }
    end = std::chrono::high_resolution_clock::now();
    
    auto scalar_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Results
    std::cout << "\n📊 Performance Results:" << std::endl;
    std::cout << "NEON result: " << neon_result << " (time: " << neon_time.count() << " μs)" << std::endl;
    std::cout << "Scalar result: " << scalar_result << " (time: " << scalar_time.count() << " μs)" << std::endl;
    
    float speedup = (float)scalar_time.count() / neon_time.count();
    std::cout << "Speedup: " << speedup << "x" << std::endl;
    
    // Accuracy check
    float error = std::abs(neon_result - scalar_result);
    std::cout << "Accuracy error: " << error << std::endl;
    
    if (error < 1e-5f && speedup > 2.0f) {
        std::cout << "✅ ARM64 NEON integration successful!" << std::endl;
        return 0;
    } else {
        std::cout << "❌ ARM64 NEON integration failed" << std::endl;
        return 1;
    }
}