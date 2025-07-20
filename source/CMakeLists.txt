cmake_minimum_required(VERSION 3.14)
project(cxl_pmr_memory VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(BUILD_EXAMPLES "Build example programs" ON)
option(BUILD_TESTS "Build test programs" ON)
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)

# Compiler flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    add_compile_options(-march=native) # For CPU-specific optimizations
    
    if(ENABLE_ASAN)
        add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address)
    endif()
    
    if(ENABLE_TSAN)
        add_compile_options(-fsanitize=thread)
        add_link_options(-fsanitize=thread)
    endif()
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Library sources
set(LIB_SOURCES
    src/region_memory_resource.cpp
    src/async_transfer.cpp
)

# Create static library
add_library(cxl_pmr_static STATIC ${LIB_SOURCES})
target_include_directories(cxl_pmr_static PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Create shared library
add_library(cxl_pmr_shared SHARED ${LIB_SOURCES})
target_include_directories(cxl_pmr_shared PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Link libraries
if(UNIX AND NOT APPLE)
    target_link_libraries(cxl_pmr_static PUBLIC pthread rt)
    target_link_libraries(cxl_pmr_shared PUBLIC pthread rt)
else()
    target_link_libraries(cxl_pmr_static PUBLIC pthread)
    target_link_libraries(cxl_pmr_shared PUBLIC pthread)
endif()

# Set library properties
set_target_properties(cxl_pmr_static PROPERTIES OUTPUT_NAME cxl_pmr)
set_target_properties(cxl_pmr_shared PROPERTIES OUTPUT_NAME cxl_pmr)

# Examples
if(BUILD_EXAMPLES)
    add_executable(basic_usage examples/basic_usage.cpp)
    target_link_libraries(basic_usage PRIVATE cxl_pmr_static)
    
    add_executable(async_transfer_demo examples/async_transfer_demo.cpp)
    target_link_libraries(async_transfer_demo PRIVATE cxl_pmr_static)
endif()

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation
install(TARGETS cxl_pmr_static cxl_pmr_shared
    EXPORT cxl_pmr_targets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/pmr DESTINATION include)

# Package configuration
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/cxl_pmr_config_version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cxl_pmr_config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cxl_pmr_config.cmake"
    INSTALL_DESTINATION lib/cmake/cxl_pmr
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/cxl_pmr_config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/cxl_pmr_config_version.cmake"
    DESTINATION lib/cmake/cxl_pmr
)

install(EXPORT cxl_pmr_targets
    FILE cxl_pmr_targets.cmake
    NAMESPACE cxl_pmr::
    DESTINATION lib/cmake/cxl_pmr
)