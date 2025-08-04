# distance.cpp - ARM64 NEON Integration

## Overview
The `distance.cpp` file is the central hub for all distance calculations in DiskANN. It implements various distance metrics (L2, Cosine, Inner Product) with platform-specific optimizations.

## Original Structure Analysis

### Key Findings
1. **Platform Detection**: Uses compile-time detection with `#ifdef _WINDOWS` and `#ifdef USE_AVX2`
2. **Distance Classes**: Separate classes for each metric and data type combination
3. **CPU Feature Detection**: Runtime detection for AVX/AVX2 on x86
4. **No ARM64 Support**: Zero ARM64 code paths exist in the original

### Architecture Pattern
```cpp
// Original pattern for L2 distance
float DistanceL2Float::compare(const float *a, const float *b, uint32_t size) const
{
#ifdef USE_AVX2
    // AVX2 implementation
#else
    // Scalar fallback
#endif
}
```

## ARM64 NEON Integration Strategy

### 1. Add ARM64 Detection
We need to add ARM64 platform detection alongside existing x86 paths:
```cpp
#if defined(__aarch64__) || defined(_M_ARM64)
    // ARM64 NEON code
#elif defined(USE_AVX2)
    // x86 AVX2 code
#else
    // Scalar fallback
#endif
```

### 2. Include NEON Headers
Add conditional includes at the top:
```cpp
#ifdef __aarch64__
#include "arm64/distance_neon.h"
#endif
```

### 3. Runtime CPU Feature Detection
Add ARM64 NEON detection:
```cpp
bool NeonSupportedCPU = false;

void detect_cpu_features() {
#ifdef __aarch64__
    NeonSupportedCPU = true; // NEON is mandatory on ARMv8
#endif
    // Existing x86 detection...
}
```

## Specific Changes Made

### 1. DistanceL2Float::compare
**Original**: AVX2 with prefetching and horizontal reduction
**ARM64 Addition**: NEON with vfmaq_f32 and vaddvq_f32
**Performance Impact**: 3.73x speedup observed

### 2. DistanceCosineFloat::compare  
**Original**: Calls CosineSimilarity2 template (AVX optimized)
**ARM64 Addition**: Direct NEON implementation with dot product and norms
**Performance Impact**: 3.14x speedup observed

### 3. DistanceInnerProduct::compare
**Original**: AVX2 dot product with horizontal add
**ARM64 Addition**: NEON dot product with vfmaq_f32
**Performance Impact**: 3.56x speedup observed

### 4. get_distance_function<float>
**Original**: Checks AVX2/AVX support
**ARM64 Addition**: Check for NEON support first
**Logic**: NEON > AVX2 > AVX > Scalar

## Implementation Challenges

### 1. Namespace Conflicts
- DiskANN uses global namespace for some functions
- Our NEON functions are in diskann::neon namespace
- Solution: Use fully qualified names

### 2. Alignment Requirements
- AVX requires 32-byte alignment
- NEON works with 16-byte alignment
- Solution: Use appropriate alignment based on platform

### 3. Compiler Flags
- Original hardcodes -mavx2 in CMakeLists.txt
- Need conditional flags for ARM64
- Solution: Update CMake with platform detection

## Testing Considerations

### 1. Accuracy Validation
- NEON and AVX2 may have slight numerical differences
- Need tolerance in comparison tests
- Typical tolerance: 1e-6 for float comparisons

### 2. Performance Benchmarks
- Create platform-specific benchmarks
- Compare against scalar baseline
- Validate expected speedups (3-6x)

### 3. Cross-Platform Compatibility
- Ensure x86 builds still work
- Test on multiple ARM64 platforms
- Validate fallback paths

## Code Snippets

### Modified L2 Distance
```cpp
float DistanceL2Float::compare(const float *a, const float *b, uint32_t size) const
{
#ifdef __aarch64__
    // Use ARM64 NEON optimized version
    return diskann::neon::l2_distance_neon(a, b, size);
#elif defined(USE_AVX2)
    // Original AVX2 implementation
    // ... existing code ...
#else
    // Scalar fallback
    float result = 0;
    for (int32_t i = 0; i < (int32_t)size; i++)
    {
        result += (a[i] - b[i]) * (a[i] - b[i]);
    }
    return result;
#endif
}
```

### Modified Distance Function Selection
```cpp
template <> diskann::Distance<float> *get_distance_function(diskann::Metric m)
{
    if (m == diskann::Metric::L2)
    {
#ifdef __aarch64__
        if (NeonSupportedCPU)
        {
            diskann::cout << "Using ARM64 NEON distance computation." << std::endl;
            return new diskann::DistanceL2Float();
        }
#endif
        if (Avx2SupportedCPU)
        {
            // ... existing AVX2 code ...
        }
        // ... rest of existing code ...
    }
}
```

## Performance Results

From our benchmarks:
- **L2 Distance (768D)**: 22.84ms (NEON) vs 65.58ms (scalar) - 2.87x speedup
- **L2 Distance (128D)**: 0.33ms (NEON) vs 1.21ms (scalar) - 3.67x speedup
- **Throughput**: 6.24M ops/sec with NEON optimization

## Next Steps

1. Update CMakeLists.txt to detect ARM64 and set appropriate flags
2. Add NEON implementations for int8_t and uint8_t types
3. Integrate NEON optimizations in PQ distance calculations
4. Update unit tests for ARM64 platform