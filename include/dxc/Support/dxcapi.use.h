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

namespace dxc {

// Helper class to dynamically load the dxcompiler or a compatible libraries.
class DxcDllSupport {
protected:
  HMODULE m_dll;
  DxcCreateInstanceProc m_createFn;

  HRESULT InitializeInternal(LPCWSTR dllName, LPCSTR fnName) {
    if (m_dll != nullptr) return S_OK;
    m_dll = LoadLibraryW(dllName);

    if (m_dll == nullptr) return HRESULT_FROM_WIN32(GetLastError());
    m_createFn = (DxcCreateInstanceProc)GetProcAddress(m_dll, fnName);

    if (m_createFn == nullptr) {
      HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
      FreeLibrary(m_dll);
      m_dll = nullptr;
      return hr;
    }

    return S_OK;
  }

public:
  DxcDllSupport() : m_dll(nullptr), m_createFn(nullptr) {
  }

  DxcDllSupport(DxcDllSupport&& other) {
    m_dll = other.m_dll; other.m_dll = nullptr;
    m_createFn = other.m_createFn; other.m_dll = nullptr;
  }

  ~DxcDllSupport() {
    Cleanup();
  }

  HRESULT Initialize() {
    return InitializeInternal(L"dxcompiler.dll", "DxcCreateInstance");
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
    if (m_dll == nullptr) return E_FAIL;
    HRESULT hr = m_createFn(clsid, riid, (LPVOID*)pResult);
    return hr;
  }

  bool IsEnabled() const {
    return m_dll != nullptr;
  }

  void Cleanup() {
    if (m_dll != nullptr) {
      m_createFn = nullptr;
      FreeLibrary(m_dll);
      m_dll = nullptr;
    }
  }

  HMODULE Detach() {
    HMODULE module = m_dll;
    m_dll = nullptr;
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
void WriteBlobToConsole(_In_opt_ IDxcBlob *pBlob);
void WriteBlobToFile(_In_opt_ IDxcBlob *pBlob, _In_ LPCWSTR pFileName);
void WriteBlobToHandle(_In_opt_ IDxcBlob *pBlob, HANDLE hFile, _In_opt_ LPCWSTR pFileName);
void WriteUtf8ToConsole(_In_opt_count_(charCount) const char *pText,
                        int charCount);
void WriteUtf8ToConsoleSizeT(_In_opt_count_(charCount) const char *pText,
                             size_t charCount);
void WriteOperationErrorsToConsole(_In_ IDxcOperationResult *pResult,
                                   bool outputWarnings);
void WriteOperationResultToConsole(_In_ IDxcOperationResult *pRewriteResult,
                                   bool outputWarnings);

} // namespace dxc

#endif
