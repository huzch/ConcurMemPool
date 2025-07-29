#include "ThreadCache.h"

#include "CentralCache.h"
#include "Common.h"

void* ThreadCache::Allocate(size_t bytes) {
  assert(bytes <= MAX_BYTES);
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
  assert(ptr);
  assert(bytes <= MAX_BYTES);
  size_t index = SizeMap::Index(bytes);
  _freeLists[index].Push(ptr);
}

// 从CentralCache获取批量对应大小的对象
void* ThreadCache::FetchFromCentralCache(size_t index, size_t objSize) {
  assert(objSize <= MAX_BYTES);
  FreeList& list = _freeLists[index];
  // Slow Start慢启动
  // FreeList初期对象少就分配少，后期对象多再按SizeMap规则分配
  size_t& maxSize = list.GetMaxSize();
  size_t batchNum = std::min(SizeMap::ObjectMoveNum(objSize), maxSize);
  if (batchNum == maxSize) {
    maxSize += 1;
  }

  void* start = nullptr;
  void* end = nullptr;
  size_t actualNum = CentralCache::GetInstance().RemoveRange(start, end, batchNum, objSize);
  list.PushRange(start, end, actualNum);

  return list.Pop();
}

void ThreadCache::ReleaseToCentralCache() {}
void ThreadCache::ListTooLong() {}