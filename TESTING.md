# DiskANN ARM64 Testing Guide

## Overview
This document describes the comprehensive testing strategy for DiskANN ARM64, covering all aspects from basic smoke tests to performance benchmarks.

## Test Categories

### 1. Smoke Tests (Quick Validation)
**Purpose**: Verify basic functionality works after each build
**Runtime**: < 1 minute
**Coverage**: Critical paths only

```bash
# Run smoke tests
./build/tests/test_smoke --gtest_filter="*Smoke*"
```

### 2. Unit Tests (Component Testing)
**Purpose**: Test individual functions and classes in isolation
**Runtime**: < 5 minutes
**Coverage**: All public APIs in src/ directory

```bash
# Run all unit tests
./build/tests/test_unit_all

# Run specific component tests
./build/tests/test_distance_functions
./build/tests/test_math_utils
./build/tests/test_parallel_stl
./build/tests/test_index_operations
```

### 3. Performance Tests (Micro-benchmarks)
**Purpose**: Measure performance of critical operations
**Runtime**: 5-10 minutes
**Coverage**: Hot paths and optimized functions

```bash
# Run performance tests
./build/tests/test_performance --gtest_filter="*Performance*"
```

### 4. Integration Tests (System Testing)
**Purpose**: Test complete workflows and component interactions
**Runtime**: 10-30 minutes
**Coverage**: End-to-end scenarios

```bash
# Run integration tests
./build/tests/test_integration
```

### 5. Benchmark Tests (Optional)
**Purpose**: Compare against baseline performance
**Runtime**: 30+ minutes
**Coverage**: Real-world datasets and workloads

```bash
# Run full benchmarks
./build/tests/benchmark_suite
```

## Test Structure

```
tests/
├── CMakeLists.txt              # Test build configuration
├── test_helpers.h              # Common test utilities
├── datasets/                   # Test data
│   ├── small_vectors.bin       # 1K vectors for quick tests
│   ├── medium_vectors.bin      # 10K vectors for thorough tests
│   └── large_vectors.bin       # 100K vectors for benchmarks
│
├── smoke/                      # Smoke tests
│   ├── test_smoke_basic.cpp    # Basic functionality
│   └── test_smoke_build.cpp    # Build and load index
│
├── unit/                       # Unit tests
│   ├── test_distance_functions.cpp
│   ├── test_math_utils.cpp
│   ├── test_parallel_stl.cpp
│   ├── test_index_operations.cpp
│   ├── test_pq_operations.cpp
│   ├── test_disk_operations.cpp
│   └── test_memory_operations.cpp
│
├── performance/                # Performance tests
│   ├── test_perf_distance.cpp
│   ├── test_perf_parallel.cpp
│   └── test_perf_index.cpp
│
├── integration/                # Integration tests
│   ├── test_build_search_flow.cpp
│   ├── test_disk_index_flow.cpp
│   └── test_filtered_search.cpp
│
└── benchmark/                  # Benchmark tests
    ├── benchmark_distance.cpp
    ├── benchmark_index_build.cpp
    └── benchmark_search.cpp
```

## Coverage Requirements

### Core Source Files (src/)
Each source file must have corresponding tests:

| Source File | Test Coverage | Status |
|------------|---------------|---------|
| distance.cpp | Unit + Performance | ✅ |
| math_utils.cpp | Unit + Performance | ✅ |
| index.cpp | Unit + Integration | 🔄 |
| pq.cpp | Unit | 🔄 |
| disk_utils.cpp | Unit + Integration | 🔄 |
| parallel execution | Unit + Performance | ✅ |
| ... | ... | ... |

### ARM64 NEON Optimizations
- Correctness tests comparing against scalar implementations
- Performance tests showing speedup metrics
- Edge case tests (alignment, small sizes, etc.)

### Parallel STL Implementation
- Thread safety tests
- Work distribution tests
- Performance scaling tests
- Fallback behavior tests

## Running Tests

### Quick Test (CI/CD)
```bash
# Run smoke and fast unit tests
make test-quick
```

### Full Test Suite
```bash
# Run all tests except benchmarks
make test
```

### Platform-Specific Tests
```bash
# ARM64 NEON tests (only on ARM64)
./build/tests/test_arm64_neon

# x86 tests (only on x86)
./build/tests/test_x86_avx
```

### Memory and Thread Safety
```bash
# Run with address sanitizer
make test-asan

# Run with thread sanitizer
make test-tsan

# Run with valgrind
valgrind --leak-check=full ./build/tests/test_unit_all
```

## Test Data Generation

Generate test vectors for consistent testing:

```cpp
// Generate random float vectors
./build/tests/generate_test_data --type float --num 1000 --dim 128 --output small_vectors.bin

// Generate clustered data
./build/tests/generate_test_data --type float --num 10000 --dim 128 --clusters 100 --output clustered_vectors.bin
```

## Performance Baselines

Expected performance on ARM64 (M2 Max):

| Operation | Scalar | NEON | Speedup |
|-----------|---------|------|---------|
| L2 Distance (128d) | 361 ns | 97 ns | 3.7x |
| Dot Product (128d) | 342 ns | 89 ns | 3.8x |
| Index Build (10K) | 156 ms | 52 ms | 3.0x |
| Search (10K index) | 0.051 ms | 0.014 ms | 3.6x |

## Continuous Integration

### GitHub Actions Workflow
```yaml
test:
  strategy:
    matrix:
      platform: [ubuntu-22.04-arm64, macos-14-arm64, windows-2022-arm64]
  steps:
    - name: Run smoke tests
      run: make test-smoke
    - name: Run unit tests
      run: make test-unit
    - name: Run performance tests
      run: make test-performance
```

## Debugging Test Failures

### Enable verbose output
```bash
./build/tests/test_name --gtest_filter="TestSuite.TestName" --gtest_print_time=1 -v
```

### Generate test report
```bash
./build/tests/test_all --gtest_output="xml:test_report.xml"
```

### Debug specific test
```bash
gdb ./build/tests/test_distance_functions
(gdb) break DistanceFunctionTest::L2DistanceCorrectness
(gdb) run --gtest_filter="DistanceFunctionTest.L2DistanceCorrectness"
```

## Adding New Tests

1. **Identify test category** (smoke/unit/performance/integration)
2. **Create test file** in appropriate directory
3. **Include test helpers** and required headers
4. **Write focused tests** with clear assertions
5. **Update CMakeLists.txt** to include new test
6. **Run locally** before committing
7. **Update this documentation** if adding new test categories

## Test Best Practices

1. **Fast and Focused**: Each test should run quickly and test one thing
2. **Deterministic**: Use fixed seeds for random data
3. **Independent**: Tests should not depend on each other
4. **Clear Names**: Test names should describe what they test
5. **Good Assertions**: Use appropriate EXPECT_* macros with clear messages
6. **Resource Cleanup**: Ensure proper cleanup in teardown
7. **Platform Aware**: Use compile-time checks for platform-specific tests

## Troubleshooting

### Common Issues

1. **NEON tests failing on x86**: These tests are conditionally compiled
2. **Parallel tests flaky**: Check for race conditions, use atomic operations
3. **Performance tests slow**: Reduce iteration counts for CI/CD
4. **Memory leaks**: Run with ASAN or valgrind
5. **Floating point differences**: Use EXPECT_NEAR with appropriate epsilon

### Getting Help

- Check test output for detailed error messages
- Enable verbose logging with `--gtest_print_time=1`
- Look at similar existing tests for examples
- File issues with test category and platform information