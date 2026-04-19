# Ceph 源码学习技能路线

> 适合有C语言基础，从零开始学习C++并阅读Ceph源码
> 
> **核心目标：能读Ceph代码，能改小patch，而不是成为语言专家**

## 概述

Ceph 代码库分析：
- **C++ 文件**: ~17000 个 (.cc/.h) - 核心逻辑
- **Python 文件**: ~2100 个 - 管理工具/mgr模块
- **构建系统**: CMake + Ninja

## 学习目标分层

| 层次 | 目标 | 说明 |
|------|------|------|
| 第一层 | **能看懂** | 能读Ceph核心路径代码，追踪数据流 |
| 第二层 | **能改小patch** | 加日志、改简单逻辑、修小bug |
| 第三层 | 能独立设计 | 暂不急，后期再学 |

## 学习阶段（按优先级）

| 阶段 | 内容 | 时间 | 优先级 | 说明 |
|------|------|------|--------|------|
| 00 | 用户态工程思维 | - | P0 | 建立异步/状态机意识 |
| 01 | C++基础语法+面向对象 | 2周 | P0 | 类、继承、虚函数、const |
| 02 | STL+智能指针 | 1周 | P0 | vector/map/shared_ptr |
| 03 | 现代C++特性 | 1周 | P0 | auto/lambda/range-for |
| 04 | **并发与异步** | 2周 | **P0** | 线程/锁/回调/状态机 |
| 05 | Python基础 | 1周 | P0 | 能读脚本、改脚本 |
| 06 | Shell基础 | 1周 | P0 | 构建/测试/日志收集 |
| 07 | 模板阅读能力 | - | P1 | 能看懂模板定义 |
| 08 | Boost/Seastar | - | P2 | 遇到再学 |

## 注意：为什么并发异步是P0？

Ceph不是单机文件系统，它有：
- 网络消息
- 集群节点交互
- 请求排队
- 后台线程
- 回调续跑
- 状态转换

读代码时必须能回答：
- 这个请求当前在哪个线程？
- 同步执行还是异步提交？
- 后续谁继续处理？
- 完成通知在哪里发出？

## 学习路径

```
00-user-space-mindset/  ← 异步/状态机思维（新增）
01-cpp-foundation/      ← 从C到C++过渡
02-stl-containers/      ← STL标准库
03-concurrency/         ← 并发基础（重点）
04-modern-cpp/          ← 现代C++特性（新增）
05-templates-advanced/   ← 模板阅读能力
06-boost-library/        ← Boost/Seastar
07-async-programming/    ← 异步编程
08-shell-basics/        ← Shell脚本（新增）
09-python-basics/       ← Python（升级为P0）
10-ceph-code-path/       ← Ceph源码实战（新增）
```

## Ceph 入门代码路径

推荐阅读顺序：
```
src/include/           # 基础类型定义
  └── buffer_fwd.h     # bufferptr/bufferlist
  └── types.h          # 基础类型
  └── utime.h          # 时间类型

src/common/            # 通用工具
  └── config.h         # 配置系统
  └── dout.h           # 日志输出
  └── ceph_mutex.h     # 锁封装

src/osd/               # OSD核心
src/mon/               # Monitor
src/mds/               # MDS元数据服务
```

## 每日学习建议

1. **理论学习** (1小时): 阅读概念和示例
2. **代码实践** (30分钟): 完成练习
3. **源码阅读** (30分钟): 对应Ceph代码

## 快速开始

```bash
# 进入第一阶段
cd 01-cpp-foundation
cat README.md
```