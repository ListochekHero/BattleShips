add_library(net
    deferred_actions.cpp
    network_routine.cpp
    socket_routine.cpp
)

target_include_directories(net PUBLIC ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(net PUBLIC project_options core utility interfaces)
