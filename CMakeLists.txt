cmake_minimum_required(VERSION 3.16)
project(custom_shell C)

set(CMAKE_C_STANDARD 11)

# Source and header directories
include_directories(include)
file(GLOB SOURCES src/*.c)

# Find SDL2 and SDL2_ttf
find_package(SDL2 REQUIRED)
find_package(SDL2_ttf REQUIRED)

add_executable(shell ${SOURCES})

# Link both SDL2 and SDL2_ttf
target_link_libraries(shell PRIVATE SDL2::SDL2 SDL2_ttf::SDL2_ttf)
