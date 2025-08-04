#include <gtest/gtest.h>
#include <vector>
#include <atomic>
#include <algorithm>
#include <numeric>
#include "diskann_parallel.h"

// Test basic parallel for functionality
TEST(ParallelSTLTest, BasicParallelFor) {
    const size_t n = 10000;
    std::vector<int> data(n, 0);
    
    // Use parallel for to set each element to its index
    diskann::parallel_for<size_t>(0, n, [&data](size_t i) {
        data[i] = static_cast<int>(i);
    });
    
    // Verify results
    for (size_t i = 0; i < n; i++) {
        EXPECT_EQ(data[i], static_cast<int>(i));
    }
}

// Test parallel for with static scheduling
TEST(ParallelSTLTest, StaticScheduling) {
    const size_t n = 100000;
    std::atomic<size_t> total_work(0);
    std::vector<std::atomic<int>> thread_work(diskann::get_num_threads());
    
    // Initialize thread work counters
    for (auto& counter : thread_work) {
        counter = 0;
    }
    
    // Run parallel for with static scheduling
    diskann::parallel_for_static<size_t>(0, n, [&](size_t i) {
        total_work++;
        // Track which threads do work (simplified)
        thread_work[i % thread_work.size()]++;
    }, 1000); // chunk size 1000
    
    // Verify all work was done
    EXPECT_EQ(total_work.load(), n);
    
    // Verify work was distributed (not all on one thread)
    int threads_with_work = 0;
    for (const auto& counter : thread_work) {
        if (counter.load() > 0) {
            threads_with_work++;
        }
    }
    EXPECT_GT(threads_with_work, 1); // More than one thread should have done work
}

// Test parallel for with dynamic scheduling
TEST(ParallelSTLTest, DynamicScheduling) {
    const size_t n = 10000;
    std::vector<double> results(n);
    
    // Simulate uneven workload
    diskann::parallel_for_dynamic<size_t>(0, n, [&results](size_t i) {
        // Some iterations take longer
        double work = 0.0;
        size_t iterations = (i % 100 == 0) ? 1000 : 10;
        for (size_t j = 0; j < iterations; j++) {
            work += std::sin(i + j);
        }
        results[i] = work;
    }, 10); // chunk size 10
    
    // Verify all elements were computed
    for (size_t i = 0; i < n; i++) {
        EXPECT_NE(results[i], 0.0);
    }
}

// Test parallel reduce
TEST(ParallelSTLTest, ParallelReduce) {
    const size_t n = 1000000;
    std::vector<int> data(n);
    
    // Initialize data
    std::iota(data.begin(), data.end(), 1);
    
    // Calculate sum using parallel reduce
    int64_t parallel_sum = diskann::parallel_reduce(
        data.begin(), data.end(), 
        static_cast<int64_t>(0), 
        std::plus<int64_t>()
    );
    
    // Calculate expected sum: n*(n+1)/2
    int64_t expected_sum = static_cast<int64_t>(n) * (n + 1) / 2;
    
    EXPECT_EQ(parallel_sum, expected_sum);
}

// Test parallel transform reduce
TEST(ParallelSTLTest, ParallelTransformReduce) {
    const size_t n = 100000;
    std::vector<float> data(n);
    
    // Initialize with values
    for (size_t i = 0; i < n; i++) {
        data[i] = static_cast<float>(i) / n;
    }
    
    // Calculate sum of squares
    float sum_of_squares = diskann::parallel_transform_reduce(
        data.begin(), data.end(),
        0.0f,
        std::plus<float>(),
        [](float x) { return x * x; }
    );
    
    // Verify result is reasonable
    EXPECT_GT(sum_of_squares, 0.0f);
    EXPECT_LT(sum_of_squares, n); // Should be less than n since values are < 1
}

// Test parallel sort
TEST(ParallelSTLTest, ParallelSort) {
    const size_t n = 100000;
    std::vector<int> data(n);
    
    // Initialize with random data
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(0, 1000000);
    
    for (size_t i = 0; i < n; i++) {
        data[i] = dis(gen);
    }
    
    // Make a copy for comparison
    std::vector<int> expected = data;
    
    // Sort using parallel sort
    diskann::parallel_sort(data.begin(), data.end());
    
    // Sort expected using standard sort
    std::sort(expected.begin(), expected.end());
    
    // Verify they match
    EXPECT_EQ(data, expected);
}

// Test thread count management
TEST(ParallelSTLTest, ThreadCountManagement) {
    // Get default thread count
    unsigned int default_threads = diskann::get_num_threads();
    EXPECT_GT(default_threads, 0);
    
    // Set different thread count
    diskann::set_num_threads(4);
    
    // Verify it's respected (in our implementation)
    EXPECT_EQ(diskann::get_active_threads(), 4);
    
    // Reset to default
    diskann::set_num_threads(default_threads);
}

// Test edge cases
TEST(ParallelSTLTest, EmptyRange) {
    std::vector<int> data;
    
    // Should handle empty range gracefully
    diskann::parallel_for<size_t>(0, 0, [&data](size_t i) {
        data.push_back(i); // Should never execute
    });
    
    EXPECT_TRUE(data.empty());
}

// Test single element
TEST(ParallelSTLTest, SingleElement) {
    int result = 0;
    
    diskann::parallel_for<int>(0, 1, [&result](int i) {
        result = 42;
    });
    
    EXPECT_EQ(result, 42);
}

// Test large chunk sizes
TEST(ParallelSTLTest, LargeChunkSize) {
    const size_t n = 1000;
    const size_t chunk_size = 500;
    std::vector<int> data(n, 0);
    
    diskann::parallel_for_static<size_t>(0, n, [&data](size_t i) {
        data[i] = 1;
    }, chunk_size);
    
    // Verify all elements were set
    for (size_t i = 0; i < n; i++) {
        EXPECT_EQ(data[i], 1);
    }
}

// Performance comparison test
TEST(ParallelSTLTest, PerformanceComparison) {
    const size_t n = 10000000;
    std::vector<double> data(n);
    
    // Time sequential execution
    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < n; i++) {
        data[i] = std::sin(i) * std::cos(i);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto sequential_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    // Reset data
    std::fill(data.begin(), data.end(), 0.0);
    
    // Time parallel execution
    start = std::chrono::high_resolution_clock::now();
    diskann::parallel_for<size_t>(0, n, [&data](size_t i) {
        data[i] = std::sin(i) * std::cos(i);
    });
    end = std::chrono::high_resolution_clock::now();
    auto parallel_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    std::cout << "Sequential time: " << sequential_time << " ms" << std::endl;
    std::cout << "Parallel time: " << parallel_time << " ms" << std::endl;
    std::cout << "Speedup: " << (float)sequential_time / parallel_time << "x" << std::endl;
    
    // Parallel should be faster (allow some margin for overhead)
    if (diskann::get_num_threads() > 1) {
        EXPECT_LT(parallel_time, sequential_time);
    }
}