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

#include "llvm/ADT/ArrayRef.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/IR/DiagnosticPrinter.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"

#include "DxilValidationUtils.h"

#include <memory>
#include <unordered_set>

using namespace llvm;
using namespace hlsl;

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

struct SimpleViewIDState {
  unsigned NumInputSigScalars = 0;
  unsigned NumOutputSigScalars[DXIL::kNumOutputStreams] = {0, 0, 0, 0};
  unsigned NumPCOrPrimSigScalars = 0;

  std::vector<uint32_t> InputToOutputTable[DXIL::kNumOutputStreams];
  std::vector<uint32_t> InputToPCOutputTable;
  std::vector<uint32_t> PCInputToOutputTable;
  MutableArrayRef<uint32_t> ViewIDOutputMask[DXIL::kNumOutputStreams];
  MutableArrayRef<uint32_t> ViewIDPCOutputMask;
  bool IsValid = true;
  SimpleViewIDState(std::vector<uint32_t> &Data, PSVShaderKind,
                    bool UsesViewID);
  void Print(raw_ostream &OS, PSVShaderKind ShaderStage, bool UsesViewID) {
    unsigned NumStreams =
        ShaderStage == PSVShaderKind::Geometry ? PSV_GS_MAX_STREAMS : 1;

    if (UsesViewID) {
      for (unsigned i = 0; i < NumStreams; ++i) {
        OS << "Outputs affected by ViewID as a bitmask for stream " << i
           << ":\n ";
        uint8_t OutputVectors =
            llvm::RoundUpToAlignment(NumOutputSigScalars[i], 4) / 4;
        const PSVComponentMask ViewIDMask(ViewIDOutputMask[i].data(),
                                          OutputVectors);
        std::string OutputSetName = "Outputs";
        OutputSetName += "[" + std::to_string(i) + "]";
        ViewIDMask.Print(OS, "ViewID", OutputSetName.c_str());
      }

      if (ShaderStage == PSVShaderKind::Hull ||
          ShaderStage == PSVShaderKind::Mesh) {
        OS << "PCOutputs affected by ViewID as a bitmask:\n";
        uint8_t OutputVectors =
            llvm::RoundUpToAlignment(NumPCOrPrimSigScalars, 4) / 4;
        const PSVComponentMask ViewIDMask(ViewIDPCOutputMask.data(),
                                          OutputVectors);
        ViewIDMask.Print(OS, "ViewID", "PCOutputs");
      }
    }

    for (unsigned i = 0; i < NumStreams; ++i) {
      OS << "Outputs affected by inputs as a table of bitmasks for stream " << i
         << ":\n";
      uint8_t InputVectors =
          llvm::RoundUpToAlignment(NumInputSigScalars, 4) / 4;
      uint8_t OutputVectors =
          llvm::RoundUpToAlignment(NumOutputSigScalars[i], 4) / 4;
      const PSVDependencyTable Table(InputToOutputTable[i].data(), InputVectors,
                                     OutputVectors);
      std::string OutputSetName = "Outputs";
      OutputSetName += "[" + std::to_string(i) + "]";
      Table.Print(OS, "Inputs", OutputSetName.c_str());
    }

    if (ShaderStage == PSVShaderKind::Hull ||
        ShaderStage == PSVShaderKind::Mesh) {
      OS << "Patch constant outputs affected by inputs as a table of "
            "bitmasks:\n";
      uint8_t InputVectors =
          llvm::RoundUpToAlignment(NumInputSigScalars, 4) / 4;
      uint8_t OutputVectors =
          llvm::RoundUpToAlignment(NumPCOrPrimSigScalars, 4) / 4;
      const PSVDependencyTable Table(InputToPCOutputTable.data(), InputVectors,
                                     OutputVectors);
      Table.Print(OS, "Inputs", "PatchConstantOutputs");
    } else if (ShaderStage == PSVShaderKind::Domain) {
      OS << "Outputs affected by patch constant inputs as a table of "
            "bitmasks:\n";
      uint8_t InputVectors =
          llvm::RoundUpToAlignment(NumPCOrPrimSigScalars, 4) / 4;
      uint8_t OutputVectors =
          llvm::RoundUpToAlignment(NumOutputSigScalars[0], 4) / 4;
      const PSVDependencyTable Table(PCInputToOutputTable.data(), InputVectors,
                                     OutputVectors);
      Table.Print(OS, "PatchConstantInputs", "Outputs");
    }
  }
};
SimpleViewIDState::SimpleViewIDState(std::vector<uint32_t> &Data,
                                     PSVShaderKind SK, bool UsesViewID) {
  if (Data.empty())
    return;

  unsigned Offset = 0;
  // #Inputs.
  NumInputSigScalars = Data[Offset++];
  unsigned NumStreams =
      SK == PSVShaderKind::Geometry ? DXIL::kNumOutputStreams : 1;
  for (unsigned i = 0; i < NumStreams; i++) {
    // #Outputs for stream StreamId.
    unsigned OutputScalars = Data[Offset++];
    NumOutputSigScalars[i] = OutputScalars;
    unsigned MaskDwords = llvm::RoundUpToAlignment(OutputScalars, 32) / 32;
    if (UsesViewID) {
      ViewIDOutputMask[i] = {Data.data() + Offset, MaskDwords};
      Offset += MaskDwords;
    }
    unsigned TabSize = MaskDwords * NumInputSigScalars;
    if (TabSize == 0)
      continue;
    unsigned AlignedTabSize =
        MaskDwords * llvm::RoundUpToAlignment(NumInputSigScalars, 4);
    InputToOutputTable[i] = std::vector<uint32_t>(AlignedTabSize, 0);
    memcpy(InputToOutputTable[i].data(), Data.data() + Offset, TabSize * 4);
    Offset += TabSize;
  }
  if (SK == PSVShaderKind::Hull || SK == PSVShaderKind::Mesh) {
    // #PatchConstant.
    NumPCOrPrimSigScalars = Data[Offset++];
    unsigned MaskDwords =
        llvm::RoundUpToAlignment(NumPCOrPrimSigScalars, 32) / 32;
    if (UsesViewID) {
      ViewIDPCOutputMask = {Data.data() + Offset, MaskDwords};
      Offset += MaskDwords;
    }
    unsigned TabSize = MaskDwords * NumInputSigScalars;
    if (TabSize) {
      unsigned AlignedTabSize =
          MaskDwords * llvm::RoundUpToAlignment(NumInputSigScalars, 4);
      InputToPCOutputTable = std::vector<uint32_t>(AlignedTabSize, 0);
      memcpy(InputToPCOutputTable.data(), Data.data() + Offset, TabSize * 4);
      Offset += TabSize;
    }
  } else if (SK == PSVShaderKind::Domain) {
    // #PatchConstant.
    NumPCOrPrimSigScalars = Data[Offset++];
    unsigned OutputScalars = NumOutputSigScalars[0];
    unsigned MaskDwords = llvm::RoundUpToAlignment(OutputScalars, 32) / 32;
    unsigned TabSize = MaskDwords * NumPCOrPrimSigScalars;
    if (TabSize) {
      unsigned AlignedTabSize =
          MaskDwords * llvm::RoundUpToAlignment(NumPCOrPrimSigScalars, 4);
      PCInputToOutputTable = std::vector<uint32_t>(AlignedTabSize, 0);
      memcpy(PCInputToOutputTable.data(), Data.data() + Offset, TabSize * 4);
      Offset += TabSize;
    }
  }
  IsValid = Offset == Data.size();
}

