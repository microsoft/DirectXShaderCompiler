///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxcvalidatorp.cpp                                                         //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Implements the DirectX Validator object, including signing support.       //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/IR/DiagnosticPrinter.h"
#include "llvm/IR/LLVMContext.h"

#include "dxc/DxilContainer/DxilContainer.h"
#include "dxc/HLSL/DxilValidation.h"
#include "dxc/Support/WinIncludes.h"
#include "dxc/Support/microcom.h"

#include "dxc/DxilRootSignature/DxilRootSignature.h"
#include "dxc/Support/FileIOHelper.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/dxcapi.impl.h"
#include "dxc/Support/microcom.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MSFileSystem.h"
#ifdef _WIN32
#include "dxc/Tracing/dxcetw.h"
#endif
#include "DxilHash.h"

using namespace llvm;
using namespace hlsl;

class DxcValidator : public IDxcValidator2, public IDxcVersionInfo {
private:
  DXC_MICROCOM_TM_REF_FIELDS()

  HRESULT RunValidation(_In_ IDxcBlob *pShader,
                        _In_ AbstractMemoryStream *pDiagStream,
                        _In_ UINT32 Flags, _In_opt_ DxcBuffer *pOptDebugBitcode,
                        _COM_Outptr_ IDxcBlob **pSigned);

  HRESULT
  RunRootSignatureValidation(_In_ IDxcBlob *pShader, // Shader to validate.
                             _In_ AbstractMemoryStream *pDiagStream,
                             _In_ UINT32 Flags,
                             _COM_Outptr_ IDxcBlob **pSigned);

public:
  DXC_MICROCOM_TM_ADDREF_RELEASE_IMPL()
  DXC_MICROCOM_TM_CTOR(DxcValidator)

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,
                                           void **ppvObject) override {

    return DoBasicQueryInterface<IDxcValidator, IDxcValidator2,
                                 IDxcVersionInfo>(this, iid, ppvObject);
  }

  HRESULT STDMETHODCALLTYPE Validate(
      _In_ IDxcBlob *pShader, // Shader to validate.
      _In_ UINT32 Flags,      // Validation flags.
      _COM_Outptr_ IDxcOperationResult *
          *ppResult // Validation output status, buffer, and errors
      ) override;

  // IDxcValidator2
  HRESULT STDMETHODCALLTYPE ValidateWithDebug(
      _In_ IDxcBlob *pShader, // Shader to validate.
      _In_ UINT32 Flags,      // Validation flags.
      _In_ DxcBuffer
          *pDebugModule, // Debug module bitcode to provide line numbers
      _COM_Outptr_ IDxcOperationResult *
          *ppResult // Validation output status, buffer, and errors
      ) override;

  // IDxcVersionInfo
  HRESULT STDMETHODCALLTYPE GetVersion(_Out_ UINT32 *pMajor,
                                       _Out_ UINT32 *pMinor) override;
  HRESULT STDMETHODCALLTYPE GetFlags(_Out_ UINT32 *pFlags) override;
};

HRESULT STDMETHODCALLTYPE DxcValidator::GetVersion(_Out_ UINT32 *pMajor,
                                                   _Out_ UINT32 *pMinor) {
  DxcThreadMalloc TM(m_pMalloc);
  if (pMajor == nullptr || pMinor == nullptr)
    return E_INVALIDARG;
  GetValidationVersion(pMajor, pMinor);
  return S_OK;
}

HRESULT STDMETHODCALLTYPE DxcValidator::GetFlags(_Out_ UINT32 *pFlags) {
  DxcThreadMalloc TM(m_pMalloc);
  if (pFlags == nullptr)
    return E_INVALIDARG;
  *pFlags = DxcVersionInfoFlags_None;
#ifdef _DEBUG
  *pFlags |= DxcVersionInfoFlags_Debug;
#endif
  return S_OK;
}

static void HashAndUpdate(DxilContainerHeader *pContainer) {
  // Compute hash and update stored hash.
  // Hash the container from this offset to the end.
  static const UINT32 DXBCHashStartOffset =
      offsetof(struct DxilContainerHeader, Version);
  const BYTE *pDataToHash = (const BYTE *)pContainer + DXBCHashStartOffset;
  UINT AmountToHash = pContainer->ContainerSizeInBytes - DXBCHashStartOffset;
  ComputeHashRetail(pDataToHash, AmountToHash, pContainer->Hash.Digest);
}

static void HashAndUpdateOrCopy(UINT32 Flags, IDxcBlob *pShader,
                                IDxcBlob **pSigned) {
  if (Flags & DxcValidatorFlags_InPlaceEdit) {
    HashAndUpdate((DxilContainerHeader *)pShader->GetBufferPointer());
    *pSigned = pShader;
    pShader->AddRef();
  } else {
    // Possible gotcha: the blob allocated here is tied to this .dll, so the
    // DLL shouldn't be unloaded before the blob is released.
    CComPtr<AbstractMemoryStream> pSignedBlobStream;
    IFT(CreateMemoryStream(DxcGetThreadMallocNoRef(), &pSignedBlobStream));
    ULONG cb;
    IFT(pSignedBlobStream->Write(pShader->GetBufferPointer(),
                                 pShader->GetBufferSize(), &cb));
    HashAndUpdate((DxilContainerHeader *)pSignedBlobStream->GetPtr());
    IFT(pSignedBlobStream.QueryInterface(pSigned));
  }
}

