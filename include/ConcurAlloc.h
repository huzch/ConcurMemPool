#pragma once
#include "ThreadCache.h"

// 以下函数用于配合线程并发测试
void* ConcurAlloc(size_t bytes) {
  if (pThreadCache == nullptr) {
    pThreadCache = new ThreadCache;
  }
  cout << std::this_thread::get_id() << ":" << pThreadCache << endl;
  return pThreadCache->Allocate(bytes);
}

void ConcurFree() {};