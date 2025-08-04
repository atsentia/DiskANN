// Test for Parallel STL with Intel TBB backend

#include <iostream>
#include <vector>
#include <algorithm>
#include <execution>
#include <numeric>
#include <chrono>

// Force TBB backend for parallel STL
#define PSTL_USE_TBB_BACKEND 1

// Test if we can use Intel TBB's parallel STL
void test_tbb_parallel_stl() {
    std::cout << "\n=== Testing Parallel STL with TBB ===" << std::endl;
    
    const size_t N = 10000000;
    std::vector<int> data(N);
    std::iota(data.begin(), data.end(), 1);
    
    // Test 1: Parallel for_each
    {
        auto start = std::chrono::high_resolution_clock::now();
        
        try {
            std::for_each(std::execution::par_unseq, 
                         data.begin(), data.end(),
                         [](int& x) { x = x * 2; });
            
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            
            std::cout << "Parallel for_each: SUCCESS (" << duration.count() << " ms)" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Parallel for_each: FAILED - " << e.what() << std::endl;
        }
    }
    
    // Test 2: Parallel reduce
    {
        auto start = std::chrono::high_resolution_clock::now();
        
        try {
            auto sum = std::reduce(std::execution::par_unseq,
                                  data.begin(), data.end(),
                                  0L, std::plus<long>());
            
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            
            std::cout << "Parallel reduce: SUCCESS (" << duration.count() << " ms)" << std::endl;
            std::cout << "Sum: " << sum << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Parallel reduce: FAILED - " << e.what() << std::endl;
        }
    }
    
    // Test 3: Parallel sort
    {
        std::vector<int> sort_data(1000000);
        std::generate(sort_data.begin(), sort_data.end(), std::rand);
        
        auto start = std::chrono::high_resolution_clock::now();
        
        try {
            std::sort(std::execution::par_unseq,
                     sort_data.begin(), sort_data.end());
            
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            
            bool sorted = std::is_sorted(sort_data.begin(), sort_data.end());
            std::cout << "Parallel sort: " << (sorted ? "SUCCESS" : "FAILED") 
                      << " (" << duration.count() << " ms)" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Parallel sort: FAILED - " << e.what() << std::endl;
        }
    }
}

// Test compilation with execution policies
void test_execution_policies() {
    std::cout << "\n=== Testing Execution Policy Availability ===" << std::endl;
    
    // Check if policies are defined
    #ifdef __cpp_lib_execution
        std::cout << "__cpp_lib_execution is defined" << std::endl;
    #else
        std::cout << "__cpp_lib_execution is NOT defined" << std::endl;
    #endif
    
    #ifdef __cpp_lib_parallel_algorithm
        std::cout << "__cpp_lib_parallel_algorithm = " << __cpp_lib_parallel_algorithm << std::endl;
    #else
        std::cout << "__cpp_lib_parallel_algorithm is NOT defined" << std::endl;
    #endif
    
    // Try to use policies directly
    std::cout << "\nTrying to instantiate execution policies..." << std::endl;
    
    try {
        auto seq = std::execution::seq;
        (void)seq;
        std::cout << "std::execution::seq: OK" << std::endl;
    } catch (...) {
        std::cout << "std::execution::seq: FAILED" << std::endl;
    }
    
    try {
        auto par = std::execution::par;
        (void)par;
        std::cout << "std::execution::par: OK" << std::endl;
    } catch (...) {
        std::cout << "std::execution::par: FAILED" << std::endl;
    }
    
    try {
        auto par_unseq = std::execution::par_unseq;
        (void)par_unseq;
        std::cout << "std::execution::par_unseq: OK" << std::endl;
    } catch (...) {
        std::cout << "std::execution::par_unseq: FAILED" << std::endl;
    }
}

int main() {
    std::cout << "=== Parallel STL with TBB Test ===" << std::endl;
    
    // Check TBB availability
    #ifdef TBB_VERSION_MAJOR
        std::cout << "Intel TBB version: " << TBB_VERSION_MAJOR << "." << TBB_VERSION_MINOR << std::endl;
    #else
        std::cout << "Intel TBB version not detected at compile time" << std::endl;
    #endif
    
    test_execution_policies();
    test_tbb_parallel_stl();
    
    std::cout << "\nTest completed!" << std::endl;
    return 0;
}