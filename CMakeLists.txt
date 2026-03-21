cmake_minimum_required(VERSION 3.20)
project(Vanguard VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AR "C:/Program Files/LLVM/bin/llvm-lib.exe")

add_subdirectory(core)
add_subdirectory(storage)
add_subdirectory(ui)

add_executable(vanguard WIN32 main.cpp
    libs/imgui/imgui.cpp
    libs/imgui/imgui_draw.cpp
    libs/imgui/imgui_tables.cpp
    libs/imgui/imgui_widgets.cpp
    libs/imgui/backends/imgui_impl_win32.cpp
    libs/imgui/backends/imgui_impl_dx11.cpp
)

target_link_libraries(vanguard PRIVATE ui core storage)
target_include_directories(vanguard PRIVATE ${CMAKE_SOURCE_DIR})
target_compile_options(vanguard PRIVATE /EHsc)
target_compile_definitions(vanguard PRIVATE SODIUM_STATIC IMGUI_USE_WCHAR32)
