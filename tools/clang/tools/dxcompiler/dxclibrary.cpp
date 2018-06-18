///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxclibrary.cpp                                                            //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
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
  DXC_MICROCOM_TM_REF_FIELDS()
public:
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()
  DXC_MICROCOM_TM_CTOR(DxcIncludeHandlerForFS)

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) override {
    return DoBasicQueryInterface<IDxcIncludeHandler>(this, iid, ppvObject);
  }

  HRESULT STDMETHODCALLTYPE LoadSource(
    _In_ LPCWSTR pFilename,                                   // Candidate filename.
    _COM_Outptr_result_maybenull_ IDxcBlob **ppIncludeSource  // Resultant source object for included file, nullptr if not found.
    ) override {
    CComPtr<IDxcBlobEncoding> pEncoding;
    HRESULT hr = ::hlsl::DxcCreateBlobFromFile(m_pMalloc, pFilename, nullptr, &pEncoding);
    if (SUCCEEDED(hr)) {
      *ppIncludeSource = pEncoding.Detach();
    }
    return hr;
  }
};

class DxcLibrary : public IDxcLibrary {
private:
  DXC_MICROCOM_TM_REF_FIELDS()
public:
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()
  DXC_MICROCOM_TM_CTOR(DxcLibrary)
  
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) override {
    return DoBasicQueryInterface<IDxcLibrary>(this, iid, ppvObject);
  }

  HRESULT STDMETHODCALLTYPE SetMalloc(_In_opt_ IMalloc *pMalloc) override {
    UNREFERENCED_PARAMETER(pMalloc);
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE CreateBlobFromBlob(
    _In_ IDxcBlob *pBlob, UINT32 offset, UINT32 length, _COM_Outptr_ IDxcBlob **ppResult) override {
    DxcThreadMalloc TM(m_pMalloc);
    return ::hlsl::DxcCreateBlobFromBlob(pBlob, offset, length, ppResult);
  }

  HRESULT STDMETHODCALLTYPE CreateBlobFromFile(
    LPCWSTR pFileName, _In_opt_ UINT32* pCodePage,
    _COM_Outptr_ IDxcBlobEncoding **pBlobEncoding) override {
    DxcThreadMalloc TM(m_pMalloc);
    return ::hlsl::DxcCreateBlobFromFile(pFileName, pCodePage, pBlobEncoding);
  }

  HRESULT STDMETHODCALLTYPE CreateBlobWithEncodingFromPinned(
    LPBYTE pText, UINT32 size, UINT32 codePage,
    _COM_Outptr_ IDxcBlobEncoding **pBlobEncoding) override {
    DxcThreadMalloc TM(m_pMalloc);
    return ::hlsl::DxcCreateBlobWithEncodingFromPinned(pText, size, codePage, pBlobEncoding);
  }

  HRESULT STDMETHODCALLTYPE CreateBlobWithEncodingOnHeapCopy(
      _In_bytecount_(size) LPCVOID pText, UINT32 size, UINT32 codePage,
      _COM_Outptr_ IDxcBlobEncoding **pBlobEncoding) override {
    DxcThreadMalloc TM(m_pMalloc);
    return ::hlsl::DxcCreateBlobWithEncodingOnHeapCopy(pText, size, codePage,
                                                       pBlobEncoding);
  }

  HRESULT STDMETHODCALLTYPE CreateBlobWithEncodingOnMalloc(
    _In_bytecount_(size) LPCVOID pText, IMalloc *pIMalloc, UINT32 size, UINT32 codePage,
    _COM_Outptr_ IDxcBlobEncoding **pBlobEncoding) override {
    DxcThreadMalloc TM(m_pMalloc);
    return ::hlsl::DxcCreateBlobWithEncodingOnMalloc(pText, pIMalloc, size, codePage, pBlobEncoding);
  }

  HRESULT STDMETHODCALLTYPE CreateIncludeHandler(
    _COM_Outptr_ IDxcIncludeHandler **ppResult) override {
    DxcThreadMalloc TM(m_pMalloc);
    CComPtr<DxcIncludeHandlerForFS> result;
    result = DxcIncludeHandlerForFS::Alloc(m_pMalloc);
    if (result.p == nullptr) {
      return E_OUTOFMEMORY;
    }
    *ppResult = result.Detach();
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE CreateStreamFromBlobReadOnly(
    _In_ IDxcBlob *pBlob, _COM_Outptr_ IStream **ppStream) override {
    DxcThreadMalloc TM(m_pMalloc);
    return ::hlsl::CreateReadOnlyBlobStream(pBlob, ppStream);
  }

  HRESULT STDMETHODCALLTYPE GetBlobAsUtf8(
    _In_ IDxcBlob *pBlob, _COM_Outptr_ IDxcBlobEncoding **pBlobEncoding) override {
    DxcThreadMalloc TM(m_pMalloc);
    return ::hlsl::DxcGetBlobAsUtf8(pBlob, pBlobEncoding);
  }

  HRESULT STDMETHODCALLTYPE GetBlobAsUtf16(
    _In_ IDxcBlob *pBlob, _COM_Outptr_ IDxcBlobEncoding **pBlobEncoding) override {
    return ::hlsl::DxcGetBlobAsUtf16(pBlob, m_pMalloc, pBlobEncoding);
  }
};

HRESULT CreateDxcLibrary(_In_ REFIID riid, _Out_ LPVOID* ppv) {
  CComPtr<DxcLibrary> result = DxcLibrary::Alloc(DxcGetThreadMallocNoRef());
  if (result == nullptr) {
    *ppv = nullptr;
    return E_OUTOFMEMORY;
  }

  return result.p->QueryInterface(riid, ppv);
}
