///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilPIXRayQueryLog.cpp                                                    //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/DXIL/DxilFunctionProps.h"
#include "dxc/DXIL/DxilInstructions.h"
#include "dxc/DXIL/DxilModule.h"
#include "dxc/DXIL/DxilOperations.h"
#include "dxc/DXIL/DxilResourceBinding.h"
#include "dxc/DXIL/DxilSignatureElement.h"
#include "dxc/DXIL/DxilUtil.h"
#include "dxc/DxilPIXPasses/DxilPIXPasses.h"
#include "dxc/HLSL/ComputeViewIdState.h"

#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/SSAUpdater.h"

#include "PixPassHelpers.h"

#include <algorithm>
#include <array>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace llvm;
using namespace hlsl;
using namespace PIXPassHelpers;

namespace {

constexpr uint32_t CounterUAVRegister = 2;
constexpr uint32_t LogUAVRegister = 3;
constexpr uint32_t CandidateCounterUAVRegister = 4;
constexpr uint32_t CandidateLogUAVRegister = 5;
constexpr uint32_t DefaultRegisterSpace = 0xFFFFFFFEu;
constexpr uint32_t DemandCounterOffset = 0;
constexpr uint32_t ReservationCounterOffset = 4;
constexpr uint32_t RecordStrideBytes = 64;
constexpr uint32_t CandidateRecordStrideBytes = 48;
// Query record dwords:
//   0-2 world origin, 3 tMin, 4-6 world direction, 7 committedT,
//   8-9 identity, 10 instance/mask, 11 primitive, 12 geometry/status,
//   13 trace-site/effective flags, 14 AS dynamic index, 15 header.
// Candidate record dwords:
//   0 candidateT, 1-2 barycentrics, 3 candidate type/proc-nonopaque,
//   4-5 identity, 6 candidate instance/mask, 7 candidate primitive,
//   8 candidate geometry/status, 9 trace-site/effective flags,
//   10 AS dynamic index, 11 header.
// Header bits 24-31 carry the per-thread trace invocation counter in both
// record kinds; together with identity and trace-site ID, this joins candidates
// back to their 64-byte query record.
constexpr uint32_t QueryRecordType = 1;
constexpr uint32_t CandidateRecordType = 2;
constexpr uint32_t RecordFormatVersion = 1;
constexpr uint32_t StateIdle = 0;
constexpr uint32_t StateTraced = 1;
constexpr uint32_t StateFinalized = 2;
constexpr uint32_t ReasonCompleted = 0;
constexpr uint32_t ReasonAborted = 1;
constexpr uint32_t ReasonRetraced = 2;
constexpr uint32_t ReasonExited = 3;
constexpr uint32_t RawCandidateTypeTriangle = 0;
constexpr uint32_t RawCandidateTypeProcedural = 1;
constexpr uint32_t CandidateProceduralNonOpaqueFlag = 0x100;
constexpr uint32_t GeomStatusHelper = 1u << 28;
constexpr uint32_t GeomStatusHelperUnknown = 1u << 29;
constexpr uint32_t GeomStatusIdentityInvalid = 1u << 30;
constexpr uint32_t GeomStatusIdentityOverflow = 1u << 31;
constexpr uint32_t UnknownIndex = 0xFFFFFFFFu;
constexpr uint32_t SampleHashSeed = 0x31415926u;

bool IsDxrShaderKind(DXIL::ShaderKind ShaderKind) {
  return ShaderKind == DXIL::ShaderKind::RayGeneration ||
         ShaderKind == DXIL::ShaderKind::ClosestHit ||
         ShaderKind == DXIL::ShaderKind::Miss ||
         ShaderKind == DXIL::ShaderKind::Callable ||
         ShaderKind == DXIL::ShaderKind::AnyHit ||
         ShaderKind == DXIL::ShaderKind::Intersection;
}

bool FunctionContainsDxilOp(Function *TargetFunction, DXIL::OpCode OpCode) {
  for (BasicBlock &Block : *TargetFunction) {
    for (Instruction &Instruction : Block) {
      auto *Call = dyn_cast<CallInst>(&Instruction);
      if (Call == nullptr || Call->getNumArgOperands() == 0) {
        continue;
      }
      Function *CalledFunction = Call->getCalledFunction();
      auto *Opcode = dyn_cast<ConstantInt>(Call->getArgOperand(0));
      if (CalledFunction != nullptr && Opcode != nullptr &&
          Opcode->getZExtValue() == static_cast<unsigned>(OpCode) &&
          CalledFunction->getName().startswith("dx.op.")) {
        return true;
      }
    }
  }
  return false;
}

enum class InstrumentationPointKind {
  BeforeTrace,
  AfterTrace,
  AfterProceed,
  AfterAbort,
  BeforeShaderExit,
  BeforeReturn
};

enum class PointerAliasKind { Equivalent, Distinct, MayAlias };

struct PassOptionsState {
  uint64_t MaxNumEntriesInLog = 1;
  uint32_t RoiMinX = 0;
  uint32_t RoiMinY = 0;
  uint32_t RoiMinZ = 0;
  uint32_t RoiMaxX = 0xFFFFFFFFu;
  uint32_t RoiMaxY = 0xFFFFFFFFu;
  uint32_t RoiMaxZ = 0xFFFFFFFFu;
  uint32_t SampleRate = 1;
  bool LogCandidates = false;
  uint32_t TraceSiteBase = 0;
  uint32_t SubCallIndex = 0;
  int UpstreamSVPositionRow = -1;
};

struct ShadowState {
  Value *State = nullptr;
  Value *TraceSiteId = nullptr;
  Value *TraceInvocation = nullptr;
  Value *InstanceMask = nullptr;
  Value *EffectiveRayFlags = nullptr;
  Value *AsDynamicIndex = nullptr;
  Value *OriginX = nullptr;
  Value *OriginY = nullptr;
  Value *OriginZ = nullptr;
  Value *RayTMin = nullptr;
  Value *DirectionX = nullptr;
  Value *DirectionY = nullptr;
  Value *DirectionZ = nullptr;
  Value *RayTMax = nullptr;
};

using ShadowValues = std::array<Value *, 14>;

static ShadowValues GetShadowValues(const ShadowState &Shadow) {
  return {{Shadow.State, Shadow.TraceSiteId, Shadow.TraceInvocation,
           Shadow.InstanceMask, Shadow.EffectiveRayFlags, Shadow.AsDynamicIndex,
           Shadow.OriginX, Shadow.OriginY, Shadow.OriginZ, Shadow.RayTMin,
           Shadow.DirectionX, Shadow.DirectionY, Shadow.DirectionZ,
           Shadow.RayTMax}};
}

static ShadowState CreateShadowState(const ShadowValues &Values) {
  return {Values[0],  Values[1],  Values[2],  Values[3], Values[4],
          Values[5],  Values[6],  Values[7],  Values[8], Values[9],
          Values[10], Values[11], Values[12], Values[13]};
}

static ShadowState CreateShadowSelect(IRBuilder<> &Builder, Value *Condition,
                                      const ShadowState &TrueShadow,
                                      const ShadowState &FalseShadow) {
  ShadowValues SelectedValues;
  ShadowValues TrueValues = GetShadowValues(TrueShadow);
  ShadowValues FalseValues = GetShadowValues(FalseShadow);
  for (size_t ValueIndex = 0; ValueIndex < SelectedValues.size();
       ++ValueIndex) {
    SelectedValues[ValueIndex] =
        Builder.CreateSelect(Condition, TrueValues[ValueIndex],
                             FalseValues[ValueIndex], "IrtSelectedShadow");
  }
  return CreateShadowState(SelectedValues);
}

struct TraceSiteInfo {
  uint32_t LocalOrdinal = 0;
  uint32_t TraceSiteId = 0;
  Function *ParentFunction = nullptr;
  std::string DebugLocation;
  std::string StageName;
  std::string EntryName;
  bool PartialLifecycle = true;
};

struct EmitAnnotationInfo {
  uint32_t TraceSiteId = 0;
  std::string EmitKind;
  std::string RecordKind;
  std::string ReasonKind;
  Function *ParentFunction = nullptr;
  std::string DebugLocation;
  std::string StageName;
  std::string EntryName;
};

struct FunctionContext {
  Function *TargetFunction = nullptr;
  CallInst *CounterUAVHandle = nullptr;
  CallInst *LogUAVHandle = nullptr;
  CallInst *CandidateCounterUAVHandle = nullptr;
  CallInst *CandidateLogUAVHandle = nullptr;
  Value *SharedTraceInvocation = nullptr;
  ShadowState FallbackShadow;
  std::map<Value *, ShadowState> ShadowsByKey;
  std::map<Value *, ShadowState> AliasedShadows;
  std::map<Value *, uint32_t> TemplateFlagsByKey;
  std::map<Value *, std::set<uint32_t>> TraceSitesByKey;
};

class DxilPIXRayQueryLog : public ModulePass {
  PassOptionsState Options;
  std::vector<TraceSiteInfo> TraceSites;
  std::vector<EmitAnnotationInfo> EmitAnnotations;
  uint32_t NextLocalTraceSite = 0;

public:
  static char ID;
  DxilPIXRayQueryLog() : ModulePass(ID) {}
  StringRef getPassName() const override {
    return "DXIL Logs RayQuery invocations into a UAV";
  }

  void applyOptions(PassOptions O) override;
  bool runOnModule(Module &M) override;

private:
  bool InstrumentFunction(DxilModule &DM, Function &TargetFunction);
  ShadowState GetShadowForHandle(FunctionContext &Context, Value *Handle);
  Value *GetHandleKey(Value *Handle);
  void AllocateShadow(Function *TargetFunction, ShadowState &Shadow);
  void InitializeShadow(IRBuilder<> &Builder, OP *HlslOP, ShadowState &Shadow);
  void StoreTraceState(IRBuilder<> &Builder, OP *HlslOP, ShadowState &Shadow,
                       Value *Handle, uint32_t TraceSiteId, Value *InstanceMask,
                       Value *TraceInvocation, Value *EffectiveRayFlags,
                       Value *AsDynamicIndex, Value *OriginX, Value *OriginY,
                       Value *OriginZ, Value *RayTMin, Value *DirectionX,
                       Value *DirectionY, Value *DirectionZ, Value *RayTMax);
  Instruction *ResetTraceState(IRBuilder<> &Builder, OP *HlslOP,
                               ShadowState &Shadow);
  void EmitIfTraced(DxilModule &DM, FunctionContext &Context,
                    Instruction *InsertBefore, Value *Handle, uint32_t Reason);
  void EmitIfTracedFromShadow(DxilModule &DM, FunctionContext &Context,
                              Instruction *InsertBefore, ShadowState &Shadow,
                              uint32_t Reason, bool UseCommittedGetters,
                              Value *HandleOverride = nullptr);
  void EmitCandidateRecord(DxilModule &DM, FunctionContext &Context,
                           Instruction *InsertBefore, Value *Handle);
  void EmitRecordBody(DxilModule &DM, FunctionContext &Context,
                      IRBuilder<> &Builder, Value *Handle, ShadowState &Shadow,
                      uint32_t RecordType, uint32_t Reason,
                      bool UseCommittedGetters = true,
                      Value *EmitActive = nullptr);
  void EmitIfTracedWithoutCommittedGetters(DxilModule &DM,
                                           FunctionContext &Context,
                                           Instruction *InsertBefore,
                                           Value *Handle, uint32_t Reason);
  Value *CreateAsDynamicIndex(DxilModule &DM, IRBuilder<> &Builder,
                              Value *AccelerationStructureHandle);
  Value *CreateRayQueryGetter(DxilModule &DM, IRBuilder<> &Builder,
                              DXIL::OpCode OpCode, Type *ReturnType,
                              Value *Handle, const Twine &Name);
  Value *CreateRayQueryComponentGetter(DxilModule &DM, IRBuilder<> &Builder,
                                       DXIL::OpCode OpCode, Type *ReturnType,
                                       Value *Handle, uint8_t Component,
                                       const Twine &Name);
  Value *CreateIdentityComponent(DxilModule &DM, Function *TargetFunction,
                                 IRBuilder<> &Builder, unsigned Component);
  std::pair<Value *, Value *> CreatePackedIdentity(DxilModule &DM,
                                                   Function *TargetFunction,
                                                   IRBuilder<> &Builder);
  Value *CreateDomainLocationHash(DxilModule &DM, Function *TargetFunction,
                                  IRBuilder<> &Builder);
  Value *CreateIdentityInvalid(DxilModule &DM, Function *TargetFunction,
                               IRBuilder<> &Builder);
  bool EnsureIdentitySignatureInputs(DxilModule &DM, Function *TargetFunction);
  Value *CreateSignatureInput(DxilModule &DM, IRBuilder<> &Builder,
                              unsigned ElementId, Type *ReturnType,
                              uint8_t Component, const Twine &Name);
  bool TryFindInputSemantic(DxilModule &DM, DXIL::SemanticKind SemanticKind,
                            unsigned &ElementId);
  Value *CreateHelperLane(DxilModule &DM, Function *TargetFunction,
                          IRBuilder<> &Builder);
  bool HelperLaneIsKnown(DxilModule &DM, Function *TargetFunction);
  Value *CreateSampleHash(DxilModule &DM, IRBuilder<> &Builder,
                          ArrayRef<Value *> HashWords);
  Value *CreateHashMix(DxilModule &DM, IRBuilder<> &Builder, Value *ValueToMix);
  uint32_t GetTemplateFlagsForHandle(FunctionContext &Context, Value *Handle);
  uint32_t GetTemplateFlagsForValue(FunctionContext &Context,
                                    Value *ValueToRead,
                                    SmallPtrSetImpl<Value *> &VisitedValues);
  PointerAliasKind ClassifyPointerAlias(FunctionContext &Context,
                                        Value *FirstPointer,
                                        Value *SecondPointer);
  bool AreEquivalentPointers(Value *FirstPointer, Value *SecondPointer);
  bool TryGetRayQueryArrayStorage(Value *Handle, Value *&BasePointer,
                                  Value *&ElementIndex, uint32_t &ElementCount);
  bool IsShaderTerminationFunction(DxilModule &DM, Function &TargetFunction);
  std::string GetStageName(DxilModule &DM, Function *TargetFunction);
  std::string GetEntryName(DxilModule &DM, Function *TargetFunction);
  void AddEmitAnnotations(DxilModule &DM, FunctionContext &Context,
                          Instruction *InstructionToDescribe, Value *Handle,
                          StringRef EmitKind, StringRef RecordKind,
                          StringRef ReasonKind);
  void EmitSideTable();
  std::string GetDebugLocation(Instruction *InstructionToDescribe);
};

static bool IsRayQueryOp(Instruction *InstructionToCheck, DXIL::OpCode OpCode) {
  return OP::IsDxilOpFuncCallInst(InstructionToCheck, OpCode);
}

static Value *GetRayQueryHandleForInstruction(Instruction *InstructionToCheck) {
  if (IsRayQueryOp(InstructionToCheck, DXIL::OpCode::RayQuery_TraceRayInline)) {
    return DxilInst_RayQuery_TraceRayInline(InstructionToCheck)
        .get_rayQueryHandle();
  }
  if (IsRayQueryOp(InstructionToCheck, DXIL::OpCode::RayQuery_Proceed) ||
      IsRayQueryOp(InstructionToCheck, DXIL::OpCode::RayQuery_Abort) ||
      IsRayQueryOp(InstructionToCheck,
                   DXIL::OpCode::RayQuery_CommittedStatus) ||
      IsRayQueryOp(InstructionToCheck, DXIL::OpCode::RayQuery_CommittedRayT) ||
      IsRayQueryOp(InstructionToCheck,
                   DXIL::OpCode::RayQuery_CommittedInstanceIndex) ||
      IsRayQueryOp(InstructionToCheck,
                   DXIL::OpCode::RayQuery_CommittedGeometryIndex) ||
      IsRayQueryOp(InstructionToCheck,
                   DXIL::OpCode::RayQuery_CommittedPrimitiveIndex) ||
      IsRayQueryOp(InstructionToCheck, DXIL::OpCode::RayQuery_RayFlags) ||
      IsRayQueryOp(InstructionToCheck, DXIL::OpCode::RayQuery_WorldRayOrigin) ||
      IsRayQueryOp(InstructionToCheck,
                   DXIL::OpCode::RayQuery_WorldRayDirection) ||
      IsRayQueryOp(InstructionToCheck, DXIL::OpCode::RayQuery_RayTMin)) {
    return InstructionToCheck->getOperand(1);
  }
  return nullptr;
}

static bool HasDynamicIndex(Value *Pointer) {
  Pointer = Pointer->stripPointerCasts();
  while (auto *GetElementPointer = dyn_cast<GEPOperator>(Pointer)) {
    for (auto IndexIterator = GetElementPointer->idx_begin(),
              IndexEnd = GetElementPointer->idx_end();
         IndexIterator != IndexEnd; ++IndexIterator) {
      if (!isa<ConstantInt>(IndexIterator->get())) {
        return true;
      }
    }
    Pointer = GetElementPointer->getPointerOperand()->stripPointerCasts();
  }
  return false;
}

static bool IsRayQueryAllocation(Instruction *InstructionToCheck) {
  return IsRayQueryOp(InstructionToCheck, DXIL::OpCode::AllocateRayQuery) ||
         IsRayQueryOp(InstructionToCheck, DXIL::OpCode::AllocateRayQuery2);
}

static uint32_t GetConstUInt32(Value *ValueToRead, uint32_t DefaultValue) {
  if (auto *ConstantValue = dyn_cast<ConstantInt>(ValueToRead)) {
    return static_cast<uint32_t>(ConstantValue->getZExtValue());
  }
  return DefaultValue;
}

static int GetNextSignatureRow(const DxilSignature &Signature) {
  int NextRow = 0;
  for (const std::unique_ptr<DxilSignatureElement> &Element :
       Signature.GetElements()) {
    if (Element == nullptr ||
        Element->GetStartRow() == Semantic::kUndefinedRow) {
      continue;
    }
    NextRow = std::max(NextRow, Element->GetStartRow() +
                                    static_cast<int>(Element->GetRows()));
  }
  return NextRow;
}

static bool EnsureInputSignatureElement(
    DxilSignature &InputSignature, DXIL::SigPointKind SigPointKind,
    DXIL::SemanticKind SemanticKind, StringRef Name, CompType ComponentType,
    InterpolationMode InterpolationMode, unsigned RowCount,
    unsigned ColumnCount, int StartRow, int StartColumn, unsigned UsageMask) {
  for (const std::unique_ptr<DxilSignatureElement> &Element :
       InputSignature.GetElements()) {
    if (Element != nullptr && Element->GetSemantic() != nullptr &&
        Element->GetSemantic()->GetKind() == SemanticKind) {
      return false;
    }
  }

  std::unique_ptr<DxilSignatureElement> AddedElement =
      InputSignature.CreateElement();
  AddedElement->SetSigPointKind(SigPointKind);
  AddedElement->Initialize(Name, ComponentType, InterpolationMode, RowCount,
                           ColumnCount, StartRow, StartColumn);
  AddedElement->AppendSemanticIndex(0);
  AddedElement->SetKind(SemanticKind);
  AddedElement->SetUsageMask(UsageMask);
  InputSignature.AppendElement(std::move(AddedElement));
  return true;
}

void DxilPIXRayQueryLog::applyOptions(PassOptions O) {
  GetPassOptionUInt64(O, "maxNumEntriesInLog", &Options.MaxNumEntriesInLog, 1);
  GetPassOptionUInt32(O, "roiMinX", &Options.RoiMinX, 0);
  GetPassOptionUInt32(O, "roiMinY", &Options.RoiMinY, 0);
  GetPassOptionUInt32(O, "roiMinZ", &Options.RoiMinZ, 0);
  GetPassOptionUInt32(O, "roiMaxX", &Options.RoiMaxX, 0xFFFFFFFFu);
  GetPassOptionUInt32(O, "roiMaxY", &Options.RoiMaxY, 0xFFFFFFFFu);
  GetPassOptionUInt32(O, "roiMaxZ", &Options.RoiMaxZ, 0xFFFFFFFFu);
  GetPassOptionUInt32(O, "sampleRate", &Options.SampleRate, 1);
  GetPassOptionBool(O, "logCandidates", &Options.LogCandidates, false);
  GetPassOptionUInt32(O, "traceSiteBase", &Options.TraceSiteBase, 0);
  GetPassOptionUInt32(O, "subCallIndex", &Options.SubCallIndex, 0);
  GetPassOptionInt(O, "upstreamSVPositionRow", &Options.UpstreamSVPositionRow,
                   -1);
}

bool DxilPIXRayQueryLog::runOnModule(Module &M) {
  if (Options.MaxNumEntriesInLog == 0) {
    EmitSideTable();
    return false;
  }
  uint32_t RecordStride = RecordStrideBytes;
  uint64_t MaximumSafeEntries =
      static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) /
      RecordStride;
  if (Options.MaxNumEntriesInLog > MaximumSafeEntries) {
    Options.MaxNumEntriesInLog = MaximumSafeEntries;
  }

