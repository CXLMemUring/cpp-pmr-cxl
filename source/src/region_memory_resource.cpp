#include <pmr/region_memory.hpp>
#include <cstdlib>
#include <new>
#include <atomic>
#include <thread>
#include <immintrin.h>

#ifdef __linux__
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace std::pmr {

// Anonymous namespace for implementation details
namespace {

// Platform-specific memory allocation
void* allocate_region_memory(size_t bytes, size_t alignment, memory_region region) {
    void* ptr = nullptr;
    
    switch (region) {
        case memory_region::local: {
            // Use aligned allocation for local memory
            if (alignment > alignof(std::max_align_t)) {
                ptr = std::aligned_alloc(alignment, bytes);
            } else {
                ptr = std::malloc(bytes);
            }
            break;
        }
        
        case memory_region::middle: {
            // Simulate CXL memory allocation
            // In real implementation, this would use CXL-specific APIs
#ifdef __linux__
            // Use huge pages for middle region to simulate CXL characteristics
            ptr = mmap(nullptr, bytes, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
            if (ptr == MAP_FAILED) {
                // Fallback to regular allocation
                ptr = std::aligned_alloc(alignment, bytes);
            }
#else
            ptr = std::aligned_alloc(alignment, bytes);
#endif
            break;
        }
        
        case memory_region::remote: {
            // Simulate remote memory allocation
            // In real implementation, this would use RDMA or similar APIs
#ifdef __linux__
            // Use shared memory for remote region simulation
            int fd = shm_open("/pmr_remote_memory", O_CREAT | O_RDWR, 0666);
            if (fd != -1) {
                ftruncate(fd, bytes);
                ptr = mmap(nullptr, bytes, PROT_READ | PROT_WRITE,
                          MAP_SHARED, fd, 0);
                close(fd);
                if (ptr == MAP_FAILED) {
                    ptr = nullptr;
                }
            }
#endif
            if (!ptr) {
                // Fallback
                ptr = std::aligned_alloc(alignment, bytes);
            }
            break;
        }
    }
    
    if (!ptr) {
        throw std::bad_alloc();
    }
    
    return ptr;
}

void deallocate_region_memory(void* ptr, size_t bytes, memory_region region) {
    if (!ptr) return;
    
    switch (region) {
        case memory_region::local:
            std::free(ptr);
            break;
            
        case memory_region::middle:
#ifdef __linux__
            // Try munmap first in case it was mmap'd
            if (munmap(ptr, bytes) != 0) {
                std::free(ptr);
            }
#else
            std::free(ptr);
#endif
            break;
            
        case memory_region::remote:
#ifdef __linux__
            if (munmap(ptr, bytes) != 0) {
                std::free(ptr);
            }
#else
            std::free(ptr);
#endif
            break;
    }
}

} // anonymous namespace

// region_memory_resource implementation
region_memory_resource::region_memory_resource(memory_region region) noexcept
    : region_(region) {}

coherency_model region_memory_resource::get_coherency_model() const noexcept {
    switch (region_) {
        case memory_region::local:
            return region_traits<memory_region::local>::coherency;
        case memory_region::middle:
            return region_traits<memory_region::middle>::coherency;
        case memory_region::remote:
            return region_traits<memory_region::remote>::coherency;
    }
    return coherency_model::strict; // Default
}

consistency_model region_memory_resource::get_consistency_model() const noexcept {
    switch (region_) {
        case memory_region::local:
            return region_traits<memory_region::local>::consistency;
        case memory_region::middle:
            return region_traits<memory_region::middle>::consistency;
        case memory_region::remote:
            return region_traits<memory_region::remote>::consistency;
    }
    return consistency_model::sequential; // Default
}

// Concrete implementations for each region
class local_memory_resource final : public region_memory_resource {
public:
    local_memory_resource() : region_memory_resource(memory_region::local) {}
    
protected:
    void* do_allocate(size_t bytes, size_t alignment) override {
        return allocate_region_memory(bytes, alignment, memory_region::local);
    }
    
    void do_deallocate(void* p, size_t bytes, size_t alignment) override {
        deallocate_region_memory(p, bytes, memory_region::local);
    }
    
    bool do_is_equal(const memory_resource& other) const noexcept override {
        return this == &other;
    }
};

class middle_memory_resource final : public region_memory_resource {
public:
    middle_memory_resource() : region_memory_resource(memory_region::middle) {}
    
protected:
    void* do_allocate(size_t bytes, size_t alignment) override {
        return allocate_region_memory(bytes, alignment, memory_region::middle);
    }
    
    void do_deallocate(void* p, size_t bytes, size_t alignment) override {
        deallocate_region_memory(p, bytes, memory_region::middle);
    }
    
    bool do_is_equal(const memory_resource& other) const noexcept override {
        return this == &other;
    }
};

class remote_memory_resource final : public region_memory_resource {
public:
    remote_memory_resource() : region_memory_resource(memory_region::remote) {}
    
protected:
    void* do_allocate(size_t bytes, size_t alignment) override {
        return allocate_region_memory(bytes, alignment, memory_region::remote);
    }
    
    void do_deallocate(void* p, size_t bytes, size_t alignment) override {
        deallocate_region_memory(p, bytes, memory_region::remote);
    }
    
    bool do_is_equal(const memory_resource& other) const noexcept override {
        return this == &other;
    }
};

// Global memory resources
static local_memory_resource local_resource;
static middle_memory_resource middle_resource;
static remote_memory_resource remote_resource;

// Get memory resource for a specific region
memory_resource* get_memory_resource(memory_region region) {
    switch (region) {
        case memory_region::local:
            return &local_resource;
        case memory_region::middle:
            return &middle_resource;
        case memory_region::remote:
            return &remote_resource;
    }
    return &local_resource; // Default
}

// Synchronization primitives implementation
void coherence_barrier(memory_region region) {
    switch (region) {
        case memory_region::local:
            // Full memory barrier for local region
            std::atomic_thread_fence(std::memory_order_seq_cst);
            _mm_mfence();
            break;
            
        case memory_region::middle:
            // Lighter barrier for middle region
            std::atomic_thread_fence(std::memory_order_acq_rel);
            _mm_sfence();
            break;
            
        case memory_region::remote:
            // Explicit flush required for remote region
            // In real implementation, this would trigger CXL flush
            std::atomic_thread_fence(std::memory_order_release);
            break;
    }
}

void memory_fence(memory_region region, std::memory_order order) {
    // Apply appropriate fence based on region and ordering
    std::atomic_thread_fence(order);
    
    if (region == memory_region::local || 
        (region == memory_region::middle && order >= std::memory_order_acq_rel)) {
        _mm_mfence();
    }
}

// Bias control implementation (stub)
void set_bias_mode(void* ptr, size_t size, bias_mode mode) {
    // In real implementation, this would interact with CXL hardware
    // to set the bias mode for the specified memory range
    
    // For now, just ensure the memory is accessible
    if (mode != bias_mode::none) {
        volatile char* p = static_cast<volatile char*>(ptr);
        for (size_t i = 0; i < size; i += 4096) {
            p[i] = p[i]; // Touch pages
        }
    }
}

bias_mode get_bias_mode(const void* ptr) {
    // In real implementation, query CXL hardware
    return bias_mode::none;
}

// Remote memory flush
void flush_remote(void* ptr, size_t size) {
    // Ensure all writes to remote memory are visible
    memory_fence(memory_region::remote, std::memory_order_release);
    
    // In real implementation, this would trigger explicit flush
    // For now, use cache line flush instructions
    char* p = static_cast<char*>(ptr);
    for (size_t i = 0; i < size; i += 64) {
        _mm_clflush(p + i);
    }
    
    _mm_mfence();
}

} // namespace std::pmr