add_library(transport
    peer.cpp
    reliable.cpp
)

target_include_directories(transport PUBLIC
    ${CMAKE_SOURCE_DIR}/libs/asio-master/include
    ${CMAKE_SOURCE_DIR}/libs/libsodium/include
    ${CMAKE_SOURCE_DIR}
)

target_compile_definitions(transport PUBLIC 
    ASIO_STANDALONE 
    SODIUM_STATIC
    _WIN32_WINNT=0x0601
)
target_compile_options(transport PRIVATE /EHsc)
target_link_libraries(transport PUBLIC core)
