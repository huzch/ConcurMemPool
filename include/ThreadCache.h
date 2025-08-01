#pragma once
#include "Common.h"

class ThreadCache {
 public:
  // 与Thread交互
  void* Allocate(size_t bytes);
  void Deallocate(void* ptr, size_t bytes);
  // 与CentralCache交互
  void* FetchFromCentralCache(FreeList& list, size_t objSize);
  void ReleaseToCentralCache(FreeList& list, size_t objSize);

 private:
  FreeList _freeLists[LIST_NUM];
};

// TLS:Thread Local Storage
// 线程独立缓存，无锁设计，提升性能
static thread_local ThreadCache* pThreadCache = nullptr;

static ObjectPool<ThreadCache> tcPool;