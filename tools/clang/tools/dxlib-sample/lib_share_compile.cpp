///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// lib_share_compile.cpp                                                     //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Compile function split shader to lib then link.                           //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/Support/WinIncludes.h"
#include "dxc/HLSL/DxilContainer.h"
#include "dxc/Support/Global.h"
#include "dxc/dxcapi.h"

#include "dxc/Support/FileIOHelper.h"

#include "dxc/Support/Unicode.h"
#include <d3dcompiler.h>
#include <string>
#include <vector>
#include "lib_share_helper.h"

using namespace libshare;
using namespace hlsl;
using namespace llvm;

HRESULT CreateLibrary(IDxcLibrary **pLibrary) {
  return DxcCreateInstance(CLSID_DxcLibrary, __uuidof(IDxcLibrary),
                           (void **)pLibrary);
}

HRESULT CreateCompiler(IDxcCompiler **ppCompiler) {
  return DxcCreateInstance(CLSID_DxcCompiler, __uuidof(IDxcCompiler),
                           (void **)ppCompiler);
}

HRESULT CreateLinker(IDxcLinker **ppLinker) {
  return DxcCreateInstance(CLSID_DxcLinker, __uuidof(IDxcLinker),
                           (void **)ppLinker);
}

HRESULT CreateContainerReflection(IDxcContainerReflection **ppReflection) {
  return DxcCreateInstance(CLSID_DxcContainerReflection,
                           __uuidof(IDxcContainerReflection),
                           (void **)ppReflection);
}

HRESULT CompileToLib(IDxcBlob *pSource, std::vector<DxcDefine> &defines,
                     IDxcIncludeHandler *pInclude,
                     std::vector<LPCWSTR> &arguments, IDxcBlob **ppCode,
                     IDxcBlob **ppErrorMsgs) {
  CComPtr<IDxcCompiler> compiler;
  CComPtr<IDxcOperationResult> operationResult;

  IFR(CreateCompiler(&compiler));
  IFR(compiler->Compile(pSource, L"input.hlsl", L"", L"lib_6_1",
                        arguments.data(), (UINT)arguments.size(),
                        defines.data(), (UINT)defines.size(), pInclude,
                        &operationResult));
  HRESULT hr;
  operationResult->GetStatus(&hr);
  if (SUCCEEDED(hr)) {
    return operationResult->GetResult((IDxcBlob **)ppCode);
  } else {
    if (ppErrorMsgs)
      operationResult->GetErrorBuffer((IDxcBlobEncoding **)ppErrorMsgs);
    return hr;
  }
  return hr;
}


#include "dxc/Support/dxcapi.impl.h"
#include "dxc/Support/HLSLOptions.h"
static void ReadOptsAndValidate(hlsl::options::MainArgs &mainArgs,
                                hlsl::options::DxcOpts &opts,
                                AbstractMemoryStream *pOutputStream,
                                _COM_Outptr_ IDxcOperationResult **ppResult,
                                bool &finished) {
  const llvm::opt::OptTable *table = ::options::getHlslOptTable();
  raw_stream_ostream outStream(pOutputStream);
  if (0 != hlsl::options::ReadDxcOpts(table, hlsl::options::CompilerFlags,
                                      mainArgs, opts, outStream)) {
    CComPtr<IDxcBlob> pErrorBlob;
    IFT(pOutputStream->QueryInterface(&pErrorBlob));
    CComPtr<IDxcBlobEncoding> pErrorBlobWithEncoding;
    outStream.flush();
    IFT(DxcCreateBlobWithEncodingSet(pErrorBlob.p, CP_UTF8,
                                     &pErrorBlobWithEncoding));
    IFT(DxcOperationResult::CreateFromResultErrorStatus(
        nullptr, pErrorBlobWithEncoding.p, E_INVALIDARG, ppResult));
    finished = true;
    return;
  }
  DXASSERT(!opts.HLSL2015, "else ReadDxcOpts didn't fail for non-isense");
  finished = false;
}

