cmake_minimum_required(VERSION 2.8)
project(live-values)
set(live-values_VERSION_MAJOR 1)
set(live-values_VERSION_MINOR 0)

find_package(LLVM REQUIRED CONFIG)

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS} include)

add_library(LiveValues MODULE src/LiveValues.cpp src/LoopNestingTree.cpp)

install(TARGETS LiveValues DESTINATION lib)
