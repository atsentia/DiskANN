// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include <thread>
#include <vector>
#include <functional>
#include <atomic>
#include <algorithm>
#include <future>

#ifdef _OPENMP
#include <omp.h>
#endif

// Include our enhanced parallel execution support
#include "diskann_parallel.h"

namespace diskann
{

// Cross-platform thread count detection
inline uint32_t get_num_threads()
{
#ifdef _OPENMP
    return static_cast<uint32_t>(omp_get_num_procs());
#else
    uint32_t num_threads = std::thread::hardware_concurrency();
    return num_threads > 0 ? num_threads : 1;
#endif
}

// Set number of threads for parallel operations
inline void set_num_threads(uint32_t num_threads)
{
#ifdef _OPENMP
    omp_set_num_threads(static_cast<int>(num_threads));
#else
    // Store in thread-local storage for use by parallel_for
    static thread_local uint32_t tl_num_threads = num_threads;
    tl_num_threads = num_threads;
#endif
}

// Parallel for loop implementation
template <typename IndexType, typename Function>
inline void parallel_for(IndexType start, IndexType end, Function fn, int num_threads = -1)
{
#ifdef _OPENMP
    #pragma omp parallel for num_threads(num_threads)
    for (IndexType i = start; i < end; ++i)
    {
        fn(i);
    }
#else
    // Standard C++ thread-based implementation
    if (num_threads <= 0)
    {
        num_threads = get_num_threads();
    }
    
    const IndexType total_work = end - start;
    if (total_work <= 0) return;
    
    // For small workloads, run sequentially
    if (total_work < static_cast<IndexType>(num_threads) || num_threads == 1)
    {
        for (IndexType i = start; i < end; ++i)
        {
            fn(i);
        }
        return;
    }
    
    // Divide work among threads
    std::vector<std::thread> threads;
    threads.reserve(num_threads);
    
    const IndexType chunk_size = total_work / num_threads;
    const IndexType remainder = total_work % num_threads;
    
    IndexType current_start = start;
    
    for (int t = 0; t < num_threads; ++t)
    {
        IndexType current_end = current_start + chunk_size;
        if (t < remainder)
        {
            current_end += 1;
        }
        
        threads.emplace_back([current_start, current_end, &fn]() {
            for (IndexType i = current_start; i < current_end; ++i)
            {
                fn(i);
            }
        });
        
        current_start = current_end;
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads)
    {
        thread.join();
    }
#endif
}

// Parallel for loop with dynamic scheduling
template <typename IndexType, typename Function>
inline void parallel_for_dynamic(IndexType start, IndexType end, Function fn, 
                                IndexType chunk_size = 1, int num_threads = -1)
{
#ifdef _OPENMP
    #pragma omp parallel for schedule(dynamic, chunk_size) num_threads(num_threads)
    for (IndexType i = start; i < end; ++i)
    {
        fn(i);
    }
#else
    // Standard C++ implementation with work stealing
    if (num_threads <= 0)
    {
        num_threads = get_num_threads();
    }
    
    const IndexType total_work = end - start;
    if (total_work <= 0) return;
    
    // For small workloads, run sequentially
    if (total_work < static_cast<IndexType>(num_threads) || num_threads == 1)
    {
        for (IndexType i = start; i < end; ++i)
        {
            fn(i);
        }
        return;
    }
    
    // Use atomic counter for dynamic work distribution
    std::atomic<IndexType> next_work(start);
    std::vector<std::thread> threads;
    threads.reserve(num_threads);
    
    for (int t = 0; t < num_threads; ++t)
    {
        threads.emplace_back([&next_work, end, chunk_size, &fn]() {
            while (true)
            {
                IndexType work_start = next_work.fetch_add(chunk_size);
                if (work_start >= end) break;
                
                IndexType work_end = std::min(work_start + chunk_size, end);
                for (IndexType i = work_start; i < work_end; ++i)
                {
                    fn(i);
                }
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads)
    {
        thread.join();
    }
#endif
}

// Parallel reduce implementation
template <typename IndexType, typename T, typename ReduceOp, typename TransformOp>
inline T parallel_reduce(IndexType start, IndexType end, T identity, 
                        ReduceOp reduce_op, TransformOp transform_op, 
                        int num_threads = -1)
{
#ifdef _OPENMP
    T result = identity;
    #pragma omp parallel for reduction(+:result) num_threads(num_threads)
    for (IndexType i = start; i < end; ++i)
    {
        result = reduce_op(result, transform_op(i));
    }
    return result;
#else
    if (num_threads <= 0)
    {
        num_threads = get_num_threads();
    }
    
    const IndexType total_work = end - start;
    if (total_work <= 0) return identity;
    
    // For small workloads, run sequentially
    if (total_work < static_cast<IndexType>(num_threads) || num_threads == 1)
    {
        T result = identity;
        for (IndexType i = start; i < end; ++i)
        {
            result = reduce_op(result, transform_op(i));
        }
        return result;
    }
    
    // Parallel reduction using futures
    std::vector<std::future<T>> futures;
    futures.reserve(num_threads);
    
    const IndexType chunk_size = total_work / num_threads;
    const IndexType remainder = total_work % num_threads;
    
    IndexType current_start = start;
    
    for (int t = 0; t < num_threads; ++t)
    {
        IndexType current_end = current_start + chunk_size;
        if (t < remainder)
        {
            current_end += 1;
        }
        
        futures.push_back(std::async(std::launch::async, 
            [current_start, current_end, identity, &reduce_op, &transform_op]() {
                T local_result = identity;
                for (IndexType i = current_start; i < current_end; ++i)
                {
                    local_result = reduce_op(local_result, transform_op(i));
                }
                return local_result;
            }));
        
        current_start = current_end;
    }
    
    // Combine results
    T final_result = identity;
    for (auto& future : futures)
    {
        final_result = reduce_op(final_result, future.get());
    }
    
    return final_result;
#endif
}

// Get current thread ID (for debugging and thread-local storage)
inline int get_thread_id()
{
#ifdef _OPENMP
    return omp_get_thread_num();
#else
    static std::atomic<int> next_id(0);
    static thread_local int thread_id = next_id.fetch_add(1);
    return thread_id;
#endif
}

// Check if we're in a parallel region
inline bool in_parallel()
{
#ifdef _OPENMP
    return omp_in_parallel() != 0;
#else
    // Without OpenMP, we're never technically "in parallel" 
    // in the OpenMP sense
    return false;
#endif
}

} // namespace diskann

// The OpenMP compatibility macros are now provided by diskann_parallel.h
// which is included above. This prevents duplicate macro definitions.