HRESULT CompileFromBlob(IDxcBlobEncoding *pSource, LPCWSTR pSourceName,
                        std::vector<DxcDefine> &defines,
                        IDxcIncludeHandler *pInclude, LPCSTR pEntrypoint,
                        LPCSTR pTarget, std::vector<LPCWSTR> &arguments,
                        IDxcOperationResult **ppOperationResult) {
  CComPtr<IDxcCompiler> compiler;
  CComPtr<IDxcLinker> linker;

  // Upconvert legacy targets
  char Target[7] = "?s_6_0";
  Target[6] = 0;
  Target[0] = pTarget[0];

  try {
    CA2W pEntrypointW(pEntrypoint);
    CA2W pTargetProfileW(Target);

    // Preprocess.
    std::unique_ptr<IncludeToLibPreprocessor> preprocessor = IncludeToLibPreprocessor::CreateIncludeToLibPreprocessor(pInclude);
    preprocessor->SetupDefines(defines.data(), defines.size());
    if (arguments.size()) {
      CComPtr<AbstractMemoryStream> pOutputStream;
      IFT(CreateMemoryStream(GetGlobalHeapMalloc(), &pOutputStream));

      hlsl::options::MainArgs mainArgs(arguments.size(), arguments.data(), 0);
      hlsl::options::DxcOpts opts;
      bool finished;
      ReadOptsAndValidate(mainArgs, opts, pOutputStream, ppOperationResult,
                          finished);
      if (finished) {
        return E_FAIL;
      }

      for (const llvm::opt::Arg *A : opts.Args.filtered(options::OPT_I)) {
        const bool IsFrameworkFalse = false;
        const bool IgnoreSysRoot = true;
        if (dxcutil::IsAbsoluteOrCurDirRelative(A->getValue())) {
          preprocessor->AddIncPath(A->getValue());
        } else {
          std::string s("./");
          s += A->getValue();
          preprocessor->AddIncPath(s);
        }
      }
    }

    preprocessor->Preprocess(pSource, pSourceName);

    CompileInput compilerInput{defines, arguments};

    LibCacheManager &libCache = LibCacheManager::GetLibCacheManager();
    IFR(CreateLinker(&linker));
    IDxcIncludeHandler * const kNoIncHandler = nullptr;
    const auto &headers = preprocessor->GetHeaders();
    std::vector<std::wstring> hashStrList;
    std::vector<LPCWSTR> hashList;
    for (const auto &header : headers) {
      CComPtr<IDxcBlob> pOutputBlob;
      CComPtr<IDxcBlobEncoding> pSource;
      IFT(DxcCreateBlobWithEncodingOnMallocCopy(GetGlobalHeapMalloc(), header.data(), header.size(), CP_UTF8, &pSource));
      size_t hash;
      if (!libCache.GetLibBlob(pSource, compilerInput, hash, &pOutputBlob)) {
        // Cannot find existing blob, create from pSource.
        IDxcBlob **ppCode = &pOutputBlob;

        auto compileFn = [&]() {
          IFT(CompileToLib(pSource, defines, kNoIncHandler, arguments,
                           ppCode, nullptr));
        };
        libCache.AddLibBlob(pSource, compilerInput, hash, &pOutputBlob,
                            compileFn);
      }
      pSource.Detach(); // Don't keep the ownership.
      hashStrList.emplace_back(std::to_wstring(hash));
      hashList.emplace_back(hashStrList.back().c_str());
      linker->RegisterLibrary(hashList.back(), pOutputBlob);
      pOutputBlob.Detach(); // Ownership is in libCache.
    }
    std::wstring wEntry = Unicode::UTF8ToUTF16StringOrThrow(pEntrypoint);
    std::wstring wTarget = Unicode::UTF8ToUTF16StringOrThrow(Target);

    // Link
    return linker->Link(wEntry.c_str(), wTarget.c_str(), hashList.data(),
                        hashList.size(), nullptr, 0, ppOperationResult);
  } catch (const std::bad_alloc &) {
    return E_OUTOFMEMORY;
  } catch (const CAtlException &err) {
    return err.m_hr;
  }
}

