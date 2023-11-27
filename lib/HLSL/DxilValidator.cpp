///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilValidator.cpp                                                         //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Implements the DirectX Validator object.                                  //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/IR/DiagnosticPrinter.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"

#include "dxc/DxilContainer/DxilContainer.h"
#include "dxc/HLSL/DxilValidation.h"
#include "dxc/HLSL/DxilValidator.h"
#include "dxc/Support/WinIncludes.h"

#include "dxc/DxilRootSignature/DxilRootSignature.h"
#include "dxc/Support/FileIOHelper.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/dxcapi.impl.h"
#include "dxc/dxcapi.h"
#include "llvm/Support/MemoryBuffer.h"

#ifdef _WIN32
#include "dxcetw.h"
#endif

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

// Compile a single entry point to the target shader model
HRESULT STDMETHODCALLTYPE DxilValidator::Validate(
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

HRESULT STDMETHODCALLTYPE DxilValidator::ValidateWithDebug(
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

HRESULT DxilValidator::ValidateWithOptModules(
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

HRESULT DxilValidator::RunValidation(
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
DxilValidator::RunRootSignatureValidation(IDxcBlob *pShader,
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

  DxilValidator *pInternalValidator = (DxilValidator *)pValidator;
  return pInternalValidator->ValidateWithOptModules(pShader, Flags, pModule,
                                                    pDebugModule, ppResult);
}
