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

# 寻找 Boost 库并指定所需的组件
find_package(Boost REQUIRED COMPONENTS system thread)

# 创建可执行文件
add_executable(IMServer
    src/main.cpp
    src/message.cpp
    src/network_logger.cpp
    src/server.cpp
    src/session_manager.cpp
    src/session.cpp
)

# 包含头文件目录
target_include_directories(IMServer PRIVATE include)

# 链接 Boost 库的 system 和 thread 组件，以及 pthread 库
target_link_libraries(IMServer PRIVATE Boost::system Boost::thread pthread)
