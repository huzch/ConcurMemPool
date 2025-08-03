#include "ThreadCache.h"

#include "CentralCache.h"

void* ThreadCache::Allocate(size_t bytes) {
  assert(bytes <= MAX_BYTES);

  size_t index = SizeMap::Index(bytes);
  FreeList& list = _freeLists[index];

  if (!list.Empty()) {
    return list.Pop();
  } else {
    size_t alignSize = SizeMap::RoundUp(bytes);
    return FetchFromCentralCache(list, alignSize);
  }
}

void ThreadCache::Deallocate(void* ptr, size_t bytes) {
  assert(ptr);
  assert(bytes <= MAX_BYTES);

  size_t index = SizeMap::Index(bytes);
  FreeList& list = _freeLists[index];
  list.Push(ptr);

  if (list.Size() > list.MaxSize()) {
    size_t alignSize = SizeMap::RoundUp(bytes);
    ReleaseToCentralCache(list, alignSize);
  }
}

// 从CentralCache获取批量对应大小的对象
void* ThreadCache::FetchFromCentralCache(FreeList& list, size_t objSize) {
  assert(objSize <= MAX_BYTES);
  // Slow Start慢启动
  // FreeList初期对象少就分配少，后期对象多再按SizeMap规则分配
  size_t& maxSize = list.MaxSize();
  size_t batchNum = std::min(SizeMap::ObjectMoveNum(objSize), maxSize);
  if (batchNum == maxSize) {
    maxSize += 1;
  }

  void* start = nullptr;
  void* end = nullptr;
  size_t actualNum = CentralCache::Instance().RemoveRange(start, end, batchNum, objSize);
  list.PushRange(start, end, actualNum);

  return list.Pop();
}

// 释放批量对应大小的对象到CentralCache
void ThreadCache::ReleaseToCentralCache(FreeList& list, size_t objSize) {
  assert(objSize <= MAX_BYTES);

  size_t& maxSize = list.MaxSize();
  size_t batchNum = std::min(SizeMap::ObjectMoveNum(objSize), maxSize);
  if (batchNum == maxSize) {
    ++maxSize;
  }

  void* start = nullptr;
  void* end = nullptr;
  list.PopRange(start, end, batchNum);

  CentralCache::Instance().InsertRange(start, end, objSize);
}