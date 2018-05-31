///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxcmem.cpp                                                                //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides support for a thread-local allocator.                            //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/Support/Global.h"
#include <specstrings.h>

#include "dxc/Support/WinIncludes.h"
#include "llvm/Support/ThreadLocal.h"
#include "llvm/Support/WinFunctions.h"
#include <memory>

static llvm::sys::ThreadLocal<IMalloc> *g_ThreadMallocTls;
static IMalloc *g_pDefaultMalloc;

// Used by DllMain to set up and tear down per-thread tracking.
HRESULT DxcInitThreadMalloc() throw();
void DxcCleanupThreadMalloc() throw();

// Used by APIs that are entry points to set up per-thread/invocation allocator.
void DxcSetThreadMalloc(IMalloc *pMalloc) throw();
void DxcSetThreadMallocOrDefault(IMalloc *pMalloc) throw(); 
void DxcClearThreadMalloc() throw();

// Used to retrieve the current invocation's allocator or perform an alloc/free/realloc.
IMalloc *DxcGetThreadMallocNoRef() throw();
_Ret_maybenull_ _Post_writable_byte_size_(nBytes) void *DxcThreadAlloc(size_t nBytes) throw();
void DxcThreadFree(void *) throw();

HRESULT DxcInitThreadMalloc() throw() {
  DXASSERT(g_ThreadMallocTlsIndex == 0, "else InitThreadMalloc already called");
  DXASSERT(g_pDefaultMalloc == nullptr, "else InitThreadMalloc already called");

  // We capture the default malloc early to avoid potential failures later on.
  HRESULT hrMalloc = CoGetMalloc(1, &g_pDefaultMalloc);
  if (FAILED(hrMalloc)) return hrMalloc;

  g_ThreadMallocTls = (llvm::sys::ThreadLocal<IMalloc>*)g_pDefaultMalloc->Alloc(sizeof(llvm::sys::ThreadLocal<IMalloc>));
  if (g_ThreadMallocTls == nullptr) {
    g_pDefaultMalloc->Release();
    g_pDefaultMalloc = nullptr;
    return E_OUTOFMEMORY;
  }
  g_ThreadMallocTls = new(g_ThreadMallocTls) llvm::sys::ThreadLocal<IMalloc>;

  return S_OK;
}

void DxcCleanupThreadMalloc() throw() {
  if (g_ThreadMallocTls) {
    g_ThreadMallocTls->llvm::sys::ThreadLocal<IMalloc>::~ThreadLocal();
    g_pDefaultMalloc->Free(g_ThreadMallocTls);
    g_ThreadMallocTls = nullptr;
    DXASSERT(g_pDefaultMalloc, "else DxcInitThreadMalloc didn't work/fail atomically");
    g_pDefaultMalloc->Release();
    g_pDefaultMalloc = nullptr;
  }
}

IMalloc *DxcGetThreadMallocNoRef() throw() {
  DXASSERT(g_ThreadMallocTls != nullptr, "else prior to DxcInitThreadMalloc or after DxcCleanupThreadMalloc");
  return g_ThreadMallocTls->get();
}

void DxcClearThreadMalloc() throw() {
  DXASSERT(g_ThreadMallocTls != nullptr, "else prior to DxcInitThreadMalloc or after DxcCleanupThreadMalloc");
  IMalloc *pMalloc = DxcGetThreadMallocNoRef();
  g_ThreadMallocTls->erase();
  pMalloc->Release();
}
void DxcSetThreadMalloc(IMalloc *pMalloc) throw() {
  DXASSERT(g_ThreadMallocTls != nullptr, "else prior to DxcInitThreadMalloc or after DxcCleanupThreadMalloc");
  DXASSERT(DxcGetThreadMallocNoRef() == nullptr, "else nested allocation invoked");
  g_ThreadMallocTls->set(pMalloc);
  pMalloc->AddRef();
}
void DxcSetThreadMallocOrDefault(IMalloc *pMalloc) throw() {
  DxcSetThreadMalloc(pMalloc ? pMalloc : g_pDefaultMalloc);
}
IMalloc *DxcSwapThreadMalloc(IMalloc *pMalloc, IMalloc **ppPrior) throw() {
  DXASSERT(g_ThreadMallocTls != nullptr, "else prior to DxcInitThreadMalloc or after DxcCleanupThreadMalloc");
  IMalloc *pPrior = DxcGetThreadMallocNoRef();
  if (ppPrior) {
    *ppPrior = pPrior;
  }
  g_ThreadMallocTls->set(pMalloc);
  return pMalloc;
}
IMalloc *DxcSwapThreadMallocOrDefault(IMalloc *pMallocOrNull, IMalloc **ppPrior) throw() {
  return DxcSwapThreadMalloc(pMallocOrNull ? pMallocOrNull : g_pDefaultMalloc, ppPrior);
}
