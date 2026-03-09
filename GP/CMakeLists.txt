cmake_minimum_required(VERSION 3.10)

project(Practice)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# For lsp
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-ftemplate-depth=256)
endif()

# DEBUG for lsp
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")

# RELEASE set(CMAKE_BUILD_TYPE Release)

set(CMAKE_CXX_CLANG_TIDY clang-tidy; -p=${CMAKE_BINARY_DIR})

file(COPY ${CMAKE_SOURCE_DIR}/dataset/
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/dataset/)

add_subdirectory(csv-parser)

# Source files
set(SOURCES main.cpp Node.cpp Tree.cpp FullGrowTree.cpp GrowTree.cpp
            DataProcessor.cpp)

# Create executable
add_executable(out ${SOURCES})

set_target_properties(out PROPERTIES COMPILE_FLAGS "-O0")

target_link_libraries(out csv)

# For finding local packages find_package(CURL REQUIRED)
# target_link_libraries(out PRIVATE CURL::libcurl)
