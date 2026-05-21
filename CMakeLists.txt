cmake_minimum_required(VERSION 3.21)

project(cockpit_soundscape LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

find_package(SDL2 CONFIG QUIET)
if(TARGET SDL2::SDL2)
    set(SDL2_TARGET SDL2::SDL2)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED sdl2)
    set(SDL2_TARGET ${SDL2_LIBRARIES})
endif()

find_package(SndFile CONFIG QUIET)
if(TARGET SndFile::sndfile)
    set(SNDFILE_TARGET SndFile::sndfile)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SNDFILE REQUIRED sndfile)
    set(SNDFILE_TARGET ${SNDFILE_LIBRARIES})
endif()

find_package(yaml-cpp CONFIG QUIET)
if(NOT yaml-cpp_FOUND)
    find_package(yaml-cpp REQUIRED)
endif()

if(TARGET yaml-cpp::yaml-cpp)
    set(YAML_CPP_TARGET yaml-cpp::yaml-cpp)
elseif(TARGET yaml-cpp)
    set(YAML_CPP_TARGET yaml-cpp)
else()
    set(YAML_CPP_TARGET ${YAML_CPP_LIBRARIES})
endif()

add_executable(cockpit_soundscape
    src/audio_asset_manager.cpp
    src/audio_engine.cpp
    src/mixer.cpp
    src/main.cpp
    src/sdl_audio_output.cpp
    src/sound_catalog_runtime.cpp
    src/sound_control_queue.cpp
    src/sound_policy.cpp
)

set_target_properties(cockpit_soundscape PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_compile_definitions(cockpit_soundscape PRIVATE SDL_MAIN_HANDLED)

target_include_directories(cockpit_soundscape PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SDL2_INCLUDE_DIRS}
    ${SNDFILE_INCLUDE_DIRS}
    ${YAML_CPP_INCLUDE_DIR}
    ${YAML_CPP_INCLUDE_DIRS}
)

target_compile_options(cockpit_soundscape PRIVATE
    ${SDL2_CFLAGS_OTHER}
    ${SNDFILE_CFLAGS_OTHER}
    $<$<PLATFORM_ID:Darwin>:-Wno-deprecated-declarations>
)

target_link_directories(cockpit_soundscape PRIVATE
    ${SDL2_LIBRARY_DIRS}
    ${SNDFILE_LIBRARY_DIRS}
)

target_link_libraries(cockpit_soundscape PRIVATE
    ${SDL2_TARGET}
    ${SNDFILE_TARGET}
    Threads::Threads
    ${YAML_CPP_TARGET}
)

if(WIN32)
    add_custom_command(TARGET cockpit_soundscape POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:cockpit_soundscape>
            $<TARGET_FILE_DIR:cockpit_soundscape>
        COMMAND_EXPAND_LISTS
    )
endif()
