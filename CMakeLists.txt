# 最低版本要求
cmake_minimum_required(VERSION 3.10)

# 项目名称
project(IMServer)

# ASan 开关选项
option(ENABLE_ASAN "Enable AddressSanitizer for memory checks" OFF)

# 配置编译选项
if(ENABLE_ASAN)
    # 添加 ASan 编译和链接标志
    add_compile_options(-fsanitize=address -g -O1)
    add_link_options(-fsanitize=address)
endif()

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 寻找 Boost 库并指定所需的组件
find_package(Boost REQUIRED COMPONENTS system thread)

# 寻找 OpenSSL 库
find_package(OpenSSL REQUIRED)

# 包含项目头文件目录
include_directories(
    include
    /usr/include/mariadb          # MariaDB 头文件
    /usr/local/include             # 基础头文件
    /usr/local/include/bsoncxx/v_noabi  # BSONCXX 头文件
    /usr/local/include/mongocxx/v_noabi # MONGOCXX 头文件
    ${Boost_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
)

# 强制指定库文件路径
link_directories(/usr/local/lib)

# 创建可执行文件
add_executable(IMServer
    src/database_pool.cpp
    src/logger.cpp
    src/main.cpp
    src/message.cpp
    src/server.cpp
    src/session_manager.cpp
    src/session.cpp
)

# 手动定义库文件名称（无需查找）
set(MARIADB_LIB "mariadb")
set(BSONCXX_LIB "bsoncxx")
set(MONGOCXX_LIB "mongocxx")

# 链接组件
target_link_libraries(IMServer PRIVATE
    Boost::system
    Boost::thread
    OpenSSL::SSL
    OpenSSL::Crypto
    hiredis
    ${MARIADB_LIB}
    ${BSONCXX_LIB}
    ${MONGOCXX_LIB}
    pthread z
)