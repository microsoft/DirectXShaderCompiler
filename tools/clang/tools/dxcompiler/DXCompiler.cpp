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
#include "dxillib.h"

namespace hlsl { HRESULT SetupRegistryPassForHLSL(); }

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD Reason, LPVOID reserved) {
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
      if (SUCCEEDED(hr)) {
        DxilLibInitialize();
      }
      else {
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
    if (reserved == NULL) { // FreeLibrary has been called or the DLL load failed
      DxilLibCleanup(DxilLibCleanUpType::UnloadLibrary);
    }
    else { // Process termination. We should not call FreeLibrary()
      DxilLibCleanup(DxilLibCleanUpType::ProcessTermination);
    }
  } 

  return result;
}
