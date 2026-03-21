cmake_minimum_required(VERSION 3.10)
project(Practice)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 1. Define the custom toggle (Defaults to OFF/Debug mode)
option(Optimize "Build with high optimizations and no assertions" OFF)

# 2. Source files
set(SOURCES 
    main.cpp Node.cpp Tree.cpp FullGrowTree.cpp 
    GrowTree.cpp DataProcessor.cpp
)

add_subdirectory(csv-parser)
add_executable(out ${SOURCES})
target_link_libraries(out csv)

# 3. Apply Flags based on the Optimize option
if(Optimize)
    message(STATUS "Build Mode: OPTIMIZED (Assertions disabled)")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(out PRIVATE -O3 -march=native -mtune=native -flto -ffast-math)
        target_compile_definitions(out PRIVATE NDEBUG) # This removes the assertions!
    endif()
else()
    message(STATUS "Build Mode: DEBUG (Assertions enabled, slow)")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(out PRIVATE -g -O0)
        # Note: We do NOT define NDEBUG here, so assert() stays active.
    endif()
endif()

# File handling
file(COPY ${CMAKE_SOURCE_DIR}/dataset/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/dataset/)
