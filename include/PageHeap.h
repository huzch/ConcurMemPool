#pragma once
#include "Common.h"
#include "PageMap.hpp"

// 单例模式 -- 懒汉式
class PageHeap {
 public:
  static PageHeap& Instance() {
    // Magic Static，局部静态变量初始化时保证线程安全
    static PageHeap instance;
    return instance;
  }

  // 与CentralCache交互
  Span* New(size_t pages);
  void Delete(Span* span);

  Span* ObjectToSpan(void* obj);
  std::mutex& Mutex();

 private:
  PageHeap() : _idSpanMap(SystemAllocator::Alloc) {}
  PageHeap(const PageHeap&) = delete;
  PageHeap& operator=(const PageHeap&) = delete;

 private:
  SpanList _spanLists[PAGE_NUM + 1];
  PageMap3<ADDRESS_BITS - PAGE_SHIFT> _idSpanMap;  //<页号,Span*>
  std::mutex _mutex;
};