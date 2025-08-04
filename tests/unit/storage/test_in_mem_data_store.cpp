#define BOOST_TEST_MODULE InMemDataStoreTests
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>

#include "in_mem_data_store.h"
#include "distance.h"
#include "../../test_utils.h"

namespace bdata = boost::unit_test::data;
using namespace diskann::test;

BOOST_AUTO_TEST_SUITE(InMemDataStoreTests)

BOOST_AUTO_TEST_CASE(test_data_store_creation) {
    const size_t num_points = 1000;
    const size_t dim = 128;
    
    diskann::InMemDataStore<float> data_store(num_points, dim);
    
    BOOST_CHECK_EQUAL(data_store.get_num_points(), num_points);
    BOOST_CHECK_EQUAL(data_store.get_dims(), dim);
    BOOST_CHECK_EQUAL(data_store.get_distance_fn()->get_type(), diskann::Metric::L2);
}

BOOST_AUTO_TEST_CASE(test_data_alignment) {
    const size_t num_points = 100;
    const size_t dim = 128;
    
    diskann::InMemDataStore<float> data_store(num_points, dim);
    
    // Check alignment of data vectors
    for (size_t i = 0; i < num_points; ++i) {
        const float* vec = data_store.get_vector(i);
        
        // Data should be at least 16-byte aligned for SIMD
        bool aligned = is_aligned(vec, 16);
        BOOST_CHECK_MESSAGE(aligned, 
            "Vector " << i << " is not 16-byte aligned (address: " << vec << ")");
        
#if defined(__aarch64__) || defined(_M_ARM64)
        // On ARM64, prefer 32-byte alignment for better cache performance
        if (is_neon_available()) {
            bool aligned_32 = is_aligned(vec, 32);
            if (!aligned_32) {
                BOOST_TEST_MESSAGE("Warning: Vector " << i << 
                    " is not 32-byte aligned for optimal NEON performance");
            }
        }
#endif
    }
}

BOOST_AUTO_TEST_CASE(test_data_store_operations) {
    const size_t num_points = 100;
    const size_t dim = 64;
    
    diskann::InMemDataStore<float> data_store(num_points, dim);
    
    // Generate and store test data
    std::vector<std::vector<float>> test_vectors;
    for (size_t i = 0; i < num_points; ++i) {
        test_vectors.push_back(generate_random_vector<float>(dim));
    }
    
    // Set vectors
    for (size_t i = 0; i < num_points; ++i) {
        data_store.set_vector(i, test_vectors[i].data());
    }
    
    // Verify vectors
    for (size_t i = 0; i < num_points; ++i) {
        const float* stored = data_store.get_vector(i);
        BOOST_CHECK(vectors_equal(stored, test_vectors[i].data(), dim));
    }
    
    // Test prefetch
    data_store.prefetch_vector(0);
    data_store.prefetch_vectors({1, 2, 3, 4, 5});
    
    // Test distance computation
    float dist = data_store.get_distance(0, 1);
    BOOST_CHECK(dist >= 0);
    BOOST_CHECK(std::isfinite(dist));
    
    // Compute expected distance
    auto dist_fn = data_store.get_distance_fn();
    float expected = dist_fn->compare(test_vectors[0].data(), 
                                     test_vectors[1].data(), dim);
    BOOST_CHECK_CLOSE(dist, expected, 0.01);
}

BOOST_AUTO_TEST_CASE(test_data_store_with_different_metrics) {
    const size_t num_points = 50;
    const size_t dim = 128;
    
    // Test with L2 distance
    {
        diskann::InMemDataStore<float> l2_store(num_points, dim, diskann::Metric::L2);
        auto vec1 = generate_random_vector<float>(dim);
        auto vec2 = generate_random_vector<float>(dim);
        
        l2_store.set_vector(0, vec1.data());
        l2_store.set_vector(1, vec2.data());
        
        float dist = l2_store.get_distance(0, 1);
        
        // Manually compute L2 distance
        float expected = 0;
        for (size_t i = 0; i < dim; ++i) {
            float diff = vec1[i] - vec2[i];
            expected += diff * diff;
        }
        
        BOOST_CHECK_CLOSE(dist, expected, 0.01);
    }
    
    // Test with Cosine distance
    {
        diskann::InMemDataStore<float> cosine_store(num_points, dim, diskann::Metric::COSINE);
        auto vec1 = generate_normalized_vector<float>(dim);
        auto vec2 = generate_normalized_vector<float>(dim);
        
        cosine_store.set_vector(0, vec1.data());
        cosine_store.set_vector(1, vec2.data());
        
        float dist = cosine_store.get_distance(0, 1);
        
        // Manually compute cosine distance
        float dot = 0;
        for (size_t i = 0; i < dim; ++i) {
            dot += vec1[i] * vec2[i];
        }
        float expected = 1.0f - dot;
        
        BOOST_CHECK_CLOSE(dist, expected, 0.1);
    }
}

BOOST_AUTO_TEST_CASE(test_batch_operations) {
    const size_t num_points = 1000;
    const size_t dim = 128;
    const size_t batch_size = 100;
    
    diskann::InMemDataStore<float> data_store(num_points, dim);
    
    // Generate batch data
    std::vector<float> batch_data;
    for (size_t i = 0; i < batch_size; ++i) {
        auto vec = generate_random_vector<float>(dim);
        batch_data.insert(batch_data.end(), vec.begin(), vec.end());
    }
    
    // Load batch
    std::vector<location_t> locations;
    for (size_t i = 0; i < batch_size; ++i) {
        locations.push_back(i);
    }
    
    data_store.load_batch(locations, batch_data.data());
    
    // Verify batch load
    for (size_t i = 0; i < batch_size; ++i) {
        const float* stored = data_store.get_vector(i);
        const float* expected = batch_data.data() + i * dim;
        BOOST_CHECK(vectors_equal(stored, expected, dim));
    }
    
    // Test batch extract
    std::vector<float> extracted_data(batch_size * dim);
    data_store.extract_batch(locations, extracted_data.data());
    
    // Verify extraction
    BOOST_CHECK(vectors_equal(extracted_data.data(), batch_data.data(), 
                             batch_size * dim));
}

