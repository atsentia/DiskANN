#pragma once

#include <vector>
#include <random>
#include <chrono>
#include <string>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <algorithm>

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif

namespace diskann {
namespace test {

// Platform detection helpers
inline bool is_arm64_platform() {
#if defined(__aarch64__) || defined(_M_ARM64)
    return true;
#else
    return false;
#endif
}

inline bool is_neon_available() {
#ifdef __ARM_NEON
    return true;
#else
    return false;
#endif
}

// Test data generation
template<typename T>
std::vector<T> generate_random_vector(size_t dim, T min_val = -1.0, T max_val = 1.0, 
                                     unsigned seed = std::random_device{}()) {
    std::mt19937 gen(seed);
    std::uniform_real_distribution<T> dis(min_val, max_val);
    
    std::vector<T> vec(dim);
    for (size_t i = 0; i < dim; ++i) {
        vec[i] = dis(gen);
    }
    return vec;
}

template<typename T>
std::vector<T> generate_normalized_vector(size_t dim, unsigned seed = std::random_device{}()) {
    auto vec = generate_random_vector<T>(dim, -1.0, 1.0, seed);
    
    T norm = 0;
    for (const auto& v : vec) {
        norm += v * v;
    }
    norm = std::sqrt(norm);
    
    for (auto& v : vec) {
        v /= norm;
    }
    return vec;
}

// Timing utilities
class Timer {
public:
    Timer() : start_(std::chrono::high_resolution_clock::now()) {}
    
    double elapsed_ms() const {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start_).count();
    }
    
    double elapsed_us() const {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::micro>(end - start_).count();
    }
    
    void reset() {
        start_ = std::chrono::high_resolution_clock::now();
    }
    
private:
    std::chrono::high_resolution_clock::time_point start_;
};

// Performance comparison
template<typename Func>
double measure_performance(Func func, size_t iterations = 10000) {
    Timer timer;
    for (size_t i = 0; i < iterations; ++i) {
        func();
    }
    return timer.elapsed_us() / iterations;
}

// Validation helpers
template<typename T>
bool vectors_equal(const T* a, const T* b, size_t dim, T tolerance = 1e-6) {
    for (size_t i = 0; i < dim; ++i) {
        if (std::abs(a[i] - b[i]) > tolerance) {
            return false;
        }
    }
    return true;
}

template<typename T>
T compute_error(const T* a, const T* b, size_t dim) {
    T error = 0;
    for (size_t i = 0; i < dim; ++i) {
        T diff = a[i] - b[i];
        error += diff * diff;
    }
    return std::sqrt(error / dim);
}

// ARM64 NEON test macros
#define SKIP_IF_NOT_ARM64() \
    if (!diskann::test::is_arm64_platform()) { \
        BOOST_TEST_MESSAGE("Skipping test - not on ARM64 platform"); \
        return; \
    }

#define SKIP_IF_NO_NEON() \
    if (!diskann::test::is_neon_available()) { \
        BOOST_TEST_MESSAGE("Skipping test - NEON not available"); \
        return; \
    }

#define REQUIRE_ARM64_NEON() \
    BOOST_REQUIRE(diskann::test::is_arm64_platform()); \
    BOOST_REQUIRE(diskann::test::is_neon_available())

// Performance validation
struct PerformanceResult {
    std::string name;
    double scalar_time_us;
    double optimized_time_us;
    double speedup;
    bool passed;
    
