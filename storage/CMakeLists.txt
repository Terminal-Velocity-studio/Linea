add_library(storage
    messages.cpp
)

target_include_directories(storage PUBLIC
    ${CMAKE_SOURCE_DIR}
)

target_compile_options(storage PRIVATE /EHsc)
