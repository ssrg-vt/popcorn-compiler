cmake_minimum_required(VERSION 2.8)
project(stack-transformation)
set(stack-transformation_VERSION_MAJOR 1)
set(stack-transformation_VERSION_MINOR 0)

find_package(LLVM REQUIRED CONFIG)

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS} include ../live_values/include)
link_directories(${CMAKE_LIBRARY_PATH})

add_library(StackInfo MODULE src/StackInfo.cpp)

target_link_libraries(StackInfo libLiveValues.so)

install(TARGETS StackInfo DESTINATION lib)
