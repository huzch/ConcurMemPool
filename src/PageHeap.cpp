#include "PageHeap.h"

// 分配一个对应大小的Span到CentralCache
Span* PageHeap::New(size_t pages) {
  // 直接向堆申请
  if (pages > PAGE_NUM) {
    Span* span = spanPool.New();
    void* ptr = SystemAllocator::Alloc(pages);

    span->_start = (uintptr_t)ptr >> PAGE_SHIFT;
    span->_size = pages;
    _idSpanMap[span->_start] = span;

    span->_inUse = true;
    return span;
  }

  if (!_spanLists[pages].Empty()) {
    Span* span = _spanLists[pages].PopFront();
    span->_inUse = true;
    return span;
  } else {
    // 向后寻找大Span(n)，将其切成Span(k)和Span(n-k)
    for (size_t i = pages + 1; i <= PAGE_NUM; ++i) {
      if (!_spanLists[i].Empty()) {
        Span* kSpan = spanPool.New();            // k页
        Span* nSpan = _spanLists[i].PopFront();  // n页

        kSpan->_start = nSpan->_start;
        kSpan->_size = pages;
        // 标记页映射
        for (size_t i = 0; i < kSpan->_size; ++i) {
          _idSpanMap[kSpan->_start + i] = kSpan;
        }

        nSpan->_start += pages;
        nSpan->_size -= pages;

        _spanLists[nSpan->_size].PushFront(nSpan);
        kSpan->_inUse = true;
        return kSpan;
      }
    }
  }

  // 若全为空，则向系统申请一个大Span
  void* ptr = SystemAllocator::Alloc(PAGE_NUM);
  Span* hugeSpan = spanPool.New();

  hugeSpan->_start = (uintptr_t)ptr >> PAGE_SHIFT;
  hugeSpan->_size = PAGE_NUM;
  // 标记页映射
  for (size_t i = 0; i < hugeSpan->_size; ++i) {
    _idSpanMap[hugeSpan->_start + i] = hugeSpan;
  }

  _spanLists[hugeSpan->_size].PushFront(hugeSpan);
  return New(pages);  // 递归复用
}

// 从CentralCache释放一个对应大小的Span
void PageHeap::Delete(Span* span) {
  assert(span);
  // 直接向堆释放
  size_t pages = span->_size;
  if (pages > PAGE_NUM) {
    void* ptr = (void*)(span->_start << PAGE_SHIFT);
    SystemAllocator::Free(ptr, pages);
    spanPool.Delete(span);
    return;
  }

  // 向前合并
  while (true) {
    uintptr_t prevId = span->_start - 1;
    if (!_idSpanMap.count(prevId)) {
      break;
    }

    Span* prevSpan = _idSpanMap[prevId];
    if (prevSpan->_inUse) {
      break;
    } else if (prevSpan->_size + span->_size > PAGE_NUM) {
      break;
    }

    span->_start = prevSpan->_start;
    span->_size += prevSpan->_size;

    _spanLists[prevSpan->_size].Remove(prevSpan);
    spanPool.Delete(prevSpan);
  }
  // 向后合并
  while (true) {
    uintptr_t nextId = span->_start + span->_size;
    if (!_idSpanMap.count(nextId)) {
      break;
    }

    Span* nextSpan = _idSpanMap[nextId];
    if (nextSpan->_inUse) {
      break;
    } else if (nextSpan->_size + span->_size > PAGE_NUM) {
      break;
    }

    span->_size += nextSpan->_size;

    _spanLists[nextSpan->_size].Remove(nextSpan);
    spanPool.Delete(nextSpan);
  }

  // 标记页映射
  for (size_t i = 0; i < span->_size; ++i) {
    _idSpanMap[span->_start + i] = span;
  }
  _spanLists[span->_size].PushFront(span);
  span->_inUse = false;
}

// 将对象Object映射到对应的Span
Span* PageHeap::ObjectToSpan(void* obj) {
  assert(obj);
  std::lock_guard<std::mutex> lock(_mutex);
  // 计算对象所属页号
  uintptr_t start = (uintptr_t)obj >> PAGE_SHIFT;
  return _idSpanMap[start];
}

std::mutex& PageHeap::Mutex() { return _mutex; }