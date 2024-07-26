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
#include "dxc/dxcapi.h"
#include "dxcvalidator.h"

#include "dxc/DxilRootSignature/DxilRootSignature.h"
#include "dxc/Support/FileIOHelper.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/dxcapi.impl.h"

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

static uint32_t runValidation(
    IDxcBlob *Shader,
    uint32_t Flags,            // Validation flags.
    llvm::Module *Module,      // Module to validate, if available.
    llvm::Module *DebugModule, // Debug module to validate, if available
    AbstractMemoryStream *DiagMemStream) {

  // Run validation may throw, but that indicates an inability to validate,
  // not that the validation failed (eg out of memory). That is indicated
  // by a failing HRESULT, and possibly error messages in the diagnostics
  // stream.

  raw_stream_ostream DiagStream(DiagMemStream);

  if (Flags & DxcValidatorFlags_ModuleOnly) {
    if (IsDxilContainerLike(Shader->GetBufferPointer(),
                            Shader->GetBufferSize()))
      return E_INVALIDARG;
  } else {
    if (!IsDxilContainerLike(Shader->GetBufferPointer(),
                             Shader->GetBufferSize()))
      return DXC_E_CONTAINER_INVALID;
  }

  if (!Module) {
    DXASSERT_NOMSG(DebugModule == nullptr);
    if (Flags & DxcValidatorFlags_ModuleOnly) {
      return ValidateDxilBitcode((const char *)Shader->GetBufferPointer(),
                                 (uint32_t)Shader->GetBufferSize(), DiagStream);
    } else {
      return ValidateDxilContainer(Shader->GetBufferPointer(),
                                   Shader->GetBufferSize(), DiagStream);
    }
  }

  llvm::DiagnosticPrinterRawOStream DiagPrinter(DiagStream);
  PrintDiagnosticContext DiagContext(DiagPrinter);
  DiagRestore DR(Module->getContext(), &DiagContext);

  HRESULT hr = hlsl::ValidateDxilModule(Module, DebugModule);
  if (FAILED(hr))
    return hr;
  if (!(Flags & DxcValidatorFlags_ModuleOnly)) {
    hr = ValidateDxilContainerParts(
        Module, DebugModule,
        IsDxilContainerLike(Shader->GetBufferPointer(),
                            Shader->GetBufferSize()),
        (uint32_t)Shader->GetBufferSize());
    if (FAILED(hr))
      return hr;
  }

  if (DiagContext.HasErrors() || DiagContext.HasWarnings()) {
    return DXC_E_IR_VERIFICATION_FAILED;
  }

  return S_OK;
}

static uint32_t
runRootSignatureValidation(IDxcBlob *Shader,
                           AbstractMemoryStream *DiagMemStream) {

  const DxilContainerHeader *DxilContainer =
      IsDxilContainerLike(Shader->GetBufferPointer(), Shader->GetBufferSize());
  if (!DxilContainer)
    return DXC_E_IR_VERIFICATION_FAILED;

  const DxilProgramHeader *ProgramHeader =
      GetDxilProgramHeader(DxilContainer, DFCC_DXIL);
  const DxilPartHeader *PSVPart =
      GetDxilPartByType(DxilContainer, DFCC_PipelineStateValidation);
  const DxilPartHeader *RSPart =
      GetDxilPartByType(DxilContainer, DFCC_RootSignature);
  if (!RSPart)
    return DXC_E_MISSING_PART;
  if (ProgramHeader) {
    // Container has shader part, make sure we have PSV.
    if (!PSVPart)
      return DXC_E_MISSING_PART;
  }
  try {
    RootSignatureHandle RSH;
    RSH.LoadSerialized((const uint8_t *)GetDxilPartData(RSPart),
                       RSPart->PartSize);
    RSH.Deserialize();
    raw_stream_ostream DiagStream(DiagMemStream);
    if (ProgramHeader) {
      if (!VerifyRootSignatureWithShaderPSV(
              RSH.GetDesc(),
              GetVersionShaderType(ProgramHeader->ProgramVersion),
              GetDxilPartData(PSVPart), PSVPart->PartSize, DiagStream))
        return DXC_E_INCORRECT_ROOT_SIGNATURE;
    } else {
      if (!VerifyRootSignature(RSH.GetDesc(), DiagStream, false))
        return DXC_E_INCORRECT_ROOT_SIGNATURE;
    }
  } catch (...) {
    return DXC_E_IR_VERIFICATION_FAILED;
  }

  return S_OK;
}

// Compile a single entry point to the target shader model
uint32_t hlsl::validate(
    IDxcBlob *Shader,            // Shader to validate.
    uint32_t Flags,              // Validation flags.
    IDxcOperationResult **Result // Validation output status, buffer, and errors
) {
  DxcThreadMalloc TM(DxcGetThreadMallocNoRef());
  if (Result == nullptr)
    return false;
  *Result = nullptr;
  if (Shader == nullptr || Flags & ~DxcValidatorFlags_ValidMask)
    return false;
  if ((Flags & DxcValidatorFlags_ModuleOnly) &&
      (Flags &
       (DxcValidatorFlags_InPlaceEdit | DxcValidatorFlags_RootSignatureOnly)))
    return false;
  return validateWithOptModules(Shader, Flags, nullptr, nullptr, Result);
}

