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

#include "DxilHash.h"

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

static void HashAndUpdate(DxilContainerHeader *Container) {
  // Compute hash and update stored hash.
  // Hash the container from this offset to the end.
  static const uint32_t DXBCHashStartOffset =
      offsetof(struct DxilContainerHeader, Version);
  const unsigned char *DataToHash =
      (const unsigned char *)Container + DXBCHashStartOffset;
  unsigned AmountToHash = Container->ContainerSizeInBytes - DXBCHashStartOffset;
  ComputeHashRetail(DataToHash, AmountToHash, Container->Hash.Digest);
}

static void HashAndUpdateOrCopy(uint32_t Flags, IDxcBlob *Shader,
                                IDxcBlob **Hashed) {
  if (Flags & DxcValidatorFlags_InPlaceEdit) {
    HashAndUpdate((DxilContainerHeader *)Shader->GetBufferPointer());
    *Hashed = Shader;
    Shader->AddRef();
  } else {
    CComPtr<AbstractMemoryStream> HashedBlobStream;
    uint32_t HR =
        CreateMemoryStream(DxcGetThreadMallocNoRef(), &HashedBlobStream);
    if (FAILED(HR))
      throw hlsl::Exception(HR);

    unsigned long CB;
    HR = HashedBlobStream->Write(Shader->GetBufferPointer(),
                                 Shader->GetBufferSize(), &CB);
    if (FAILED(HR))
      throw hlsl::Exception(HR);
    HashAndUpdate((DxilContainerHeader *)HashedBlobStream->GetPtr());
    HR = HashedBlobStream.QueryInterface(Hashed);
    if (FAILED(HR))
      throw hlsl::Exception(HR);
  }
}

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
    if (Flags & DxcValidatorFlags_ModuleOnly)
      return ValidateDxilBitcode((const char *)Shader->GetBufferPointer(),
                                 (uint32_t)Shader->GetBufferSize(), DiagStream);
    else
      return ValidateDxilContainer(Shader->GetBufferPointer(),
                                   Shader->GetBufferSize(), DiagStream);
  }

  llvm::DiagnosticPrinterRawOStream DiagPrinter(DiagStream);
  PrintDiagnosticContext DiagContext(DiagPrinter);
  DiagRestore DR(Module->getContext(), &DiagContext);

  HRESULT HR = hlsl::ValidateDxilModule(Module, DebugModule);
  if (FAILED(HR))
    return HR;
  if (!(Flags & DxcValidatorFlags_ModuleOnly)) {
    HR = ValidateDxilContainerParts(
        Module, DebugModule,
        IsDxilContainerLike(Shader->GetBufferPointer(),
                            Shader->GetBufferSize()),
        (uint32_t)Shader->GetBufferSize());
    if (FAILED(HR))
      return HR;
  }

  if (DiagContext.HasErrors() || DiagContext.HasWarnings())
    return DXC_E_IR_VERIFICATION_FAILED;

  return S_OK;
}

static uint32_t runValidation(IDxcBlob *Shader,
                              AbstractMemoryStream *DiagMemStream,
                              uint32_t Flags, DxcBuffer *DebugBitcode,
                              IDxcBlob **Hashed) {

  // Run validation may throw, but that indicates an inability to validate,
  // not that the validation failed (eg out of memory). That is indicated
  // by a failing HRESULT, and possibly error messages in the diagnostics
  // stream.

  *Hashed = nullptr;
  raw_stream_ostream DiagStream(DiagMemStream);

  if (IsDxilContainerLike(Shader->GetBufferPointer(),
                          Shader->GetBufferSize())) {
    uint32_t HR = ValidateDxilContainer(
        Shader->GetBufferPointer(), Shader->GetBufferSize(),
        DebugBitcode ? DebugBitcode->Ptr : nullptr,
        DebugBitcode ? (uint32_t)DebugBitcode->Size : 0, DiagStream);
    if (DXC_FAILED(HR))
      return HR;
  } else
    return DXC_E_CONTAINER_INVALID;

  HashAndUpdateOrCopy(Flags, Shader, Hashed);

  return S_OK;
}

static uint32_t runRootSignatureValidation(IDxcBlob *Shader,
                                           AbstractMemoryStream *DiagMemStream,
                                           uint32_t Flags, IDxcBlob **Hashed) {

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
      HashAndUpdateOrCopy(Flags, Shader, Hashed);
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
    bool IsInternalValidator,    // Run internal validator.
    IDxcOperationResult **Result // Validation output status, buffer, and errors
) {
  return validateWithDebug(Shader, Flags, IsInternalValidator, nullptr, Result);
}

