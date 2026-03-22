cmake_minimum_required(VERSION 3.20)
project(Vanguard VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AR "C:/Program Files/LLVM/bin/llvm-lib.exe")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

# Подавляем предупреждение asio про WIN32
add_compile_definitions(_WIN32_WINNT=0x0601 ASIO_STANDALONE SODIUM_STATIC IMGUI_USE_WCHAR32)

add_subdirectory(core)
add_subdirectory(net)
add_subdirectory(storage)
add_subdirectory(ui)

add_executable(vanguard main.cpp
    libs/imgui/imgui.cpp
    libs/imgui/imgui_draw.cpp
    libs/imgui/imgui_tables.cpp
    libs/imgui/imgui_widgets.cpp
    libs/imgui/backends/imgui_impl_sdl3.cpp
    libs/imgui/backends/imgui_impl_sdlrenderer3.cpp
)

target_include_directories(vanguard PRIVATE
    `${CMAKE_SOURCE_DIR}/libs/imgui
    `${CMAKE_SOURCE_DIR}/libs/imgui/backends
    `${CMAKE_SOURCE_DIR}/libs/SDL3-3.2.10/include
)
target_link_libraries(vanguard PRIVATE core net storage ui dbghelp)
target_include_directories(vanguard PRIVATE ${CMAKE_SOURCE_DIR})
target_compile_options(vanguard PRIVATE /EHsc)
