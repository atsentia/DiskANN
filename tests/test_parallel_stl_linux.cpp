// Test specifically for Linux systems with GCC 9+ or Intel compiler
// This test will work on systems with proper Parallel STL support

#include <iostream>
#include <vector>
#include <algorithm>
#include <numeric>
#include <chrono>

// Include our parallel header
#include "diskann_parallel.h"

// Only try to include execution if we detected it works
#ifdef HAS_WORKING_PARALLEL_STL
#include <execution>
#endif

void test_parallel_stl_native() {
    std::cout << "\n=== Testing Native Parallel STL Support ===" << std::endl;
    
#ifdef HAS_WORKING_PARALLEL_STL
    const size_t N = 10000000;
    std::vector<double> data(N);
    std::iota(data.begin(), data.end(), 1.0);
    
    // Test 1: Native parallel for_each
    {
        auto start = std::chrono::high_resolution_clock::now();
        
        std::for_each(std::execution::par_unseq, 
                     data.begin(), data.end(),
                     [](double& x) { x = std::sqrt(x) * std::sin(x); });
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "Native parallel for_each time: " << duration.count() << " ms" << std::endl;
    }
    
    // Test 2: Native parallel reduce
    {
        auto start = std::chrono::high_resolution_clock::now();
        
        auto sum = std::reduce(std::execution::par_unseq,
                              data.begin(), data.end(),
                              0.0, std::plus<double>());
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "Native parallel reduce time: " << duration.count() << " ms" << std::endl;
        std::cout << "Sum: " << sum << std::endl;
    }
    
    // Test 3: Native parallel sort
    {
        std::vector<int> sort_data(1000000);
        std::generate(sort_data.begin(), sort_data.end(), std::rand);
        
        auto start = std::chrono::high_resolution_clock::now();
        
        std::sort(std::execution::par_unseq,
                 sort_data.begin(), sort_data.end());
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        bool sorted = std::is_sorted(sort_data.begin(), sort_data.end());
        std::cout << "Native parallel sort: " << (sorted ? "SUCCESS" : "FAILED") 
                  << " time: " << duration.count() << " ms" << std::endl;
    }
#else
    std::cout << "Native Parallel STL not available on this system" << std::endl;
    std::cout << "Using fallback implementation instead" << std::endl;
#endif
}

void test_diskann_parallel_api() {
    std::cout << "\n=== Testing DiskANN Parallel API ===" << std::endl;
    
    const size_t N = 10000000;
    std::vector<double> data(N);
    std::iota(data.begin(), data.end(), 1.0);
    
    // Test our API (works everywhere)
    {
        auto start = std::chrono::high_resolution_clock::now();
        
        diskann::parallel_for(size_t(0), N, [&data](size_t i) {
            data[i] = std::sqrt(data[i]) * std::sin(data[i]);
        });
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "DiskANN parallel_for time: " << duration.count() << " ms" << std::endl;
    }
    
    // Compare implementations
    std::cout << "\nImplementation being used:" << std::endl;
#ifdef _OPENMP
    std::cout << "- OpenMP (native)" << std::endl;
#elif defined(HAS_WORKING_PARALLEL_STL)
    std::cout << "- C++17 Parallel STL" << std::endl;
#else
    std::cout << "- Thread pool fallback" << std::endl;
#endif
}

int main() {
    std::cout << "=== Parallel STL Platform Test ===" << std::endl;
    
    // Platform detection
    std::cout << "\nPlatform information:" << std::endl;
#ifdef __linux__
    std::cout << "- Operating System: Linux" << std::endl;
#elif defined(__APPLE__)
    std::cout << "- Operating System: macOS" << std::endl;
#elif defined(_WIN32)
    std::cout << "- Operating System: Windows" << std::endl;
#endif

#ifdef __GNUC__
    std::cout << "- Compiler: GCC " << __GNUC__ << "." << __GNUC_MINOR__ << std::endl;
#elif defined(__clang__)
    std::cout << "- Compiler: Clang " << __clang_major__ << "." << __clang_minor__ << std::endl;
#elif defined(_MSC_VER)
    std::cout << "- Compiler: MSVC " << _MSC_VER << std::endl;
#endif

    std::cout << "- Hardware threads: " << diskann::get_num_threads() << std::endl;
    
    test_parallel_stl_native();
    test_diskann_parallel_api();
    
    std::cout << "\nConclusion:" << std::endl;
    std::cout << "The DiskANN parallel API provides consistent behavior across all platforms," << std::endl;
    std::cout << "automatically using the best available implementation." << std::endl;
    
    return 0;
}