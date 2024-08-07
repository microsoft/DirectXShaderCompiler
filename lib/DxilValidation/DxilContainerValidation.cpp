///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilContainerValidation.cpp                                               //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// This file provides support for validating DXIL container.                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/Support/FileIOHelper.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/WinIncludes.h"

#include "dxc/DxilContainer/DxilContainer.h"
#include "dxc/DxilContainer/DxilContainerAssembler.h"
#include "dxc/DxilContainer/DxilPipelineStateValidation.h"
#include "dxc/DxilContainer/DxilRuntimeReflection.h"
#include "dxc/DxilRootSignature/DxilRootSignature.h"
#include "dxc/DxilValidation/DxilValidation.h"

#include "dxc/DXIL/DxilModule.h"
#include "dxc/DXIL/DxilUtil.h"

#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/IR/DiagnosticPrinter.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"

#include "DxilValidationUtils.h"

#include <memory>

using std::unique_ptr;
using std::unordered_set;
using std::vector;

namespace {

// Utility class for setting and restoring the diagnostic context so we may
// capture errors/warnings
struct DiagRestore {
  LLVMContext *Ctx = nullptr;
  void *OrigDiagContext;
  LLVMContext::DiagnosticHandlerTy OrigHandler;

  DiagRestore(llvm::LLVMContext &InputCtx, void *DiagContext) : Ctx(&InputCtx) {
    init(DiagContext);
  }
  DiagRestore(Module *M, void *DiagContext) {
    if (!M)
      return;
    Ctx = &M->getContext();
    init(DiagContext);
  }
  ~DiagRestore() {
    if (!Ctx)
      return;
    Ctx->setDiagnosticHandler(OrigHandler, OrigDiagContext);
  }

private:
  void init(void *DiagContext) {
    OrigHandler = Ctx->getDiagnosticHandler();
    OrigDiagContext = Ctx->getDiagnosticContext();
    Ctx->setDiagnosticHandler(
        hlsl::PrintDiagnosticContext::PrintDiagnosticHandler, DiagContext);
  }
};

static void emitDxilDiag(LLVMContext &Ctx, const char *str) {
  hlsl::dxilutil::EmitErrorOnContext(Ctx, str);
}
} // namespace

