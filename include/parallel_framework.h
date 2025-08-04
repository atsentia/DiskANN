// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include <thread>
#include <vector>
#include <functional>
#include <atomic>
#include <algorithm>
#include <future>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <memory>
#include <barrier>

#ifdef _OPENMP
#include <omp.h>
#else

namespace diskann {
namespace parallel {

// Thread pool for efficient thread reuse (like OpenMP's thread pool)
class ThreadPool {
private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable condition;
    std::atomic<bool> stop{false};
    std::atomic<int> active_threads{0};
    
    static ThreadPool& getInstance() {
        static thread_local ThreadPool instance(std::thread::hardware_concurrency());
        return instance;
    }
    
public:
    ThreadPool(size_t num_threads) {
        for(size_t i = 0; i < num_threads; ++i) {
            workers.emplace_back([this] {
                for(;;) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->condition.wait(lock, [this]{ 
                            return this->stop || !this->tasks.empty(); 
                        });
                        if(this->stop && this->tasks.empty())
                            return;
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                        active_threads++;
                    }
                    task();
                    active_threads--;
                }
            });
        }
    }
    
    template<class F>
    void enqueue(F&& f) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            if(stop)
                throw std::runtime_error("enqueue on stopped ThreadPool");
            tasks.emplace(std::forward<F>(f));
        }
        condition.notify_one();
    }
    
    void wait_all() {
        while(active_threads > 0 || !tasks.empty()) {
            std::this_thread::yield();
        }
    }
    
    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        for(std::thread &worker: workers)
            worker.join();
    }
    
    size_t size() const { return workers.size(); }
};

// Global thread count (mimics OpenMP's global state)
inline std::atomic<int>& get_global_num_threads() {
    static std::atomic<int> num_threads(std::thread::hardware_concurrency());
    return num_threads;
}

// Thread-local thread ID
inline int get_thread_num() {
    static std::atomic<int> next_id(0);
    static thread_local int thread_id = next_id.fetch_add(1);
    return thread_id;
}

// Critical section implementation
class critical_section {
private:
    static std::mutex& get_mutex() {
        static std::mutex mtx;
        return mtx;
    }
public:
    critical_section() { get_mutex().lock(); }
    ~critical_section() { get_mutex().unlock(); }
};

// Barrier implementation for thread synchronization
class barrier {
private:
    std::mutex mtx;
    std::condition_variable cv;
    std::atomic<int> count;
    std::atomic<int> generation{0};
    const int total;
    
public:
    explicit barrier(int num_threads) : count(num_threads), total(num_threads) {}
    
    void wait() {
        std::unique_lock<std::mutex> lock(mtx);
        auto gen = generation.load();
        
        if(--count == 0) {
            generation++;
            count = total;
            cv.notify_all();
        } else {
            cv.wait(lock, [this, gen] { return generation > gen; });
        }
    }
};

// Reduction variable template
template<typename T, typename Op = std::plus<T>>
class reduction_variable {
private:
    std::vector<T> thread_local_values;
    Op operation;
    T identity;
    
public:
    reduction_variable(T init_value, Op op = Op()) 
        : thread_local_values(get_global_num_threads(), init_value),
          operation(op), identity(init_value) {}
    
    T& local() {
        return thread_local_values[get_thread_num() % thread_local_values.size()];
    }
    
    T reduce() {
        T result = identity;
        for(const auto& val : thread_local_values) {
            result = operation(result, val);
        }
        return result;
    }
};

// Advanced parallel for with all OpenMP features
template<typename IndexType>
class parallel_for_executor {
private:
    IndexType start_, end_;
    int num_threads_;
    bool dynamic_schedule_;
    IndexType chunk_size_;
    std::atomic<IndexType> next_iteration_;
    
public:
    parallel_for_executor(IndexType start, IndexType end, int num_threads = -1,
                         bool dynamic = false, IndexType chunk = 1)
        : start_(start), end_(end), 
          num_threads_(num_threads > 0 ? num_threads : get_global_num_threads()),
          dynamic_schedule_(dynamic), chunk_size_(chunk),
          next_iteration_(start) {}
    
    template<typename Func>
    void execute(Func&& fn) {
        if(end_ <= start_) return;
        
        const IndexType total_work = end_ - start_;
        
        // Single thread execution for small workloads
        if(total_work < num_threads_ * 2 || num_threads_ == 1) {
            for(IndexType i = start_; i < end_; ++i) {
                fn(i);
            }
            return;
        }
        
        // Get or create thread pool
        auto& pool = ThreadPool::getInstance();
        std::vector<std::future<void>> futures;
        
        if(dynamic_schedule_) {
            // Dynamic scheduling with work stealing
            for(int t = 0; t < num_threads_; ++t) {
                auto task = [this, fn]() {
                    while(true) {
                        IndexType my_start = next_iteration_.fetch_add(chunk_size_);
                        if(my_start >= end_) break;
                        
                        IndexType my_end = std::min(my_start + chunk_size_, end_);
                        for(IndexType i = my_start; i < my_end; ++i) {
                            fn(i);
                        }
                    }
                };
                
                futures.push_back(std::async(std::launch::async, task));
            }
        } else {
            // Static scheduling
            const IndexType chunk_size = total_work / num_threads_;
            const IndexType remainder = total_work % num_threads_;
            
            IndexType current_start = start_;
            
            for(int t = 0; t < num_threads_; ++t) {
                IndexType current_end = current_start + chunk_size;
                if(t < remainder) {
                    current_end += 1;
                }
                
                auto task = [current_start, current_end, fn]() {
                    for(IndexType i = current_start; i < current_end; ++i) {
                        fn(i);
                    }
                };
                
                futures.push_back(std::async(std::launch::async, task));
                current_start = current_end;
            }
        }
        
        // Wait for all tasks to complete
        for(auto& future : futures) {
            future.wait();
        }
    }
};

