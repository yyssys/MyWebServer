cmake_minimum_required(VERSION 3.15)

project(tinyWebServer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        # 只影响 __FILE__ 宏的输出 -fmacro-prefix-map=old=new
        # 所有 __FILE__ 宏输出的路径中，只要以 old 开头，就会把 old 部分删掉，只保留后面的相对路径。
        -fmacro-prefix-map=${CMAKE_SOURCE_DIR}/=
    )
endif()

find_package(Threads REQUIRED)

find_path(FMT_INCLUDE_DIR fmt/core.h)
find_library(FMT_LIBRARY fmt)

if(NOT FMT_INCLUDE_DIR OR NOT FMT_LIBRARY)
    message(FATAL_ERROR "fmt library not found. Please install libfmt-dev.")
endif()

find_path(MYSQLCPPCONN_INCLUDE_DIR mysql_driver.h
    PATH_SUFFIXES jdbc mysql-cppconn-8/jdbc
)
find_library(MYSQLCPPCONN_LIBRARY NAMES mysqlcppconn mysqlcppconn8)

if(NOT MYSQLCPPCONN_INCLUDE_DIR OR NOT MYSQLCPPCONN_LIBRARY)
    message(FATAL_ERROR "MySQL Connector/C++ library not found.")
endif()

add_executable(tinyWebServer
    main.cpp
    webServer.cpp
    buffer/buffer.cpp
    channel/channel.cpp
    config/config.cpp
    dispatcher/dispatcher.cpp
    dispatcher/epoll_dispatcher.cpp
    dispatcher/poll_dispatcher.cpp
    dispatcher/select_dispatcher.cpp
    http_conn/http_conn.cpp
    log/log.cpp
    sql_conn_pool/sql_conn_pool.cpp
    threadpool/threadpool.cpp
    threadpool/worker_thread.cpp
    timer/timer.cpp
)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR})

target_include_directories(tinyWebServer
    PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${FMT_INCLUDE_DIR}
    ${MYSQLCPPCONN_INCLUDE_DIR}
)

target_link_libraries(tinyWebServer
    PRIVATE
    ${FMT_LIBRARY}
    ${MYSQLCPPCONN_LIBRARY}
    Threads::Threads
)
