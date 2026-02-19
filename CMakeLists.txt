cmake_minimum_required(VERSION 3.10)
project(foxscript)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-fno-omit-frame-pointer)

find_package(OpenCV REQUIRED)
find_package(SDL2 REQUIRED)

include_directories(foxscript PUBLIC
   ${OpenCV_INCLUDE_DIRS}
   ${SDL2_INCLUDE_DIRS}
   res
   ${CMAKE_SOURCE_DIR}
)

add_subdirectory(res)

add_executable(foxscript
   foxscript.cxx
   
   src/Text.cxx
)

target_link_libraries(foxscript
   ${OpenCV_LIBS}
   SDL2::SDL2
   z
   res
)