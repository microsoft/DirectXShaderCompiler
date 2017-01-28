///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxcapi.impl.h                                                             //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides support for DXC API implementations.                             //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef __DXCAPI_IMPL__
#define __DXCAPI_IMPL__

#include "dxc/dxcapi.h"
#include "dxc/Support/microcom.h"
#include "llvm/Support/raw_ostream.h"

// Simple adaptor for IStream. Can probably do better.
class raw_stream_ostream : public llvm::raw_ostream {
private:
  CComPtr<hlsl::AbstractMemoryStream> m_pStream;
  void write_impl(const char *Ptr, size_t Size) override {
    ULONG cbWritten;
    IFT(m_pStream->Write(Ptr, Size, &cbWritten));
  }
  uint64_t current_pos() const { return m_pStream->GetPosition(); }
public:
  raw_stream_ostream(hlsl::AbstractMemoryStream* pStream) : m_pStream(pStream) { }
  ~raw_stream_ostream() override {
    flush();
  }
};

class DxcOperationResult : public IDxcOperationResult {
private:
  DXC_MICROCOM_REF_FIELD(m_dwRef)

  DxcOperationResult(_In_opt_ IDxcBlob *pResultBlob,
    _In_opt_ IDxcBlobEncoding *pErrorBlob, HRESULT status)
    : m_dwRef(0), m_status(status), m_result(pResultBlob),
    m_errors(pErrorBlob) {}

public:
  DXC_MICROCOM_ADDREF_RELEASE_IMPL(m_dwRef)

  HRESULT m_status;
  CComPtr<IDxcBlob> m_result;
  CComPtr<IDxcBlobEncoding> m_errors;

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) {
    return DoBasicQueryInterface<IDxcOperationResult>(this, iid, ppvObject);
  }

  static HRESULT CreateFromResultErrorStatus(_In_opt_ IDxcBlob *pResultBlob,
                                             _In_opt_ IDxcBlobEncoding *pErrorBlob,
                                             HRESULT status,
                                             _COM_Outptr_ IDxcOperationResult **ppResult) {
    *ppResult = nullptr;
    CComPtr<DxcOperationResult> result = new (std::nothrow) DxcOperationResult(pResultBlob, pErrorBlob, status);
    if (result.p == nullptr) return E_OUTOFMEMORY;
    *ppResult = result.Detach();
    return S_OK;
  }

  static HRESULT
  CreateFromUtf8Strings(_In_opt_z_ LPCSTR pErrorStr,
      _In_opt_z_ LPCSTR pResultStr, HRESULT status,
      _COM_Outptr_ IDxcOperationResult **pResult) {
    *pResult = nullptr;
    CComPtr<IDxcBlobEncoding> resultBlob;
    CComPtr<IDxcBlobEncoding> errorBlob;
    CComPtr<DxcOperationResult> result;

    HRESULT hr = S_OK;

    if (pErrorStr != nullptr) {
      hr = hlsl::DxcCreateBlobWithEncodingOnHeapCopy(
        pErrorStr, strlen(pErrorStr), CP_UTF8, &errorBlob);
      if (FAILED(hr)) {
        return hr;
      }
    }

    if (pResultStr != nullptr) {
      hr = hlsl::DxcCreateBlobWithEncodingOnHeap(
        pResultStr, strlen(pResultStr), CP_UTF8, &resultBlob);
      if (FAILED(hr)) {
        return hr;
      }
    }

    return CreateFromResultErrorStatus(resultBlob, errorBlob, status, pResult);
  }

  __override HRESULT STDMETHODCALLTYPE GetStatus(_Out_ HRESULT *pStatus) {
    if (pStatus == nullptr)
      return E_INVALIDARG;

    *pStatus = m_status;
    return S_OK;
  }

  __override HRESULT STDMETHODCALLTYPE
    GetResult(_COM_Outptr_result_maybenull_ IDxcBlob **ppResult) {
    return m_result.CopyTo(ppResult);
  }

  __override HRESULT STDMETHODCALLTYPE
    GetErrorBuffer(_COM_Outptr_result_maybenull_ IDxcBlobEncoding **ppErrors) {
    return m_errors.CopyTo(ppErrors);
  }
};

#endif
