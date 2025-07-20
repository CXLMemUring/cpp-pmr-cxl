# CXL Region Memory Model Implementation

This repository contains the reference implementation for the C++ proposal P3XXX: "Region-Based Memory Coherency and Consistency Model for CXL Memory Systems".

## Overview

The implementation provides:
- Three-tier memory region model (local, middle, remote) with distinct coherency and consistency guarantees
- Asynchronous transfer APIs for efficient data movement between regions
- Region-aware memory allocators compatible with C++ PMR
- Bias control for CXL memory optimization

## Building

```bash
mkdir build
cd build
cmake ..
make -j
```

### Build Options

- `BUILD_EXAMPLES`: Build example programs (default: ON)
- `BUILD_TESTS`: Build test suite (default: ON)
- `ENABLE_ASAN`: Enable AddressSanitizer (default: OFF)
- `ENABLE_TSAN`: Enable ThreadSanitizer (default: OFF)

## Usage

### Basic Example

```cpp
#include <pmr/region_memory.hpp>
#include <pmr/region_allocator.hpp>

// Create region-specific allocators
auto local_alloc = std::pmr::make_local_allocator<int>();
auto middle_alloc = std::pmr::make_middle_allocator<int>();
auto remote_alloc = std::pmr::make_remote_allocator<int>();

// Use with STL containers
std::pmr::vector<int> local_data(local_alloc);
std::pmr::vector<int> remote_data(remote_alloc);
```

### Asynchronous Transfers

```cpp
#include <pmr/async_transfer.hpp>

// Async copy from remote to local
auto handle = std::pmr::async_get(local_buf, remote_buf, size);

// Do other work while transfer happens
compute_something_else();

// Wait for completion
handle.wait();
```

## Architecture

### Memory Regions

1. **Local Region**
   - CPU-attached memory
   - Full hardware coherency
   - Sequential consistency
   - Best for frequently accessed data

2. **Middle Region**
   - CXL-attached memory
   - Relaxed coherency with synchronization
   - Acquire-release semantics
   - Supports bias control

3. **Remote Region**
   - Pooled/fabric-attached memory
   - Eventual consistency
   - Relaxed ordering
   - Optimized for bulk transfers

### Key Components

- `region_memory_resource`: PMR-compatible memory resources
- `region_allocator`: STL-compatible allocators
- `async_transfer`: Asynchronous data movement primitives
- Synchronization primitives for cross-region coherency

## Examples

See the `examples/` directory for:
- `basic_usage.cpp`: Region allocation and benchmarking
- `async_transfer_demo.cpp`: Asynchronous transfer patterns

## Paper

The accompanying paper "CXLMemUring: A Hardware-Software Co-design Paradigm for Asynchronous and Flexible Parallel CXL Memory Pool Access" is available in the `paper/` directory.

## License

This implementation is provided as a reference for the C++ standards proposal. See LICENSE for details.