///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxcvalidator.cpp                                                         //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Implements the DirectX Validator object, including hashing support.       //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxcvalidator.h"

#include "dxc/Support/WinIncludes.h"

#include "dxc/Support/Global.h"
#include "dxc/Support/microcom.h"
#include "dxc/dxcapi.h"

using namespace llvm;
using namespace hlsl;

class DxcValidator : public IDxcValidator2, public IDxcVersionInfo {
private:
  DXC_MICROCOM_TM_REF_FIELDS()

public:
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()
  DXC_MICROCOM_TM_CTOR(DxcValidator)

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,
                                           void **ppvObject) override {

    return DoBasicQueryInterface<IDxcValidator, IDxcValidator2,
                                 IDxcVersionInfo>(this, iid, ppvObject);
  }

  HRESULT STDMETHODCALLTYPE Validate(
      IDxcBlob *pShader, // Shader to validate.
      UINT32 Flags,      // Validation flags.
      IDxcOperationResult *
          *ppResult // Validation output status, buffer, and errors
      ) override;

  // IDxcValidator2
  HRESULT STDMETHODCALLTYPE ValidateWithDebug(
      IDxcBlob *pShader,       // Shader to validate.
      UINT32 Flags,            // Validation flags.
      DxcBuffer *pDebugModule, // Debug module bitcode to provide line numbers
      IDxcOperationResult *
          *ppResult // Validation output status, buffer, and errors
      ) override;

  // IDxcVersionInfo
  HRESULT STDMETHODCALLTYPE GetVersion(UINT32 *pMajor, UINT32 *pMinor) override;
  HRESULT STDMETHODCALLTYPE GetFlags(UINT32 *pFlags) override;
};

// Compile a single entry point to the target shader model
HRESULT STDMETHODCALLTYPE DxcValidator::Validate(
    IDxcBlob *pShader, // Shader to validate.
    UINT32 Flags,      // Validation flags.
    IDxcOperationResult *
        *ppResult // Validation output status, buffer, and errors
) {
  return hlsl::validateWithDebug(pShader, Flags, nullptr, ppResult);
}

HRESULT STDMETHODCALLTYPE DxcValidator::ValidateWithDebug(
    IDxcBlob *pShader,           // Shader to validate.
    UINT32 Flags,                // Validation flags.
    DxcBuffer *pOptDebugBitcode, // Optional debug module bitcode to provide
                                 // line numbers
    IDxcOperationResult *
        *ppResult // Validation output status, buffer, and errors
) {
  return hlsl::validateWithDebug(pShader, Flags, pOptDebugBitcode, ppResult);
}

HRESULT STDMETHODCALLTYPE DxcValidator::GetVersion(UINT32 *pMajor,
                                                   UINT32 *pMinor) {
  return hlsl::getValidationVersion(pMajor, pMinor);
}

HRESULT STDMETHODCALLTYPE DxcValidator::GetFlags(UINT32 *pFlags) {
  DxcThreadMalloc TM(m_pMalloc);
  if (pFlags == nullptr)
    return E_INVALIDARG;
  *pFlags = DxcVersionInfoFlags_None;
#ifdef _DEBUG
  *pFlags |= DxcVersionInfoFlags_Debug;
#endif
  return S_OK;
}

HRESULT CreateDxcValidator(REFIID riid, LPVOID *ppv) {
  try {
    CComPtr<DxcValidator> result(
        DxcValidator::Alloc(DxcGetThreadMallocNoRef()));
    if (result.p == nullptr)
      return E_OUTOFMEMORY;
    return result.p->QueryInterface(riid, ppv);
  }
  CATCH_CPP_RETURN_HRESULT();
}
