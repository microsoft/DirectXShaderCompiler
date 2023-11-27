///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxcvalidator.cpp                                                          //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Implements the DirectX Validator object.                                  //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/IR/DiagnosticPrinter.h"
#include "llvm/IR/LLVMContext.h"

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
#include "llvm/Support/MemoryBuffer.h"

#ifdef _WIN32
#include "dxcetw.h"
#endif

#ifdef SUPPORT_QUERY_GIT_COMMIT_INFO
#include "clang/Basic/Version.h"
#endif // SUPPORT_QUERY_GIT_COMMIT_INFO

using namespace llvm;
using namespace hlsl;

// Utility class for setting and restoring the diagnostic context so we may
// capture errors/warnings
struct DiagRestore {
  LLVMContext &Ctx;
  void *OrigDiagContext;
  LLVMContext::DiagnosticHandlerTy OrigHandler;

  DiagRestore(llvm::LLVMContext &Ctx, void *DiagContext) : Ctx(Ctx) {
    OrigHandler = Ctx.getDiagnosticHandler();
    OrigDiagContext = Ctx.getDiagnosticContext();
    Ctx.setDiagnosticHandler(PrintDiagnosticContext::PrintDiagnosticHandler,
                             DiagContext);
  }
  ~DiagRestore() { Ctx.setDiagnosticHandler(OrigHandler, OrigDiagContext); }
};

class DxcValidator : public IDxcValidator2,
#ifdef SUPPORT_QUERY_GIT_COMMIT_INFO
                     public IDxcVersionInfo2
#else
                     public IDxcVersionInfo
#endif // SUPPORT_QUERY_GIT_COMMIT_INFO
{
private:
  DXC_MICROCOM_TM_REF_FIELDS()

  HRESULT RunValidation(
      IDxcBlob *pShader,          // Shader to validate.
      UINT32 Flags,               // Validation flags.
      llvm::Module *pModule,      // Module to validate, if available.
      llvm::Module *pDebugModule, // Debug module to validate, if available
      AbstractMemoryStream *pDiagStream);

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

  // For internal use only.
  HRESULT ValidateWithOptModules(
      IDxcBlob *pShader,          // Shader to validate.
      UINT32 Flags,               // Validation flags.
      llvm::Module *pModule,      // Module to validate, if available.
      llvm::Module *pDebugModule, // Debug module to validate, if available
      IDxcOperationResult *
          *ppResult // Validation output status, buffer, and errors
  );

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

#ifdef SUPPORT_QUERY_GIT_COMMIT_INFO
  // IDxcVersionInfo2
  HRESULT STDMETHODCALLTYPE GetCommitInfo(UINT32 *pCommitCount,
                                          char **pCommitHash) override;
#endif
};

// Compile a single entry point to the target shader model
HRESULT STDMETHODCALLTYPE DxcValidator::Validate(
    IDxcBlob *pShader, // Shader to validate.
    UINT32 Flags,      // Validation flags.
    IDxcOperationResult *
        *ppResult // Validation output status, buffer, and errors
) {
  DxcThreadMalloc TM(m_pMalloc);
  if (ppResult == nullptr)
    return E_INVALIDARG;
  *ppResult = nullptr;
  if (pShader == nullptr || Flags & ~DxcValidatorFlags_ValidMask)
    return E_INVALIDARG;
  if ((Flags & DxcValidatorFlags_ModuleOnly) &&
      (Flags &
       (DxcValidatorFlags_InPlaceEdit | DxcValidatorFlags_RootSignatureOnly)))
    return E_INVALIDARG;
  return ValidateWithOptModules(pShader, Flags, nullptr, nullptr, ppResult);
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

  HRESULT hr = S_OK;
  DxcThreadMalloc TM(m_pMalloc);
  try {
    LLVMContext Ctx;
    CComPtr<AbstractMemoryStream> pDiagStream;
    IFT(CreateMemoryStream(m_pMalloc, &pDiagStream));
    raw_stream_ostream DiagStream(pDiagStream);
    llvm::DiagnosticPrinterRawOStream DiagPrinter(DiagStream);
    PrintDiagnosticContext DiagContext(DiagPrinter);
    Ctx.setDiagnosticHandler(PrintDiagnosticContext::PrintDiagnosticHandler,
                             &DiagContext, true);
    std::unique_ptr<llvm::Module> pDebugModule;
    if (pOptDebugBitcode) {
      IFT(ValidateLoadModule((const char *)pOptDebugBitcode->Ptr,
                             (uint32_t)pOptDebugBitcode->Size, pDebugModule,
                             Ctx, DiagStream, /*bLazyLoad*/ false));
    }
    return ValidateWithOptModules(pShader, Flags, nullptr, pDebugModule.get(),
                                  ppResult);
  }
  CATCH_CPP_ASSIGN_HRESULT();
  return hr;
}

