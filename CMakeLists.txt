cmake_minimum_required(VERSION 3.25)

project(Preservation-CO-Client-CPP VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(sandbox-data)

add_subdirectory(sandbox)
