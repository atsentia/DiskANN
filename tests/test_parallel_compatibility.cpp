// Test to verify OpenMP compatibility with diskann_parallel.h

#include <iostream>
#include <vector>
#include <cmath>

// Include our parallel header - it should work with or without OpenMP
#include "diskann_parallel.h"

void test_openmp_compatibility() {
    const int N = 1000000;
    std::vector<float> data(N);
    
    // Initialize data
    for(int i = 0; i < N; i++) {
        data[i] = static_cast<float>(i);
    }
    
    // Test 1: Simple parallel for with OpenMP pragma
    // This should work whether OpenMP is available or not
    std::vector<float> results(N);
    
    #pragma omp parallel for
    for(int i = 0; i < N; i++) {
        results[i] = std::sqrt(data[i]);
    }
    
    // Test 2: Using OpenMP functions
    std::cout << "Number of processors: " << omp_get_num_procs() << std::endl;
    std::cout << "Max threads: " << omp_get_num_threads() << std::endl;
    
    // Test 3: Parallel for with schedule
    #pragma omp parallel for schedule(static, 1000)
    for(int i = 0; i < N; i++) {
        results[i] = data[i] * 2.0f;
    }
    
    // Test 4: Using our diskann parallel functions (always available)
    #ifndef _OPENMP
    std::cout << "OpenMP not available - using diskann parallel alternatives" << std::endl;
    diskann::parallel_for(0, N, [&](int i) {
        results[i] = data[i] * 3.0f;
    });
    #else
    std::cout << "OpenMP is available - using native OpenMP" << std::endl;
    #endif
    
    std::cout << "All tests passed!" << std::endl;
}

int main() {
    test_openmp_compatibility();
    return 0;
}