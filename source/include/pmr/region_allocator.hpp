#ifndef PMR_REGION_ALLOCATOR_HPP
#define PMR_REGION_ALLOCATOR_HPP

#include "region_memory.hpp"
#include <memory>
#include <utility>

namespace std::pmr {

// Region-aware allocator
template<typename T>
class region_allocator {
public:
    using value_type = T;
    using pointer = region_ptr<T>;
    using const_pointer = region_ptr<const T>;
    using reference = T&;
    using const_reference = const T&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    
    template<typename U>
    struct rebind {
        using other = region_allocator<U>;
    };
    
    region_allocator() noexcept = default;
    
    explicit region_allocator(region_memory_resource* resource) noexcept
        : resource_(resource) {}
    
    template<typename U>
    region_allocator(const region_allocator<U>& other) noexcept
        : resource_(other.resource()) {}
    
    region_memory_resource* resource() const noexcept { return resource_; }
    
    [[nodiscard]] pointer allocate(size_type n) {
        if (n > std::numeric_limits<size_type>::max() / sizeof(T)) {
            throw std::bad_array_new_length();
        }
        
        void* p = resource_->allocate(n * sizeof(T), alignof(T));
        return pointer(static_cast<T*>(p), resource_->get_region());
    }
    
    void deallocate(pointer p, size_type n) noexcept {
        resource_->deallocate(p.get(), n * sizeof(T), alignof(T));
    }
    
    template<typename U, typename... Args>
    void construct(U* p, Args&&... args) {
        ::new(static_cast<void*>(p)) U(std::forward<Args>(args)...);
    }
    
    template<typename U>
    void destroy(U* p) {
        p->~U();
    }
    
    bool operator==(const region_allocator& other) const noexcept {
        return resource_ == other.resource_;
    }
    
    bool operator!=(const region_allocator& other) const noexcept {
        return !(*this == other);
    }
    
private:
    region_memory_resource* resource_ = get_default_resource();
};

// Specialization for void
template<>
class region_allocator<void> {
public:
    using value_type = void;
    using pointer = void*;
    using const_pointer = const void*;
    
    template<typename U>
    struct rebind {
        using other = region_allocator<U>;
    };
    
    region_allocator() noexcept = default;
    explicit region_allocator(region_memory_resource* resource) noexcept
        : resource_(resource) {}
    
    template<typename U>
    region_allocator(const region_allocator<U>& other) noexcept
        : resource_(other.resource()) {}
    
    region_memory_resource* resource() const noexcept { return resource_; }
    
private:
    region_memory_resource* resource_ = get_default_resource();
};

// Helper functions for creating region allocators
template<typename T>
region_allocator<T> make_local_allocator() {
    static thread_local region_memory_resource local_resource(memory_region::local);
    return region_allocator<T>(&local_resource);
}

template<typename T>
region_allocator<T> make_middle_allocator() {
    static thread_local region_memory_resource middle_resource(memory_region::middle);
    return region_allocator<T>(&middle_resource);
}

template<typename T>
region_allocator<T> make_remote_allocator() {
    static thread_local region_memory_resource remote_resource(memory_region::remote);
    return region_allocator<T>(&remote_resource);
}

} // namespace std::pmr

#endif // PMR_REGION_ALLOCATOR_HPP