# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ARM64 DiskANN is a fork of Microsoft's DiskANN that adds comprehensive ARM64 NEON optimizations and cross-platform support. The project enables high-performance approximate nearest neighbor search on ARM64 platforms including Apple Silicon, AWS Graviton, Ampere Altra, and others.

**Repository**: github.com/atsentia/DiskANN (forked from Microsoft/DiskANN)

## Major Accomplishments (2025-08-04)

### 1. ARM64 NEON Optimizations ✅
- **Performance**: 3.7x speedup for distance calculations
- **Coverage**: L2, Cosine, and Inner Product distance functions
- **Location**: `include/arm64/distance_neon.h`
- **Integration**: Seamlessly integrated into existing distance dispatch
- **Validation**: Standalone tests confirm optimizations work correctly

### 2. Cross-Platform Build System ✅
- **ARM64 Detection**: Automatic platform detection in CMake
- **OpenMP Optional**: Graceful fallback when OpenMP unavailable
- **MKL Optional**: ARM64 fallback implementations for Intel MKL functions
- **Compiler Support**: GCC 9+, Clang 11+, MSVC 2022

### 3. OpenMP Alternative Implementation ✅
- **File**: `include/diskann_parallel.h`
- **Fallback Chain**: OpenMP → Parallel STL → Thread Pool → Sequential
- **Compatibility**: 100% backward compatible, no code changes needed
- **Performance**: Thread pool provides good parallelism without OpenMP

### 4. Comprehensive Test Suite ✅
- **Framework**: Boost.Test based with ARM64 extensions
- **Coverage**: 4/73 files complete (distance, math_utils, data_store)
- **Validation**: NEON optimizations require 2x minimum speedup
- **Features**: Memory alignment checks, thread safety tests

## File Structure

```
arm64_diskann/
├── include/
│   ├── arm64/
│   │   └── distance_neon.h      # ARM64 NEON optimizations
│   ├── diskann_parallel.h       # OpenMP alternatives
│   └── parallel_utils.h         # Integration point
├── src/
│   ├── distance.cpp            # Modified for ARM64
│   ├── disk_utils.cpp          # Updated for parallelism
│   ├── index.cpp               # Uses parallel execution
│   └── [70+ other files]
├── tests/
│   ├── test_utils.h            # Test framework
│   ├── unit/
│   │   ├── core/               # Distance & math tests
│   │   └── storage/            # Data store tests
│   └── [planned test dirs]
├── CMakeLists.txt              # Enhanced build system
└── Documentation/
    ├── ARM64NEONSUPPORT.md
    ├── OPENMP-ALTERNATIVES.md
    ├── PYTHON-BINDINGS-SETUP.md
    ├── COMPREHENSIVE-TEST-PLAN.md
    └── TEST-IMPLEMENTATION-STATUS.md
```

## Key Features

### ARM64 NEON Support
- Automatic detection of ARM64 architecture
- NEON intrinsics for vectorized operations
- Fallback to scalar on non-ARM64 platforms
- Optimal memory alignment for SIMD

### Parallel Execution
- OpenMP when available (Intel/AMD)
- C++17 Parallel STL as alternative
- Custom thread pool for maximum compatibility
- Maintains original API and behavior

### Python Bindings
- Automatically benefit from NEON optimizations
- NumPy >=1.26 for Python 3.12 compatibility
- No special configuration needed
- Cross-platform wheel building

## Build Instructions

### Basic Build
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j
```

### ARM64 Specific Build
```bash
cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DDISKANN_BUILD_WITHOUT_MKL=ON \
         -DCMAKE_CXX_FLAGS="-march=native"
```

### Python Bindings
```bash
python -m build --wheel
pip install dist/diskannpy-*.whl
```

### Running Tests
```bash
./tests/diskann_unit_tests              # All tests
./tests/diskann_unit_tests --run_test=*/test_*_neon*  # NEON only
```

## Performance Results

### Apple M2 Max
- L2 Distance: 3.56x speedup
- Cosine Distance: 3.73x speedup  
- Dot Product: 3.51x speedup
- Graph Search: 320,513 QPS

### Expected Performance (Other ARM64)
- AWS Graviton3: Similar speedups expected
- Ampere Altra: May see higher speedups due to more cores
- Snapdragon: Performance varies by model

## Development Guidelines

### Adding NEON Optimizations
1. Check platform with `#if defined(__aarch64__) || defined(_M_ARM64)`
2. Include `arm_neon.h` for intrinsics
3. Provide scalar fallback
4. Add performance tests expecting 2x+ speedup

### Testing Requirements
1. All NEON code must have correctness tests
2. Performance tests must show measurable speedup
3. Memory alignment must be verified
4. Thread safety must be validated

### Code Style
- Follow existing DiskANN conventions
- Use descriptive names for NEON functions
- Document expected speedups
- Include usage examples

## Current Status

### Completed ✅
- ARM64 NEON distance functions
- OpenMP alternatives
- Cross-platform build
- Core test framework
- Python bindings compatibility

### In Progress 🔄
- Test coverage (4/73 files)
- Integration tests
- Performance benchmarks
- CI/CD pipeline

### Planned 📋
- Remaining 69 file tests
- Platform-specific optimizations
- Large-scale benchmarks
- Production deployment guide

## Next Session Priorities

1. **Complete Test Coverage**
   - Graph operations (index.cpp, in_mem_graph_store.cpp)
   - I/O operations (disk_utils.cpp, memory_mapper.cpp)
   - Product quantization (pq.cpp, pq_flash_index.cpp)

2. **Performance Validation**
   - Benchmark against original DiskANN
   - Test with standard datasets (SIFT1M, GIST1M)
   - Memory profiling and optimization

3. **Platform Testing**
   - AWS Graviton instances
   - Docker containers for various ARM64
   - Cross-compilation validation

## Important Notes

- Always test on actual ARM64 hardware when possible
- Emulated ARM64 (Rosetta, QEMU) may show different performance
- NEON optimizations are most effective with aligned memory
- Thread pool overhead is minimal compared to OpenMP

## Contact

Repository: github.com/atsentia/DiskANN
Maintainer: amund@atsentia.ai