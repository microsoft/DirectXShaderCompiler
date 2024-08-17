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

  ArrayRef<uint32_t> InputToOutputTable[DXIL::kNumOutputStreams];
  ArrayRef<uint32_t> InputToPCOutputTable;
  ArrayRef<uint32_t> PCInputToOutputTable;
  ArrayRef<uint32_t> ViewIDOutputMask[DXIL::kNumOutputStreams];
  ArrayRef<uint32_t> ViewIDPCOutputMask;
  bool IsValid = true;
  SimpleViewIDState(std::vector<uint32_t> &Data, PSVShaderKind,
                    bool UsesViewID);
};
static unsigned RoundUpToUINT(unsigned x) { return (x + 31) / 32; }
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
    unsigned MaskDwords = RoundUpToUINT(OutputScalars);
    if (UsesViewID) {
      ViewIDOutputMask[i] = {Data.data() + Offset, MaskDwords};
      Offset += MaskDwords;
    }
    unsigned TabSize = MaskDwords * NumInputSigScalars;
    InputToOutputTable[i] = {Data.data() + Offset, TabSize};
    Offset += TabSize;
  }
  if (SK == PSVShaderKind::Hull || SK == PSVShaderKind::Mesh) {
    // #PatchConstant.
    NumPCOrPrimSigScalars = Data[Offset++];
    unsigned MaskDwords = RoundUpToUINT(NumPCOrPrimSigScalars);
    if (UsesViewID) {
      ViewIDPCOutputMask = {Data.data() + Offset, MaskDwords};
      Offset += MaskDwords;
    }
    unsigned TabSize = MaskDwords * NumInputSigScalars;
    InputToPCOutputTable = {Data.data() + Offset, TabSize};
    Offset += TabSize;
  } else if (SK == PSVShaderKind::Domain) {
    // #PatchConstant.
    NumPCOrPrimSigScalars = Data[Offset++];
    unsigned OutputScalars = NumOutputSigScalars[0];
    unsigned MaskDwords = RoundUpToUINT(OutputScalars);
    unsigned TabSize = MaskDwords * NumPCOrPrimSigScalars;
    PCInputToOutputTable = {Data.data() + Offset, TabSize};
    Offset += TabSize;
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
                       unsigned Count, bool i1ToUnknownCompat);
  void VerifySignatureElement(const DxilSignatureElement &, PSVSignatureSet &,
                              const PSVStringTable &,
                              const PSVSemanticIndexTable &, bool);
  void VerifyResources(unsigned PSVVersion);
  template <typename T>
  void VerifyResourceTable(T &ResTab, PSVResourceSet &ResSet,
                           unsigned &ResIndex, unsigned PSVVersion);
  void VerifyViewIDDependence(PSVRuntimeInfo1 *PSV1);
  void VerifyViewIDMask(PSVComponentMask &&, ArrayRef<uint32_t> &,
                        unsigned NumOutputScalars);
  void VerifyDependencyTable(PSVDependencyTable &&, ArrayRef<uint32_t> &,
                             unsigned NumInputScalars,
                             unsigned NumOutputScalars);
  void VerifyEntryProperties(const ShaderModel *SM, PSVRuntimeInfo0 *PSV0,
                             PSVRuntimeInfo1 *PSV1, PSVRuntimeInfo2 *PSV2);
  void EmitFormatError(StringRef Name) {
#ifdef _DEBUG
    // Only emit detailed error for debugging.
    std::string ErrorMsg = Name.str() + " does not match";
    ValCtx.EmitFormatError(ValidationRule::ContainerPartMatches, {ErrorMsg});
#endif
    PSVContentValid = false;
  }
  void EmitError(StringRef ErrorMsg) {
#ifdef _DEBUG
    // Only emit detailed error for debugging.
    ValCtx.EmitFormatError(ValidationRule::ContainerPartMatches, {ErrorMsg});
#endif
    PSVContentValid = false;
  }
};

