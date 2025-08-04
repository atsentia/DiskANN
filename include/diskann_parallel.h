// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include <algorithm>
#include <numeric>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <future>
#include <atomic>

// This header provides parallel execution alternatives when OpenMP is not available
// Priority order: OpenMP > Parallel STL > Thread Pool > Sequential

// Check if we need this header (only when OpenMP is not available)
#ifdef _OPENMP
    // OpenMP is available - this header does nothing
    // All the original #pragma omp directives will work as before
#else
    // OpenMP is NOT available - provide alternatives

// Check for C++17 parallel algorithms support
// Note: Having <execution> doesn't guarantee implementation
#if __has_include(<execution>) && defined(__cpp_lib_parallel_algorithm) && __cpp_lib_parallel_algorithm >= 201603L
    #include <execution>
    #define HAS_WORKING_PARALLEL_STL 1
#endif

// DiskANN parallel execution using standard C++ 
// Falls back to thread pool or sequential based on availability

namespace diskann {

// Simple thread pool for basic parallelism when Parallel STL not available
class ThreadPool {
private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable condition;
    std::atomic<bool> stop{false};
    
public:
    ThreadPool(size_t num_threads = 0) {
        if (num_threads == 0) {
            num_threads = std::thread::hardware_concurrency();
            if (num_threads == 0) num_threads = 1;
        }
        
        for(size_t i = 0; i < num_threads; ++i) {
            workers.emplace_back([this] {
                while(true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(queue_mutex);
                        condition.wait(lock, [this]{ return stop || !tasks.empty(); });
                        if(stop && tasks.empty()) return;
                        task = std::move(tasks.front());
                        tasks.pop();
                    }
                    task();
                }
            });
        }
    }
    
    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        for(auto& worker : workers) {
            worker.join();
        }
    }
    
    template<typename Func>
    auto enqueue(Func&& f) -> std::future<typename std::result_of<Func()>::type> {
        using return_type = typename std::result_of<Func()>::type;
        
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::forward<Func>(f)
        );
        
        std::future<return_type> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            tasks.emplace([task](){ (*task)(); });
        }
        condition.notify_one();
        return res;
    }
    
    size_t num_threads() const { return workers.size(); }
};

// Global thread pool (lazy initialized)
inline ThreadPool& get_thread_pool() {
    static ThreadPool pool;
    return pool;
}

// Get hardware thread count (replaces omp_get_num_procs)
inline unsigned int get_num_threads() {
    unsigned int n = std::thread::hardware_concurrency();
    return n > 0 ? n : 1;
}

// Thread-local storage for thread count (mimics omp_set_num_threads behavior)
inline unsigned int& get_active_threads() {
    static thread_local unsigned int active_threads = get_num_threads();
    return active_threads;
}

// Set number of threads (for compatibility)
inline void set_num_threads(unsigned int n) {
    get_active_threads() = n;
}

// Get thread ID (for compatibility with omp_get_thread_num)
inline int get_thread_id() {
    static std::atomic<int> next_id(0);
    static thread_local int thread_id = next_id.fetch_add(1);
    return thread_id;
}

// Main parallel for implementation
template<typename IndexType, typename Func>
void parallel_for(IndexType start, IndexType end, Func fn) {
    if (start >= end) return;
    
#ifdef HAS_WORKING_PARALLEL_STL
    // Option 1: Use C++17 Parallel STL
    std::vector<IndexType> indices(end - start);
    std::iota(indices.begin(), indices.end(), start);
    
    std::for_each(std::execution::par_unseq,
                  indices.begin(), indices.end(),
                  fn);
#else
    // Option 2: Use thread pool for basic parallelism
    const IndexType range = end - start;
    const size_t num_threads = get_thread_pool().num_threads();
    
    // Only parallelize if worthwhile (>1000 items per thread)
    if (range > 1000 && num_threads > 1) {
        const IndexType chunk_size = (range + num_threads - 1) / num_threads;
        std::vector<std::future<void>> futures;
        
        for (size_t t = 0; t < num_threads; ++t) {
            IndexType chunk_start = start + t * chunk_size;
            IndexType chunk_end = std::min(chunk_start + chunk_size, end);
            
            if (chunk_start < chunk_end) {
                futures.push_back(
                    get_thread_pool().enqueue([chunk_start, chunk_end, fn]() {
                        for (IndexType i = chunk_start; i < chunk_end; ++i) {
                            fn(i);
                        }
                    })
                );
            }
        }
        
        // Wait for all tasks to complete
        for (auto& future : futures) {
            future.wait();
        }
    } else {
        // Option 3: Sequential fallback for small ranges
        for (IndexType i = start; i < end; ++i) {
            fn(i);
        }
    }
#endif
}

