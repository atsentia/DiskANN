#define BOOST_TEST_MODULE DistanceTests
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

#include "distance.h"
#include "../../test_utils.h"

#if defined(__aarch64__) || defined(_M_ARM64)
#include "arm64/distance_neon.h"
#endif

namespace bdata = boost::unit_test::data;
using namespace diskann::test;

BOOST_AUTO_TEST_SUITE(DistanceFunctionTests)

// Test dimensions to validate
static const std::vector<size_t> test_dimensions = {16, 32, 64, 128, 256, 512, 1024, 1536};

BOOST_AUTO_TEST_CASE(test_platform_detection) {
    BOOST_TEST_MESSAGE("Platform: " << (is_arm64_platform() ? "ARM64" : "Other"));
    BOOST_TEST_MESSAGE("NEON: " << (is_neon_available() ? "Available" : "Not available"));
    
    if (is_arm64_platform()) {
        BOOST_CHECK(is_neon_available());
    }
}

BOOST_DATA_TEST_CASE(test_l2_distance_correctness, bdata::make(test_dimensions), dim) {
    BOOST_TEST_MESSAGE("Testing L2 distance for dimension: " << dim);
    
    // Generate test vectors
    auto vec1 = generate_random_vector<float>(dim, -10.0f, 10.0f, 42);
    auto vec2 = generate_random_vector<float>(dim, -10.0f, 10.0f, 43);
    
    // Get distance function
    auto dist_fn = std::unique_ptr<diskann::Distance<float>>(
        diskann::get_distance_function<float>(diskann::Metric::L2));
    
    // Compute distance
    float distance = dist_fn->compare(vec1.data(), vec2.data(), dim);
    
    // Compute expected distance manually
    float expected = 0;
    for (size_t i = 0; i < dim; ++i) {
        float diff = vec1[i] - vec2[i];
        expected += diff * diff;
    }
    
    // Check within tolerance
    BOOST_CHECK_CLOSE(distance, expected, 0.01); // 0.01% tolerance
}

#if defined(__aarch64__) || defined(_M_ARM64)
BOOST_DATA_TEST_CASE(test_l2_neon_correctness, bdata::make(test_dimensions), dim) {
    SKIP_IF_NO_NEON();
    
    BOOST_TEST_MESSAGE("Testing L2 NEON for dimension: " << dim);
    
    // Generate test vectors
    auto vec1 = generate_random_vector<float>(dim, -10.0f, 10.0f, 42);
    auto vec2 = generate_random_vector<float>(dim, -10.0f, 10.0f, 43);
    
    // Compute scalar reference
    float scalar_result = 0;
    for (size_t i = 0; i < dim; ++i) {
        float diff = vec1[i] - vec2[i];
        scalar_result += diff * diff;
    }
    
    // Compute NEON result
    float neon_result = diskann::neon::l2_distance_neon(vec1.data(), vec2.data(), dim);
    
    // Check correctness
    BOOST_CHECK_CLOSE(neon_result, scalar_result, 0.01);
}

BOOST_AUTO_TEST_CASE(test_l2_neon_performance) {
    SKIP_IF_NO_NEON();
    
    const size_t dim = 128;
    const size_t iterations = 100000;
    
    auto vec1 = generate_random_vector<float>(dim);
    auto vec2 = generate_random_vector<float>(dim);
    
    // Scalar implementation
    auto scalar_fn = [&]() {
        float sum = 0;
        for (size_t i = 0; i < dim; ++i) {
            float diff = vec1[i] - vec2[i];
            sum += diff * diff;
        }
        return sum;
    };
    
    // NEON implementation
    auto neon_fn = [&]() {
        return diskann::neon::l2_distance_neon(vec1.data(), vec2.data(), dim);
    };
    
    auto result = compare_performance("L2 Distance", scalar_fn, neon_fn, iterations);
    result.print();
    
    BOOST_CHECK_MESSAGE(result.passed, 
        "NEON L2 distance should be at least 2x faster than scalar (got " 
        << result.speedup << "x)");
}
#endif

BOOST_DATA_TEST_CASE(test_cosine_distance_correctness, bdata::make(test_dimensions), dim) {
    BOOST_TEST_MESSAGE("Testing Cosine distance for dimension: " << dim);
    
    // Generate normalized vectors for cosine distance
    auto vec1 = generate_normalized_vector<float>(dim, 42);
    auto vec2 = generate_normalized_vector<float>(dim, 43);
    
    // Get distance function
    auto dist_fn = std::unique_ptr<diskann::Distance<float>>(
        diskann::get_distance_function<float>(diskann::Metric::COSINE));
    
    // Compute distance
    float distance = dist_fn->compare(vec1.data(), vec2.data(), dim);
    
    // Compute expected cosine distance (1 - dot product for normalized vectors)
    float dot_product = 0;
    for (size_t i = 0; i < dim; ++i) {
        dot_product += vec1[i] * vec2[i];
    }
    float expected = 1.0f - dot_product;
    
    // Check within tolerance
    BOOST_CHECK_CLOSE(distance, expected, 0.1); // 0.1% tolerance
}