class PSVContentVerifier {
  DxilModule &DM;
  DxilPipelineStateValidation &PSV;
  ValidationContext &ValCtx;
  bool PSVContentValid = true;

  struct PSVSignatureElementHash {
    uint64_t operator()(const PSVSignatureElement0 *SE) const {
      return SE->ColsAndStart | (SE->StartRow << 8) | (SE->SemanticKind << 16) |
             ((uint64_t)SE->DynamicMaskAndStream << 24) |
             ((uint64_t)SE->SemanticIndexes << 32);
    }
  };
  struct PSVSignatureElementEqual {
    bool operator()(const PSVSignatureElement0 *lhs,
                    const PSVSignatureElement0 *rhs) const {
      return lhs->ColsAndStart == rhs->ColsAndStart &&
             lhs->StartRow == rhs->StartRow &&
             lhs->DynamicMaskAndStream == rhs->DynamicMaskAndStream &&
             lhs->SemanticKind == rhs->SemanticKind &&
             lhs->SemanticIndexes == rhs->SemanticIndexes;
    }
  };
  using PSVSignatureSet =
      std::unordered_set<PSVSignatureElement0 *, PSVSignatureElementHash,
                         PSVSignatureElementEqual>;

  struct PSVResourceHash {
    uint64_t operator()(const PSVResourceBindInfo0 *BI) const {
      return BI->Space | ((uint64_t)BI->LowerBound << 32) |
             ((uint64_t)BI->ResType << 56);
    }
  };
  struct PSVResourceEqual {
    template <typename T> bool operator()(const T *lhs, const T *rhs) const {
      return memcmp(lhs, rhs, sizeof(T)) == 0;
    }
  };
  using PSVResourceSet = std::unordered_set<PSVResourceBindInfo0 *,
                                            PSVResourceHash, PSVResourceEqual>;

public:
  PSVContentVerifier(DxilPipelineStateValidation &PSV, DxilModule &DM,
                     ValidationContext &ValCtx)
      : DM(DM), PSV(PSV), ValCtx(ValCtx) {}
  void Verify();

private:
  void VerifySignatures(unsigned ValMajor, unsigned ValMinor);
  void VerifySignature(const DxilSignature &, PSVSignatureElement0 *Base,
                       unsigned Count, std::string Name,
                       bool i1ToUnknownCompat);
  void VerifySignatureElement(const DxilSignatureElement &, PSVSignatureSet &,
                              const PSVStringTable &,
                              const PSVSemanticIndexTable &, std::string, bool);
  void VerifyResources(unsigned PSVVersion);
  template <typename T>
  void VerifyResourceTable(T &ResTab, PSVResourceSet &ResSet,
                           unsigned &ResIndex, unsigned PSVVersion);
  void VerifyViewIDDependence(PSVRuntimeInfo1 *PSV1);
  void VerifyEntryProperties(const ShaderModel *SM, PSVRuntimeInfo0 *PSV0,
                             PSVRuntimeInfo1 *PSV1, PSVRuntimeInfo2 *PSV2);
  void EmitDuplicateError(StringRef Name, StringRef DupContent) {
    ValCtx.EmitFormatError(ValidationRule::ContainerContentDuplicates,
                           {Name, DupContent});
    PSVContentValid = false;
  }
  void EmitMissingError(StringRef Name, StringRef Content) {
    ValCtx.EmitFormatError(ValidationRule::ContainerContentMissing,
                           {Name, Content});
    PSVContentValid = false;
  }
  void EmitMismatchError(StringRef Name, StringRef Expected, StringRef Actual) {
    ValCtx.EmitFormatError(ValidationRule::ContainerContentMatches,
                           {Name, Expected, Actual});
    PSVContentValid = false;
  }
  void EmitInvalidError(StringRef Name) {
    ValCtx.EmitFormatError(ValidationRule::ContainerContentInvalid, {Name});
    PSVContentValid = false;
  }
  template <typename Ty> static std::string GetDump(const Ty &T) {
    std::string Str;
    raw_string_ostream OS(Str);
    T.Print(OS);
    OS.flush();
    return Str;
  }
  template <typename Ty>
  static std::string GetViewIDDump(const Ty &T, const char *InputName,
                                   const char *OutputName) {
    std::string Str;
    raw_string_ostream OS(Str);
    T.Print(OS, InputName, OutputName);
    OS.flush();
    return Str;
  }
};

