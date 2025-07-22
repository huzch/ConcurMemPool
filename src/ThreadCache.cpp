#include "ThreadCache.h"

#include "Common.h"

void* ThreadCache::Allocate(size_t bytes) {
  size_t index = SizeMap::Index(bytes);

  if (!_freeLists[index].Empty()) {
    return _freeLists[index].Pop();
  } else {
    size_t alignSize = SizeMap::RoundUp(bytes);
    return FetchFromCentralCache(index, alignSize);
  }
}

// 后续会优化，函数只用传入指针
void ThreadCache::Deallocate(void* ptr, size_t bytes) {
  size_t index = SizeMap::Index(bytes);
  _freeLists[index].Push(ptr);
}

void* ThreadCache::FetchFromCentralCache(size_t index, size_t bytes) {
  return nullptr;
}