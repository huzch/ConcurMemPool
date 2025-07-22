#include "ConcurAlloc.h"
#include "ObjectPool.hpp"

struct TreeNode {
  int _val;
  TreeNode *_left;
  TreeNode *_right;
  TreeNode() : _val(0), _left(nullptr), _right(nullptr) {}
};

void TestObjectPool() {
  const size_t Rounds = 3;  // 申请轮次
  const size_t N = 100000;  // 每轮申请次数

  std::vector<TreeNode *> v1;
  v1.reserve(N);

  size_t begin1 = clock();
  for (size_t j = 0; j < Rounds; ++j) {
    for (int i = 0; i < N; ++i) {
      v1.push_back(new TreeNode);
    }
    for (int i = 0; i < N; ++i) {
      delete v1[i];
    }
    v1.clear();
  }
  size_t end1 = clock();

  ObjectPool<TreeNode> TNPool;
  std::vector<TreeNode *> v2;
  v2.reserve(N);

  size_t begin2 = clock();
  for (size_t j = 0; j < Rounds; ++j) {
    for (int i = 0; i < N; ++i) {
      v2.push_back(TNPool.New());
    }
    for (int i = 0; i < N; ++i) {
      TNPool.Delete(v2[i]);
    }
    v2.clear();
  }
  size_t end2 = clock();

  cout << "new cost time:" << end1 - begin1 << endl;
  cout << "object pool cost time:" << end2 - begin2 << endl;
}

void TestConcurAlloc() {
  std::thread t1([]() {
    for (size_t i = 0; i < 5; ++i) {
      ConcurAlloc(6);
    }
  });

  std::thread t2([]() {
    for (size_t i = 0; i < 5; ++i) {
      ConcurAlloc(7);
    }
  });

  t1.join();
  t2.join();
}

int main() {
  // TestObjectPool();
  TestConcurAlloc();
  return 0;
}