bool ViewIDTableAndMaskMismatched(const PSVDependencyTable &PSVTable,
                                  std::vector<uint32_t> &VecPSVTable,
                                  const PSVComponentMask &&PSVMask,
                                  MutableArrayRef<uint32_t> ArrayMask,
                                  uint32_t NumInputVectors,
                                  uint32_t NumOutputVectors, bool UsesViewID) {
  if (NumInputVectors > 0 && NumOutputVectors > 0) {
    const PSVDependencyTable DxilTable(VecPSVTable.data(), NumInputVectors,
                                       NumOutputVectors);
    if (PSVTable != DxilTable)
      return true;
  }

  if (UsesViewID && NumOutputVectors > 0) {
    PSVComponentMask DxilMask(ArrayMask.data(), NumOutputVectors);
    if (PSVMask != DxilMask)
      return true;
  }
  return false;
}

void PSVContentVerifier::VerifyViewIDDependence(PSVRuntimeInfo1 *PSV1) {
  SimpleViewIDState ViewIDState(DM.GetSerializedViewIdState(),
                                PSV.GetShaderKind(), PSV1->UsesViewID);

  if (!ViewIDState.IsValid) {
    EmitInvalidError("ViewIDState");
    return;
  }

  unsigned NumInputScalars = ViewIDState.NumInputSigScalars;
  auto &NumOutputScalars = ViewIDState.NumOutputSigScalars;
  unsigned NumPCOrPrimScalars = ViewIDState.NumPCOrPrimSigScalars;
  bool Mismatched = false;
  for (unsigned i = 0; i < DXIL::kNumOutputStreams; i++) {
    // NumInputScalars as input, NumOutputScalars[i] as output.
    uint32_t NumInputVectors = llvm::RoundUpToAlignment(NumInputScalars, 4) / 4;
    uint32_t NumOutputVectors =
        llvm::RoundUpToAlignment(NumOutputScalars[i], 4) / 4;
    Mismatched |= ViewIDTableAndMaskMismatched(
        PSV.GetInputToOutputTable(i), ViewIDState.InputToOutputTable[i],
        PSV.GetViewIDOutputMask(i), ViewIDState.ViewIDOutputMask[i],
        NumInputVectors, NumOutputVectors, PSV1->UsesViewID);
  }

  // NumInputScalars as input, NumPCOrPrimScalars as output.
  if (PSV1->ShaderStage == static_cast<uint8_t>(PSVShaderKind::Hull) ||
      PSV1->ShaderStage == static_cast<uint8_t>(PSVShaderKind::Mesh)) {
    uint32_t NumInputVectors = llvm::RoundUpToAlignment(NumInputScalars, 4) / 4;
    uint32_t NumOutputVectors =
        llvm::RoundUpToAlignment(NumPCOrPrimScalars, 4) / 4;
    Mismatched |= ViewIDTableAndMaskMismatched(
        PSV.GetInputToPCOutputTable(), ViewIDState.InputToPCOutputTable,
        PSV.GetViewIDPCOutputMask(), ViewIDState.ViewIDPCOutputMask,
        NumInputVectors, NumOutputVectors, PSV1->UsesViewID);
  }

  // NumPCOrPrimScalars as input, NumOutputScalars[0] as output.
  if (PSV1->ShaderStage == static_cast<uint8_t>(PSVShaderKind::Domain)) {
    uint32_t NumInputVectors =
        llvm::RoundUpToAlignment(NumPCOrPrimScalars, 4) / 4;
    uint32_t NumOutputVectors =
        llvm::RoundUpToAlignment(NumOutputScalars[0], 4) / 4;
    Mismatched |= ViewIDTableAndMaskMismatched(
        PSV.GetPCInputToOutputTable(), ViewIDState.PCInputToOutputTable,
        PSVComponentMask(), ViewIDState.ViewIDOutputMask[0], NumInputVectors,
        NumOutputVectors, false);
  }
  if (Mismatched) {
    std::string Str;
    raw_string_ostream OS(Str);
    PSV.PrintViewIDState(OS);
    OS.flush();
    std::string Str1;
    raw_string_ostream OS1(Str1);
    ViewIDState.Print(OS1, static_cast<PSVShaderKind>(PSV1->ShaderStage),
                      PSV1->UsesViewID);
    OS1.flush();
    EmitMismatchError("ViewIDState", Str, Str1);
  }
}