HRESULT STDMETHODCALLTYPE DxcValidator::ValidateWithDebug(
    _In_ IDxcBlob *pShader, // Shader to validate.
    _In_ UINT32 Flags,      // Validation flags.
    _In_opt_ DxcBuffer
        *pOptDebugBitcode, // Debug module bitcode to provide line numbers
    _COM_Outptr_ IDxcOperationResult *
        *ppResult // Validation output status, buffer, and errors
) {
  DxcThreadMalloc TM(m_pMalloc);
  if (pShader == nullptr || ppResult == nullptr ||
      Flags & ~DxcValidatorFlags_ValidMask)
    return E_INVALIDARG;
  if (Flags & DxcValidatorFlags_ModuleOnly)
    return E_INVALIDARG;
  if (pOptDebugBitcode &&
      (pOptDebugBitcode->Ptr == nullptr || pOptDebugBitcode->Size == 0 ||
       pOptDebugBitcode->Size >= UINT32_MAX))
    return E_INVALIDARG;

  *ppResult = nullptr;
  HRESULT hr = S_OK;
  HRESULT validationStatus = S_OK;
  DxcEtw_DxcValidation_Start();
  try {
    CComPtr<AbstractMemoryStream> pDiagStream;
    CComPtr<IDxcBlob> pSignedBlob;
    IFT(CreateMemoryStream(m_pMalloc, &pDiagStream));

    // Run validation may throw, but that indicates an inability to validate,
    // not that the validation failed (eg out of memory).
    if (Flags & DxcValidatorFlags_RootSignatureOnly) {
      validationStatus =
          RunRootSignatureValidation(pShader, pDiagStream, Flags, &pSignedBlob);
    } else {
      validationStatus = RunValidation(pShader, pDiagStream, Flags,
                                       pOptDebugBitcode, &pSignedBlob);
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
        validationStatus, DXC_OUT_OBJECT,
        {DxcOutputObject::DataOutput(DXC_OUT_OBJECT, pSignedBlob),
         DxcOutputObject::DataOutput(DXC_OUT_ERRORS, pDiagBlobEnconding)},
        ppResult));
  }
  CATCH_CPP_ASSIGN_HRESULT();

  DxcEtw_DxcValidation_Stop(SUCCEEDED(hr) ? validationStatus : hr);
  return hr;
}

HRESULT STDMETHODCALLTYPE DxcValidator::Validate(
    _In_ IDxcBlob *pShader, // Shader to validate.
    _In_ UINT32 Flags,      // Validation flags.
    _COM_Outptr_ IDxcOperationResult *
        *ppResult // Validation output status, buffer, and errors
) {
  return ValidateWithDebug(pShader, Flags, nullptr, ppResult);
}

HRESULT DxcValidator::RunValidation(_In_ IDxcBlob *pShader,
                                    _In_ AbstractMemoryStream *pDiagStream,
                                    _In_ UINT32 Flags,
                                    _In_ DxcBuffer *pDebugBitcode,
                                    _COM_Outptr_ IDxcBlob **pSigned) {

  // Run validation may throw, but that indicates an inability to validate,
  // not that the validation failed (eg out of memory). That is indicated
  // by a failing HRESULT, and possibly error messages in the diagnostics
  // stream.

  *pSigned = nullptr;
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

  HashAndUpdateOrCopy(Flags, pShader, pSigned);

  return S_OK;
}

HRESULT DxcValidator::RunRootSignatureValidation(
    _In_ IDxcBlob *pShader, _In_ AbstractMemoryStream *pDiagStream,
    _In_ UINT32 Flags, _COM_Outptr_ IDxcBlob **pSigned) {

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
      // Do not sign here; shaders must go through full shader validation for
      // signing.
    } else {
      IFRBOOL(VerifyRootSignature(RSH.GetDesc(), DiagStream, false),
              DXC_E_INCORRECT_ROOT_SIGNATURE);
      HashAndUpdateOrCopy(Flags, pShader, pSigned);
    }
  } catch (...) {
    return DXC_E_IR_VERIFICATION_FAILED;
  }

  return S_OK;
}

HRESULT CreateDxcValidator(_In_ REFIID riid, _Out_ LPVOID *ppv) {
  try {
    CComPtr<DxcValidator> result(
        DxcValidator::Alloc(DxcGetThreadMallocNoRef()));
    IFROOM(result.p);
    return result.p->QueryInterface(riid, ppv);
  }
  CATCH_CPP_RETURN_HRESULT();
}
