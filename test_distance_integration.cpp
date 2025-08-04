#include <iostream>
#include <vector>
#include <chrono>
#include <random>

// Minimal DiskANN distance testing
#include "include/distance.h"
#include "include/utils.h"

// Mock globals that distance.cpp expects
namespace diskann {
    bool Avx2SupportedCPU = false;
    bool AvxSupportedCPU = false;
    
#if defined(__aarch64__) || defined(_M_ARM64)
    bool NeonSupportedCPU = true;
#else
    bool NeonSupportedCPU = false;
#endif
}

int main() {
    std::cout << "Testing DiskANN Distance Integration" << std::endl;
    
#if defined(__aarch64__) || defined(_M_ARM64)
    std::cout << "✅ ARM64 architecture detected" << std::endl;
    std::cout << "✅ NEON support: " << (diskann::NeonSupportedCPU ? "Yes" : "No") << std::endl;
#else
    std::cout << "❌ Not ARM64 architecture" << std::endl;
    return 1;
#endif

    // Create distance function for L2
    auto distance_func = diskann::get_distance_function<float>(diskann::Metric::L2);
    if (!distance_func) {
        std::cout << "❌ Failed to create distance function" << std::endl;
        return 1;
    }
    
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
    
    // Test DiskANN distance function
    auto start = std::chrono::high_resolution_clock::now();
    float diskann_result = 0;
    for (size_t i = 0; i < num_tests; i++) {
        diskann_result = distance_func->compare(a.data(), b.data(), dim);
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    auto diskann_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Test scalar L2 distance for comparison
    start = std::chrono::high_resolution_clock::now();
    float scalar_result = 0;
    for (size_t i = 0; i < num_tests; i++) {
        float sum = 0;
        for (size_t j = 0; j < dim; j++) {
            float diff = a[j] - b[j];
            sum += diff * diff;
        }
        scalar_result = sum; // Note: DiskANN L2 returns squared distance
    }
    end = std::chrono::high_resolution_clock::now();
    
    auto scalar_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Results
    std::cout << "\n📊 Performance Results:" << std::endl;
    std::cout << "DiskANN L2 result: " << diskann_result << " (time: " << diskann_time.count() << " μs)" << std::endl;
    std::cout << "Scalar L2 result: " << scalar_result << " (time: " << scalar_time.count() << " μs)" << std::endl;
    
    float speedup = (float)scalar_time.count() / diskann_time.count();
    std::cout << "Speedup: " << speedup << "x" << std::endl;
    
    // Accuracy check
    float error = std::abs(diskann_result - scalar_result);
    std::cout << "Accuracy error: " << error << std::endl;
    
    delete distance_func;
    
    if (error < 1e-3f && speedup > 2.0f) {
        std::cout << "✅ DiskANN ARM64 distance integration successful!" << std::endl;
        return 0;
    } else {
        std::cout << "❌ DiskANN ARM64 distance integration failed" << std::endl;
        return 1;
    }
}