#!/bin/bash

# 确保脚本出错时停止执行
set -e

# 删除旧的构建目录
echo "Cleaning up old build directory..."
rm -rf build

# 创建新的构建目录
echo "Creating new build directory..."
mkdir build

# 进入构建目录
cd build

# 运行 cmake 配置项目
echo "Running cmake to configure the project..."
cmake ..

# 编译项目
echo "Building the project..."
make

# 提示完成
echo "Build complete. You can now run './IMServer'."