void PSVContentVerifier::VerifySignatures(unsigned ValMajor,
                                          unsigned ValMinor) {
  bool i1ToUnknownCompat = DXIL::CompareVersions(ValMajor, ValMinor, 1, 5) < 0;
  // Verify input signature
  VerifySignature(DM.GetInputSignature(), PSV.GetInputElement0(0),
                  PSV.GetSigInputElements(), "SigInput", i1ToUnknownCompat);
  // Verify output signature
  VerifySignature(DM.GetOutputSignature(), PSV.GetOutputElement0(0),
                  PSV.GetSigOutputElements(), "SigOutput", i1ToUnknownCompat);
  // Verify patch constant signature
  VerifySignature(DM.GetPatchConstOrPrimSignature(),
                  PSV.GetPatchConstOrPrimElement0(0),
                  PSV.GetSigPatchConstOrPrimElements(),
                  "SigPatchConstantOrPrim", i1ToUnknownCompat);
}

void PSVContentVerifier::VerifySignature(const DxilSignature &Sig,
                                         PSVSignatureElement0 *Base,
                                         unsigned Count, std::string Name,
                                         bool i1ToUnknownCompat) {
  if (Count != Sig.GetElements().size()) {
    EmitMismatchError(Name + "Elements", std::to_string(Count),
                      std::to_string(Sig.GetElements().size()));
    return;
  }

  ArrayRef<PSVSignatureElement0> Inputs(Base, Count);
  // Build PSVSignatureSet.
  PSVSignatureSet SigSet;
  for (unsigned i = 0; i < Count; i++) {
    if (SigSet.insert(Base + i).second == false) {
      PSVSignatureElement PSVSE(PSV.GetStringTable(),
                                PSV.GetSemanticIndexTable(), Base + i);
      std::string Str = GetDump(PSVSE);
      EmitDuplicateError(Name + "Element", Str);
      return;
    }
  }
  // Verify each element.
  const PSVStringTable &StrTab = PSV.GetStringTable();
  const PSVSemanticIndexTable &IndexTab = PSV.GetSemanticIndexTable();
  for (unsigned i = 0; i < Count; i++) {
    VerifySignatureElement(Sig.GetElement(i), SigSet, StrTab, IndexTab, Name,
                           i1ToUnknownCompat);
  }
}

