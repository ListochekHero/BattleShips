add_library(utility
    config.cpp
    data_storage.cpp
    error.cpp
    logger.cpp
    scope_guard.cpp
    utility.cpp
)

target_include_directories(utility PUBLIC ${CMAKE_SOURCE_DIR}/src)
