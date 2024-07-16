///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxcvalidatorp.cpp                                                         //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Implements the DirectX Validator object, including hashing support.       //
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

  HRESULT RunValidation(IDxcBlob *pShader, AbstractMemoryStream *pDiagStream,
                        UINT32 Flags, DxcBuffer *pOptDebugBitcode,
                        IDxcBlob **Hashed);

  HRESULT
  RunRootSignatureValidation(IDxcBlob *pShader, // Shader to validate.
                             AbstractMemoryStream *pDiagStream, UINT32 Flags,
                             IDxcBlob **Hashed);

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

HRESULT STDMETHODCALLTYPE DxcValidator::GetVersion(UINT32 *pMajor,
                                                   UINT32 *pMinor) {
  DxcThreadMalloc TM(m_pMalloc);
  if (pMajor == nullptr || pMinor == nullptr)
    return E_INVALIDARG;
  GetValidationVersion(pMajor, pMinor);
  return S_OK;
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
                                IDxcBlob **Hashed) {
  if (Flags & DxcValidatorFlags_InPlaceEdit) {
    HashAndUpdate((DxilContainerHeader *)pShader->GetBufferPointer());
    *Hashed = pShader;
    pShader->AddRef();
  } else {
    // Possible gotcha: the blob allocated here is tied to this .dll, so the
    // DLL shouldn't be unloaded before the blob is released.
    CComPtr<AbstractMemoryStream> HashedBlobStream;
    IFT(CreateMemoryStream(DxcGetThreadMallocNoRef(), &HashedBlobStream));
    ULONG cb;
    IFT(HashedBlobStream->Write(pShader->GetBufferPointer(),
                                pShader->GetBufferSize(), &cb));
    HashAndUpdate((DxilContainerHeader *)HashedBlobStream->GetPtr());
    IFT(HashedBlobStream.QueryInterface(Hashed));
  }
}

HRESULT STDMETHODCALLTYPE DxcValidator::ValidateWithDebug(
    IDxcBlob *pShader,           // Shader to validate.
    UINT32 Flags,                // Validation flags.
    DxcBuffer *pOptDebugBitcode, // Debug module bitcode to provide line numbers
    IDxcOperationResult *
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
    CComPtr<IDxcBlob> HashedBlob;
    IFT(CreateMemoryStream(m_pMalloc, &pDiagStream));

    // Run validation may throw, but that indicates an inability to validate,
    // not that the validation failed (eg out of memory).
    if (Flags & DxcValidatorFlags_RootSignatureOnly) {
      validationStatus =
          RunRootSignatureValidation(pShader, pDiagStream, Flags, &HashedBlob);
    } else {
      validationStatus = RunValidation(pShader, pDiagStream, Flags,
                                       pOptDebugBitcode, &HashedBlob);
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
        {DxcOutputObject::DataOutput(DXC_OUT_OBJECT, HashedBlob),
         DxcOutputObject::DataOutput(DXC_OUT_ERRORS, pDiagBlobEnconding)},
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
                                    UINT32 Flags, DxcBuffer *pDebugBitcode,
                                    IDxcBlob **Hashed) {

  // Run validation may throw, but that indicates an inability to validate,
  // not that the validation failed (eg out of memory). That is indicated
  // by a failing HRESULT, and possibly error messages in the diagnostics
  // stream.

  *Hashed = nullptr;
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

  HashAndUpdateOrCopy(Flags, pShader, Hashed);

  return S_OK;
}

HRESULT
DxcValidator::RunRootSignatureValidation(IDxcBlob *pShader,
                                         AbstractMemoryStream *pDiagStream,
                                         UINT32 Flags, IDxcBlob **Hashed) {

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
      // Do not hash here; shaders must go through full shader validation for
      // hashing.
    } else {
      IFRBOOL(VerifyRootSignature(RSH.GetDesc(), DiagStream, false),
              DXC_E_INCORRECT_ROOT_SIGNATURE);
      HashAndUpdateOrCopy(Flags, pShader, Hashed);
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
