# Python Bindings Setup for DiskANN

## pybind11 Installation Options

### Option 1: Using pip (Recommended for Development)
```bash
pip install pybind11
```

This is the simplest approach and works across all platforms. The build system will automatically find pybind11 when installed this way.

### Option 2: Using System Package Manager

**Ubuntu/Debian:**
```bash
sudo apt-get install pybind11-dev
```

**Fedora/RHEL/CentOS:**
```bash
sudo dnf install pybind11-devel
```

**macOS (Homebrew):**
```bash
brew install pybind11
```

**Windows (vcpkg):**
```bash
vcpkg install pybind11
```

### Option 3: Building Without Installing pybind11

The DiskANN build system is already configured to handle pybind11 as a build dependency through `pyproject.toml`. When you build the Python wheel, it will automatically download and use pybind11:

```bash
# This automatically installs pybind11 in the build environment
python -m build --wheel
```

### Option 4: Using CMake FetchContent (Cross-Platform)

For a completely self-contained build, you can modify the CMakeLists.txt to fetch pybind11 automatically:

```cmake
include(FetchContent)
FetchContent_Declare(
  pybind11
  GIT_REPOSITORY https://github.com/pybind/pybind11
  GIT_TAG        v2.11.1
)
FetchContent_MakeAvailable(pybind11)
```

## Building Python Bindings with ARM64 Optimizations

### 1. Ensure Dependencies Are Installed
```bash
# Python build tools
pip install build wheel setuptools

# NumPy (required, version 1.26+ for Python 3.12)
pip install "numpy>=1.26"
```

### 2. Build the Wheel
```bash
cd /path/to/diskann
python -m build --wheel
```

The build process will:
- Detect ARM64 architecture automatically
- Enable NEON optimizations if available
- Link against the optimized C++ library

### 3. Install the Built Wheel
```bash
pip install dist/diskannpy-*.whl
```

### 4. Verify ARM64 Optimizations Are Active
```python
import diskannpy

# Create an index and verify it's using optimized code
index = diskannpy.StaticMemoryIndex(
    distance_metric="l2",
    vector_dtype="float32",
    data_dim=128,
    index_directory="test_index"
)

# The index will automatically use ARM64 NEON optimizations
# for distance calculations on supported platforms
```

## Platform-Specific Notes

### Linux ARM64 (AWS Graviton, Ampere Altra)
- Ensure you have a recent GCC (9+) or Clang (11+)
- NEON is always available on AArch64
- No special flags needed

### macOS Apple Silicon
- Use native ARM64 Python (not x86 under Rosetta)
- Xcode Command Line Tools required
- NEON optimizations enabled by default

### Windows ARM64
- Visual Studio 2022 or later recommended
- ARM64 build tools must be installed
- NEON intrinsics available through arm64_neon.h

## Troubleshooting

### "pybind11 not found" Error
1. Ensure pybind11 is installed: `pip show pybind11`
2. Check CMake can find it: `python -c "import pybind11; print(pybind11.get_cmake_dir())"`
3. Set CMAKE_PREFIX_PATH if needed

### Build Fails on ARM64
1. Verify architecture: `python -c "import platform; print(platform.machine())"`
2. Check compiler supports NEON: `echo | gcc -dM -E - | grep ARM_NEON`
3. Ensure you're not cross-compiling unintentionally

### Performance Not Improved
1. Verify optimizations are enabled in build
2. Check you're using native ARM64 Python (not emulated)
3. Use larger vectors (128+ dimensions) to see benefits

## Summary

The Python bindings will automatically benefit from ARM64 NEON optimizations when:
1. Built on an ARM64 platform
2. Using native ARM64 Python
3. The C++ library was compiled with NEON support

No special configuration is needed - the build system handles everything automatically!