///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxlib_sample.cpp                                                          //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Implements compile function which compile shader to lib then link.        //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/Support/WinIncludes.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/FileIOHelper.h"
#include "dxc/Support/Unicode.h"
#include "dxc/Support/dxcapi.use.h"
#include "dxc/Support/microcom.h"
#include "dxc/dxcapi.h"
#include "dxc/dxctools.h"
#include "clang/Frontend/CompilerInstance.h"
#include "llvm/Support/Path.h"
#include <cwchar>

using namespace hlsl;
using namespace llvm;

namespace {
bool IsAbsoluteOrCurDirRelative(const Twine &T) {
  if (llvm::sys::path::is_absolute(T)) {
    return true;
  }
  if (T.isSingleStringRef()) {
    StringRef r = T.getSingleStringRef();
    if (r.size() < 2)
      return false;
    const char *pData = r.data();
    return pData[0] == '.' && (pData[1] == '\\' || pData[1] == '/');
  }
  DXASSERT(false, "twine kind not supported");
  return false;
}
/// Max number of included files (1:1 to their directories) or search
/// directories. If programs include more than a handful, DxcArgsFileSystem will
/// need to do better than linear scans. If this is fired,
/// ERROR_OUT_OF_STRUCTURES will be returned by an attempt to open a file.
static const size_t MaxIncludedFiles = 200;

class IncludeToLibPreprocessor : public IDxcIncludeHandler {
public:
  DXC_MICROCOM_ADDREF_RELEASE_IMPL(m_dwRef)
  virtual ~IncludeToLibPreprocessor() {}
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) {
    return DoBasicQueryInterface<::IDxcIncludeHandler>(this, riid, ppvObject);
  }

  IncludeToLibPreprocessor(IDxcIncludeHandler *handler)
      : m_dwRef(0), m_pIncludeHandler(handler) {
    if (!m_dllSupport.IsEnabled())
      m_dllSupport.Initialize();
  }

  __override HRESULT STDMETHODCALLTYPE LoadSource(
      _In_ LPCWSTR pFilename, // Candidate filename.
      _COM_Outptr_result_maybenull_ IDxcBlob **ppIncludeSource // Resultant
                                                               // source object
                                                               // for included
                                                               // file, nullptr
                                                               // if not found.
  ) {
    // Read file.
    CComPtr<IDxcBlob> pIncludeSource;
    IFT(m_pIncludeHandler->LoadSource(pFilename, &pIncludeSource));
    // Preprocess the header to remove function body and static function/global.
    if (SUCCEEDED(Preprocess(pIncludeSource, pFilename))) {
      return hlsl::DxcCreateBlobOnHeapCopy(m_curHeaderContent.data(),
                                           m_curHeaderContent.size(),
                                           ppIncludeSource);
    } else {
      IFT(m_pIncludeHandler->LoadSource(pFilename, ppIncludeSource));
      return E_FAIL;
    }
  }

  void SetupDefines(const DxcDefine *pDefines, unsigned defineCount);
  HRESULT Preprocess(IDxcBlob *pSource, LPCWSTR pFilename);

  const std::vector<std::string> &GetHeaders() const { return m_headers; }

private:
  DXC_MICROCOM_REF_FIELD(m_dwRef)
  IDxcIncludeHandler *m_pIncludeHandler;
  // Vector to save headers.
  std::string m_curHeaderContent;
  // Processed header content.
  std::vector<std::string> m_headers;
  // Defines.
  std::vector<std::wstring> m_defineStrs;
  std::vector<DxcDefine> m_defines;

  dxc::DxcDllSupport m_dllSupport;
};

void IncludeToLibPreprocessor::SetupDefines(const DxcDefine *pDefines,
                                            unsigned defineCount) {
  for (unsigned i = 0; i < defineCount; i++) {
    const DxcDefine &define = pDefines[i];

    m_defineStrs.emplace_back(define.Name);
    DxcDefine tmpDefine;
    tmpDefine.Name = m_defineStrs.back().c_str();
    tmpDefine.Value = nullptr;
    if (define.Value) {
      m_defineStrs.emplace_back(define.Value);
      tmpDefine.Value = m_defineStrs.back().c_str();
    }
    m_defines.emplace_back(tmpDefine);
  }
}