  DxilModule &DM = M.GetOrCreateDxilModule();
  bool Modified = false;

  for (Function *TargetFunction :
       PIXPassHelpers::GetAllInstrumentableFunctions(DM)) {
    Modified |= InstrumentFunction(DM, *TargetFunction);
  }

  if (Modified) {
    std::vector<GlobalVariable *> UnusedOrdinalGlobals;
    for (GlobalVariable &Global : M.globals()) {
      if (Global.getName().startswith("PIX_RayQuery_TraceInvocation") &&
          Global.use_empty()) {
        UnusedOrdinalGlobals.push_back(&Global);
      }
    }
    for (GlobalVariable *Global : UnusedOrdinalGlobals) {
      Global->eraseFromParent();
    }
    DM.CollectShaderFlagsForModule();
  }

  EmitSideTable();
  return Modified;
}

bool DxilPIXRayQueryLog::InstrumentFunction(DxilModule &DM,
                                            Function &TargetFunction) {
  std::vector<Instruction *> RayQueryInstructions;
  DXIL::ShaderKind ShaderKind =
      PIXPassHelpers::GetFunctionShaderKind(DM, &TargetFunction);
  bool HasRayQueryOperation = false;
  for (BasicBlock &Block : TargetFunction) {
    for (Instruction &InstructionToCheck : Block) {
      if (IsRayQueryAllocation(&InstructionToCheck) ||
          IsRayQueryOp(&InstructionToCheck,
                       DXIL::OpCode::RayQuery_TraceRayInline) ||
          IsRayQueryOp(&InstructionToCheck, DXIL::OpCode::RayQuery_Proceed) ||
          IsRayQueryOp(&InstructionToCheck,
                       DXIL::OpCode::RayQuery_CommittedStatus) ||
          IsRayQueryOp(&InstructionToCheck, DXIL::OpCode::RayQuery_Abort)) {
        HasRayQueryOperation = true;
        RayQueryInstructions.push_back(&InstructionToCheck);
      } else if (IsRayQueryOp(&InstructionToCheck,
                              DXIL::OpCode::AcceptHitAndEndSearch) ||
                 IsRayQueryOp(&InstructionToCheck, DXIL::OpCode::IgnoreHit) ||
                 IsRayQueryOp(&InstructionToCheck, DXIL::OpCode::Discard)) {
        RayQueryInstructions.push_back(&InstructionToCheck);
      }
    }
  }

  // Termination ops only matter to flush traces; without a RayQuery there is
  // nothing to log, so leave the function (and its resources) untouched.
  if (!HasRayQueryOperation) {
    return false;
  }

  EnsureIdentitySignatureInputs(DM, &TargetFunction);

  OP *HlslOP = DM.GetOP();
  IRBuilder<> EntryBuilder(
      dxilutil::FirstNonAllocaInsertionPt(&TargetFunction));

  FunctionContext Context;
  Context.TargetFunction = &TargetFunction;
  Context.CounterUAVHandle = PIXPassHelpers::CreateUAVOnceForModule(
      DM, EntryBuilder, CounterUAVRegister, "PIX_RayQuery_CountUAV_Handle",
      DefaultRegisterSpace, OSOverride);
  Context.LogUAVHandle = PIXPassHelpers::CreateUAVOnceForModule(
      DM, EntryBuilder, LogUAVRegister, "PIX_RayQuery_LogUAV_Handle",
      DefaultRegisterSpace, OSOverride);
  if (Options.LogCandidates) {
    Context.CandidateCounterUAVHandle = PIXPassHelpers::CreateUAVOnceForModule(
        DM, EntryBuilder, CandidateCounterUAVRegister,
        "PIX_RayQuery_CandidateCountUAV_Handle", DefaultRegisterSpace,
        OSOverride);
    Context.CandidateLogUAVHandle = PIXPassHelpers::CreateUAVOnceForModule(
        DM, EntryBuilder, CandidateLogUAVRegister,
        "PIX_RayQuery_CandidateLogUAV_Handle", DefaultRegisterSpace,
        OSOverride);
  }
  DM.ReEmitDxilResources();
  Module *TargetModule = TargetFunction.getParent();
  GlobalVariable *TraceInvocationGlobal =
      TargetModule->getGlobalVariable("PIX_RayQuery_TraceInvocation", true);
  if (TraceInvocationGlobal == nullptr) {
    TraceInvocationGlobal = new GlobalVariable(
        *TargetModule, Type::getInt32Ty(TargetFunction.getContext()), false,
        GlobalValue::InternalLinkage, HlslOP->GetU32Const(0),
        "PIX_RayQuery_TraceInvocation");
  }
  Context.SharedTraceInvocation = TraceInvocationGlobal;
  if (IsShaderTerminationFunction(DM, TargetFunction)) {
    EntryBuilder.CreateStore(HlslOP->GetU32Const(0),
                             Context.SharedTraceInvocation);
  }
  Context.FallbackShadow = {nullptr, nullptr, nullptr, nullptr, nullptr,
                            nullptr, nullptr, nullptr, nullptr, nullptr,
                            nullptr, nullptr, nullptr, nullptr};
  AllocateShadow(&TargetFunction, Context.FallbackShadow);
  InitializeShadow(EntryBuilder, HlslOP, Context.FallbackShadow);

  for (Instruction *InstructionToInstrument : RayQueryInstructions) {
    if (!IsRayQueryAllocation(InstructionToInstrument)) {
      continue;
    }

    ShadowState Shadow;
    AllocateShadow(&TargetFunction, Shadow);
    IRBuilder<> Builder(dxilutil::FirstNonAllocaInsertionPt(&TargetFunction));
    InitializeShadow(Builder, HlslOP, Shadow);
    Context.ShadowsByKey[InstructionToInstrument] = Shadow;
  }

  // Register every trace site up front so allocation annotations also see
  // trace sites that appear later in the function (e.g. inside loops).
  {
    uint32_t PendingLocalTraceSite = NextLocalTraceSite;
    for (Instruction *InstructionToInstrument : RayQueryInstructions) {
      if (IsRayQueryOp(InstructionToInstrument,
                       DXIL::OpCode::RayQuery_TraceRayInline)) {
        Value *Handle =
            DxilInst_RayQuery_TraceRayInline(InstructionToInstrument)
                .get_rayQueryHandle();
        Context.TraceSitesByKey[GetHandleKey(Handle)].insert(
            Options.TraceSiteBase + PendingLocalTraceSite++);
      }
    }
  }

  for (Instruction *InstructionToInstrument : RayQueryInstructions) {
    if (IsRayQueryAllocation(InstructionToInstrument)) {
      Value *Key = GetHandleKey(InstructionToInstrument);
      uint32_t TemplateFlags =
          GetConstUInt32(InstructionToInstrument->getOperand(1), 0);
      Context.TemplateFlagsByKey[Key] = TemplateFlags;
      ShadowState Shadow = GetShadowForHandle(Context, InstructionToInstrument);
      IRBuilder<> Builder(InstructionToInstrument);
      Instruction *ResetStart = ResetTraceState(Builder, HlslOP, Shadow);
      AddEmitAnnotations(DM, Context, InstructionToInstrument,
                         InstructionToInstrument, "allocate", "query",
                         "EXITED");
      EmitIfTracedWithoutCommittedGetters(
          DM, Context, ResetStart, InstructionToInstrument, ReasonExited);
      continue;
    }

    if (IsRayQueryOp(InstructionToInstrument,
                     DXIL::OpCode::RayQuery_TraceRayInline)) {
      auto TraceRayInline =
          DxilInst_RayQuery_TraceRayInline(InstructionToInstrument);
      Value *Handle = TraceRayInline.get_rayQueryHandle();
      ShadowState Shadow = GetShadowForHandle(Context, Handle);

      uint32_t LocalTraceSite = NextLocalTraceSite++;
      if (Options.TraceSiteBase > 0xFFFFu ||
          static_cast<uint64_t>(Options.TraceSiteBase) + LocalTraceSite >
              0xFFFFu) {
        const char *ErrorMessage =
            "PIX RayQuery log traceSiteBase plus site count exceeds the "
            "16-bit trace-site record field";
        report_fatal_error(ErrorMessage);
      }
      uint32_t TraceSiteId = Options.TraceSiteBase + LocalTraceSite;
      TraceSiteInfo SiteInfo;
      SiteInfo.LocalOrdinal = LocalTraceSite;
      SiteInfo.TraceSiteId = TraceSiteId;
      SiteInfo.ParentFunction = &TargetFunction;
      SiteInfo.DebugLocation = GetDebugLocation(InstructionToInstrument);
      SiteInfo.StageName = GetStageName(DM, &TargetFunction);
      SiteInfo.EntryName = GetEntryName(DM, &TargetFunction);
      TraceSites.push_back(SiteInfo);

      Value *TraceKey = GetHandleKey(Handle);
      AddEmitAnnotations(DM, Context, InstructionToInstrument, Handle, "trace",
                         "query", "RETRACED");
      Context.TraceSitesByKey[TraceKey].insert(TraceSiteId);

      EmitIfTraced(DM, Context, InstructionToInstrument, Handle,
                   ReasonRetraced);
      Instruction *InsertAfter = InstructionToInstrument->getNextNode();

      if (InsertAfter != nullptr) {
        IRBuilder<> Builder(InsertAfter);
        Value *DynamicFlags = TraceRayInline.get_rayFlags();
        Value *TemplateFlags =
            HlslOP->GetU32Const(GetTemplateFlagsForHandle(Context, Handle));
        Value *EffectiveRayFlags =
            Builder.CreateOr(DynamicFlags, TemplateFlags, "IrtEffectiveFlags");
        Value *AsDynamicIndex = CreateAsDynamicIndex(
            DM, Builder, TraceRayInline.get_accelerationStructure());
        Value *TraceInvocation = Builder.CreateLoad(
            Context.SharedTraceInvocation, "IrtTraceInvocation");
        Builder.CreateStore(Builder.CreateAdd(TraceInvocation,
                                              HlslOP->GetU32Const(1),
                                              "IrtNextTraceInvocation"),
                            Context.SharedTraceInvocation);
        StoreTraceState(
            Builder, HlslOP, Shadow, Handle, TraceSiteId,
            TraceRayInline.get_instanceInclusionMask(), TraceInvocation,
            EffectiveRayFlags, AsDynamicIndex, TraceRayInline.get_origin_X(),
            TraceRayInline.get_origin_Y(), TraceRayInline.get_origin_Z(),
            TraceRayInline.get_tMin(), TraceRayInline.get_direction_X(),
            TraceRayInline.get_direction_Y(), TraceRayInline.get_direction_Z(),
            TraceRayInline.get_tMax());
      }
      continue;
    }

    if (IsRayQueryOp(InstructionToInstrument, DXIL::OpCode::RayQuery_Proceed)) {
      Value *Handle = GetRayQueryHandleForInstruction(InstructionToInstrument);
      Instruction *InsertAfter = InstructionToInstrument->getNextNode();
      if (InsertAfter != nullptr) {
        if (Options.LogCandidates) {
          IRBuilder<> CandidateBuilder(InsertAfter);
          Value *ProceedReturnedTrue = CandidateBuilder.CreateICmpEQ(
              InstructionToInstrument, HlslOP->GetI1Const(true),
              "IrtProceedReturnedTrue");
          TerminatorInst *CandidateTerminator = SplitBlockAndInsertIfThen(
              ProceedReturnedTrue, InsertAfter, false);
          AddEmitAnnotations(DM, Context, InstructionToInstrument, Handle,
                             "proceedTrue", "candidate", "COMPLETED");
          EmitCandidateRecord(DM, Context, CandidateTerminator, Handle);
        }

        IRBuilder<> Builder(InsertAfter);
        Value *ProceedResult = InstructionToInstrument;
        Value *ProceedReturnedFalse =
            Builder.CreateICmpEQ(ProceedResult, HlslOP->GetI1Const(false),
                                 "IrtProceedReturnedFalse");
        TerminatorInst *ThenTerminator =
            SplitBlockAndInsertIfThen(ProceedReturnedFalse, InsertAfter, false);
        IRBuilder<> RetirementBuilder(ThenTerminator);
        ShadowState Shadow = GetShadowForHandle(Context, Handle);
        Instruction *ResetStart =
            ResetTraceState(RetirementBuilder, HlslOP, Shadow);
        AddEmitAnnotations(DM, Context, InstructionToInstrument, Handle,
                           "proceedFalse", "query", "COMPLETED");
        EmitIfTraced(DM, Context, ResetStart, Handle, ReasonCompleted);
      }
      continue;
    }

    if (IsRayQueryOp(InstructionToInstrument, DXIL::OpCode::RayQuery_Abort)) {
      Value *Handle = GetRayQueryHandleForInstruction(InstructionToInstrument);
      IRBuilder<> RetirementBuilder(InstructionToInstrument);
      ShadowState Shadow = GetShadowForHandle(Context, Handle);
      Instruction *ResetStart =
          ResetTraceState(RetirementBuilder, HlslOP, Shadow);
      AddEmitAnnotations(DM, Context, InstructionToInstrument, Handle, "abort",
                         "query", "ABORTED");
      EmitIfTraced(DM, Context, ResetStart, Handle, ReasonAborted);
      continue;
    }

    if (IsRayQueryOp(InstructionToInstrument,
                     DXIL::OpCode::AcceptHitAndEndSearch) ||
        IsRayQueryOp(InstructionToInstrument, DXIL::OpCode::IgnoreHit)) {
      for (auto &ShadowEntry : Context.ShadowsByKey) {
        ShadowState &Shadow = ShadowEntry.second;
        AddEmitAnnotations(DM, Context, InstructionToInstrument,
                           ShadowEntry.first, "hitShaderTermination", "query",
                           "EXITED");
        EmitIfTracedFromShadow(DM, Context, InstructionToInstrument, Shadow,
                               ReasonExited, false);
      }
      continue;
    }

    if (IsRayQueryOp(InstructionToInstrument, DXIL::OpCode::Discard)) {
      DxilInst_Discard Discard(InstructionToInstrument);
      IRBuilder<> Builder(InstructionToInstrument);
      TerminatorInst *DiscardTrueTerminator = SplitBlockAndInsertIfThen(
          Discard.get_condition(), InstructionToInstrument, false);
      for (auto &ShadowEntry : Context.ShadowsByKey) {
        ShadowState &Shadow = ShadowEntry.second;
        AddEmitAnnotations(DM, Context, InstructionToInstrument,
                           ShadowEntry.first, "discard", "query", "EXITED");
        EmitIfTracedFromShadow(DM, Context, DiscardTrueTerminator, Shadow,
                               ReasonExited, false);
      }
      continue;
    }
  }

  std::vector<ReturnInst *> ReturnInstructions;
  for (BasicBlock &Block : TargetFunction) {
    if (auto *ReturnInstruction = dyn_cast<ReturnInst>(Block.getTerminator())) {
      ReturnInstructions.push_back(ReturnInstruction);
    }
  }

  if (IsShaderTerminationFunction(DM, TargetFunction) &&
      ShaderKind != DXIL::ShaderKind::Geometry) {
    for (ReturnInst *ReturnInstruction : ReturnInstructions) {
      if (Context.ShadowsByKey.empty()) {
        AddEmitAnnotations(DM, Context, ReturnInstruction, nullptr, "return",
                           "query", "EXITED");
        EmitIfTracedFromShadow(DM, Context, ReturnInstruction,
                               Context.FallbackShadow, ReasonExited, false);
      }
      for (auto &ShadowEntry : Context.ShadowsByKey) {
        ShadowState &Shadow = ShadowEntry.second;
        AddEmitAnnotations(DM, Context, ReturnInstruction, ShadowEntry.first,
                           "return", "query", "EXITED");
        EmitIfTracedFromShadow(DM, Context, ReturnInstruction, Shadow,
                               ReasonExited, false);
      }
    }
  }

  return true;
}

