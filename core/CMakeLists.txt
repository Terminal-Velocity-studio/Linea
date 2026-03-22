add_library(core identity.cpp)

target_include_directories(core PUBLIC
    ${CMAKE_SOURCE_DIR}/libs/libsodium/include
    ${CMAKE_SOURCE_DIR}
)

target_compile_options(core PRIVATE /EHsc)

target_link_libraries(core PUBLIC
    ${CMAKE_SOURCE_DIR}/libs/libsodium/x64/Release/v143/static/libsodium.lib
)
