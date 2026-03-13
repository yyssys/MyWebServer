cmake_minimum_required(VERSION 3.15)

project(tinyWebServer)

# 只影响 __FILE__ 宏的输出 -fmacro-prefix-map=old=new 
# 所有 __FILE__ 宏输出的路径中，只要以 old 开头，就会把 old 部分删掉，只保留后面的相对路径。
# GCC/Clang 的编译选项（MSVC 不支持）
add_compile_options(-fmacro-prefix-map=${CMAKE_SOURCE_DIR}/=)

add_subdirectory(log)

add_subdirectory(test)