add_executable(server main.cpp)
add_executable(lobby lobby.cpp)
add_executable(client client.cpp)

target_link_libraries(server PRIVATE project_options application)
target_link_libraries(lobby PRIVATE project_options application)
target_link_libraries(client PRIVATE project_options application)

target_link_libraries(server PUBLIC atomic)
target_link_libraries(lobby PUBLIC atomic)
target_link_libraries(client PUBLIC atomic)

if(ENABLED_SANITIZERS)
    target_link_options(server PRIVATE   $<$<CONFIG:Debug>:-fsanitize=address,undefined>)
    target_link_options(lobby PRIVATE   $<$<CONFIG:Debug>:-fsanitize=address,undefined>)
    target_link_options(client PRIVATE   $<$<CONFIG:Debug>:-fsanitize=address,undefined>)
endif()