BOOST_AUTO_TEST_CASE(test_data_store_performance) {
    const size_t num_points = 10000;
    const size_t dim = 128;
    const size_t num_queries = 1000;
    
    diskann::InMemDataStore<float> data_store(num_points, dim);
    
    // Load data
    for (size_t i = 0; i < num_points; ++i) {
        auto vec = generate_random_vector<float>(dim);
        data_store.set_vector(i, vec.data());
    }
    
    // Benchmark sequential access
    {
        Timer timer;
        for (size_t i = 0; i < num_queries; ++i) {
            const float* vec = data_store.get_vector(i % num_points);
            // Force read to prevent optimization
            volatile float sum = vec[0];
            (void)sum;
        }
        double time_ms = timer.elapsed_ms();
        double us_per_access = (time_ms * 1000) / num_queries;
        
        BOOST_TEST_MESSAGE("Sequential access: " << us_per_access << " μs/access");
        BOOST_CHECK_LT(us_per_access, 1.0); // Should be sub-microsecond
    }
    
    // Benchmark random access
    {
        std::vector<location_t> random_indices;
        for (size_t i = 0; i < num_queries; ++i) {
            random_indices.push_back(rand() % num_points);
        }
        
        Timer timer;
        for (location_t idx : random_indices) {
            const float* vec = data_store.get_vector(idx);
            volatile float sum = vec[0];
            (void)sum;
        }
        double time_ms = timer.elapsed_ms();
        double us_per_access = (time_ms * 1000) / num_queries;
        
        BOOST_TEST_MESSAGE("Random access: " << us_per_access << " μs/access");
        BOOST_CHECK_LT(us_per_access, 2.0); // Random should still be fast
    }
    
    // Benchmark distance computations
    {
        Timer timer;
        for (size_t i = 0; i < num_queries; ++i) {
            location_t id1 = rand() % num_points;
            location_t id2 = rand() % num_points;
            float dist = data_store.get_distance(id1, id2);
            volatile float v = dist;
            (void)v;
        }
        double time_ms = timer.elapsed_ms();
        double us_per_distance = (time_ms * 1000) / num_queries;
        
        BOOST_TEST_MESSAGE("Distance computation: " << us_per_distance << " μs/computation");
        
#if defined(__aarch64__) || defined(_M_ARM64)
        if (is_neon_available()) {
            // With NEON optimizations, should be significantly faster
            BOOST_CHECK_LT(us_per_distance, 0.5);
        }
#endif
    }
}

BOOST_AUTO_TEST_CASE(test_data_store_memory_usage) {
    const size_t num_points = 1000;
    const size_t dim = 128;
    
    diskann::InMemDataStore<float> data_store(num_points, dim);
    
    // Expected memory usage
    size_t expected_data = num_points * dim * sizeof(float);
    size_t expected_overhead = num_points * 64; // Reasonable overhead per point
    size_t expected_total = expected_data + expected_overhead;
    
    // Get actual memory usage (this would need implementation in InMemDataStore)
    // For now, just verify data is accessible
    for (size_t i = 0; i < num_points; ++i) {
        const float* vec = data_store.get_vector(i);
        BOOST_CHECK(vec != nullptr);
    }
    
    BOOST_TEST_MESSAGE("Data store created for " << num_points << 
                      " points of dimension " << dim);
    BOOST_TEST_MESSAGE("Expected data size: " << expected_data / (1024.0 * 1024.0) << " MB");
}

// Test thread safety
BOOST_AUTO_TEST_CASE(test_data_store_thread_safety) {
    const size_t num_points = 1000;
    const size_t dim = 128;
    const size_t num_threads = 4;
    const size_t ops_per_thread = 1000;
    
    diskann::InMemDataStore<float> data_store(num_points, dim);
    
    // Initialize data
    for (size_t i = 0; i < num_points; ++i) {
        auto vec = generate_random_vector<float>(dim);
        data_store.set_vector(i, vec.data());
    }
    
    std::vector<std::thread> threads;
    std::atomic<size_t> total_ops(0);
    
    // Launch threads that perform concurrent reads
    for (size_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            std::mt19937 gen(t);
            std::uniform_int_distribution<location_t> dis(0, num_points - 1);
            
            for (size_t op = 0; op < ops_per_thread; ++op) {
                location_t id1 = dis(gen);
                location_t id2 = dis(gen);
                
                // Read vectors
                const float* v1 = data_store.get_vector(id1);
                const float* v2 = data_store.get_vector(id2);
                
                // Compute distance
                float dist = data_store.get_distance(id1, id2);
                
                // Basic sanity checks
                BOOST_CHECK(v1 != nullptr);
                BOOST_CHECK(v2 != nullptr);
                BOOST_CHECK(dist >= 0);
                BOOST_CHECK(std::isfinite(dist));
                
                total_ops++;
            }
        });
    }
    
    // Wait for completion
    for (auto& thread : threads) {
        thread.join();
    }
    
    BOOST_CHECK_EQUAL(total_ops.load(), num_threads * ops_per_thread);
    BOOST_TEST_MESSAGE("Successfully completed " << total_ops << 
                      " concurrent operations");
}

BOOST_AUTO_TEST_SUITE_END()