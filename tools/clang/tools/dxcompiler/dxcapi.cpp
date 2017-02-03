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

#define DXC_API_IMPORT __declspec(dllexport)

#include "dxc/dxcisense.h"
#include "dxc/dxctools.h"
#include "dxcetw.h"
#include <memory>

HRESULT CreateDxcCompiler(_In_ REFIID riid, _Out_ LPVOID *ppv);
HRESULT CreateDxcDiaDataSource(_In_ REFIID riid, _Out_ LPVOID *ppv);
HRESULT CreateDxcIntelliSense(_In_ REFIID riid, _Out_ LPVOID *ppv);
HRESULT CreateDxcLibrary(_In_ REFIID riid, _Out_ LPVOID *ppv);
HRESULT CreateDxcRewriter(_In_ REFIID riid, _Out_ LPVOID *ppv);
HRESULT CreateDxcValidator(_In_ REFIID riid, _Out_ LPVOID *ppv);
HRESULT CreateDxcAssembler(_In_ REFIID riid, _Out_ LPVOID *ppv);
HRESULT CreateDxcOptimizer(_In_ REFIID riid, _Out_ LPVOID *ppv);
HRESULT CreateDxcContainerBuilder(_In_ REFIID riid, _Out_ LPVOID *ppv);

namespace hlsl {
void CreateDxcContainerReflection(IDxcContainerReflection **ppResult);
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

/// <summary>
/// Creates a single uninitialized object of the class associated with a specified CLSID.
/// </summary>
/// <param name="rclsid">The CLSID associated with the data and code that will be used to create the object.</param>
/// <param name="riid">A reference to the identifier of the interface to be used to communicate with the object.</param>
/// <param name="ppv">Address of pointer variable that receives the interface pointer requested in riid. Upon successful return, *ppv contains the requested interface pointer. Upon failure, *ppv contains NULL.</param>
/// <remarks>
/// While this function is similar to CoCreateInstance, there is no COM involvement.  
/// </remarks>
DXC_API_IMPORT HRESULT __stdcall
DxcCreateInstance(_In_ REFCLSID   rclsid,
                  _In_ REFIID     riid,
                  _Out_ LPVOID   *ppv) {
  if (ppv == nullptr) {
    return E_POINTER;
  }

  HRESULT hr = S_OK;
  DxcEtw_DXCompilerCreateInstance_Start();
  *ppv = nullptr;
  if (IsEqualCLSID(rclsid, CLSID_DxcIntelliSense)) {
    hr = CreateDxcIntelliSense(riid, ppv);
  }
  else if (IsEqualCLSID(rclsid, CLSID_DxcRewriter)) {
    hr = CreateDxcRewriter(riid, ppv);
  }
  else if (IsEqualCLSID(rclsid, CLSID_DxcCompiler)) {
    hr = CreateDxcCompiler(riid, ppv);
  }
  else if (IsEqualCLSID(rclsid, CLSID_DxcLibrary)) {
    hr = CreateDxcLibrary(riid, ppv);
  }
  else if (IsEqualCLSID(rclsid, CLSID_DxcValidator)) {
    hr = CreateDxcValidator(riid, ppv);
  }
  else if (IsEqualCLSID(rclsid, CLSID_DxcAssembler)) {
    hr = CreateDxcAssembler(riid, ppv);
  }
  else if (IsEqualCLSID(rclsid, CLSID_DxcOptimizer)) {
    hr = CreateDxcOptimizer(riid, ppv);
  }
  else if (IsEqualCLSID(rclsid, CLSID_DxcDiaDataSource)) {
    hr = CreateDxcDiaDataSource(riid, ppv);
  }
  else if (IsEqualCLSID(rclsid, CLSID_DxcContainerReflection)) {
    hr = CreateDxcContainerReflection(riid, ppv);
  }
  else if (IsEqualCLSID(rclsid, CLSID_DxcContainerBuilder)) {
    hr = CreateDxcContainerBuilder(riid, ppv);
  }
  else {
    hr = REGDB_E_CLASSNOTREG;
  }

  DxcEtw_DXCompilerCreateInstance_Stop(hr);
  return hr;
}
