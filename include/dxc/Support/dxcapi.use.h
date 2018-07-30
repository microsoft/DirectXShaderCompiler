//////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxcapi.use.h                                                              //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides support for DXC API users.                                       //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef __DXCAPI_USE_H__
#define __DXCAPI_USE_H__

#include "dxc/dxcapi.h"
#include "llvm/Support/DynamicLibrary.h"

namespace dxc {

// Helper class to dynamically load the dxcompiler or a compatible libraries.
class DxcDllSupport {
protected:
  using DynamicLibrary = llvm::sys::DynamicLibrary;
  DynamicLibrary m_dll;
  DxcCreateInstanceProc m_createFn;
  DxcCreateInstance2Proc m_createFn2;

  HRESULT InitializeInternal(LPCWSTR dllName, LPCSTR fnName) {
    if (m_dll.isValid()) return S_OK;
    m_dll = DynamicLibrary::getPermanentLibrary(CW2A(dllName));
    if (!m_dll.isValid()) return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    m_createFn = (DxcCreateInstanceProc)m_dll.getAddressOfSymbol(fnName);

    if (m_createFn == nullptr) {
      HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
      m_dll = DynamicLibrary();
      return hr;
    }

    // Only basic functions used to avoid requiring additional headers.
    m_createFn2 = nullptr;
    char fnName2[128];
    size_t s = strlen(fnName);
    if (s < sizeof(fnName2) - 2) {
      memcpy(fnName2, fnName, s);
      fnName2[s] = '2';
      fnName2[s + 1] = '\0';
      m_createFn2 = (DxcCreateInstance2Proc)m_dll.getAddressOfSymbol(fnName2);
    }

    return S_OK;
  }

public:
  DxcDllSupport() : m_dll(), m_createFn(nullptr), m_createFn2(nullptr) {
  }

  DxcDllSupport(DxcDllSupport&& other) {
    m_dll = other.m_dll; other.m_dll = DynamicLibrary();
    m_createFn = other.m_createFn; other.m_createFn = nullptr;
    m_createFn2 = other.m_createFn2; other.m_createFn2 = nullptr;
  }

  ~DxcDllSupport() {
    Cleanup();
  }

  HRESULT Initialize() {
    #ifdef _WIN32
    return InitializeInternal(L"dxcompiler.dll", "DxcCreateInstance");
    #elif __APPLE__
    return InitializeInternal(L"libdxcompiler.dylib", "DxcCreateInstance");
    #else
    return InitializeInternal(L"libdxcompiler.so", "DxcCreateInstance");
    #endif
  }

  HRESULT InitializeForDll(_In_z_ const wchar_t* dll, _In_z_ const char* entryPoint) {
    return InitializeInternal(dll, entryPoint);
  }

  template <typename TInterface>
  HRESULT CreateInstance(REFCLSID clsid, _Outptr_ TInterface** pResult) {
    return CreateInstance(clsid, __uuidof(TInterface), (IUnknown**)pResult);
  }

  HRESULT CreateInstance(REFCLSID clsid, REFIID riid, _Outptr_ IUnknown **pResult) {
    if (pResult == nullptr) return E_POINTER;
    if (!m_dll.isValid()) return E_FAIL;
    HRESULT hr = m_createFn(clsid, riid, (LPVOID*)pResult);
    return hr;
  }

  template <typename TInterface>
  HRESULT CreateInstance2(IMalloc *pMalloc, REFCLSID clsid, _Outptr_ TInterface** pResult) {
    return CreateInstance2(pMalloc, clsid, __uuidof(TInterface), (IUnknown**)pResult);
  }

  HRESULT CreateInstance2(IMalloc *pMalloc, REFCLSID clsid, REFIID riid, _Outptr_ IUnknown **pResult) {
    if (pResult == nullptr) return E_POINTER;
    if (!m_dll.isValid()) return E_FAIL;
    if (m_createFn2 == nullptr) return E_FAIL;
    HRESULT hr = m_createFn2(pMalloc, clsid, riid, (LPVOID*)pResult);
    return hr;
  }

  bool HasCreateWithMalloc() const {
    return m_createFn2 != nullptr;
  }

  bool IsEnabled() const {
    return m_dll.isValid();
  }

  void Cleanup() {
    if (m_dll.isValid()) {
      m_createFn = nullptr;
      m_createFn2 = nullptr;
      m_dll = DynamicLibrary();
    }
  }

  DynamicLibrary Detach() {
    DynamicLibrary module = m_dll;
    m_dll = DynamicLibrary();
    return module;
  }
};

inline DxcDefine GetDefine(_In_ LPCWSTR name, LPCWSTR value) {
  DxcDefine result;
  result.Name = name;
  result.Value = value;
  return result;
}

// Checks an HRESULT and formats an error message with the appended data.
void IFT_Data(HRESULT hr, _In_opt_ LPCWSTR data);

void EnsureEnabled(DxcDllSupport &dxcSupport);
void ReadFileIntoBlob(DxcDllSupport &dxcSupport, _In_ LPCWSTR pFileName,
                      _Outptr_ IDxcBlobEncoding **ppBlobEncoding);
void WriteBlobToConsole(_In_opt_ IDxcBlob *pBlob, DWORD streamType = STD_OUTPUT_HANDLE);
void WriteBlobToFile(_In_opt_ IDxcBlob *pBlob, _In_ LPCWSTR pFileName);
void WriteBlobToHandle(_In_opt_ IDxcBlob *pBlob, HANDLE hFile, _In_opt_ LPCWSTR pFileName);
void WriteUtf8ToConsole(_In_opt_count_(charCount) const char *pText,
                        int charCount, DWORD streamType = STD_OUTPUT_HANDLE);
void WriteUtf8ToConsoleSizeT(_In_opt_count_(charCount) const char *pText,
                             size_t charCount, DWORD streamType = STD_OUTPUT_HANDLE);
void WriteOperationErrorsToConsole(_In_ IDxcOperationResult *pResult,
                                   bool outputWarnings);
void WriteOperationResultToConsole(_In_ IDxcOperationResult *pRewriteResult,
                                   bool outputWarnings);

} // namespace dxc

#endif
