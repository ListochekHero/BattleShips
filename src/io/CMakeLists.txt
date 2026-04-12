add_library(io
    console_routine.cpp
)

target_include_directories(io PUBLIC ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(io PUBLIC project_options core interfaces)
