#!/bin/bash

# 删除旧的构建目录
echo "清空build目录..."
rm -rf build

# 创建新的构建目录
echo "创建build目录..."
mkdir build

# 进入构建目录
cd build

# 运行 cmake 配置项目
echo "运行 cmake 配置项目..."
cmake -DENABLE_ASAN=ON ..

# 编译项目
echo "编译项目中"
make -j4

# 提示完成
echo "编译完成：'./build/IMServer'."
