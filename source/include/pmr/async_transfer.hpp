#ifndef PMR_ASYNC_TRANSFER_HPP
#define PMR_ASYNC_TRANSFER_HPP

#include "region_memory.hpp"
#include <future>
#include <chrono>
#include <functional>

namespace std::pmr {

// Transfer status
enum class transfer_status {
    pending,
    in_progress,
    completed,
    failed
};

// Transfer handle for tracking async operations
class transfer_handle {
public:
    transfer_handle() = default;
    explicit transfer_handle(std::future<void>&& future) 
        : future_(std::move(future)) {}
    
    void wait() { future_.wait(); }
    
    template<typename Rep, typename Period>
    std::future_status wait_for(const std::chrono::duration<Rep, Period>& timeout) {
        return future_.wait_for(timeout);
    }
    
    bool is_ready() const {
        return future_.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    }
    
private:
    std::future<void> future_;
};

// Asynchronous transfer operations
transfer_handle async_get(void* local_dst, const void* remote_src, size_t size);
transfer_handle async_put(void* remote_dst, const void* local_src, size_t size);

// Bulk transfer operations with callback
using transfer_callback = std::function<void(transfer_status)>;

transfer_handle async_get_with_callback(
    void* local_dst, 
    const void* remote_src, 
    size_t size,
    transfer_callback callback
);

transfer_handle async_put_with_callback(
    void* remote_dst, 
    const void* local_src, 
    size_t size,
    transfer_callback callback
);

// Scatter-gather operations
struct iovec {
    void* base;
    size_t len;
};

transfer_handle async_getv(
    const iovec* local_iov,
    int local_iovcnt,
    const iovec* remote_iov,
    int remote_iovcnt
);

transfer_handle async_putv(
    const iovec* remote_iov,
    int remote_iovcnt,
    const iovec* local_iov,
    int local_iovcnt
);

// DMA-style transfer for aligned memory
transfer_handle dma_transfer(
    void* dst,
    const void* src,
    size_t size,
    memory_region dst_region,
    memory_region src_region
);

// Prefetch hints for remote memory
void prefetch_remote(const void* addr, size_t size, int locality = 3);

// Memory registration for zero-copy transfers
class memory_registration {
public:
    memory_registration(void* addr, size_t size, memory_region region);
    ~memory_registration();
    
    memory_registration(const memory_registration&) = delete;
    memory_registration& operator=(const memory_registration&) = delete;
    
    memory_registration(memory_registration&& other) noexcept;
    memory_registration& operator=(memory_registration&& other) noexcept;
    
    void* get_address() const noexcept { return addr_; }
    size_t get_size() const noexcept { return size_; }
    memory_region get_region() const noexcept { return region_; }
    
private:
    void* addr_ = nullptr;
    size_t size_ = 0;
    memory_region region_ = memory_region::local;
    void* handle_ = nullptr; // Platform-specific handle
};

// Zero-copy transfer using registered memory
transfer_handle zero_copy_transfer(
    const memory_registration& dst,
    const memory_registration& src,
    size_t offset_dst = 0,
    size_t offset_src = 0,
    size_t size = 0
);

} // namespace std::pmr

#endif // PMR_ASYNC_TRANSFER_HPP