cmake_minimum_required(VERSION 3.13)
project(TaintTrackerPass LANGUAGES CXX)

# Tell CMake where to find your custom LLVM build
# set(LLVM_DIR "../llvm-project/build/lib/cmake/llvm/")
set(LLVM_DIR "../build/lib/cmake/llvm/")

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Using LLVM from ${LLVM_DIR}")
include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})
add_definitions(${LLVM_DEFINITIONS})

include(AddLLVM)

add_llvm_pass_plugin(TaintTrackerPass
  TaintTrackerPass.cpp
)