cmake_minimum_required(VERSION 3.20)

project(cockpit_soundscape LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
find_package(OpenAL REQUIRED)
find_package(yaml-cpp REQUIRED)
pkg_check_modules(SNDFILE REQUIRED sndfile)

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
    src/openal_audio_output.cpp
    src/sound_catalog_runtime.cpp
    src/sound_policy.cpp
)

target_include_directories(cockpit_soundscape PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${OPENAL_INCLUDE_DIR}
    ${SNDFILE_INCLUDE_DIRS}
    ${YAML_CPP_INCLUDE_DIR}
    ${YAML_CPP_INCLUDE_DIRS}
)

target_compile_options(cockpit_soundscape PRIVATE
    ${SNDFILE_CFLAGS_OTHER}
)

target_link_directories(cockpit_soundscape PRIVATE
    ${SNDFILE_LIBRARY_DIRS}
)

target_link_libraries(cockpit_soundscape PRIVATE
    ${OPENAL_LIBRARY}
    ${SNDFILE_LIBRARIES}
    ${YAML_CPP_TARGET}
)
