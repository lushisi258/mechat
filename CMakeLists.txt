cmake_minimum_required(VERSION 3.10)
project(MeChat)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找外部依赖库
find_package(OpenSSL REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)
find_package(Threads REQUIRED) 
find_package(Protobuf REQUIRED)

# Protobuf 生成
set(PROTO_FILE proto/message.proto)
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILE})

# 添加 .cpp 源文件
set(SOURCES
    main.cpp
    src/WssServer.cpp
    src/WssSession.cpp
    src/MsgDispatcher.cpp
    src/MsgHandler/TestMsg.cpp
    ${PROTO_SRCS} 
)

# 生成可执行程序
add_executable(mechat_server ${SOURCES})

# 包含头文件目录
target_include_directories(mechat_server PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${Protobuf_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR} 
)

# 链接库 (这里是修改重点)
target_link_libraries(mechat_server
    PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
    Boost::system
    Threads::Threads
    protobuf::libprotobuf 
)