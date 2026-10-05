# Thread Pool

一个基于 C++11 实现的固定大小线程池项目，重点演示任务提交、异步结果回传、有界队列和优雅关闭这几类后端并发基础能力。

## Highlights

- 使用固定数量的 worker 线程复用执行任务，避免频繁创建和销毁线程。
- 使用 `std::packaged_task` 和 `std::future` 返回异步任务结果。
- 使用有界任务队列做过载保护，避免任务无限堆积。
- 提供显式 `shutdown()`，析构函数兜底回收线程资源。
- 自带一组自测，覆盖配置边界、队列上限和关闭语义。

## Requirements

- C++11 编译器 (g++ 4.8+, clang++ 3.3+, MSVC 2015+)
- CMake 3.16+
- 支持标准线程库的编译器

## Quick Start

### One-Click Build

```bash
./autobuild.sh
```

这个脚本会自动：

- 生成 `build/` 构建目录
- 编译 `test_pool`
- 运行全部自测

### Manual Build

```bash
cmake -S . -B build
cmake --build build
./build/bin/test_pool
```

### Run With CTest

```bash
ctest --test-dir build --output-on-failure
```

### Run Demo

```bash
./build/bin/thread_pool_demo
```

演示场景包括：基本用法、多任务并发、异常处理、队列满拒绝。

## Self-Test Coverage

当前自测主要覆盖：

- 非法线程数配置
- 非法队列容量配置
- 队列满时拒绝新任务
- `shutdown()` 等待已提交任务完成并拒绝新任务
- `shutdown()` 可重复调用（幂等）
- worker 线程内部调用 `shutdown()` 抛出异常
- 任务抛异常后 `future.get()` 能捕获异常

## shutdown 语义

- 停止接收新任务（`enqueue` 抛异常）
- 已入队任务继续执行完毕
- 队列清空后 worker 线程退出
- 调用线程等待所有 worker `join` 完成
- 重复调用安全返回（幂等）
- 禁止从 worker 线程内部调用，否则抛 `std::runtime_error`

## Project Layout

```text
.
├── CMakeLists.txt
├── README.md
├── autobuild.sh
├── docs
│   └── knowledge_summary.md
├── include
│   └── ThreadPool.h
├── src
│   └── ThreadPool.cpp
└── test
    ├── demo.cpp
    └── test.cpp
```

- `include/ThreadPool.h`: 线程池类声明和模板成员函数 `enqueue`
- `src/ThreadPool.cpp`: 构造函数、worker 循环、`shutdown()` 和析构逻辑
- `test/test.cpp`: 自测入口
- `test/demo.cpp`: 场景演示（基本用法、多任务并发、异常处理、队列满）
- `autobuild.sh`: 一键构建并运行测试

## Notes

- 本地构建产物默认放在 `build/` 目录，不提交到仓库。
- 如果仓库路径发生变化，`autobuild.sh` 会自动清理失效的旧 CMake 缓存。
