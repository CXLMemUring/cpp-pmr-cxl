Document number: P3XXX R0
Date: 2025-07-20
Project: Programming Language C++
Audience: SG1 Concurrency, EWG Evolution
Reply-to: Yiwei Yang <yiwei.yang@example.com>

# Region-Based Memory Coherency and Consistency Model for CXL Memory Systems

## Abstract

This proposal introduces a region-based memory coherency and consistency model for C++ to support emerging CXL (Compute Express Link) memory architectures. We define three distinct memory regions—local, middle, and remote—each with specific coherency guarantees and consistency models. This extension enables C++ programs to efficiently utilize heterogeneous memory systems while maintaining predictable behavior across different memory access patterns and latencies.

## Table of Contents

1. [Introduction](#introduction)
2. [Motivation and Scope](#motivation-and-scope)
3. [Design Overview](#design-overview)
4. [Technical Specifications](#technical-specifications)
5. [Examples](#examples)
6. [Impact on the Standard](#impact-on-the-standard)
7. [Implementation Experience](#implementation-experience)
8. [Acknowledgments](#acknowledgments)
9. [References](#references)

## Introduction

Modern computing systems increasingly rely on heterogeneous memory architectures, particularly with the advent of CXL technology. Current C++ memory models do not adequately address the varying coherency and consistency requirements of different memory regions in such systems. This proposal introduces a formal specification for three memory regions with distinct characteristics.

## Motivation and Scope

### Background

CXL enables memory expansion and pooling across PCIe with cache coherency support. As described in [1], the memory wall problem requires new approaches to memory access patterns. Current C++ memory models assume uniform memory characteristics, which no longer holds in CXL-enabled systems.

### Problem Statement

1. **Latency Heterogeneity**: Memory access latencies vary significantly between local DRAM, CXL-attached memory, and remote pooled memory.
2. **Coherency Overhead**: Maintaining full coherency across all memory regions incurs unnecessary overhead for certain use cases.
3. **Programming Model Gap**: No standard way to express different consistency requirements for different memory regions.

### Goals

- Define clear semantics for three memory regions: local, middle, and remote
- Provide mechanisms to specify memory region attributes
- Enable optimization opportunities while maintaining correctness
- Maintain backward compatibility with existing code

## Design Overview

### Memory Region Classification

We propose three memory regions based on proximity and access characteristics:

1. **Local Region**: Traditional CPU-attached memory with full coherency
2. **Middle Region**: CXL-attached memory with configurable coherency
3. **Remote Region**: Pooled memory with relaxed consistency

### Core Concepts

```cpp
namespace std::pmr {
    enum class memory_region {
        local,
        middle,
        remote
    };
    
    enum class coherency_model {
        strict,      // Full cache coherency
        relaxed,     // Relaxed coherency with explicit synchronization
        eventual     // Eventual consistency
    };
    
    enum class consistency_model {
        sequential,  // Sequential consistency
        acquire_release,  // Acquire-release semantics
        relaxed      // Relaxed ordering
    };
}
```

## Technical Specifications

### Local Region Memory Model

**Definition**: Memory directly attached to the CPU through traditional memory controllers.

**Coherency Model**: `strict`
- Full cache coherency maintained by hardware
- All CPU cores observe memory operations in program order
- Cache line granularity (typically 64 bytes)

**Consistency Model**: `sequential`
- Sequential consistency for data-race-free programs
- All threads observe all operations in a total order
- Compatible with `std::memory_order_seq_cst`

**Formal Specification**:
```cpp
namespace std::pmr {
    template<>
    struct region_traits<memory_region::local> {
        static constexpr coherency_model coherency = coherency_model::strict;
        static constexpr consistency_model consistency = consistency_model::sequential;
        static constexpr size_t coherency_granularity = hardware_destructive_interference_size;
        static constexpr bool supports_atomic_operations = true;
    };
}
```

### Middle Region Memory Model

**Definition**: Memory attached through CXL.mem protocol with configurable coherency.

**Coherency Model**: `relaxed`
- Hardware-assisted coherency with explicit synchronization points
- Coherency domain includes CPU and CXL devices
- Supports bias modes for optimized access patterns

**Consistency Model**: `acquire_release`
- Acquire-release semantics by default
- Synchronization through explicit barriers
- Compatible with `std::memory_order_acquire` and `std::memory_order_release`

**Formal Specification**:
```cpp
namespace std::pmr {
    template<>
    struct region_traits<memory_region::middle> {
        static constexpr coherency_model coherency = coherency_model::relaxed;
        static constexpr consistency_model consistency = consistency_model::acquire_release;
        static constexpr size_t coherency_granularity = 256; // CXL flit size
        static constexpr bool supports_atomic_operations = true;
        
        // CXL-specific features
        static constexpr bool supports_bias_mode = true;
        static constexpr bool supports_direct_cache_placement = true;
    };
}
```

**Synchronization Primitives**:
```cpp
namespace std::pmr {
    // Ensure coherency for middle region memory
    void coherence_barrier(memory_region region);
    
    // Bias control for CXL memory
    void set_bias_mode(void* ptr, size_t size, bias_mode mode);
}
```

### Remote Region Memory Model

**Definition**: Memory accessed through CXL switches or memory pooling infrastructure.

**Coherency Model**: `eventual`
- No hardware coherency guarantees
- Explicit software coherency management required
- Suitable for bulk data transfers and infrequent synchronization

**Consistency Model**: `relaxed`
- Relaxed memory ordering
- No ordering guarantees between operations
- Requires explicit fences for synchronization

**Formal Specification**:
```cpp
namespace std::pmr {
    template<>
    struct region_traits<memory_region::remote> {
        static constexpr coherency_model coherency = coherency_model::eventual;
        static constexpr consistency_model consistency = consistency_model::relaxed;
        static constexpr size_t coherency_granularity = 4096; // Page granularity
        static constexpr bool supports_atomic_operations = false;
        
        // Remote memory specific
        static constexpr bool requires_explicit_flush = true;
        static constexpr size_t minimum_transfer_size = 4096;
    };
}
```

**Synchronization Operations**:
```cpp
namespace std::pmr {
    // Explicit flush for remote memory
    void flush_remote(void* ptr, size_t size);
    
    // Bulk transfer operations
    future<void> async_get(void* local_dst, const void* remote_src, size_t size);
    future<void> async_put(void* remote_dst, const void* local_src, size_t size);
}
```

### Memory Allocation Interface

```cpp
namespace std::pmr {
    class region_memory_resource : public memory_resource {
    public:
        explicit region_memory_resource(memory_region region);
        
        memory_region get_region() const noexcept;
        coherency_model get_coherency_model() const noexcept;
        consistency_model get_consistency_model() const noexcept;
        
    private:
        void* do_allocate(size_t bytes, size_t alignment) override;
        void do_deallocate(void* p, size_t bytes, size_t alignment) override;
        bool do_is_equal(const memory_resource& other) const noexcept override;
        
        memory_region region_;
    };
}
```

### Memory Access Annotations

```cpp
namespace std {
    // Annotate pointers with region information
    template<typename T>
    using local_ptr = T* [[region::local]];
    
    template<typename T>
    using middle_ptr = T* [[region::middle]];
    
    template<typename T>
    using remote_ptr = T* [[region::remote]];
}
```

## Examples

### Example 1: Basic Region Allocation

```cpp
#include <memory_resource>
#include <vector>

void example_basic_allocation() {
    // Allocate in different regions
    std::pmr::region_memory_resource local_mem(std::pmr::memory_region::local);
    std::pmr::region_memory_resource middle_mem(std::pmr::memory_region::middle);
    std::pmr::region_memory_resource remote_mem(std::pmr::memory_region::remote);
    
    // Local region vector - full coherency
    std::pmr::vector<int> local_data(&local_mem);
    local_data.resize(1000);
    
    // Middle region vector - relaxed coherency
    std::pmr::vector<int> middle_data(&middle_mem);
    middle_data.resize(10000);
    
    // Remote region vector - eventual consistency
    std::pmr::vector<int> remote_data(&remote_mem);
    remote_data.resize(1000000);
}
```

### Example 2: Cross-Region Data Transfer

```cpp
void example_cross_region_transfer() {
    std::pmr::region_memory_resource local_mem(std::pmr::memory_region::local);
    std::pmr::region_memory_resource remote_mem(std::pmr::memory_region::remote);
    
    // Allocate buffers
    auto* local_buffer = static_cast<int*>(local_mem.allocate(4096 * sizeof(int)));
    auto* remote_buffer = static_cast<int*>(remote_mem.allocate(4096 * sizeof(int)));
    
    // Initialize local data
    std::fill_n(local_buffer, 4096, 42);
    
    // Asynchronous transfer to remote memory
    auto future = std::pmr::async_put(remote_buffer, local_buffer, 4096 * sizeof(int));
    
    // Do other work...
    
    // Wait for transfer completion
    future.wait();
    
    // Explicit flush for remote memory
    std::pmr::flush_remote(remote_buffer, 4096 * sizeof(int));
}
```

### Example 3: CXL Memory with Bias Control

```cpp
void example_cxl_bias_control() {
    std::pmr::region_memory_resource middle_mem(std::pmr::memory_region::middle);
    
    // Allocate CXL memory
    constexpr size_t size = 1024 * 1024; // 1MB
    auto* buffer = static_cast<char*>(middle_mem.allocate(size));
    
    // Set device bias for initial data loading
    std::pmr::set_bias_mode(buffer, size, std::pmr::bias_mode::device);
    
    // Device performs initial data processing...
    
    // Switch to host bias for CPU processing
    std::pmr::set_bias_mode(buffer, size, std::pmr::bias_mode::host);
    
    // CPU processes data
    std::transform(buffer, buffer + size, buffer, [](char c) { return std::toupper(c); });
    
    // Ensure coherency before device access
    std::pmr::coherence_barrier(std::pmr::memory_region::middle);
}
```

### Example 4: Memory Pool with Mixed Regions

```cpp
class heterogeneous_memory_pool {
    std::pmr::region_memory_resource local_resource{std::pmr::memory_region::local};
    std::pmr::region_memory_resource middle_resource{std::pmr::memory_region::middle};
    std::pmr::region_memory_resource remote_resource{std::pmr::memory_region::remote};
    
public:
    template<typename T>
    T* allocate(size_t n, std::pmr::memory_region region) {
        std::pmr::memory_resource* resource = nullptr;
        
        switch (region) {
            case std::pmr::memory_region::local:
                resource = &local_resource;
                break;
            case std::pmr::memory_region::middle:
                resource = &middle_resource;
                break;
            case std::pmr::memory_region::remote:
                resource = &remote_resource;
                break;
        }
        
        return static_cast<T*>(resource->allocate(n * sizeof(T), alignof(T)));
    }
    
    template<typename T>
    void deallocate(T* ptr, size_t n, std::pmr::memory_region region) {
        // Implementation...
    }
};
```

## Impact on the Standard

### Library Impact

1. **Header `<memory_resource>`**: Add region-based memory resource classes
2. **Header `<memory>`**: Add region-aware allocators and pointer annotations
3. **Header `<atomic>`**: Specify atomic operation support per region
4. **New header `<region_memory>`**: Region-specific utilities and synchronization primitives

### Language Impact

1. **Attributes**: New `[[region::*]]` attributes for pointer annotations
2. **Type System**: Extended type traits for region-aware programming
3. **Memory Model**: Extensions to the C++ memory model for region semantics

### Backward Compatibility

All existing code continues to work unchanged. The default behavior assumes local region semantics, maintaining full compatibility with current programs.

## Implementation Experience

### Prototype Implementation

A prototype implementation has been developed using:
- Modified LLVM/Clang for attribute support
- Custom allocators wrapping CXL memory APIs
- Runtime library for synchronization primitives

### Performance Results

Based on the CXLMemUring evaluation:
- Local region: Baseline performance (1.0x)
- Middle region: 1.5-3x latency, 80% bandwidth of local
- Remote region: 5-10x latency, 40% bandwidth of local

### Lessons Learned

1. **Explicit region specification** improves optimization opportunities
2. **Bias control** is critical for CXL memory performance
3. **Bulk transfers** are essential for remote memory efficiency

## Acknowledgments

This proposal builds upon the work presented in "CXLMemUring: A Hardware Software Co-design Paradigm for Asynchronous and Flexible Parallel CXL Memory Pool Access" by Yiwei Yang.

## References

[1] Yang, Y. (2023). CXLMemUring: A Hardware Software Co-design Paradigm for Asynchronous and Flexible Parallel CXL Memory Pool Access. ACM Conference.

[2] Williams, S., Waterman, A., & Patterson, D. (2009). Roofline: an insightful visual performance model for multicore architectures. Communications of the ACM, 52(4), 65-76.

[3] CXL Consortium. (2024). Compute Express Link Specification 3.1.

[4] ISO/IEC 14882:2023. Programming Languages — C++.

[5] P0443R14. A Unified Executors Proposal for C++.

[6] P1068R5. Vector Extensions for C++.

[7] P2300R7. std::execution.

[8] N4950. Working Draft, Standard for Programming Language C++.