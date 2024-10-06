cmake_minimum_required(VERSION 3.10)

#Project name
project(BattleShips)

#C++ version
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_BUILD_TYPE Debug)

#Server source files
set(SERVER_SOURCES
main.cpp
application.cpp
interfaces.cpp
logger.cpp
socket_routine.cpp
)

#Create an executable for the Server
add_executable(server ${SERVER_SOURCES})

#Lobby source files
set(LOBBY_SOURCES
lobby.cpp
logger.cpp
)

#Create an executable for the Lobby
add_executable(lobby ${LOBBY_SOURCES})

#Client source files
set(CLIENT_SOURCES
client.cpp
application.cpp
interfaces.cpp
logger.cpp
socket_routine.cpp
)

#Create an executable for the Client
add_executable(client ${CLIENT_SOURCES})


#Include directories for shared headers
target_include_directories(server PRIVATE ${CMAKE_SOURCE_DIR})
target_include_directories(lobby PRIVATE ${CMAKE_SOURCE_DIR})
target_include_directories(client PRIVATE ${CMAKE_SOURCE_DIR})


# Default build type if not specified
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
endif()

# Define DEBUG_LOGS macro for Debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DDEBUG_LOGS)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")