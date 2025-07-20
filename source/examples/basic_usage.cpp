#include <pmr/region_memory.hpp>
#include <pmr/region_allocator.hpp>
#include <vector>
#include <iostream>
#include <chrono>

using namespace std::pmr;
using namespace std::chrono;

void benchmark_region(memory_region region, const char* region_name) {
    // Create memory resource for the region
    std::unique_ptr<region_memory_resource> resource;
    
    switch (region) {
        case memory_region::local:
            resource = std::make_unique<region_memory_resource>(memory_region::local);
            break;
        case memory_region::middle:
            resource = std::make_unique<region_memory_resource>(memory_region::middle);
            break;
        case memory_region::remote:
            resource = std::make_unique<region_memory_resource>(memory_region::remote);
            break;
    }
    
    std::cout << "\nBenchmarking " << region_name << " region:\n";
    std::cout << "Coherency model: ";
    switch (resource->get_coherency_model()) {
        case coherency_model::strict: std::cout << "strict\n"; break;
        case coherency_model::relaxed: std::cout << "relaxed\n"; break;
        case coherency_model::eventual: std::cout << "eventual\n"; break;
    }
    
    std::cout << "Consistency model: ";
    switch (resource->get_consistency_model()) {
        case consistency_model::sequential: std::cout << "sequential\n"; break;
        case consistency_model::acquire_release: std::cout << "acquire_release\n"; break;
        case consistency_model::relaxed: std::cout << "relaxed\n"; break;
    }
    
    // Benchmark allocation
    const size_t size = 1024 * 1024; // 1MB
    auto start = high_resolution_clock::now();
    void* ptr = resource->allocate(size, 64);
    auto end = high_resolution_clock::now();
    
    std::cout << "Allocation time: " 
              << duration_cast<microseconds>(end - start).count() 
              << " μs\n";
    
    // Benchmark write
    start = high_resolution_clock::now();
    std::memset(ptr, 0x42, size);
    coherence_barrier(region);
    end = high_resolution_clock::now();
    
    std::cout << "Write time: " 
              << duration_cast<microseconds>(end - start).count() 
              << " μs\n";
    
    // Benchmark read
    volatile int sum = 0;
    start = high_resolution_clock::now();
    char* data = static_cast<char*>(ptr);
    for (size_t i = 0; i < size; i += 64) {
        sum += data[i];
    }
    end = high_resolution_clock::now();
    
    std::cout << "Read time: " 
              << duration_cast<microseconds>(end - start).count() 
              << " μs\n";
    
    // Calculate bandwidth
    double write_bw = (size / 1024.0 / 1024.0) / 
                      (duration_cast<microseconds>(end - start).count() / 1e6);
    std::cout << "Read bandwidth: " << write_bw << " MB/s\n";
    
    resource->deallocate(ptr, size, 64);
}

void demonstrate_bias_control() {
    std::cout << "\n--- CXL Bias Control Demo ---\n";
    
    region_memory_resource middle_resource(memory_region::middle);
    const size_t size = 4096 * 256; // 1MB
    
    void* buffer = middle_resource.allocate(size, 4096);
    
    // Set device bias for initialization
    std::cout << "Setting device bias for initialization...\n";
    set_bias_mode(buffer, size, bias_mode::device);
    
    // Simulate device writing data
    std::memset(buffer, 0xAA, size);
    
    // Switch to host bias for processing
    std::cout << "Switching to host bias for CPU processing...\n";
    set_bias_mode(buffer, size, bias_mode::host);
    
    // CPU processes data
    char* data = static_cast<char*>(buffer);
    for (size_t i = 0; i < size; ++i) {
        data[i] = ~data[i]; // Invert bits
    }
    
    coherence_barrier(memory_region::middle);
    
    std::cout << "Data processing completed with bias control\n";
    
    middle_resource.deallocate(buffer, size, 4096);
}

void demonstrate_mixed_allocation() {
    std::cout << "\n--- Mixed Region Allocation Demo ---\n";
    
    // Create allocators for different regions
    auto local_alloc = make_local_allocator<int>();
    auto middle_alloc = make_middle_allocator<int>();
    auto remote_alloc = make_remote_allocator<int>();
    
    // Create vectors in different regions
    std::vector<int, region_allocator<int>> local_vec(local_alloc);
    std::vector<int, region_allocator<int>> middle_vec(middle_alloc);
    std::vector<int, region_allocator<int>> remote_vec(remote_alloc);
    
    // Populate vectors
    const size_t count = 1000;
    
    std::cout << "Populating local vector...\n";
    for (size_t i = 0; i < count; ++i) {
        local_vec.push_back(i);
    }
    
    std::cout << "Populating middle vector...\n";
    for (size_t i = 0; i < count; ++i) {
        middle_vec.push_back(i * 2);
    }
    
    std::cout << "Populating remote vector...\n";
    for (size_t i = 0; i < count; ++i) {
        remote_vec.push_back(i * 3);
    }
    
    // Ensure coherency
    coherence_barrier(memory_region::local);
    coherence_barrier(memory_region::middle);
    flush_remote(remote_vec.data(), remote_vec.size() * sizeof(int));
    
    std::cout << "All vectors populated successfully\n";
    std::cout << "Local vector size: " << local_vec.size() << "\n";
    std::cout << "Middle vector size: " << middle_vec.size() << "\n";
    std::cout << "Remote vector size: " << remote_vec.size() << "\n";
}

int main() {
    std::cout << "=== CXL Region Memory Model Demo ===\n";
    
    // Benchmark each region
    benchmark_region(memory_region::local, "Local");
    benchmark_region(memory_region::middle, "Middle (CXL)");
    benchmark_region(memory_region::remote, "Remote");
    
    // Demonstrate CXL-specific features
    if (region_traits<memory_region::middle>::supports_bias_mode) {
        demonstrate_bias_control();
    }
    
    // Demonstrate mixed allocation
    demonstrate_mixed_allocation();
    
    return 0;
}