//===-- regmalloc.cpp - Memory allocation for regex implementation --------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
// Support operator new/delete overriding for regex memory allocations.
//===----------------------------------------------------------------------===//

#include "regutils.h"
#include <algorithm>
#include <new>
#include <cstring>

extern "C" {
  void *re_malloc(size_t size) {
    return ::operator new(size, std::nothrow);
  }
  void *re_calloc(size_t num, size_t size) {
    void* ptr = re_malloc(num * size);
    if (ptr) std::memset(ptr, 0, num * size);
    return ptr;
  }
  void* re_realloc(void* ptr, size_t oldsize, size_t newsize) {
    void* newptr = re_malloc(newsize);
    if (newptr == nullptr) return nullptr;
    std::memcpy(newptr, ptr, std::min(oldsize, newsize));
    re_free(ptr);
    return newptr;
  }
  void re_free(void *ptr) {
    return ::operator delete(ptr);
  }
}