HRESULT IncludeToLibPreprocessor::Preprocess(IDxcBlob *pSource,
                                             LPCWSTR pFilename) {
  CComPtr<IDxcBlobEncoding> pEncodingIncludeSource;
  IFR(DxcCreateBlobWithEncodingSet(GetGlobalHeapMalloc(), pSource, CP_UTF8,
                                   &pEncodingIncludeSource));

  // Create header with no function body.
  CComPtr<IDxcRewriter> pRewriter;
  m_dllSupport.CreateInstance(CLSID_DxcRewriter, &pRewriter);

  CComPtr<IDxcOperationResult> pRewriteResult;
  IFT(pRewriter->RewriteUnchangedWithInclude(
      pEncodingIncludeSource, pFilename, m_defines.data(), m_defines.size(),
      this,
      RewriterOptionMask::SkipFunctionBody | RewriterOptionMask::SkipStatic,
      &pRewriteResult));

  HRESULT status;
  if (!SUCCEEDED(pRewriteResult->GetStatus(&status)) || !SUCCEEDED(status)) {
    CComPtr<IDxcBlobEncoding> pErr;
    IFT(pRewriteResult->GetErrorBuffer(&pErr));
    std::string errString =
        std::string((char *)pErr->GetBufferPointer(), pErr->GetBufferSize());
    IFTMSG(E_FAIL, errString);
    return E_FAIL;
  };
  // Append existing header.
  std::string includeSource =
      m_curHeaderContent + std::string((char *)pSource->GetBufferPointer(),
                                       pSource->GetBufferSize());
  m_headers.push_back(includeSource);

  // Save the content of processed header.
  CComPtr<IDxcBlob> result;
  IFT(pRewriteResult->GetResult(&result));
  // Update m_curHeaderContent.
  m_curHeaderContent = std::string((char *)(result)->GetBufferPointer(),
                                   (result)->GetBufferSize());
  return S_OK;
}
} // namespace

namespace {
// Include handler which ignore all include.
class EmptyIncludeHandler : public IDxcIncludeHandler {
public:
  DXC_MICROCOM_ADDREF_RELEASE_IMPL(m_dwRef)

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) {
    return DoBasicQueryInterface<::IDxcIncludeHandler>(this, riid, ppvObject);
  }

  EmptyIncludeHandler() {}

  __override HRESULT STDMETHODCALLTYPE LoadSource(
      _In_ LPCWSTR pFilename, // Candidate filename.
      _COM_Outptr_result_maybenull_ IDxcBlob **ppIncludeSource // Resultant
                                                               // source object
                                                               // for included
                                                               // file, nullptr
                                                               // if not found.
  ) {
    // Return empty source.
    if (ppIncludeSource) {
      return hlsl::DxcCreateBlobOnHeapCopy(" ", 1, ppIncludeSource);
    } else {
      return E_FAIL;
    }
  }

private:
  DXC_MICROCOM_REF_FIELD(m_dwRef)
};
} // namespace

