#define BOOST_TEST_MODULE MathUtilsTests
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>

#include "math_utils.h"
#include "../../test_utils.h"

#if defined(__aarch64__) || defined(_M_ARM64)
extern "C" {
    // External NEON implementations from math_utils_neon.cpp
    float compute_l2_norm_neon(const float* vec, uint32_t dim);
    float compute_cosine_similarity_neon(const float* vec1, const float* vec2, uint32_t dim);
    float compute_dot_product_neon(const float* vec1, const float* vec2, uint32_t dim);
}
#endif

namespace bdata = boost::unit_test::data;
using namespace diskann::test;

BOOST_AUTO_TEST_SUITE(MathUtilsTests)

// Test dimensions
static const std::vector<size_t> test_dimensions = {16, 32, 64, 128, 256, 512, 1024};

BOOST_AUTO_TEST_CASE(test_compute_l2_norm) {
    const size_t dim = 128;
    auto vec = generate_random_vector<float>(dim, -10.0f, 10.0f);
    
    // Compute using math_utils
    float norm = diskann::compute_l2_norm(vec.data(), dim);
    
    // Compute expected
    float expected = 0;
    for (size_t i = 0; i < dim; ++i) {
        expected += vec[i] * vec[i];
    }
    expected = std::sqrt(expected);
    
    BOOST_CHECK_CLOSE(norm, expected, 0.01);
}

#if defined(__aarch64__) || defined(_M_ARM64)
BOOST_DATA_TEST_CASE(test_l2_norm_neon_correctness, bdata::make(test_dimensions), dim) {
    SKIP_IF_NO_NEON();
    
    auto vec = generate_random_vector<float>(dim, -10.0f, 10.0f);
    
    // Compute scalar reference
    float scalar_norm = 0;
    for (size_t i = 0; i < dim; ++i) {
        scalar_norm += vec[i] * vec[i];
    }
    scalar_norm = std::sqrt(scalar_norm);
    
    // Compute NEON result
    float neon_norm = compute_l2_norm_neon(vec.data(), dim);
    
    // Check correctness
    BOOST_CHECK_CLOSE(neon_norm, scalar_norm, 0.01);
}

BOOST_AUTO_TEST_CASE(test_l2_norm_neon_performance) {
    SKIP_IF_NO_NEON();
    
    const size_t dim = 256;
    const size_t iterations = 100000;
    
    auto vec = generate_random_vector<float>(dim);
    
    // Scalar implementation
    auto scalar_fn = [&]() {
        float sum = 0;
        for (size_t i = 0; i < dim; ++i) {
            sum += vec[i] * vec[i];
        }
        return std::sqrt(sum);
    };
    
    // NEON implementation
    auto neon_fn = [&]() {
        return compute_l2_norm_neon(vec.data(), dim);
    };
    
    auto result = compare_performance("L2 Norm", scalar_fn, neon_fn, iterations);
    result.print();
    
    BOOST_CHECK_MESSAGE(result.passed, 
        "NEON L2 norm should be at least 2x faster than scalar");
}
#endif

BOOST_AUTO_TEST_CASE(test_normalize_data_file) {
    const size_t num_points = 100;
    const size_t dim = 128;
    
    // Create test data
    std::vector<float> data;
    std::vector<float> norms(num_points);
    
    for (size_t i = 0; i < num_points; ++i) {
        auto vec = generate_random_vector<float>(dim, -10.0f, 10.0f);
        data.insert(data.end(), vec.begin(), vec.end());
        
        // Compute norm for verification
        float norm = 0;
        for (size_t j = 0; j < dim; ++j) {
            norm += vec[j] * vec[j];
        }
        norms[i] = std::sqrt(norm);
    }
    
    // Normalize data
    std::vector<float> normalized_data = data; // Copy
    diskann::normalize_data_file(normalized_data.data(), num_points, dim);
    
    // Verify normalization
    for (size_t i = 0; i < num_points; ++i) {
        float norm = 0;
        for (size_t j = 0; j < dim; ++j) {
            float val = normalized_data[i * dim + j];
            norm += val * val;
        }
        norm = std::sqrt(norm);
        
        // Each vector should have unit norm
        BOOST_CHECK_CLOSE(norm, 1.0f, 0.01);
        
        // Check scaling is correct
        for (size_t j = 0; j < dim; ++j) {
            float original = data[i * dim + j];
            float normalized = normalized_data[i * dim + j];
            float expected = original / norms[i];
            BOOST_CHECK_CLOSE(normalized, expected, 0.01);
        }
    }
}

BOOST_AUTO_TEST_CASE(test_compute_closest_centers) {
    const size_t num_points = 1000;
    const size_t num_centers = 10;
    const size_t dim = 64;
    
    // Create clustered test data
    auto dataset = TestDataSet::create_clustered("test", num_centers, 
                                               num_points / num_centers, dim);
    
    // Extract data and centers
    std::vector<float> data;
    for (const auto& vec : dataset.vectors) {
        data.insert(data.end(), vec.begin(), vec.end());
    }
    
    // Use first point from each cluster as centers
    std::vector<float> centers;
    for (size_t i = 0; i < num_centers; ++i) {
        size_t idx = i * (num_points / num_centers);
        centers.insert(centers.end(), 
                      dataset.vectors[idx].begin(), 
                      dataset.vectors[idx].end());
    }
    
    // Compute closest centers
    std::vector<uint32_t> closest(num_points);
    std::vector<float> dist_matrix(num_points * num_centers);
    
    diskann::compute_closest_centers(data.data(), num_points, dim,
                                   centers.data(), num_centers, 1,
                                   closest.data(), nullptr, dist_matrix.data());
    
    // Verify assignments
    size_t correct_assignments = 0;
    for (size_t i = 0; i < num_points; ++i) {
        uint32_t assigned_center = closest[i];
        uint32_t true_center = dataset.labels[i];
        
        if (assigned_center == true_center) {
            correct_assignments++;
        }
    }
    
    // Should have reasonably good clustering (at least 80% correct)
    double accuracy = static_cast<double>(correct_assignments) / num_points;
    BOOST_CHECK_GT(accuracy, 0.8);
    BOOST_TEST_MESSAGE("Clustering accuracy: " << (accuracy * 100) << "%");
}

