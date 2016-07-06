cmake_minimum_required(VERSION 2.8)
project(associate-stringliteral)
set(associate-stringliteral_VERSION_MAJOR 1)
set(associate-stringliteral_VERSION_MINOR 0)

find_package(LLVM REQUIRED CONFIG)

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS} include)

add_library(AssociateLiteral MODULE src/AssociateLiteral.cpp)

install(TARGETS AssociateLiteral DESTINATION lib)
