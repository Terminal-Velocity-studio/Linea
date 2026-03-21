cmake_minimum_required(VERSION 3.20)
project(Vanguard VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AR "C:/Program Files/LLVM/bin/llvm-lib.exe")

add_subdirectory(core)
add_subdirectory(storage)

add_executable(vanguard main.cpp)
target_link_libraries(vanguard PRIVATE core storage)
target_include_directories(vanguard PRIVATE ${CMAKE_SOURCE_DIR})
target_compile_options(vanguard PRIVATE /EHsc)
