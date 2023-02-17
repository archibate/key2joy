cmake_minimum_required(VERSION 3.18)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_C_STANDARD 99)

project(key2joy LANGUAGES C)

add_executable(key2joy key2joy.c)
