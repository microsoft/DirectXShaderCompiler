///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxclibrary.cpp                                                            //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// Implements the DirectX Compiler Library helper.                           //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/Support/WinIncludes.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/Unicode.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MSFileSystem.h"
#include "dxc/Support/microcom.h"
#include "dxc/Support/FileIOHelper.h"

#include "dxc/dxcapi.internal.h"
#include "dxc/dxctools.h"

using namespace llvm;
using namespace hlsl;

class DxcIncludeHandlerForFS : public IDxcIncludeHandler {
private:
  DXC_MICROCOM_REF_FIELD(m_dwRef)
public:
  DXC_MICROCOM_ADDREF_RELEASE_IMPL(m_dwRef)

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) {
    return DoBasicQueryInterface<IDxcIncludeHandler>(this, iid, ppvObject);
  }

  DxcIncludeHandlerForFS() : m_dwRef(0) { }

  __override HRESULT STDMETHODCALLTYPE LoadSource(
    _In_ LPCWSTR pFilename,                                   // Candidate filename.
    _COM_Outptr_result_maybenull_ IDxcBlob **ppIncludeSource  // Resultant source object for included file, nullptr if not found.
    ) {
    CComPtr<IDxcBlobEncoding> pEncoding;
    HRESULT hr = ::hlsl::DxcCreateBlobFromFile(pFilename, nullptr, &pEncoding);
    if (SUCCEEDED(hr)) {
      *ppIncludeSource = pEncoding.Detach();
    }
    return hr;
  }
};

class DxcLibrary : public IDxcLibrary {
private:
  DXC_MICROCOM_REF_FIELD(m_dwRef)
public:
  DXC_MICROCOM_ADDREF_RELEASE_IMPL(m_dwRef)

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) {
    return DoBasicQueryInterface<IDxcLibrary>(this, iid, ppvObject);
  }

  __override HRESULT STDMETHODCALLTYPE SetMalloc(_In_opt_ IMalloc *pMalloc) {
    UNREFERENCED_PARAMETER(pMalloc);
    return E_NOTIMPL;
  }

  __override HRESULT STDMETHODCALLTYPE CreateBlobFromBlob(
    _In_ IDxcBlob *pBlob, UINT32 offset, UINT32 length, _COM_Outptr_ IDxcBlob **ppResult) {
    return ::hlsl::DxcCreateBlobFromBlob(pBlob, offset, length, ppResult);
  }

  __override HRESULT STDMETHODCALLTYPE CreateBlobFromFile(
    LPCWSTR pFileName, _In_opt_ UINT32* pCodePage,
    _COM_Outptr_ IDxcBlobEncoding **pBlobEncoding) {
    return ::hlsl::DxcCreateBlobFromFile(pFileName, pCodePage, pBlobEncoding);
  }

  __override HRESULT STDMETHODCALLTYPE CreateBlobWithEncodingFromPinned(
    LPBYTE pText, UINT32 size, UINT32 codePage,
    _COM_Outptr_ IDxcBlobEncoding **pBlobEncoding) {
    return ::hlsl::DxcCreateBlobWithEncodingFromPinned(pText, size, codePage, pBlobEncoding);
  }

  __override HRESULT STDMETHODCALLTYPE CreateBlobWithEncodingOnHeapCopy(
      _In_bytecount_(size) LPCVOID pText, UINT32 size, UINT32 codePage,
      _COM_Outptr_ IDxcBlobEncoding **pBlobEncoding) {
    return ::hlsl::DxcCreateBlobWithEncodingOnHeapCopy(pText, size, codePage,
                                                       pBlobEncoding);
  }

  __override HRESULT STDMETHODCALLTYPE CreateBlobWithEncodingOnMalloc(
    _In_bytecount_(size) LPCVOID pText, IMalloc *pIMalloc, UINT32 size, UINT32 codePage,
    _COM_Outptr_ IDxcBlobEncoding **pBlobEncoding) {
    return ::hlsl::DxcCreateBlobWithEncodingOnMalloc(pText, pIMalloc, size, codePage, pBlobEncoding);
  }

  __override HRESULT STDMETHODCALLTYPE CreateIncludeHandler(
    _COM_Outptr_ IDxcIncludeHandler **ppResult) {
    CComPtr<DxcIncludeHandlerForFS> result;
    result = new (std::nothrow) DxcIncludeHandlerForFS();
    if (result.p == nullptr) {
      return E_OUTOFMEMORY;
    }
    *ppResult = result.Detach();
    return S_OK;
  }

  __override HRESULT STDMETHODCALLTYPE CreateStreamFromBlobReadOnly(
    _In_ IDxcBlob *pBlob, _COM_Outptr_ IStream **ppStream) {
    return ::hlsl::CreateReadOnlyBlobStream(pBlob, ppStream);
  }

  __override HRESULT STDMETHODCALLTYPE GetBlobAsUtf8(
    _In_ IDxcBlob *pBlob, _COM_Outptr_ IDxcBlobEncoding **pBlobEncoding) {
    return ::hlsl::DxcGetBlobAsUtf8(pBlob, pBlobEncoding);
  }

  __override HRESULT STDMETHODCALLTYPE GetBlobAsUtf16(
    _In_ IDxcBlob *pBlob, _COM_Outptr_ IDxcBlobEncoding **pBlobEncoding) {
    return ::hlsl::DxcGetBlobAsUtf16(pBlob, pBlobEncoding);
  }
};

HRESULT CreateDxcLibrary(_In_ REFIID riid, _Out_ LPVOID* ppv) {
  CComPtr<DxcLibrary> result = new (std::nothrow) DxcLibrary();
  if (result == nullptr) {
    *ppv = nullptr;
    return E_OUTOFMEMORY;
  }

  return result.p->QueryInterface(riid, ppv);
}
