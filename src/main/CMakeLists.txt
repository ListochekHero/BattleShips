add_executable(server main.cpp)
add_executable(lobby lobby.cpp)
add_executable(client client.cpp)

target_link_libraries(server PRIVATE project_options application)
target_link_libraries(lobby PRIVATE project_options application)
target_link_libraries(client PRIVATE project_options application)

target_link_libraries(server PUBLIC atomic)
target_link_libraries(lobby PUBLIC atomic)
target_link_libraries(client PUBLIC atomic)
