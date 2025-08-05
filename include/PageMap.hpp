#include "Common.h"

// Single-level array
template <int BITS>
class PageMap1 {
 private:
  static const int LENGTH = 1 << BITS;

  void** _array;

 public:
  typedef uintptr_t Number;

  explicit PageMap1(void* (*allocator)(size_t)) {
    _array = reinterpret_cast<void**>((*allocator)(sizeof(void*) << BITS));
    memset(_array, 0, sizeof(void*) << BITS);
  }

  // Ensure that the map contains initialized entries "x .. x+n-1".
  // Returns true if successful, false if we could not allocate memory.
  bool Ensure(Number x, size_t n) {
    // Nothing to do since flat array was allocated at start.  All
    // that's left is to check for overflow (that is, we don't want to
    // ensure a number y where _array[y] would be an out-of-bounds
    // access).
    return n <= LENGTH - x;  // an overflow-free way to do "x + n <= LENGTH"
  }

  void PreallocateMoreMemory() {}

  // Return the current value for KEY.  Returns nullptr if not yet
  // set, or if k is out of range.
  void* get(Number k) const {
    if ((k >> BITS) > 0) {
      return nullptr;
    }
    return _array[k];
  }

  // REQUIRES "k" is in range "[0,2^BITS-1]".
  // REQUIRES "k" has been ensured before.
  //
  // Sets the value 'v' for key 'k'.
  void set(Number k, void* v) { _array[k] = v; }

  // Return the first non-nullptr pointer found in this map for a page
  // number >= k.  Returns nullptr if no such number is found.
  void* Next(Number k) const {
    while (k < (1 << BITS)) {
      if (_array[k] != nullptr) return _array[k];
      k++;
    }
    return nullptr;
  }
};

// Two-level radix tree
template <int BITS>
class PageMap2 {
 private:
  static const int LEAF_BITS = (BITS + 1) / 2;
  static const int LEAF_LENGTH = 1 << LEAF_BITS;

  static const int ROOT_BITS = BITS - LEAF_BITS;
  static const int ROOT_LENGTH = 1 << ROOT_BITS;

  // Leaf node
  struct Leaf {
    void* values[LEAF_LENGTH];
  };

  Leaf* _root[ROOT_LENGTH];     // Pointers to child nodes
  void* (*_allocator)(size_t);  // Memory allocator

 public:
  typedef uintptr_t Number;

  explicit PageMap2(void* (*allocator)(size_t)) {
    _allocator = allocator;
    memset(_root, 0, sizeof(_root));
  }

  void* get(Number k) const {
    const Number i1 = k >> LEAF_BITS;
    const Number i2 = k & (LEAF_LENGTH - 1);
    if ((k >> BITS) > 0 || _root[i1] == nullptr) {
      return nullptr;
    }
    return _root[i1]->values[i2];
  }

  void set(Number k, void* v) {
    const Number i1 = k >> LEAF_BITS;
    const Number i2 = k & (LEAF_LENGTH - 1);
    assert(i1 < ROOT_LENGTH);

    // Ensure the necessary nodes exist
    if (!Ensure(k, 1)) return;  // allocation failed

    _root[i1]->values[i2] = v;
  }

  bool Ensure(Number start, size_t n) {
    for (Number key = start; key <= start + n - 1;) {
      const Number i1 = key >> LEAF_BITS;

      // Check for overflow
      if (i1 >= ROOT_LENGTH) return false;

      // Make 2nd level node if necessary
      if (_root[i1] == nullptr) {
        Leaf* leaf = reinterpret_cast<Leaf*>((*_allocator)(sizeof(Leaf)));
        if (leaf == nullptr) return false;
        memset(leaf, 0, sizeof(*leaf));
        _root[i1] = leaf;
      }

      // Advance key past whatever is covered by this leaf node
      key = ((key >> LEAF_BITS) + 1) << LEAF_BITS;
    }
    return true;
  }

  void PreallocateMoreMemory() {
    // Allocate enough to keep track of all possible pages
    if (BITS < 20) {
      Ensure(0, Number(1) << BITS);
    }
  }

  void* Next(Number k) const {
    while (k < (Number(1) << BITS)) {
      const Number i1 = k >> LEAF_BITS;
      Leaf* leaf = _root[i1];
      if (leaf != nullptr) {
        // Scan forward in leaf
        for (Number i2 = k & (LEAF_LENGTH - 1); i2 < LEAF_LENGTH; i2++) {
          if (leaf->values[i2] != nullptr) {
            return leaf->values[i2];
          }
        }
      }
      // Skip to next top-level entry
      k = (i1 + 1) << LEAF_BITS;
    }
    return nullptr;
  }
};

