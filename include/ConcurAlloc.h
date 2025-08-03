#pragma once
#include "Common.h"
#include "PageHeap.h"
#include "ThreadCache.h"

// 对外申请内存接口（代替malloc）
void* ConcurAlloc(size_t bytes);

// 对外释放内存接口（代替free）
void ConcurFree(void* ptr);