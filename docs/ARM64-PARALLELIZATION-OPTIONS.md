# ARM64 Cross-Platform Parallelization Options

## Overview
When porting high-performance code like DiskANN to ARM64, choosing the right parallelization framework is crucial. Here are the best options that work across all ARM64 platforms (Apple Silicon, AWS Graviton, Azure Ampere, Qualcomm Snapdragon).

## 1. **std::execution (C++17/20) - RECOMMENDED**
The C++ standard library's parallel algorithms are the most portable solution.

### Pros:
- Part of the C++ standard (no external dependencies)
- Works on ALL platforms with modern C++ compilers
- Excellent ARM64 support across all vendors
- Automatically uses platform-specific optimizations

### Example:
```cpp
#include <execution>
#include <algorithm>
#include <vector>

// Parallel transform
std::transform(std::execution::par_unseq, 
               data.begin(), data.end(), 
               result.begin(),
               [](float x) { return x * x; });

// Parallel reduce
float sum = std::reduce(std::execution::par,
                       data.begin(), data.end(),
                       0.0f);
```

### Platform Support:
- ✅ Apple Silicon: Full support in Xcode 12+
- ✅ Linux ARM64: GCC 9+, Clang 10+
- ✅ Windows ARM64: MSVC 2019+

## 2. **Intel TBB (Threading Building Blocks) - Now oneTBB**
Despite the Intel name, TBB has excellent ARM64 support and is open-source.

### Pros:
- Mature, battle-tested library
- Excellent work-stealing scheduler
- Rich set of parallel algorithms
- Available via package managers (apt, brew, vcpkg)

### Example:
```cpp
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>

tbb::parallel_for(tbb::blocked_range<size_t>(0, n),
    [&](const tbb::blocked_range<size_t>& r) {
        for(size_t i = r.begin(); i != r.end(); ++i) {
            // Process data[i]
        }
    });
```

### Installation:
```bash
# macOS
brew install tbb

# Ubuntu/Debian
sudo apt-get install libtbb-dev

# Build from source for any ARM64 platform
git clone https://github.com/oneapi-src/oneTBB.git
cd oneTBB && mkdir build && cd build
cmake .. && make -j
```

## 3. **HPX (High Performance ParalleX)**
Modern C++ parallel runtime system with excellent ARM64 support.

### Pros:
- Future-based programming model
- Scales from single-core to clusters
- Active development with ARM64 focus
- NASA and supercomputing adoption

### Example:
```cpp
#include <hpx/hpx.hpp>
#include <hpx/parallel/algorithms/for_each.hpp>

hpx::parallel::for_each(hpx::parallel::execution::par,
                       data.begin(), data.end(),
                       [](float& x) { x *= 2.0f; });
```

## 4. **cpp-taskflow (Now Taskflow)**
A modern C++ parallel programming framework using task graphs.

### Pros:
- Header-only library (easy integration)
- Excellent ARM64 performance
- Used by major projects (AMD, Nvidia)
- Intuitive task-based API

### Example:
```cpp
#include <taskflow/taskflow.hpp>

tf::Executor executor;
tf::Taskflow taskflow;

auto [A, B, C, D] = taskflow.emplace(
    [] () { std::cout << "Task A\n"; },
    [] () { std::cout << "Task B\n"; },
    [] () { std::cout << "Task C\n"; },
    [] () { std::cout << "Task D\n"; }
);

A.precede(B, C);  // A runs before B and C
B.precede(D);     // B runs before D
C.precede(D);     // C runs before D

executor.run(taskflow).wait();
```

## 5. **Platform-Specific Options**

### Apple Silicon: Grand Central Dispatch (GCD)
```cpp
#include <dispatch/dispatch.h>

dispatch_apply(count, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
    ^(size_t i) {
        // Process item i
    });
```

### Linux ARM64: POSIX Threads with Thread Pools
Most portable but requires more manual work.

## 6. **Kokkos (HPC-focused)**
Developed by Sandia National Labs, excellent for scientific computing.

### Pros:
- Performance portable across CPUs and GPUs
- Strong ARM64 support (used on Fugaku supercomputer)
- Abstracts memory layouts for optimal performance

## Recommendations for DiskANN

### Best Choice: **std::execution + TBB hybrid**

1. **Use std::execution for simple parallel loops**
   - Most portable
   - Zero dependencies
   - Compiler can optimize for specific ARM64 variant

2. **Use TBB for complex patterns**
   - Work stealing for imbalanced loads
   - Concurrent containers
   - Pipeline patterns

### Implementation Strategy:
```cpp
#ifdef __has_include
  #if __has_include(<execution>)
    #define HAS_PARALLEL_STL 1
    #include <execution>
  #endif
#endif

#ifdef HAS_PARALLEL_STL
  #define DISKANN_PAR_UNSEQ std::execution::par_unseq
#else
  // Fallback to TBB or custom thread pool
  #include "parallel_framework.h"
#endif
```

## ARM64-Specific Optimizations

### 1. **Thread Affinity**
```cpp
#ifdef __linux__
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_id, &cpuset);
  pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
#elif __APPLE__
  // macOS doesn't allow direct CPU affinity
  // Use QoS classes instead
  pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
#endif
```

### 2. **NUMA Awareness** (for server ARM64)
```cpp
#ifdef __linux__
  #include <numa.h>
  if (numa_available() >= 0) {
    // Bind memory and threads to NUMA nodes
    numa_set_localalloc();
  }
#endif
```

### 3. **ARM64 Cache Line Size**
```cpp
// ARM64 typically has 64-byte cache lines (some have 128)
#if defined(__aarch64__) || defined(_M_ARM64)
  constexpr size_t CACHE_LINE_SIZE = 64;
#else
  constexpr size_t CACHE_LINE_SIZE = 64; // x86-64 default
#endif

// Align data structures to prevent false sharing
struct alignas(CACHE_LINE_SIZE) WorkerData {
  // ...
};
```

## Testing & Benchmarking

### Platform Detection:
```cpp
std::string get_arm64_platform() {
#ifdef __APPLE__
    return "Apple Silicon";
#elif defined(__linux__)
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;
    while (std::getline(cpuinfo, line)) {
        if (line.find("CPU implementer") != std::string::npos) {
            if (line.find("0x41") != std::string::npos) return "ARM Cortex";
            if (line.find("0x42") != std::string::npos) return "Broadcom";
            if (line.find("0x43") != std::string::npos) return "Cavium";
            if (line.find("0x50") != std::string::npos) return "APM";
            if (line.find("0x51") != std::string::npos) return "Qualcomm";
            if (line.find("0x53") != std::string::npos) return "Samsung";
            if (line.find("0x54") != std::string::npos) return "HiSilicon";
            if (line.find("0x61") != std::string::npos) return "Apple";
            if (line.find("0xC0") != std::string::npos) return "Ampere";
        }
    }
#elif defined(_WIN32)
    return "Windows ARM64";
#endif
    return "Unknown ARM64";
}
```

## Conclusion

For DiskANN on ARM64, the best approach is:

1. **Primary**: Use C++17 parallel STL (std::execution) for maximum portability
2. **Fallback**: Intel TBB for complex parallel patterns
3. **Custom**: Our parallel_framework.h for fine control when needed

This combination ensures:
- ✅ No dependency on OpenMP
- ✅ Excellent performance on all ARM64 platforms
- ✅ Future-proof as C++ standard evolves
- ✅ Easy to maintain and understand