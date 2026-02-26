cmake_minimum_required(VERSION 3.10)
project(foxscript)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (ENABLE_ASAN)
   set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fsanitize=address")
   set (CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fno-omit-frame-pointer -fsanitize=address")
endif()


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
   
   src/MainWindow.cxx
   src/Text.cxx
   src/ZoomGesture.cxx
   src/VideoCaptureV4L2.cxx
)

target_link_libraries(foxscript
   ${OpenCV_LIBS}
   SDL2::SDL2
   z
   res
)


add_executable(v4l2_test
   v4l2_test.cxx
   
   
   # src/VideoCaptureV4L2.cxx
   src/Contours.cxx
)

target_link_libraries(v4l2_test
   ${OpenCV_LIBS}
)