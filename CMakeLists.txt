cmake_minimum_required(VERSION 3.15)
project(LiteFunctions)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "-pthread -O0")

find_package(Boost COMPONENTS thread)

add_executable(LiteFunctions main.cpp)
target_link_libraries(LiteFunctions ${Boost_THREAD_LIBRARY})