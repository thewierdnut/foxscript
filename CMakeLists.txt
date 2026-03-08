# foxscript
# Copyright (C) 2026 thewierdnut
# 
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
cmake_minimum_required(VERSION 3.10)
project(foxscript)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (ENABLE_ASAN)
   set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fsanitize=address")
   set (CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fno-omit-frame-pointer -fsanitize=address")
endif()


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
   src/Image.cxx
)

target_link_libraries(foxscript
   SDL2::SDL2
   z
   res
)

