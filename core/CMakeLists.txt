add_library(core
    identity.cpp
)

target_include_directories(core PUBLIC
    ${CMAKE_SOURCE_DIR}/libs/libsodium/include
)

target_compile_definitions(core PUBLIC SODIUM_STATIC)

target_link_libraries(core PUBLIC
    ${CMAKE_SOURCE_DIR}/libs/libsodium/x64/Release/v143/static/libsodium.lib
)

target_compile_options(core PRIVATE /EHsc)