static uint32_t
findSemanticIndex(const PSVSemanticIndexTable &IndexTab,
                  const std::vector<unsigned> &SemanticIndexVec) {
  for (uint32_t i = 0; i < IndexTab.Entries; i++) {
    const uint32_t *PSVSemanticIndexVec = IndexTab.Get(i);
    bool Match = true;
    for (unsigned i = 0; i < SemanticIndexVec.size(); i++) {
      if (PSVSemanticIndexVec[i] != SemanticIndexVec[i]) {
        Match = false;
        break;
      }
    }
    if (Match)
      return i;
  }
  return UINT32_MAX;
}

void PSVContentVerifier::VerifySignatureElement(
    const DxilSignatureElement &SE, PSVSignatureSet &SigSet,
    const PSVStringTable &StrTab, const PSVSemanticIndexTable &IndexTab,
    std::string Name, bool i1ToUnknownCompat) {
  // Find the signature element in the set.
  PSVSignatureElement0 PSVSE0;
  InitPSVSignatureElement(PSVSE0, SE, i1ToUnknownCompat);

  PSVSE0.SemanticIndexes =
      findSemanticIndex(IndexTab, SE.GetSemanticIndexVec());

  auto it = SigSet.find(&PSVSE0);
  if (it == SigSet.end()) {
    PSVSignatureElement PSVSE(StrTab, IndexTab, &PSVSE0);
    std::string Str;
    raw_string_ostream OS(Str);
    PSVSE.Print(OS, SE.GetName());
    OS.flush();
    EmitMissingError(Name + "Element", Str);
    return;
  }
  // Check the Name and SemanticIndex.
  bool Mismatch = false;
  PSVSignatureElement0 *PSVSE0Found = *it;
  PSVSignatureElement PSVSEFound(StrTab, IndexTab, PSVSE0Found);
  if (SE.IsArbitrary())
    Mismatch = strcmp(PSVSEFound.GetSemanticName(), SE.GetName());
  else
    Mismatch = PSVSE0Found->SemanticKind != static_cast<uint8_t>(SE.GetKind());

  PSVSE0.SemanticName = PSVSE0Found->SemanticName;
  // Compare all fields.
  Mismatch |= memcmp(&PSVSE0, PSVSE0Found, sizeof(PSVSignatureElement0)) != 0;
  if (Mismatch) {
    PSVSignatureElement PSVSE(StrTab, IndexTab, &PSVSE0);
    std::string Str = GetDump(PSVSE);
    std::string Str1;
    raw_string_ostream OS(Str1);
    PSVSEFound.Print(OS, SE.GetName());
    OS.flush();
    EmitMismatchError(Name + "Element", Str, Str1);
  }
}

