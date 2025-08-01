#pragma once
#include "Common.h"

// 针对不同大小的特定对象的定长内存池
template <class T>
class ObjectPool {
 public:
  // 每次分配对象大小的内存
  T *New() {
    T *obj = nullptr;

    if (_freeList) {  // 利用空闲回收内存
      obj = (T *)_freeList;
      _freeList = Next(obj);
    } else {  // 利用申请内存
      if (_remainBytes < sizeof(T)) {
        _remainBytes = 16 << PAGE_SHIFT;  // 一次申请16页
        // 此处会直接丢弃小于对象大小的剩余内存，会造成内存泄漏（后续等待修补）
        _memory = (char *)SystemAlloc(16);
        if (_memory == nullptr) {
          throw std::bad_alloc();
        }
      }

      obj = (T *)_memory;
      // 保证分配内存大小能存储指针
      size_t objSize = sizeof(T) > sizeof(void *) ? sizeof(T) : sizeof(void *);
      _memory += objSize;
      _remainBytes -= objSize;
    }

    new (obj) T;  // 定位new，调用构造函数初始化对象资源
    return obj;
  }

  void Delete(T *obj) {
    assert(obj);
    obj->~T();  // 调用析构函数清理对象资源

    // 在回收内存的头部存储指针，将所有回收内存链接起来
    Next(obj) = _freeList;
    _freeList = obj;
  }

 private:
  char *_memory = nullptr;    // 大块分配内存
  size_t _remainBytes = 0;    // 大块分配内存的剩余字节数
  void *_freeList = nullptr;  // 回收内存
};
