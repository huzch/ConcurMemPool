#pragma once
#include "Common.h"

// 单例模式 -- 懒汉式
class CentralCache {
 public:
  static CentralCache& GetInstance() {
    // Magic Static，局部静态变量初始化时保证线程安全
    static CentralCache instance;
    return instance;
  }

  // 与ThreadCache交互
  void InsertRange(void* start, void* end);
  size_t RemoveRange(void*& start, void*& end, size_t batchNum, size_t objSize);
  // 与PageHeap交互
  Span* AllocateSpan(size_t index, size_t objSize);
  void DeallocateSpans();

  Span* GetSpan(size_t index);

 private:
  CentralCache() {}
  CentralCache(const CentralCache&) = delete;
  CentralCache& operator=(const CentralCache&) = delete;

 private:
  SpanList _spanLists[LIST_NUM];
};