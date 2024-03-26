///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// NoSigDxilLibValidator.cpp                                                 //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Implements the DirectX Validator object.                                  //
// Merely adds the version queries to the Dxilvalidator                      //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/HLSL/DxcValidatorBase.h"
#include "dxc/HLSL/DxilValidation.h"
#include "dxc/Support/FileIOHelper.h"
#include "dxc/Support/dxcapi.impl.h"

class NoSigDxilLibValidator : public DxcValidatorBase, public IDxcVersionInfo {
private:
  DXC_MICROCOM_TM_REF_FIELDS()

public:
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()
  NoSigDxilLibValidator(IMalloc *pMalloc)
      : DxcValidatorBase(pMalloc), m_dwRef(0), m_pMalloc(pMalloc) {}
  DXC_MICROCOM_TM_ALLOC(NoSigDxilLibValidator)

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,
                                           void **ppvObject) override {
    return DoBasicQueryInterface<IDxcValidator, IDxcValidator2,
                                 IDxcVersionInfo>(this, iid, ppvObject);
  }

  // IDxcVersionInfo
  HRESULT STDMETHODCALLTYPE GetVersion(UINT32 *pMajor, UINT32 *pMinor) override;
  HRESULT STDMETHODCALLTYPE GetFlags(UINT32 *pFlags) override;
};

HRESULT STDMETHODCALLTYPE NoSigDxilLibValidator::GetVersion(UINT32 *pMajor,
                                                            UINT32 *pMinor) {
  if (pMajor == nullptr || pMinor == nullptr)
    return E_INVALIDARG;
  hlsl::GetValidationVersion(pMajor, pMinor);
  return S_OK;
}

HRESULT STDMETHODCALLTYPE NoSigDxilLibValidator::GetFlags(UINT32 *pFlags) {
  if (pFlags == nullptr)
    return E_INVALIDARG;
  *pFlags = DxcVersionInfoFlags_None;
#ifndef NDEBUG
  *pFlags |= DxcVersionInfoFlags_Debug;
#endif
  return S_OK;
}

HRESULT CreateDxcValidator(REFIID riid, LPVOID *ppv) {
  try {
    CComPtr<NoSigDxilLibValidator> result(
        NoSigDxilLibValidator::Alloc(DxcGetThreadMallocNoRef()));
    IFROOM(result.p);
    return result.p->QueryInterface(riid, ppv);
  }
  CATCH_CPP_RETURN_HRESULT();
}
