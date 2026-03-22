cmake_minimum_required(VERSION 3.20)
project(Vanguard VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AR "C:/Program Files/LLVM/bin/llvm-lib.exe")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

add_subdirectory(core)
add_subdirectory(storage)
add_subdirectory(transport)
add_subdirectory(ui)

add_executable(vanguard main.cpp
    libs/imgui/imgui.cpp
    libs/imgui/imgui_draw.cpp
    libs/imgui/imgui_tables.cpp
    libs/imgui/imgui_widgets.cpp
    libs/imgui/backends/imgui_impl_sdl3.cpp
    libs/imgui/backends/imgui_impl_sdlrenderer3.cpp
)

target_link_libraries(vanguard PRIVATE ui core storage transport)
target_include_directories(vanguard PRIVATE ${CMAKE_SOURCE_DIR})
target_compile_options(vanguard PRIVATE /EHsc)
target_compile_definitions(vanguard PRIVATE SODIUM_STATIC IMGUI_USE_WCHAR32)
