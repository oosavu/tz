cmake_minimum_required(VERSION 3.5)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(sorter LANGUAGES CXX)
add_executable(sorter sorter_main.cpp sorter.cpp  asyncfile.cpp)

project(filegenerator LANGUAGES CXX)
add_executable(filegenerator filegenerator_main.cpp asyncfile.cpp )
