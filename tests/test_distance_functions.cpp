#include <gtest/gtest.h>
#include <vector>
#include <random>
#include <cmath>
#include "distance.h"
#include "arm64/distance_neon.h"

// Test fixture for distance functions
class DistanceFunctionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize test vectors
        dim = 128;
        vec1.resize(dim);
        vec2.resize(dim);
        
        // Fill with known values
        for (size_t i = 0; i < dim; i++) {
            vec1[i] = static_cast<float>(i) / dim;
            vec2[i] = static_cast<float>(dim - i) / dim;
        }
    }
    
    size_t dim;
    std::vector<float> vec1;
    std::vector<float> vec2;
};

// Test L2 distance calculation
TEST_F(DistanceFunctionTest, L2DistanceCorrectness) {
    // Calculate expected L2 distance
    float expected = 0.0f;
    for (size_t i = 0; i < dim; i++) {
        float diff = vec1[i] - vec2[i];
        expected += diff * diff;
    }
    expected = std::sqrt(expected);
    
    // Test standard implementation
    auto l2_func = diskann::get_distance_function<float>(diskann::Metric::L2);
    float result = l2_func->compare(vec1.data(), vec2.data(), dim);
    EXPECT_NEAR(result, expected, 1e-5);
    
#if defined(__aarch64__) || defined(_M_ARM64)
    // Test ARM64 NEON implementation
    float neon_result = diskann::neon::l2_distance_neon(vec1.data(), vec2.data(), dim);
    EXPECT_NEAR(neon_result, expected, 1e-5);
    
    // Ensure NEON and standard match
    EXPECT_NEAR(neon_result, result, 1e-6);
#endif
}

// Test inner product calculation
TEST_F(DistanceFunctionTest, InnerProductCorrectness) {
    // Calculate expected inner product
    float expected = 0.0f;
    for (size_t i = 0; i < dim; i++) {
        expected += vec1[i] * vec2[i];
    }
    
    // Test standard implementation
    auto ip_func = diskann::get_distance_function<float>(diskann::Metric::INNER_PRODUCT);
    float result = ip_func->compare(vec1.data(), vec2.data(), dim);
    // Inner product distance is negative of dot product
    EXPECT_NEAR(result, -expected, 1e-5);
    
#if defined(__aarch64__) || defined(_M_ARM64)
    // Test ARM64 NEON implementation
    float neon_result = diskann::neon::dot_product_neon(vec1.data(), vec2.data(), dim);
    EXPECT_NEAR(neon_result, expected, 1e-5);
#endif
}

// Test cosine similarity
TEST_F(DistanceFunctionTest, CosineDistanceCorrectness) {
    // Normalize vectors for cosine
    float norm1 = 0.0f, norm2 = 0.0f, dot = 0.0f;
    for (size_t i = 0; i < dim; i++) {
        norm1 += vec1[i] * vec1[i];
        norm2 += vec2[i] * vec2[i];
        dot += vec1[i] * vec2[i];
    }
    norm1 = std::sqrt(norm1);
    norm2 = std::sqrt(norm2);
    float expected_cosine = dot / (norm1 * norm2);
    float expected_distance = 1.0f - expected_cosine;
    
    // Test implementation
    auto cosine_func = diskann::get_distance_function<float>(diskann::Metric::COSINE);
    float result = cosine_func->compare(vec1.data(), vec2.data(), dim);
    EXPECT_NEAR(result, expected_distance, 1e-5);
    
#if defined(__aarch64__) || defined(_M_ARM64)
    // Test ARM64 NEON implementation
    float neon_result = diskann::neon::cosine_distance_neon(vec1.data(), vec2.data(), dim);
    EXPECT_NEAR(neon_result, expected_distance, 1e-5);
#endif
}

// Performance test for large vectors
TEST(DistancePerformanceTest, LargeVectorPerformance) {
    const size_t large_dim = 1024;
    std::vector<float> a(large_dim), b(large_dim);
    
    // Initialize with random data
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(0.0f, 1.0f);
    
    for (size_t i = 0; i < large_dim; i++) {
        a[i] = dis(gen);
        b[i] = dis(gen);
    }
    
    // Time standard implementation
    auto start = std::chrono::high_resolution_clock::now();
    auto l2_func = diskann::get_distance_function<float>(diskann::Metric::L2);
    for (int i = 0; i < 10000; i++) {
        l2_func->compare(a.data(), b.data(), large_dim);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto standard_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    
#if defined(__aarch64__) || defined(_M_ARM64)
    // Time NEON implementation
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 10000; i++) {
        diskann::neon::l2_distance_neon(a.data(), b.data(), large_dim);
    }
    end = std::chrono::high_resolution_clock::now();
    auto neon_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    
    // NEON should be significantly faster
    std::cout << "Standard time: " << standard_time << " us" << std::endl;
    std::cout << "NEON time: " << neon_time << " us" << std::endl;
    std::cout << "Speedup: " << (float)standard_time / neon_time << "x" << std::endl;
    
    // Expect at least 2x speedup
    EXPECT_LT(neon_time, standard_time / 2);
#endif
}

// Test edge cases
TEST(DistanceEdgeCaseTest, ZeroVectors) {
    const size_t dim = 128;
    std::vector<float> zero1(dim, 0.0f);
    std::vector<float> zero2(dim, 0.0f);
    
    auto l2_func = diskann::get_distance_function<float>(diskann::Metric::L2);
    float result = l2_func->compare(zero1.data(), zero2.data(), dim);
    EXPECT_FLOAT_EQ(result, 0.0f);
    
#if defined(__aarch64__) || defined(_M_ARM64)
    float neon_result = diskann::neon::l2_distance_neon(zero1.data(), zero2.data(), dim);
    EXPECT_FLOAT_EQ(neon_result, 0.0f);
#endif
}

// Test alignment requirements
TEST(DistanceAlignmentTest, UnalignedVectors) {
    // Create unaligned vectors
    std::vector<float> buffer1(129);
    std::vector<float> buffer2(129);
    
    // Use pointers that are not 16-byte aligned
    float* unaligned1 = &buffer1[1];
    float* unaligned2 = &buffer2[1];
    
    const size_t dim = 127;
    for (size_t i = 0; i < dim; i++) {
        unaligned1[i] = i * 0.1f;
        unaligned2[i] = i * 0.2f;
    }
    
    // Should still work correctly
    auto l2_func = diskann::get_distance_function<float>(diskann::Metric::L2);
    float result = l2_func->compare(unaligned1, unaligned2, dim);
    EXPECT_GT(result, 0.0f);
    
#if defined(__aarch64__) || defined(_M_ARM64)
    float neon_result = diskann::neon::l2_distance_neon(unaligned1, unaligned2, dim);
    EXPECT_NEAR(neon_result, result, 1e-5);
#endif
}