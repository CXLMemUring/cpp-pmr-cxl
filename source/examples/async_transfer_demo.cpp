#include <pmr/region_memory.hpp>
#include <pmr/async_transfer.hpp>
#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <iomanip>

using namespace std::pmr;
using namespace std::chrono;

void demonstrate_async_transfer() {
    std::cout << "\n--- Async Transfer Demo ---\n";
    
    // Allocate memory in different regions
    region_memory_resource local_resource(memory_region::local);
    region_memory_resource remote_resource(memory_region::remote);
    
    const size_t size = 10 * 1024 * 1024; // 10MB
    void* local_buffer = local_resource.allocate(size, 4096);
    void* remote_buffer = remote_resource.allocate(size, 4096);
    
    // Initialize local buffer with random data
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    char* local_data = static_cast<char*>(local_buffer);
    for (size_t i = 0; i < size; ++i) {
        local_data[i] = dis(gen);
    }
    
    // Async PUT operation
    std::cout << "Starting async PUT (local -> remote)...\n";
    auto put_start = high_resolution_clock::now();
    auto put_handle = async_put(remote_buffer, local_buffer, size);
    
    // Do some work while transfer is in progress
    std::cout << "Doing other work while transfer is in progress...\n";
    std::this_thread::sleep_for(milliseconds(10));
    
    // Wait for transfer to complete
    put_handle.wait();
    auto put_end = high_resolution_clock::now();
    
    double put_time = duration_cast<microseconds>(put_end - put_start).count() / 1e6;
    double put_bandwidth = (size / 1024.0 / 1024.0) / put_time;
    std::cout << "PUT completed: " << std::fixed << std::setprecision(2) 
              << put_bandwidth << " MB/s\n";
    
    // Clear local buffer
    std::memset(local_buffer, 0, size);
    
    // Async GET operation
    std::cout << "\nStarting async GET (remote -> local)...\n";
    auto get_start = high_resolution_clock::now();
    auto get_handle = async_get(local_buffer, remote_buffer, size);
    
    // Check if transfer is ready (non-blocking)
    if (!get_handle.is_ready()) {
        std::cout << "Transfer still in progress...\n";
    }
    
    get_handle.wait();
    auto get_end = high_resolution_clock::now();
    
    double get_time = duration_cast<microseconds>(get_end - get_start).count() / 1e6;
    double get_bandwidth = (size / 1024.0 / 1024.0) / get_time;
    std::cout << "GET completed: " << std::fixed << std::setprecision(2) 
              << get_bandwidth << " MB/s\n";
    
    // Verify data integrity
    bool data_valid = true;
    for (size_t i = 0; i < size; ++i) {
        if (local_data[i] != static_cast<char*>(remote_buffer)[i]) {
            data_valid = false;
            break;
        }
    }
    std::cout << "Data integrity: " << (data_valid ? "PASS" : "FAIL") << "\n";
    
    local_resource.deallocate(local_buffer, size, 4096);
    remote_resource.deallocate(remote_buffer, size, 4096);
}

void demonstrate_scatter_gather() {
    std::cout << "\n--- Scatter-Gather Transfer Demo ---\n";
    
    region_memory_resource local_resource(memory_region::local);
    region_memory_resource remote_resource(memory_region::remote);
    
    // Create multiple buffers
    const size_t num_buffers = 4;
    const size_t buffer_size = 1024 * 1024; // 1MB each
    
    std::vector<iovec> local_iovs;
    std::vector<iovec> remote_iovs;
    
    // Allocate local buffers
    for (size_t i = 0; i < num_buffers; ++i) {
        void* ptr = local_resource.allocate(buffer_size, 64);
        local_iovs.push_back({ptr, buffer_size});
        
        // Initialize with pattern
        std::memset(ptr, i + 1, buffer_size);
    }
    
    // Allocate remote buffers
    for (size_t i = 0; i < num_buffers; ++i) {
        void* ptr = remote_resource.allocate(buffer_size, 64);
        remote_iovs.push_back({ptr, buffer_size});
    }
    
    // Scatter-gather PUT
    std::cout << "Starting scatter-gather PUT...\n";
    auto sg_start = high_resolution_clock::now();
    auto sg_handle = async_putv(remote_iovs.data(), remote_iovs.size(),
                                local_iovs.data(), local_iovs.size());
    sg_handle.wait();
    auto sg_end = high_resolution_clock::now();
    
    double sg_time = duration_cast<microseconds>(sg_end - sg_start).count() / 1e6;
    double sg_bandwidth = (num_buffers * buffer_size / 1024.0 / 1024.0) / sg_time;
    std::cout << "Scatter-gather completed: " << std::fixed << std::setprecision(2) 
              << sg_bandwidth << " MB/s\n";
    
    // Cleanup
    for (auto& iov : local_iovs) {
        local_resource.deallocate(iov.base, iov.len, 64);
    }
    for (auto& iov : remote_iovs) {
        remote_resource.deallocate(iov.base, iov.len, 64);
    }
}

