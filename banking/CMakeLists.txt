cmake_minimum_required(VERSION 3.4)
project(banking)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(banking STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/Account.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Transaction.cpp
)

target_include_directories(banking PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