Value *DxilPIXRayQueryLog::GetHandleKey(Value *Handle) {
  if (auto *Load = dyn_cast<LoadInst>(Handle)) {
    if (auto *GetElementPointer =
            dyn_cast<GetElementPtrInst>(Load->getPointerOperand())) {
      for (auto IndexIterator = GetElementPointer->idx_begin(),
                IndexEnd = GetElementPointer->idx_end();
           IndexIterator != IndexEnd; ++IndexIterator) {
        if (!isa<ConstantInt>(IndexIterator->get())) {
          return GetElementPointer->getPointerOperand();
        }
      }
    }
    return Load->getPointerOperand();
  }
  if (auto *Store = dyn_cast<StoreInst>(Handle)) {
    if (auto *GetElementPointer =
            dyn_cast<GetElementPtrInst>(Store->getPointerOperand())) {
      for (auto IndexIterator = GetElementPointer->idx_begin(),
                IndexEnd = GetElementPointer->idx_end();
           IndexIterator != IndexEnd; ++IndexIterator) {
        if (!isa<ConstantInt>(IndexIterator->get())) {
          return GetElementPointer->getPointerOperand();
        }
      }
    }
    return Store->getPointerOperand();
  }
  if (isa<PHINode>(Handle) || isa<SelectInst>(Handle)) {
    return Handle;
  }
  return Handle;
}

ShadowState DxilPIXRayQueryLog::GetShadowForHandle(FunctionContext &Context,
                                                   Value *Handle) {
  auto CachedShadow = Context.AliasedShadows.find(Handle);
  if (CachedShadow != Context.AliasedShadows.end()) {
    return CachedShadow->second;
  }

  auto AllocationShadow = Context.ShadowsByKey.find(Handle);
  if (AllocationShadow != Context.ShadowsByKey.end()) {
    return AllocationShadow->second;
  }

  std::function<ShadowState(Value *)> ResolveShadow =
      [&](Value *HandleValue) -> ShadowState {
    auto Cached = Context.AliasedShadows.find(HandleValue);
    if (Cached != Context.AliasedShadows.end()) {
      return Cached->second;
    }

    auto Allocation = Context.ShadowsByKey.find(HandleValue);
    if (Allocation != Context.ShadowsByKey.end()) {
      return Allocation->second;
    }

    if (auto *Phi = dyn_cast<PHINode>(HandleValue)) {
      static const char *ShadowNames[] = {
          "IrtStateAlias",           "IrtTraceSiteAlias",
          "IrtTraceInvocationAlias", "IrtInstanceMaskAlias",
          "IrtRayFlagsAlias",        "IrtAsDynamicIndexAlias",
          "IrtOriginXAlias",         "IrtOriginYAlias",
          "IrtOriginZAlias",         "IrtRayTMinAlias",
          "IrtDirectionXAlias",      "IrtDirectionYAlias",
          "IrtDirectionZAlias",      "IrtRayTMaxAlias"};
      ShadowValues PhiValues;
      IRBuilder<> PhiBuilder(&*Phi->getParent()->getFirstInsertionPt());
      for (size_t ValueIndex = 0; ValueIndex < PhiValues.size(); ++ValueIndex) {
        PhiValues[ValueIndex] = PhiBuilder.CreatePHI(
            GetShadowValues(Context.FallbackShadow)[ValueIndex]->getType(),
            Phi->getNumIncomingValues(), ShadowNames[ValueIndex]);
      }
      ShadowState PhiShadow = CreateShadowState(PhiValues);
      Context.AliasedShadows[Phi] = PhiShadow;
      ShadowValues PhiNodes = GetShadowValues(PhiShadow);
      for (unsigned IncomingIndex = 0;
           IncomingIndex < Phi->getNumIncomingValues(); ++IncomingIndex) {
        BasicBlock *IncomingBlock = Phi->getIncomingBlock(IncomingIndex);
        ShadowState IncomingShadow =
            ResolveShadow(Phi->getIncomingValue(IncomingIndex));
        ShadowValues IncomingValues = GetShadowValues(IncomingShadow);
        for (size_t ValueIndex = 0; ValueIndex < PhiNodes.size();
             ++ValueIndex) {
          cast<PHINode>(PhiNodes[ValueIndex])
              ->addIncoming(IncomingValues[ValueIndex], IncomingBlock);
        }
      }
      return PhiShadow;
    }

    if (auto *Select = dyn_cast<SelectInst>(HandleValue)) {
      ShadowState TrueShadow = ResolveShadow(Select->getTrueValue());
      ShadowState FalseShadow = ResolveShadow(Select->getFalseValue());
      IRBuilder<> Builder(Select->getNextNode());
      ShadowState SelectedShadow = CreateShadowSelect(
          Builder, Select->getCondition(), TrueShadow, FalseShadow);
      Context.AliasedShadows[Select] = SelectedShadow;
      return SelectedShadow;
    }

    if (isa<Constant>(HandleValue)) {
      return Context.FallbackShadow;
    }

    if (auto *Load = dyn_cast<LoadInst>(HandleValue)) {
      Value *LoadPointer = Load->getPointerOperand();
      auto ClassifyStore = [&](StoreInst *Store) {
        PointerAliasKind AliasKind = ClassifyPointerAlias(
            Context, Store->getPointerOperand(), LoadPointer);
        if (AliasKind == PointerAliasKind::Equivalent &&
            Store->getValueOperand()->getType() != Load->getType()) {
          return PointerAliasKind::MayAlias;
        }
        return AliasKind;
      };

      // A store earlier in the load's own block is the reaching definition
      // unless a store that may alias the slot intervenes.
      StoreInst *LastEquivalentStoreBeforeLoad = nullptr;
      bool LoadBlockIsClobberedBeforeLoad = false;
      for (Instruction &InstructionInBlock : *Load->getParent()) {
        if (&InstructionInBlock == Load) {
          break;
        }

        auto *Store = dyn_cast<StoreInst>(&InstructionInBlock);
        if (Store == nullptr) {
          continue;
        }

        PointerAliasKind AliasKind = ClassifyStore(Store);
        if (AliasKind == PointerAliasKind::Equivalent) {
          LastEquivalentStoreBeforeLoad = Store;
          LoadBlockIsClobberedBeforeLoad = false;
        } else if (AliasKind == PointerAliasKind::MayAlias) {
          LastEquivalentStoreBeforeLoad = nullptr;
          LoadBlockIsClobberedBeforeLoad = true;
        }
      }

      if (LastEquivalentStoreBeforeLoad != nullptr) {
        ShadowState ResolvedShadow =
            ResolveShadow(LastEquivalentStoreBeforeLoad->getValueOperand());
        Context.AliasedShadows[Load] = ResolvedShadow;
        return ResolvedShadow;
      }

      // Across blocks a dynamic index can change between the store and the
      // load, so only constant-indexed slots are renamed through SSA. Each
      // block's last store (including stores after the load in its own block,
      // which reach it through loop back edges) is an end-of-block definition.
      if (!LoadBlockIsClobberedBeforeLoad && !HasDynamicIndex(LoadPointer)) {
        bool HasPotentialClobber = false;
        std::map<BasicBlock *, StoreInst *> LastStoresByBlock;
        for (BasicBlock &Block : *Context.TargetFunction) {
          StoreInst *LastEquivalentStoreInBlock = nullptr;
          bool EndOfBlockIsClobbered = false;
          for (Instruction &InstructionInBlock : Block) {
            auto *Store = dyn_cast<StoreInst>(&InstructionInBlock);
            if (Store == nullptr) {
              continue;
            }

            PointerAliasKind AliasKind = ClassifyStore(Store);
            if (AliasKind == PointerAliasKind::Equivalent) {
              LastEquivalentStoreInBlock = Store;
              EndOfBlockIsClobbered = false;
            } else if (AliasKind == PointerAliasKind::MayAlias) {
              LastEquivalentStoreInBlock = nullptr;
              EndOfBlockIsClobbered = true;
            }
          }

          if (EndOfBlockIsClobbered) {
            HasPotentialClobber = true;
            break;
          }
          if (LastEquivalentStoreInBlock != nullptr) {
            LastStoresByBlock[&Block] = LastEquivalentStoreInBlock;
          }
        }

        if (!HasPotentialClobber && !LastStoresByBlock.empty()) {
          SSAUpdater HandleSSA;
          HandleSSA.Initialize(Load->getType(), "IrtAliasedRayQueryHandle");
          for (const auto &StoreEntry : LastStoresByBlock) {
            HandleSSA.AddAvailableValue(StoreEntry.first,
                                        StoreEntry.second->getValueOperand());
          }
          // With a definition in the load's block this yields the live-in
          // value, which is correct because no store precedes the load there.
          Value *ResolvedHandle =
              HandleSSA.GetValueInMiddleOfBlock(Load->getParent());
          if (ResolvedHandle != Load) {
            ShadowState ResolvedShadow = ResolveShadow(ResolvedHandle);
            Context.AliasedShadows[Load] = ResolvedShadow;
            return ResolvedShadow;
          }
        }
      }
    }

    // Compare the actual handle against every allocation. The selects are
    // inserted right after the handle definition so the cached result
    // dominates every later use of the handle on any path.
    auto *HandleInstruction = dyn_cast<Instruction>(HandleValue);
    if (HandleInstruction == nullptr ||
        HandleInstruction->getNextNode() == nullptr) {
      return Context.FallbackShadow;
    }
    Instruction *InsertBefore = HandleInstruction->getNextNode();

    DominatorTree Dominators;
    Dominators.recalculate(*Context.TargetFunction);
    IRBuilder<> Builder(InsertBefore);
    ShadowState SelectedShadow = Context.FallbackShadow;
    for (const auto &ShadowEntry : Context.ShadowsByKey) {
      auto *Allocation = dyn_cast<Instruction>(ShadowEntry.first);
      if (Allocation == nullptr ||
          !Dominators.dominates(Allocation, InsertBefore)) {
        continue;
      }

      Value *MatchesAllocation = Builder.CreateICmpEQ(
          HandleValue, ShadowEntry.first, "IrtHandleMatchesAllocation");
      SelectedShadow = CreateShadowSelect(Builder, MatchesAllocation,
                                          ShadowEntry.second, SelectedShadow);
    }
    Context.AliasedShadows[HandleValue] = SelectedShadow;
    return SelectedShadow;
  };

  return ResolveShadow(Handle);
}

