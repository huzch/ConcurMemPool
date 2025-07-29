#pragma once
#include "Common.h"

// 单例模式 -- 懒汉式
class PageHeap {
 public:
  static PageHeap& GetInstance() {
    // Magic Static，局部静态变量初始化时保证线程安全
    static PageHeap instance;
    return instance;
  }

  Span* New(size_t pages);
  void Delete(Span* span);

  std::mutex& GetMutex();

 private:
  PageHeap() {}
  PageHeap(const PageHeap&) = delete;
  PageHeap& operator=(const PageHeap&) = delete;

 private:
  SpanList _spanLists[PAGE_NUM];
  std::mutex _mutex;
};