#if defined(__aarch64__) || defined(_M_ARM64)
BOOST_DATA_TEST_CASE(test_cosine_neon_correctness, bdata::make(test_dimensions), dim) {
    SKIP_IF_NO_NEON();
    
    BOOST_TEST_MESSAGE("Testing Cosine NEON for dimension: " << dim);
    
    // Generate normalized vectors
    auto vec1 = generate_normalized_vector<float>(dim, 42);
    auto vec2 = generate_normalized_vector<float>(dim, 43);
    
    // Compute scalar reference
    float dot_product = 0;
    for (size_t i = 0; i < dim; ++i) {
        dot_product += vec1[i] * vec2[i];
    }
    float scalar_result = 1.0f - dot_product;
    
    // Compute NEON result
    float neon_result = diskann::neon::cosine_distance_neon(vec1.data(), vec2.data(), dim);
    
    // Check correctness
    BOOST_CHECK_CLOSE(neon_result, scalar_result, 0.1);
}

BOOST_AUTO_TEST_CASE(test_cosine_neon_performance) {
    SKIP_IF_NO_NEON();
    
    const size_t dim = 128;
    const size_t iterations = 100000;
    
    auto vec1 = generate_normalized_vector<float>(dim);
    auto vec2 = generate_normalized_vector<float>(dim);
    
    // Scalar implementation
    auto scalar_fn = [&]() {
        float dot = 0;
        for (size_t i = 0; i < dim; ++i) {
            dot += vec1[i] * vec2[i];
        }
        return 1.0f - dot;
    };
    
    // NEON implementation
    auto neon_fn = [&]() {
        return diskann::neon::cosine_distance_neon(vec1.data(), vec2.data(), dim);
    };
    
    auto result = compare_performance("Cosine Distance", scalar_fn, neon_fn, iterations);
    result.print();
    
    BOOST_CHECK_MESSAGE(result.passed, 
        "NEON Cosine distance should be at least 2x faster than scalar (got " 
        << result.speedup << "x)");
}
#endif

BOOST_DATA_TEST_CASE(test_inner_product_correctness, bdata::make(test_dimensions), dim) {
    BOOST_TEST_MESSAGE("Testing Inner Product for dimension: " << dim);
    
    // Generate test vectors
    auto vec1 = generate_random_vector<float>(dim, -10.0f, 10.0f, 42);
    auto vec2 = generate_random_vector<float>(dim, -10.0f, 10.0f, 43);
    
    // Get distance function
    auto dist_fn = std::unique_ptr<diskann::Distance<float>>(
        diskann::get_distance_function<float>(diskann::Metric::INNER_PRODUCT));
    
    // Compute distance (negative inner product)
    float distance = dist_fn->compare(vec1.data(), vec2.data(), dim);
    
    // Compute expected inner product
    float expected = 0;
    for (size_t i = 0; i < dim; ++i) {
        expected += vec1[i] * vec2[i];
    }
    expected = -expected; // DiskANN uses negative inner product
    
    // Check within tolerance
    BOOST_CHECK_CLOSE(distance, expected, 0.01); // 0.01% tolerance
}

#if defined(__aarch64__) || defined(_M_ARM64)
BOOST_DATA_TEST_CASE(test_dot_product_neon_correctness, bdata::make(test_dimensions), dim) {
    SKIP_IF_NO_NEON();
    
    BOOST_TEST_MESSAGE("Testing Dot Product NEON for dimension: " << dim);
    
    // Generate test vectors
    auto vec1 = generate_random_vector<float>(dim, -10.0f, 10.0f, 42);
    auto vec2 = generate_random_vector<float>(dim, -10.0f, 10.0f, 43);
    
    // Compute scalar reference
    float scalar_result = 0;
    for (size_t i = 0; i < dim; ++i) {
        scalar_result += vec1[i] * vec2[i];
    }
    
    // Compute NEON result
    float neon_result = diskann::neon::dot_product_neon(vec1.data(), vec2.data(), dim);
    
    // Check correctness
    BOOST_CHECK_CLOSE(neon_result, scalar_result, 0.01);
}

BOOST_AUTO_TEST_CASE(test_dot_product_neon_performance) {
    SKIP_IF_NO_NEON();
    
    const size_t dim = 128;
    const size_t iterations = 100000;
    
    auto vec1 = generate_random_vector<float>(dim);
    auto vec2 = generate_random_vector<float>(dim);
    
    // Scalar implementation
    auto scalar_fn = [&]() {
        float sum = 0;
        for (size_t i = 0; i < dim; ++i) {
            sum += vec1[i] * vec2[i];
        }
        return sum;
    };
    
    // NEON implementation
    auto neon_fn = [&]() {
        return diskann::neon::dot_product_neon(vec1.data(), vec2.data(), dim);
    };
    
    auto result = compare_performance("Dot Product", scalar_fn, neon_fn, iterations);
    result.print();
    
    BOOST_CHECK_MESSAGE(result.passed, 
        "NEON Dot Product should be at least 2x faster than scalar (got " 
        << result.speedup << "x)");
}
#endif

