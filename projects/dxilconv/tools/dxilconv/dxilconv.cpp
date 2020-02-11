///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxilconv.cpp                                                              //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Implements the DLL entry point and DxcCreateInstance function.            //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MD5.h"
#include "dxc/Support/WinIncludes.h"
#include "dxc/Support/Global.h"
#include "Tracing/DxcRuntimeEtw.h"

#define DXC_API_IMPORT

#include "dxc/dxcisense.h"
#include "dxc/dxctools.h"
#include "dxcetw.h"
#include "Tracing/DxcRuntimeEtw.h"
#include "DxbcConverter.h"

// Defined in DxbcConverter.lib (projects/dxilconv/lib/DxbcConverter/DxbcConverter.cpp)
HRESULT CreateDxbcConverter(_In_ REFIID riid, _Out_ LPVOID *ppv);

/// <summary>
/// Creates a single uninitialized object of the class associated with a specified CLSID.
/// </summary>
/// <param name="rclsid">The CLSID associated with the data and code that will be used to create the object.</param>
/// <param name="riid">A reference to the identifier of the interface to be used to communicate with the object.</param>
/// <param name="ppv">Address of pointer variable that receives the interface pointer requested in riid. Upon successful return, *ppv contains the requested interface pointer. Upon failure, *ppv contains NULL.</param>
/// <remarks>
/// While this function is similar to CoCreateInstance, there is no COM involvement.
/// </remarks>
static HRESULT ThreadMallocDxcCreateInstance(
    _In_ REFCLSID rclsid,
    _In_ REFIID riid,
    _Out_ LPVOID *ppv) {
    *ppv = nullptr;
    if (IsEqualCLSID(rclsid, CLSID_DxbcConverter)) {
      return CreateDxbcConverter(riid, ppv);
    }
    return REGDB_E_CLASSNOTREG;
}

DXC_API_IMPORT HRESULT __stdcall
DxcCreateInstance(_In_ REFCLSID   rclsid,
    _In_ REFIID     riid,
    _Out_ LPVOID   *ppv) {
    HRESULT hr = S_OK;
    DxcEtw_DXCompilerCreateInstance_Start();
    DxcThreadMalloc TM(nullptr);
    hr = ThreadMallocDxcCreateInstance(rclsid, riid, ppv);
    DxcEtw_DXCompilerCreateInstance_Stop(hr);
    return hr;
}

DXC_API_IMPORT HRESULT __stdcall
DxcCreateInstance2(_In_ IMalloc *pMalloc,
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


// C++ exception specification ignored except to indicate a function is not __declspec(nothrow)
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
    }
    else {
        DxcClearThreadMalloc();
    }
    return hr;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD Reason, LPVOID )
{
  if (Reason == DLL_PROCESS_ATTACH)
  {
    DisableThreadLibraryCalls(hinstDLL);
    EventRegisterMicrosoft_Windows_DxcRuntime_API();

    DxcRuntimeEtw_DxcRuntimeInitialization_Start();
    DisableThreadLibraryCalls(hinstDLL);
    HRESULT hr = InitMaybeFail();
    if (FAILED(hr)) {
      DxcRuntimeEtw_DxcRuntimeInitialization_Stop(hr);
      return FALSE;
    }

    DxcRuntimeEtw_DxcRuntimeInitialization_Stop(S_OK);
  }
  else if (Reason == DLL_PROCESS_DETACH)
  {
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