void DxilPIXRayQueryLog::AllocateShadow(Function *TargetFunction,
                                        ShadowState &Shadow) {
  LLVMContext &Ctx = TargetFunction->getContext();
  IRBuilder<> AllocaBuilder(&TargetFunction->getEntryBlock().front());
  Type *Int32Type = Type::getInt32Ty(Ctx);
  Type *FloatType = Type::getFloatTy(Ctx);
  Shadow.State = AllocaBuilder.CreateAlloca(Int32Type, nullptr, "IrtState");
  Shadow.TraceSiteId =
      AllocaBuilder.CreateAlloca(Int32Type, nullptr, "IrtTraceSiteId");
  Shadow.TraceInvocation =
      AllocaBuilder.CreateAlloca(Int32Type, nullptr, "IrtTraceInvocation");
  Shadow.InstanceMask =
      AllocaBuilder.CreateAlloca(Int32Type, nullptr, "IrtInstanceMask");
  Shadow.EffectiveRayFlags =
      AllocaBuilder.CreateAlloca(Int32Type, nullptr, "IrtEffectiveRayFlags");
  Shadow.AsDynamicIndex =
      AllocaBuilder.CreateAlloca(Int32Type, nullptr, "IrtAsDynamicIndex");
  Shadow.OriginX = AllocaBuilder.CreateAlloca(FloatType, nullptr, "IrtOriginX");
  Shadow.OriginY = AllocaBuilder.CreateAlloca(FloatType, nullptr, "IrtOriginY");
  Shadow.OriginZ = AllocaBuilder.CreateAlloca(FloatType, nullptr, "IrtOriginZ");
  Shadow.RayTMin = AllocaBuilder.CreateAlloca(FloatType, nullptr, "IrtRayTMin");
  Shadow.DirectionX =
      AllocaBuilder.CreateAlloca(FloatType, nullptr, "IrtDirectionX");
  Shadow.DirectionY =
      AllocaBuilder.CreateAlloca(FloatType, nullptr, "IrtDirectionY");
  Shadow.DirectionZ =
      AllocaBuilder.CreateAlloca(FloatType, nullptr, "IrtDirectionZ");
  Shadow.RayTMax = AllocaBuilder.CreateAlloca(FloatType, nullptr, "IrtRayTMax");
}

void DxilPIXRayQueryLog::InitializeShadow(IRBuilder<> &Builder, OP *HlslOP,
                                          ShadowState &Shadow) {
  Builder.CreateStore(HlslOP->GetU32Const(StateIdle), Shadow.State);
  Builder.CreateStore(HlslOP->GetU32Const(0), Shadow.TraceSiteId);
  Builder.CreateStore(HlslOP->GetU32Const(0), Shadow.TraceInvocation);
  Builder.CreateStore(HlslOP->GetU32Const(0), Shadow.InstanceMask);
  Builder.CreateStore(HlslOP->GetU32Const(0), Shadow.EffectiveRayFlags);
  Builder.CreateStore(HlslOP->GetU32Const(UnknownIndex), Shadow.AsDynamicIndex);
  Value *FloatZero =
      ConstantFP::get(Type::getFloatTy(Builder.getContext()), 0.0);
  Builder.CreateStore(FloatZero, Shadow.OriginX);
  Builder.CreateStore(FloatZero, Shadow.OriginY);
  Builder.CreateStore(FloatZero, Shadow.OriginZ);
  Builder.CreateStore(FloatZero, Shadow.RayTMin);
  Builder.CreateStore(FloatZero, Shadow.DirectionX);
  Builder.CreateStore(FloatZero, Shadow.DirectionY);
  Builder.CreateStore(FloatZero, Shadow.DirectionZ);
  Builder.CreateStore(FloatZero, Shadow.RayTMax);
}

void DxilPIXRayQueryLog::StoreTraceState(
    IRBuilder<> &Builder, OP *HlslOP, ShadowState &Shadow, Value *Handle,
    uint32_t TraceSiteId, Value *InstanceMask, Value *TraceInvocation,
    Value *EffectiveRayFlags, Value *AsDynamicIndex, Value *OriginX,
    Value *OriginY, Value *OriginZ, Value *RayTMin, Value *DirectionX,
    Value *DirectionY, Value *DirectionZ, Value *RayTMax) {
  Builder.CreateStore(HlslOP->GetU32Const(StateTraced), Shadow.State);
  Builder.CreateStore(HlslOP->GetU32Const(TraceSiteId), Shadow.TraceSiteId);
  Builder.CreateStore(TraceInvocation, Shadow.TraceInvocation);
  Builder.CreateStore(InstanceMask, Shadow.InstanceMask);
  Builder.CreateStore(EffectiveRayFlags, Shadow.EffectiveRayFlags);
  Builder.CreateStore(AsDynamicIndex, Shadow.AsDynamicIndex);
  Builder.CreateStore(OriginX, Shadow.OriginX);
  Builder.CreateStore(OriginY, Shadow.OriginY);
  Builder.CreateStore(OriginZ, Shadow.OriginZ);
  Builder.CreateStore(RayTMin, Shadow.RayTMin);
  Builder.CreateStore(DirectionX, Shadow.DirectionX);
  Builder.CreateStore(DirectionY, Shadow.DirectionY);
  Builder.CreateStore(DirectionZ, Shadow.DirectionZ);
  Builder.CreateStore(RayTMax, Shadow.RayTMax);
}

Instruction *DxilPIXRayQueryLog::ResetTraceState(IRBuilder<> &Builder,
                                                 OP *HlslOP,
                                                 ShadowState &Shadow) {
  Instruction *FirstStore =
      Builder.CreateStore(HlslOP->GetU32Const(StateIdle), Shadow.State);
  Builder.CreateStore(HlslOP->GetU32Const(0), Shadow.TraceSiteId);
  Builder.CreateStore(HlslOP->GetU32Const(0), Shadow.TraceInvocation);
  Builder.CreateStore(HlslOP->GetU32Const(0), Shadow.InstanceMask);
  Builder.CreateStore(HlslOP->GetU32Const(0), Shadow.EffectiveRayFlags);
  Builder.CreateStore(HlslOP->GetU32Const(UnknownIndex), Shadow.AsDynamicIndex);
  Value *FloatZero =
      ConstantFP::get(Type::getFloatTy(Builder.getContext()), 0.0);
  Builder.CreateStore(FloatZero, Shadow.OriginX);
  Builder.CreateStore(FloatZero, Shadow.OriginY);
  Builder.CreateStore(FloatZero, Shadow.OriginZ);
  Builder.CreateStore(FloatZero, Shadow.RayTMin);
  Builder.CreateStore(FloatZero, Shadow.DirectionX);
  Builder.CreateStore(FloatZero, Shadow.DirectionY);
  Builder.CreateStore(FloatZero, Shadow.DirectionZ);
  Builder.CreateStore(FloatZero, Shadow.RayTMax);
  return FirstStore;
}

void DxilPIXRayQueryLog::EmitIfTraced(DxilModule &DM, FunctionContext &Context,
                                      Instruction *InsertBefore, Value *Handle,
                                      uint32_t Reason) {
  ShadowState Shadow = GetShadowForHandle(Context, Handle);
  bool UseCommittedGetters = Reason != ReasonRetraced && Reason != ReasonExited;
  EmitIfTracedFromShadow(DM, Context, InsertBefore, Shadow, Reason,
                         UseCommittedGetters, Handle);
}

void DxilPIXRayQueryLog::EmitIfTracedFromShadow(
    DxilModule &DM, FunctionContext &Context, Instruction *InsertBefore,
    ShadowState &Shadow, uint32_t Reason, bool UseCommittedGetters,
    Value *HandleOverride) {
  IRBuilder<> Builder(InsertBefore);
  OP *HlslOP = DM.GetOP();
  Value *State = Builder.CreateLoad(Shadow.State, "IrtLoadedState");
  Value *IsTraced = Builder.CreateICmpEQ(
      State, HlslOP->GetU32Const(StateTraced), "IrtIsTraced");
  Value *CurrentHandle = HandleOverride;
  if (CurrentHandle == nullptr) {
    CurrentHandle = HlslOP->GetU32Const(0);
  }
  Value *NextState = Builder.CreateSelect(
      IsTraced, HlslOP->GetU32Const(StateFinalized), State, "IrtNextState");
  Builder.CreateStore(NextState, Shadow.State);
  EmitRecordBody(DM, Context, Builder, CurrentHandle, Shadow, QueryRecordType,
                 Reason, UseCommittedGetters, IsTraced);
}

void DxilPIXRayQueryLog::EmitIfTracedWithoutCommittedGetters(
    DxilModule &DM, FunctionContext &Context, Instruction *InsertBefore,
    Value *Handle, uint32_t Reason) {
  ShadowState Shadow = GetShadowForHandle(Context, Handle);
  IRBuilder<> Builder(InsertBefore);
  OP *HlslOP = DM.GetOP();
  Value *State = Builder.CreateLoad(Shadow.State, "IrtLoadedSyntheticState");
  Value *IsTraced = Builder.CreateICmpEQ(
      State, HlslOP->GetU32Const(StateTraced), "IrtSyntheticIsTraced");
  Value *PreviousHandle = HlslOP->GetU32Const(0);
  Value *NextState =
      Builder.CreateSelect(IsTraced, HlslOP->GetU32Const(StateFinalized), State,
                           "IrtSyntheticNextState");
  Builder.CreateStore(NextState, Shadow.State);
  EmitRecordBody(DM, Context, Builder, PreviousHandle, Shadow, QueryRecordType,
                 Reason, false, IsTraced);
}

void DxilPIXRayQueryLog::EmitCandidateRecord(DxilModule &DM,
                                             FunctionContext &Context,
                                             Instruction *InsertBefore,
                                             Value *Handle) {
  ShadowState Shadow = GetShadowForHandle(Context, Handle);
  IRBuilder<> Builder(InsertBefore);
  Value *State = Builder.CreateLoad(Shadow.State, "IrtLoadedCandidateState");
  Value *IsTraced = Builder.CreateICmpEQ(
      State, DM.GetOP()->GetU32Const(StateTraced), "IrtCandidateIsTraced");
  EmitRecordBody(DM, Context, Builder, Handle, Shadow, CandidateRecordType,
                 ReasonCompleted, true, IsTraced);
}

Value *
DxilPIXRayQueryLog::CreateAsDynamicIndex(DxilModule &DM, IRBuilder<> &Builder,
                                         Value *AccelerationStructureHandle) {
  OP *HlslOP = DM.GetOP();
  Value *Handle = AccelerationStructureHandle;
  if (auto *Call = dyn_cast<CallInst>(Handle)) {
    if (OP::IsDxilOpFuncCallInst(Call, DXIL::OpCode::AnnotateHandle)) {
      DxilInst_AnnotateHandle AnnotateHandle(Call);
      Handle = AnnotateHandle.get_res();
    }
  }

  if (auto *Call = dyn_cast<CallInst>(Handle)) {
    if (OP::IsDxilOpFuncCallInst(Call, DXIL::OpCode::CreateHandle)) {
      DxilInst_CreateHandle CreateHandle(Call);
      Value *Index = CreateHandle.get_index();
      if (auto *ConstantIndex = dyn_cast<ConstantInt>(Index)) {
        (void)ConstantIndex;
        return HlslOP->GetU32Const(UnknownIndex);
      }
      unsigned RangeId = CreateHandle.get_rangeId_val();
      uint32_t LowerBound = 0;
      if (RangeId < DM.GetSRVs().size()) {
        LowerBound = DM.GetSRV(RangeId).GetLowerBound();
      }
      return Builder.CreateSub(Index, HlslOP->GetU32Const(LowerBound),
                               "IrtAsDynamicIndex");
    }

    if (OP::IsDxilOpFuncCallInst(Call, DXIL::OpCode::CreateHandleFromBinding)) {
      DxilInst_CreateHandleFromBinding CreateHandleFromBinding(Call);
      DxilResourceBinding Binding =
          resource_helper::loadBindingFromCreateHandleFromBinding(
              CreateHandleFromBinding, HlslOP->GetHandleType(),
              *DM.GetShaderModel());
      Value *Index = CreateHandleFromBinding.get_index();
      if (auto *ConstantIndex = dyn_cast<ConstantInt>(Index)) {
        (void)ConstantIndex;
        return HlslOP->GetU32Const(UnknownIndex);
      }
      return Builder.CreateSub(Index,
                               HlslOP->GetU32Const(Binding.rangeLowerBound),
                               "IrtAsDynamicIndex");
    }
  }

  return HlslOP->GetU32Const(UnknownIndex);
}

Value *DxilPIXRayQueryLog::CreateRayQueryGetter(DxilModule &DM,
                                                IRBuilder<> &Builder,
                                                DXIL::OpCode OpCode,
                                                Type *ReturnType, Value *Handle,
                                                const Twine &Name) {
  OP *HlslOP = DM.GetOP();
  Function *GetterFunction = HlslOP->GetOpFunc(OpCode, ReturnType);
  return Builder.CreateCall(
      GetterFunction,
      {HlslOP->GetU32Const(static_cast<unsigned>(OpCode)), Handle}, Name);
}

Value *DxilPIXRayQueryLog::CreateRayQueryComponentGetter(
    DxilModule &DM, IRBuilder<> &Builder, DXIL::OpCode OpCode, Type *ReturnType,
    Value *Handle, uint8_t Component, const Twine &Name) {
  OP *HlslOP = DM.GetOP();
  Function *GetterFunction = HlslOP->GetOpFunc(OpCode, ReturnType);
  return Builder.CreateCall(GetterFunction,
                            {HlslOP->GetU32Const(static_cast<unsigned>(OpCode)),
                             Handle, HlslOP->GetI8Const(Component)},
                            Name);
}

bool DxilPIXRayQueryLog::EnsureIdentitySignatureInputs(
    DxilModule &DM, Function *TargetFunction) {
  if (DM.GetShaderModel()->IsLib()) {
    return false;
  }

  DxilSignature &InputSignature = DM.GetInputSignature();
  DXIL::ShaderKind ShaderKind =
      PIXPassHelpers::GetFunctionShaderKind(DM, TargetFunction);
  bool SignatureChanged = false;

  switch (ShaderKind) {
  case DXIL::ShaderKind::Vertex:
    SignatureChanged |= EnsureInputSignatureElement(
        InputSignature, DXIL::SigPointKind::VSIn, DXIL::SemanticKind::VertexID,
        Semantic::Get(DXIL::SemanticKind::VertexID)->GetName(),
        CompType::getU32(), DXIL::InterpolationMode::Undefined, 1, 1,
        GetNextSignatureRow(InputSignature), 0, 1);
    SignatureChanged |= EnsureInputSignatureElement(
        InputSignature, DXIL::SigPointKind::VSIn,
        DXIL::SemanticKind::InstanceID,
        Semantic::Get(DXIL::SemanticKind::InstanceID)->GetName(),
        CompType::getU32(), DXIL::InterpolationMode::Undefined, 1, 1,
        GetNextSignatureRow(InputSignature), 0, 1);
    break;
  case DXIL::ShaderKind::Pixel: {
    if (Options.UpstreamSVPositionRow >= 0) {
      SignatureChanged |= EnsureInputSignatureElement(
          InputSignature, DXIL::SigPointKind::PSIn,
          DXIL::SemanticKind::Position, "Position", CompType::getF32(),
          DXIL::InterpolationMode::Linear, 1, 4, Options.UpstreamSVPositionRow,
          0, 0xFu);
    }
    break;
  }
  default:
    break;
  }

  if (SignatureChanged) {
    DM.ReEmitDxilResources();
    // The serialized ViewID state describes the old signature; recompute it.
    DM.GetSerializedViewIdState().clear();
    legacy::PassManager ViewIdPassManager;
    ViewIdPassManager.add(createComputeViewIdStatePass());
    ViewIdPassManager.run(*DM.GetModule());
  }
  return SignatureChanged;
}

