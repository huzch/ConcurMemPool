#include "ConcurAlloc.h"

// 对外申请内存接口（代替malloc）
void* ConcurAlloc(size_t bytes) {
  // 小于256KB内存，缓存架构申请
  if (bytes <= MAX_BYTES) {
    if (pThreadCache == nullptr) {
      pThreadCache = tcPool.New();
    }
    cout << std::this_thread::get_id() << ":" << pThreadCache << endl;
    return pThreadCache->Allocate(bytes);
  }
  // 大于256KB但小于1024KB(128页)，直接向PageHeap申请
  else if (bytes <= (PAGE_NUM << PAGE_SHIFT)) {
    size_t pages = SizeMap::RoundUp(bytes) >> PAGE_SHIFT;

    PageHeap::Instance().Mutex().lock();
    Span* span = PageHeap::Instance().New(pages);
    PageHeap::Instance().Mutex().unlock();

    span->_inUse = true;

    void* ptr = (void*)(span->_start << PAGE_SHIFT);
    return ptr;
  }
  // 大于1024KB(128页)，直接向堆申请
  else {
    size_t pages = SizeMap::RoundUp(bytes) >> PAGE_SHIFT;
    return SystemAllocator::Alloc(pages);
  }
}

// 对外释放内存接口（代替free）
void ConcurFree(void* ptr) {
  assert(ptr);

  Span* span = PageHeap::Instance().ObjectToSpan(ptr);
  size_t objSize = span->_objSize;

  // 小于256KB内存，缓存架构释放
  if (objSize <= MAX_BYTES) {
    assert(pThreadCache);
    pThreadCache->Deallocate(ptr, objSize);
  }
  // 大于256KB但小于1024KB(128页)，直接向PageHeap释放
  else if (objSize <= (PAGE_NUM << PAGE_SHIFT)) {
    PageHeap::Instance().Mutex().lock();
    PageHeap::Instance().Delete(span);
    PageHeap::Instance().Mutex().unlock();

    span->_inUse = false;
  }
  // 大于1024KB(128页)，直接向堆释放
  else {
    SystemAllocator::Free(ptr);
  }
}