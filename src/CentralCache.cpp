#include "CentralCache.h"

#include "PageHeap.h"

// 从ThreadCache插入批量对应大小的对象
void CentralCache::InsertRange(void* start, void* end, size_t objSize) {
  assert(start && end);
  assert(objSize <= MAX_BYTES);

  size_t index = SizeMap::Index(objSize);
  SpanList& list = _spanLists[index];
  list.Mutex().lock();

  void* cur = start;
  while (cur != nullptr) {
    ReleaseToSpans(list, cur);
    cur = FreeList::Next(cur);
  }

  list.Mutex().unlock();
}

// 移除批量对应大小的对象到ThreadCache
size_t CentralCache::RemoveRange(void*& start, void*& end, size_t batchNum, size_t objSize) {
  assert(objSize <= MAX_BYTES);

  size_t index = SizeMap::Index(objSize);
  SpanList& list = _spanLists[index];
  list.Mutex().lock();

  Span* span = FetchSpan(list, objSize);
  assert(span && span->_freeList);

  size_t actualNum = 1;
  start = end = span->_freeList;
  for (size_t i = 0; i < batchNum - 1 && FreeList::Next(end) != nullptr; ++i) {
    end = FreeList::Next(end);
    ++actualNum;
  }
  span->_freeList = FreeList::Next(end);
  FreeList::Next(end) = nullptr;

  span->_useCount += actualNum;
  list.Mutex().unlock();
  return actualNum;
}

// 从PageHeap分配一个对应大小的Span
Span* CentralCache::AllocateSpan(SpanList& list, size_t objSize) {
  assert(objSize <= MAX_BYTES);
  // 解除桶锁，让ThreadCache能够释放对象给CentralCache
  list.Mutex().unlock();

  PageHeap::Instance().Mutex().lock();
  Span* span = PageHeap::Instance().New(SizeMap::PageMoveNum(objSize));
  PageHeap::Instance().Mutex().unlock();

  span->_objSize = objSize;
  span->_inUse = true;

  // 计算Span管理的大块内存的首尾地址
  char* start = (char*)(span->_start << PAGE_SHIFT);
  char* end = start + (span->_size << PAGE_SHIFT);

  // 将Span管理的大块内存切割为对象，悬挂于_freeList
  span->_freeList = start;
  char* prev = start;
  char* cur = start + objSize;
  while (cur < end) {
    FreeList::Next(prev) = cur;
    prev = cur;
    cur += objSize;
  }
  FreeList::Next(prev) = nullptr;

  // 切分Span时无需加锁，要挂入SpanList前再加桶锁
  list.Mutex().lock();
  // 将新的Span挂入对应的SpanList
  list.PushFront(span);
  return span;
}

// 释放一个对应大小的Span到PageHeap
void CentralCache::DeallocateSpans(SpanList& list, Span* span) {
  assert(span);

  list.Remove(span);
  span->_prev = nullptr;
  span->_next = nullptr;
  span->_freeList = nullptr;

  list.Mutex().unlock();

  PageHeap::Instance().Mutex().lock();
  PageHeap::Instance().Delete(span);
  PageHeap::Instance().Mutex().unlock();

  span->_inUse = false;

  list.Mutex().lock();
}

// 获取第一个非空的Span
Span* CentralCache::FetchSpan(SpanList& list, size_t objSize) {
  assert(objSize <= MAX_BYTES);

  auto cur = list.Begin();
  while (cur != list.End()) {
    if (cur->_freeList != nullptr) {
      return cur;
    }
    cur = cur->_next;
  }

  return AllocateSpan(list, objSize);
}

// 释放一个对象到对应的Span
void CentralCache::ReleaseToSpans(SpanList& list, void* obj) {
  assert(obj);

  Span* span = PageHeap::Instance().ObjectToSpan(obj);
  FreeList::Next(obj) = span->_freeList;
  span->_freeList = obj;
  --span->_useCount;

  if (span->_useCount == 0) {
    DeallocateSpans(list, span);
  }
}