Value *DxilPIXRayQueryLog::CreateIdentityComponent(DxilModule &DM,
                                                   Function *TargetFunction,
                                                   IRBuilder<> &Builder,
                                                   unsigned Component) {
  LLVMContext &Ctx = TargetFunction->getContext();
  OP *HlslOP = DM.GetOP();
  DXIL::ShaderKind ShaderKind =
      PIXPassHelpers::GetFunctionShaderKind(DM, TargetFunction);

  if (DM.GetShaderModel()->IsLib() && !IsDxrShaderKind(ShaderKind)) {
    return HlslOP->GetU32Const(0);
  }

  if (ShaderKind == DXIL::ShaderKind::Compute ||
      ShaderKind == DXIL::ShaderKind::Amplification ||
      ShaderKind == DXIL::ShaderKind::Mesh) {
    Function *ThreadIdFunction =
        HlslOP->GetOpFunc(DXIL::OpCode::ThreadId, Type::getInt32Ty(Ctx));
    return Builder.CreateCall(
        ThreadIdFunction,
        {HlslOP->GetU32Const(static_cast<unsigned>(DXIL::OpCode::ThreadId)),
         HlslOP->GetU32Const(Component)},
        "IrtThreadId");
  }

  if (IsDxrShaderKind(ShaderKind)) {
    Function *DispatchRaysIndexFunction = HlslOP->GetOpFunc(
        DXIL::OpCode::DispatchRaysIndex, Type::getInt32Ty(Ctx));
    return Builder.CreateCall(DispatchRaysIndexFunction,
                              {HlslOP->GetU32Const(static_cast<unsigned>(
                                   DXIL::OpCode::DispatchRaysIndex)),
                               HlslOP->GetI8Const(Component)},
                              "IrtDispatchRaysIndex");
  }

  if (ShaderKind == DXIL::ShaderKind::Vertex) {
    unsigned ElementId = 0;
    if (Component == 0 &&
        TryFindInputSemantic(DM, DXIL::SemanticKind::VertexID, ElementId)) {
      return CreateSignatureInput(DM, Builder, ElementId, Type::getInt32Ty(Ctx),
                                  0, "IrtVertexId");
    }
    if (Component == 1 &&
        TryFindInputSemantic(DM, DXIL::SemanticKind::InstanceID, ElementId)) {
      return CreateSignatureInput(DM, Builder, ElementId, Type::getInt32Ty(Ctx),
                                  0, "IrtInstanceId");
    }
    return HlslOP->GetU32Const(0);
  }

  if (ShaderKind == DXIL::ShaderKind::Pixel) {
    unsigned ElementId = 0;
    if ((Component == 0 || Component == 1) &&
        TryFindInputSemantic(DM, DXIL::SemanticKind::Position, ElementId)) {
      Value *Position = CreateSignatureInput(
          DM, Builder, ElementId, Type::getFloatTy(Ctx),
          static_cast<uint8_t>(Component), "IrtPixelPosition");
      return Builder.CreateFPToUI(Position, Type::getInt32Ty(Ctx),
                                  "IrtPixelPositionIndex");
    }
    if (Component == 2 &&
        TryFindInputSemantic(DM, DXIL::SemanticKind::SampleIndex, ElementId)) {
      Function *SampleIndexFunction =
          HlslOP->GetOpFunc(DXIL::OpCode::SampleIndex, Type::getInt32Ty(Ctx));
      return Builder.CreateCall(SampleIndexFunction,
                                {HlslOP->GetU32Const(static_cast<unsigned>(
                                    DXIL::OpCode::SampleIndex))},
                                "IrtSampleIndex");
    }
    return HlslOP->GetU32Const(0);
  }

  if (ShaderKind == DXIL::ShaderKind::Geometry) {
    if (Component == 0) {
      Function *PrimitiveIdFunction =
          HlslOP->GetOpFunc(DXIL::OpCode::PrimitiveID, Type::getInt32Ty(Ctx));
      return Builder.CreateCall(PrimitiveIdFunction,
                                {HlslOP->GetU32Const(static_cast<unsigned>(
                                    DXIL::OpCode::PrimitiveID))},
                                "IrtPrimitiveId");
    }
    if (Component == 1) {
      Function *GsInstanceIdFunction =
          HlslOP->GetOpFunc(DXIL::OpCode::GSInstanceID, Type::getInt32Ty(Ctx));
      return Builder.CreateCall(GsInstanceIdFunction,
                                {HlslOP->GetU32Const(static_cast<unsigned>(
                                    DXIL::OpCode::GSInstanceID))},
                                "IrtGeometryShaderInstanceId");
    }
    return HlslOP->GetU32Const(0);
  }

  if (ShaderKind == DXIL::ShaderKind::Hull) {
    if (Component == 0) {
      Function *PrimitiveIdFunction =
          HlslOP->GetOpFunc(DXIL::OpCode::PrimitiveID, Type::getInt32Ty(Ctx));
      return Builder.CreateCall(PrimitiveIdFunction,
                                {HlslOP->GetU32Const(static_cast<unsigned>(
                                    DXIL::OpCode::PrimitiveID))},
                                "IrtPrimitiveId");
    }
    bool CanUseOutputControlPointId =
        (PIXPassHelpers::GetEntryFunction(DM) == TargetFunction &&
         !DM.IsPatchConstantShader(TargetFunction)) ||
        FunctionContainsDxilOp(TargetFunction,
                               DXIL::OpCode::OutputControlPointID);
    if (Component == 1 && CanUseOutputControlPointId) {
      Function *OutputControlPointIdFunction = HlslOP->GetOpFunc(
          DXIL::OpCode::OutputControlPointID, Type::getInt32Ty(Ctx));
      return Builder.CreateCall(OutputControlPointIdFunction,
                                {HlslOP->GetU32Const(static_cast<unsigned>(
                                    DXIL::OpCode::OutputControlPointID))},
                                "IrtOutputControlPointId");
    }
    return HlslOP->GetU32Const(0);
  }

  if (ShaderKind == DXIL::ShaderKind::Domain) {
    // SV_PrimitiveID has no signature element in a DS; it is always readable
    // through dx.op.primitiveID.
    if (Component == 0) {
      Function *PrimitiveIdFunction =
          HlslOP->GetOpFunc(DXIL::OpCode::PrimitiveID, Type::getInt32Ty(Ctx));
      return Builder.CreateCall(PrimitiveIdFunction,
                                {HlslOP->GetU32Const(static_cast<unsigned>(
                                    DXIL::OpCode::PrimitiveID))},
                                "IrtPrimitiveId");
    }
    return HlslOP->GetU32Const(0);
  }

  return HlslOP->GetU32Const(0);
}

Value *DxilPIXRayQueryLog::CreateDomainLocationHash(DxilModule &DM,
                                                    Function *TargetFunction,
                                                    IRBuilder<> &Builder) {
  LLVMContext &Ctx = TargetFunction->getContext();
  OP *HlslOP = DM.GetOP();
  Type *FloatType = Type::getFloatTy(Ctx);
  Type *Int32Type = Type::getInt32Ty(Ctx);
  Value *Zero = ConstantFP::get(FloatType, 0.0);
  Value *One = ConstantFP::get(FloatType, 1.0);
  Value *Scale = ConstantFP::get(FloatType, 65535.0);
  Function *DomainLocationFunction =
      HlslOP->GetOpFunc(DXIL::OpCode::DomainLocation, FloatType);

  auto CreateQuantizedOpComponent = [&](uint8_t Component,
                                        const Twine &Name) -> Value * {
    Value *DomainLocation =
        Builder.CreateCall(DomainLocationFunction,
                           {HlslOP->GetU32Const(static_cast<unsigned>(
                                DXIL::OpCode::DomainLocation)),
                            HlslOP->GetI8Const(Component)},
                           Name + "Input");
    Value *ClampedLow =
        Builder.CreateSelect(Builder.CreateFCmpOLT(DomainLocation, Zero), Zero,
                             DomainLocation, Name + "ClampLow");
    Value *Clamped =
        Builder.CreateSelect(Builder.CreateFCmpOGT(ClampedLow, One), One,
                             ClampedLow, Name + "ClampHigh");
    Value *Scaled = Builder.CreateFMul(Clamped, Scale, Name + "Scaled");
    Value *Rounded = Builder.CreateFAdd(Scaled, ConstantFP::get(FloatType, 0.5),
                                        Name + "RoundToNearest");
    return Builder.CreateFPToUI(Rounded, Int32Type, Name + "Quantized");
  };

  Value *QuantizedX = CreateQuantizedOpComponent(0, "IrtDomainLocationX");
  Value *QuantizedY = CreateQuantizedOpComponent(1, "IrtDomainLocationY");
  return Builder.CreateOr(
      Builder.CreateAnd(QuantizedX, HlslOP->GetU32Const(0xFFFFu)),
      Builder.CreateShl(
          Builder.CreateAnd(QuantizedY, HlslOP->GetU32Const(0xFFFFu)),
          HlslOP->GetU32Const(16)),
      "IrtDomainLocationHash");
}

std::pair<Value *, Value *> DxilPIXRayQueryLog::CreatePackedIdentity(
    DxilModule &DM, Function *TargetFunction, IRBuilder<> &Builder) {
  OP *HlslOP = DM.GetOP();
  DXIL::ShaderKind ShaderKind =
      PIXPassHelpers::GetFunctionShaderKind(DM, TargetFunction);

  if (ShaderKind == DXIL::ShaderKind::Pixel) {
    Value *PixelX = CreateIdentityComponent(DM, TargetFunction, Builder, 0);
    Value *PixelY = CreateIdentityComponent(DM, TargetFunction, Builder, 1);
    Value *IdentityLo = Builder.CreateOr(
        Builder.CreateAnd(PixelX, HlslOP->GetU32Const(0xFFFFu)),
        Builder.CreateShl(
            Builder.CreateAnd(PixelY, HlslOP->GetU32Const(0xFFFFu)),
            HlslOP->GetU32Const(16)),
        "IrtPixelIdentityLo");
    return {IdentityLo, HlslOP->GetU32Const(0)};
  }

  if (ShaderKind == DXIL::ShaderKind::Domain) {
    Value *IdentityLo = CreateIdentityComponent(DM, TargetFunction, Builder, 0);
    Value *IdentityHi = CreateDomainLocationHash(DM, TargetFunction, Builder);
    return {IdentityLo, IdentityHi};
  }

  Value *IdentityX = CreateIdentityComponent(DM, TargetFunction, Builder, 0);
  Value *IdentityY = CreateIdentityComponent(DM, TargetFunction, Builder, 1);
  Value *IdentityZ = CreateIdentityComponent(DM, TargetFunction, Builder, 2);
  Value *IdentityHi = Builder.CreateOr(
      Builder.CreateAnd(IdentityY, HlslOP->GetU32Const(0x000FFFFFu)),
      Builder.CreateShl(
          Builder.CreateAnd(IdentityZ, HlslOP->GetU32Const(0x00000FFFu)),
          HlslOP->GetU32Const(20)),
      "IrtIdentityHi");
  return {IdentityX, IdentityHi};
}

Value *DxilPIXRayQueryLog::CreateIdentityInvalid(DxilModule &DM,
                                                 Function *TargetFunction,
                                                 IRBuilder<> &Builder) {
  DXIL::ShaderKind ShaderKind =
      PIXPassHelpers::GetFunctionShaderKind(DM, TargetFunction);
  if (DM.GetShaderModel()->IsLib() && !IsDxrShaderKind(ShaderKind)) {
    return DM.GetOP()->GetI1Const(true);
  }
  switch (ShaderKind) {
  case DXIL::ShaderKind::Vertex: {
    unsigned ElementId = 0;
    return DM.GetOP()->GetI1Const(
        !TryFindInputSemantic(DM, DXIL::SemanticKind::VertexID, ElementId) ||
        !TryFindInputSemantic(DM, DXIL::SemanticKind::InstanceID, ElementId));
  }
  case DXIL::ShaderKind::Pixel: {
    unsigned ElementId = 0;
    return DM.GetOP()->GetI1Const(
        !TryFindInputSemantic(DM, DXIL::SemanticKind::Position, ElementId));
  }
  case DXIL::ShaderKind::Geometry:
  case DXIL::ShaderKind::Hull: {
    unsigned ElementId = 0;
    bool HasPrimitiveId = true;
    if (ShaderKind == DXIL::ShaderKind::Hull) {
      bool HasOutputControlPointId = TryFindInputSemantic(
          DM, DXIL::SemanticKind::OutputControlPointID, ElementId);
      bool CanUseOutputControlPointId =
          (PIXPassHelpers::GetEntryFunction(DM) == TargetFunction &&
           !DM.IsPatchConstantShader(TargetFunction)) ||
          FunctionContainsDxilOp(TargetFunction,
                                 DXIL::OpCode::OutputControlPointID);
      HasOutputControlPointId =
          HasOutputControlPointId || CanUseOutputControlPointId;
      return DM.GetOP()->GetI1Const(!HasPrimitiveId &&
                                    !HasOutputControlPointId);
    }
    return DM.GetOP()->GetI1Const(!HasPrimitiveId);
  }
  case DXIL::ShaderKind::Domain: {
    return DM.GetOP()->GetI1Const(false);
  }
  case DXIL::ShaderKind::Compute:
  case DXIL::ShaderKind::Amplification:
  case DXIL::ShaderKind::Mesh:
  case DXIL::ShaderKind::RayGeneration:
  case DXIL::ShaderKind::ClosestHit:
  case DXIL::ShaderKind::Miss:
  case DXIL::ShaderKind::Callable:
  case DXIL::ShaderKind::AnyHit:
  case DXIL::ShaderKind::Intersection:
    return DM.GetOP()->GetI1Const(false);
  default:
    return DM.GetOP()->GetI1Const(true);
  }
}

Value *
DxilPIXRayQueryLog::CreateSignatureInput(DxilModule &DM, IRBuilder<> &Builder,
                                         unsigned ElementId, Type *ReturnType,
                                         uint8_t Component, const Twine &Name) {
  OP *HlslOP = DM.GetOP();
  Function *LoadInputFunction =
      HlslOP->GetOpFunc(DXIL::OpCode::LoadInput, ReturnType);
  Value *UndefinedVertexId =
      UndefValue::get(Type::getInt32Ty(Builder.getContext()));
  return Builder.CreateCall(
      LoadInputFunction,
      {HlslOP->GetU32Const(static_cast<unsigned>(DXIL::OpCode::LoadInput)),
       HlslOP->GetU32Const(ElementId), HlslOP->GetU32Const(0),
       HlslOP->GetI8Const(Component), UndefinedVertexId},
      Name);
}

