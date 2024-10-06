cmake_minimum_required(VERSION 3.10)

#Project name
project(BattleShips)

#C++ version
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

#Server source files
set(SERVER_SOURCES
main.cpp
application.cpp
interfaces.cpp
logger.cpp
socket_routine.cpp)

#Create an executable for the Server
add_executable(server ${SERVER_SOURCES})

#Lobby source files
set(LOBBY_SOURCES
lobby.cpp)

#Create an executable for the Lobby
add_executable(looby ${LOBBY_SOURCES})

#Include directories for shared headers
target_include_directories(server PRIVATE ${CMAKE_SOURCE_DIR})
target_include_directories(lobby PRIVATE ${CMAKE_SOURCE_DIR})
