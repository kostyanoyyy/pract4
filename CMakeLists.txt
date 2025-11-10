cmake_minimum_required(VERSION 3.10)
project(pr4)

set(CMAKE_C_STANDARD 99)

add_executable(server src/server.c)
add_executable(client src/client.c)

# Линковка с библиотекой ws2_32.lib для работы с сокетами
target_link_libraries(server ws2_32)
target_link_libraries(client ws2_32)
