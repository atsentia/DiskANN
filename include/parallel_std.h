// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include <algorithm>
#include <numeric>
#include <execution>
#include <vector>
#include <thread>

// Simplified parallel execution using C++ standard library
// This works on ALL platforms with C++17 support, including ARM64

namespace diskann {
namespace parallel {

// Get hardware thread count
inline unsigned int get_num_threads() {
    return std::thread::hardware_concurrency();
}

// Parallel for using std::for_each with parallel execution
template<typename IndexType, typename Func>
void parallel_for(IndexType start, IndexType end, Func fn, int num_threads = -1) {
    if (start >= end) return;
    
    // Create index sequence
    std::vector<IndexType> indices(end - start);
    std::iota(indices.begin(), indices.end(), start);
    
    // Execute in parallel
    std::for_each(std::execution::par_unseq,
                  indices.begin(), indices.end(),
                  fn);
}

// Parallel for with dynamic scheduling (chunked)
template<typename IndexType, typename Func>
void parallel_for_dynamic(IndexType start, IndexType end, Func fn, 
                         IndexType chunk_size = 1, int num_threads = -1) {
    if (start >= end) return;
    
    // Create chunks
    std::vector<std::pair<IndexType, IndexType>> chunks;
    for (IndexType i = start; i < end; i += chunk_size) {
        chunks.emplace_back(i, std::min(i + chunk_size, end));
    }
    
    // Process chunks in parallel
    std::for_each(std::execution::par,
                  chunks.begin(), chunks.end(),
                  [&fn](const auto& chunk) {
                      for (IndexType i = chunk.first; i < chunk.second; ++i) {
                          fn(i);
                      }
                  });
}

// Parallel transform
template<typename InputIt, typename OutputIt, typename UnaryOp>
void parallel_transform(InputIt first, InputIt last, OutputIt d_first, UnaryOp op) {
    std::transform(std::execution::par_unseq, first, last, d_first, op);
}

// Parallel reduce
template<typename InputIt, typename T, typename BinaryOp>
T parallel_reduce(InputIt first, InputIt last, T init, BinaryOp op) {
    return std::reduce(std::execution::par_unseq, first, last, init, op);
}

// Parallel sort
template<typename RandomIt>
void parallel_sort(RandomIt first, RandomIt last) {
    std::sort(std::execution::par_unseq, first, last);
}

// Parallel find
template<typename InputIt, typename T>
InputIt parallel_find(InputIt first, InputIt last, const T& value) {
    return std::find(std::execution::par_unseq, first, last, value);
}

} // namespace parallel
} // namespace diskann

// Convenience macros for easy migration
#define DISKANN_PARALLEL_FOR(start, end) \
    diskann::parallel::parallel_for(start, end, [&](auto i)

#define DISKANN_PARALLEL_FOR_DYNAMIC(start, end, chunk) \
    diskann::parallel::parallel_for_dynamic(start, end, [&](auto i), chunk

#define DISKANN_END_PARALLEL )