bool DxilPIXRayQueryLog::TryFindInputSemantic(DxilModule &DM,
                                              DXIL::SemanticKind SemanticKind,
                                              unsigned &ElementId) {
  for (const std::unique_ptr<DxilSignatureElement> &Element :
       DM.GetInputSignature().GetElements()) {
    if (Element != nullptr && Element->GetSemantic() != nullptr &&
        Element->GetSemantic()->GetKind() == SemanticKind) {
      ElementId = Element->GetID();
      return true;
    }
  }
  return false;
}

bool DxilPIXRayQueryLog::HelperLaneIsKnown(DxilModule &DM,
                                           Function *TargetFunction) {
  DXIL::ShaderKind ShaderKind =
      PIXPassHelpers::GetFunctionShaderKind(DM, TargetFunction);
  return ShaderKind == DXIL::ShaderKind::Pixel &&
         (DM.GetShaderModel()->GetMajor() > 6 ||
          (DM.GetShaderModel()->GetMajor() == 6 &&
           DM.GetShaderModel()->GetMinor() >= 6));
}

Value *DxilPIXRayQueryLog::CreateHelperLane(DxilModule &DM,
                                            Function *TargetFunction,
                                            IRBuilder<> &Builder) {
  LLVMContext &Ctx = TargetFunction->getContext();
  if (!HelperLaneIsKnown(DM, TargetFunction)) {
    return DM.GetOP()->GetI1Const(false);
  }

  OP *HlslOP = DM.GetOP();
  Function *HelperLaneFunction =
      HlslOP->GetOpFunc(DXIL::OpCode::IsHelperLane, Type::getInt1Ty(Ctx));
  return Builder.CreateCall(
      HelperLaneFunction,
      {HlslOP->GetU32Const(static_cast<unsigned>(DXIL::OpCode::IsHelperLane))},
      "IrtIsHelperLane");
}

Value *DxilPIXRayQueryLog::CreateHashMix(DxilModule &DM, IRBuilder<> &Builder,
                                         Value *ValueToMix) {
  OP *HlslOP = DM.GetOP();
  Value *State = Builder.CreateAdd(
      Builder.CreateMul(ValueToMix, HlslOP->GetU32Const(747796405u)),
      HlslOP->GetU32Const(2891336453u));
  Value *Shift =
      Builder.CreateAdd(Builder.CreateLShr(State, HlslOP->GetU32Const(28)),
                        HlslOP->GetU32Const(4));
  Value *Word = Builder.CreateMul(
      Builder.CreateXor(Builder.CreateLShr(State, Shift), State),
      HlslOP->GetU32Const(277803737u));
  return Builder.CreateXor(Builder.CreateLShr(Word, HlslOP->GetU32Const(22)),
                           Word);
}

Value *DxilPIXRayQueryLog::CreateSampleHash(DxilModule &DM,
                                            IRBuilder<> &Builder,
                                            ArrayRef<Value *> HashWords) {
  OP *HlslOP = DM.GetOP();
  Value *State = HlslOP->GetU32Const(SampleHashSeed);
  for (Value *HashWord : HashWords) {
    State = CreateHashMix(DM, Builder, Builder.CreateXor(State, HashWord));
  }
  return State;
}

