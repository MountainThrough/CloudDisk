cmake_minimum_required(VERSION 3.12)
project(Calc)
SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR})
set(CMAKE_CXX_STANDARD 11)
file(GLOB SRC_LIST "*.cc")
add_executable(signupserver ${SRC_LIST})
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g")
enable_testing()  # 启用测试功能
add_test(NAME MyTest COMMAND signupserver)  # 添加测试
target_link_libraries(signupserver PRIVATE log4cpp ssl crypto crypt ppconsul srpc lz4 snappy protobuf workflow)
