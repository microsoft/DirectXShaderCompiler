///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxcapi.cpp                                                                //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Implements the DxcCreateInstance function for the DirectX Compiler.       //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/Support/WinIncludes.h"

#ifdef _WIN32
#define DXC_API_IMPORT __declspec(dllexport)
#else
#define DXC_API_IMPORT __attribute__ ((visibility ("default")))
#endif

#include "dxc/dxcisense.h"
#include "dxc/dxctools.h"
#include "dxc/Support/Global.h"
#ifdef _WIN32
#include "dxcetw.h"
#endif
#include "dxillib.h"
#include "dxc/DxilContainer/DxcContainerBuilder.h"
#include <memory>

// Initialize the UUID for the interfaces.
DEFINE_CROSS_PLATFORM_UUIDOF(IDxcLibrary)
DEFINE_CROSS_PLATFORM_UUIDOF(IDxcBlobEncoding)
DEFINE_CROSS_PLATFORM_UUIDOF(IDxcOperationResult)
DEFINE_CROSS_PLATFORM_UUIDOF(IDxcAssembler)
DEFINE_CROSS_PLATFORM_UUIDOF(IDxcBlob)
DEFINE_CROSS_PLATFORM_UUIDOF(IDxcIncludeHandler)
DEFINE_CROSS_PLATFORM_UUIDOF(IDxcCompiler)
DEFINE_CROSS_PLATFORM_UUIDOF(IDxcCompiler2)
DEFINE_CROSS_PLATFORM_UUIDOF(IDxcVersionInfo)
DEFINE_CROSS_PLATFORM_UUIDOF(IDxcVersionInfo2)
DEFINE_CROSS_PLATFORM_UUIDOF(IDxcValidator)
DEFINE_CROSS_PLATFORM_UUIDOF(IDxcContainerBuilder)
DEFINE_CROSS_PLATFORM_UUIDOF(IDxcOptimizerPass)
DEFINE_CROSS_PLATFORM_UUIDOF(IDxcOptimizer)
DEFINE_CROSS_PLATFORM_UUIDOF(IDxcRewriter)
DEFINE_CROSS_PLATFORM_UUIDOF(IDxcRewriter2)
DEFINE_CROSS_PLATFORM_UUIDOF(IDxcIntelliSense)
DEFINE_CROSS_PLATFORM_UUIDOF(IDxcLinker)
DEFINE_CROSS_PLATFORM_UUIDOF(IDxcBlobUtf16)
DEFINE_CROSS_PLATFORM_UUIDOF(IDxcBlobUtf8)
DEFINE_CROSS_PLATFORM_UUIDOF(IDxcCompilerArgs)
DEFINE_CROSS_PLATFORM_UUIDOF(IDxcUtils)
DEFINE_CROSS_PLATFORM_UUIDOF(IDxcResult)
DEFINE_CROSS_PLATFORM_UUIDOF(IDxcCompiler3)

HRESULT CreateDxcCompiler(_In_ REFIID riid, _Out_ LPVOID *ppv);
HRESULT CreateDxcDiaDataSource(_In_ REFIID riid, _Out_ LPVOID *ppv);
HRESULT CreateDxcIntelliSense(_In_ REFIID riid, _Out_ LPVOID *ppv);
HRESULT CreateDxcCompilerArgs(_In_ REFIID riid, _Out_ LPVOID *ppv);
HRESULT CreateDxcUtils(_In_ REFIID riid, _Out_ LPVOID *ppv);
HRESULT CreateDxcRewriter(_In_ REFIID riid, _Out_ LPVOID *ppv);
HRESULT CreateDxcValidator(_In_ REFIID riid, _Out_ LPVOID *ppv);
HRESULT CreateDxcAssembler(_In_ REFIID riid, _Out_ LPVOID *ppv);
HRESULT CreateDxcOptimizer(_In_ REFIID riid, _Out_ LPVOID *ppv);
HRESULT CreateDxcContainerBuilder(_In_ REFIID riid, _Out_ LPVOID *ppv);
HRESULT CreateDxcLinker(_In_ REFIID riid, _Out_ LPVOID *ppv);

namespace hlsl {
void CreateDxcContainerReflection(IDxcContainerReflection **ppResult);
void CreateDxcLinker(IDxcContainerReflection **ppResult);
}

