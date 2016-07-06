cmake_minimum_required(VERSION 2.8)
project(section-static)
set(section-static_VERSION_MAJOR 1)
set(section-static_VERSION_MINOR 0)

find_package(LLVM REQUIRED CONFIG)

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS} include)

add_library(SectionStatic MODULE src/SectionStatic.cpp)

install(TARGETS SectionStatic DESTINATION lib)
