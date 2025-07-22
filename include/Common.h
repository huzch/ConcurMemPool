#pragma once
#include <cassert>
#include <iostream>
#include <thread>
#include <vector>
using std::cout;
using std::endl;

static const int FREELIST_NUM = 256;

// 指针大小兼容32位和64位平台
static void*& Next(void* obj) { return *(void**)obj; }

class FreeList {
 public:
  void Push(void* obj) {
    Next(obj) = _freeList;
    _freeList = obj;
  }

  void* Pop() {
    void* obj = _freeList;
    _freeList = Next(obj);
    return obj;
  }

  bool Empty() { return _freeList == nullptr; }

 private:
  void* _freeList = nullptr;
};

class SizeMap {
 public:
  //  由于只用8byte对齐会导致桶数太多，所以采用分段对齐，在保证内碎片不大幅增加的情况下，减少桶数
  //  整体控制在最多10%左右的内碎片浪费
  //  [1,128]               8byte对齐       freelist[0,16)
  //  [128+1,1024]          16byte对齐      freelist[16,72)
  //  [1024+1,8*1024]       128byte对齐     freelist[72,128)
  //  [8*1024+1,64*1024]    1024byte对齐    freelist[128,184)
  //  [64*1024+1,256*1024]  8*1024byte对齐  freelist[184,208)

  // 输入申请字节数，返回对齐字节数
  static size_t RoundUp(size_t bytes) {
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
      assert(false);
    }
  }

  // 输入申请字节数，返回对应哈希桶的下标索引
  static size_t Index(size_t bytes) {
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
      return _Index(bytes - (64 << 10), 13) + groups[0] + groups[1] +
             groups[2] + groups[3];
    } else {
      assert(false);
    }
  }

 private:
  // size_t _RoundUp(size_t bytes, size_t alignNum) {
  //   if (bytes % 8 == 0) {
  //     return bytes;
  //   } else {
  //     return (bytes / alignNum + 1) * alignNum;
  //   }
  // }

  // 位运算写法，较精妙（代入数字便于理解）
  static size_t _RoundUp(size_t bytes, size_t alignNum) {
    return (bytes + alignNum - 1) & ~(alignNum - 1);
  }

  // size_t _Index(size_t bytes, size_t alignNum) {
  //   if (bytes % 8 == 0) {
  //     return bytes / alignNum - 1;
  //   } else {
  //     return bytes / alignNum;
  //   }
  // }

  // 计算当前区间的第几个桶
  static size_t _Index(size_t bytes, size_t alignShift) {
    return ((bytes + (1 << alignShift) - 1) >> alignShift) - 1;
  }
};
