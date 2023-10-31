///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilValidator.cpp                                                         //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Implements the DirectX Validator object for testing dxil lib              //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////


#include "dxc/DxilContainer/DxilContainer.h"
#include "dxc/HLSL/DxilValidation.h"
#include "dxc/Support/WinIncludes.h"

#include "dxc/DxilRootSignature/DxilRootSignature.h"
#include "dxc/Support/FileIOHelper.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/dxcapi.impl.h"
#include "dxc/Support/microcom.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MSFileSystem.h"

#ifdef _WIN32
#include "dxcetw.h"
#endif

using namespace llvm;
using namespace hlsl;

class DxcValidator : public IDxcValidator2, public IDxcVersionInfo {
private:
  DXC_MICROCOM_TM_REF_FIELDS()

  HRESULT RunValidation(IDxcBlob *pShader, AbstractMemoryStream *pDiagStream,
                        DxcBuffer *pOptDebugBitcode);

  HRESULT RunRootSignatureValidation(IDxcBlob *pShader, // Shader to validate.
                                     AbstractMemoryStream *pDiagStream);

public:
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()
  DXC_MICROCOM_TM_CTOR(DxcValidator)

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,
                                           void **ppvObject) override {

    return DoBasicQueryInterface<IDxcValidator, IDxcValidator2,
                                 IDxcVersionInfo>(this, iid, ppvObject);
  }

  // IDxcValidator
  HRESULT STDMETHODCALLTYPE Validate(
      IDxcBlob *pShader, // Shader to validate.
      UINT32 Flags,      // Validation flags.
      IDxcOperationResult *
          *ppResult // Validation output status, buffer, and errors
      ) override;

  // IDxcValidator2
  HRESULT STDMETHODCALLTYPE ValidateWithDebug(
      IDxcBlob *pShader,           // Shader to validate.
      UINT32 Flags,                // Validation flags.
      DxcBuffer *pOptDebugBitcode, // Optional debug module bitcode to provide
                                   // line numbers
      IDxcOperationResult *
          *ppResult // Validation output status, buffer, and errors
      ) override;

  // IDxcVersionInfo
  HRESULT STDMETHODCALLTYPE GetVersion(UINT32 *pMajor, UINT32 *pMinor) override;
  HRESULT STDMETHODCALLTYPE GetFlags(UINT32 *pFlags) override;
};

HRESULT STDMETHODCALLTYPE DxcValidator::GetVersion(UINT32 *pMajor,
                                                   UINT32 *pMinor) {
  if (pMajor == nullptr || pMinor == nullptr)
    return E_INVALIDARG;
  GetValidationVersion(pMajor, pMinor);
  return S_OK;
}

HRESULT STDMETHODCALLTYPE DxcValidator::GetFlags(UINT32 *pFlags) {
  if (pFlags == nullptr)
    return E_INVALIDARG;
  *pFlags = DxcVersionInfoFlags_None;
#ifndef NDEBUG
  *pFlags |= DxcVersionInfoFlags_Debug;
#endif
  return S_OK;
}

HRESULT STDMETHODCALLTYPE DxcValidator::ValidateWithDebug(
    IDxcBlob *pShader,           // Shader to validate.
    UINT32 Flags,                // Validation flags.
    DxcBuffer *pOptDebugBitcode, // Optional debug module bitcode to provide
                                 // line numbers
    IDxcOperationResult *
        *ppResult // Validation output status, buffer, and errors
) {
  if (ppResult == nullptr)
    return E_INVALIDARG;
  *ppResult = nullptr;
  if (pShader == nullptr || Flags & ~DxcValidatorFlags_ValidMask)
    return E_INVALIDARG;
  if ((Flags & DxcValidatorFlags_ModuleOnly) &&
      (Flags &
       (DxcValidatorFlags_InPlaceEdit | DxcValidatorFlags_RootSignatureOnly)))
    return E_INVALIDARG;
  if (pOptDebugBitcode &&
      (pOptDebugBitcode->Ptr == nullptr || pOptDebugBitcode->Size == 0 ||
       pOptDebugBitcode->Size >= UINT32_MAX))
    return E_INVALIDARG;

  *ppResult = nullptr;
  HRESULT hr = S_OK;
  HRESULT validationStatus = S_OK;
  DxcEtw_DxcValidation_Start();
  DxcThreadMalloc TM(m_pMalloc);
  try {
    CComPtr<AbstractMemoryStream> pDiagStream;
    IFT(CreateMemoryStream(m_pMalloc, &pDiagStream));

    // Run validation may throw, but that indicates an inability to validate,
    // not that the validation failed (eg out of memory).
    if (Flags & DxcValidatorFlags_RootSignatureOnly) {
      validationStatus = RunRootSignatureValidation(pShader, pDiagStream);
    } else {
      validationStatus = RunValidation(pShader, pDiagStream, pOptDebugBitcode);
    }
    if (FAILED(validationStatus)) {
      std::string msg("Validation failed.\n");
      ULONG cbWritten;
      pDiagStream->Write(msg.c_str(), msg.size(), &cbWritten);
    }
    // Assemble the result object.
    CComPtr<IDxcBlob> pDiagBlob;
    CComPtr<IDxcBlobEncoding> pDiagBlobEnconding;
    hr = pDiagStream.QueryInterface(&pDiagBlob);
    DXASSERT_NOMSG(SUCCEEDED(hr));
    IFT(DxcCreateBlobWithEncodingSet(pDiagBlob, CP_UTF8, &pDiagBlobEnconding));
    IFT(DxcResult::Create(
        validationStatus, DXC_OUT_NONE,
        {DxcOutputObject::DataOutput(DXC_OUT_ERRORS, pDiagBlobEnconding)},
        ppResult));
  }
  CATCH_CPP_ASSIGN_HRESULT();

  DxcEtw_DxcValidation_Stop(SUCCEEDED(hr) ? validationStatus : hr);
  return hr;
}

