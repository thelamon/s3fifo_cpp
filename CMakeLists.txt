cmake_minimum_required(VERSION 3.24)
project(s3fifo_cpp)

set(CMAKE_CXX_STANDARD 17)

add_executable(s3fifo_cpp main.cpp userver/lru_cache_component.hpp)