void PSVContentVerifier::VerifyDependencyTable(PSVDependencyTable &&Tab,
                                               ArrayRef<uint32_t> &Mem,
                                               unsigned NumInputScalars,
                                               unsigned NumOutputScalars) {
  if (Tab.InputVectors == 0 && Tab.OutputVectors == 0 && Mem.empty())
    return;

  if (Tab.InputVectors != (PSVALIGN4(NumInputScalars) >> 2))
    EmitFormatError("Number of InputVectors for PSVDependencyTable");
  if (Tab.OutputVectors != (PSVALIGN4(NumOutputScalars) >> 2))
    EmitFormatError("Number of OutputVectors for PSVDependencyTable");

  if (Mem.size() != NumInputScalars * RoundUpToUINT(NumOutputScalars)) {
    EmitFormatError("Size of PSVDependencyTable");
    return;
  }
  if (memcmp(Mem.data(), Tab.Table, 4 * Mem.size()) != 0) {
    EmitFormatError("PSVDependencyTable");
  }
}

void PSVContentVerifier::VerifyViewIDMask(PSVComponentMask &&Mask,
                                          ArrayRef<uint32_t> &Mem,
                                          unsigned NumOutputScalars) {
  if (Mask.NumVectors == 0 && Mem.empty())
    return;

  if (Mask.NumVectors != (PSVALIGN4(NumOutputScalars) >> 2))
    EmitFormatError("NumVectors of PSVComponentMask");

  if (Mem.size() != RoundUpToUINT(NumOutputScalars)) {
    EmitFormatError("Size of PSVComponentMask");
    return;
  }
  if (memcmp(Mem.data(), Mask.Mask, 4 * Mem.size()) != 0) {
    EmitFormatError("PSVComponentMask");
  }
}

static void getNumScalarsInSignature(DxilSignature &DxilSig,
                                     unsigned *NumScalars,
                                     unsigned NumStreams) {
  DXASSERT_NOMSG(NumStreams == 1 || NumStreams == DXIL::kNumOutputStreams);

  for (unsigned i = 0; i < NumStreams; i++)
    NumScalars[i] = 0;

  for (auto &E : DxilSig.GetElements()) {
    if (E->GetStartRow() == Semantic::kUndefinedRow)
      continue;

    unsigned StreamId = E->GetOutputStream();
    unsigned Offset = (((unsigned)E->GetRows() - 1) + E->GetStartRow()) * 4 +
                      (E->GetCols() - 1) + E->GetStartCol();

    NumScalars[StreamId] = std::max(NumScalars[StreamId], Offset + 1);
  }
}

void PSVContentVerifier::VerifyViewIDDependence(PSVRuntimeInfo1 *PSV1) {
  if (PSV1->SigInputVectors != DM.GetInputSignature().NumVectorsUsed())
    EmitFormatError("SigInputVectors");

  unsigned SigPatchConstOrPrimVectors = 0;
  if (PSV.IsHS() || PSV.IsDS() || PSV.IsMS())
    SigPatchConstOrPrimVectors =
        DM.GetPatchConstOrPrimSignature().NumVectorsUsed();

  if (!PSV.IsGS() &&
      PSV1->SigPatchConstOrPrimVectors != SigPatchConstOrPrimVectors)
    EmitFormatError("SigPatchConstOrPrimVectors");

  for (int i = 0; i < 4; i++) {
    if (PSV1->SigOutputVectors[i] !=
        DM.GetOutputSignature().NumVectorsUsed(i)) {
      Twine Name = "SigOutputVectors[" + Twine(i) + "]";
      EmitFormatError(Name.str());
    }
  }

  SimpleViewIDState ViewIDState(DM.GetSerializedViewIdState(),
                                PSV.GetShaderKind(), PSV1->UsesViewID);

  if (!ViewIDState.IsValid) {
    EmitError("ViewIDState in DXIL module is invalid");
    return;
  }

  unsigned NumInputScalars = 0;
  getNumScalarsInSignature(DM.GetInputSignature(), &NumInputScalars, 1);
  unsigned NumOutputScalars[DXIL::kNumOutputStreams] = {0, 0, 0, 0};
  getNumScalarsInSignature(DM.GetOutputSignature(), NumOutputScalars,
                           PSV.IsGS() ? 4 : 1);
  unsigned NumPCOrPrimScalars = 0;
  getNumScalarsInSignature(DM.GetPatchConstOrPrimSignature(),
                           &NumPCOrPrimScalars, 1);

  for (unsigned i = 0; i < DXIL::kNumOutputStreams; i++) {
    // NumInputScalars as input, NumOutputScalars[i] as output.
    VerifyDependencyTable(PSV.GetInputToOutputTable(i),
                          ViewIDState.InputToOutputTable[i], NumInputScalars,
                          NumOutputScalars[i]);
    VerifyViewIDMask(PSV.GetViewIDOutputMask(i),
                     ViewIDState.ViewIDOutputMask[i], NumOutputScalars[i]);
  }

  // NumInputScalars as input, NumPCOrPrimScalars as output.
  VerifyDependencyTable(PSV.GetInputToPCOutputTable(),
                        ViewIDState.InputToPCOutputTable, NumInputScalars,
                        NumPCOrPrimScalars);

  VerifyViewIDMask(PSV.GetViewIDPCOutputMask(), ViewIDState.ViewIDPCOutputMask,
                   NumPCOrPrimScalars);

  // NumPCOrPrimScalars as input, NumOutputScalars[0] as output.
  VerifyDependencyTable(PSV.GetPCInputToOutputTable(),
                        ViewIDState.PCInputToOutputTable, NumPCOrPrimScalars,
                        NumOutputScalars[0]);
}

