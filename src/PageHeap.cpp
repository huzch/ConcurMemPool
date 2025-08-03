#include "PageHeap.h"

// 分配一个对应大小的Span到CentralCache
Span* PageHeap::New(size_t pages) {
  assert(pages <= PAGE_NUM);

  if (!_spanLists[pages].Empty()) {
    return _spanLists[pages].PopFront();
  } else {
    // 向后寻找大Span(n)，将其切成Span(k)和Span(n-k)
    for (size_t i = pages + 1; i <= PAGE_NUM; ++i) {
      if (!_spanLists[i].Empty()) {
        Span* kSpan = spanPool.New();            // k页
        Span* nSpan = _spanLists[i].PopFront();  // n页

        kSpan->_start = nSpan->_start;
        kSpan->_size = pages;
        // 标记首尾页映射
        _idSpanMap[kSpan->_start] = kSpan;
        _idSpanMap[kSpan->_start + kSpan->_size - 1] = kSpan;

        nSpan->_start += pages;
        nSpan->_size -= pages;
        // 标记首尾页映射
        _idSpanMap[nSpan->_start] = nSpan;
        _idSpanMap[nSpan->_start + nSpan->_size - 1] = nSpan;

        _spanLists[nSpan->_size].PushFront(nSpan);
        return kSpan;
      }
    }
  }

  // 若全为空，则向系统申请一个大Span
  void* ptr = SystemAllocator::Alloc(PAGE_NUM);
  Span* hugeSpan = spanPool.New();

  hugeSpan->_start = (uintptr_t)ptr >> PAGE_SHIFT;
  hugeSpan->_size = PAGE_NUM;
  // 标记首尾页映射
  _idSpanMap[hugeSpan->_start] = hugeSpan;
  _idSpanMap[hugeSpan->_start + hugeSpan->_size - 1] = hugeSpan;

  _spanLists[hugeSpan->_size].PushFront(hugeSpan);
  return New(pages);  // 递归复用
}

// 从CentralCache释放一个对应大小的Span
void PageHeap::Delete(Span* span) {
  assert(span);
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

    _idSpanMap.erase(prevSpan->_start);
    _idSpanMap.erase(prevSpan->_start + prevSpan->_size - 1);
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

    _idSpanMap.erase(nextSpan->_start);
    _idSpanMap.erase(nextSpan->_start + nextSpan->_size - 1);
    _spanLists[nextSpan->_size].Remove(nextSpan);
    spanPool.Delete(nextSpan);
  }

  _idSpanMap[span->_start] = span;
  _idSpanMap[span->_start + span->_size - 1] = span;
  _spanLists[span->_size].PushFront(span);
}

// 将对象Object映射到对应的Span
Span* PageHeap::ObjectToSpan(void* obj) {
  assert(obj);
  // 计算对象所属页号
  uintptr_t start = (uintptr_t)obj >> PAGE_SHIFT;
  return _idSpanMap[start];
}

std::mutex& PageHeap::Mutex() { return _mutex; }