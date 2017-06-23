///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxctools.h                                                                //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides declarations for the DirectX Compiler tooling components.        //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef __DXC_TOOLS__
#define __DXC_TOOLS__

#include <dxc/dxcapi.h>

struct __declspec(uuid("c012115b-8893-4eb9-9c5a-111456ea1c45"))
IDxcRewriter : public IUnknown {

  virtual HRESULT STDMETHODCALLTYPE RemoveUnusedGlobals(_In_ IDxcBlobEncoding *pSource,
                                                        _In_z_ LPCWSTR entryPoint,
                                                        _In_count_(defineCount) DxcDefine *pDefines,
                                                        _In_ UINT32 defineCount,
                                                        _COM_Outptr_ IDxcOperationResult **ppResult) = 0;


  virtual HRESULT STDMETHODCALLTYPE RewriteUnchanged(_In_ IDxcBlobEncoding *pSource,
                                                     _In_count_(defineCount) DxcDefine *pDefines,
                                                     _In_ UINT32 defineCount,
                                                     _COM_Outptr_ IDxcOperationResult **ppResult) = 0;

  virtual HRESULT STDMETHODCALLTYPE RewriteUnchangedWithInclude(_In_ IDxcBlobEncoding *pSource,
                                                     // Optional file name for pSource. Used in errors and include handlers.
                                                     _In_opt_ LPCWSTR pSourceName,
                                                     _In_count_(defineCount) DxcDefine *pDefines,
                                                     _In_ UINT32 defineCount,
                                                     // user-provided interface to handle #include directives (optional)
                                                     _In_opt_ IDxcIncludeHandler *pIncludeHandler,
                                                     bool  bSkipFunctionBody,
                                                     _COM_Outptr_ IDxcOperationResult **ppResult) = 0;

};

__declspec(selectany)
extern const CLSID CLSID_DxcRewriter = { /* b489b951-e07f-40b3-968d-93e124734da4 */
  0xb489b951,
  0xe07f,
  0x40b3,
  { 0x96, 0x8d, 0x93, 0xe1, 0x24, 0x73, 0x4d, 0xa4 }
};

#endif
