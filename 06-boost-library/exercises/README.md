# Boost 库练习

## 练习文件：boost_examples.cpp

本文件演示了 Boost 中三个 Ceph 最常用的组件：
1. **Boost.Container**: `small_vector` (节省堆分配)
2. **Boost.Intrusive**: `intrusive_list` (嵌入式链表，无节点分配)
3. **Boost.SmartPtr**: `intrusive_ptr` (零开销引用计数)

## 编译说明

由于 Boost 源码已下载至 `/home/i_ingfeng/boost_1_85_0`，请使用以下命令指定 include 路径进行编译：

```bash
# 使用本地下载的 Boost 源码编译
g++ -std=c++17 -I /home/i_ingfeng/boost_1_85_0 -O2 boost_examples.cpp -o boost_examples
./boost_examples

# 如果系统已全局安装 Boost (libboost-all-dev)，可以使用标准编译
g++ -std=c++17 -O2 boost_examples.cpp -o boost_examples
```
