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

add_library(common OBJECT
application.cpp
config.cpp
data_storage.cpp
deferred_actions.cpp
dispatcher.cpp
error.cpp
interfaces.cpp
lobby_manager.cpp
logger.cpp
network_routine.cpp
socket_routine.cpp
utility.cpp

)

# add_library(sanitizers INTERFACE)

# target_compile_options(sanitizers INTERFACE
#   $<$<CONFIG:Debug>:-fsanitize=address,undefined>
# )

# target_link_options(sanitizers INTERFACE
#   $<$<CONFIG:Debug>:-fsanitize=address,undefined>
# )

target_compile_options(common PRIVATE
-Wall
-Wextra
-Wpedantic
-Wconversion
-Wsign-conversion
-Weffc++
)

target_compile_options(common PRIVATE
  $<$<CONFIG:Debug>:-O0 -ggdb>
)

add_definitions(-DDEBUG_LOGS)

target_compile_definitions(common PRIVATE
  $<$<CONFIG:Debug>:DEBUG_LOGS>
)

option(ENABLE_SANITIZERS "Enable address/undefined sanitizers" ON)

if(ENABLE_SANITIZERS)
  target_compile_options(common PRIVATE
    $<$<CONFIG:Debug>:-fsanitize=address,undefined>
  )
  target_link_options(common PRIVATE
    $<$<CONFIG:Debug>:-fsanitize=address,undefined>
  )
endif()

#Create an executable for the Server
add_executable(server
  main.cpp
  $<TARGET_OBJECTS:common>
)

#Create an executable for the Lobby
add_executable(lobby
  lobby.cpp
  $<TARGET_OBJECTS:common>
)

#Create an executable for the Client
add_executable(client
  client.cpp
  $<TARGET_OBJECTS:common>
)

target_link_options(server PRIVATE   $<$<CONFIG:Debug>:-fsanitize=address,undefined>)
target_link_options(lobby PRIVATE   $<$<CONFIG:Debug>:-fsanitize=address,undefined>)
target_link_options(client PRIVATE   $<$<CONFIG:Debug>:-fsanitize=address,undefined>)