void DxilPIXRayQueryLog::EmitRecordBody(
    DxilModule &DM, FunctionContext &Context, IRBuilder<> &Builder,
    Value *Handle, ShadowState &Shadow, uint32_t RecordType, uint32_t Reason,
    bool UseCommittedGetters, Value *EmitActive) {
  OP *HlslOP = DM.GetOP();
  LLVMContext &Ctx = Context.TargetFunction->getContext();
  Value *Zero = HlslOP->GetU32Const(0);
  Value *One = HlslOP->GetU32Const(1);
  if (EmitActive == nullptr) {
    EmitActive = HlslOP->GetI1Const(true);
  }
  Value *Unknown = HlslOP->GetU32Const(UnknownIndex);
  Value *QuietNaNBits = HlslOP->GetU32Const(0x7FC00000u);
  Value *Undef = UndefValue::get(Type::getInt32Ty(Ctx));

  Type *FloatType = Type::getFloatTy(Ctx);
  Value *FloatZero = ConstantFP::get(FloatType, 0.0);
  Value *OriginX = FloatZero;
  Value *OriginY = FloatZero;
  Value *OriginZ = FloatZero;
  Value *DirectionX = FloatZero;
  Value *DirectionY = FloatZero;
  Value *DirectionZ = FloatZero;
  Value *RayTMin = FloatZero;
  OriginX = Builder.CreateLoad(Shadow.OriginX, "IrtRecordOriginX");
  OriginY = Builder.CreateLoad(Shadow.OriginY, "IrtRecordOriginY");
  OriginZ = Builder.CreateLoad(Shadow.OriginZ, "IrtRecordOriginZ");
  DirectionX = Builder.CreateLoad(Shadow.DirectionX, "IrtRecordDirectionX");
  DirectionY = Builder.CreateLoad(Shadow.DirectionY, "IrtRecordDirectionY");
  DirectionZ = Builder.CreateLoad(Shadow.DirectionZ, "IrtRecordDirectionZ");
  RayTMin = Builder.CreateLoad(Shadow.RayTMin, "IrtRecordRayTMin");

  std::pair<Value *, Value *> PackedIdentity =
      CreatePackedIdentity(DM, Context.TargetFunction, Builder);
  Value *IdentityLo = PackedIdentity.first;
  Value *IdentityHi = PackedIdentity.second;
  Value *IdentityX =
      CreateIdentityComponent(DM, Context.TargetFunction, Builder, 0);
  Value *IdentityY =
      CreateIdentityComponent(DM, Context.TargetFunction, Builder, 1);
  Value *IdentityZ =
      CreateIdentityComponent(DM, Context.TargetFunction, Builder, 2);

  Value *IdentityOverflow = Builder.CreateOr(
      Builder.CreateICmpUGT(IdentityY, HlslOP->GetU32Const(0x000FFFFFu)),
      Builder.CreateICmpUGT(IdentityZ, HlslOP->GetU32Const(0x00000FFFu)));
  Value *IdentityInvalid =
      CreateIdentityInvalid(DM, Context.TargetFunction, Builder);
  Value *HelperLane = CreateHelperLane(DM, Context.TargetFunction, Builder);
  bool HelperLaneCanBeKnown = HelperLaneIsKnown(DM, Context.TargetFunction);
  Value *HelperLaneUnknown = HlslOP->GetI1Const(
      PIXPassHelpers::GetFunctionShaderKind(DM, Context.TargetFunction) ==
          DXIL::ShaderKind::Pixel &&
      !HelperLaneCanBeKnown);

  Value *PassesRoi = Builder.CreateAnd(
      Builder.CreateICmpUGE(IdentityX, HlslOP->GetU32Const(Options.RoiMinX)),
      Builder.CreateICmpULE(IdentityX, HlslOP->GetU32Const(Options.RoiMaxX)));
  PassesRoi = Builder.CreateAnd(
      PassesRoi,
      Builder.CreateAnd(Builder.CreateICmpUGE(
                            IdentityY, HlslOP->GetU32Const(Options.RoiMinY)),
                        Builder.CreateICmpULE(
                            IdentityY, HlslOP->GetU32Const(Options.RoiMaxY))));
  PassesRoi = Builder.CreateAnd(
      PassesRoi,
      Builder.CreateAnd(Builder.CreateICmpUGE(
                            IdentityZ, HlslOP->GetU32Const(Options.RoiMinZ)),
                        Builder.CreateICmpULE(
                            IdentityZ, HlslOP->GetU32Const(Options.RoiMaxZ))));

  Value *PassesSampling = HlslOP->GetI1Const(true);
  if (Options.SampleRate > 1) {
    Value *SampleHash = CreateSampleHash(
        DM, Builder,
        {IdentityLo, IdentityHi,
         Builder.CreateBitCast(OriginX, Type::getInt32Ty(Ctx)),
         Builder.CreateBitCast(OriginY, Type::getInt32Ty(Ctx)),
         Builder.CreateBitCast(OriginZ, Type::getInt32Ty(Ctx)),
         Builder.CreateBitCast(DirectionX, Type::getInt32Ty(Ctx)),
         Builder.CreateBitCast(DirectionY, Type::getInt32Ty(Ctx)),
         Builder.CreateBitCast(DirectionZ, Type::getInt32Ty(Ctx)),
         Builder.CreateBitCast(RayTMin, Type::getInt32Ty(Ctx))});
    PassesSampling = Builder.CreateICmpEQ(
        Builder.CreateURem(SampleHash, HlslOP->GetU32Const(Options.SampleRate)),
        Zero);
  }

  Value *ShouldWrite = Builder.CreateAnd(PassesRoi, PassesSampling);
  ShouldWrite = Builder.CreateAnd(ShouldWrite, EmitActive, "IrtActiveWrite");
  if (HelperLaneCanBeKnown) {
    ShouldWrite = Builder.CreateAnd(ShouldWrite, Builder.CreateNot(HelperLane),
                                    "IrtSuppressHelperLane");
  }

  // Filtered events (ROI, sampling, inactive emit, helper lane) branch around
  // the counter atomics, RayQuery getters, and payload stores entirely. Nothing
  // computed below is used after the record body, so no PHIs are needed at the
  // join point.
  TerminatorInst *WriteTerminator =
      SplitBlockAndInsertIfThen(ShouldWrite, &*Builder.GetInsertPoint(), false);
  IRBuilder<> WriteBuilder(WriteTerminator);
  Instruction *AfterHitInstruction = WriteTerminator;

  Value *CommittedT = nullptr;
  Value *CommittedStatus = nullptr;
  if (!UseCommittedGetters) {
    CommittedT =
        WriteBuilder.CreateLoad(Shadow.RayTMax, "IrtSyntheticCommittedT");
    CommittedStatus = Zero;
  } else if (RecordType == CandidateRecordType) {
    CommittedT = WriteBuilder.CreateBitCast(Unknown, Type::getFloatTy(Ctx),
                                            "IrtCandidateTUnavailable");
    CommittedStatus = CreateRayQueryGetter(
        DM, WriteBuilder, DXIL::OpCode::RayQuery_CandidateType,
        Type::getInt32Ty(Ctx), Handle, "IrtCandidateType");
  } else {
    CommittedStatus = CreateRayQueryGetter(
        DM, WriteBuilder, DXIL::OpCode::RayQuery_CommittedStatus,
        Type::getInt32Ty(Ctx), Handle, "IrtCommittedStatus");
    CommittedT = CreateRayQueryGetter(
        DM, WriteBuilder, DXIL::OpCode::RayQuery_CommittedRayT,
        Type::getFloatTy(Ctx), Handle, "IrtCommittedT");
  }

  Value *LoadedTraceSite = WriteBuilder.CreateLoad(Shadow.TraceSiteId);
  Value *LoadedTraceInvocation = WriteBuilder.CreateLoad(
      Shadow.TraceInvocation, "IrtLoadedTraceInvocation");
  Value *LoadedInstanceMask = WriteBuilder.CreateLoad(Shadow.InstanceMask);
  Value *LoadedEffectiveFlags =
      WriteBuilder.CreateLoad(Shadow.EffectiveRayFlags);
  Value *LoadedAsDynamicIndex = WriteBuilder.CreateLoad(Shadow.AsDynamicIndex);

  Value *NoHitIndexValue = RecordType == CandidateRecordType ? Unknown : Zero;
  Value *CommittedInstanceIndex = NoHitIndexValue;
  Value *CommittedGeometryIndex = NoHitIndexValue;
  Value *CommittedPrimitiveIndex = NoHitIndexValue;
  if (UseCommittedGetters) {
    Value *HasCommittedHit =
        RecordType == CandidateRecordType
            ? HlslOP->GetI1Const(true)
            : WriteBuilder.CreateICmpNE(CommittedStatus, Zero,
                                        "IrtHasCommittedHit");
    BasicBlock *NoHitBlock = AfterHitInstruction->getParent();
    TerminatorInst *HitTerminator =
        SplitBlockAndInsertIfThen(HasCommittedHit, AfterHitInstruction, false);
    IRBuilder<> HitBuilder(HitTerminator);
    DXIL::OpCode InstanceIndexOp =
        RecordType == CandidateRecordType
            ? DXIL::OpCode::RayQuery_CandidateInstanceIndex
            : DXIL::OpCode::RayQuery_CommittedInstanceIndex;
    DXIL::OpCode GeometryIndexOp =
        RecordType == CandidateRecordType
            ? DXIL::OpCode::RayQuery_CandidateGeometryIndex
            : DXIL::OpCode::RayQuery_CommittedGeometryIndex;
    DXIL::OpCode PrimitiveIndexOp =
        RecordType == CandidateRecordType
            ? DXIL::OpCode::RayQuery_CandidatePrimitiveIndex
            : DXIL::OpCode::RayQuery_CommittedPrimitiveIndex;
    Value *HitInstanceIndex =
        CreateRayQueryGetter(DM, HitBuilder, InstanceIndexOp,
                             Type::getInt32Ty(Ctx), Handle, "IrtInstanceIndex");
    Value *HitGeometryIndex =
        CreateRayQueryGetter(DM, HitBuilder, GeometryIndexOp,
                             Type::getInt32Ty(Ctx), Handle, "IrtGeometryIndex");
    Value *HitPrimitiveIndex = CreateRayQueryGetter(
        DM, HitBuilder, PrimitiveIndexOp, Type::getInt32Ty(Ctx), Handle,
        "IrtPrimitiveIndex");

    IRBuilder<> AfterHitBuilder(AfterHitInstruction);
    PHINode *SelectedInstanceIndex = AfterHitBuilder.CreatePHI(
        Type::getInt32Ty(Ctx), 2, "IrtCommittedInstanceIndexSelected");
    SelectedInstanceIndex->addIncoming(NoHitIndexValue, NoHitBlock);
    SelectedInstanceIndex->addIncoming(HitInstanceIndex,
                                       HitTerminator->getParent());
    PHINode *SelectedGeometryIndex = AfterHitBuilder.CreatePHI(
        Type::getInt32Ty(Ctx), 2, "IrtCommittedGeometryIndexSelected");
    SelectedGeometryIndex->addIncoming(NoHitIndexValue, NoHitBlock);
    SelectedGeometryIndex->addIncoming(HitGeometryIndex,
                                       HitTerminator->getParent());
    PHINode *SelectedPrimitiveIndex = AfterHitBuilder.CreatePHI(
        Type::getInt32Ty(Ctx), 2, "IrtCommittedPrimitiveIndexSelected");
    SelectedPrimitiveIndex->addIncoming(NoHitIndexValue, NoHitBlock);
    SelectedPrimitiveIndex->addIncoming(HitPrimitiveIndex,
                                        HitTerminator->getParent());
    CommittedInstanceIndex = SelectedInstanceIndex;
    CommittedGeometryIndex = SelectedGeometryIndex;
    CommittedPrimitiveIndex = SelectedPrimitiveIndex;
  }

  IRBuilder<> AfterHitBuilder(AfterHitInstruction);
  Value *Capacity =
      HlslOP->GetU32Const(static_cast<uint32_t>(Options.MaxNumEntriesInLog));
  Function *AtomicFunction =
      HlslOP->GetOpFunc(OP::OpCode::AtomicBinOp, Type::getInt32Ty(Ctx));
  Value *AtomicOpcode =
      HlslOP->GetU32Const(static_cast<unsigned>(OP::OpCode::AtomicBinOp));
  Value *AtomicAdd =
      HlslOP->GetU32Const(static_cast<unsigned>(DXIL::AtomicBinOpCode::Add));
  Value *AtomicUMax =
      HlslOP->GetU32Const(static_cast<unsigned>(DXIL::AtomicBinOpCode::UMax));
  Value *AtomicUMin =
      HlslOP->GetU32Const(static_cast<unsigned>(DXIL::AtomicBinOpCode::UMin));
  Value *DemandValue = AfterHitBuilder.CreateCall(
      AtomicFunction,
      {AtomicOpcode,
       RecordType == CandidateRecordType ? Context.CandidateCounterUAVHandle
                                         : Context.CounterUAVHandle,
       AtomicAdd, HlslOP->GetU32Const(DemandCounterOffset), Undef, Undef, One},
      "IrtDemand");
  Value *DemandNext =
      AfterHitBuilder.CreateAdd(DemandValue, One, "IrtDemandNext");
  Value *DemandClamped = AfterHitBuilder.CreateSelect(
      AfterHitBuilder.CreateICmpULT(
          DemandValue,
          HlslOP->GetU32Const(std::numeric_limits<uint32_t>::max())),
      DemandNext, HlslOP->GetU32Const(std::numeric_limits<uint32_t>::max()),
      "IrtDemandClamped");
  AfterHitBuilder.CreateCall(
      AtomicFunction,
      {AtomicOpcode,
       RecordType == CandidateRecordType ? Context.CandidateCounterUAVHandle
                                         : Context.CounterUAVHandle,
       AtomicUMax, HlslOP->GetU32Const(DemandCounterOffset), Undef, Undef,
       DemandClamped},
      "IrtDemandClamp");

  Value *SlotBeforeReservation = AfterHitBuilder.CreateCall(
      AtomicFunction,
      {AtomicOpcode,
       RecordType == CandidateRecordType ? Context.CandidateCounterUAVHandle
                                         : Context.CounterUAVHandle,
       AtomicAdd, HlslOP->GetU32Const(ReservationCounterOffset), Undef, Undef,
       One},
      "IrtSlot");
  AfterHitBuilder.CreateCall(
      AtomicFunction,
      {AtomicOpcode,
       RecordType == CandidateRecordType ? Context.CandidateCounterUAVHandle
                                         : Context.CounterUAVHandle,
       AtomicUMin, HlslOP->GetU32Const(ReservationCounterOffset), Undef, Undef,
       Capacity},
      "IrtSlotClamp");
  Value *ShouldStore = AfterHitBuilder.CreateICmpULT(
      SlotBeforeReservation, Capacity, "IrtShouldStore");
  IRBuilder<> StoreBuilder(AfterHitInstruction);

  Function *StoreIntFunction =
      HlslOP->GetOpFunc(OP::OpCode::RawBufferStore, Type::getInt32Ty(Ctx));
  Value *StoreOpcode =
      HlslOP->GetU32Const(static_cast<unsigned>(OP::OpCode::RawBufferStore));
  Value *StoreAlignment = HlslOP->GetU32Const(4);
  uint32_t RecordStride = RecordType == CandidateRecordType
                              ? CandidateRecordStrideBytes
                              : RecordStrideBytes;
  Value *BaseOffset = StoreBuilder.CreateMul(SlotBeforeReservation,
                                             HlslOP->GetU32Const(RecordStride),
                                             "IrtRecordOffset");
  Value *Offset16 = StoreBuilder.CreateAdd(BaseOffset, HlslOP->GetU32Const(16));
  Value *Offset32 = StoreBuilder.CreateAdd(BaseOffset, HlslOP->GetU32Const(32));

  Value *InstanceMaskHigh = StoreBuilder.CreateShl(
      StoreBuilder.CreateAnd(LoadedInstanceMask, HlslOP->GetU32Const(0xFFu)),
      HlslOP->GetU32Const(24));
  Value *InstanceIndexLow = StoreBuilder.CreateAnd(
      CommittedInstanceIndex, HlslOP->GetU32Const(0x00FFFFFFu));
  Value *InstIdxMask =
      StoreBuilder.CreateOr(InstanceIndexLow, InstanceMaskHigh);
  Value *EncodedStatus = RecordType == CandidateRecordType
                             ? StoreBuilder.CreateAdd(CommittedStatus, One,
                                                      "IrtEncodedCandidateType")
                             : CommittedStatus;
  Value *GeomStatus = StoreBuilder.CreateOr(
      StoreBuilder.CreateAnd(CommittedGeometryIndex,
                             HlslOP->GetU32Const(0x00FFFFFFu)),
      StoreBuilder.CreateShl(
          StoreBuilder.CreateAnd(EncodedStatus, HlslOP->GetU32Const(0x3u)),
          HlslOP->GetU32Const(24)));
  GeomStatus = StoreBuilder.CreateOr(
      GeomStatus, StoreBuilder.CreateShl(HlslOP->GetU32Const(Reason & 0x3u),
                                         HlslOP->GetU32Const(26)));
  GeomStatus = StoreBuilder.CreateSelect(
      IdentityOverflow,
      StoreBuilder.CreateOr(GeomStatus,
                            HlslOP->GetU32Const(GeomStatusIdentityOverflow)),
      GeomStatus);
  GeomStatus = StoreBuilder.CreateSelect(
      IdentityInvalid,
      StoreBuilder.CreateOr(GeomStatus,
                            HlslOP->GetU32Const(GeomStatusIdentityInvalid)),
      GeomStatus);
  GeomStatus = StoreBuilder.CreateSelect(
      HelperLane,
      StoreBuilder.CreateOr(GeomStatus, HlslOP->GetU32Const(GeomStatusHelper)),
      GeomStatus);
  GeomStatus = StoreBuilder.CreateSelect(
      HelperLaneUnknown,
      StoreBuilder.CreateOr(GeomStatus,
                            HlslOP->GetU32Const(GeomStatusHelperUnknown)),
      GeomStatus);
  Value *SiteFlags = StoreBuilder.CreateOr(
      StoreBuilder.CreateAnd(LoadedTraceSite, HlslOP->GetU32Const(0xFFFFu)),
      StoreBuilder.CreateShl(
          StoreBuilder.CreateAnd(LoadedEffectiveFlags,
                                 HlslOP->GetU32Const(0xFFFFu)),
          HlslOP->GetU32Const(16)));
  Value *Header = StoreBuilder.CreateOr(
      HlslOP->GetU32Const(RecordType | (RecordFormatVersion << 4)),
      StoreBuilder.CreateShl(
          HlslOP->GetU32Const(Options.SubCallIndex & 0xFFFFu),
          HlslOP->GetU32Const(8)));
  Header = StoreBuilder.CreateOr(
      Header,
      StoreBuilder.CreateShl(StoreBuilder.CreateAnd(LoadedTraceInvocation,
                                                    HlslOP->GetU32Const(0xFFu)),
                             HlslOP->GetU32Const(24)));
  Value *SelectedLogUAVHandle = RecordType == CandidateRecordType
                                    ? Context.CandidateLogUAVHandle
                                    : Context.LogUAVHandle;

  if (RecordType == CandidateRecordType) {
    Value *CandidateIsTriangle = StoreBuilder.CreateICmpEQ(
        CommittedStatus, HlslOP->GetU32Const(RawCandidateTypeTriangle),
        "IrtCandidateIsTriangle");
    BasicBlock *NotTriangleBlock = AfterHitInstruction->getParent();
    TerminatorInst *TriangleTerminator = SplitBlockAndInsertIfThen(
        CandidateIsTriangle, AfterHitInstruction, false);
    IRBuilder<> TriangleBuilder(TriangleTerminator);
    Value *TriangleCandidateT = CreateRayQueryGetter(
        DM, TriangleBuilder, DXIL::OpCode::RayQuery_CandidateTriangleRayT,
        Type::getFloatTy(Ctx), Handle, "IrtCandidateT");
    Value *TriangleCandidateBaryX = CreateRayQueryComponentGetter(
        DM, TriangleBuilder,
        DXIL::OpCode::RayQuery_CandidateTriangleBarycentrics,
        Type::getFloatTy(Ctx), Handle, 0, "IrtCandidateBaryX");
    Value *TriangleCandidateBaryY = CreateRayQueryComponentGetter(
        DM, TriangleBuilder,
        DXIL::OpCode::RayQuery_CandidateTriangleBarycentrics,
        Type::getFloatTy(Ctx), Handle, 1, "IrtCandidateBaryY");

    IRBuilder<> AfterTriangleBuilder(AfterHitInstruction);
    PHINode *CandidateRayTBits = AfterTriangleBuilder.CreatePHI(
        Type::getInt32Ty(Ctx), 2, "IrtCandidateRayTBits");
    CandidateRayTBits->addIncoming(Unknown, NotTriangleBlock);
    CandidateRayTBits->addIncoming(
        TriangleBuilder.CreateBitCast(TriangleCandidateT,
                                      Type::getInt32Ty(Ctx)),
        TriangleTerminator->getParent());
    PHINode *CandidateBaryXBits = AfterTriangleBuilder.CreatePHI(
        Type::getInt32Ty(Ctx), 2, "IrtCandidateBaryXBits");
    CandidateBaryXBits->addIncoming(QuietNaNBits, NotTriangleBlock);
    CandidateBaryXBits->addIncoming(
        TriangleBuilder.CreateBitCast(TriangleCandidateBaryX,
                                      Type::getInt32Ty(Ctx)),
        TriangleTerminator->getParent());
    PHINode *CandidateBaryYBits = AfterTriangleBuilder.CreatePHI(
        Type::getInt32Ty(Ctx), 2, "IrtCandidateBaryYBits");
    CandidateBaryYBits->addIncoming(QuietNaNBits, NotTriangleBlock);
    CandidateBaryYBits->addIncoming(
        TriangleBuilder.CreateBitCast(TriangleCandidateBaryY,
                                      Type::getInt32Ty(Ctx)),
        TriangleTerminator->getParent());

    Value *CandidateIsProcedural = AfterTriangleBuilder.CreateICmpEQ(
        CommittedStatus, HlslOP->GetU32Const(RawCandidateTypeProcedural),
        "IrtCandidateIsProcedural");
    BasicBlock *NotProceduralBlock = AfterHitInstruction->getParent();
    TerminatorInst *ProceduralTerminator = SplitBlockAndInsertIfThen(
        CandidateIsProcedural, AfterHitInstruction, false);
    IRBuilder<> ProceduralBuilder(ProceduralTerminator);
    Value *ProceduralNonOpaque = CreateRayQueryGetter(
        DM, ProceduralBuilder,
        DXIL::OpCode::RayQuery_CandidateProceduralPrimitiveNonOpaque,
        Type::getInt1Ty(Ctx), Handle, "IrtCandidateProceduralNonOpaque");

    IRBuilder<> AfterProceduralBuilder(AfterHitInstruction);
    PHINode *CandidateProceduralNonOpaque = AfterProceduralBuilder.CreatePHI(
        Type::getInt1Ty(Ctx), 2, "IrtCandidateProceduralNonOpaqueSelected");
    CandidateProceduralNonOpaque->addIncoming(HlslOP->GetI1Const(false),
                                              NotProceduralBlock);
    CandidateProceduralNonOpaque->addIncoming(
        ProceduralNonOpaque, ProceduralTerminator->getParent());

    Value *CandidateTypeFlags = AfterProceduralBuilder.CreateSelect(
        CandidateProceduralNonOpaque,
        AfterProceduralBuilder.CreateOr(
            EncodedStatus,
            HlslOP->GetU32Const(CandidateProceduralNonOpaqueFlag)),
        EncodedStatus);

    TerminatorInst *StoreTerminator =
        SplitBlockAndInsertIfThen(ShouldStore, AfterHitInstruction, false);
    IRBuilder<> GuardedStoreBuilder(StoreTerminator);
    GuardedStoreBuilder.CreateCall(
        StoreIntFunction,
        {StoreOpcode, SelectedLogUAVHandle, BaseOffset, Undef,
         CandidateRayTBits, CandidateBaryXBits, CandidateBaryYBits,
         CandidateTypeFlags, HlslOP->GetI8Const(15), StoreAlignment});
    GuardedStoreBuilder.CreateCall(StoreIntFunction,
                                   {StoreOpcode, SelectedLogUAVHandle, Offset16,
                                    Undef, IdentityLo, IdentityHi, InstIdxMask,
                                    CommittedPrimitiveIndex,
                                    HlslOP->GetI8Const(15), StoreAlignment});
    GuardedStoreBuilder.CreateCall(
        StoreIntFunction, {StoreOpcode, SelectedLogUAVHandle, Offset32, Undef,
                           GeomStatus, SiteFlags, LoadedAsDynamicIndex, Header,
                           HlslOP->GetI8Const(15), StoreAlignment});
  } else {
    Value *Offset48 =
        StoreBuilder.CreateAdd(BaseOffset, HlslOP->GetU32Const(48));
    Value *OriginXBits =
        StoreBuilder.CreateBitCast(OriginX, Type::getInt32Ty(Ctx));
    Value *OriginYBits =
        StoreBuilder.CreateBitCast(OriginY, Type::getInt32Ty(Ctx));
    Value *OriginZBits =
        StoreBuilder.CreateBitCast(OriginZ, Type::getInt32Ty(Ctx));
    Value *RayTMinBits =
        StoreBuilder.CreateBitCast(RayTMin, Type::getInt32Ty(Ctx));
    Value *DirectionXBits =
        StoreBuilder.CreateBitCast(DirectionX, Type::getInt32Ty(Ctx));
    Value *DirectionYBits =
        StoreBuilder.CreateBitCast(DirectionY, Type::getInt32Ty(Ctx));
    Value *DirectionZBits =
        StoreBuilder.CreateBitCast(DirectionZ, Type::getInt32Ty(Ctx));
    Value *CommittedTBits =
        StoreBuilder.CreateBitCast(CommittedT, Type::getInt32Ty(Ctx));
    TerminatorInst *StoreTerminator =
        SplitBlockAndInsertIfThen(ShouldStore, AfterHitInstruction, false);
    IRBuilder<> GuardedStoreBuilder(StoreTerminator);
    GuardedStoreBuilder.CreateCall(
        StoreIntFunction, {StoreOpcode, SelectedLogUAVHandle, BaseOffset, Undef,
                           OriginXBits, OriginYBits, OriginZBits, RayTMinBits,
                           HlslOP->GetI8Const(15), StoreAlignment});
    GuardedStoreBuilder.CreateCall(StoreIntFunction,
                                   {StoreOpcode, SelectedLogUAVHandle, Offset16,
                                    Undef, DirectionXBits, DirectionYBits,
                                    DirectionZBits, CommittedTBits,
                                    HlslOP->GetI8Const(15), StoreAlignment});
    GuardedStoreBuilder.CreateCall(StoreIntFunction,
                                   {StoreOpcode, SelectedLogUAVHandle, Offset32,
                                    Undef, IdentityLo, IdentityHi, InstIdxMask,
                                    CommittedPrimitiveIndex,
                                    HlslOP->GetI8Const(15), StoreAlignment});
    GuardedStoreBuilder.CreateCall(
        StoreIntFunction, {StoreOpcode, SelectedLogUAVHandle, Offset48, Undef,
                           GeomStatus, SiteFlags, LoadedAsDynamicIndex, Header,
                           HlslOP->GetI8Const(15), StoreAlignment});
  }
}

uint32_t DxilPIXRayQueryLog::GetTemplateFlagsForHandle(FunctionContext &Context,
                                                       Value *Handle) {
  SmallPtrSet<Value *, 8> VisitedValues;
  return GetTemplateFlagsForValue(Context, Handle, VisitedValues);
}

