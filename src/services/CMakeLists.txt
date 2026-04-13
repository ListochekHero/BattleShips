add_library(services lobby_manager.cpp)

target_include_directories(services PUBLIC ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(services PUBLIC project_options protocol utility)
