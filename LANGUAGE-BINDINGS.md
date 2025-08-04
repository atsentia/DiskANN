# DiskANN Language Bindings Overview

## Architecture

DiskANN is primarily implemented in C++ and provides bindings for Python and Rust, allowing users to leverage the high-performance indexing and search capabilities from these languages.

```
┌─────────────────────────────────────────────┐
│           Python Applications               │
│  (Data Science, ML Pipelines, Research)     │
└─────────────────┬───────────────────────────┘
                  │
┌─────────────────▼───────────────────────────┐
│         Python Bindings (PyBind11)          │
│   • Dynamic/Static Memory Index             │
│   • Disk Index                              │
│   • Builder Functions                       │
└─────────────────┬───────────────────────────┘
                  │
┌─────────────────▼───────────────────────────┐
│          C++ Core Library                   │ ←─── Primary Implementation
│   • Index algorithms (Vamana)               │
│   • Distance functions (L2, Cosine, IP)     │
│   • PQ compression                          │
│   • Disk I/O operations                     │
│   • ARM64 NEON optimizations               │
└─────────────────┬───────────────────────────┘
                  │
┌─────────────────▼───────────────────────────┐
│          Rust Implementation                │
│   • Safe memory management                  │
│   • Concurrent operations                   │
│   • Cross-platform I/O                      │
└─────────────────────────────────────────────┘
                  │
┌─────────────────▼───────────────────────────┐
│          Rust Applications                  │
│  (Systems Programming, Web Services)        │
└─────────────────────────────────────────────┘
```

## Python Bindings (python/)

### Purpose
Provides a high-level, easy-to-use interface for Python developers to use DiskANN without dealing with C++ complexities.

### Implementation
- **Technology**: PyBind11 - a lightweight header-only library
- **Location**: `python/src/module.cpp` - main binding definitions
- **Coverage**: All major DiskANN functionality

### Key Components

1. **Builder Functions** (`builder.cpp`)
   - `build_disk_index()` - Build disk-based indices
   - `build_memory_index()` - Build in-memory indices

2. **Index Classes**
   - `DynamicMemoryIndex` - For indices that support insertions/deletions
   - `StaticMemoryIndex` - For read-only in-memory indices
   - `StaticDiskIndex` - For disk-based indices

3. **Python API Example**
```python
import diskann

# Build an index
diskann.build_memory_float_index(
    distance_metric="l2",
    data_file_path="vectors.bin",
    index_output_path="index",
    graph_degree=32,
    complexity=100,
    alpha=1.2,
    num_threads=8
)

# Load and search
index = diskann.StaticMemoryFloatIndex("index", num_threads=8)
ids, dists = index.search(query_vector, k=10, complexity=100)
```

### How Python Bindings Work
1. **Type Mapping**: C++ types are mapped to Python types
   - `std::vector<float>` → `numpy.ndarray`
   - `uint32_t` → `int`
   - `std::string` → `str`

2. **Memory Management**: PyBind11 handles reference counting
3. **Thread Safety**: GIL (Global Interpreter Lock) released during compute-intensive operations

## Rust Implementation (rust/)

### Purpose
Provides a complete reimplementation of DiskANN in Rust, offering memory safety and modern concurrency primitives.

### Relationship to C++
- **Independent Implementation**: Not bindings, but a full rewrite
- **Algorithm Parity**: Implements the same Vamana algorithm
- **Performance Focus**: Leverages Rust's zero-cost abstractions

### Key Differences

1. **Memory Safety**
   ```rust
   // Rust ensures no data races at compile time
   pub struct InMemIndex<T> {
       data: Arc<RwLock<Dataset<T>>>,
       graph: Arc<RwLock<Graph>>,
   }
   ```

2. **Modern Concurrency**
   ```rust
   // Rayon for parallel iteration
   use rayon::prelude::*;
   
   points.par_iter()
       .map(|point| self.search_single(point))
       .collect()
   ```

3. **Cross-Platform I/O**
   ```rust
   // Platform-specific optimizations
   #[cfg(target_os = "windows")]
   use platform::WindowsAlignedFileReader;
   
   #[cfg(not(target_os = "windows"))]
   use platform::PosixAlignedFileReader;
   ```

### Rust Components

1. **Core Library** (`diskann/`)
   - Full index implementation
   - Distance functions
   - Graph algorithms
   - PQ compression

2. **Command Line Tools** (`cmd_drivers/`)
   - `build_memory_index` - CLI for building indices
   - `search_memory_index` - CLI for searching
   - Direct equivalents to C++ tools

3. **Platform Layer** (`platform/`)
   - OS-specific optimizations
   - File I/O abstractions

## Language Binding Comparison

| Feature | C++ Core | Python Bindings | Rust Implementation |
|---------|----------|-----------------|---------------------|
| Performance | Highest | ~5% overhead | Comparable to C++ |
| Memory Safety | Manual | Python GC | Compile-time guaranteed |
| Ease of Use | Complex | Very Easy | Moderate |
| ARM64 NEON | ✅ Optimized | ✅ Uses C++ | ⚠️ Via intrinsics |
| Parallel STL | ✅ Full support | ✅ Uses C++ | ✅ Rayon |
| Type Safety | Compile-time | Runtime | Compile-time |
| Package Management | CMake | pip | Cargo |

## ARM64 Considerations

### Python Bindings
- Automatically benefit from C++ ARM64 optimizations
- No additional work needed
- NumPy arrays are efficiently passed to C++

### Rust Implementation
- Must implement ARM64 optimizations separately
- Uses Rust's `std::arch::aarch64` intrinsics
- Example:
```rust
#[cfg(target_arch = "aarch64")]
use std::arch::aarch64::*;

#[cfg(target_arch = "aarch64")]
unsafe fn dot_product_neon(a: &[f32], b: &[f32]) -> f32 {
    // NEON implementation
}
```

## Testing Strategy

### Python Tests (`python/tests/`)
- Use pytest framework
- Test C++ functionality through Python API
- Focus on API correctness and usability

### Rust Tests (`rust/diskann/tests/`)
- Use built-in Rust testing
- Unit tests for each module
- Integration tests with test data

### Cross-Language Validation
- Same test vectors used across languages
- Results should match within floating-point tolerance
- Performance benchmarks to ensure parity

## Building and Distribution

### Python Package
```bash
# Build Python wheel
cd python
pip install -e .

# Create distribution
python setup.py bdist_wheel
```

### Rust Package
```bash
# Build all crates
cargo build --release

# Run tests
cargo test

# Build specific tool
cargo build -p build_memory_index --release
```

## Recommendations

1. **For Data Scientists**: Use Python bindings
   - Integrates with NumPy/Pandas
   - Easy to use in Jupyter notebooks
   - Full access to DiskANN features

2. **For Systems Programmers**: Use Rust implementation
   - Memory safety guarantees
   - Better for long-running services
   - Modern async/await support

3. **For Maximum Performance**: Use C++ directly
   - All optimizations available
   - Finest control over resources
   - Direct access to new features

## Future Directions

1. **WebAssembly Bindings**: Compile to WASM for browser/edge
2. **Java/JNI Bindings**: For JVM ecosystem
3. **Swift Bindings**: For iOS/macOS native apps
4. **Unified ARM64 Optimizations**: Share NEON code across languages