cmake_minimum_required(VERSION 3.8)
project(fake_FTP)

set(CMAKE_C_STANDARD 99)

set(SOURCE_FILES1 server.c tools.h tools.c)
add_executable(fake_server ${SOURCE_FILES1})

set(SOURCE_FILES2 client.c tools.h tools.c)
add_executable(fake_client ${SOURCE_FILES2})