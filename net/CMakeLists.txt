add_library(net
    socket.cpp
    reliable.cpp
    peer.cpp
)

target_include_directories(net PUBLIC
    ${CMAKE_SOURCE_DIR}/libs/asio-master/include
    ${CMAKE_SOURCE_DIR}/libs/libsodium/include
    ${CMAKE_SOURCE_DIR}
)

target_compile_definitions(net PUBLIC SODIUM_STATIC)
target_compile_options(net PRIVATE /EHsc)
target_link_libraries(net PUBLIC core)
