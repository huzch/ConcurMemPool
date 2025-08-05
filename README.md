# ConcurMemPool - 高性能并发内存池

## 项目简介

ConcurMemPool 是一个高性能的多线程内存池实现，旨在替代标准的 `malloc/free` 接口，提供更高效的内存分配和释放机制。该项目采用三层缓存架构设计，能够显著减少内存分配的锁竞争，提升多线程程序的性能。

## 特性

- 🚀 **高性能**: 采用三层缓存架构，减少锁竞争
- 🔒 **线程安全**: Thread Local Storage (TLS) 设计，线程独立缓存
- 📦 **内存对齐**: 分段对齐策略，控制内碎片率在 10% 以内
- 🎯 **智能分配**: 根据对象大小智能选择分配策略
- 🔄 **内存复用**: 高效的内存回收和复用机制
- 📊 **基数树映射**: 使用 PageMap 快速定位内存块

## 架构设计

### 三层缓存架构

```
┌─────────────┐    ┌──────────────┐    ┌─────────────┐
│ ThreadCache │───▶│ CentralCache │───▶│  PageHeap   │
│ (线程缓存)    │    │  (中心缓存)    │    │   (页堆)    │
└─────────────┘    └──────────────┘    └─────────────┘
       ▲                   ▲                   ▲
       │                   │                   │
   无锁访问              桶锁保护             全局锁保护
```

#### 1. ThreadCache (线程缓存)
- 每个线程独享，无锁设计
- 维护 256 个不同大小的自由链表
- 采用慢启动算法优化批量分配

#### 2. CentralCache (中心缓存)
- 全局共享，使用桶锁减少竞争
- 管理 Span 对象，负责切分和回收
- 与 ThreadCache 进行批量交互

#### 3. PageHeap (页堆)
- 系统级内存管理
- 负责向操作系统申请/释放大块内存
- 支持 Span 的合并和拆分

### 内存对齐策略

| 大小范围 | 对齐方式 | 桶数量 | 内碎片率 |
|---------|----------|--------|----------|
| [1, 128] | 8 字节对齐 | 16 个 | ≤ 12.5% |
| [129, 1024] | 16 字节对齐 | 56 个 | ≤ 6.25% |
| [1025, 8KB] | 128 字节对齐 | 56 个 | ≤ 12.5% |
| [8KB+1, 64KB] | 1024 字节对齐 | 56 个 | ≤ 12.5% |
| [64KB+1, 256KB] | 8KB 对齐 | 24 个 | ≤ 12.5% |

## 项目结构

```
ConcurMemPool/
├── include/                 # 头文件目录
│   ├── Common.h            # 公共定义和工具类
│   ├── ConcurAlloc.h       # 对外接口声明
│   ├── ThreadCache.h       # 线程缓存类
│   ├── CentralCache.h      # 中心缓存类
│   ├── PageHeap.h          # 页堆类
│   ├── ObjectPool.hpp      # 对象池模板
│   └── PageMap.hpp         # 基数树页映射
├── src/                    # 源文件目录
│   ├── Common.cpp          # 公共功能实现
│   ├── ConcurAlloc.cpp     # 主要接口实现
│   ├── ThreadCache.cpp     # 线程缓存实现
│   ├── CentralCache.cpp    # 中心缓存实现
│   └── PageHeap.cpp        # 页堆实现
├── test/                   # 测试文件目录
│   ├── UnitTest.cpp        # 单元测试
│   └── BenchMark.cpp       # 性能测试
├── build/                  # 编译输出目录
├── Makefile               # 构建脚本
└── README.md              # 项目说明
```

## 快速开始

### 编译要求

- C++17 或更高版本
- GCC 或 Clang 编译器
- Linux/macOS/Windows 系统支持

### 编译和运行

```bash
# 克隆项目
git clone https://github.com/your-username/ConcurMemPool.git
cd ConcurMemPool

# 编译项目
make

# 运行性能测试
make run

# 清理编译文件
make clean
```

### 使用示例

```cpp
#include "ConcurAlloc.h"

int main() {
    // 分配内存
    void* ptr1 = ConcurAlloc(64);    // 分配 64 字节
    void* ptr2 = ConcurAlloc(1024);  // 分配 1024 字节
    
    // 使用内存
    // ...
    
    // 释放内存
    ConcurFree(ptr1);
    ConcurFree(ptr2);
    
    return 0;
}
```

### 多线程使用

```cpp
#include "ConcurAlloc.h"
#include <thread>
#include <vector>

void worker_thread() {
    for (int i = 0; i < 10000; ++i) {
        void* ptr = ConcurAlloc(rand() % 1024 + 1);
        // 使用内存...
        ConcurFree(ptr);
    }
}

int main() {
    std::vector<std::thread> threads;
    
    // 创建多个工作线程
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back(worker_thread);
    }
    
    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }
    
    return 0;
}
```

## 性能测试

项目内置了性能测试程序，可以与标准 `malloc/free` 进行对比：

```bash
make run
```

典型测试结果：
```
==========================================================
4个线程并发执行10轮次，每轮次malloc 10000次: 花费：XXX ms
4个线程并发执行10轮次，每轮次free 10000次: 花费：XXX ms
4个线程并发malloc&free 400000次，总计花费：XXX ms


4个线程并发执行10轮次，每轮次ConcurAlloc 10000次: 花费：XXX ms
4个线程并发执行10轮次，每轮次ConcurFree 10000次: 花费：XXX ms
4个线程并发ConcurAlloc&ConcurFree 400000次，总计花费：XXX ms
==========================================================
```

## 核心技术

### 1. 线程本地存储 (TLS)
使用 `thread_local` 关键字实现线程独立的缓存，避免锁竞争。

### 2. 对象池技术
预分配内存块，避免频繁的系统调用。

### 3. 慢启动算法
ThreadCache 初期分配少量对象，随着使用频率增加逐步提升批量大小。

### 4. 基数树映射
使用多级基数树快速定位内存块所属的 Span，支持 O(1) 查找。

### 5. 内存合并算法
PageHeap 支持相邻 Span 的自动合并，减少内存碎片。

## 适用场景

- 高并发服务器应用
- 游戏引擎内存管理
- 实时系统内存分配
- 需要减少内存碎片的应用
- 对内存分配性能要求较高的程序

## 技术亮点

1. **零锁 ThreadCache**: 线程独立设计，消除锁竞争
2. **智能批量分配**: 根据使用模式动态调整批量大小
3. **高效页映射**: 基数树实现 O(1) 时间复杂度查找
4. **内存碎片控制**: 分段对齐策略将内碎片率控制在 10% 以内
5. **跨平台支持**: 支持 Windows/Linux/macOS 多平台

## 贡献

欢迎提交 Issue 和 Pull Request！

## 许可证

MIT License

## 联系方式

如有问题或建议，请通过以下方式联系：
- Email: huzch123@gmail.com
- GitHub: [huzch](https://github.com/huzch)
