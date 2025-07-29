#include "CentralCache.h"

#include "Common.h"
#include "PageHeap.h"

// 移除批量对应大小的对象到ThreadCache
size_t CentralCache::RemoveRange(void*& start, void*& end, size_t batchNum, size_t objSize) {
  assert(objSize <= MAX_BYTES);
  size_t index = SizeMap::Index(objSize);
  _spanLists[index].GetMutex().lock();

  Span* span = GetSpan(index);
  if (span == nullptr) {
    span = AllocateSpan(index, objSize);
  }
  assert(span && span->_freeList);

  size_t actualNum = 1;
  start = end = span->_freeList;
  for (size_t i = 0; i < batchNum - 1 && Next(end) != nullptr; ++i) {
    end = Next(end);
    ++actualNum;
  }
  span->_freeList = Next(end);
  Next(end) = nullptr;

  span->_useCount += actualNum;
  _spanLists[index].GetMutex().unlock();
  return actualNum;
}

// 从PageHeap分配一个对应大小的Span
Span* CentralCache::AllocateSpan(size_t index, size_t objSize) {
  assert(objSize <= MAX_BYTES);
  // 解除桶锁，让ThreadCache能够释放对象给CentralCache
  _spanLists[index].GetMutex().unlock();

  PageHeap::GetInstance().GetMutex().lock();
  Span* span = PageHeap::GetInstance().New(SizeMap::PageMoveNum(objSize));
  PageHeap::GetInstance().GetMutex().unlock();

  span->_objSize = objSize;
  span->_isUsed = true;

  // 计算Span管理的大块内存的首尾地址
  char* start = (char*)(span->_start << PAGE_SHIFT);
  char* end = start + (span->_size << PAGE_SHIFT);

  // 将Span管理的大块内存切割为对象，悬挂于_freeList
  span->_freeList = start;
  char* prev = start;
  char* cur = start + objSize;
  while (cur < end) {
    Next(prev) = cur;
    prev = cur;
    cur += objSize;
  }
  Next(prev) = nullptr;

  // 切分Span时无需加锁，要挂入SpanList前再加桶锁
  _spanLists[index].GetMutex().lock();
  // 将新的Span挂入对应的SpanList
  _spanLists[index].PushFront(span);
  return span;
}

// 获取第一个非空的Span
Span* CentralCache::GetSpan(size_t index) {
  SpanList& list = _spanLists[index];

  auto cur = list.Begin();
  while (cur != list.End()) {
    if (cur != nullptr) {
      return cur;
    }
    cur = cur->_next;
  }
  return nullptr;
}