void demonstrate_callback_transfers() {
    std::cout << "\n--- Callback-based Transfer Demo ---\n";
    
    region_memory_resource local_resource(memory_region::local);
    region_memory_resource remote_resource(memory_region::remote);
    
    const size_t size = 5 * 1024 * 1024; // 5MB
    void* local_buffer = local_resource.allocate(size, 4096);
    void* remote_buffer = remote_resource.allocate(size, 4096);
    
    // Initialize data
    std::memset(local_buffer, 0x55, size);
    
    // Transfer with callback
    std::atomic<bool> transfer_done{false};
    transfer_status final_status = transfer_status::pending;
    
    std::cout << "Starting transfer with callback...\n";
    auto handle = async_put_with_callback(
        remote_buffer, local_buffer, size,
        [&](transfer_status status) {
            final_status = status;
            transfer_done.store(true);
            std::cout << "Callback invoked - Transfer status: ";
            switch (status) {
                case transfer_status::completed:
                    std::cout << "COMPLETED\n";
                    break;
                case transfer_status::failed:
                    std::cout << "FAILED\n";
                    break;
                default:
                    std::cout << "UNKNOWN\n";
            }
        }
    );
    
    // Poll for completion
    while (!transfer_done.load()) {
        std::cout << "." << std::flush;
        std::this_thread::sleep_for(milliseconds(100));
    }
    
    handle.wait();
    std::cout << "\nTransfer finished\n";
    
    local_resource.deallocate(local_buffer, size, 4096);
    remote_resource.deallocate(remote_buffer, size, 4096);
}

void demonstrate_zero_copy() {
    std::cout << "\n--- Zero-Copy Transfer Demo ---\n";
    
    region_memory_resource middle_resource(memory_region::middle);
    region_memory_resource remote_resource(memory_region::remote);
    
    const size_t size = 8 * 1024 * 1024; // 8MB
    void* middle_buffer = middle_resource.allocate(size, 4096);
    void* remote_buffer = remote_resource.allocate(size, 4096);
    
    // Register memory for zero-copy
    std::cout << "Registering memory regions...\n";
    memory_registration middle_reg(middle_buffer, size, memory_region::middle);
    memory_registration remote_reg(remote_buffer, size, memory_region::remote);
    
    // Initialize middle buffer
    std::memset(middle_buffer, 0x33, size);
    coherence_barrier(memory_region::middle);
    
    // Zero-copy transfer
    std::cout << "Starting zero-copy transfer...\n";
    auto zc_start = high_resolution_clock::now();
    auto zc_handle = zero_copy_transfer(remote_reg, middle_reg);
    zc_handle.wait();
    auto zc_end = high_resolution_clock::now();
    
    double zc_time = duration_cast<microseconds>(zc_end - zc_start).count() / 1e6;
    double zc_bandwidth = (size / 1024.0 / 1024.0) / zc_time;
    std::cout << "Zero-copy transfer completed: " << std::fixed << std::setprecision(2) 
              << zc_bandwidth << " MB/s\n";
    
    middle_resource.deallocate(middle_buffer, size, 4096);
    remote_resource.deallocate(remote_buffer, size, 4096);
}

int main() {
    std::cout << "=== CXL Async Transfer Demo ===\n";
    
    demonstrate_async_transfer();
    demonstrate_scatter_gather();
    demonstrate_callback_transfers();
    demonstrate_zero_copy();
    
    return 0;
}