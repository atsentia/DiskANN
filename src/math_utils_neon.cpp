// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#include "math_utils.h"
#include "diskann_parallel.h"  // Provides OpenMP alternatives when not available
#include <cmath>
#include <cstdlib>
#include <cstring>

#if defined(__aarch64__) || defined(_M_ARM64)
#include <arm_neon.h>
#include "arm64/distance_neon.h"

namespace diskann
{

// ARM64 NEON optimized L2 squared norm calculation
void compute_vecs_l2sq_neon(float *vecs_l2sq, float *data, const size_t num_points, const size_t dim)
{
    #pragma omp parallel for schedule(static, 8192)
    for (int64_t n_iter = 0; n_iter < (int64_t)num_points; n_iter++)
    {
        float *vec = data + (n_iter * dim);
        float sum = 0.0f;
        
        size_t d = 0;
        
        // Process 16 floats at a time using NEON
        for (; d + 15 < dim; d += 16)
        {
            float32x4_t v0 = vld1q_f32(vec + d);
            float32x4_t v1 = vld1q_f32(vec + d + 4);
            float32x4_t v2 = vld1q_f32(vec + d + 8);
            float32x4_t v3 = vld1q_f32(vec + d + 12);
            
            v0 = vmulq_f32(v0, v0);
            v1 = vmulq_f32(v1, v1);
            v2 = vmulq_f32(v2, v2);
            v3 = vmulq_f32(v3, v3);
            
            float32x4_t sum0 = vaddq_f32(v0, v1);
            float32x4_t sum1 = vaddq_f32(v2, v3);
            float32x4_t total = vaddq_f32(sum0, sum1);
            
            sum += vaddvq_f32(total);
        }
        
        // Process 4 floats at a time
        for (; d + 3 < dim; d += 4)
        {
            float32x4_t v = vld1q_f32(vec + d);
            v = vmulq_f32(v, v);
            sum += vaddvq_f32(v);
        }
        
        // Handle remaining elements
        for (; d < dim; d++)
        {
            float val = vec[d];
            sum += val * val;
        }
        
        vecs_l2sq[n_iter] = sum;
    }
}

// ARM64 NEON optimized matrix multiplication (C = A * B)
// This is optimized for the common case in DiskANN where we multiply
// data matrix with rotation matrix
void matrix_multiply_neon(float *C, const float *A, const float *B, 
                         size_t rows_A, size_t cols_A, size_t cols_B,
                         bool transpose_B)
{
    #pragma omp parallel for
    for (int64_t i = 0; i < (int64_t)rows_A; i++)
    {
        for (size_t j = 0; j < cols_B; j++)
        {
            float32x4_t sum_vec = vdupq_n_f32(0.0f);
            float sum = 0.0f;
            
            size_t k = 0;
            
            if (!transpose_B)
            {
                // B is not transposed: B[k][j]
                // Process 4 elements at a time
                for (; k + 3 < cols_A; k += 4)
                {
                    float32x4_t a = vld1q_f32(&A[i * cols_A + k]);
                    float32x4_t b = {B[k * cols_B + j], 
                                    B[(k + 1) * cols_B + j],
                                    B[(k + 2) * cols_B + j], 
                                    B[(k + 3) * cols_B + j]};
                    sum_vec = vfmaq_f32(sum_vec, a, b);
                }
            }
            else
            {
                // B is transposed: B[j][k]
                // Process 16 elements at a time for better cache efficiency
                for (; k + 15 < cols_A; k += 16)
                {
                    float32x4_t a0 = vld1q_f32(&A[i * cols_A + k]);
                    float32x4_t a1 = vld1q_f32(&A[i * cols_A + k + 4]);
                    float32x4_t a2 = vld1q_f32(&A[i * cols_A + k + 8]);
                    float32x4_t a3 = vld1q_f32(&A[i * cols_A + k + 12]);
                    
                    float32x4_t b0 = vld1q_f32(&B[j * cols_A + k]);
                    float32x4_t b1 = vld1q_f32(&B[j * cols_A + k + 4]);
                    float32x4_t b2 = vld1q_f32(&B[j * cols_A + k + 8]);
                    float32x4_t b3 = vld1q_f32(&B[j * cols_A + k + 12]);
                    
                    sum_vec = vfmaq_f32(sum_vec, a0, b0);
                    sum_vec = vfmaq_f32(sum_vec, a1, b1);
                    sum_vec = vfmaq_f32(sum_vec, a2, b2);
                    sum_vec = vfmaq_f32(sum_vec, a3, b3);
                }
                
                // Process 4 elements at a time
                for (; k + 3 < cols_A; k += 4)
                {
                    float32x4_t a = vld1q_f32(&A[i * cols_A + k]);
                    float32x4_t b = vld1q_f32(&B[j * cols_A + k]);
                    sum_vec = vfmaq_f32(sum_vec, a, b);
                }
            }
            
            // Sum the vector elements
            sum += vaddvq_f32(sum_vec);
            
            // Handle remaining elements
            for (; k < cols_A; k++)
            {
                if (!transpose_B)
                {
                    sum += A[i * cols_A + k] * B[k * cols_B + j];
                }
                else
                {
                    sum += A[i * cols_A + k] * B[j * cols_A + k];
                }
            }
            
            C[i * cols_B + j] = sum;
        }
    }
}

// ARM64 NEON optimized matrix-vector multiplication with addition
// Computes: C = alpha * A * B + beta * C
// This is used in the distance matrix computation
void matrix_vector_multiply_add_neon(float *C, const float *A, const float *B,
                                    size_t rows_A, size_t cols_A,
                                    float alpha, float beta)
{
    float32x4_t alpha_vec = vdupq_n_f32(alpha);
    float32x4_t beta_vec = vdupq_n_f32(beta);
    
    #pragma omp parallel for
    for (int64_t i = 0; i < (int64_t)rows_A; i++)
    {
        float32x4_t sum_vec = vdupq_n_f32(0.0f);
        float sum = 0.0f;
        
        size_t j = 0;
        
        // Process 16 elements at a time
        for (; j + 15 < cols_A; j += 16)
        {
            float32x4_t a0 = vld1q_f32(&A[i * cols_A + j]);
            float32x4_t a1 = vld1q_f32(&A[i * cols_A + j + 4]);
            float32x4_t a2 = vld1q_f32(&A[i * cols_A + j + 8]);
            float32x4_t a3 = vld1q_f32(&A[i * cols_A + j + 12]);
            
            float32x4_t b0 = vld1q_f32(&B[j]);
            float32x4_t b1 = vld1q_f32(&B[j + 4]);
            float32x4_t b2 = vld1q_f32(&B[j + 8]);
            float32x4_t b3 = vld1q_f32(&B[j + 12]);
            
            sum_vec = vfmaq_f32(sum_vec, a0, b0);
            sum_vec = vfmaq_f32(sum_vec, a1, b1);
            sum_vec = vfmaq_f32(sum_vec, a2, b2);
            sum_vec = vfmaq_f32(sum_vec, a3, b3);
        }
        
        // Process 4 elements at a time
        for (; j + 3 < cols_A; j += 4)
        {
            float32x4_t a = vld1q_f32(&A[i * cols_A + j]);
            float32x4_t b = vld1q_f32(&B[j]);
            sum_vec = vfmaq_f32(sum_vec, a, b);
        }
        
        // Sum the vector elements
        sum = vaddvq_f32(sum_vec);
        
        // Handle remaining elements
        for (; j < cols_A; j++)
        {
            sum += A[i * cols_A + j] * B[j];
        }
        
        // Apply alpha and beta scaling
        C[i] = alpha * sum + beta * C[i];
    }
}

// Specialized NEON implementation for squared distance computation
// This computes the distance matrix used in k-means clustering
void compute_closest_centers_neon(float *dist_matrix, 
                                 float *data, float *centers,
                                 float *docs_l2sq, float *centers_l2sq,
                                 size_t num_points, size_t num_centers, size_t dim)
{
    // Step 1: Add docs_l2sq to each row (broadcast)
    #pragma omp parallel for
    for (int64_t i = 0; i < (int64_t)num_points; i++)
    {
        float32x4_t doc_l2sq = vdupq_n_f32(docs_l2sq[i]);
        size_t j = 0;
        
        // Process 4 centers at a time
        for (; j + 3 < num_centers; j += 4)
        {
            vst1q_f32(&dist_matrix[i * num_centers + j], doc_l2sq);
        }
        
        // Handle remaining centers
        for (; j < num_centers; j++)
        {
            dist_matrix[i * num_centers + j] = docs_l2sq[i];
        }
    }
    
    // Step 2: Add centers_l2sq to each column
    #pragma omp parallel for
    for (int64_t i = 0; i < (int64_t)num_points; i++)
    {
        size_t j = 0;
        
        // Process 4 centers at a time
        for (; j + 3 < num_centers; j += 4)
        {
            float32x4_t dist = vld1q_f32(&dist_matrix[i * num_centers + j]);
            float32x4_t center_l2sq = vld1q_f32(&centers_l2sq[j]);
            dist = vaddq_f32(dist, center_l2sq);
            vst1q_f32(&dist_matrix[i * num_centers + j], dist);
        }
        
        // Handle remaining centers
        for (; j < num_centers; j++)
        {
            dist_matrix[i * num_centers + j] += centers_l2sq[j];
        }
    }
    
    // Step 3: Subtract 2 * dot product
    #pragma omp parallel for
    for (int64_t i = 0; i < (int64_t)num_points; i++)
    {
        float *point = &data[i * dim];
        
        for (size_t j = 0; j < num_centers; j++)
        {
            float *center = &centers[j * dim];
            
            // Use our optimized NEON dot product
            float dot = diskann::neon::dot_product_neon(point, center, dim);
            
            // dist = ||a||² + ||b||² - 2⟨a,b⟩
            dist_matrix[i * num_centers + j] -= 2.0f * dot;
        }
    }
}

} // namespace diskann

#endif // ARM64 NEON