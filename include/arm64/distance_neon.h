#pragma once

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif

#include <cmath>
#include <stddef.h>

namespace diskann {
namespace neon {

/**
 * ARM64 NEON optimized distance functions for DiskANN
 * 
 * These functions provide SIMD-accelerated distance calculations
 * specifically optimized for ARM64 architecture with NEON support.
 */

#ifdef __ARM_NEON

/**
 * NEON-optimized L2 (Euclidean) distance calculation
 * Processes 4 floats at a time using NEON SIMD instructions
 * 
 * @param a First vector
 * @param b Second vector  
 * @param dim Dimension of vectors (should be > 0)
 * @return L2 distance between vectors
 */
inline float l2_distance_neon(const float* a, const float* b, size_t dim) {
    float32x4_t sum_vec = vdupq_n_f32(0.0f);
    size_t i = 0;
    
    // Process 4 elements at a time
    for (; i + 4 <= dim; i += 4) {
        float32x4_t a_vec = vld1q_f32(&a[i]);
        float32x4_t b_vec = vld1q_f32(&b[i]);
        float32x4_t diff = vsubq_f32(a_vec, b_vec);
        sum_vec = vfmaq_f32(sum_vec, diff, diff);  // Fused multiply-add
    }
    
    // Reduce the vector to a single sum
    float sum = vaddvq_f32(sum_vec);
    
    // Handle remaining elements
    for (; i < dim; i++) {
        float diff = a[i] - b[i];
        sum += diff * diff;
    }
    
    return std::sqrt(sum);
}

/**
 * NEON-optimized squared L2 distance (avoids sqrt for performance)
 * Useful when only relative distances matter
 */
inline float l2_distance_squared_neon(const float* a, const float* b, size_t dim) {
    float32x4_t sum_vec = vdupq_n_f32(0.0f);
    size_t i = 0;
    
    for (; i + 4 <= dim; i += 4) {
        float32x4_t a_vec = vld1q_f32(&a[i]);
        float32x4_t b_vec = vld1q_f32(&b[i]);
        float32x4_t diff = vsubq_f32(a_vec, b_vec);
        sum_vec = vfmaq_f32(sum_vec, diff, diff);
    }
    
    float sum = vaddvq_f32(sum_vec);
    
    for (; i < dim; i++) {
        float diff = a[i] - b[i];
        sum += diff * diff;
    }
    
    return sum;
}

/**
 * NEON-optimized dot product calculation
 * Used for cosine similarity and inner product distances
 */
inline float dot_product_neon(const float* a, const float* b, size_t dim) {
    float32x4_t sum_vec = vdupq_n_f32(0.0f);
    size_t i = 0;
    
    for (; i + 4 <= dim; i += 4) {
        float32x4_t a_vec = vld1q_f32(&a[i]);
        float32x4_t b_vec = vld1q_f32(&b[i]);
        sum_vec = vfmaq_f32(sum_vec, a_vec, b_vec);
    }
    
    float sum = vaddvq_f32(sum_vec);
    
    for (; i < dim; i++) {
        sum += a[i] * b[i];
    }
    
    return sum;
}

/**
 * NEON-optimized vector norm calculation
 * Used for normalizing vectors in cosine similarity
 */
inline float vector_norm_neon(const float* vec, size_t dim) {
    float32x4_t sum_vec = vdupq_n_f32(0.0f);
    size_t i = 0;
    
    for (; i + 4 <= dim; i += 4) {
        float32x4_t v = vld1q_f32(&vec[i]);
        sum_vec = vfmaq_f32(sum_vec, v, v);
    }
    
    float sum = vaddvq_f32(sum_vec);
    
    for (; i < dim; i++) {
        sum += vec[i] * vec[i];
    }
    
    return std::sqrt(sum);
}

/**
 * NEON-optimized cosine distance calculation
 * cosine_distance = 1 - (dot_product / (norm_a * norm_b))
 */
inline float cosine_distance_neon(const float* a, const float* b, size_t dim) {
    float dot_product = dot_product_neon(a, b, dim);
    float norm_a = vector_norm_neon(a, dim);
    float norm_b = vector_norm_neon(b, dim);
    
    if (norm_a == 0.0f || norm_b == 0.0f) {
        return 1.0f;  // Maximum distance for zero vectors
    }
    
    float cosine_sim = dot_product / (norm_a * norm_b);
    return 1.0f - cosine_sim;
}

/**
 * NEON-optimized inner product distance
 * inner_product_distance = -dot_product (negative for maximum inner product search)
 */
inline float inner_product_distance_neon(const float* a, const float* b, size_t dim) {
    return -dot_product_neon(a, b, dim);
}

/**
 * Batch distance calculation using NEON
 * Computes distances from query to multiple points simultaneously
 */
inline void batch_l2_distances_neon(const float* query, const float* points, 
                                   float* distances, size_t num_points, size_t dim) {
    for (size_t i = 0; i < num_points; i++) {
        distances[i] = l2_distance_neon(query, &points[i * dim], dim);
    }
}

/**
 * NEON-optimized vector addition
 * Useful for centroid updates in clustering algorithms
 */
inline void vector_add_neon(const float* a, const float* b, float* result, size_t dim) {
    size_t i = 0;
    
    for (; i + 4 <= dim; i += 4) {
        float32x4_t a_vec = vld1q_f32(&a[i]);
        float32x4_t b_vec = vld1q_f32(&b[i]);
        float32x4_t sum_vec = vaddq_f32(a_vec, b_vec);
        vst1q_f32(&result[i], sum_vec);
    }
    
    for (; i < dim; i++) {
        result[i] = a[i] + b[i];
    }
}

/**
 * NEON-optimized vector scaling
 * Multiplies vector by a scalar value
 */
inline void vector_scale_neon(const float* vec, float scale, float* result, size_t dim) {
    float32x4_t scale_vec = vdupq_n_f32(scale);
    size_t i = 0;
    
    for (; i + 4 <= dim; i += 4) {
        float32x4_t v = vld1q_f32(&vec[i]);
        float32x4_t scaled = vmulq_f32(v, scale_vec);
        vst1q_f32(&result[i], scaled);
    }
    
    for (; i < dim; i++) {
        result[i] = vec[i] * scale;
    }
}

/**
 * NEON-optimized vector normalization
 * Normalizes vector to unit length
 */
inline void vector_normalize_neon(const float* vec, float* result, size_t dim) {
    float norm = vector_norm_neon(vec, dim);
    if (norm > 0.0f) {
        vector_scale_neon(vec, 1.0f / norm, result, dim);
    } else {
        // Handle zero vector case
        for (size_t i = 0; i < dim; i++) {
            result[i] = 0.0f;
        }
    }
}

#else // !__ARM_NEON

// Fallback to scalar implementations when NEON is not available
inline float l2_distance_neon(const float* a, const float* b, size_t dim) {
    float sum = 0.0f;
    for (size_t i = 0; i < dim; i++) {
        float diff = a[i] - b[i];
        sum += diff * diff;
    }
    return std::sqrt(sum);
}

inline float l2_distance_squared_neon(const float* a, const float* b, size_t dim) {
    float sum = 0.0f;
    for (size_t i = 0; i < dim; i++) {
        float diff = a[i] - b[i];
        sum += diff * diff;
    }
    return sum;
}

inline float dot_product_neon(const float* a, const float* b, size_t dim) {
    float sum = 0.0f;
    for (size_t i = 0; i < dim; i++) {
        sum += a[i] * b[i];
    }
    return sum;
}

inline float vector_norm_neon(const float* vec, size_t dim) {
    float sum = 0.0f;
    for (size_t i = 0; i < dim; i++) {
        sum += vec[i] * vec[i];
    }
    return std::sqrt(sum);
}

inline float cosine_distance_neon(const float* a, const float* b, size_t dim) {
    float dot_product = dot_product_neon(a, b, dim);
    float norm_a = vector_norm_neon(a, dim);
    float norm_b = vector_norm_neon(b, dim);
    
    if (norm_a == 0.0f || norm_b == 0.0f) {
        return 1.0f;
    }
    
    float cosine_sim = dot_product / (norm_a * norm_b);
    return 1.0f - cosine_sim;
}

inline float inner_product_distance_neon(const float* a, const float* b, size_t dim) {
    return -dot_product_neon(a, b, dim);
}

inline void batch_l2_distances_neon(const float* query, const float* points, 
                                   float* distances, size_t num_points, size_t dim) {
    for (size_t i = 0; i < num_points; i++) {
        distances[i] = l2_distance_neon(query, &points[i * dim], dim);
    }
}

inline void vector_add_neon(const float* a, const float* b, float* result, size_t dim) {
    for (size_t i = 0; i < dim; i++) {
        result[i] = a[i] + b[i];
    }
}

inline void vector_scale_neon(const float* vec, float scale, float* result, size_t dim) {
    for (size_t i = 0; i < dim; i++) {
        result[i] = vec[i] * scale;
    }
}

inline void vector_normalize_neon(const float* vec, float* result, size_t dim) {
    float norm = vector_norm_neon(vec, dim);
    if (norm > 0.0f) {
        vector_scale_neon(vec, 1.0f / norm, result, dim);
    } else {
        for (size_t i = 0; i < dim; i++) {
            result[i] = 0.0f;
        }
    }
}

#endif // __ARM_NEON

} // namespace neon
} // namespace diskann