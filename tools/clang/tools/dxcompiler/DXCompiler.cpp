///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DXCompiler.cpp                                                            //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Implements the entry point for the dxcompiler DLL.                        //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/FileSystem.h"

#include "dxc/Support/WinIncludes.h"
#include "dxcetw.h"

namespace hlsl { HRESULT SetupRegistryPassForHLSL(); }

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD Reason, LPVOID) {
  BOOL result = TRUE;
  if (Reason == DLL_PROCESS_ATTACH) {
    EventRegisterMicrosoft_Windows_DXCompiler_API();
    DxcEtw_DXCompilerInitialization_Start();
    DisableThreadLibraryCalls(hinstDLL);
    HRESULT hr = S_OK;
    if (::llvm::sys::fs::SetupPerThreadFileSystem()) {
      hr = E_FAIL;
    }
    else {
      hr = hlsl::SetupRegistryPassForHLSL();
      if (FAILED(hr)) {
        ::llvm::sys::fs::CleanupPerThreadFileSystem();
      }
    }
    DxcEtw_DXCompilerInitialization_Stop(hr);
    result = SUCCEEDED(hr) ? TRUE : FALSE;
  } else if (Reason == DLL_PROCESS_DETACH) {
    DxcEtw_DXCompilerShutdown_Start();
    ::llvm::sys::fs::CleanupPerThreadFileSystem();
    ::llvm::llvm_shutdown();
    DxcEtw_DXCompilerShutdown_Stop(S_OK);
    EventUnregisterMicrosoft_Windows_DXCompiler_API();
  }

  return result;
}
