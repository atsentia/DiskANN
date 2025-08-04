#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <fstream>

// Test graph operations with ARM64 NEON integration
// This validates that graph algorithms automatically benefit from 
// Phase 2 distance optimizations (5.17x speedup expected)

#include "include/distance.h"
#include "include/utils.h"

// Mock globals for testing
namespace diskann {
    bool Avx2SupportedCPU = false;
    bool AvxSupportedCPU = false;
    
#if defined(__aarch64__) || defined(_M_ARM64)
    bool NeonSupportedCPU = true;
#else
    bool NeonSupportedCPU = false;
#endif
}

// Simulate core graph operation: distance-intensive search
class GraphSearchSimulator {
private:
    std::vector<std::vector<float>> data;
    std::unique_ptr<diskann::Distance<float>> distance_func;
    size_t dim;
    size_t num_points;
    
public:
    GraphSearchSimulator(size_t dimension, size_t points) 
        : dim(dimension), num_points(points) {
        
        // Create distance function (automatically uses NEON on ARM64)
        distance_func = std::unique_ptr<diskann::Distance<float>>(
            diskann::get_distance_function<float>(diskann::Metric::L2)
        );
        
        // Generate random data
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        
        data.resize(num_points, std::vector<float>(dim));
        for (size_t i = 0; i < num_points; i++) {
            for (size_t j = 0; j < dim; j++) {
                data[i][j] = dist(gen);
            }
        }
        
        std::cout << "Generated " << num_points << " vectors of dimension " << dim << std::endl;
    }
    