HRESULT CreateDxcContainerReflection(_In_ REFIID riid, _Out_ LPVOID *ppv) {
  try {
    CComPtr<IDxcContainerReflection> pReflection;
    hlsl::CreateDxcContainerReflection(&pReflection);
    return pReflection->QueryInterface(riid, ppv);
  }
  catch (const std::bad_alloc&) {
    return E_OUTOFMEMORY;
  }
}

HRESULT CreateDxcContainerBuilder(_In_ REFIID riid, _Out_ LPVOID *ppv) {
  // Call dxil.dll's containerbuilder 
  *ppv = nullptr;
  const char *warning;
  HRESULT hr = DxilLibCreateInstance(CLSID_DxcContainerBuilder, (IDxcContainerBuilder**)ppv);
  if (FAILED(hr)) {
    warning = "Unable to create container builder from dxil.dll. Resulting container will not be signed.\n";
  }
  else {
    return hr;
  }

  CComPtr<DxcContainerBuilder> Result = DxcContainerBuilder::Alloc(DxcGetThreadMallocNoRef());
  IFROOM(Result.p);
  Result->Init(warning);
  return Result->QueryInterface(riid, ppv);
}

static HRESULT ThreadMallocDxcCreateInstance(
  _In_ REFCLSID   rclsid,
                  _In_ REFIID     riid,
                  _Out_ LPVOID   *ppv) {
  HRESULT hr = S_OK;
  *ppv = nullptr;
  if (IsEqualCLSID(rclsid, CLSID_DxcCompiler)) {
    hr = CreateDxcCompiler(riid, ppv);
  }
  else if (IsEqualCLSID(rclsid, CLSID_DxcCompilerArgs)) {
    hr = CreateDxcCompilerArgs(riid, ppv);
  }
  else if (IsEqualCLSID(rclsid, CLSID_DxcUtils)) {
    hr = CreateDxcUtils(riid, ppv);
  }
  else if (IsEqualCLSID(rclsid, CLSID_DxcValidator)) {
    if (DxilLibIsEnabled()) {
      hr = DxilLibCreateInstance(rclsid, riid, (IUnknown**)ppv);
    } else {
      hr = CreateDxcValidator(riid, ppv);
    }
  }
  else if (IsEqualCLSID(rclsid, CLSID_DxcAssembler)) {
    hr = CreateDxcAssembler(riid, ppv);
  }
  else if (IsEqualCLSID(rclsid, CLSID_DxcOptimizer)) {
    hr = CreateDxcOptimizer(riid, ppv);
  }
  else if (IsEqualCLSID(rclsid, CLSID_DxcIntelliSense)) {
    hr = CreateDxcIntelliSense(riid, ppv);
  }
// Note: The following targets are not yet enabled for non-Windows platforms.
#ifdef _WIN32
  else if (IsEqualCLSID(rclsid, CLSID_DxcRewriter)) {
    hr = CreateDxcRewriter(riid, ppv);
  }
  else if (IsEqualCLSID(rclsid, CLSID_DxcDiaDataSource)) {
    hr = CreateDxcDiaDataSource(riid, ppv);
  }
  else if (IsEqualCLSID(rclsid, CLSID_DxcContainerReflection)) {
    hr = CreateDxcContainerReflection(riid, ppv);
  }
  else if (IsEqualCLSID(rclsid, CLSID_DxcLinker)) {
    hr = CreateDxcLinker(riid, ppv);
  }
  else if (IsEqualCLSID(rclsid, CLSID_DxcContainerBuilder)) {
    hr = CreateDxcContainerBuilder(riid, ppv);
  }
#endif
  else {
    hr = REGDB_E_CLASSNOTREG;
  }
  return hr;
}

DXC_API_IMPORT HRESULT __stdcall
DxcCreateInstance(
  _In_ REFCLSID   rclsid,
  _In_ REFIID     riid,
  _Out_ LPVOID   *ppv) {
  if (ppv == nullptr) {
    return E_POINTER;
  }

  HRESULT hr = S_OK;
  DxcEtw_DXCompilerCreateInstance_Start();
  DxcThreadMalloc TM(nullptr);
  hr = ThreadMallocDxcCreateInstance(rclsid, riid, ppv);
  DxcEtw_DXCompilerCreateInstance_Stop(hr);
  return hr;
}

DXC_API_IMPORT HRESULT __stdcall
DxcCreateInstance2(
  _In_ IMalloc    *pMalloc,
  _In_ REFCLSID   rclsid,
  _In_ REFIID     riid,
  _Out_ LPVOID   *ppv) {
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
