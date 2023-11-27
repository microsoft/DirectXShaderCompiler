///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// NoSigDxilLib.cpp                                                          //
// Copyright (C) Microsoft. All rights reserved.                             //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Implements the DxcCreateInstance, DxcCreateInstance2, and DllMain         //
// functions for the dxil library module                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/Support/Global.h"
#include "dxc/Support/WinIncludes.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/ManagedStatic.h"
#include <algorithm>
#ifdef _WIN32
#include "Tracing/DxcRuntimeEtw.h"
#include "dxc/Tracing/dxcetw.h"
#endif

#ifdef _WIN32
#define DXC_API_IMPORT
#else
#define DXC_API_IMPORT __attribute__((visibility("default")))
#endif

#include "dxc/dxcisense.h"
#include "dxc/dxctools.h"

HRESULT CreateDxcValidator(_In_ REFIID riid, _Out_ LPVOID *ppv);
HRESULT CreateDxcContainerBuilder(_In_ REFIID riid, _Out_ LPVOID *ppv);

// C++ exception specification ignored except to indicate a function is not
// __declspec(nothrow)
static HRESULT InitMaybeFail() throw() {
  HRESULT hr;
  bool memSetup = false;
  IFC(DxcInitThreadMalloc());
  DxcSetThreadMallocToDefault();
  memSetup = true;
  if (::llvm::sys::fs::SetupPerThreadFileSystem()) {
    hr = E_FAIL;
    goto Cleanup;
  }
Cleanup:
  if (FAILED(hr)) {
    if (memSetup) {
      DxcClearThreadMalloc();
      DxcCleanupThreadMalloc();
    }
  } else {
    DxcClearThreadMalloc();
  }
  return hr;
}

#if defined(LLVM_ON_UNIX)
HRESULT __attribute__((constructor)) DllMain() { return InitMaybeFail(); }

void __attribute__((destructor)) DllShutdown() {
  DxcSetThreadMallocToDefault();
  ::llvm::sys::fs::CleanupPerThreadFileSystem();
  ::llvm::llvm_shutdown();
  DxcClearThreadMalloc();
  DxcCleanupThreadMalloc();
}
#else

#pragma warning(disable : 4290)
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD Reason, LPVOID) {
  if (Reason == DLL_PROCESS_ATTACH) {
    EventRegisterMicrosoft_Windows_DxcRuntime_API();
    DxcRuntimeEtw_DxcRuntimeInitialization_Start();
    HRESULT hr = InitMaybeFail();
    DxcRuntimeEtw_DxcRuntimeInitialization_Stop(hr);
    if (FAILED(hr)) {
      EventUnregisterMicrosoft_Windows_DxcRuntime_API();
      return hr;
    }
  } else if (Reason == DLL_PROCESS_DETACH) {
    DxcRuntimeEtw_DxcRuntimeShutdown_Start();
    DxcSetThreadMallocToDefault();
    ::llvm::sys::fs::CleanupPerThreadFileSystem();
    ::llvm::llvm_shutdown();
    DxcClearThreadMalloc();
    DxcCleanupThreadMalloc();
    DxcRuntimeEtw_DxcRuntimeShutdown_Stop(S_OK);
    EventUnregisterMicrosoft_Windows_DxcRuntime_API();
  }

  return TRUE;
}

void *__CRTDECL operator new(std::size_t size) {
  void *ptr = DxcNew(size);
  if (ptr == nullptr)
    throw std::bad_alloc();
  return ptr;
}
void *__CRTDECL operator new(std::size_t size,
                             const std::nothrow_t &nothrow_value) throw() {
  return DxcNew(size);
}
void __CRTDECL operator delete(void *ptr) throw() { DxcDelete(ptr); }
void __CRTDECL operator delete(void *ptr,
                               const std::nothrow_t &nothrow_constant) throw() {
  DxcDelete(ptr);
}
#endif

static HRESULT ThreadMallocDxcCreateInstance(_In_ REFCLSID rclsid,
                                             _In_ REFIID riid,
                                             _Out_ LPVOID *ppv) {
  *ppv = nullptr;
  if (IsEqualCLSID(rclsid, CLSID_DxcValidator)) {
    return CreateDxcValidator(riid, ppv);
  }
  if (IsEqualCLSID(rclsid, CLSID_DxcContainerBuilder)) {
    return CreateDxcContainerBuilder(riid, ppv);
  }
  return REGDB_E_CLASSNOTREG;
}

DXC_API_IMPORT HRESULT __stdcall DxcCreateInstance(_In_ REFCLSID rclsid,
                                                   _In_ REFIID riid,
                                                   _Out_ LPVOID *ppv) {
  HRESULT hr = S_OK;
  DxcEtw_DXCompilerCreateInstance_Start();
  DxcThreadMalloc TM(nullptr);
  hr = ThreadMallocDxcCreateInstance(rclsid, riid, ppv);
  DxcEtw_DXCompilerCreateInstance_Stop(hr);
  return hr;
}

DXC_API_IMPORT HRESULT __stdcall DxcCreateInstance2(_In_ IMalloc *pMalloc,
                                                    _In_ REFCLSID rclsid,
                                                    _In_ REFIID riid,
                                                    _Out_ LPVOID *ppv) {
  if (ppv == nullptr) {
    return E_POINTER;
  }
  HRESULT hr = S_OK;
  DxcEtw_DXCompilerCreateInstance_Start();
  DxcThreadMalloc TM(pMalloc);
  hr = ThreadMallocDxcCreateInstance(rclsid, riid, ppv);
  DxcEtw_DXCompilerCreateInstance_Stop(hr);
  return hr;
}