HRESULT STDMETHODCALLTYPE DxcValidator::Validate(
    IDxcBlob *pShader, // Shader to validate.
    UINT32 Flags,      // Validation flags.
    IDxcOperationResult *
        *ppResult // Validation output status, buffer, and errors
) {
  return ValidateWithDebug(pShader, Flags, nullptr, ppResult);
}

HRESULT DxcValidator::RunValidation(IDxcBlob *pShader,
                                    AbstractMemoryStream *pDiagStream,
                                    DxcBuffer *pDebugBitcode) {

  // Run validation may throw, but that indicates an inability to validate,
  // not that the validation failed (eg out of memory). That is indicated
  // by a failing HRESULT, and possibly error messages in the diagnostics
  // stream.

  raw_stream_ostream DiagStream(pDiagStream);

  if (IsDxilContainerLike(pShader->GetBufferPointer(),
                          pShader->GetBufferSize())) {
    IFR(ValidateDxilContainer(
        pShader->GetBufferPointer(), pShader->GetBufferSize(),
        pDebugBitcode ? pDebugBitcode->Ptr : nullptr,
        pDebugBitcode ? (uint32_t)pDebugBitcode->Size : 0, DiagStream));
  } else {
    IFR(DXC_E_CONTAINER_INVALID);
  }

  return S_OK;
}

HRESULT
DxcValidator::RunRootSignatureValidation(IDxcBlob *pShader,
                                         AbstractMemoryStream *pDiagStream) {

  const DxilContainerHeader *pDxilContainer = IsDxilContainerLike(
      pShader->GetBufferPointer(), pShader->GetBufferSize());
  if (!pDxilContainer) {
    return DXC_E_IR_VERIFICATION_FAILED;
  }

  const DxilProgramHeader *pProgramHeader =
      GetDxilProgramHeader(pDxilContainer, DFCC_DXIL);
  const DxilPartHeader *pPSVPart =
      GetDxilPartByType(pDxilContainer, DFCC_PipelineStateValidation);
  const DxilPartHeader *pRSPart =
      GetDxilPartByType(pDxilContainer, DFCC_RootSignature);
  IFRBOOL(pRSPart, DXC_E_MISSING_PART);
  if (pProgramHeader) {
    // Container has shader part, make sure we have PSV.
    IFRBOOL(pPSVPart, DXC_E_MISSING_PART);
  }
  try {
    RootSignatureHandle RSH;
    RSH.LoadSerialized((const uint8_t *)GetDxilPartData(pRSPart),
                       pRSPart->PartSize);
    RSH.Deserialize();
    raw_stream_ostream DiagStream(pDiagStream);
    if (pProgramHeader) {
      IFRBOOL(VerifyRootSignatureWithShaderPSV(
                  RSH.GetDesc(),
                  GetVersionShaderType(pProgramHeader->ProgramVersion),
                  GetDxilPartData(pPSVPart), pPSVPart->PartSize, DiagStream),
              DXC_E_INCORRECT_ROOT_SIGNATURE);
    } else {
      IFRBOOL(VerifyRootSignature(RSH.GetDesc(), DiagStream, false),
              DXC_E_INCORRECT_ROOT_SIGNATURE);
    }
  } catch (...) {
    return DXC_E_IR_VERIFICATION_FAILED;
  }

  return S_OK;
}

HRESULT CreateDxcValidator(REFIID riid, LPVOID *ppv) {
  try {
    CComPtr<DxcValidator> result(
        DxcValidator::Alloc(DxcGetThreadMallocNoRef()));
    IFROOM(result.p);
    return result.p->QueryInterface(riid, ppv);
  }
  CATCH_CPP_RETURN_HRESULT();
}