template <typename T>
void PSVContentVerifier::VerifyResourceTable(T &ResTab, PSVResourceSet &ResSet,
                                             unsigned &ResIndex,
                                             unsigned PSVVersion) {
  for (auto &&R : ResTab) {
    PSVResourceBindInfo1 BI;
    InitPSVResourceBinding(&BI, &BI, R.get());
    auto It = ResSet.find(&BI);
    if (It == ResSet.end()) {
      std::string Str = GetDump(BI);
      EmitMissingError("ResourceBindInfo", Str);
      return;
    }

    if (PSVVersion > 1) {
      PSVResourceBindInfo1 *BindInfo1 = (PSVResourceBindInfo1 *)*It;
      if (memcmp(&BI, BindInfo1, sizeof(PSVResourceBindInfo1)) != 0) {
        std::string Str = GetDump(BI);
        std::string Str1 = GetDump(*BindInfo1);
        EmitMismatchError("ResourceBindInfo", Str, Str1);
      }
    } else {
      PSVResourceBindInfo0 *BindInfo = *It;
      if (memcmp(&BI, BindInfo, sizeof(PSVResourceBindInfo0)) != 0) {
        std::string Str = GetDump(BI);
        std::string Str1 = GetDump(*BindInfo);
        EmitMismatchError("ResourceBindInfo", Str, Str1);
      }
    }
    ResIndex++;
  }
}

void PSVContentVerifier::VerifyResources(unsigned PSVVersion) {
  UINT uCBuffers = DM.GetCBuffers().size();
  UINT uSamplers = DM.GetSamplers().size();
  UINT uSRVs = DM.GetSRVs().size();
  UINT uUAVs = DM.GetUAVs().size();
  unsigned ResourceCount = uCBuffers + uSamplers + uSRVs + uUAVs;
  if (PSV.GetBindCount() != ResourceCount) {
    EmitMismatchError("ResourceCount", std::to_string(PSV.GetBindCount()),
                      std::to_string(ResourceCount));
    return;
  }
  unsigned ResIndex = 0;
  PSVResourceSet ResBindInfo0Set;
  for (unsigned i = 0; i < ResourceCount; i++) {
    PSVResourceBindInfo0 *BindInfo = PSV.GetPSVResourceBindInfo0(i);
    if (!ResBindInfo0Set.insert(BindInfo).second) {
      std::string Str = GetDump(*BindInfo);
      EmitDuplicateError("ResourceBindInfo", Str);
      return;
    }
  }

  // CBV
  VerifyResourceTable(DM.GetCBuffers(), ResBindInfo0Set, ResIndex, PSVVersion);
  // Sampler
  VerifyResourceTable(DM.GetSamplers(), ResBindInfo0Set, ResIndex, PSVVersion);
  // SRV
  VerifyResourceTable(DM.GetSRVs(), ResBindInfo0Set, ResIndex, PSVVersion);
  // UAV
  VerifyResourceTable(DM.GetUAVs(), ResBindInfo0Set, ResIndex, PSVVersion);
}

void PSVContentVerifier::VerifyEntryProperties(const ShaderModel *SM,
                                               PSVRuntimeInfo0 *PSV0,
                                               PSVRuntimeInfo1 *PSV1,
                                               PSVRuntimeInfo2 *PSV2) {
  PSVRuntimeInfo3 DMPSV;
  memset(&DMPSV, 0, sizeof(PSVRuntimeInfo3));
  hlsl::InitPSVRuntimeInfo(&DMPSV, &DMPSV, &DMPSV, &DMPSV, DM);
  if (PSV1) {
    // Init things not set in InitPSVRuntimeInfo.
    DMPSV.ShaderStage = static_cast<uint8_t>(SM->GetKind());
    DMPSV.SigInputElements = DM.GetInputSignature().GetElements().size();
    DMPSV.SigOutputElements = DM.GetOutputSignature().GetElements().size();
    DMPSV.SigPatchConstOrPrimElements =
        DM.GetPatchConstOrPrimSignature().GetElements().size();
    // Set up ViewID and signature dependency info
    DMPSV.UsesViewID = DM.m_ShaderFlags.GetViewID() ? true : false;
    DMPSV.SigInputVectors = DM.GetInputSignature().NumVectorsUsed(0);
    for (unsigned streamIndex = 0; streamIndex < 4; streamIndex++)
      DMPSV.SigOutputVectors[streamIndex] =
          DM.GetOutputSignature().NumVectorsUsed(streamIndex);
    if (SM->IsHS() || SM->IsDS() || SM->IsMS())
      DMPSV.SigPatchConstOrPrimVectors =
          DM.GetPatchConstOrPrimSignature().NumVectorsUsed(0);
  }
  bool Mismatched = false;
  if (PSV2) {
    Mismatched = memcmp(PSV2, &DMPSV, sizeof(PSVRuntimeInfo2)) != 0;
  } else if (PSV1) {
    Mismatched = memcmp(PSV1, &DMPSV, sizeof(PSVRuntimeInfo1)) != 0;
  } else {
    Mismatched = memcmp(PSV0, &DMPSV, sizeof(PSVRuntimeInfo0)) != 0;
  }
  if (Mismatched) {
    std::string Str;
    raw_string_ostream OS(Str);
    hlsl::PrintPSVRuntimeInfo(OS, &DMPSV, &DMPSV, &DMPSV, &DMPSV,
                              static_cast<uint8_t>(SM->GetKind()),
                              DM.GetEntryFunctionName().c_str(), "");
    OS.flush();
    std::string Str1;
    raw_string_ostream OS1(Str1);
    PSV.PrintPSVRuntimeInfo(OS1, static_cast<uint8_t>(PSVShaderKind::Library),
                            "");
    OS1.flush();
    EmitMismatchError("PSVRuntimeInfo", Str, Str1);
  }
}