namespace hlsl {

// DXIL Container Verification Functions

static void VerifyBlobPartMatches(ValidationContext &ValCtx, LPCSTR pName,
                                  DxilPartWriter *pWriter, const void *pData,
                                  uint32_t Size) {
  if (!pData && pWriter->size()) {
    // No blob part, but writer says non-zero size is expected.
    ValCtx.EmitFormatError(ValidationRule::ContainerPartMissing, {pName});
    return;
  }

  // Compare sizes
  if (pWriter->size() != Size) {
    ValCtx.EmitFormatError(ValidationRule::ContainerPartMatches, {pName});
    return;
  }

  if (Size == 0) {
    return;
  }

  CComPtr<AbstractMemoryStream> pOutputStream;
  IFT(CreateMemoryStream(DxcGetThreadMallocNoRef(), &pOutputStream));
  pOutputStream->Reserve(Size);

  pWriter->write(pOutputStream);
  DXASSERT(pOutputStream->GetPtrSize() == Size,
           "otherwise, DxilPartWriter misreported size");

  if (memcmp(pData, pOutputStream->GetPtr(), Size)) {
    ValCtx.EmitFormatError(ValidationRule::ContainerPartMatches, {pName});
    return;
  }

  return;
}

static void VerifySignatureMatches(ValidationContext &ValCtx,
                                   DXIL::SignatureKind SigKind,
                                   const void *pSigData, uint32_t SigSize) {
  // Generate corresponding signature from module and memcmp

  const char *pName = nullptr;
  switch (SigKind) {
  case hlsl::DXIL::SignatureKind::Input:
    pName = "Program Input Signature";
    break;
  case hlsl::DXIL::SignatureKind::Output:
    pName = "Program Output Signature";
    break;
  case hlsl::DXIL::SignatureKind::PatchConstOrPrim:
    if (ValCtx.DxilMod.GetShaderModel()->GetKind() == DXIL::ShaderKind::Mesh)
      pName = "Program Primitive Signature";
    else
      pName = "Program Patch Constant Signature";
    break;
  default:
    break;
  }

  unique_ptr<DxilPartWriter> pWriter(
      NewProgramSignatureWriter(ValCtx.DxilMod, SigKind));
  VerifyBlobPartMatches(ValCtx, pName, pWriter.get(), pSigData, SigSize);
}

bool VerifySignatureMatches(llvm::Module *pModule, DXIL::SignatureKind SigKind,
                            const void *pSigData, uint32_t SigSize) {
  ValidationContext ValCtx(*pModule, nullptr, pModule->GetOrCreateDxilModule());
  VerifySignatureMatches(ValCtx, SigKind, pSigData, SigSize);
  return !ValCtx.Failed;
}

static void VerifyPSVMatches(ValidationContext &ValCtx, const void *pPSVData,
                             uint32_t PSVSize) {
  uint32_t PSVVersion =
      MAX_PSV_VERSION; // This should be set to the newest version
  unique_ptr<DxilPartWriter> pWriter(NewPSVWriter(ValCtx.DxilMod, PSVVersion));
  // Try each version in case an earlier version matches module
  while (PSVVersion && pWriter->size() != PSVSize) {
    PSVVersion--;
    pWriter.reset(NewPSVWriter(ValCtx.DxilMod, PSVVersion));
  }
  // generate PSV data from module and memcmp
  VerifyBlobPartMatches(ValCtx, "Pipeline State Validation", pWriter.get(),
                        pPSVData, PSVSize);
}

static void VerifyFeatureInfoMatches(ValidationContext &ValCtx,
                                     const void *pFeatureInfoData,
                                     uint32_t FeatureInfoSize) {
  // generate Feature Info data from module and memcmp
  unique_ptr<DxilPartWriter> pWriter(NewFeatureInfoWriter(ValCtx.DxilMod));
  VerifyBlobPartMatches(ValCtx, "Feature Info", pWriter.get(), pFeatureInfoData,
                        FeatureInfoSize);
}

// return true if the pBlob is a valid, well-formed CompilerVersion part, false
// otherwise
bool ValidateCompilerVersionPart(const void *pBlobPtr, UINT blobSize) {
  // The hlsl::DxilCompilerVersion struct is always 16 bytes. (2 2-byte
  // uint16's, 3 4-byte uint32's) The blob size should absolutely never be less
  // than 16 bytes.
  if (blobSize < sizeof(hlsl::DxilCompilerVersion)) {
    return false;
  }

  const hlsl::DxilCompilerVersion *pDCV =
      (const hlsl::DxilCompilerVersion *)pBlobPtr;
  if (pDCV->VersionStringListSizeInBytes == 0) {
    // No version strings, just make sure there is no extra space.
    return blobSize == sizeof(hlsl::DxilCompilerVersion);
  }

  // after this point, we know VersionStringListSizeInBytes >= 1, because it is
  // a UINT

  UINT EndOfVersionStringIndex =
      sizeof(hlsl::DxilCompilerVersion) + pDCV->VersionStringListSizeInBytes;
  // Make sure that the buffer size is large enough to contain both the DCV
  // struct and the version string but not any larger than necessary
  if (PSVALIGN4(EndOfVersionStringIndex) != blobSize) {
    return false;
  }

  const char *VersionStringsListData =
      (const char *)pBlobPtr + sizeof(hlsl::DxilCompilerVersion);
  UINT VersionStringListSizeInBytes = pDCV->VersionStringListSizeInBytes;

  // now make sure that any pad bytes that were added are null-terminators.
  for (UINT i = VersionStringListSizeInBytes;
       i < blobSize - sizeof(hlsl::DxilCompilerVersion); i++) {
    if (VersionStringsListData[i] != '\0') {
      return false;
    }
  }

  // Now, version string validation
  // first, the final byte of the string should always be null-terminator so
  // that the string ends
  if (VersionStringsListData[VersionStringListSizeInBytes - 1] != '\0') {
    return false;
  }

  // construct the first string
  // data format for VersionString can be see in the definition for the
  // DxilCompilerVersion struct. summary: 2 strings that each end with the null
  // terminator, and [0-3] null terminators after the final null terminator
  StringRef firstStr(VersionStringsListData);

  // if the second string exists, attempt to construct it.
  if (VersionStringListSizeInBytes > (firstStr.size() + 1)) {
    StringRef secondStr(VersionStringsListData + firstStr.size() + 1);

    // the VersionStringListSizeInBytes member should be exactly equal to the
    // two string lengths, plus the 2 null terminator bytes.
    if (VersionStringListSizeInBytes !=
        firstStr.size() + secondStr.size() + 2) {
      return false;
    }
  } else {
    // the VersionStringListSizeInBytes member should be exactly equal to the
    // first string length, plus the 1 null terminator byte.
    if (VersionStringListSizeInBytes != firstStr.size() + 1) {
      return false;
    }
  }

  return true;
}

static void VerifyRDATMatches(ValidationContext &ValCtx, const void *pRDATData,
                              uint32_t RDATSize) {
  const char *PartName = "Runtime Data (RDAT)";
  RDAT::DxilRuntimeData rdat(pRDATData, RDATSize);
  if (!rdat.Validate()) {
    ValCtx.EmitFormatError(ValidationRule::ContainerPartMatches, {PartName});
    return;
  }

  // If DxilModule subobjects already loaded, validate these against the RDAT
  // blob, otherwise, load subobject into DxilModule to generate reference RDAT.
  if (!ValCtx.DxilMod.GetSubobjects()) {
    auto table = rdat.GetSubobjectTable();
    if (table && table.Count() > 0) {
      ValCtx.DxilMod.ResetSubobjects(new DxilSubobjects());
      if (!LoadSubobjectsFromRDAT(*ValCtx.DxilMod.GetSubobjects(), rdat)) {
        ValCtx.EmitFormatError(ValidationRule::ContainerPartMatches,
                               {PartName});
        return;
      }
    }
  }

  unique_ptr<DxilPartWriter> pWriter(NewRDATWriter(ValCtx.DxilMod));
  VerifyBlobPartMatches(ValCtx, PartName, pWriter.get(), pRDATData, RDATSize);
}

bool VerifyRDATMatches(llvm::Module *pModule, const void *pRDATData,
                       uint32_t RDATSize) {
  ValidationContext ValCtx(*pModule, nullptr, pModule->GetOrCreateDxilModule());
  VerifyRDATMatches(ValCtx, pRDATData, RDATSize);
  return !ValCtx.Failed;
}

bool VerifyFeatureInfoMatches(llvm::Module *pModule,
                              const void *pFeatureInfoData,
                              uint32_t FeatureInfoSize) {
  ValidationContext ValCtx(*pModule, nullptr, pModule->GetOrCreateDxilModule());
  VerifyFeatureInfoMatches(ValCtx, pFeatureInfoData, FeatureInfoSize);
  return !ValCtx.Failed;
}

HRESULT ValidateDxilContainerParts(llvm::Module *pModule,
                                   llvm::Module *pDebugModule,
                                   const DxilContainerHeader *pContainer,
                                   uint32_t ContainerSize) {

  DXASSERT_NOMSG(pModule);
  if (!pContainer || !IsValidDxilContainer(pContainer, ContainerSize)) {
    return DXC_E_CONTAINER_INVALID;
  }

  DxilModule *pDxilModule = DxilModule::TryGetDxilModule(pModule);
  if (!pDxilModule) {
    return DXC_E_IR_VERIFICATION_FAILED;
  }

  ValidationContext ValCtx(*pModule, pDebugModule, *pDxilModule);

  DXIL::ShaderKind ShaderKind = pDxilModule->GetShaderModel()->GetKind();
  bool bTessOrMesh = ShaderKind == DXIL::ShaderKind::Hull ||
                     ShaderKind == DXIL::ShaderKind::Domain ||
                     ShaderKind == DXIL::ShaderKind::Mesh;

  std::unordered_set<uint32_t> FourCCFound;
  const DxilPartHeader *pRootSignaturePart = nullptr;
  const DxilPartHeader *pPSVPart = nullptr;

  for (auto it = begin(pContainer), itEnd = end(pContainer); it != itEnd;
       ++it) {
    const DxilPartHeader *pPart = *it;

    char szFourCC[5];
    PartKindToCharArray(pPart->PartFourCC, szFourCC);
    if (FourCCFound.find(pPart->PartFourCC) != FourCCFound.end()) {
      // Two parts with same FourCC found
      ValCtx.EmitFormatError(ValidationRule::ContainerPartRepeated, {szFourCC});
      continue;
    }
    FourCCFound.insert(pPart->PartFourCC);

    switch (pPart->PartFourCC) {
    case DFCC_InputSignature:
      if (ValCtx.isLibProfile) {
        ValCtx.EmitFormatError(ValidationRule::ContainerPartInvalid,
                               {szFourCC});
      } else {
        VerifySignatureMatches(ValCtx, DXIL::SignatureKind::Input,
                               GetDxilPartData(pPart), pPart->PartSize);
      }
      break;
    case DFCC_OutputSignature:
      if (ValCtx.isLibProfile) {
        ValCtx.EmitFormatError(ValidationRule::ContainerPartInvalid,
                               {szFourCC});
      } else {
        VerifySignatureMatches(ValCtx, DXIL::SignatureKind::Output,
                               GetDxilPartData(pPart), pPart->PartSize);
      }
      break;
    case DFCC_PatchConstantSignature:
      if (ValCtx.isLibProfile) {
        ValCtx.EmitFormatError(ValidationRule::ContainerPartInvalid,
                               {szFourCC});
      } else {
        if (bTessOrMesh) {
          VerifySignatureMatches(ValCtx, DXIL::SignatureKind::PatchConstOrPrim,
                                 GetDxilPartData(pPart), pPart->PartSize);
        } else {
          ValCtx.EmitFormatError(ValidationRule::ContainerPartMatches,
                                 {"Program Patch Constant Signature"});
        }
      }
      break;
    case DFCC_FeatureInfo:
      VerifyFeatureInfoMatches(ValCtx, GetDxilPartData(pPart), pPart->PartSize);
      break;
    case DFCC_CompilerVersion:
      // This blob is either a PDB, or a library profile
      if (ValCtx.isLibProfile) {
        if (!ValidateCompilerVersionPart((void *)GetDxilPartData(pPart),
                                         pPart->PartSize)) {
          ValCtx.EmitFormatError(ValidationRule::ContainerPartInvalid,
                                 {szFourCC});
        }
      } else {
        ValCtx.EmitFormatError(ValidationRule::ContainerPartInvalid,
                               {szFourCC});
      }
      break;

    case DFCC_RootSignature:
      pRootSignaturePart = pPart;
      if (ValCtx.isLibProfile) {
        ValCtx.EmitFormatError(ValidationRule::ContainerPartInvalid,
                               {szFourCC});
      }
      break;
    case DFCC_PipelineStateValidation:
      pPSVPart = pPart;
      if (ValCtx.isLibProfile) {
        ValCtx.EmitFormatError(ValidationRule::ContainerPartInvalid,
                               {szFourCC});
      } else {
        VerifyPSVMatches(ValCtx, GetDxilPartData(pPart), pPart->PartSize);
      }
      break;

    // Skip these
    case DFCC_ResourceDef:
    case DFCC_ShaderStatistics:
    case DFCC_PrivateData:
    case DFCC_DXIL:
    case DFCC_ShaderDebugInfoDXIL:
    case DFCC_ShaderDebugName:
      continue;

    case DFCC_ShaderHash:
      if (pPart->PartSize != sizeof(DxilShaderHash)) {
        ValCtx.EmitFormatError(ValidationRule::ContainerPartInvalid,
                               {szFourCC});
      }
      break;

    // Runtime Data (RDAT) for libraries
    case DFCC_RuntimeData:
      if (ValCtx.isLibProfile) {
        // TODO: validate without exact binary comparison of serialized data
        //  - support earlier versions
        //  - verify no newer record versions than known here (size no larger
        //  than newest version)
        //  - verify all data makes sense and matches expectations based on
        //  module
        VerifyRDATMatches(ValCtx, GetDxilPartData(pPart), pPart->PartSize);
      } else {
        ValCtx.EmitFormatError(ValidationRule::ContainerPartInvalid,
                               {szFourCC});
      }
      break;

    case DFCC_Container:
    default:
      ValCtx.EmitFormatError(ValidationRule::ContainerPartInvalid, {szFourCC});
      break;
    }
  }

  // Verify required parts found
  if (ValCtx.isLibProfile) {
    if (FourCCFound.find(DFCC_RuntimeData) == FourCCFound.end()) {
      ValCtx.EmitFormatError(ValidationRule::ContainerPartMissing,
                             {"Runtime Data (RDAT)"});
    }
  } else {
    if (FourCCFound.find(DFCC_InputSignature) == FourCCFound.end()) {
      VerifySignatureMatches(ValCtx, DXIL::SignatureKind::Input, nullptr, 0);
    }
    if (FourCCFound.find(DFCC_OutputSignature) == FourCCFound.end()) {
      VerifySignatureMatches(ValCtx, DXIL::SignatureKind::Output, nullptr, 0);
    }
    if (bTessOrMesh &&
        FourCCFound.find(DFCC_PatchConstantSignature) == FourCCFound.end() &&
        pDxilModule->GetPatchConstOrPrimSignature().GetElements().size()) {
      ValCtx.EmitFormatError(ValidationRule::ContainerPartMissing,
                             {"Program Patch Constant Signature"});
    }
    if (FourCCFound.find(DFCC_FeatureInfo) == FourCCFound.end()) {
      // Could be optional, but RS1 runtime doesn't handle this case properly.
      ValCtx.EmitFormatError(ValidationRule::ContainerPartMissing,
                             {"Feature Info"});
    }

    // Validate Root Signature
    if (pPSVPart) {
      if (pRootSignaturePart) {
        std::string diagStr;
        raw_string_ostream DiagStream(diagStr);
        try {
          RootSignatureHandle RS;
          RS.LoadSerialized(
              (const uint8_t *)GetDxilPartData(pRootSignaturePart),
              pRootSignaturePart->PartSize);
          RS.Deserialize();
          IFTBOOL(VerifyRootSignatureWithShaderPSV(
                      RS.GetDesc(), pDxilModule->GetShaderModel()->GetKind(),
                      GetDxilPartData(pPSVPart), pPSVPart->PartSize,
                      DiagStream),
                  DXC_E_INCORRECT_ROOT_SIGNATURE);
        } catch (...) {
          ValCtx.EmitError(ValidationRule::ContainerRootSignatureIncompatible);
          emitDxilDiag(pModule->getContext(), DiagStream.str().c_str());
        }
      }
    } else {
      ValCtx.EmitFormatError(ValidationRule::ContainerPartMissing,
                             {"Pipeline State Validation"});
    }
  }

  if (ValCtx.Failed) {
    return DXC_E_MALFORMED_CONTAINER;
  }
  return S_OK;
}

static HRESULT FindDxilPart(const void *pContainerBytes, uint32_t ContainerSize,
                            DxilFourCC FourCC, const DxilPartHeader **ppPart) {

  const DxilContainerHeader *pContainer =
      IsDxilContainerLike(pContainerBytes, ContainerSize);

  if (!pContainer) {
    IFR(DXC_E_CONTAINER_INVALID);
  }
  if (!IsValidDxilContainer(pContainer, ContainerSize)) {
    IFR(DXC_E_CONTAINER_INVALID);
  }

  DxilPartIterator it =
      std::find_if(begin(pContainer), end(pContainer), DxilPartIsType(FourCC));
  if (it == end(pContainer)) {
    IFR(DXC_E_CONTAINER_MISSING_DXIL);
  }

  const DxilProgramHeader *pProgramHeader =
      reinterpret_cast<const DxilProgramHeader *>(GetDxilPartData(*it));
  if (!IsValidDxilProgramHeader(pProgramHeader, (*it)->PartSize)) {
    IFR(DXC_E_CONTAINER_INVALID);
  }

  *ppPart = *it;
  return S_OK;
}

HRESULT ValidateLoadModule(const char *pIL, uint32_t ILLength,
                           unique_ptr<llvm::Module> &pModule, LLVMContext &Ctx,
                           llvm::raw_ostream &DiagStream, unsigned bLazyLoad) {

  llvm::DiagnosticPrinterRawOStream DiagPrinter(DiagStream);
  PrintDiagnosticContext DiagContext(DiagPrinter);
  DiagRestore DR(Ctx, &DiagContext);

  std::unique_ptr<llvm::MemoryBuffer> pBitcodeBuf;
  pBitcodeBuf.reset(llvm::MemoryBuffer::getMemBuffer(
                        llvm::StringRef(pIL, ILLength), "", false)
                        .release());

  ErrorOr<std::unique_ptr<Module>> loadedModuleResult =
      bLazyLoad == 0
          ? llvm::parseBitcodeFile(pBitcodeBuf->getMemBufferRef(), Ctx, nullptr,
                                   true /*Track Bitstream*/)
          : llvm::getLazyBitcodeModule(std::move(pBitcodeBuf), Ctx, nullptr,
                                       false, true /*Track Bitstream*/);

  // DXIL disallows some LLVM bitcode constructs, like unaccounted-for
  // sub-blocks. These appear as warnings, which the validator should reject.
  if (DiagContext.HasErrors() || DiagContext.HasWarnings() ||
      loadedModuleResult.getError())
    return DXC_E_IR_VERIFICATION_FAILED;

  pModule = std::move(loadedModuleResult.get());
  return S_OK;
}

HRESULT ValidateDxilBitcode(const char *pIL, uint32_t ILLength,
                            llvm::raw_ostream &DiagStream) {

  LLVMContext Ctx;
  std::unique_ptr<llvm::Module> pModule;

  llvm::DiagnosticPrinterRawOStream DiagPrinter(DiagStream);
  PrintDiagnosticContext DiagContext(DiagPrinter);
  Ctx.setDiagnosticHandler(PrintDiagnosticContext::PrintDiagnosticHandler,
                           &DiagContext, true);

  HRESULT hr;
  if (FAILED(hr = ValidateLoadModule(pIL, ILLength, pModule, Ctx, DiagStream,
                                     /*bLazyLoad*/ false)))
    return hr;

  if (FAILED(hr = ValidateDxilModule(pModule.get(), nullptr)))
    return hr;

  DxilModule &dxilModule = pModule->GetDxilModule();
  auto &SerializedRootSig = dxilModule.GetSerializedRootSignature();
  if (!SerializedRootSig.empty()) {
    unique_ptr<DxilPartWriter> pWriter(NewPSVWriter(dxilModule));
    DXASSERT_NOMSG(pWriter->size());
    CComPtr<AbstractMemoryStream> pOutputStream;
    IFT(CreateMemoryStream(DxcGetThreadMallocNoRef(), &pOutputStream));
    pOutputStream->Reserve(pWriter->size());
    pWriter->write(pOutputStream);
    DxilVersionedRootSignature desc;
    try {
      DeserializeRootSignature(SerializedRootSig.data(),
                               SerializedRootSig.size(), desc.get_address_of());
      if (!desc.get()) {
        return DXC_E_INCORRECT_ROOT_SIGNATURE;
      }
      IFTBOOL(VerifyRootSignatureWithShaderPSV(
                  desc.get(), dxilModule.GetShaderModel()->GetKind(),
                  pOutputStream->GetPtr(), pWriter->size(), DiagStream),
              DXC_E_INCORRECT_ROOT_SIGNATURE);
    } catch (...) {
      return DXC_E_INCORRECT_ROOT_SIGNATURE;
    }
  }

  if (DiagContext.HasErrors() || DiagContext.HasWarnings()) {
    return DXC_E_IR_VERIFICATION_FAILED;
  }

  return S_OK;
}

static HRESULT ValidateLoadModuleFromContainer(
    const void *pContainer, uint32_t ContainerSize,
    std::unique_ptr<llvm::Module> &pModule,
    std::unique_ptr<llvm::Module> &pDebugModule, llvm::LLVMContext &Ctx,
    LLVMContext &DbgCtx, llvm::raw_ostream &DiagStream, unsigned bLazyLoad) {
  llvm::DiagnosticPrinterRawOStream DiagPrinter(DiagStream);
  PrintDiagnosticContext DiagContext(DiagPrinter);
  DiagRestore DR(Ctx, &DiagContext);
  DiagRestore DR2(DbgCtx, &DiagContext);

  const DxilPartHeader *pPart = nullptr;
  IFR(FindDxilPart(pContainer, ContainerSize, DFCC_DXIL, &pPart));

  const char *pIL = nullptr;
  uint32_t ILLength = 0;
  GetDxilProgramBitcode(
      reinterpret_cast<const DxilProgramHeader *>(GetDxilPartData(pPart)), &pIL,
      &ILLength);

  IFR(ValidateLoadModule(pIL, ILLength, pModule, Ctx, DiagStream, bLazyLoad));

  HRESULT hr;
  const DxilPartHeader *pDbgPart = nullptr;
  if (FAILED(hr = FindDxilPart(pContainer, ContainerSize,
                               DFCC_ShaderDebugInfoDXIL, &pDbgPart)) &&
      hr != DXC_E_CONTAINER_MISSING_DXIL) {
    return hr;
  }

  if (pDbgPart) {
    GetDxilProgramBitcode(
        reinterpret_cast<const DxilProgramHeader *>(GetDxilPartData(pDbgPart)),
        &pIL, &ILLength);
    if (FAILED(hr = ValidateLoadModule(pIL, ILLength, pDebugModule, DbgCtx,
                                       DiagStream, bLazyLoad))) {
      return hr;
    }
  }

  return S_OK;
}

HRESULT ValidateLoadModuleFromContainer(
    const void *pContainer, uint32_t ContainerSize,
    std::unique_ptr<llvm::Module> &pModule,
    std::unique_ptr<llvm::Module> &pDebugModule, llvm::LLVMContext &Ctx,
    llvm::LLVMContext &DbgCtx, llvm::raw_ostream &DiagStream) {
  return ValidateLoadModuleFromContainer(pContainer, ContainerSize, pModule,
                                         pDebugModule, Ctx, DbgCtx, DiagStream,
                                         /*bLazyLoad*/ false);
}
// Lazy loads module from container, validating load, but not module.
HRESULT ValidateLoadModuleFromContainerLazy(
    const void *pContainer, uint32_t ContainerSize,
    std::unique_ptr<llvm::Module> &pModule,
    std::unique_ptr<llvm::Module> &pDebugModule, llvm::LLVMContext &Ctx,
    llvm::LLVMContext &DbgCtx, llvm::raw_ostream &DiagStream) {
  return ValidateLoadModuleFromContainer(pContainer, ContainerSize, pModule,
                                         pDebugModule, Ctx, DbgCtx, DiagStream,
                                         /*bLazyLoad*/ true);
}

HRESULT ValidateDxilContainer(const void *pContainer, uint32_t ContainerSize,
                              llvm::Module *pDebugModule,
                              llvm::raw_ostream &DiagStream) {
  LLVMContext Ctx, DbgCtx;
  std::unique_ptr<llvm::Module> pModule, pDebugModuleInContainer;

  llvm::DiagnosticPrinterRawOStream DiagPrinter(DiagStream);
  PrintDiagnosticContext DiagContext(DiagPrinter);
  Ctx.setDiagnosticHandler(PrintDiagnosticContext::PrintDiagnosticHandler,
                           &DiagContext, true);
  DbgCtx.setDiagnosticHandler(PrintDiagnosticContext::PrintDiagnosticHandler,
                              &DiagContext, true);

  DiagRestore DR(pDebugModule, &DiagContext);

  IFR(ValidateLoadModuleFromContainer(pContainer, ContainerSize, pModule,
                                      pDebugModuleInContainer, Ctx, DbgCtx,
                                      DiagStream));

  if (pDebugModuleInContainer)
    pDebugModule = pDebugModuleInContainer.get();

  // Validate DXIL Module
  IFR(ValidateDxilModule(pModule.get(), pDebugModule));

  if (DiagContext.HasErrors() || DiagContext.HasWarnings()) {
    return DXC_E_IR_VERIFICATION_FAILED;
  }

  return ValidateDxilContainerParts(
      pModule.get(), pDebugModule,
      IsDxilContainerLike(pContainer, ContainerSize), ContainerSize);
}

HRESULT ValidateDxilContainer(const void *pContainer, uint32_t ContainerSize,
                              llvm::raw_ostream &DiagStream) {
  return ValidateDxilContainer(pContainer, ContainerSize, nullptr, DiagStream);
}
} // namespace hlsl
