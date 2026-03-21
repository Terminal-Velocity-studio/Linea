add_library(transport
    peer.cpp
)

target_include_directories(transport PUBLIC
    ${CMAKE_SOURCE_DIR}/libs/asio-master/include
    ${CMAKE_SOURCE_DIR}
)

target_compile_definitions(transport PUBLIC ASIO_STANDALONE)
target_compile_options(transport PRIVATE /EHsc)

target_link_libraries(transport PUBLIC core)
