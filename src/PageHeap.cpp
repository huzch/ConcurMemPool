#include "PageHeap.h"

#include "Common.h"

// 分配一个对应大小的Span到CentralCache
Span* PageHeap::New(size_t pages) {
  assert(pages <= PAGE_NUM);

  if (!_spanLists[pages].Empty()) {
    return _spanLists[pages].PopFront();
  } else {
    // 向后寻找大Span(n)，将其切成Span(k)和Span(n-k)
    for (size_t i = pages + 1; i < PAGE_NUM; ++i) {
      if (!_spanLists[i].Empty()) {
        Span* kSpan = new Span;                  // k页
        Span* nSpan = _spanLists[i].PopFront();  // n页

        kSpan->_start = nSpan->_start;
        kSpan->_size = pages;
        nSpan->_start += pages;
        nSpan->_size -= pages;

        _spanLists[nSpan->_size].PushFront(nSpan);
        return kSpan;
      }
    }
  }

  // 若全为空，则向系统申请一个大Span
  void* ptr = SystemAlloc(PAGE_NUM - 1);
  Span* hugeSpan = new Span;

  hugeSpan->_start = (uintptr_t)ptr >> PAGE_SHIFT;
  hugeSpan->_size = PAGE_NUM - 1;

  _spanLists[hugeSpan->_size].PushFront(hugeSpan);
  return New(pages);  // 递归妙用
}

void PageHeap::Delete(Span* span) {}

std::mutex& PageHeap::GetMutex() { return _mutex; }