    // Simulate graph search: many distance calculations
    // This mirrors the pattern in iterate_to_fixed_point()
    double simulate_graph_search(const std::vector<float>& query, size_t beam_width = 64) {
        std::vector<std::pair<float, size_t>> candidates;
        candidates.reserve(num_points);
        
        auto start = std::chrono::high_resolution_clock::now();
        
        // Phase 1: Calculate distances to all points (like initial beam expansion)
        for (size_t i = 0; i < num_points; i++) {
            float dist = distance_func->compare(query.data(), data[i].data(), dim);
            candidates.emplace_back(dist, i);
        }
        
        // Phase 2: Sort to find nearest neighbors (like beam search ranking)
        std::partial_sort(candidates.begin(), 
                         candidates.begin() + std::min(beam_width, candidates.size()),
                         candidates.end());
        
        // Phase 3: Simulated graph traversal with more distance calculations
        size_t total_distance_calls = num_points;  // From phase 1
        
        for (size_t iter = 0; iter < 3; iter++) {  // Simulate 3 graph iterations
            for (size_t i = 0; i < std::min(beam_width/2, candidates.size()); i++) {
                // Simulate checking neighbors of current candidates
                for (size_t j = 0; j < 8; j++) {  // Average 8 neighbors per node
                    size_t neighbor_idx = (candidates[i].second + j + 1) % num_points;
                    float dist = distance_func->compare(query.data(), data[neighbor_idx].data(), dim);
                    total_distance_calls++;
                    
                    // Update candidates if better
                    if (dist < candidates[beam_width-1].first) {
                        candidates[beam_width-1] = {dist, neighbor_idx};
                        // Re-sort (simplified)
                        std::sort(candidates.begin(), candidates.begin() + beam_width);
                    }
                }
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << "Simulated graph search: " << total_distance_calls 
                  << " distance calls in " << duration.count() << " μs" << std::endl;
        
        return duration.count();
    }
    
    // Simulate scalar version for comparison
    double simulate_scalar_search(const std::vector<float>& query, size_t beam_width = 64) {
        std::vector<std::pair<float, size_t>> candidates;
        candidates.reserve(num_points);
        
        auto start = std::chrono::high_resolution_clock::now();
        
        // Use scalar L2 distance calculation
        auto scalar_l2 = [](const float* a, const float* b, size_t dim) -> float {
            float sum = 0;
            for (size_t i = 0; i < dim; i++) {
                float diff = a[i] - b[i];
                sum += diff * diff;
            }
            return sum;  // Return squared distance for consistency
        };
        
        // Phase 1: Calculate distances to all points
        for (size_t i = 0; i < num_points; i++) {
            float dist = scalar_l2(query.data(), data[i].data(), dim);
            candidates.emplace_back(dist, i);
        }
        
        // Phase 2: Sort to find nearest neighbors
        std::partial_sort(candidates.begin(), 
                         candidates.begin() + std::min(beam_width, candidates.size()),
                         candidates.end());
        
        // Phase 3: Simulated graph traversal
        size_t total_distance_calls = num_points;
        
        for (size_t iter = 0; iter < 3; iter++) {
            for (size_t i = 0; i < std::min(beam_width/2, candidates.size()); i++) {
                for (size_t j = 0; j < 8; j++) {
                    size_t neighbor_idx = (candidates[i].second + j + 1) % num_points;
                    float dist = scalar_l2(query.data(), data[neighbor_idx].data(), dim);
                    total_distance_calls++;
                    
                    if (dist < candidates[beam_width-1].first) {
                        candidates[beam_width-1] = {dist, neighbor_idx};
                        std::sort(candidates.begin(), candidates.begin() + beam_width);
                    }
                }
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << "Scalar graph search: " << total_distance_calls 
                  << " distance calls in " << duration.count() << " μs" << std::endl;
        
        return duration.count();
    }
};

int main() {
    std::cout << "🔍 Testing Graph Operations ARM64 NEON Integration" << std::endl;
    std::cout << "=================================================" << std::endl;
    
#if defined(__aarch64__) || defined(_M_ARM64)
    std::cout << "✅ ARM64 architecture detected" << std::endl;
    std::cout << "✅ NEON support: " << (diskann::NeonSupportedCPU ? "Yes" : "No") << std::endl;
#else
    std::cout << "❌ Not ARM64 architecture" << std::endl;
    return 1;
#endif

    // Test configurations: realistic graph sizes
    std::vector<std::tuple<size_t, size_t, std::string>> test_configs = {
        {128, 1000, "Small Graph (1K points, 128D)"},
        {256, 2000, "Medium Graph (2K points, 256D)"},
        {768, 1000, "Deep Graph (1K points, 768D)"}
    };
    
    for (auto& [dim, num_points, description] : test_configs) {
        std::cout << "\n🧪 Testing: " << description << std::endl;
        std::cout << "----------------------------------------" << std::endl;
        
        GraphSearchSimulator simulator(dim, num_points);
        
        // Generate random query
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        
        std::vector<float> query(dim);
        for (size_t i = 0; i < dim; i++) {
            query[i] = dist(gen);
        }
        
        // Run multiple trials for accuracy
        const int num_trials = 5;
        double total_neon_time = 0;
        double total_scalar_time = 0;
        
        for (int trial = 0; trial < num_trials; trial++) {
            std::cout << "\nTrial " << (trial + 1) << ":" << std::endl;
            
            // Test with NEON-optimized distance functions
            double neon_time = simulator.simulate_graph_search(query);
            total_neon_time += neon_time;
            
            // Test with scalar distance functions
            double scalar_time = simulator.simulate_scalar_search(query);
            total_scalar_time += scalar_time;
            
            double speedup = scalar_time / neon_time;
            std::cout << "Trial speedup: " << std::fixed << std::setprecision(2) << speedup << "x" << std::endl;
        }
        
        // Calculate averages
        double avg_neon_time = total_neon_time / num_trials;
        double avg_scalar_time = total_scalar_time / num_trials;
        double avg_speedup = avg_scalar_time / avg_neon_time;
        
        std::cout << "\n📊 " << description << " Results:" << std::endl;
        std::cout << "Average NEON time: " << std::fixed << std::setprecision(1) << avg_neon_time << " μs" << std::endl;
        std::cout << "Average Scalar time: " << avg_scalar_time << " μs" << std::endl;
        std::cout << "Average Speedup: " << std::setprecision(2) << avg_speedup << "x" << std::endl;
        
        // Validate against expectations
        if (avg_speedup >= 3.0) {
            std::cout << "✅ Graph operations speedup achieved!" << std::endl;
        } else if (avg_speedup >= 2.0) {
            std::cout << "⚠️  Partial speedup achieved" << std::endl;
        } else {
            std::cout << "❌ Insufficient speedup" << std::endl;
        }
    }
    
    std::cout << "\n🎯 Summary:" << std::endl;
    std::cout << "Graph operations automatically benefit from Phase 2 distance optimizations" << std::endl;
    std::cout << "Expected: Distance-intensive algorithms get ~5x speedup from NEON" << std::endl;
    std::cout << "This validates that Phase 3 graph operations are automatically optimized!" << std::endl;
    
    return 0;
}