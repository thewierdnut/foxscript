cmake_minimum_required(VERSION 3.6)

project(foxscript)

# armeabi-v7a requires cpufeatures library
# include(AndroidNdkModules)
# android_ndk_import_module_cpufeatures()

set(CMAKE_CXX_STANDARD 20 CACHE STRING "")

STRING(REPLACE "-g" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
STRING(REPLACE "-g" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")

# enable link time optimization. Also, override the optimization flags here,
# otherwise -Oz gets passed to the linker, which does not understand it.
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -O3 -flto")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -flto")

# SDL sources are in a subfolder named "SDL"
OPTION(SDL_STATIC_PIC "" ON)
set(SDL_SHARED OFF CACHE BOOL "shared") # doesn't seem to work right... For now passing in via gradle
set(SDL_STATIC ON CACHE BOOL "static")
add_subdirectory(SDL)

# Compilation of companion libraries
#add_subdirectory(SDL_image)
#add_subdirectory(SDL_mixer)
#add_subdirectory(SDL_ttf)

include_directories(
   ${CMAKE_SOURCE_DIR}/../../../
   ${CMAKE_SOURCE_DIR}/../../../res
   ${CMAKE_SOURCE_DIR}
)

add_subdirectory(${CMAKE_SOURCE_DIR}/../../../res ${CMAKE_BINARY_DIR}/res)
add_subdirectory(src)