uint32_t hlsl::validateWithDebug(
    IDxcBlob *Shader,            // Shader to validate.
    uint32_t Flags,              // Validation flags.
    bool IsInternalValidator,    // Run internal validator.
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

  HRESULT HR = S_OK;
  HRESULT ValidationStatus = S_OK;
  DxcThreadMalloc TM(DxcGetThreadMallocNoRef());
  try {
    CComPtr<AbstractMemoryStream> DiagMemStream;
    CComPtr<IDxcBlob> HashedBlob;
    HR = CreateMemoryStream(TM.GetInstalledAllocator(), &DiagMemStream);
    if (FAILED(HR))
      throw hlsl::Exception(HR);

    // Run validation may throw, but that indicates an inability to validate,
    // not that the validation failed (eg out of memory).
    if (Flags & DxcValidatorFlags_RootSignatureOnly)
      ValidationStatus =
          runRootSignatureValidation(Shader, DiagMemStream, Flags, &HashedBlob);
    else if ((Flags & DxcValidatorFlags_ModuleOnly) && IsInternalValidator) {
      LLVMContext Ctx;
      raw_stream_ostream DiagStream(DiagMemStream);
      llvm::DiagnosticPrinterRawOStream DiagPrinter(DiagStream);
      PrintDiagnosticContext DiagContext(DiagPrinter);
      Ctx.setDiagnosticHandler(PrintDiagnosticContext::PrintDiagnosticHandler,
                               &DiagContext, true);
      std::unique_ptr<llvm::Module> DebugModule;
      if (OptDebugBitcode) {
        HR = ValidateLoadModule((const char *)OptDebugBitcode->Ptr,
                                (uint32_t)OptDebugBitcode->Size, DebugModule,
                                Ctx, DiagStream, /*bLazyLoad*/ false);
        if (FAILED(HR))
          throw hlsl::Exception(HR);
      }
      ValidationStatus = runValidation(Shader, Flags, nullptr,
                                       DebugModule.get(), DiagMemStream);
    } else {
      ValidationStatus = runValidation(Shader, DiagMemStream, Flags,
                                       OptDebugBitcode, &HashedBlob);
    }
    if (FAILED(ValidationStatus)) {
      std::string msg("Validation failed.\n");
      ULONG cbWritten;
      DiagMemStream->Write(msg.c_str(), msg.size(), &cbWritten);
    }
    // Assemble the result object.
    CComPtr<IDxcBlob> DiagBlob;
    CComPtr<IDxcBlobEncoding> DiagBlobEnconding;
    HR = DiagMemStream.QueryInterface(&DiagBlob);
    DXASSERT_NOMSG(SUCCEEDED(HR));
    HR = DxcCreateBlobWithEncodingSet(DiagBlob, CP_UTF8, &DiagBlobEnconding);
    if (FAILED(HR))
      throw hlsl::Exception(HR);
    HR = DxcResult::Create(
        ValidationStatus, DXC_OUT_OBJECT,
        {DxcOutputObject::DataOutput(DXC_OUT_OBJECT, HashedBlob),
         DxcOutputObject::DataOutput(DXC_OUT_ERRORS, DiagBlobEnconding)},
        Result);
    if (FAILED(HR))
      throw hlsl::Exception(HR);
  } catch (std::bad_alloc &) {
    HR = E_OUTOFMEMORY;
  } catch (hlsl::Exception &_hlsl_exception_) {
    HR = _hlsl_exception_.hr;
  } catch (...) {
    HR = E_FAIL;
  }
  return HR;
}

uint32_t hlsl::validateWithOptModules(
    IDxcBlob *Shader,            // Shader to validate.
    uint32_t Flags,              // Validation flags.
    llvm::Module *Module,        // Module to validate, if available.
    llvm::Module *DebugModule,   // Debug module to validate, if available
    IDxcOperationResult **Result // Validation output status, buffer, and errors
) {
  *Result = nullptr;
  HRESULT HR = S_OK;
  HRESULT ValidationStatus = S_OK;
  DxcEtw_DxcValidation_Start();
  DxcThreadMalloc TM(DxcGetThreadMallocNoRef());
  try {
    CComPtr<AbstractMemoryStream> DiagStream;
    HR = CreateMemoryStream(TM.GetInstalledAllocator(), &DiagStream);
    if (FAILED(HR))
      throw hlsl::Exception(HR);
    CComPtr<IDxcBlob> HashedBlob;
    // Run validation may throw, but that indicates an inability to validate,
    // not that the validation failed (eg out of memory).
    if (Flags & DxcValidatorFlags_RootSignatureOnly)
      ValidationStatus =
          runRootSignatureValidation(Shader, DiagStream, Flags, &HashedBlob);
    else
      ValidationStatus =
          runValidation(Shader, Flags, Module, DebugModule, DiagStream);
    if (FAILED(ValidationStatus)) {
      std::string msg("Validation failed.\n");
      ULONG cbWritten;
      DiagStream->Write(msg.c_str(), msg.size(), &cbWritten);
    }
    // Assemble the result object.
    CComPtr<IDxcBlob> pDiagBlob;
    HR = DiagStream.QueryInterface(&pDiagBlob);
    DXASSERT_NOMSG(SUCCEEDED(HR));
    CComPtr<IDxcBlobEncoding> pDiagBlobEnconding;
    HR = DxcCreateBlobWithEncodingSet(pDiagBlob, CP_UTF8, &pDiagBlobEnconding);
    if (FAILED(HR))
      throw hlsl::Exception(HR);

    HR = DxcResult::Create(
        ValidationStatus, DXC_OUT_OBJECT,
        {DxcOutputObject::DataOutput(DXC_OUT_OBJECT, HashedBlob),
         DxcOutputObject::DataOutput(DXC_OUT_ERRORS, pDiagBlobEnconding)},
        Result);
    if (FAILED(HR))
      throw hlsl::Exception(HR);
  } catch (std::bad_alloc &) {
    HR = E_OUTOFMEMORY;
  } catch (hlsl::Exception &_hlsl_exception_) {
    HR = _hlsl_exception_.hr;
  } catch (...) {
    HR = E_FAIL;
  }

  DxcEtw_DxcValidation_Stop(SUCCEEDED(HR) ? ValidationStatus : HR);
  return HR;
}

uint32_t hlsl::getValidationVersion(unsigned *Major, unsigned *Minor) {
  if (Major == nullptr || Minor == nullptr)
    return E_INVALIDARG;
  hlsl::GetValidationVersion(Major, Minor);
  return S_OK;
}
