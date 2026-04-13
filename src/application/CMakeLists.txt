add_library(application application.cpp)

target_include_directories(application PUBLIC ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(
    application
    PUBLIC project_options utility services core interfaces io net
)
