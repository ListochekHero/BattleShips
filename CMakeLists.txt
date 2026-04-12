cmake_minimum_required(VERSION 3.16)

#Project name
project(BattleShips)

#C++ version
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

option(ENABLE_SANITIZERS "Enable address/undefined sanitizers" ON)

add_library(project_options INTERFACE)

target_compile_options(project_options INTERFACE
  -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Weffc++
  $<$<CONFIG:Debug>:-O0 -ggdb>
)

target_compile_definitions(project_options INTERFACE
  $<$<CONFIG:Debug>:DEBUG_LOGS>
)

if(ENABLE_SANITIZERS)
  target_compile_options(project_options INTERFACE
    $<$<CONFIG:Debug>:-fsanitize=address,undefined>
  )
  target_link_options(project_options INTERFACE
    $<$<CONFIG:Debug>:-fsanitize=address,undefined>
  )
endif()

add_subdirectory(src)