BOOST_AUTO_TEST_CASE(test_compute_vamana_sample_sizes) {
    // Test various graph sizes
    std::vector<size_t> test_sizes = {100, 1000, 10000, 100000, 1000000};
    
    for (size_t size : test_sizes) {
        double sample_rate = 0.1; // 10% sampling
        
        size_t sample_size = static_cast<size_t>(
            std::round(diskann::compute_alpha_scratch_size(size, sample_rate)));
        
        // Sample size should be reasonable
        BOOST_CHECK_GE(sample_size, 1);
        BOOST_CHECK_LE(sample_size, size);
        
        // For large graphs, should respect sampling rate
        if (size >= 10000) {
            double actual_rate = static_cast<double>(sample_size) / size;
            BOOST_CHECK_CLOSE(actual_rate, sample_rate, 20.0); // Within 20%
        }
        
        BOOST_TEST_MESSAGE("Graph size: " << size << ", Sample size: " << sample_size);
    }
}

BOOST_AUTO_TEST_CASE(test_math_utils_edge_cases) {
    // Test with zero vector
    std::vector<float> zero_vec(128, 0.0f);
    float zero_norm = diskann::compute_l2_norm(zero_vec.data(), 128);
    BOOST_CHECK_SMALL(zero_norm, 1e-6f);
    
    // Test normalization of zero vector (should handle gracefully)
    std::vector<float> zero_copy = zero_vec;
    diskann::normalize_data_file(zero_copy.data(), 1, 128);
    // Should either leave as zero or handle gracefully
    
    // Test with very small values
    std::vector<float> small_vec(128, 1e-10f);
    float small_norm = diskann::compute_l2_norm(small_vec.data(), 128);
    BOOST_CHECK(small_norm > 0);
    BOOST_CHECK(std::isfinite(small_norm));
    
    // Test with very large values
    std::vector<float> large_vec(128, 1e10f);
    float large_norm = diskann::compute_l2_norm(large_vec.data(), 128);
    BOOST_CHECK(large_norm > 0);
    BOOST_CHECK(std::isfinite(large_norm));
}

#if defined(__aarch64__) || defined(_M_ARM64)
BOOST_AUTO_TEST_CASE(test_cosine_similarity_neon) {
    SKIP_IF_NO_NEON();
    
    const size_t dim = 128;
    const size_t iterations = 50000;
    
    auto vec1 = generate_normalized_vector<float>(dim);
    auto vec2 = generate_normalized_vector<float>(dim);
    
    // Scalar implementation
    auto scalar_fn = [&]() {
        float dot = 0;
        for (size_t i = 0; i < dim; ++i) {
            dot += vec1[i] * vec2[i];
        }
        return dot;
    };
    
    // NEON implementation
    auto neon_fn = [&]() {
        return compute_cosine_similarity_neon(vec1.data(), vec2.data(), dim);
    };
    
    // Verify correctness first
    float scalar_result = scalar_fn();
    float neon_result = neon_fn();
    BOOST_CHECK_CLOSE(scalar_result, neon_result, 0.01);
    
    // Then test performance
    auto result = compare_performance("Cosine Similarity", scalar_fn, neon_fn, iterations);
    result.print();
    
    BOOST_CHECK_MESSAGE(result.passed, 
        "NEON Cosine similarity should be at least 2x faster than scalar");
}
#endif

// Comprehensive benchmark of all math utilities
BOOST_AUTO_TEST_CASE(math_utils_comprehensive_benchmark) {
    BOOST_TEST_MESSAGE("\n=== Math Utils Comprehensive Benchmark ===");
    
    BenchmarkReporter reporter;
    
    for (size_t dim : {64, 128, 256, 512}) {
        BOOST_TEST_MESSAGE("\nDimension: " << dim);
        
        auto vec1 = generate_random_vector<float>(dim);
        auto vec2 = generate_random_vector<float>(dim);
        
        // Benchmark L2 norm
        {
            Timer timer;
            for (int i = 0; i < 10000; ++i) {
                diskann::compute_l2_norm(vec1.data(), dim);
            }
            double time_us = timer.elapsed_us() / 10000;
            BOOST_TEST_MESSAGE("  L2 Norm: " << time_us << " μs");
        }
        
        // Benchmark normalization
        {
            std::vector<float> data_copy = vec1;
            Timer timer;
            for (int i = 0; i < 1000; ++i) {
                diskann::normalize_data_file(data_copy.data(), 1, dim);
            }
            double time_us = timer.elapsed_us() / 1000;
            BOOST_TEST_MESSAGE("  Normalize: " << time_us << " μs");
        }
        
#if defined(__aarch64__) || defined(_M_ARM64)
        if (is_neon_available()) {
            // Benchmark NEON L2 norm
            Timer timer;
            for (int i = 0; i < 10000; ++i) {
                compute_l2_norm_neon(vec1.data(), dim);
            }
            double time_us = timer.elapsed_us() / 10000;
            BOOST_TEST_MESSAGE("  L2 Norm (NEON): " << time_us << " μs");
        }
#endif
    }
}

BOOST_AUTO_TEST_SUITE_END()