#pragma once
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

#include <algorithm>
#include <cassert>
#include <iostream>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

using std::cout;
using std::endl;

static const size_t MAX_BYTES = 256 << 10;
static const size_t LIST_NUM = 256;
static const size_t PAGE_NUM = 128;
static const size_t PAGE_SHIFT = 13;

// 向堆申请空间
inline static void* SystemAlloc(size_t pages) {
#ifdef _WIN32
  void* ptr = VirtualAlloc(0, pages << PAGE_SHIFT, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else  // linux/macOS下用mmap分配内存
  void* ptr = mmap(nullptr, pages << PAGE_SHIFT, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (ptr == MAP_FAILED) {
    ptr = nullptr;
  }
#endif
  if (ptr == nullptr) {
    throw std::bad_alloc();
  }
  return ptr;
}

// 向堆释放空间
inline static void SystemFree(void* ptr, size_t bytes) {
#ifdef _WIN32
  VirtualFree(ptr, 0, MEM_RELEASE);
#else  // linux/macOS下用munmap分配内存
  munmap(ptr, bytes);
#endif
}

// 在每个内存块（对象）头部存储指针，指针大小兼容32位和64位平台
static void*& Next(void* obj) { return *(void**)obj; }

// 以小块内存（对象）为单位的单向链表
class FreeList {
 public:
  void Push(void* obj) {
    assert(obj);
    Next(obj) = _freeList;
    _freeList = obj;
    ++_size;
  }

  void* Pop() {
    void* obj = _freeList;
    _freeList = Next(obj);
    --_size;
    return obj;
  }

  void PushRange(void* start, void* end, size_t n) {
    assert(start && end);
    Next(end) = _freeList;
    _freeList = start;
    _size += n;
  }

  // 输出型参数
  size_t PopRange(void*& start, void*& end, size_t n) {
    start = end = _freeList;
    size_t actualNum = 1;
    for (size_t i = 0; i < n - 1 && Next(end) != nullptr; ++i) {
      end = Next(end);
      ++actualNum;
    }
    _freeList = Next(end);
    Next(end) = nullptr;
    _size -= actualNum;
    return actualNum;
  }

  bool Empty() { return _freeList == nullptr; }

  size_t& Size() { return _size; }

  size_t& MaxSize() { return _maxSize; }

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
  static size_t RoundUp(size_t bytes) {
    if (bytes <= 128) {
      return _RoundUp(bytes, 8);
    } else if (bytes <= 1024) {
      return _RoundUp(bytes, 16);
    } else if (bytes <= (8 << 10)) {
      return _RoundUp(bytes, 128);
    } else if (bytes <= (64 << 10)) {
      return _RoundUp(bytes, 1024);
    } else if (bytes <= (256 << 10)) {
      return _RoundUp(bytes, 8 << 10);
    } else {
      return _RoundUp(bytes, 1 << PAGE_SHIFT);
    }
  }

  // 输入申请字节数，返回对应哈希桶的下标索引
  static size_t Index(size_t bytes) {
    // 每个区间的桶数
    int groups[] = {16, 56, 56, 56};
    // 传参时要减去前一个区间的最大字节数
    if (bytes <= 128) {
      return _Index(bytes, 3);
    } else if (bytes <= 1024) {
      return _Index(bytes - 128, 4) + groups[0];
    } else if (bytes <= (8 << 10)) {
      return _Index(bytes - 1024, 7) + groups[0] + groups[1];
    } else if (bytes <= (64 << 10)) {
      return _Index(bytes - (8 << 10), 10) + groups[0] + groups[1] + groups[2];
    } else if (bytes <= (256 << 10)) {
      return _Index(bytes - (64 << 10), 13) + groups[0] + groups[1] + groups[2] + groups[3];
    } else {
      assert(false);
    }
  }

  // 输入对象大小，输出（从CentralCache到ThreadCache）对象移动数量
  static size_t ObjectMoveNum(size_t objSize) {
    assert(objSize <= MAX_BYTES);
    // 慢启动上限
    size_t objNum = MAX_BYTES / objSize;
    if (objNum < 2) {
      objNum = 2;
    } else if (objNum > 512) {
      objNum = 512;
    }
    return objNum;
  }

  // 输入对象大小，输出（从PageHeap到CentralCache）页移动数量
  static size_t PageMoveNum(size_t objSize) {
    assert(objSize <= MAX_BYTES);
    size_t objNum = ObjectMoveNum(objSize);
    size_t pageNum = (objNum * objSize) >> PAGE_SHIFT;
    if (pageNum == 0) {
      pageNum = 1;
    }
    return pageNum;
  }

 private:
  // size_t _RoundUp(size_t bytes, size_t alignNum) {
  //   if (bytes % 8 == 0) {
  //     return bytes;
  //   } else {
  //     return (bytes / alignNum + 1) * alignNum;
  //   }
  // }

  // 位运算写法，较精妙（代入数字便于理解）
  static size_t _RoundUp(size_t bytes, size_t alignNum) {
    return (bytes + alignNum - 1) & ~(alignNum - 1);
  }

  // size_t _Index(size_t bytes, size_t alignNum) {
  //   if (bytes % 8 == 0) {
  //     return bytes / alignNum - 1;
  //   } else {
  //     return bytes / alignNum;
  //   }
  // }

  // 计算当前区间的第几个桶
  static size_t _Index(size_t bytes, size_t alignShift) {
    return ((bytes + (1 << alignShift) - 1) >> alignShift) - 1;
  }
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
  SpanList() : _head(spanPool.New()) {
    _head->_prev = _head;
    _head->_next = _head;
  }

  Span* Begin() { return _head->_next; }

  Span* End() { return _head; }

  void PushFront(Span* span) { Insert(Begin(), span); }

  Span* PopFront() { return Remove(Begin()); }

  void Insert(Span* pos, Span* span) {
    assert(pos && span);
    Span* prev = pos->_prev;

    prev->_next = span;
    span->_prev = prev;
    span->_next = pos;
    pos->_prev = span;
  }

  Span* Remove(Span* pos) {
    assert(pos && pos != _head);
    Span* prev = pos->_prev;
    Span* next = pos->_next;

    prev->_next = next;
    next->_prev = prev;
    return pos;
  }

  bool Empty() { return _head == _head->_next; }

  std::mutex& Mutex() { return _mutex; }

 private:
  Span* _head;        // 哨兵位
  std::mutex _mutex;  // 桶锁
};