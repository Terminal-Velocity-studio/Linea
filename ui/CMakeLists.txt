add_library(ui
    window.cpp
)

target_include_directories(ui PUBLIC
    ${CMAKE_SOURCE_DIR}/libs/imgui
    ${CMAKE_SOURCE_DIR}/libs/imgui/backends
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/ui
)

target_compile_definitions(ui PUBLIC SODIUM_STATIC IMGUI_USE_WCHAR32)
target_compile_options(ui PRIVATE /EHsc)

target_link_libraries(ui PUBLIC
    core
    storage
    d3d11
    dxgi
    d3dcompiler
)