HRESULT WINAPI DxilD3DCompile(LPCVOID pSrcData, SIZE_T SrcDataSize,
                              LPCSTR pSourceName,
                              const D3D_SHADER_MACRO *pDefines,
                              IDxcIncludeHandler *pInclude, LPCSTR pEntrypoint,
                              LPCSTR pTarget, UINT Flags1, UINT Flags2,
                              ID3DBlob **ppCode, ID3DBlob **ppErrorMsgs) {
  CComPtr<IDxcLibrary> library;
  CComPtr<IDxcBlobEncoding> source;
  CComPtr<IDxcOperationResult> operationResult;
  *ppCode = nullptr;
  if (ppErrorMsgs != nullptr)
    *ppErrorMsgs = nullptr;

  IFR(CreateLibrary(&library));
  IFR(library->CreateBlobWithEncodingFromPinned((LPBYTE)pSrcData, SrcDataSize,
                                                CP_ACP, &source));
  HRESULT hr;
    CComPtr<IMalloc> m_pMalloc(GetGlobalHeapMalloc());
    DxcThreadMalloc TM(m_pMalloc);
  try {
    CA2W pFileName(pSourceName);

    std::vector<std::wstring> defineValues;
    std::vector<DxcDefine> defines;
    if (pDefines) {
      CONST D3D_SHADER_MACRO *pCursor = pDefines;

      // Convert to UTF-16.
      while (pCursor->Name) {
        defineValues.push_back(std::wstring(CA2W(pCursor->Name)));
        if (pCursor->Definition)
          defineValues.push_back(std::wstring(CA2W(pCursor->Definition)));
        else
          defineValues.push_back(std::wstring());
        ++pCursor;
      }

      // Build up array.
      pCursor = pDefines;
      size_t i = 0;
      while (pCursor->Name) {
        defines.push_back(
            DxcDefine{defineValues[i++].c_str(), defineValues[i++].c_str()});
        ++pCursor;
      }
    }

    std::vector<LPCWSTR> arguments;
    // /Gec, /Ges Not implemented:
    // if(Flags1 & D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY)
    // arguments.push_back(L"/Gec");  if(Flags1 & D3DCOMPILE_ENABLE_STRICTNESS)
    // arguments.push_back(L"/Ges");
    if (Flags1 & D3DCOMPILE_IEEE_STRICTNESS)
      arguments.push_back(L"/Gis");
    if (Flags1 & D3DCOMPILE_OPTIMIZATION_LEVEL2) {
      switch (Flags1 & D3DCOMPILE_OPTIMIZATION_LEVEL2) {
      case D3DCOMPILE_OPTIMIZATION_LEVEL0:
        arguments.push_back(L"/O0");
        break;
      case D3DCOMPILE_OPTIMIZATION_LEVEL2:
        arguments.push_back(L"/O2");
        break;
      case D3DCOMPILE_OPTIMIZATION_LEVEL3:
        arguments.push_back(L"/O3");
        break;
      }
    }
    // Currently, /Od turns off too many optimization passes, causing incorrect
    // DXIL to be generated. Re-enable once /Od is implemented properly:
    // if(Flags1 & D3DCOMPILE_SKIP_OPTIMIZATION) arguments.push_back(L"/Od");
    if (Flags1 & D3DCOMPILE_DEBUG)
      arguments.push_back(L"/Zi");
    if (Flags1 & D3DCOMPILE_PACK_MATRIX_ROW_MAJOR)
      arguments.push_back(L"/Zpr");
    if (Flags1 & D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR)
      arguments.push_back(L"/Zpc");
    if (Flags1 & D3DCOMPILE_AVOID_FLOW_CONTROL)
      arguments.push_back(L"/Gfa");
    if (Flags1 & D3DCOMPILE_PREFER_FLOW_CONTROL)
      arguments.push_back(L"/Gfp");
    // We don't implement this:
    // if(Flags1 & D3DCOMPILE_PARTIAL_PRECISION) arguments.push_back(L"/Gpp");
    if (Flags1 & D3DCOMPILE_RESOURCES_MAY_ALIAS)
      arguments.push_back(L"/res_may_alias");

    CompileFromBlob(source, pFileName, defines, pInclude, pEntrypoint, pTarget,
                    arguments, &operationResult);
    operationResult->GetStatus(&hr);
    if (SUCCEEDED(hr)) {
      return operationResult->GetResult((IDxcBlob **)ppCode);
    } else {
      if (ppErrorMsgs)
        operationResult->GetErrorBuffer((IDxcBlobEncoding **)ppErrorMsgs);
      return hr;
    }
  } catch (const std::bad_alloc &) {
    return E_OUTOFMEMORY;
  } catch (const CAtlException &err) {
    return err.m_hr;
  }
}

HRESULT WINAPI DxilD3DCompile2(
    LPCVOID pSrcData, SIZE_T SrcDataSize, LPCSTR pSourceName,
    LPCSTR pEntrypoint, LPCSTR pTarget,
    const DxcDefine *pDefines, // Array of defines
    UINT32 defineCount,        // Number of defines
    LPCWSTR *pArguments,       // Array of pointers to arguments
    UINT32 argCount,           // Number of arguments
    IDxcIncludeHandler *pInclude, IDxcOperationResult **ppOperationResult) {
  CComPtr<IDxcLibrary> library;
  CComPtr<IDxcBlobEncoding> source;

  *ppOperationResult = nullptr;

  IFR(CreateLibrary(&library));
  IFR(library->CreateBlobWithEncodingFromPinned((LPBYTE)pSrcData, SrcDataSize,
                                                CP_ACP, &source));
    CComPtr<IMalloc> m_pMalloc(GetGlobalHeapMalloc());
    DxcThreadMalloc TM(m_pMalloc);
  try {
    CA2W pFileName(pSourceName);
    std::vector<DxcDefine> defines(pDefines, pDefines + defineCount);
    std::vector<LPCWSTR> arguments(pArguments, pArguments + argCount);
    return CompileFromBlob(source, pFileName, defines, pInclude, pEntrypoint,
                           pTarget, arguments, ppOperationResult);
  } catch (const std::bad_alloc &) {
    return E_OUTOFMEMORY;
  } catch (const CAtlException &err) {
    return err.m_hr;
  }
}