cmake_minimum_required(VERSION 3.19)
project(file_system)

set(CMAKE_CXX_STANDARD 20)

set(TOP_DIR ${CMAKE_SOURCE_DIR})
set(SRC_DIR ${TOP_DIR}/src)

include_directories(${SRC_DIR})

set(SRC_LIST
        ${SRC_DIR}/io.hpp
        ${SRC_DIR}/fs.hpp
        ${SRC_DIR}/fs.cpp
        ${SRC_DIR}/fs_utils.cpp
        )

set(LIB_NAME ${PROJECT_NAME}_core)
add_library(${LIB_NAME} STATIC ${SRC_LIST})

add_executable(file_system main.cpp)
target_link_libraries(file_system PRIVATE ${LIB_NAME})