// Parallel for with static scheduling (chunked for better cache usage)
template<typename IndexType, typename Func>
void parallel_for_static(IndexType start, IndexType end, Func fn, 
                        IndexType chunk_size = 0) {
    if (start >= end) return;
    
    if (chunk_size == 0) {
        // Auto chunk size for cache efficiency
        chunk_size = std::max<IndexType>(1, (end - start) / (get_active_threads() * 4));
    }
    
#ifdef HAS_WORKING_PARALLEL_STL
    // Create chunks for better cache locality
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
#else
    // Use our parallel_for with explicit chunks
    const IndexType range = end - start;
    const size_t num_threads = get_thread_pool().num_threads();
    
    if (range > chunk_size && num_threads > 1) {
        std::vector<std::future<void>> futures;
        
        for (IndexType i = start; i < end; i += chunk_size) {
            IndexType chunk_end = std::min(i + chunk_size, end);
            
            futures.push_back(
                get_thread_pool().enqueue([i, chunk_end, fn]() {
                    for (IndexType j = i; j < chunk_end; ++j) {
                        fn(j);
                    }
                })
            );
        }
        
        for (auto& future : futures) {
            future.wait();
        }
    } else {
        // Sequential fallback
        for (IndexType i = start; i < end; ++i) {
            fn(i);
        }
    }
#endif
}

// Parallel for with dynamic scheduling (work stealing)
template<typename IndexType, typename Func>
void parallel_for_dynamic(IndexType start, IndexType end, Func fn, 
                         IndexType chunk_size = 1) {
    if (start >= end) return;
    
#ifdef HAS_WORKING_PARALLEL_STL
    // For dynamic scheduling, we use smaller chunks
    std::vector<IndexType> indices;
    indices.reserve((end - start) / chunk_size + 1);
    
    for (IndexType i = start; i < end; i += chunk_size) {
        indices.push_back(i);
    }
    
    // Process with parallel execution (runtime handles work stealing)
    std::for_each(std::execution::par,
                  indices.begin(), indices.end(),
                  [&fn, end, chunk_size](IndexType chunk_start) {
                      IndexType chunk_end = std::min(chunk_start + chunk_size, end);
                      for (IndexType i = chunk_start; i < chunk_end; ++i) {
                          fn(i);
                      }
                  });
#else
    // Dynamic scheduling with thread pool
    std::atomic<IndexType> next_chunk(start);
    const size_t num_threads = get_thread_pool().num_threads();
    
    if ((end - start) > chunk_size && num_threads > 1) {
        std::vector<std::future<void>> futures;
        
        for (size_t t = 0; t < num_threads; ++t) {
            futures.push_back(
                get_thread_pool().enqueue([&next_chunk, end, chunk_size, fn]() {
                    while (true) {
                        IndexType chunk_start = next_chunk.fetch_add(chunk_size);
                        if (chunk_start >= end) break;
                        
                        IndexType chunk_end = std::min(chunk_start + chunk_size, end);
                        for (IndexType i = chunk_start; i < chunk_end; ++i) {
                            fn(i);
                        }
                    }
                })
            );
        }
        
        for (auto& future : futures) {
            future.wait();
        }
    } else {
        // Sequential fallback
        for (IndexType i = start; i < end; ++i) {
            fn(i);
        }
    }
#endif
}

