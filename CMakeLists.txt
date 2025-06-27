cmake_minimum_required(VERSION 3.10)
project(Poke)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows XP compatible subsystem
add_definitions(-D_WIN32_WINNT=0x0501)

add_executable(Poke poke.cpp)
