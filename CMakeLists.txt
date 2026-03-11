cmake_minimum_required(VERSION 3.10)
project(MeChat)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加 .cpp 源文件
set(SOURCES
    main.cpp
    src/WssServer.cpp
    src/WssSession.cpp
)

# 查找外部依赖库
find_package(OpenSSL REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)
find_package(Threads REQUIRED) 

# 生成可执行程序
add_executable(mechat_server ${SOURCES})

# 链接库
target_link_libraries(mechat_server
    OpenSSL::SSL
    OpenSSL::Crypto
    Boost::system
    Threads::Threads
)