// Parallel reduce operation
template<typename InputIt, typename T, typename BinaryOp>
T parallel_reduce(InputIt first, InputIt last, T init, BinaryOp op) {
#ifdef HAS_WORKING_PARALLEL_STL
    return std::reduce(std::execution::par_unseq, first, last, init, op);
#else
    // Thread pool based parallel reduce
    auto distance = std::distance(first, last);
    const size_t num_threads = get_thread_pool().num_threads();
    
    if (distance > 1000 && num_threads > 1) {
        const auto chunk_size = distance / num_threads;
        std::vector<std::future<T>> futures;
        
        auto it = first;
        for (size_t t = 0; t < num_threads; ++t) {
            auto chunk_begin = it;
            auto chunk_end = (t == num_threads - 1) ? last : 
                           std::next(it, chunk_size);
            
            futures.push_back(
                get_thread_pool().enqueue([chunk_begin, chunk_end, init, op]() -> T {
                    return std::accumulate(chunk_begin, chunk_end, init, op);
                })
            );
            
            it = chunk_end;
        }
        
        // Combine results
        T result = init;
        for (auto& future : futures) {
            result = op(result, future.get());
        }
        return result;
    } else {
        // Sequential fallback
        return std::accumulate(first, last, init, op);
    }
#endif
}

// Transform reduce (map-reduce pattern)
template<typename InputIt, typename T, typename UnaryOp, typename BinaryOp>
T parallel_transform_reduce(InputIt first, InputIt last, T init,
                           BinaryOp reduce_op, UnaryOp transform_op) {
#ifdef HAS_WORKING_PARALLEL_STL
    return std::transform_reduce(std::execution::par_unseq, 
                                first, last, init, reduce_op, transform_op);
#else
    // Parallel transform reduce using thread pool
    auto distance = std::distance(first, last);
    const size_t num_threads = get_thread_pool().num_threads();
    
    if (distance > 1000 && num_threads > 1) {
        const auto chunk_size = distance / num_threads;
        std::vector<std::future<T>> futures;
        
        auto it = first;
        for (size_t t = 0; t < num_threads; ++t) {
            auto chunk_begin = it;
            auto chunk_end = (t == num_threads - 1) ? last : 
                           std::next(it, chunk_size);
            
            futures.push_back(
                get_thread_pool().enqueue([chunk_begin, chunk_end, init, reduce_op, transform_op]() -> T {
                    T result = init;
                    for (auto it = chunk_begin; it != chunk_end; ++it) {
                        result = reduce_op(result, transform_op(*it));
                    }
                    return result;
                })
            );
            
            it = chunk_end;
        }
        
        // Combine results
        T result = init;
        for (auto& future : futures) {
            result = reduce_op(result, future.get());
        }
        return result;
    } else {
        // Sequential fallback
        T result = init;
        for (auto it = first; it != last; ++it) {
            result = reduce_op(result, transform_op(*it));
        }
        return result;
    }
#endif
}

// Parallel sort
template<typename RandomIt>
void parallel_sort(RandomIt first, RandomIt last) {
#ifdef HAS_WORKING_PARALLEL_STL
    std::sort(std::execution::par_unseq, first, last);
#else
    // For now, use sequential sort
    // TODO: Could implement parallel merge sort with thread pool
    std::sort(first, last);
#endif
}

// Parallel partial sort (for top-k operations common in DiskANN)
template<typename RandomIt>
void parallel_partial_sort(RandomIt first, RandomIt middle, RandomIt last) {
#ifdef HAS_WORKING_PARALLEL_STL
    std::partial_sort(std::execution::par, first, middle, last);
#else
    // Sequential partial sort is often fast enough for top-k
    std::partial_sort(first, middle, last);
#endif
}

} // namespace diskann

// Function replacements when OpenMP not available
#define omp_get_num_procs() diskann::get_num_threads()
#define omp_set_num_threads(n) diskann::set_num_threads(n)
#define omp_get_thread_num() 0  // Single thread ID in sequential mode
#define omp_get_num_threads() 1
#define omp_in_parallel() 0

// Transform OpenMP pragmas to our parallel execution
// This allows gradual migration of existing code

// For simple parallel for loops
#define PRAGMA_OMP_PARALLEL_FOR(start, end) \
    diskann::parallel_for(start, end, [&](auto i)

// For parallel for with static scheduling
#define PRAGMA_OMP_PARALLEL_FOR_STATIC(start, end, chunk) \
    diskann::parallel_for_static(start, end, [&](auto i), chunk

// For parallel for with dynamic scheduling  
#define PRAGMA_OMP_PARALLEL_FOR_DYNAMIC(start, end, chunk) \
    diskann::parallel_for_dynamic(start, end, [&](auto i), chunk

#define END_PRAGMA_OMP )

#endif // !_OPENMP (outer check - only provide alternatives when OpenMP not available)