//===-- WinAdapter.cpp - Windows Adapter for other platforms ----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef _WIN32

#include "dxc/Support/WinAdapter.h"
#include "dxc/Support/WinFunctions.h"

DEFINE_CROSS_PLATFORM_UUIDOF(IUnknown)
DEFINE_CROSS_PLATFORM_UUIDOF(INoMarshal)
DEFINE_CROSS_PLATFORM_UUIDOF(IStream)
DEFINE_CROSS_PLATFORM_UUIDOF(ISequentialStream)

//===--------------------------- IUnknown ---------------------------------===//

ULONG IUnknown::AddRef() {
  ++m_count;
  return m_count;
}
ULONG IUnknown::Release() {
  --m_count;
  if (m_count == 0) {
    delete this;
  }
  return m_count;
}
IUnknown::~IUnknown() {}

//===--------------------------- IMalloc ----------------------------------===//

void *IMalloc::Alloc(size_t size) { return malloc(size); }
void *IMalloc::Realloc(void *ptr, size_t size) { return realloc(ptr, size); }
void IMalloc::Free(void *ptr) { free(ptr); }
HRESULT IMalloc::QueryInterface(REFIID riid, void **ppvObject) {
  assert(false && "QueryInterface not implemented for IMalloc.");
  return E_NOINTERFACE;
}

//===--------------------------- CAllocator -------------------------------===//

void *CAllocator::Reallocate(void *p, size_t nBytes) throw() {
  return realloc(p, nBytes);
}
void *CAllocator::Allocate(size_t nBytes) throw() { return malloc(nBytes); }
void CAllocator::Free(void *p) throw() { free(p); }

//===--------------------------- CHandle -------------------------------===//

CHandle::CHandle(HANDLE h) { m_h = h; }
CHandle::~CHandle() { CloseHandle(m_h); }
CHandle::operator HANDLE() const throw() { return m_h; }

#endif
