cmake_minimum_required(VERSION 3.12)
project(ex4)

set(CMAKE_CXX_STANDARD 14)

add_library(ex4 MemoryConstants.h PhysicalMemory.cpp PhysicalMemory.h VirtualMemory.h VirtualMemory.cpp)