uint32_t DxilPIXRayQueryLog::GetTemplateFlagsForValue(
    FunctionContext &Context, Value *ValueToRead,
    SmallPtrSetImpl<Value *> &VisitedValues) {
  if (ValueToRead == nullptr || !VisitedValues.insert(ValueToRead).second) {
    return 0;
  }

  Value *Key = GetHandleKey(ValueToRead);
  auto TemplateFlagIterator = Context.TemplateFlagsByKey.find(Key);
  if (TemplateFlagIterator != Context.TemplateFlagsByKey.end()) {
    return TemplateFlagIterator->second;
  }

  if (auto *Phi = dyn_cast<PHINode>(ValueToRead)) {
    uint32_t Flags = 0;
    for (Value *IncomingValue : Phi->incoming_values()) {
      Flags |= GetTemplateFlagsForValue(Context, IncomingValue, VisitedValues);
    }
    return Flags;
  }

  if (auto *Select = dyn_cast<SelectInst>(ValueToRead)) {
    return GetTemplateFlagsForValue(Context, Select->getTrueValue(),
                                    VisitedValues) |
           GetTemplateFlagsForValue(Context, Select->getFalseValue(),
                                    VisitedValues);
  }

  if (auto *Load = dyn_cast<LoadInst>(ValueToRead)) {
    uint32_t Flags = 0;
    Value *Pointer = Load->getPointerOperand();
    Value *ArrayBasePointer = nullptr;
    Value *ArrayElementIndex = nullptr;
    uint32_t ArrayElementCount = 0;
    if (TryGetRayQueryArrayStorage(ValueToRead, ArrayBasePointer,
                                   ArrayElementIndex, ArrayElementCount)) {
      for (inst_iterator
               InstructionIterator = inst_begin(Context.TargetFunction),
               InstructionEnd = inst_end(Context.TargetFunction);
           InstructionIterator != InstructionEnd; ++InstructionIterator) {
        Instruction &InstructionToCheck = *InstructionIterator;
        if (&InstructionToCheck == Load) {
          break;
        }
        auto *Store = dyn_cast<StoreInst>(&InstructionToCheck);
        if (Store == nullptr) {
          continue;
        }
        auto *StoreGetElementPointer =
            dyn_cast<GetElementPtrInst>(Store->getPointerOperand());
        if (StoreGetElementPointer != nullptr &&
            StoreGetElementPointer->getPointerOperand() == ArrayBasePointer) {
          Flags |= GetTemplateFlagsForValue(Context, Store->getValueOperand(),
                                            VisitedValues);
        }
      }
      return Flags;
    }

    for (inst_iterator InstructionIterator = inst_begin(Context.TargetFunction),
                       InstructionEnd = inst_end(Context.TargetFunction);
         InstructionIterator != InstructionEnd; ++InstructionIterator) {
      Instruction &InstructionToCheck = *InstructionIterator;
      if (&InstructionToCheck == Load) {
        break;
      }
      auto *Store = dyn_cast<StoreInst>(&InstructionToCheck);
      if (Store != nullptr &&
          AreEquivalentPointers(Store->getPointerOperand(), Pointer)) {
        Flags |= GetTemplateFlagsForValue(Context, Store->getValueOperand(),
                                          VisitedValues);
      }
    }
    return Flags;
  }

  return 0;
}

PointerAliasKind DxilPIXRayQueryLog::ClassifyPointerAlias(
    FunctionContext &Context, Value *FirstPointer, Value *SecondPointer) {
  FirstPointer = FirstPointer->stripPointerCasts();
  SecondPointer = SecondPointer->stripPointerCasts();
  if (AreEquivalentPointers(FirstPointer, SecondPointer)) {
    return PointerAliasKind::Equivalent;
  }

  const DataLayout &DataLayout =
      Context.TargetFunction->getParent()->getDataLayout();
  Value *FirstUnderlying = GetUnderlyingObject(FirstPointer, DataLayout);
  Value *SecondUnderlying = GetUnderlyingObject(SecondPointer, DataLayout);
  if (FirstUnderlying != SecondUnderlying) {
    if ((isa<AllocaInst>(FirstUnderlying) ||
         isa<GlobalVariable>(FirstUnderlying)) &&
        (isa<AllocaInst>(SecondUnderlying) ||
         isa<GlobalVariable>(SecondUnderlying))) {
      return PointerAliasKind::Distinct;
    }
    return PointerAliasKind::MayAlias;
  }

  auto *FirstGetElementPointer = dyn_cast<GEPOperator>(FirstPointer);
  auto *SecondGetElementPointer = dyn_cast<GEPOperator>(SecondPointer);
  if (FirstGetElementPointer == nullptr || SecondGetElementPointer == nullptr ||
      FirstGetElementPointer->getNumIndices() !=
          SecondGetElementPointer->getNumIndices() ||
      FirstGetElementPointer->getPointerOperandType() !=
          SecondGetElementPointer->getPointerOperandType() ||
      !AreEquivalentPointers(FirstGetElementPointer->getPointerOperand(),
                             SecondGetElementPointer->getPointerOperand())) {
    return PointerAliasKind::MayAlias;
  }

  auto FirstIndex = FirstGetElementPointer->idx_begin();
  auto SecondIndex = SecondGetElementPointer->idx_begin();
  for (; FirstIndex != FirstGetElementPointer->idx_end();
       ++FirstIndex, ++SecondIndex) {
    Value *FirstIndexValue = FirstIndex->get();
    Value *SecondIndexValue = SecondIndex->get();
    if (FirstIndexValue == SecondIndexValue) {
      continue;
    }

    auto *FirstConstant = dyn_cast<ConstantInt>(FirstIndexValue);
    auto *SecondConstant = dyn_cast<ConstantInt>(SecondIndexValue);
    if (FirstConstant == nullptr || SecondConstant == nullptr) {
      return PointerAliasKind::MayAlias;
    }

    if (FirstConstant->getValue() != SecondConstant->getValue()) {
      return PointerAliasKind::Distinct;
    }
  }

  return PointerAliasKind::MayAlias;
}

bool DxilPIXRayQueryLog::AreEquivalentPointers(Value *FirstPointer,
                                               Value *SecondPointer) {
  FirstPointer = FirstPointer->stripPointerCasts();
  SecondPointer = SecondPointer->stripPointerCasts();
  if (FirstPointer == SecondPointer) {
    return true;
  }

  auto *FirstGetElementPointer = dyn_cast<GEPOperator>(FirstPointer);
  auto *SecondGetElementPointer = dyn_cast<GEPOperator>(SecondPointer);
  if (FirstGetElementPointer == nullptr || SecondGetElementPointer == nullptr) {
    return false;
  }

  if (!AreEquivalentPointers(FirstGetElementPointer->getPointerOperand(),
                             SecondGetElementPointer->getPointerOperand())) {
    return false;
  }

  if (FirstGetElementPointer->getNumIndices() !=
      SecondGetElementPointer->getNumIndices()) {
    return false;
  }

  auto FirstIndex = FirstGetElementPointer->idx_begin();
  auto SecondIndex = SecondGetElementPointer->idx_begin();
  for (; FirstIndex != FirstGetElementPointer->idx_end();
       ++FirstIndex, ++SecondIndex) {
    Value *FirstIndexValue = FirstIndex->get();
    Value *SecondIndexValue = SecondIndex->get();
    if (FirstIndexValue == SecondIndexValue) {
      continue;
    }
    auto *FirstConstant = dyn_cast<ConstantInt>(FirstIndexValue);
    auto *SecondConstant = dyn_cast<ConstantInt>(SecondIndexValue);
    if (FirstConstant == nullptr || SecondConstant == nullptr ||
        FirstConstant->getZExtValue() != SecondConstant->getZExtValue()) {
      return false;
    }
  }

  return true;
}

bool DxilPIXRayQueryLog::TryGetRayQueryArrayStorage(Value *Handle,
                                                    Value *&BasePointer,
                                                    Value *&ElementIndex,
                                                    uint32_t &ElementCount) {
  auto *Load = dyn_cast<LoadInst>(Handle);
  if (Load == nullptr) {
    return false;
  }

  auto *GetElementPointer =
      dyn_cast<GetElementPtrInst>(Load->getPointerOperand());
  if (GetElementPointer == nullptr || GetElementPointer->getNumIndices() != 2) {
    return false;
  }

  auto *ArrayAlloca =
      dyn_cast<AllocaInst>(GetElementPointer->getPointerOperand());
  if (ArrayAlloca == nullptr) {
    return false;
  }

  auto *ArrayStorageType = dyn_cast<ArrayType>(ArrayAlloca->getAllocatedType());
  if (ArrayStorageType == nullptr ||
      !ArrayStorageType->getElementType()->isIntegerTy(32)) {
    return false;
  }

  auto IndexIterator = GetElementPointer->idx_begin();
  Value *OuterIndex = IndexIterator->get();
  ++IndexIterator;
  if (!isa<ConstantInt>(OuterIndex)) {
    return false;
  }

  BasePointer = ArrayAlloca;
  ElementIndex = IndexIterator->get();
  ElementCount = static_cast<uint32_t>(ArrayStorageType->getNumElements());
  return ElementCount > 1;
}

bool DxilPIXRayQueryLog::IsShaderTerminationFunction(DxilModule &DM,
                                                     Function &TargetFunction) {
  if (DM.GetEntryFunction() != nullptr ||
      DM.GetPatchConstantFunction() != nullptr) {
    // The patch-constant function is a separate root from the hull entry.
    return PIXPassHelpers::GetEntryFunction(DM) == &TargetFunction ||
           DM.GetPatchConstantFunction() == &TargetFunction;
  }
  return DM.HasDxilFunctionProps(&TargetFunction);
}

std::string DxilPIXRayQueryLog::GetStageName(DxilModule &DM,
                                             Function *TargetFunction) {
  switch (PIXPassHelpers::GetFunctionShaderKind(DM, TargetFunction)) {
  case DXIL::ShaderKind::Compute:
    return "cs";
  case DXIL::ShaderKind::Vertex:
    return "vs";
  case DXIL::ShaderKind::Pixel:
    return "ps";
  case DXIL::ShaderKind::Geometry:
    return "gs";
  case DXIL::ShaderKind::Hull:
    return "hs";
  case DXIL::ShaderKind::Domain:
    return "ds";
  case DXIL::ShaderKind::Amplification:
    return "as";
  case DXIL::ShaderKind::Mesh:
    return "ms";
  case DXIL::ShaderKind::RayGeneration:
    return "raygeneration";
  case DXIL::ShaderKind::ClosestHit:
    return "closesthit";
  case DXIL::ShaderKind::Miss:
    return "miss";
  case DXIL::ShaderKind::Callable:
    return "callable";
  case DXIL::ShaderKind::AnyHit:
    return "anyhit";
  case DXIL::ShaderKind::Intersection:
    return "intersection";
  default:
    return "unknown";
  }
}

std::string DxilPIXRayQueryLog::GetEntryName(DxilModule &DM,
                                             Function *TargetFunction) {
  if (DM.HasDxilFunctionProps(TargetFunction) ||
      PIXPassHelpers::GetEntryFunction(DM) == TargetFunction) {
    return TargetFunction->getName().str();
  }
  return "unknown";
}

std::string
DxilPIXRayQueryLog::GetDebugLocation(Instruction *InstructionToDescribe) {
  if (DILocation *DebugLocation = InstructionToDescribe->getDebugLoc()) {
    std::string Result;
    raw_string_ostream Stream(Result);
    Stream << DebugLocation->getFilename() << ":" << DebugLocation->getLine()
           << ":" << DebugLocation->getColumn();
    return Stream.str();
  }
  return "";
}

void DxilPIXRayQueryLog::AddEmitAnnotations(DxilModule &DM,
                                            FunctionContext &Context,
                                            Instruction *InstructionToDescribe,
                                            Value *Handle, StringRef EmitKind,
                                            StringRef RecordKind,
                                            StringRef ReasonKind) {
  std::set<uint32_t> TraceSiteIds;
  if (Handle != nullptr) {
    Value *Key = GetHandleKey(Handle);
    auto TraceSiteIterator = Context.TraceSitesByKey.find(Key);
    if (TraceSiteIterator != Context.TraceSitesByKey.end()) {
      TraceSiteIds.insert(TraceSiteIterator->second.begin(),
                          TraceSiteIterator->second.end());
    }
  }

  if (TraceSiteIds.empty() && Handle == nullptr) {
    for (const TraceSiteInfo &Site : TraceSites) {
      if (Site.ParentFunction == Context.TargetFunction) {
        TraceSiteIds.insert(Site.TraceSiteId);
      }
    }
  }

  for (uint32_t TraceSiteId : TraceSiteIds) {
    EmitAnnotationInfo Annotation;
    Annotation.TraceSiteId = TraceSiteId;
    Annotation.EmitKind = EmitKind.str();
    Annotation.RecordKind = RecordKind.str();
    Annotation.ReasonKind = ReasonKind.str();
    Annotation.ParentFunction = Context.TargetFunction;
    Annotation.DebugLocation = GetDebugLocation(InstructionToDescribe);
    Annotation.StageName = GetStageName(DM, Context.TargetFunction);
    Annotation.EntryName = GetEntryName(DM, Context.TargetFunction);
    EmitAnnotations.push_back(Annotation);
  }
}

void DxilPIXRayQueryLog::EmitSideTable() {
  if (OSOverride == nullptr) {
    return;
  }

  formatted_raw_ostream FOS(*OSOverride);
  FOS << "PIX_RAYQUERY_LOG_V1\n";
  for (const TraceSiteInfo &Site : TraceSites) {
    FOS << "site id=" << Site.TraceSiteId << " local=" << Site.LocalOrdinal
        << " flags=0x1 lifecycle=PARTIAL asKind=unknown space=unknown"
        << " register=unknown rangeLower=unknown dynamicIndex=static entry=";
    FOS << (Site.EntryName.empty() ? "unknown" : Site.EntryName);
    FOS << " stage=" << (Site.StageName.empty() ? "unknown" : Site.StageName)
        << " function=";
    if (Site.ParentFunction != nullptr) {
      FOS << Site.ParentFunction->getName();
    } else {
      FOS << "unknown";
    }
    FOS << " debug=\"" << Site.DebugLocation << "\"\n";
  }
  for (const EmitAnnotationInfo &Annotation : EmitAnnotations) {
    FOS << "annotation site=" << Annotation.TraceSiteId
        << " emit=" << Annotation.EmitKind
        << " record=" << Annotation.RecordKind
        << " reason=" << Annotation.ReasonKind << " entry=";
    FOS << (Annotation.EntryName.empty() ? "unknown" : Annotation.EntryName);
    FOS << " stage="
        << (Annotation.StageName.empty() ? "unknown" : Annotation.StageName)
        << " function=";
    if (Annotation.ParentFunction != nullptr) {
      FOS << Annotation.ParentFunction->getName();
    } else {
      FOS << "unknown";
    }
    FOS << " debug=\"" << Annotation.DebugLocation << "\"\n";
  }
  FOS << "summary sites=" << TraceSites.size()
      << " partial=" << TraceSites.size() << "\n";
  FOS << "END_PIX_RAYQUERY_LOG_V1\n";
}

} // namespace

char DxilPIXRayQueryLog::ID = 0;

ModulePass *llvm::createDxilPIXRayQueryLogPass() {
  return new DxilPIXRayQueryLog();
}

INITIALIZE_PASS(DxilPIXRayQueryLog, "hlsl-dxil-pix-rayquery-log",
                "HLSL DXIL Logs RayQuery invocations into a UAV", false, false)
