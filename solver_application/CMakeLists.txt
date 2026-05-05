cmake_minimum_required(VERSION 3.4)
project(solver)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(solver ${CMAKE_CURRENT_SOURCE_DIR}/equation.cpp)

target_link_libraries(solver formatter_ex solver_lib)
