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

enum RewriterOptionMask {
  Default = 0,
  SkipFunctionBody = 1,
  SkipStatic = 2,
  GlobalExternByDefault = 4,
  KeepUserMacro = 8,
};

struct IDxcRewriter;
__CRT_UUID_DECL(IDxcRewriter, 0xc012115b, 0x8893, 0x4eb9, 0x9c, 0x5a, 0x11, 0x14, 0x56, 0xea, 0x1c, 0x45)
struct IDxcRewriter : public IUnknown {

  virtual HRESULT STDMETHODCALLTYPE RemoveUnusedGlobals(
      IDxcBlobEncoding *pSource, LPCWSTR entryPoint, DxcDefine *pDefines,
      UINT32 defineCount, IDxcOperationResult **ppResult) = 0;

  virtual HRESULT STDMETHODCALLTYPE
  RewriteUnchanged(IDxcBlobEncoding *pSource, DxcDefine *pDefines,
                   UINT32 defineCount, IDxcOperationResult **ppResult) = 0;

  virtual HRESULT STDMETHODCALLTYPE RewriteUnchangedWithInclude(
      IDxcBlobEncoding *pSource,
      // Optional file name for pSource. Used in errors and include handlers.
      LPCWSTR pSourceName, DxcDefine *pDefines, UINT32 defineCount,
      // user-provided interface to handle #include directives (optional)
      IDxcIncludeHandler *pIncludeHandler, UINT32 rewriteOption,
      IDxcOperationResult **ppResult) = 0;
};

#ifdef _MSC_VER
#define CLSID_SCOPE __declspec(selectany) extern
#else
#define CLSID_SCOPE
#endif

CLSID_SCOPE const CLSID
    CLSID_DxcRewriter = {/* b489b951-e07f-40b3-968d-93e124734da4 */
                         0xb489b951,
                         0xe07f,
                         0x40b3,
                         {0x96, 0x8d, 0x93, 0xe1, 0x24, 0x73, 0x4d, 0xa4}};

struct IDxcRewriter2;
__CRT_UUID_DECL(IDxcRewriter2, 0x261afca1, 0x0609, 0x4ec6, 0xa7, 0x7f, 0xd9, 0x8c, 0x70, 0x35, 0x19, 0x4e)
struct IDxcRewriter2 : public IDxcRewriter {

  virtual HRESULT STDMETHODCALLTYPE RewriteWithOptions(
      IDxcBlobEncoding *pSource,
      // Optional file name for pSource. Used in errors and include handlers.
      LPCWSTR pSourceName,
      // Compiler arguments
      LPCWSTR *pArguments, UINT32 argCount,
      // Defines
      DxcDefine *pDefines, UINT32 defineCount,
      // user-provided interface to handle #include directives (optional)
      IDxcIncludeHandler *pIncludeHandler, IDxcOperationResult **ppResult) = 0;
};

#endif