void PSVContentVerifier::Verify() {
  unsigned ValMajor, ValMinor;
  DM.GetValidatorVersion(ValMajor, ValMinor);
  unsigned PSVVersion = MAX_PSV_VERSION;
  // Constraint PSVVersion based on validator version
  if (DXIL::CompareVersions(ValMajor, ValMinor, 1, 1) < 0)
    PSVVersion = 0;
  else if (DXIL::CompareVersions(ValMajor, ValMinor, 1, 6) < 0)
    PSVVersion = 1;
  else if (DXIL::CompareVersions(ValMajor, ValMinor, 1, 8) < 0)
    PSVVersion = 2;

  VerifyResources(PSVVersion);

  PSVRuntimeInfo0 *PSV0 = PSV.GetPSVRuntimeInfo0();
  if (!PSV0) {
    EmitMismatchError("PSVRuntimeInfo0", "null", "non-null");
    return;
  }
  PSVRuntimeInfo1 *PSV1 = PSV.GetPSVRuntimeInfo1();
  if (!PSV1 && PSVVersion > 0) {
    EmitMismatchError("PSVRuntimeInfo1", "null", "non-null");
    return;
  }
  PSVRuntimeInfo2 *PSV2 = PSV.GetPSVRuntimeInfo2();
  if (!PSV2 && PSVVersion > 1) {
    EmitMismatchError("PSVRuntimeInfo2", "null", "non-null");
    return;
  }
  PSVRuntimeInfo3 *PSV3 = PSV.GetPSVRuntimeInfo3();
  if (!PSV3 && PSVVersion > 2) {
    EmitMismatchError("PSVRuntimeInfo3", "null", "non-null");
    return;
  }

  const ShaderModel *SM = DM.GetShaderModel();
  VerifyEntryProperties(SM, PSV0, PSV1, PSV2);
  if (PSVVersion > 0) {
    uint8_t ShaderStage = static_cast<uint8_t>(SM->GetKind());
    PSVRuntimeInfo1 *PSV1 = PSV.GetPSVRuntimeInfo1();
    if (PSV1->ShaderStage != ShaderStage) {
      EmitMismatchError("ShaderStage", std::to_string(PSV1->ShaderStage),
                        std::to_string(ShaderStage));
      return;
    }
    if (PSV1->UsesViewID != DM.m_ShaderFlags.GetViewID())
      EmitMismatchError("UsesViewID", std::to_string(PSV1->UsesViewID),
                        std::to_string(DM.m_ShaderFlags.GetViewID()));

    VerifySignatures(ValMajor, ValMinor);

    VerifyViewIDDependence(PSV1);
  }
  if (PSVVersion > 1) {
    // PSV2 only added NumThreadsX/Y/Z which verified in VerifyEntryProperties.
  }
  if (PSVVersion > 2) {
    if (DM.GetEntryFunctionName() != PSV.GetEntryFunctionName())
      EmitMismatchError("EntryFunctionName", DM.GetEntryFunctionName(),
                        PSV.GetEntryFunctionName());
  }

  if (!PSVContentValid)
    ValCtx.EmitFormatError(ValidationRule::ContainerPartMatches,
                           {"Pipeline State Validation"});
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
  DxilPipelineStateValidation PSV;
  if (!PSV.InitFromPSV0(pPSVData, PSVSize)) {
    ValCtx.EmitFormatError(ValidationRule::ContainerPartMatches,
                           {"Pipeline State Validation"});
    return;
  }
  PSVContentVerifier Verifier(PSV, ValCtx.DxilMod, ValCtx);
  Verifier.Verify();
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
