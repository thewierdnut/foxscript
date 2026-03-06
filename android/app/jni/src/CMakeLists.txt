cmake_minimum_required(VERSION 3.6)

project(foxscript)

add_library(main SHARED
   ${CMAKE_SOURCE_DIR}../../../../src/Image.cxx
   ${CMAKE_SOURCE_DIR}../../../../src/MainWindow.cxx
   ${CMAKE_SOURCE_DIR}../../../../src/Text.cxx
   ${CMAKE_SOURCE_DIR}../../../../src/VideoCaptureAndroid.cxx
   ${CMAKE_SOURCE_DIR}../../../../src/ZoomGesture.cxx
   ${CMAKE_SOURCE_DIR}../../../../foxscript.cxx
)

target_link_libraries(main
   SDL2main
   SDL2-static
   res
   z

   camera2ndk
   mediandk
)


