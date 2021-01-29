///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxcompileradapater.h                                                      //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides helper code for dxcompiler.                                      //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "dxc/dxcapi.h"
#include "dxc/Support/microcom.h"

namespace hlsl
{
// This class provides an adapter for the legacy compiler interfaces
// (i.e. IDxcCompiler and IDxcCompiler2) that is backed by an IDxcCompiler3
// implemenation. It allows a single core IDxcCompiler3 implementation to be
// used to implement all IDxcCompiler interfaces.
//
// This must be owned/managed by IDxcCompiler3 instance.
class DxcCompilerAdapter: public IDxcCompiler2
{
private:
  IDxcCompiler3 *m_pCompilerImpl;
  IMalloc *m_pMalloc;

  // Internal wrapper for compile
  HRESULT WrapCompile(
    _In_ BOOL bPreprocess,                         // Preprocess mode
    _In_ IDxcBlob *pSource,                       // Source text to compile
    _In_opt_ LPCWSTR pSourceName,                 // Optional file name for pSource. Used in errors and include handlers.
    _In_ LPCWSTR pEntryPoint,                     // Entry point name
    _In_ LPCWSTR pTargetProfile,                  // Shader profile to compile
    _In_count_(argCount) LPCWSTR *pArguments,     // Array of pointers to arguments
    _In_ UINT32 argCount,                         // Number of arguments
    _In_count_(defineCount) const DxcDefine *pDefines,  // Array of defines
    _In_ UINT32 defineCount,                      // Number of defines
    _In_opt_ IDxcIncludeHandler *pIncludeHandler, // user-provided interface to handle #include directives (optional)
    _COM_Outptr_ IDxcOperationResult **ppResult,  // Compiler output status, buffer, and errors
    _Outptr_opt_result_z_ LPWSTR *ppDebugBlobName,// Suggested file name for debug blob.
    _COM_Outptr_opt_ IDxcBlob **ppDebugBlob       // Debug blob
  );

public:
  DxcCompilerAdapter(IDxcCompiler3 *impl, IMalloc *pMalloc) : m_pCompilerImpl(impl), m_pMalloc(pMalloc) {}
  ULONG STDMETHODCALLTYPE AddRef() override;
  ULONG STDMETHODCALLTYPE Release() override;
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) override;

  // ================ IDxcCompiler ================

  // Compile a single entry point to the target shader model
  HRESULT STDMETHODCALLTYPE Compile(
    _In_ IDxcBlob *pSource,                       // Source text to compile
    _In_opt_ LPCWSTR pSourceName,                 // Optional file name for pSource. Used in errors and include handlers.
    _In_ LPCWSTR pEntryPoint,                     // entry point name
    _In_ LPCWSTR pTargetProfile,                  // shader profile to compile
    _In_count_(argCount) LPCWSTR *pArguments,     // Array of pointers to arguments
    _In_ UINT32 argCount,                         // Number of arguments
    _In_count_(defineCount) const DxcDefine *pDefines,  // Array of defines
    _In_ UINT32 defineCount,                      // Number of defines
    _In_opt_ IDxcIncludeHandler *pIncludeHandler, // user-provided interface to handle #include directives (optional)
    _COM_Outptr_ IDxcOperationResult **ppResult   // Compiler output status, buffer, and errors
  ) override {
    return CompileWithDebug(pSource, pSourceName, pEntryPoint, pTargetProfile,
                            pArguments, argCount, pDefines, defineCount,
                            pIncludeHandler, ppResult, nullptr, nullptr);
  }

  // Preprocess source text
  HRESULT STDMETHODCALLTYPE Preprocess(
    _In_ IDxcBlob *pSource,                       // Source text to preprocess
    _In_opt_z_ LPCWSTR pSourceName,               // Optional file name for pSource. Used in errors and include handlers.
    _In_opt_count_(argCount) LPCWSTR *pArguments, // Array of pointers to arguments
    _In_ UINT32 argCount,                         // Number of arguments
    _In_count_(defineCount)
      const DxcDefine *pDefines,                  // Array of defines
    _In_ UINT32 defineCount,                      // Number of defines
    _In_opt_ IDxcIncludeHandler *pIncludeHandler, // user-provided interface to handle #include directives (optional)
    _COM_Outptr_ IDxcOperationResult **ppResult   // Preprocessor output status, buffer, and errors
  ) override;

  // Disassemble a program.
  HRESULT STDMETHODCALLTYPE Disassemble(
    _In_ IDxcBlob *pSource,                         // Program to disassemble.
    _COM_Outptr_ IDxcBlobEncoding **ppDisassembly   // Disassembly text.
  ) override;

  // ================ IDxcCompiler2 ================

  // Compile a single entry point to the target shader model with debug information.
  HRESULT STDMETHODCALLTYPE CompileWithDebug(
    _In_ IDxcBlob *pSource,                       // Source text to compile
    _In_opt_ LPCWSTR pSourceName,                 // Optional file name for pSource. Used in errors and include handlers.
    _In_ LPCWSTR pEntryPoint,                     // Entry point name
    _In_ LPCWSTR pTargetProfile,                  // Shader profile to compile
    _In_count_(argCount) LPCWSTR *pArguments,     // Array of pointers to arguments
    _In_ UINT32 argCount,                         // Number of arguments
    _In_count_(defineCount) const DxcDefine *pDefines,  // Array of defines
    _In_ UINT32 defineCount,                      // Number of defines
    _In_opt_ IDxcIncludeHandler *pIncludeHandler, // user-provided interface to handle #include directives (optional)
    _COM_Outptr_ IDxcOperationResult **ppResult,  // Compiler output status, buffer, and errors
    _Outptr_opt_result_z_ LPWSTR *ppDebugBlobName,// Suggested file name for debug blob.
    _COM_Outptr_opt_ IDxcBlob **ppDebugBlob       // Debug blob
  ) override;
};

}