void PSVContentVerifier::VerifySignatures(unsigned ValMajor,
                                          unsigned ValMinor) {
  bool i1ToUnknownCompat = DXIL::CompareVersions(ValMajor, ValMinor, 1, 5) < 0;
  // Verify input signature
  VerifySignature(DM.GetInputSignature(), PSV.GetInputElement0(0),
                  PSV.GetSigInputElements(), i1ToUnknownCompat);
  // Verify output signature
  VerifySignature(DM.GetOutputSignature(), PSV.GetOutputElement0(0),
                  PSV.GetSigOutputElements(), i1ToUnknownCompat);
  // Verify patch constant signature
  VerifySignature(DM.GetPatchConstOrPrimSignature(),
                  PSV.GetPatchConstOrPrimElement0(0),
                  PSV.GetSigPatchConstOrPrimElements(), i1ToUnknownCompat);
}

void PSVContentVerifier::VerifySignature(const DxilSignature &Sig,
                                         PSVSignatureElement0 *Base,
                                         unsigned Count,
                                         bool i1ToUnknownCompat) {
  if (Count != Sig.GetElements().size()) {
    EmitFormatError("SigInputElements");
    return;
  }

  ArrayRef<PSVSignatureElement0> Inputs(Base, Count);
  // Build PSVSignatureSet.
  PSVSignatureSet SigSet;
  for (unsigned i = 0; i < Count; i++) {
    if (SigSet.insert(Base + i).second == false) {

      EmitFormatError("SignatureElement duplicate");
      return;
    }
  }
  // Verify each element.
  const PSVStringTable &StrTab = PSV.GetStringTable();
  const PSVSemanticIndexTable &IndexTab = PSV.GetSemanticIndexTable();
  for (unsigned i = 0; i < Count; i++) {
    VerifySignatureElement(Sig.GetElement(i), SigSet, StrTab, IndexTab,
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
    bool i1ToUnknownCompat) {
  // Find the signature element in the set.
  PSVSignatureElement0 PSVSE0;
  initPSVSignatureElement(PSVSE0, SE, i1ToUnknownCompat);

  PSVSE0.SemanticIndexes =
      findSemanticIndex(IndexTab, SE.GetSemanticIndexVec());

  auto it = SigSet.find(&PSVSE0);
  if (it == SigSet.end()) {
    EmitFormatError("SignatureElement missing");
    return;
  }
  // Check the Name and SemanticIndex.
  PSVSignatureElement0 *PSVSE0Found = *it;
  PSVSignatureElement PSVSEFound(StrTab, IndexTab, PSVSE0Found);
  if (SE.IsArbitrary()) {
    if (strcmp(PSVSEFound.GetSemanticName(), SE.GetName()))
      EmitFormatError("SignatureElement.SemanticName");
  } else {
    if (PSVSE0Found->SemanticKind != static_cast<uint8_t>(SE.GetKind()))
      EmitFormatError("SignatureElement.SemanticKind");
  }
  PSVSE0.SemanticName = PSVSE0Found->SemanticName;
  // Compare all fields.
  if (memcmp(&PSVSE0, PSVSE0Found, sizeof(PSVSignatureElement0)) != 0)
    EmitFormatError("SignatureElement mismatch");
}

template <typename T>
void PSVContentVerifier::VerifyResourceTable(T &ResTab, PSVResourceSet &ResSet,
                                             unsigned &ResIndex,
                                             unsigned PSVVersion) {
  for (auto &&R : ResTab) {
    PSVResourceBindInfo1 BI;
    initPSVResourceBinding(&BI, &BI, R.get());
    auto It = ResSet.find(&BI);
    if (It == ResSet.end()) {
      EmitFormatError("ResourceBindInfo missing");
      return;
    }

    if (PSVVersion > 1) {
      PSVResourceBindInfo1 *BindInfo1 = (PSVResourceBindInfo1 *)*It;
      if (memcmp(&BI, BindInfo1, sizeof(PSVResourceBindInfo1)) != 0)
        EmitFormatError("ResourceBindInfo1 mismatch");
    } else {
      PSVResourceBindInfo0 *BindInfo = *It;
      if (memcmp(&BI, BindInfo, sizeof(PSVResourceBindInfo0)) != 0)
        EmitFormatError("ResourceBindInfo mismatch");
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
    EmitFormatError("ResourceCount");
    return;
  }
  unsigned ResIndex = 0;
  PSVResourceSet ResBindInfo0Set;
  for (unsigned i = 0; i < ResourceCount; i++) {
    PSVResourceBindInfo0 *BindInfo = PSV.GetPSVResourceBindInfo0(i);
    if (!ResBindInfo0Set.insert(BindInfo).second) {
      EmitFormatError("ResourceBindInfo0 duplicate");
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

static char getOutputPositionPresent(DxilModule &DM) {
  char OutputPositionPresent = 0;
  DxilSignature &S = DM.GetOutputSignature();
  for (auto &&E : S.GetElements()) {
    if (E->GetKind() == Semantic::Kind::Position) {
      // Ideally, we might check never writes mask here,
      // but this is not yet part of the signature element in Dxil
      OutputPositionPresent = 1;
      break;
    }
  }
  return OutputPositionPresent;
}

void PSVContentVerifier::VerifyEntryProperties(const ShaderModel *SM,
                                               PSVRuntimeInfo0 *PSV0,
                                               PSVRuntimeInfo1 *PSV1,
                                               PSVRuntimeInfo2 *PSV2) {
  switch (SM->GetKind()) {
  case DXIL::ShaderKind::Vertex: {
    VSInfo &VS = PSV0->VS;
    char OutputPositionPresent = getOutputPositionPresent(DM);
    if (OutputPositionPresent != VS.OutputPositionPresent)
      EmitFormatError("VS.OutputPositionPresent");
  } break;
  case DXIL::ShaderKind::Hull: {
    HSInfo &HS = PSV0->HS;
    if (HS.InputControlPointCount != DM.GetInputControlPointCount())
      EmitFormatError("HS.InputControlPointCount");
    if (HS.OutputControlPointCount != DM.GetOutputControlPointCount())
      EmitFormatError("HS.OutputControlPointCount");
    if (HS.TessellatorDomain !=
        static_cast<uint32_t>(DM.GetTessellatorDomain()))
      EmitFormatError("HS.TessellatorDomain");
    if (HS.TessellatorOutputPrimitive !=
        static_cast<uint32_t>(DM.GetTessellatorOutputPrimitive()))
      EmitFormatError("HS.TessellatorOutputPrimitive");
  } break;
  case DXIL::ShaderKind::Domain: {
    DSInfo &DS = PSV0->DS;
    if (DS.TessellatorDomain !=
        static_cast<uint32_t>(DM.GetTessellatorDomain()))
      EmitFormatError("DS.TessellatorDomain");
    if (DS.InputControlPointCount !=
        static_cast<uint32_t>(DM.GetInputControlPointCount()))
      EmitFormatError("DS.InputControlPointCount");

    char OutputPositionPresent = getOutputPositionPresent(DM);
    if (OutputPositionPresent != DS.OutputPositionPresent)
      EmitFormatError("DS.OutputPositionPresent");
  } break;
  case DXIL::ShaderKind::Geometry: {
    GSInfo &GS = PSV0->GS;
    if (GS.InputPrimitive != static_cast<uint32_t>(DM.GetInputPrimitive()))
      EmitFormatError("GS.InputPrimitive");
    if (GS.OutputTopology !=
        static_cast<uint32_t>(DM.GetStreamPrimitiveTopology()))
      EmitFormatError("GS.OutputTopology");
    // NOTE: For OutputTopology, pick one from a used stream, or if none
    // are used, use stream 0, and set OutputStreamMask to 1.
    unsigned OutputStreamMask = DM.GetActiveStreamMask();
    if (OutputStreamMask == 0)
      OutputStreamMask = 1; // This is what runtime expects.
    if (GS.OutputStreamMask != OutputStreamMask)
      EmitFormatError("GS.OutputStreamMask");
    char OutputPositionPresent = getOutputPositionPresent(DM);
    if (OutputPositionPresent != GS.OutputPositionPresent)
      EmitFormatError("GS.OutputPositionPresent");
    if (PSV1) {
      if (PSV.IsGS() && PSV1->MaxVertexCount != DM.GetMaxVertexCount())
        EmitFormatError("GS.InstanceCount");
    }
  } break;
  case DXIL::ShaderKind::Pixel: {
    PSInfo &PS = PSV0->PS;
    char DepthOutput = 0;
    char SampleFrequency = 0;
    {
      DxilSignature &S = DM.GetInputSignature();
      for (auto &&E : S.GetElements()) {
        if (E->GetInterpolationMode()->IsAnySample() ||
            E->GetKind() == Semantic::Kind::SampleIndex) {
          SampleFrequency = 1;
          break;
        }
      }
    }
    {
      DxilSignature &S = DM.GetOutputSignature();
      for (auto &&E : S.GetElements()) {
        if (E->IsAnyDepth()) {
          DepthOutput = 1;
          break;
        }
      }
    }
    if (DepthOutput != PS.DepthOutput)
      EmitFormatError("PS.DepthOutput");
    if (SampleFrequency != PS.SampleFrequency)
      EmitFormatError("PS.SampleFrequency");
  } break;
  case DXIL::ShaderKind::Compute: {
    DxilWaveSize WaveSize = DM.GetWaveSize();
    unsigned MinimumExpectedWaveLaneCount = 0;
    unsigned MaximumExpectedWaveLaneCount = UINT32_MAX;
    if (WaveSize.IsDefined()) {
      MinimumExpectedWaveLaneCount = WaveSize.Min;
      MaximumExpectedWaveLaneCount =
          WaveSize.IsRange() ? WaveSize.Max : WaveSize.Min;
    }
    if (PSV0->MinimumExpectedWaveLaneCount != MinimumExpectedWaveLaneCount)
      EmitFormatError("MinimumExpectedWaveLaneCount");
    if (PSV0->MaximumExpectedWaveLaneCount != MaximumExpectedWaveLaneCount)
      EmitFormatError("MaximumExpectedWaveLaneCount");

  } break;
  case ShaderModel::Kind::Library:
  case ShaderModel::Kind::Invalid:
    // Library and Invalid not relevant to PSVRuntimeInfo0
    break;
  case ShaderModel::Kind::Mesh: {
    MSInfo &MS = PSV0->MS;
    if (MS.PayloadSizeInBytes != DM.GetPayloadSizeInBytes())
      EmitFormatError("MS.PayloadSizeInBytes");
    if (MS.MaxOutputPrimitives != DM.GetMaxOutputPrimitives())
      EmitFormatError("MS.GetMaxOutputPrimitives");
    if (MS.MaxOutputVertices != DM.GetMaxOutputVertices())
      EmitFormatError("MS.GetMaxOutputVertices");
    {
      Module *M = DM.GetModule();
      const DataLayout &DL = M->getDataLayout();
      unsigned GroupSharedBytesUsed = 0;
      for (GlobalVariable &GV : M->globals()) {
        PointerType *PtrType = cast<PointerType>(GV.getType());
        if (PtrType->getAddressSpace() == hlsl::DXIL::kTGSMAddrSpace) {
          Type *Ty = PtrType->getPointerElementType();
          unsigned ByteSize = DL.getTypeAllocSize(Ty);
          GroupSharedBytesUsed += ByteSize;
        }
      }
      if (MS.GroupSharedBytesUsed != GroupSharedBytesUsed)
        EmitFormatError("MS.GroupSharedBytesUsed");
    }
    if (PSV1) {
      if (PSV1->MS1.MeshOutputTopology !=
          static_cast<uint32_t>(DM.GetMeshOutputTopology()))
        EmitFormatError("MS.MeshOutputTopology");
    }
  } break;
  case ShaderModel::Kind::Amplification: {
    if (PSV0->AS.PayloadSizeInBytes != DM.GetPayloadSizeInBytes())
      EmitFormatError("AmplificationCount");
  } break;
  }

  if (PSV2) {
    switch (SM->GetKind()) {
    case ShaderModel::Kind::Compute:
    case ShaderModel::Kind::Mesh:
    case ShaderModel::Kind::Amplification:
      if (PSV2->NumThreadsX != DM.GetNumThreads(0))
        EmitFormatError("NumThreadsX");
      if (PSV2->NumThreadsY != DM.GetNumThreads(1))
        EmitFormatError("NumThreadsY");
      if (PSV2->NumThreadsZ != DM.GetNumThreads(2))
        EmitFormatError("NumThreadsZ");
      break;
    }
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
    EmitFormatError("PSVRuntimeInfo0"); // PSV0 is required
    return;
  }
  PSVRuntimeInfo1 *PSV1 = PSV.GetPSVRuntimeInfo1();
  if (!PSV1 && PSVVersion > 0) {
    EmitFormatError("PSVRuntimeInfo1"); // PSV1 is required for PSVVersion > 0
    return;
  }
  PSVRuntimeInfo2 *PSV2 = PSV.GetPSVRuntimeInfo2();
  if (!PSV2 && PSVVersion > 1) {
    EmitFormatError("PSVRuntimeInfo2"); // PSV2 is required for PSVVersion > 1
    return;
  }
  PSVRuntimeInfo3 *PSV3 = PSV.GetPSVRuntimeInfo3();
  if (!PSV3 && PSVVersion > 2) {
    EmitFormatError("PSVRuntimeInfo0"); // PSV3 is required for PSVVersion > 2
    return;
  }

  const ShaderModel *SM = DM.GetShaderModel();
  VerifyEntryProperties(SM, PSV0, PSV1, PSV2);
  if (PSVVersion > 0) {
    uint8_t ShaderStage = static_cast<uint8_t>(SM->GetKind());
    PSVRuntimeInfo1 *PSV1 = PSV.GetPSVRuntimeInfo1();
    if (PSV1->ShaderStage != ShaderStage) {
      EmitFormatError("ShaderStage");
      return;
    }
    if (PSV1->UsesViewID != DM.m_ShaderFlags.GetViewID()) {
      EmitFormatError("UsesViewID");
    }

    VerifySignatures(ValMajor, ValMinor);

    VerifyViewIDDependence(PSV1);
  }
  if (PSVVersion > 1) {
    // PSV2 only added NumThreadsX/Y/Z which verified in VerifyEntryProperties.
  }
  if (PSVVersion > 2) {
    if (DM.GetEntryFunctionName() != PSV.GetEntryFunctionName())
      EmitFormatError("EntryFunctionName");
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
