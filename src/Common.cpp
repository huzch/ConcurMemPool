#include "Common.h"

// 向堆申请空间
void* SystemAllocator::Alloc(size_t bytes) {
#ifdef _WIN32
  void* ptr = VirtualAlloc(0, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else  // linux/macOS下用mmap分配内存
  void* ptr = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (ptr == MAP_FAILED) {
    ptr = nullptr;
  }
#endif
  if (ptr == nullptr) {
    throw std::bad_alloc();
  }
  return ptr;
}

// 向堆释放空间
void SystemAllocator::Free(void* ptr, size_t bytes) {
#ifdef _WIN32
  VirtualFree(ptr, 0, MEM_RELEASE);
#else  // linux/macOS下用munmap分配内存
  munmap(ptr, bytes);
#endif
}

// 在每个内存块（对象）头部存储指针，指针大小兼容32位和64位平台
void*& FreeList::Next(void* obj) { return *(void**)obj; }

void FreeList::Push(void* obj) {
  assert(obj);
  Next(obj) = _freeList;
  _freeList = obj;
  ++_size;
}

void* FreeList::Pop() {
  assert(_freeList);
  void* obj = _freeList;
  _freeList = Next(obj);
  --_size;
  return obj;
}

void FreeList::PushRange(void* start, void* end, size_t n) {
  assert(start && end);
  Next(end) = _freeList;
  _freeList = start;
  _size += n;
}

// 输出型参数
size_t FreeList::PopRange(void*& start, void*& end, size_t n) {
  start = end = _freeList;
  size_t actualNum = 1;
  for (size_t i = 0; i < n - 1 && Next(end) != nullptr; ++i) {
    end = Next(end);
    ++actualNum;
  }
  _freeList = Next(end);
  Next(end) = nullptr;
  _size -= actualNum;
  return actualNum;
}

bool FreeList::Empty() { return _freeList == nullptr; }

size_t& FreeList::Size() { return _size; }

size_t& FreeList::MaxSize() { return _maxSize; }

// 输入申请字节数，返回对齐字节数
size_t SizeMap::RoundUp(size_t bytes) {
  if (bytes <= 128) {
    return _RoundUp(bytes, 8);
  } else if (bytes <= 1024) {
    return _RoundUp(bytes, 16);
  } else if (bytes <= (8 << 10)) {
    return _RoundUp(bytes, 128);
  } else if (bytes <= (64 << 10)) {
    return _RoundUp(bytes, 1024);
  } else if (bytes <= (256 << 10)) {
    return _RoundUp(bytes, 8 << 10);
  } else {
    return _RoundUp(bytes, 1 << PAGE_SHIFT);
  }
}

// 输入申请字节数，返回对应哈希桶的下标索引
size_t SizeMap::Index(size_t bytes) {
  // 每个区间的桶数
  int groups[] = {16, 56, 56, 56};
  // 传参时要减去前一个区间的最大字节数
  if (bytes <= 128) {
    return _Index(bytes, 3);
  } else if (bytes <= 1024) {
    return _Index(bytes - 128, 4) + groups[0];
  } else if (bytes <= (8 << 10)) {
    return _Index(bytes - 1024, 7) + groups[0] + groups[1];
  } else if (bytes <= (64 << 10)) {
    return _Index(bytes - (8 << 10), 10) + groups[0] + groups[1] + groups[2];
  } else if (bytes <= (256 << 10)) {
    return _Index(bytes - (64 << 10), 13) + groups[0] + groups[1] + groups[2] + groups[3];
  } else {
    assert(false);
  }
}

// 输入对象大小，输出（从CentralCache到ThreadCache）对象移动数量
size_t SizeMap::ObjectMoveNum(size_t objSize) {
  assert(objSize <= MAX_BYTES);
  // 慢启动上限
  size_t objNum = MAX_BYTES / objSize;
  if (objNum < 2) {
    objNum = 2;
  } else if (objNum > 512) {
    objNum = 512;
  }
  return objNum;
}

// 输入对象大小，输出（从PageHeap到CentralCache）页移动数量
size_t SizeMap::PageMoveNum(size_t objSize) {
  assert(objSize <= MAX_BYTES);
  size_t objNum = ObjectMoveNum(objSize);
  size_t pageNum = (objNum * objSize) >> PAGE_SHIFT;
  if (pageNum == 0) {
    pageNum = 1;
  }
  return pageNum;
}

// size_t SizeMap::_RoundUp(size_t bytes, size_t alignNum) {
//   if (bytes % 8 == 0) {
//     return bytes;
//   } else {
//     return (bytes / alignNum + 1) * alignNum;
//   }
// }

// 位运算写法，较精妙（代入数字便于理解）
size_t SizeMap::_RoundUp(size_t bytes, size_t alignNum) {
  return (bytes + alignNum - 1) & ~(alignNum - 1);
}

// size_t SizeMap::_Index(size_t bytes, size_t alignNum) {
//   if (bytes % 8 == 0) {
//     return bytes / alignNum - 1;
//   } else {
//     return bytes / alignNum;
//   }
// }

// 计算当前区间的第几个桶
size_t SizeMap::_Index(size_t bytes, size_t alignShift) {
  return ((bytes + (1 << alignShift) - 1) >> alignShift) - 1;
}

SpanList::SpanList() : _head(spanPool.New()) {
  _head->_prev = _head;
  _head->_next = _head;
}

Span* SpanList::Begin() { return _head->_next; }

Span* SpanList::End() { return _head; }

void SpanList::PushFront(Span* span) { Insert(Begin(), span); }

Span* SpanList::PopFront() { return Remove(Begin()); }

void SpanList::Insert(Span* pos, Span* span) {
  assert(pos && span);
  Span* prev = pos->_prev;

  prev->_next = span;
  span->_prev = prev;
  span->_next = pos;
  pos->_prev = span;
}

Span* SpanList::Remove(Span* pos) {
  assert(pos && pos != _head);
  Span* prev = pos->_prev;
  Span* next = pos->_next;

  prev->_next = next;
  next->_prev = prev;
  return pos;
}

bool SpanList::Empty() { return _head == _head->_next; }

std::mutex& SpanList::Mutex() { return _mutex; }