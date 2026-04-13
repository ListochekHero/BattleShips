add_library(interfaces interfaces.cpp modules.cpp)

target_include_directories(interfaces PUBLIC ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(interfaces PUBLIC project_options core)
