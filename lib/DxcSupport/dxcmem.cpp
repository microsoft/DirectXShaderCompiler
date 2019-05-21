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
#ifdef _WIN32
#include <specstrings.h>
#endif

#include "dxc/Support/WinIncludes.h"
#include "dxc/Support/WinFunctions.h"
#include "llvm/Support/ThreadLocal.h"
#include <memory>

static llvm::sys::ThreadLocal<IMalloc> *g_ThreadMallocTls;
static IMalloc *g_pDefaultMalloc;

#ifndef _WIN32
#pragma GCC visibility push(hidden)
#endif

HRESULT DxcInitThreadMalloc() throw() {
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

void DxcSetThreadMallocToDefault() throw() {
  DXASSERT(g_ThreadMallocTls != nullptr, "else prior to DxcInitThreadMalloc or after DxcCleanupThreadMalloc");
  DXASSERT(DxcGetThreadMallocNoRef() == nullptr, "else nested allocation invoked");
  g_ThreadMallocTls->set(g_pDefaultMalloc);
  g_pDefaultMalloc->AddRef();
}

static IMalloc *DxcSwapThreadMalloc(IMalloc *pMalloc, IMalloc **ppPrior) throw() {
  DXASSERT(g_ThreadMallocTls != nullptr, "else prior to DxcInitThreadMalloc or after DxcCleanupThreadMalloc");
  IMalloc *pPrior = DxcGetThreadMallocNoRef();
  if (ppPrior) {
    *ppPrior = pPrior;
  }
  g_ThreadMallocTls->set(pMalloc);
  return pMalloc;
}

DXC_HIDDEN_LINKAGE DxcThreadMalloc::DxcThreadMalloc(IMalloc *pMallocOrNull) throw() {
    p = DxcSwapThreadMalloc(pMallocOrNull ? pMallocOrNull : g_pDefaultMalloc, &pPrior);
}

DXC_HIDDEN_LINKAGE DxcThreadMalloc::~DxcThreadMalloc() {
    DxcSwapThreadMalloc(pPrior, nullptr);
}

#ifndef _WIN32
#pragma GCC visibility pop
#endif