// Parallel sections implementation
class parallel_sections {
private:
    std::vector<std::function<void()>> sections;
    
public:
    void add_section(std::function<void()> fn) {
        sections.push_back(fn);
    }
    
    void execute() {
        std::vector<std::future<void>> futures;
        for(auto& section : sections) {
            futures.push_back(std::async(std::launch::async, section));
        }
        for(auto& future : futures) {
            future.wait();
        }
    }
};

// Single execution (like #pragma omp single)
class single_executor {
private:
    static std::atomic<bool> executed;
    
public:
    template<typename Func>
    void execute(Func&& fn) {
        bool expected = false;
        if(executed.compare_exchange_strong(expected, true)) {
            fn();
        }
    }
    
    static void reset() { executed = false; }
};

std::atomic<bool> single_executor::executed{false};

} // namespace parallel
} // namespace diskann

// Convenience macros to match OpenMP syntax
#define DISKANN_PARALLEL_FOR(start, end) \
    diskann::parallel::parallel_for_executor<decltype(start)>(start, end).execute([&](auto i)

#define DISKANN_PARALLEL_FOR_DYNAMIC(start, end, chunk) \
    diskann::parallel::parallel_for_executor<decltype(start)>(start, end, -1, true, chunk).execute([&](auto i)

#define DISKANN_END_PARALLEL )

#define DISKANN_CRITICAL diskann::parallel::critical_section _critical_lock;

#define DISKANN_BARRIER diskann::parallel::barrier _barrier(diskann::parallel::get_global_num_threads()); \
                       _barrier.wait();

#define DISKANN_SINGLE diskann::parallel::single_executor _single; \
                      _single.execute([&]()

#define DISKANN_END_SINGLE );

// Function replacements
namespace diskann {
    inline uint32_t get_num_threads() {
        return parallel::get_global_num_threads();
    }
    
    inline void set_num_threads(uint32_t num_threads) {
        parallel::get_global_num_threads() = num_threads;
    }
    
    inline int get_thread_id() {
        return parallel::get_thread_num();
    }
    
    // Main parallel for implementation
    template<typename IndexType, typename Func>
    inline void parallel_for(IndexType start, IndexType end, Func fn, 
                            int num_threads = -1, bool dynamic = false, 
                            IndexType chunk_size = 1) {
        parallel::parallel_for_executor<IndexType>(start, end, num_threads, dynamic, chunk_size)
            .execute(fn);
    }
    
    // Parallel reduce with proper work distribution
    template<typename IndexType, typename T, typename TransformOp, typename ReduceOp>
    inline T parallel_reduce(IndexType start, IndexType end, T identity,
                           TransformOp transform_op, ReduceOp reduce_op,
                           int num_threads = -1) {
        if(num_threads <= 0) {
            num_threads = get_num_threads();
        }
        
        const IndexType total_work = end - start;
        if(total_work <= 0) return identity;
        
        // Use thread-local accumulation for efficiency
        std::vector<T> partial_results(num_threads, identity);
        
        parallel_for(start, end, [&](IndexType i) {
            int tid = parallel::get_thread_num() % num_threads;
            partial_results[tid] = reduce_op(partial_results[tid], transform_op(i));
        }, num_threads);
        
        // Final reduction
        T result = identity;
        for(const auto& partial : partial_results) {
            result = reduce_op(result, partial);
        }
        
        return result;
    }
}

// Replace OpenMP function calls when OpenMP is not available
#define omp_get_num_procs() diskann::get_num_threads()
#define omp_set_num_threads(n) diskann::set_num_threads(n)
#define omp_get_thread_num() diskann::get_thread_id()
#define omp_get_num_threads() diskann::get_num_threads()
#define omp_in_parallel() true

// Pragma replacements - these allow gradual migration
#define pragma_omp_parallel_for _Pragma("GCC diagnostic push") \
                               _Pragma("GCC diagnostic ignored \"-Wunknown-pragmas\"")
#define pragma_omp_critical DISKANN_CRITICAL
#define pragma_omp_barrier DISKANN_BARRIER
#define pragma_omp_single DISKANN_SINGLE

#endif // _OPENMP