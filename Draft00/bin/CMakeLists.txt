cmake_minimum_required(VERSION 3.12)
project(Calc)
SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR})
set(CMAKE_CXX_STANDARD 11)
#file(GLOB SRC_LIST "../src/*.cc")
#add_executable(CloudDisk ${SRC_LIST})
add_executable(CloudDisk 
    ../src/CloudDiskServer.cc
    ../src/main.cc
    ../src/SafePreparedStatement.cc
    ../src/signup.pb.cc
)
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g")
enable_testing()  # 启用测试功能
add_test(NAME MyTest COMMAND CloudDisk)  # 添加测试
target_link_libraries(CloudDisk PRIVATE log4cpp pthread ssl crypto crypt srpc lz4 snappy protobuf  wfrest workflow)
