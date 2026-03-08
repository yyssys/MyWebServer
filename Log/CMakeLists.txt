cmake_minimum_required(VERSION 3.15)

project(log)

set(SRC log.cpp)

# set(LIBRARY_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../lib)

add_library(log STATIC ${SRC})

target_include_directories(log PRIVATE ./)

target_link_libraries(log PUBLIC pthread fmt)