// Test edge cases
BOOST_AUTO_TEST_CASE(test_distance_edge_cases) {
    const size_t dim = 128;
    
    // Test with zero vectors
    std::vector<float> zero1(dim, 0.0f);
    std::vector<float> zero2(dim, 0.0f);
    
    auto l2_fn = std::unique_ptr<diskann::Distance<float>>(
        diskann::get_distance_function<float>(diskann::Metric::L2));
    
    float zero_distance = l2_fn->compare(zero1.data(), zero2.data(), dim);
    BOOST_CHECK_SMALL(zero_distance, 1e-6f);
    
    // Test with identical vectors
    auto vec = generate_random_vector<float>(dim);
    float same_distance = l2_fn->compare(vec.data(), vec.data(), dim);
    BOOST_CHECK_SMALL(same_distance, 1e-6f);
    
    // Test with very small dimensions
    for (size_t small_dim : {1, 2, 3, 4, 7, 8, 15, 16}) {
        auto v1 = generate_random_vector<float>(small_dim);
        auto v2 = generate_random_vector<float>(small_dim);
        
        float dist = l2_fn->compare(v1.data(), v2.data(), small_dim);
        
        // Verify it doesn't crash and gives reasonable results
        BOOST_CHECK(dist >= 0);
        BOOST_CHECK(std::isfinite(dist));
    }
}

// Test memory alignment sensitivity
BOOST_AUTO_TEST_CASE(test_distance_alignment) {
    const size_t dim = 128;
    
    // Create misaligned vectors
    std::vector<float> buffer1(dim + 16);
    std::vector<float> buffer2(dim + 16);
    
    // Test different alignments
    for (size_t offset = 0; offset < 4; ++offset) {
        float* vec1 = buffer1.data() + offset;
        float* vec2 = buffer2.data() + offset;
        
        // Fill with random data
        for (size_t i = 0; i < dim; ++i) {
            vec1[i] = static_cast<float>(rand()) / RAND_MAX;
            vec2[i] = static_cast<float>(rand()) / RAND_MAX;
        }
        
        auto l2_fn = std::unique_ptr<diskann::Distance<float>>(
            diskann::get_distance_function<float>(diskann::Metric::L2));
        
        float dist = l2_fn->compare(vec1, vec2, dim);
        
        // Should work regardless of alignment
        BOOST_CHECK(dist >= 0);
        BOOST_CHECK(std::isfinite(dist));
    }
}

BOOST_AUTO_TEST_SUITE_END()

// Performance benchmark suite
BOOST_AUTO_TEST_SUITE(DistancePerformanceBenchmarks)

BOOST_AUTO_TEST_CASE(comprehensive_performance_test) {
    BOOST_TEST_MESSAGE("\n=== Comprehensive Distance Performance Test ===");
    
    BenchmarkReporter reporter;
    
    // Test various dimensions
    std::vector<size_t> bench_dims = {32, 64, 128, 256, 512, 1024};
    
    for (size_t dim : bench_dims) {
        BOOST_TEST_MESSAGE("\nDimension: " << dim);
        
        auto vec1 = generate_random_vector<float>(dim);
        auto vec2 = generate_random_vector<float>(dim);
        auto norm_vec1 = generate_normalized_vector<float>(dim);
        auto norm_vec2 = generate_normalized_vector<float>(dim);
        
        // Create distance functions
        auto l2_fn = std::unique_ptr<diskann::Distance<float>>(
            diskann::get_distance_function<float>(diskann::Metric::L2));
        auto cosine_fn = std::unique_ptr<diskann::Distance<float>>(
            diskann::get_distance_function<float>(diskann::Metric::COSINE));
        auto ip_fn = std::unique_ptr<diskann::Distance<float>>(
            diskann::get_distance_function<float>(diskann::Metric::INNER_PRODUCT));
        
        // Benchmark L2
        {
            Timer timer;
            for (int i = 0; i < 10000; ++i) {
                l2_fn->compare(vec1.data(), vec2.data(), dim);
            }
            double time_us = timer.elapsed_us() / 10000;
            BOOST_TEST_MESSAGE("  L2 Distance: " << time_us << " μs");
        }
        
        // Benchmark Cosine
        {
            Timer timer;
            for (int i = 0; i < 10000; ++i) {
                cosine_fn->compare(norm_vec1.data(), norm_vec2.data(), dim);
            }
            double time_us = timer.elapsed_us() / 10000;
            BOOST_TEST_MESSAGE("  Cosine Distance: " << time_us << " μs");
        }
        
        // Benchmark Inner Product
        {
            Timer timer;
            for (int i = 0; i < 10000; ++i) {
                ip_fn->compare(vec1.data(), vec2.data(), dim);
            }
            double time_us = timer.elapsed_us() / 10000;
            BOOST_TEST_MESSAGE("  Inner Product: " << time_us << " μs");
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()