cmake_minimum_required(VERSION 3.20)

project(cockpit_soundscape LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(SNDFILE REQUIRED sndfile)

add_executable(cockpit_soundscape
    src/audio_asset_manager.cpp
    src/audio_engine.cpp
    src/mixer.cpp
    src/main.cpp
    src/sdl_audio_output.cpp
    src/sound_catalog_runtime.cpp
    src/sound_policy.cpp
)

target_include_directories(cockpit_soundscape PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SDL2_INCLUDE_DIRS}
    ${SNDFILE_INCLUDE_DIRS}
)

target_compile_options(cockpit_soundscape PRIVATE
    ${SDL2_CFLAGS_OTHER}
    ${SNDFILE_CFLAGS_OTHER}
)

target_link_directories(cockpit_soundscape PRIVATE
    ${SDL2_LIBRARY_DIRS}
    ${SNDFILE_LIBRARY_DIRS}
)

target_link_libraries(cockpit_soundscape PRIVATE
    ${SDL2_LIBRARIES}
    ${SNDFILE_LIBRARIES}
)