HRESULT DxcValidator::ValidateWithOptModules(
    IDxcBlob *pShader,          // Shader to validate.
    UINT32 Flags,               // Validation flags.
    llvm::Module *pModule,      // Module to validate, if available.
    llvm::Module *pDebugModule, // Debug module to validate, if available
    IDxcOperationResult *
        *ppResult // Validation output status, buffer, and errors
) {
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
      validationStatus =
          RunValidation(pShader, Flags, pModule, pDebugModule, pDiagStream);
    }
    if (FAILED(validationStatus)) {
      std::string msg("Validation failed.\n");
      ULONG cbWritten;
      pDiagStream->Write(msg.c_str(), msg.size(), &cbWritten);
    }
    // Assemble the result object.
    CComPtr<IDxcBlob> pDiagBlob;
    hr = pDiagStream.QueryInterface(&pDiagBlob);
    DXASSERT_NOMSG(SUCCEEDED(hr));
    IFT(DxcResult::Create(
        validationStatus, DXC_OUT_NONE,
        {DxcOutputObject::ErrorOutput(
            CP_UTF8, // TODO Support DefaultTextCodePage
            (LPCSTR)pDiagBlob->GetBufferPointer(), pDiagBlob->GetBufferSize())},
        ppResult));
  }
  CATCH_CPP_ASSIGN_HRESULT();

  DxcEtw_DxcValidation_Stop(SUCCEEDED(hr) ? validationStatus : hr);
  return hr;
}

HRESULT STDMETHODCALLTYPE DxcValidator::GetVersion(UINT32 *pMajor,
                                                   UINT32 *pMinor) {
  if (pMajor == nullptr || pMinor == nullptr)
    return E_INVALIDARG;
  GetValidationVersion(pMajor, pMinor);
  return S_OK;
}

#ifdef SUPPORT_QUERY_GIT_COMMIT_INFO
HRESULT STDMETHODCALLTYPE DxcValidator::GetCommitInfo(UINT32 *pCommitCount,
                                                      char **pCommitHash) {
  if (pCommitCount == nullptr || pCommitHash == nullptr)
    return E_INVALIDARG;

  char *const hash = (char *)CoTaskMemAlloc(
      8 + 1); // 8 is guaranteed by utils/GetCommitInfo.py
  if (hash == nullptr)
    return E_OUTOFMEMORY;
  std::strcpy(hash, clang::getGitCommitHash());

  *pCommitHash = hash;
  *pCommitCount = clang::getGitCommitCount();

  return S_OK;
}
#endif // SUPPORT_QUERY_GIT_COMMIT_INFO

HRESULT STDMETHODCALLTYPE DxcValidator::GetFlags(UINT32 *pFlags) {
  if (pFlags == nullptr)
    return E_INVALIDARG;
  *pFlags = DxcVersionInfoFlags_None;
#ifndef NDEBUG
  *pFlags |= DxcVersionInfoFlags_Debug;
#endif
  *pFlags |= DxcVersionInfoFlags_Internal;
  return S_OK;
}

HRESULT DxcValidator::RunValidation(
    IDxcBlob *pShader,
    UINT32 Flags,               // Validation flags.
    llvm::Module *pModule,      // Module to validate, if available.
    llvm::Module *pDebugModule, // Debug module to validate, if available
    AbstractMemoryStream *pDiagStream) {

  // Run validation may throw, but that indicates an inability to validate,
  // not that the validation failed (eg out of memory). That is indicated
  // by a failing HRESULT, and possibly error messages in the diagnostics
  // stream.

  raw_stream_ostream DiagStream(pDiagStream);

  if (Flags & DxcValidatorFlags_ModuleOnly) {
    IFRBOOL(!IsDxilContainerLike(pShader->GetBufferPointer(),
                                 pShader->GetBufferSize()),
            E_INVALIDARG);
  } else {
    IFRBOOL(IsDxilContainerLike(pShader->GetBufferPointer(),
                                pShader->GetBufferSize()),
            DXC_E_CONTAINER_INVALID);
  }

  if (!pModule) {
    DXASSERT_NOMSG(pDebugModule == nullptr);
    if (Flags & DxcValidatorFlags_ModuleOnly) {
      return ValidateDxilBitcode((const char *)pShader->GetBufferPointer(),
                                 (uint32_t)pShader->GetBufferSize(),
                                 DiagStream);
    } else {
      return ValidateDxilContainer(pShader->GetBufferPointer(),
                                   pShader->GetBufferSize(), DiagStream);
    }
  }

  llvm::DiagnosticPrinterRawOStream DiagPrinter(DiagStream);
  PrintDiagnosticContext DiagContext(DiagPrinter);
  DiagRestore DR(pModule->getContext(), &DiagContext);

  IFR(hlsl::ValidateDxilModule(pModule, pDebugModule));
  if (!(Flags & DxcValidatorFlags_ModuleOnly)) {
    IFR(ValidateDxilContainerParts(
        pModule, pDebugModule,
        IsDxilContainerLike(pShader->GetBufferPointer(),
                            pShader->GetBufferSize()),
        (uint32_t)pShader->GetBufferSize()));
  }

  if (DiagContext.HasErrors() || DiagContext.HasWarnings()) {
    return DXC_E_IR_VERIFICATION_FAILED;
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

///////////////////////////////////////////////////////////////////////////////

HRESULT RunInternalValidator(IDxcValidator *pValidator, llvm::Module *pModule,
                             llvm::Module *pDebugModule, IDxcBlob *pShader,
                             UINT32 Flags, IDxcOperationResult **ppResult) {
  DXASSERT_NOMSG(pValidator != nullptr);
  DXASSERT_NOMSG(pModule != nullptr);
  DXASSERT_NOMSG(pShader != nullptr);
  DXASSERT_NOMSG(ppResult != nullptr);

  DxcValidator *pInternalValidator = (DxcValidator *)pValidator;
  return pInternalValidator->ValidateWithOptModules(pShader, Flags, pModule,
                                                    pDebugModule, ppResult);
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
