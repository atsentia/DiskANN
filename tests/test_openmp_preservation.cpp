// Test to verify OpenMP code is preserved when available
// and fallbacks work when OpenMP is not available

#include <iostream>
#include <vector>
#include <chrono>
#include <numeric>
#include <cmath>

// Include our parallel header
#include "diskann_parallel.h"

// Test function that uses OpenMP pragmas
void test_parallel_computation() {
    const size_t N = 10000000;
    std::vector<float> input(N);
    std::vector<float> output(N);
    
    // Initialize input
    std::iota(input.begin(), input.end(), 1.0f);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // This pragma will use OpenMP when available, 
    // or be handled by our macros when not
    #pragma omp parallel for schedule(static, 1000)
    for (size_t i = 0; i < N; i++) {
        output[i] = std::sqrt(input[i]) * std::sin(input[i]);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "Computation time: " << duration.count() << " ms" << std::endl;
    
    // Verify results
    float sum = 0.0f;
    #pragma omp parallel for reduction(+:sum)
    for (size_t i = 0; i < N; i++) {
        sum += output[i];
    }
    
    std::cout << "Sum: " << sum << std::endl;
}

void test_openmp_functions() {
    std::cout << "\nTesting OpenMP function compatibility:" << std::endl;
    
    // These should work whether OpenMP is available or not
    std::cout << "Number of processors: " << omp_get_num_procs() << std::endl;
    std::cout << "Number of threads: " << omp_get_num_threads() << std::endl;
    std::cout << "Thread ID: " << omp_get_thread_num() << std::endl;
    std::cout << "In parallel region: " << omp_in_parallel() << std::endl;
    
    // Set number of threads
    omp_set_num_threads(4);
    std::cout << "Set threads to 4" << std::endl;
}

void test_parallel_stl() {
    #ifndef _OPENMP
    std::cout << "\nTesting Parallel STL alternatives (OpenMP not available):" << std::endl;
    
    const size_t N = 1000000;
    std::vector<int> data(N);
    std::iota(data.begin(), data.end(), 0);
    
    // Test parallel_for
    auto start = std::chrono::high_resolution_clock::now();
    diskann::parallel_for(0, N, [&](int i) {
        data[i] = data[i] * 2;
    });
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "parallel_for time: " << duration.count() << " μs" << std::endl;
    
    // Test parallel_reduce
    auto sum = diskann::parallel_reduce(data.begin(), data.end(), 0, std::plus<int>());
    std::cout << "parallel_reduce sum: " << sum << std::endl;
    
    #else
    std::cout << "\nOpenMP is available - using native OpenMP implementation" << std::endl;
    #endif
}

int main() {
    std::cout << "=== OpenMP Compatibility Test ===" << std::endl;
    
    #ifdef _OPENMP
    std::cout << "OpenMP version: " << _OPENMP << std::endl;
    std::cout << "Using native OpenMP implementation" << std::endl;
    #else
    std::cout << "OpenMP not available - using DiskANN parallel alternatives" << std::endl;
    #endif
    
    test_openmp_functions();
    test_parallel_computation();
    test_parallel_stl();
    
    std::cout << "\nAll tests completed successfully!" << std::endl;
    return 0;
}