uint32_t hlsl::validateWithDebug(
    IDxcBlob *Shader,            // Shader to validate.
    uint32_t Flags,              // Validation flags.
    DxcBuffer *OptDebugBitcode,  // Optional debug module bitcode to provide
                                 // line numbers
    IDxcOperationResult **Result // Validation output status, buffer, and errors
) {
  if (Result == nullptr)
    return E_INVALIDARG;
  *Result = nullptr;
  if (Shader == nullptr || Flags & ~DxcValidatorFlags_ValidMask)
    return E_INVALIDARG;
  if ((Flags & DxcValidatorFlags_ModuleOnly) &&
      (Flags &
       (DxcValidatorFlags_InPlaceEdit | DxcValidatorFlags_RootSignatureOnly)))
    return E_INVALIDARG;
  if (OptDebugBitcode &&
      (OptDebugBitcode->Ptr == nullptr || OptDebugBitcode->Size == 0 ||
       OptDebugBitcode->Size >= UINT32_MAX))
    return E_INVALIDARG;

  HRESULT hr = S_OK;
  DxcThreadMalloc TM(DxcGetThreadMallocNoRef());
  try {
    LLVMContext Ctx;
    CComPtr<AbstractMemoryStream> DiagMemStream;
    hr = CreateMemoryStream(TM.GetInstalledAllocator(), &DiagMemStream);
    if (FAILED(hr))
      throw hlsl::Exception(hr);
    raw_stream_ostream DiagStream(DiagMemStream);
    llvm::DiagnosticPrinterRawOStream DiagPrinter(DiagStream);
    PrintDiagnosticContext DiagContext(DiagPrinter);
    Ctx.setDiagnosticHandler(PrintDiagnosticContext::PrintDiagnosticHandler,
                             &DiagContext, true);
    std::unique_ptr<llvm::Module> DebugModule;
    if (OptDebugBitcode) {
      hr = ValidateLoadModule((const char *)OptDebugBitcode->Ptr,
                              (uint32_t)OptDebugBitcode->Size, DebugModule, Ctx,
                              DiagStream, /*bLazyLoad*/ false);
      if (FAILED(hr))
        throw hlsl::Exception(hr);
    }
    return validateWithOptModules(Shader, Flags, nullptr, DebugModule.get(),
                                  Result);
  }
  CATCH_CPP_ASSIGN_HRESULT();
  return hr;
}

uint32_t hlsl::validateWithOptModules(
    IDxcBlob *Shader,            // Shader to validate.
    uint32_t Flags,              // Validation flags.
    llvm::Module *Module,        // Module to validate, if available.
    llvm::Module *DebugModule,   // Debug module to validate, if available
    IDxcOperationResult **Result // Validation output status, buffer, and errors
) {
  *Result = nullptr;
  HRESULT hr = S_OK;
  HRESULT validationStatus = S_OK;
  DxcEtw_DxcValidation_Start();
  DxcThreadMalloc TM(DxcGetThreadMallocNoRef());
  try {
    CComPtr<AbstractMemoryStream> DiagStream;
    hr = CreateMemoryStream(TM.GetInstalledAllocator(), &DiagStream);
    if (FAILED(hr))
      throw hlsl::Exception(hr);
    // Run validation may throw, but that indicates an inability to validate,
    // not that the validation failed (eg out of memory).
    if (Flags & DxcValidatorFlags_RootSignatureOnly) {
      validationStatus = runRootSignatureValidation(Shader, DiagStream);
    } else {
      validationStatus =
          runValidation(Shader, Flags, Module, DebugModule, DiagStream);
    }
    if (FAILED(validationStatus)) {
      std::string msg("Validation failed.\n");
      ULONG cbWritten;
      DiagStream->Write(msg.c_str(), msg.size(), &cbWritten);
    }
    // Assemble the result object.
    CComPtr<IDxcBlob> pDiagBlob;
    hr = DiagStream.QueryInterface(&pDiagBlob);
    DXASSERT_NOMSG(SUCCEEDED(hr));
    hr = DxcResult::Create(
        validationStatus, DXC_OUT_NONE,
        {DxcOutputObject::ErrorOutput(
            CP_UTF8, // TODO Support DefaultTextCodePage
            (LPCSTR)pDiagBlob->GetBufferPointer(), pDiagBlob->GetBufferSize())},
        Result);
    if (FAILED(hr))
      throw hlsl::Exception(hr);
  }
  CATCH_CPP_ASSIGN_HRESULT();

  DxcEtw_DxcValidation_Stop(SUCCEEDED(hr) ? validationStatus : hr);
  return hr;
}

uint32_t hlsl::getValidationVersion(unsigned *Major, unsigned *Minor) {
  if (Major == nullptr || Minor == nullptr)
    return E_INVALIDARG;
  hlsl::GetValidationVersion(Major, Minor);
  return S_OK;
}
