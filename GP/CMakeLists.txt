cmake_minimum_required(VERSION 3.10)

project(Practice)

# For lsp
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-ftemplate-depth=256)
endif()


# DEBUG
# for lsp
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")

# RELEASE
# set(CMAKE_BUILD_TYPE Release)

set(CMAKE_CXX_CLANG_TIDY
    clang-tidy;
    -p=${CMAKE_BINARY_DIR}
)

# For finding local packages
# find_package(CURL REQUIRED)

# Create the executable from your source files
add_executable(out main.cpp Node.cpp)

set_target_properties(out PROPERTIES COMPILE_FLAGS "-O0")

# link the libraries to trading_bot during execution
# target_link_libraries(trading_bot PRIVATE CURL::libcurl)

