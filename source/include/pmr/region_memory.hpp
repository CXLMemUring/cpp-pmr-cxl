#ifndef PMR_REGION_MEMORY_HPP
#define PMR_REGION_MEMORY_HPP

#include <memory_resource>
#include <atomic>
#include <cstddef>
#include <type_traits>

namespace std::pmr {

// Memory region enumeration
enum class memory_region {
    local,   // CPU-attached memory
    middle,  // CXL-attached memory
    remote   // Remote pooled memory
};

// Coherency models
enum class coherency_model {
    strict,      // Full cache coherency
    relaxed,     // Relaxed coherency with explicit synchronization
    eventual     // Eventual consistency
};

// Consistency models
enum class consistency_model {
    sequential,      // Sequential consistency
    acquire_release, // Acquire-release semantics
    relaxed         // Relaxed ordering
};

// Bias modes for CXL memory
enum class bias_mode {
    none,
    host,
    device
};

// Forward declarations
template<memory_region Region>
struct region_traits;

// Region traits specializations
template<>
struct region_traits<memory_region::local> {
    static constexpr coherency_model coherency = coherency_model::strict;
    static constexpr consistency_model consistency = consistency_model::sequential;
    static constexpr size_t coherency_granularity = 64; // Cache line size
    static constexpr bool supports_atomic_operations = true;
    static constexpr bool supports_bias_mode = false;
    static constexpr bool supports_direct_cache_placement = true;
    static constexpr bool requires_explicit_flush = false;
    static constexpr size_t minimum_transfer_size = 1;
};

template<>
struct region_traits<memory_region::middle> {
    static constexpr coherency_model coherency = coherency_model::relaxed;
    static constexpr consistency_model consistency = consistency_model::acquire_release;
    static constexpr size_t coherency_granularity = 256; // CXL flit size
    static constexpr bool supports_atomic_operations = true;
    static constexpr bool supports_bias_mode = true;
    static constexpr bool supports_direct_cache_placement = true;
    static constexpr bool requires_explicit_flush = false;
    static constexpr size_t minimum_transfer_size = 64;
};

template<>
struct region_traits<memory_region::remote> {
    static constexpr coherency_model coherency = coherency_model::eventual;
    static constexpr consistency_model consistency = consistency_model::relaxed;
    static constexpr size_t coherency_granularity = 4096; // Page granularity
    static constexpr bool supports_atomic_operations = false;
    static constexpr bool supports_bias_mode = false;
    static constexpr bool supports_direct_cache_placement = false;
    static constexpr bool requires_explicit_flush = true;
    static constexpr size_t minimum_transfer_size = 4096;
};

// Base class for region-aware memory resources
class region_memory_resource : public memory_resource {
public:
    explicit region_memory_resource(memory_region region) noexcept;
    virtual ~region_memory_resource() = default;
    
    memory_region get_region() const noexcept { return region_; }
    coherency_model get_coherency_model() const noexcept;
    consistency_model get_consistency_model() const noexcept;
    
protected:
    memory_region region_;
};

// Synchronization primitives
void coherence_barrier(memory_region region);
void memory_fence(memory_region region, std::memory_order order = std::memory_order_seq_cst);

// Bias control for CXL memory
void set_bias_mode(void* ptr, size_t size, bias_mode mode);
bias_mode get_bias_mode(const void* ptr);

// Explicit flush for remote memory
void flush_remote(void* ptr, size_t size);

// Region-aware pointer annotations
template<typename T>
struct region_ptr {
    using element_type = T;
    using pointer = T*;
    using reference = T&;
    
    region_ptr() noexcept = default;
    explicit region_ptr(T* ptr, memory_region region) noexcept 
        : ptr_(ptr), region_(region) {}
    
    T* get() const noexcept { return ptr_; }
    memory_region get_region() const noexcept { return region_; }
    
    T& operator*() const { return *ptr_; }
    T* operator->() const noexcept { return ptr_; }
    
    explicit operator bool() const noexcept { return ptr_ != nullptr; }
    
private:
    T* ptr_ = nullptr;
    memory_region region_ = memory_region::local;
};

// Helper type aliases
template<typename T>
using local_ptr = region_ptr<T>;

template<typename T>
using middle_ptr = region_ptr<T>;

template<typename T>
using remote_ptr = region_ptr<T>;

} // namespace std::pmr

#endif // PMR_REGION_MEMORY_HPP