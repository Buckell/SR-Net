cmake_minimum_required(VERSION 3.23)

project(sr-client-test)
project(sr-server-test)

set(CMAKE_CXX_STANDARD 17)

set(BOOST_PATH "C:/dev/boost_1_80_0/")

include_directories(
    "${CMAKE_SOURCE_DIR}/include"
    "${BOOST_PATH}"
)

add_executable(sr-client-test client_test.cpp)
add_executable(sr-server-test server_test.cpp)