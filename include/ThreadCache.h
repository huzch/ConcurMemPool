#pragma once
#include "Common.h"

class ThreadCache {
 public:
  void* Allocate(size_t bytes);
  void Deallocate(void* ptr, size_t bytes);
  void* FetchFromCentralCache(size_t index, size_t bytes);

 private:
  FreeList _freeLists[FREELIST_NUM];
};

// TLS:Thread Local Storage
// 线程独立缓存，无锁设计，提升性能
static thread_local ThreadCache* pThreadCache = nullptr;