#include "clang/CodeGen/CodeGenAction.h"
#include "clang/Frontend/FrontendOptions.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/Hashing.h"
#include "llvm/IR/LLVMContext.h"
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
namespace {

struct KeyHash {
  std::size_t operator()(const hash_code &k) const { return k; }
};
struct KeyEqual {
  bool operator()(const hash_code &l, const hash_code &r) const {
    return l == r;
  }
};

struct CompileInput {
  std::vector<DxcDefine> &defines;
  std::vector<LPCWSTR> &arguments;
};

class LibCacheManager {
public:
  HRESULT AddLibBlob(IDxcBlob *pSource, CompileInput &compiler, size_t &hash,
                     IDxcBlob **pResultLib,
                     std::function<void(void)> compileFn);
  bool GetLibBlob(IDxcBlob *pSource, CompileInput &compiler, size_t &hash,
                  IDxcBlob **pResultLib);

private:
  hash_code GetHash(IDxcBlob *pSource, CompileInput &compiler);
  using libCacheType =
      std::unordered_map<hash_code, CComPtr<IDxcBlob>, KeyHash, KeyEqual>;
  libCacheType m_libCache;
  std::shared_mutex m_mutex;
};

static hash_code CombineWStr(hash_code hash, LPCWSTR Arg) {
  unsigned length = std::wcslen(Arg)*2;
  return hash_combine(hash, StringRef((char*)(Arg), length));
}

hash_code LibCacheManager::GetHash(IDxcBlob *pSource, CompileInput &compiler) {
  hash_code libHash = hash_value(
      StringRef((char *)pSource->GetBufferPointer(), pSource->GetBufferSize()));
  // Combine compile input.
  for (auto &Arg : compiler.arguments) {
    libHash = CombineWStr(libHash, Arg);
  }
  for (auto &Define : compiler.defines) {
    libHash = CombineWStr(libHash, Define.Name);
    if (Define.Value) {
      libHash = CombineWStr(libHash, Define.Value);
    }
  }
  return libHash;
}

bool LibCacheManager::GetLibBlob(IDxcBlob *pSource, CompileInput &compiler,
                                 size_t &hash, IDxcBlob **pResultLib) {
  if (!pSource || !pResultLib) {
    return false;
  }
  // Create hash from source.
  hash_code libHash = GetHash(pSource, compiler);
  hash = libHash;
  // lock
  std::shared_lock<std::shared_mutex> lk(m_mutex);

  auto it = m_libCache.find(libHash);
  if (it != m_libCache.end()) {
    *pResultLib = it->second;
    return true;
  } else {
    return false;
  }
}

HRESULT
LibCacheManager::AddLibBlob(IDxcBlob *pSource, CompileInput &compiler,
                            size_t &hash, IDxcBlob **pResultLib,
                            std::function<void(void)> compileFn) {
  if (!pSource || !pResultLib) {
    return E_FAIL;
  }

  std::unique_lock<std::shared_mutex> lk(m_mutex);

  auto it = m_libCache.find(hash);
  if (it != m_libCache.end()) {
    *pResultLib = it->second;
    return S_OK;
  }

  compileFn();

  m_libCache[hash] = *pResultLib;

  return S_OK;
}

LibCacheManager &GetLibCacheManager() {
  static LibCacheManager g_LibCache;
  return g_LibCache;
}

} // namespace

#include "dxc/HLSL/DxilContainer.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/WinIncludes.h"
#include "dxc/dxcapi.h"

#include <d3dcompiler.h>
#include <string>
#include <vector>

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

#include "dxc/HLSL/DxilLinker.h"
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
    CComPtr<IncludeToLibPreprocessor> preprocessor(
        new IncludeToLibPreprocessor(pInclude));
    preprocessor->SetupDefines(defines.data(), defines.size());

    preprocessor->Preprocess(pSource, pSourceName);

    CompileInput compilerInput{defines, arguments};

    EmptyIncludeHandler emptyIncludeHandler;
    emptyIncludeHandler.AddRef(); // On stack - don't try to delete on last Release().
    EmptyIncludeHandler *pEmptyIncludeHandler = &emptyIncludeHandler;
    LibCacheManager &libCache = GetLibCacheManager();
    LLVMContext llvmContext;
    IFR(CreateLinker(&linker));

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
          IFT(CompileToLib(pSource, defines, pEmptyIncludeHandler, arguments,
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
    std::wstring wTarget = Unicode::UTF8ToUTF16StringOrThrow(pTarget);

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

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD Reason, LPVOID) {
  BOOL result = TRUE;
  if (Reason == DLL_PROCESS_ATTACH) {
    DisableThreadLibraryCalls(hinstDLL);
  } else if (Reason == DLL_PROCESS_DETACH) {
    // Nothing to clean-up.
  }

  return result;
}
