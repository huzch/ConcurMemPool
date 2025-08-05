#pragma once
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

#include <algorithm>
#include <atomic>
#include <cassert>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

using std::cout;
using std::endl;

static const size_t MAX_BYTES = 256 << 10;
static const size_t LIST_NUM = 256;
static const size_t PAGE_NUM = 128;
static const size_t PAGE_SHIFT = 13;
static const size_t ADDRESS_BITS = sizeof(void*) << 3;

// 用于向操作系统申请与释放内存
class SystemAllocator {
 public:
  // 向堆申请空间
  static void* Alloc(size_t bytes);
  // 向堆释放空间
  static void Free(void* ptr, size_t bytes);
};

// 以小块内存（对象）为单位的单向链表
class FreeList {
 public:
  // 在每个内存块（对象）头部存储指针，指针大小兼容32位和64位平台
  static void*& Next(void* obj);

  void Push(void* obj);

  void* Pop();

  void PushRange(void* start, void* end, size_t n);

  // 输出型参数
  size_t PopRange(void*& start, void*& end, size_t n);

  bool Empty();

  size_t& Size();

  size_t& MaxSize();

 private:
  void* _freeList = nullptr;
  size_t _size = 0;
  size_t _maxSize = 1;  // 慢启动上限
};

// 字节对齐和哈希桶映射规则
class SizeMap {
 public:
  //  由于只用8byte对齐会导致桶数太多，所以采用分段对齐，在保证内碎片不大幅增加的情况下，减少桶数
  //  整体控制在最多10%左右的内碎片浪费
  //  [1,128]               8byte对齐       freelist[0,16)
  //  [128+1,1024]          16byte对齐      freelist[16,72)
  //  [1024+1,8*1024]       128byte对齐     freelist[72,128)
  //  [8*1024+1,64*1024]    1024byte对齐    freelist[128,184)
  //  [64*1024+1,256*1024]  8*1024byte对齐  freelist[184,208)

  // 输入申请字节数，返回对齐字节数
  static size_t RoundUp(size_t bytes);

  // 输入申请字节数，返回对应哈希桶的下标索引
  static size_t Index(size_t bytes);

  // 输入对象大小，输出（从CentralCache到ThreadCache）对象移动数量
  static size_t ObjectMoveNum(size_t objSize);

  // 输入对象大小，输出（从PageHeap到CentralCache）页移动数量
  static size_t PageMoveNum(size_t objSize);

 private:
  // size_t _RoundUp(size_t bytes, size_t alignNum);

  // 位运算写法，较精妙（代入数字便于理解）
  static size_t _RoundUp(size_t bytes, size_t alignNum);

  // size_t _Index(size_t bytes, size_t alignNum);

  // 计算当前区间的第几个桶
  static size_t _Index(size_t bytes, size_t alignShift);
};

// 以页为单位的连续大块内存
struct Span {
  // 采用uintptr_t兼容32位和64位平台
  uintptr_t _start = 0;  // 起始页号
  uintptr_t _size = 0;   // 页的数量

  // 双向链表
  Span* _prev = nullptr;
  Span* _next = nullptr;

  size_t _objSize = 0;        // 对象大小
  size_t _useCount = 0;       // 对象分配数量
  void* _freeList = nullptr;  // 对象空闲链表

  bool _inUse = false;  // Span是否被使用
};

#include "ObjectPool.hpp"
static ObjectPool<Span> spanPool;

// 以大块内存为单位的双向链表
class SpanList {
 public:
  SpanList();

  Span* Begin();

  Span* End();

  void PushFront(Span* span);

  Span* PopFront();

  void Insert(Span* pos, Span* span);

  Span* Remove(Span* pos);

  bool Empty();

  std::mutex& Mutex();

 private:
  Span* _head;        // 哨兵位
  std::mutex _mutex;  // 桶锁
};