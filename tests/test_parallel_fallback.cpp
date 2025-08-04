// Test for the enhanced parallel implementation with fallbacks

#include <iostream>
#include <vector>
#include <chrono>
#include <numeric>
#include <cmath>

// Include our parallel header
#include "diskann_parallel.h"

void test_parallel_for() {
    std::cout << "\n=== Testing parallel_for ===" << std::endl;
    
    const size_t N = 1000000;
    std::vector<double> data(N);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    diskann::parallel_for(size_t(0), N, [&data](size_t i) {
        data[i] = std::sqrt(double(i)) * std::sin(double(i));
    });
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "Time: " << duration.count() << " ms" << std::endl;
    std::cout << "Sample results: data[0]=" << data[0] 
              << ", data[N-1]=" << data[N-1] << std::endl;
}

void test_parallel_reduce() {
    std::cout << "\n=== Testing parallel_reduce ===" << std::endl;
    
    const size_t N = 10000000;
    std::vector<int> data(N);
    std::iota(data.begin(), data.end(), 1);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    auto sum = diskann::parallel_reduce(data.begin(), data.end(), 0L, std::plus<long>());
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "Time: " << duration.count() << " ms" << std::endl;
    std::cout << "Sum: " << sum << " (expected: " << (long)N*(N+1)/2 << ")" << std::endl;
}

void test_thread_pool() {
    std::cout << "\n=== Testing thread pool ===" << std::endl;
    
    #ifndef _OPENMP
    auto& pool = diskann::get_thread_pool();
    std::cout << "Thread pool size: " << pool.num_threads() << std::endl;
    
    // Test basic task execution
    std::atomic<int> counter{0};
    const int num_tasks = 100;
    
    std::vector<std::future<void>> futures;
    for (int i = 0; i < num_tasks; ++i) {
        futures.push_back(
            pool.enqueue([&counter]() {
                counter++;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            })
        );
    }
    
    // Wait for all tasks
    for (auto& future : futures) {
        future.wait();
    }
    
    std::cout << "Tasks completed: " << counter.load() 
              << " (expected: " << num_tasks << ")" << std::endl;
    #else
    std::cout << "OpenMP is available - thread pool not used" << std::endl;
    #endif
}

void test_dynamic_scheduling() {
    std::cout << "\n=== Testing dynamic scheduling ===" << std::endl;
    
    const size_t N = 100;
    std::vector<int> work_done(N, 0);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    diskann::parallel_for_dynamic(size_t(0), size_t(N), [&work_done](size_t i) {
        // Simulate variable work
        std::this_thread::sleep_for(std::chrono::microseconds(i * 10));
        work_done[i] = 1;
    }, size_t(10)); // chunk size of 10
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    int total_work = std::accumulate(work_done.begin(), work_done.end(), 0);
    
    std::cout << "Time: " << duration.count() << " ms" << std::endl;
    std::cout << "Work items completed: " << total_work 
              << " (expected: " << N << ")" << std::endl;
}

int main() {
    std::cout << "=== DiskANN Parallel Implementation Test ===" << std::endl;
    
    #ifdef _OPENMP
    std::cout << "OpenMP is available (version " << _OPENMP << ")" << std::endl;
    #else
    std::cout << "OpenMP NOT available - using fallback implementation" << std::endl;
    #endif
    
    #ifdef HAS_WORKING_PARALLEL_STL
    std::cout << "C++17 Parallel STL is available" << std::endl;
    #else
    std::cout << "C++17 Parallel STL NOT available - using thread pool" << std::endl;
    #endif
    
    std::cout << "Hardware concurrency: " << diskann::get_num_threads() << std::endl;
    
    test_thread_pool();
    test_parallel_for();
    test_parallel_reduce();
    test_dynamic_scheduling();
    
    std::cout << "\nAll tests completed!" << std::endl;
    return 0;
}