// Three-level radix tree
template <int BITS>
class PageMap3 {
 private:
  // How many bits should we consume at each interior level
  static const int INTERIOR_BITS = (BITS + 2) / 3;  // Round-up
  static const int INTERIOR_LENGTH = 1 << INTERIOR_BITS;

  // How many bits should we consume at leaf level
  static const int LEAF_BITS = BITS - 2 * INTERIOR_BITS;
  static const int LEAF_LENGTH = 1 << LEAF_BITS;

  // Interior node
  struct Node {
    Node* ptrs[INTERIOR_LENGTH];
  };

  // Leaf node
  struct Leaf {
    void* values[LEAF_LENGTH];
  };

  Node _root;                   // Root of radix tree
  void* (*_allocator)(size_t);  // Memory allocator

  Node* NewNode() {
    Node* result = reinterpret_cast<Node*>((*_allocator)(sizeof(Node)));
    if (result != nullptr) {
      memset(result, 0, sizeof(*result));
    }
    return result;
  }

 public:
  typedef uintptr_t Number;

  explicit PageMap3(void* (*allocator)(size_t)) {
    _allocator = allocator;
    memset(&_root, 0, sizeof(_root));
  }

  void* get(Number k) const {
    const Number i1 = k >> (LEAF_BITS + INTERIOR_BITS);
    const Number i2 = (k >> LEAF_BITS) & (INTERIOR_LENGTH - 1);
    const Number i3 = k & (LEAF_LENGTH - 1);
    if ((k >> BITS) > 0 || _root.ptrs[i1] == nullptr || _root.ptrs[i1]->ptrs[i2] == nullptr) {
      return nullptr;
    }
    return reinterpret_cast<Leaf*>(_root.ptrs[i1]->ptrs[i2])->values[i3];
  }

  void set(Number k, void* v) {
    assert(k >> BITS == 0);
    const Number i1 = k >> (LEAF_BITS + INTERIOR_BITS);
    const Number i2 = (k >> LEAF_BITS) & (INTERIOR_LENGTH - 1);
    const Number i3 = k & (LEAF_LENGTH - 1);

    // Ensure the necessary nodes exist
    if (!Ensure(k, 1)) return;  // allocation failed

    reinterpret_cast<Leaf*>(_root.ptrs[i1]->ptrs[i2])->values[i3] = v;
  }

  bool Ensure(Number start, size_t n) {
    for (Number key = start; key <= start + n - 1;) {
      const Number i1 = key >> (LEAF_BITS + INTERIOR_BITS);
      const Number i2 = (key >> LEAF_BITS) & (INTERIOR_LENGTH - 1);

      // Check for overflow
      if (i1 >= INTERIOR_LENGTH || i2 >= INTERIOR_LENGTH) return false;

      // Make 2nd level node if necessary
      if (_root.ptrs[i1] == nullptr) {
        Node* n = NewNode();
        if (n == nullptr) return false;
        _root.ptrs[i1] = n;
      }

      // Make leaf node if necessary
      if (_root.ptrs[i1]->ptrs[i2] == nullptr) {
        Leaf* leaf = reinterpret_cast<Leaf*>((*_allocator)(sizeof(Leaf)));
        if (leaf == nullptr) return false;
        memset(leaf, 0, sizeof(*leaf));
        _root.ptrs[i1]->ptrs[i2] = reinterpret_cast<Node*>(leaf);
      }

      // Advance key past whatever is covered by this leaf node
      key = ((key >> LEAF_BITS) + 1) << LEAF_BITS;
    }
    return true;
  }

  void PreallocateMoreMemory() {}

  void* Next(Number k) const {
    while (k < (Number(1) << BITS)) {
      const Number i1 = k >> (LEAF_BITS + INTERIOR_BITS);
      const Number i2 = (k >> LEAF_BITS) & (INTERIOR_LENGTH - 1);
      if (_root.ptrs[i1] == nullptr) {
        // Advance to next top-level entry
        k = (i1 + 1) << (LEAF_BITS + INTERIOR_BITS);
      } else {
        Leaf* leaf = reinterpret_cast<Leaf*>(_root.ptrs[i1]->ptrs[i2]);
        if (leaf != nullptr) {
          for (Number i3 = (k & (LEAF_LENGTH - 1)); i3 < LEAF_LENGTH; i3++) {
            if (leaf->values[i3] != nullptr) {
              return leaf->values[i3];
            }
          }
        }
        // Advance to next interior entry
        k = ((k >> LEAF_BITS) + 1) << LEAF_BITS;
      }
    }
    return nullptr;
  }
};