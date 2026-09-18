cmake_minimum_required(VERSION 3.25)

add_library(sandbox-data STATIC
    src/tq_archive_hash.cpp
    src/wdf.cpp
    src/dmap.cpp
    src/c3.cpp
    src/ini.cpp
    src/weapon_action.cpp
    src/map_catalog.cpp
    src/map_puzzle.cpp
    src/map_scene.cpp
    src/audio.cpp
)

target_include_directories(sandbox-data PUBLIC include)
