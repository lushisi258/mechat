cmake_minimum_required(VERSION 3.10)
project(IMServer)

option(ENABLE_ASAN "Enable AddressSanitizer" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 可通过系统包管理器安装的库
find_package(Boost REQUIRED COMPONENTS system thread)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)

add_executable(IMServer
    src/database_pool.cpp
    src/logger.cpp
    src/main.cpp
    src/message.cpp
    src/server.cpp
    src/session_manager.cpp
    src/session.cpp
)

# ASan 配置
if(ENABLE_ASAN)
    target_compile_options(IMServer PRIVATE -fsanitize=address -g -O1)
    target_link_options(IMServer PRIVATE -fsanitize=address)
endif()

# 指定头文件路径
target_include_directories(IMServer PRIVATE
    include
    ${Boost_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    /usr/include/mariadb                # MariaDB 头文件
    /usr/local/include/bsoncxx/v_noabi  # BSONCXX 头文件
    /usr/local/include/mongocxx/v_noabi # MONGOCXX 头文件
)

# 指定自己编译安装的库文件路径
target_link_directories(IMServer PRIVATE
    /usr/lib          # MariaDB 库路径
    /usr/local/lib    # MongoDB/Redis C++ 驱动库路径
)

target_link_libraries(IMServer PRIVATE
    Boost::system
    Boost::thread
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    ZLIB::ZLIB
    hiredis           # Redis 驱动库
    mariadb           # MariaDB 客户端库
    bsoncxx           # MongoDB BSON 库
    mongocxx          # MongoDB 驱动库
)