    void print() const {
        std::cout << std::fixed << std::setprecision(3);
        std::cout << name << ":\n";
        std::cout << "  Scalar: " << scalar_time_us << " μs\n";
        std::cout << "  Optimized: " << optimized_time_us << " μs\n";
        std::cout << "  Speedup: " << speedup << "x";
        if (!passed) {
            std::cout << " (FAILED - expected >= 2.0x)";
        }
        std::cout << "\n";
    }
};

template<typename ScalarFunc, typename OptimizedFunc>
PerformanceResult compare_performance(const std::string& name,
                                    ScalarFunc scalar_fn,
                                    OptimizedFunc optimized_fn,
                                    size_t iterations = 10000,
                                    double min_speedup = 2.0) {
    double scalar_time = measure_performance(scalar_fn, iterations);
    double optimized_time = measure_performance(optimized_fn, iterations);
    double speedup = scalar_time / optimized_time;
    
    PerformanceResult result{
        name,
        scalar_time,
        optimized_time,
        speedup,
        speedup >= min_speedup
    };
    
    return result;
}

// Memory alignment helpers
inline bool is_aligned(const void* ptr, size_t alignment) {
    return (reinterpret_cast<uintptr_t>(ptr) % alignment) == 0;
}

template<typename T>
std::vector<T> create_aligned_vector(size_t size, size_t alignment = 32) {
    size_t padded_size = size + (alignment / sizeof(T));
    std::vector<T> vec(padded_size);
    
    // Find aligned position
    uintptr_t addr = reinterpret_cast<uintptr_t>(vec.data());
    uintptr_t aligned_addr = (addr + alignment - 1) & ~(alignment - 1);
    size_t offset = (aligned_addr - addr) / sizeof(T);
    
    // Return sub-vector starting at aligned position
    return std::vector<T>(vec.begin() + offset, vec.begin() + offset + size);
}

// Test data sets
struct TestDataSet {
    std::string name;
    size_t num_vectors;
    size_t dimension;
    std::vector<std::vector<float>> vectors;
    std::vector<uint32_t> labels;
    
    static TestDataSet create_random(const std::string& name, size_t num_vectors, 
                                   size_t dimension, unsigned seed = 42) {
        TestDataSet dataset;
        dataset.name = name;
        dataset.num_vectors = num_vectors;
        dataset.dimension = dimension;
        
        for (size_t i = 0; i < num_vectors; ++i) {
            dataset.vectors.push_back(generate_random_vector<float>(dimension, -1.0f, 1.0f, seed + i));
            dataset.labels.push_back(i % 10); // Simple label assignment
        }
        
        return dataset;
    }
    
    static TestDataSet create_clustered(const std::string& name, size_t num_clusters,
                                      size_t vectors_per_cluster, size_t dimension,
                                      unsigned seed = 42) {
        TestDataSet dataset;
        dataset.name = name;
        dataset.num_vectors = num_clusters * vectors_per_cluster;
        dataset.dimension = dimension;
        
        std::mt19937 gen(seed);
        std::normal_distribution<float> noise(0.0f, 0.1f);
        
        // Generate cluster centers
        std::vector<std::vector<float>> centers;
        for (size_t i = 0; i < num_clusters; ++i) {
            centers.push_back(generate_random_vector<float>(dimension, -5.0f, 5.0f, seed + i));
        }
        
        // Generate points around centers
        for (size_t c = 0; c < num_clusters; ++c) {
            for (size_t i = 0; i < vectors_per_cluster; ++i) {
                std::vector<float> vec(dimension);
                for (size_t d = 0; d < dimension; ++d) {
                    vec[d] = centers[c][d] + noise(gen);
                }
                dataset.vectors.push_back(vec);
                dataset.labels.push_back(c);
            }
        }
        
        return dataset;
    }
};

// Benchmark reporting
class BenchmarkReporter {
public:
    void add_result(const PerformanceResult& result) {
        results_.push_back(result);
    }
    
    void print_summary() const {
        std::cout << "\n=== Performance Summary ===\n";
        for (const auto& result : results_) {
            result.print();
        }
        
        size_t passed = std::count_if(results_.begin(), results_.end(),
                                     [](const PerformanceResult& r) { return r.passed; });
        
        std::cout << "\nPassed: " << passed << "/" << results_.size() << "\n";
        
        if (passed < results_.size()) {
            std::cout << "\nFailed tests:\n";
            for (const auto& result : results_) {
                if (!result.passed) {
                    std::cout << "  - " << result.name << " (speedup: " 
                             << std::fixed << std::setprecision(2) 
                             << result.speedup << "x)\n";
                }
            }
        }
    }
    
private:
    std::vector<PerformanceResult> results_;
};

} // namespace test
} // namespace diskann