cmake_minimum_required(VERSION 3.10)
project(foxscript)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 1. Find OpenCV
find_package(OpenCV REQUIRED)

# 2. Find SDL2
# On many Linux systems, SDL2 provides a cmake config.
# If not, we can use pkg-config as a backup.
find_package(SDL2 REQUIRED)

# Define the executable
add_executable(foxscript foxscript.cxx)

# 3. Include headers
target_include_directories(foxscript PUBLIC ${OpenCV_INCLUDE_DIRS} ${SDL2_INCLUDE_DIRS})

# 4. Link libraries
target_link_libraries(foxscript ${OpenCV_LIBS} SDL2::SDL2)