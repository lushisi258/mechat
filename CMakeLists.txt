# 最低版本要求
cmake_minimum_required(VERSION 3.10)
# 项目名称
project(IMServer)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)

# 寻找 Boost 库并指定所需的组件
find_package(Boost REQUIRED COMPONENTS system thread)

# 创建可执行文件
add_executable(IMServer
    src/main.cpp
    src/server.cpp
    src/session.cpp
)

# 包含头文件目录
target_include_directories(IMServer PRIVATE include)

# 链接 Boost 库的 system 和 thread 组件，以及 pthread 库
target_link_libraries(IMServer PRIVATE Boost::system Boost::thread pthread)
