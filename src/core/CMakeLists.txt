add_library(core atomic_queue.cpp dispatcher.cpp scheduler.cpp)

target_include_directories(core PUBLIC ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(core PUBLIC project_options protocol)
