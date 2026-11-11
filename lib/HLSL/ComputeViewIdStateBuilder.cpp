///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// ComputeViewIdStateBuilder.cpp                                             //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/DXIL/DxilInstructions.h"
#include "dxc/DXIL/DxilModule.h"
#include "dxc/DXIL/DxilOperations.h"
#include "dxc/HLSL/ComputeViewIdState.h"
#include "dxc/HLSL/HLOperations.h"
#include "dxc/HlslIntrinsicOp.h"
#include "dxc/Support/Global.h"

#include "llvm/Analysis/CallGraph.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Operator.h"
#include "llvm/Pass.h"
#include "llvm/Support/Debug.h"

#include <algorithm>
#include <deque>
#include <functional>
#include <map>
#include <set>

using namespace llvm;
using namespace llvm::legacy;
using namespace hlsl;
using llvm::legacy::FunctionPassManager;
using llvm::legacy::PassManager;
using std::unordered_map;
using std::unordered_set;
using std::vector;

#define DXILVIEWID_DBG 0

#define DEBUG_TYPE "viewid_builder"

namespace {
class DxilViewIdStateBuilder {
  static const unsigned kNumComps = 4;
  static const unsigned kMaxSigScalars = 32 * 4;

public:
  using OutputsDependentOnViewIdType =
      DxilViewIdStateData::OutputsDependentOnViewIdType;
  using InputsContributingToOutputType =
      DxilViewIdStateData::InputsContributingToOutputType;

  DxilViewIdStateBuilder(DxilViewIdStateData &state, DxilModule *pDxilModule)
      : m_pModule(pDxilModule),
        m_NumInputSigScalars(state.m_NumInputSigScalars),
        m_NumOutputSigScalars(state.m_NumOutputSigScalars,
                              DxilViewIdStateData::kNumStreams),
        m_NumPCOrPrimSigScalars(state.m_NumPCOrPrimSigScalars),
        m_OutputsDependentOnViewId(state.m_OutputsDependentOnViewId,
                                   DxilViewIdStateData::kNumStreams),
        m_PCOrPrimOutputsDependentOnViewId(
            state.m_PCOrPrimOutputsDependentOnViewId),
        m_InputsContributingToOutputs(state.m_InputsContributingToOutputs,
                                      DxilViewIdStateData::kNumStreams),
        m_InputsContributingToPCOrPrimOutputs(
            state.m_InputsContributingToPCOrPrimOutputs),
        m_PCInputsContributingToOutputs(state.m_PCInputsContributingToOutputs),
        m_bUsesViewId(state.m_bUsesViewId) {}

  void Compute();

private:
  static const unsigned kNumStreams = 4;

  DxilModule *m_pModule;

  unsigned &m_NumInputSigScalars;
  MutableArrayRef<unsigned> m_NumOutputSigScalars;
  unsigned &m_NumPCOrPrimSigScalars;

  // Set of scalar outputs dependent on ViewID.
  MutableArrayRef<OutputsDependentOnViewIdType> m_OutputsDependentOnViewId;
  OutputsDependentOnViewIdType &m_PCOrPrimOutputsDependentOnViewId;

  // Set of scalar inputs contributing to computation of scalar outputs.
  MutableArrayRef<InputsContributingToOutputType> m_InputsContributingToOutputs;
  InputsContributingToOutputType
      &m_InputsContributingToPCOrPrimOutputs; // HS PC and MS Prim only.
  InputsContributingToOutputType &m_PCInputsContributingToOutputs; // DS only.

  bool &m_bUsesViewId;

  // Members for build ViewIdState.

  // Dynamically indexed components of signature elements.
  using DynamicallyIndexedElemsType = std::unordered_map<unsigned, unsigned>;
  DynamicallyIndexedElemsType m_InpSigDynIdxElems;
  DynamicallyIndexedElemsType m_OutSigDynIdxElems;
  DynamicallyIndexedElemsType m_PCSigDynIdxElems;

  // Information per entry point.
  using FunctionSetType = std::unordered_set<llvm::Function *>;
  using InstructionSetType = std::unordered_set<llvm::Instruction *>;
  struct EntryInfo {
    llvm::Function *pEntryFunc = nullptr;
    // Sets of functions that may be reachable from an entry.
    FunctionSetType Functions;
    // Outputs to analyze.
    InstructionSetType Outputs;
    // Contributing instructions per output.
    std::unordered_map<unsigned, InstructionSetType>
        ContributingInstructions[kNumStreams];
    void Clear();
  };

  EntryInfo m_Entry;
  EntryInfo m_PCEntry;

  // Information per function.
  using FunctionReturnSet = std::unordered_set<llvm::ReturnInst *>;
  struct FuncInfo {
    FunctionReturnSet Returns;
    BasicBlockSet Reachable;
    ControlDependence CtrlDep;
    std::unique_ptr<llvm::DominatorTreeBase<llvm::BasicBlock>> pDomTree;
    void Clear();
  };

  std::unordered_map<llvm::Function *, std::unique_ptr<FuncInfo>> m_FuncInfo;

  struct Dependency {
    bool IsMemory;
    Value *Dep;
    Value *GuardA;
    Value *GuardB;

    Dependency(bool IsMemory, Value *Dep, Value *GuardA, Value *GuardB)
        : IsMemory(IsMemory), Dep(Dep), GuardA(GuardA), GuardB(GuardB) {}

    bool operator<(const Dependency &Other) const {
      if (IsMemory != Other.IsMemory)
        return IsMemory < Other.IsMemory;
      std::less<Value *> Less;
      if (Dep != Other.Dep)
        return Less(Dep, Other.Dep);
      if (GuardA != Other.GuardA)
        return Less(GuardA, Other.GuardA);
      return Less(GuardB, Other.GuardB);
    }
  };
  using Deps = std::set<Dependency>;
  struct Summary {
    Deps Return;
    std::map<Value *, Deps> Writes;
    std::map<Instruction *, Deps> Outputs;
  };
  std::map<Function *, std::unique_ptr<Summary>> m_Summaries;
  FunctionSetType m_ActiveFunctions;
  Summary &BuildSummary(Function *F);

  void Clear();
  void DetermineMaxPackedLocation(DxilSignature &DxilSig, unsigned *pMaxSigLoc,
                                  unsigned NumStreams);
  void ComputeReachableFunctionsRec(llvm::CallGraph &CG,
                                    llvm::CallGraphNode *pNode,
                                    FunctionSetType &FuncSet);
  void AnalyzeFunctions(EntryInfo &Entry);
  void CollectValuesContributingToOutputs(EntryInfo &Entry,
                                          bool IsForPatchConstant);
  void UpdateDynamicIndexUsageState() const;
  void
  CreateViewIdSets(const std::unordered_map<unsigned, InstructionSetType>
                       &ContributingInstructions,
                   OutputsDependentOnViewIdType &OutputsDependentOnViewId,
                   InputsContributingToOutputType &InputsContributingToOutputs,
                   bool bPC);

  void UpdateDynamicIndexUsageStateForSig(
      DxilSignature &Sig, const DynamicallyIndexedElemsType &DynIdxElems) const;
  unsigned GetLinearIndex(DxilSignatureElement &SigElem, int row,
                          unsigned col) const;
};
} // namespace

void DxilViewIdStateBuilder::Compute() {
  Clear();

  const ShaderModel *pSM = m_pModule->GetShaderModel();
  m_bUsesViewId = m_pModule->m_ShaderFlags.GetViewID();

  // 1. Traverse signature MD to determine max packed location.
  DetermineMaxPackedLocation(m_pModule->GetInputSignature(),
                             &m_NumInputSigScalars, 1);
  DetermineMaxPackedLocation(m_pModule->GetOutputSignature(),
                             &m_NumOutputSigScalars[0],
                             pSM->IsGS() ? kNumStreams : 1);
  DetermineMaxPackedLocation(m_pModule->GetPatchConstOrPrimSignature(),
                             &m_NumPCOrPrimSigScalars, 1);

  // 2. Collect sets of functions reachable from main and pc entries.
  CallGraphAnalysis CGA;
  CallGraph CG = CGA.run(m_pModule->GetModule());
  m_Entry.pEntryFunc = m_pModule->GetEntryFunction();
  m_PCEntry.pEntryFunc = m_pModule->GetPatchConstantFunction();
  // For MS, use main entry as PC entry to collect primitive outputs.
  if (pSM->IsMS())
    m_PCEntry.pEntryFunc = m_pModule->GetEntryFunction();
  ComputeReachableFunctionsRec(CG, CG[m_Entry.pEntryFunc], m_Entry.Functions);
  if (m_PCEntry.pEntryFunc) {
    DXASSERT_NOMSG(pSM->IsHS() || pSM->IsMS());
    ComputeReachableFunctionsRec(CG, CG[m_PCEntry.pEntryFunc],
                                 m_PCEntry.Functions);
  }

  // 3. Determine shape components that are dynamically accesses and collect all
  // sig outputs.
  AnalyzeFunctions(m_Entry);
  if (m_PCEntry.pEntryFunc) {
    AnalyzeFunctions(m_PCEntry);
  }

  // 4. Collect sets of values contributing to outputs.
  CollectValuesContributingToOutputs(m_Entry,
                                     /*IsForPatchConstantOrPrimitive*/ false);
  if (m_PCEntry.pEntryFunc)
    CollectValuesContributingToOutputs(m_PCEntry,
                                       /*IsForPatchConstantOrPrimitive*/ true);

  // 5. Construct dependency sets.
  for (unsigned StreamId = 0; StreamId < (pSM->IsGS() ? kNumStreams : 1u);
       StreamId++) {
    CreateViewIdSets(m_Entry.ContributingInstructions[StreamId],
                     m_OutputsDependentOnViewId[StreamId],
                     m_InputsContributingToOutputs[StreamId], false);
  }
  if (pSM->IsHS() || pSM->IsMS()) {
    CreateViewIdSets(m_PCEntry.ContributingInstructions[0],
                     m_PCOrPrimOutputsDependentOnViewId,
                     m_InputsContributingToPCOrPrimOutputs, true);
  } else if (pSM->IsDS()) {
    OutputsDependentOnViewIdType OutputsDependentOnViewId;
    CreateViewIdSets(m_Entry.ContributingInstructions[0],
                     OutputsDependentOnViewId, m_PCInputsContributingToOutputs,
                     true);
    DXASSERT_NOMSG(OutputsDependentOnViewId == m_OutputsDependentOnViewId[0]);
  }

  // 6. Update dynamically indexed input/output component masks.
  UpdateDynamicIndexUsageState();

#if DXILVIEWID_DBG
  PrintSets(dbgs());
#endif
}

void DxilViewIdStateBuilder::Clear() {
  m_bUsesViewId = false;
  m_NumInputSigScalars = 0;
  for (unsigned i = 0; i < kNumStreams; i++) {
    m_NumOutputSigScalars[i] = 0;
    m_OutputsDependentOnViewId[i].reset();
    m_InputsContributingToOutputs[i].clear();
  }
  m_NumPCOrPrimSigScalars = 0;
  m_InpSigDynIdxElems.clear();
  m_OutSigDynIdxElems.clear();
  m_PCSigDynIdxElems.clear();
  m_PCOrPrimOutputsDependentOnViewId.reset();
  m_InputsContributingToPCOrPrimOutputs.clear();
  m_PCInputsContributingToOutputs.clear();
  m_Entry.Clear();
  m_PCEntry.Clear();
  m_FuncInfo.clear();
  m_Summaries.clear();
  m_ActiveFunctions.clear();
}

void DxilViewIdStateBuilder::EntryInfo::Clear() {
  pEntryFunc = nullptr;
  Functions.clear();
  Outputs.clear();
  for (unsigned i = 0; i < kNumStreams; i++)
    ContributingInstructions[i].clear();
}

void DxilViewIdStateBuilder::FuncInfo::Clear() {
  Returns.clear();
  CtrlDep.Clear();
  pDomTree.reset();
}

void DxilViewIdStateBuilder::DetermineMaxPackedLocation(DxilSignature &DxilSig,
                                                        unsigned *pMaxSigLoc,
                                                        unsigned NumStreams) {
  DXASSERT_NOMSG(NumStreams == 1 || NumStreams == kNumStreams);

  for (unsigned i = 0; i < NumStreams; i++) {
    pMaxSigLoc[i] = 0;
  }

  for (auto &E : DxilSig.GetElements()) {
    if (E->GetStartRow() == Semantic::kUndefinedRow)
      continue;

    unsigned StreamId = E->GetOutputStream();
    unsigned endLoc = GetLinearIndex(*E, E->GetRows() - 1, E->GetCols() - 1);
    pMaxSigLoc[StreamId] = std::max(pMaxSigLoc[StreamId], endLoc + 1);
    E->GetCols();
  }
}

void DxilViewIdStateBuilder::ComputeReachableFunctionsRec(
    CallGraph &CG, CallGraphNode *pNode, FunctionSetType &FuncSet) {
  Function *F = pNode->getFunction();
  IFTBOOL(F && !m_ActiveFunctions.count(F), DXC_E_GENERAL_INTERNAL_ERROR);
  // Accumulate only functions with bodies.
  if (F->empty())
    return;
  if (FuncSet.count(F))
    return;
  m_ActiveFunctions.insert(F);
  auto itIns = FuncSet.emplace(F);
  DXASSERT_NOMSG(itIns.second);
  (void)itIns;
  if (!m_FuncInfo.count(F))
    m_FuncInfo[F] = make_unique<FuncInfo>();
  auto &Reachable = m_FuncInfo[F]->Reachable;
  std::vector<BasicBlock *> Work = {&F->getEntryBlock()};
  while (!Work.empty()) {
    BasicBlock *B = Work.back();
    Work.pop_back();
    if (!Reachable.insert(B).second)
      continue;
    for (auto *S : successors(B))
      Work.push_back(S);
  }
  for (auto &BB : *F)
    if (Reachable.count(&BB))
      for (auto &I : BB)
        if (auto *CI = dyn_cast<CallInst>(&I)) {
          Function *Callee = CI->getCalledFunction();
          IFTBOOL(Callee, DXC_E_GENERAL_INTERNAL_ERROR);
          ComputeReachableFunctionsRec(CG, CG[Callee], FuncSet);
        }
  m_ActiveFunctions.erase(F);
}

static bool GetUnsignedVal(Value *V, uint32_t *pValue) {
  ConstantInt *CI = dyn_cast<ConstantInt>(V);
  if (!CI)
    return false;
  uint64_t u = CI->getZExtValue();
  if (u > UINT32_MAX)
    return false;
  *pValue = (uint32_t)u;
  return true;
}

void DxilViewIdStateBuilder::AnalyzeFunctions(EntryInfo &Entry) {
  for (auto *F : Entry.Functions) {
    DXASSERT_NOMSG(!F->empty());

    auto itFI = m_FuncInfo.find(F);
    FuncInfo *pFuncInfo = nullptr;
    if (itFI != m_FuncInfo.end()) {
      pFuncInfo = itFI->second.get();
    } else {
      m_FuncInfo[F] = make_unique<FuncInfo>();
      pFuncInfo = m_FuncInfo[F].get();
    }

    for (auto itBB = F->begin(), endBB = F->end(); itBB != endBB; ++itBB) {
      BasicBlock *BB = itBB;
      if (!pFuncInfo->Reachable.count(BB))
        continue;

      for (auto itInst = BB->begin(), endInst = BB->end(); itInst != endInst;
           ++itInst) {
        if (ReturnInst *RI = dyn_cast<ReturnInst>(itInst)) {
          pFuncInfo->Returns.emplace(RI);
          continue;
        }

        CallInst *CI = dyn_cast<CallInst>(itInst);
        if (!CI)
          continue;

        DynamicallyIndexedElemsType *pDynIdxElems = nullptr;
        int row = Semantic::kUndefinedRow;
        unsigned id, col;
        if (DxilInst_LoadInput LI = DxilInst_LoadInput(CI)) {
          pDynIdxElems = &m_InpSigDynIdxElems;
          IFTBOOL(GetUnsignedVal(LI.get_inputSigId(), &id),
                  DXC_E_GENERAL_INTERNAL_ERROR);
          GetUnsignedVal(LI.get_rowIndex(), (uint32_t *)&row);
          IFTBOOL(GetUnsignedVal(LI.get_colIndex(), &col),
                  DXC_E_GENERAL_INTERNAL_ERROR);
        } else if (DxilInst_StoreOutput SO = DxilInst_StoreOutput(CI)) {
          pDynIdxElems = &m_OutSigDynIdxElems;
          IFTBOOL(GetUnsignedVal(SO.get_outputSigId(), &id),
                  DXC_E_GENERAL_INTERNAL_ERROR);
          GetUnsignedVal(SO.get_rowIndex(), (uint32_t *)&row);
          IFTBOOL(GetUnsignedVal(SO.get_colIndex(), &col),
                  DXC_E_GENERAL_INTERNAL_ERROR);
          Entry.Outputs.emplace(CI);
        } else if (DxilInst_StoreVertexOutput SVO =
                       DxilInst_StoreVertexOutput(CI)) {
          pDynIdxElems = &m_OutSigDynIdxElems;
          IFTBOOL(GetUnsignedVal(SVO.get_outputSigId(), &id),
                  DXC_E_GENERAL_INTERNAL_ERROR);
          GetUnsignedVal(SVO.get_rowIndex(), (uint32_t *)&row);
          IFTBOOL(GetUnsignedVal(SVO.get_colIndex(), &col),
                  DXC_E_GENERAL_INTERNAL_ERROR);
          Entry.Outputs.emplace(CI);
        } else if (DxilInst_StorePrimitiveOutput SPO =
                       DxilInst_StorePrimitiveOutput(CI)) {
          pDynIdxElems = &m_PCSigDynIdxElems;
          IFTBOOL(GetUnsignedVal(SPO.get_outputSigId(), &id),
                  DXC_E_GENERAL_INTERNAL_ERROR);
          GetUnsignedVal(SPO.get_rowIndex(), (uint32_t *)&row);
          IFTBOOL(GetUnsignedVal(SPO.get_colIndex(), &col),
                  DXC_E_GENERAL_INTERNAL_ERROR);
          Entry.Outputs.emplace(CI);
        } else if (DxilInst_LoadPatchConstant LPC =
                       DxilInst_LoadPatchConstant(CI)) {
          if (m_pModule->GetShaderModel()->IsDS()) {
            pDynIdxElems = &m_PCSigDynIdxElems;
            IFTBOOL(GetUnsignedVal(LPC.get_inputSigId(), &id),
                    DXC_E_GENERAL_INTERNAL_ERROR);
            GetUnsignedVal(LPC.get_row(), (uint32_t *)&row);
            IFTBOOL(GetUnsignedVal(LPC.get_col(), &col),
                    DXC_E_GENERAL_INTERNAL_ERROR);
          } else {
            // Do nothing. This is an internal helper function for DXBC-2-DXIL
            // converter.
            DXASSERT_NOMSG(m_pModule->GetShaderModel()->IsHS());
          }
        } else if (DxilInst_StorePatchConstant SPC =
                       DxilInst_StorePatchConstant(CI)) {
          pDynIdxElems = &m_PCSigDynIdxElems;
          IFTBOOL(GetUnsignedVal(SPC.get_outputSigID(), &id),
                  DXC_E_GENERAL_INTERNAL_ERROR);
          GetUnsignedVal(SPC.get_row(), (uint32_t *)&row);
          IFTBOOL(GetUnsignedVal(SPC.get_col(), &col),
                  DXC_E_GENERAL_INTERNAL_ERROR);
          Entry.Outputs.emplace(CI);
        } else if (DxilInst_LoadOutputControlPoint LOCP =
                       DxilInst_LoadOutputControlPoint(CI)) {
          if (m_pModule->GetShaderModel()->IsDS()) {
            pDynIdxElems = &m_InpSigDynIdxElems;
            IFTBOOL(GetUnsignedVal(LOCP.get_inputSigId(), &id),
                    DXC_E_GENERAL_INTERNAL_ERROR);
            GetUnsignedVal(LOCP.get_row(), (uint32_t *)&row);
            IFTBOOL(GetUnsignedVal(LOCP.get_col(), &col),
                    DXC_E_GENERAL_INTERNAL_ERROR);
          } else if (m_pModule->GetShaderModel()->IsHS()) {
            // Do nothings, as the information has been captured by the output
            // signature of CP entry.
          } else {
            DXASSERT_NOMSG(false);
          }
        } else {
          continue;
        }

        // Record dynamic index usage.
        if (pDynIdxElems && row == Semantic::kUndefinedRow) {
          (*pDynIdxElems)[id] |= (1 << col);
        }
      }
    }

    // Compute dominator relation.
    pFuncInfo->pDomTree = make_unique<DominatorTreeBase<BasicBlock>>(false);
    pFuncInfo->pDomTree->recalculate(*F);
#if DXILVIEWID_DBG
    pFuncInfo->pDomTree->print(dbgs());
#endif

    // Compute postdominator relation.
    DominatorTreeBase<BasicBlock> PDR(true);
    PDR.recalculate(*F);
#if DXILVIEWID_DBG
    PDR.print(dbgs());
#endif
    // Compute control dependence.
    pFuncInfo->CtrlDep.Compute(F, PDR);
#if DXILVIEWID_DBG
    pFuncInfo->CtrlDep.print(dbgs());
#endif
  }
}

void DxilViewIdStateBuilder::CollectValuesContributingToOutputs(
    EntryInfo &Entry, bool IsForPatchConstantOrPrimitive) {
  auto &Summary = BuildSummary(Entry.pEntryFunc);
  for (auto *CI : Entry.Outputs) { // CI = call instruction
    DxilSignature *pDxilSig = nullptr;
    unsigned id = (unsigned)-1;
    int startRow = Semantic::kUndefinedRow, endRow = Semantic::kUndefinedRow;
    unsigned col = (unsigned)-1;
    if (DxilInst_StoreOutput SO = DxilInst_StoreOutput(CI)) {
      if (IsForPatchConstantOrPrimitive)
        continue;
      pDxilSig = &m_pModule->GetOutputSignature();
      GetUnsignedVal(SO.get_outputSigId(), &id);
      GetUnsignedVal(SO.get_colIndex(), &col);
      GetUnsignedVal(SO.get_rowIndex(), (uint32_t *)&startRow);
    } else if (DxilInst_StoreVertexOutput SVO =
                   DxilInst_StoreVertexOutput(CI)) {
      if (IsForPatchConstantOrPrimitive)
        continue;
      pDxilSig = &m_pModule->GetOutputSignature();
      GetUnsignedVal(SVO.get_outputSigId(), &id);
      GetUnsignedVal(SVO.get_colIndex(), &col);
      GetUnsignedVal(SVO.get_rowIndex(), (uint32_t *)&startRow);
    } else if (DxilInst_StorePrimitiveOutput SPO =
                   DxilInst_StorePrimitiveOutput(CI)) {
      if (!IsForPatchConstantOrPrimitive)
        continue;
      pDxilSig = &m_pModule->GetPatchConstOrPrimSignature();
      GetUnsignedVal(SPO.get_outputSigId(), &id);
      GetUnsignedVal(SPO.get_colIndex(), &col);
      GetUnsignedVal(SPO.get_rowIndex(), (uint32_t *)&startRow);
    } else if (DxilInst_StorePatchConstant SPC =
                   DxilInst_StorePatchConstant(CI)) {
      if (!IsForPatchConstantOrPrimitive)
        continue;
      pDxilSig = &m_pModule->GetPatchConstOrPrimSignature();
      GetUnsignedVal(SPC.get_outputSigID(), &id);
      GetUnsignedVal(SPC.get_row(), (uint32_t *)&startRow);
      GetUnsignedVal(SPC.get_col(), &col);
    } else {
      IFT(DXC_E_GENERAL_INTERNAL_ERROR);
    }

    DxilSignatureElement &SigElem = pDxilSig->GetElement(id);
    if (!SigElem.IsAllocated())
      continue;

    unsigned StreamId = SigElem.GetOutputStream();

    if (startRow != Semantic::kUndefinedRow) {
      endRow = startRow;
    } else {
      // The entire column is affected by value.
      DXASSERT_NOMSG(SigElem.GetID() == id &&
                     SigElem.GetStartRow() != Semantic::kUndefinedRow);
      startRow = 0;
      endRow = SigElem.GetRows() - 1;
    }

    InstructionSetType ContributingInstructionsAllRows;
    InstructionSetType *pContributingInstructions =
        &ContributingInstructionsAllRows;
    if (startRow == endRow) {
      // Scalar or indexable with known index.
      unsigned index = GetLinearIndex(SigElem, startRow, col);
      pContributingInstructions =
          &Entry.ContributingInstructions[StreamId][index];
    }

    IFTBOOL(Summary.Outputs.count(CI), DXC_E_GENERAL_INTERNAL_ERROR);
    for (auto T : Summary.Outputs.find(CI)->second)
      if (!T.IsMemory)
        if (auto *I = dyn_cast<Instruction>(T.Dep))
          pContributingInstructions->insert(I);

    if (pContributingInstructions == &ContributingInstructionsAllRows) {
      // Write dynamically indexed output contributions to all rows.
      for (int row = startRow; row <= endRow; row++) {
        unsigned index = GetLinearIndex(SigElem, row, col);
        Entry.ContributingInstructions[StreamId][index].insert(
            ContributingInstructionsAllRows.begin(),
            ContributingInstructionsAllRows.end());
      }
    }
  }
}

// Dependencies are symbolic scalar/formal values or incoming memory contents.
// A selection contribution may be guarded by inequality of two identities.
// Only formal/constant identities cross a summary boundary. Losing a guard is
// conservative; losing a dependency is not. No call-path objects are retained.
DxilViewIdStateBuilder::Summary &
DxilViewIdStateBuilder::BuildSummary(Function *F) {
  auto Cached = m_Summaries.find(F);
  if (Cached != m_Summaries.end())
    return *Cached->second;
  auto &FI = *m_FuncInfo[F];
  for (auto &BB : *F)
    if (FI.Reachable.count(&BB))
      for (auto &I : BB)
        if (auto *CI = dyn_cast<CallInst>(&I)) {
          auto *Callee = CI->getCalledFunction();
          IFTBOOL(Callee, DXC_E_GENERAL_INTERNAL_ERROR);
          if (!Callee->empty())
            BuildSummary(Callee);
          else if (!isa<MemIntrinsic>(CI)) {
            auto *II = dyn_cast<IntrinsicInst>(CI);
            bool Marker =
                II && (isa<DbgInfoIntrinsic>(II) ||
                       II->getIntrinsicID() == Intrinsic::lifetime_start ||
                       II->getIntrinsicID() == Intrinsic::lifetime_end);
            IFTBOOLMSG(Marker || OP::IsDxilOpFuncCallInst(CI) ||
                           CI->doesNotAccessMemory(),
                       DXC_E_GENERAL_INTERNAL_ERROR,
                       "ViewID analysis cannot summarize an external call "
                       "that accesses memory.");
          }
        }

  auto Result = make_unique<Summary>();
  std::map<Value *, Deps> Values, Memory;
  std::map<Value *, std::set<Value *>> Bases;
  std::vector<Instruction *> Instructions;
  for (auto &A : F->args()) {
    Values[&A].insert(Dependency(false, &A, nullptr, nullptr));
    if (A.getType()->isPointerTy())
      Bases[&A].insert(&A);
  }
  for (auto &G : F->getParent()->globals())
    Bases[&G].insert(&G);
  for (auto &BB : *F)
    if (FI.Reachable.count(&BB))
      for (auto &I : BB) {
        Instructions.push_back(&I);
        if (isa<AllocaInst>(I) ||
            OP::IsDxilOpFuncCallInst(&I, DXIL::OpCode::GetMeshPayload))
          Bases[&I].insert(&I);
      }
  // Register readers even when the queried set is still empty. A later join
  // then wakes precisely the equations that have already observed that set.
  // Remove an instruction from Pending before evaluating it so alias feedback
  // and PHI cycles can enqueue the current instruction again.
  std::deque<Instruction *> Worklist;
  std::set<Instruction *> Pending;
  auto Enqueue = [&](Instruction *I) {
    if (Pending.insert(I).second)
      Worklist.push_back(I);
  };
  auto Wake = [&](const std::set<Instruction *> &Readers) {
    for (auto *I : Readers)
      Enqueue(I);
  };

  // Close pointer identities first, including PHI/cast cycles. Only pointer
  // equations subscribe here; memory readers see completed base sets below.
  std::map<Value *, std::set<Instruction *>> BaseReaders;
  Instruction *BaseReader = nullptr;
  std::function<std::set<Value *>(Value *)> GetBases = [&](Value *V) {
    if (auto *CE = dyn_cast<ConstantExpr>(V)) {
      IFTBOOL(CE->getOpcode() == Instruction::GetElementPtr || CE->isCast(),
              DXC_E_GENERAL_INTERNAL_ERROR);
      return GetBases(CE->getOperand(0));
    }
    if (BaseReader)
      BaseReaders[V].insert(BaseReader);
    return Bases[V];
  };
  for (auto *I : Instructions)
    if (I->getType()->isPointerTy() && !isa<AllocaInst>(I))
      Enqueue(I);
  while (!Worklist.empty()) {
    auto *I = Worklist.front();
    Worklist.pop_front();
    Pending.erase(I);
    BaseReader = I;
    std::vector<Value *> Inputs;
    if (auto *P = dyn_cast<PHINode>(I)) {
      for (unsigned N = 0; N < P->getNumIncomingValues(); ++N)
        if (FI.Reachable.count(P->getIncomingBlock(N)))
          Inputs.push_back(P->getIncomingValue(N));
    } else if (auto *S = dyn_cast<SelectInst>(I)) {
      Inputs = {S->getTrueValue(), S->getFalseValue()};
    } else if (isa<GetElementPtrInst>(I) || isa<BitCastInst>(I) ||
               isa<AddrSpaceCastInst>(I))
      Inputs.push_back(I->getOperand(0));
    else
      IFTBOOL(OP::GetDxilOpFuncCallInst(I) == DXIL::OpCode::GetMeshPayload,
              DXC_E_GENERAL_INTERNAL_ERROR);
    auto Before = Bases[I].size();
    for (auto *V : Inputs) {
      auto B = GetBases(V);
      Bases[I].insert(B.begin(), B.end());
    }
    if (Before != Bases[I].size())
      Wake(BaseReaders[I]);
  }
  BaseReader = nullptr;
  BaseReaders.clear();

  std::map<Value *, std::set<Instruction *>> ValueReaders, MemoryReaders;
  Instruction *Reader = nullptr;
  auto Join = [](Deps &D, const Deps &S) {
    size_t Before = D.size();
    D.insert(S.begin(), S.end());
    return D.size() != Before;
  };
  std::function<Deps(Value *)> GetValue = [&](Value *V) {
    if (auto *CE = dyn_cast<ConstantExpr>(V)) {
      Deps D;
      for (auto &O : CE->operands()) {
        auto S = GetValue(O);
        D.insert(S.begin(), S.end());
      }
      return D;
    }
    ValueReaders[V].insert(Reader);
    return Values[V];
  };
  auto Read = [&](Value *P) {
    Deps D;
    auto B = GetBases(P);
    IFTBOOL(!B.empty(), DXC_E_GENERAL_INTERNAL_ERROR);
    for (auto *V : B) {
      MemoryReaders[V].insert(Reader);
      D.insert(Memory[V].begin(), Memory[V].end());
      if (isa<Argument>(V) || isa<GlobalVariable>(V))
        D.insert(Dependency(true, V, nullptr, nullptr));
    }
    return D;
  };
  auto Guard = [&](const Deps &D, Value *A, Value *B) {
    Deps R;
    if (A == B)
      return R;
    bool Portable = (isa<Argument>(A) || isa<Constant>(A)) &&
                    (isa<Argument>(B) || isa<Constant>(B));
    for (auto T : D) {
      // The new condition is conjoined with any existing condition. Keeping
      // either conjunct is conservative: if its identities become equal, this
      // contribution is absent regardless of the other condition. Keep the
      // existing guard to preserve identical-actual precision through nesting.
      if (Portable && !T.GuardA) {
        T.GuardA = A;
        T.GuardB = B;
      }
      R.insert(T);
    }
    return R;
  };
  auto Instantiate = [&](const Deps &D, CallInst *CI) {
    Deps R;
    auto Actual = [&](Value *V) -> Value * {
      if (auto *A = dyn_cast_or_null<Argument>(V))
        return CI->getArgOperand(A->getArgNo());
      return V;
    };
    for (auto T : D) {
      Value *V = T.Dep;
      Value *A = Actual(T.GuardA), *B = Actual(T.GuardB);
      if (A && A == B)
        continue;
      Deps S;
      if (T.IsMemory)
        S = Read(Actual(V));
      else if (isa<Argument>(V))
        S = GetValue(Actual(V));
      else
        S.insert(Dependency(false, V, nullptr, nullptr));
      if (A)
        S = Guard(S, A, B);
      R.insert(S.begin(), S.end());
    }
    return R;
  };
  auto Control = [&](BasicBlock *BB) {
    Deps D;
    for (auto *B : FI.CtrlDep.GetCDBlocks(BB))
      if (FI.Reachable.count(B)) {
        auto S = GetValue(B->getTerminator());
        D.insert(S.begin(), S.end());
      }
    return D;
  };
  auto Write = [&](Value *P, const Deps &D) {
    auto B = GetBases(P);
    IFTBOOL(!B.empty(), DXC_E_GENERAL_INTERNAL_ERROR);
    for (auto *V : B) {
      if (Join(Memory[V], D))
        Wake(MemoryReaders[V]);
      if (isa<Argument>(V) || isa<GlobalVariable>(V))
        Join(Result->Writes[V], D);
    }
  };
  // Hoist each incoming identity only through dominators that do not merge
  // different identities. This retains the existing constant-PHI precision
  // while also accounting for formal and pre-branch scalar definitions.
  auto SelectionControl = [&](PHINode *P) {
    using Node = DomTreeNodeBase<BasicBlock>;
    std::map<Node *, Value *> Markers;
    for (unsigned N = 0; N < P->getNumIncomingValues(); ++N) {
      auto *B = P->getIncomingBlock(N);
      if (FI.Reachable.count(B))
        Markers[FI.pDomTree->getNode(B)] = P->getIncomingValue(N);
    }
    for (unsigned N = 0; N < P->getNumIncomingValues(); ++N) {
      auto *B = P->getIncomingBlock(N);
      if (!FI.Reachable.count(B))
        continue;
      auto *V = P->getIncomingValue(N);
      BasicBlock *Def = &F->getEntryBlock();
      if (auto *I = dyn_cast<Instruction>(V))
        Def = I->getParent();
      if (B == Def)
        continue;
      for (auto *D = FI.pDomTree->getNode(B)->getIDom(); D; D = D->getIDom()) {
        auto Ins = Markers.emplace(D, V);
        if (!Ins.second) {
          if (Ins.first->second != V && Ins.first->second &&
              (!isa<Constant>(Ins.first->second) || !isa<Constant>(V)))
            Ins.first->second = nullptr;
          break;
        }
        if (D->getBlock() == Def)
          break;
      }
    }
    Deps R;
    for (unsigned N = 0; N < P->getNumIncomingValues(); ++N) {
      auto *B = P->getIncomingBlock(N);
      if (!FI.Reachable.count(B))
        continue;
      auto *V = P->getIncomingValue(N);
      auto *Def = FI.pDomTree->getNode(B);
      for (auto *D = Def; D; D = D->getIDom()) {
        auto It = Markers.find(D);
        if (It == Markers.end() || It->second != V)
          break;
        Def = D;
      }
      Join(R, Control(Def->getBlock()));
    }
    return R;
  };
  // All equations are monotone over finite identity sets. GetValue also
  // subscribes control-predicate and callee-actual consumers, while Read
  // discovers memory consumers during summary instantiation. A growing write
  // can therefore wake its own call, closing alias feedback without waking
  // unrelated instructions. Newly discovered readers see the current state.
  // Every instruction runs initially, preserving empty Writes/Outputs effects.
  // These terminal summary sets have no readers until the summary is complete;
  // unlike Values/Memory, their growth needs no scheduling inside this function.
  auto JoinOutput = [&](Instruction *I, const Deps &D) {
    Join(Result->Outputs[I], D);
  };
  auto JoinValue = [&](Instruction *I, const Deps &D) {
    if (Join(Values[I], D))
      Wake(ValueReaders[I]);
  };
  for (auto *I : Instructions)
    Enqueue(I);
  while (!Worklist.empty()) {
    auto *I = Worklist.front();
    Worklist.pop_front();
    Pending.erase(I);
    Reader = I;
    Deps D = Control(I->getParent());
    if (auto *CI = dyn_cast<CallInst>(I)) {
      if (!CI->getCalledFunction()->empty()) {
        const auto &S = *m_Summaries[CI->getCalledFunction()];
        Join(D, Instantiate(S.Return, CI));
        for (auto &W : S.Writes) {
          Value *P = W.first;
          if (auto *A = dyn_cast<Argument>(P))
            P = CI->getArgOperand(A->getArgNo());
          Deps Effect = Control(I->getParent());
          Join(Effect, Instantiate(W.second, CI));
          Write(P, Effect);
        }
        for (auto &O : S.Outputs) {
          Deps Effect = Control(I->getParent());
          Join(Effect, Instantiate(O.second, CI));
          JoinOutput(O.first, Effect);
        }
        JoinValue(I, D);
        continue;
      }
      if (DxilInst_ViewID(CI) || DxilInst_LoadInput(CI) ||
          DxilInst_LoadPatchConstant(CI) ||
          DxilInst_LoadOutputControlPoint(CI))
        D.insert(Dependency(false, CI, nullptr, nullptr));
    }
    if (auto *P = dyn_cast<PHINode>(I)) {
      std::vector<Value *> Incoming;
      Deps Selection = SelectionControl(P);
      for (unsigned N = 0; N < P->getNumIncomingValues(); ++N) {
        if (!FI.Reachable.count(P->getIncomingBlock(N)))
          continue;
        auto *V = P->getIncomingValue(N);
        Incoming.push_back(V);
        Join(D, GetValue(V));
      }
      for (size_t A = 1; A < Incoming.size(); ++A)
        Join(D, Guard(Selection, Incoming[0], Incoming[A]));
    } else if (auto *S = dyn_cast<SelectInst>(I)) {
      Join(D, GetValue(S->getTrueValue()));
      Join(D, GetValue(S->getFalseValue()));
      Join(D, Guard(GetValue(S->getCondition()), S->getTrueValue(),
                    S->getFalseValue()));
    } else {
      for (auto &O : I->operands())
        Join(D, GetValue(O));
    }
    Value *ReadPtr = nullptr, *WritePtr = nullptr;
    if (auto *L = dyn_cast<LoadInst>(I))
      ReadPtr = L->getPointerOperand();
    if (auto *S = dyn_cast<StoreInst>(I))
      WritePtr = S->getPointerOperand();
    if (auto *A = dyn_cast<AtomicRMWInst>(I))
      ReadPtr = WritePtr = A->getPointerOperand();
    if (auto *A = dyn_cast<AtomicCmpXchgInst>(I))
      ReadPtr = WritePtr = A->getPointerOperand();
    if (auto *M = dyn_cast<MemIntrinsic>(I))
      WritePtr = M->getRawDest();
    if (auto *M = dyn_cast<MemTransferInst>(I))
      ReadPtr = M->getRawSource();
    if (OP::IsDxilOpFuncCallInst(I)) {
      auto *CI = cast<CallInst>(I);
      // These recognized DXIL operations access ordinary pointer memory.
      // DispatchMesh consumes its payload in another stage; it does not
      // overwrite caller memory. Ray/node pointer effects are library-only,
      // outside this pass. Do not infer unknown ordinary call effects from
      // the mere presence of pointer operands.
      switch (OP::GetDxilOpFuncCallInst(CI)) {
      case DXIL::OpCode::MinPrecXRegLoad:
      case DXIL::OpCode::LinAlgMatrixLoadFromMemory:
        ReadPtr = CI->getArgOperand(1);
        break;
      case DXIL::OpCode::MinPrecXRegStore:
        WritePtr = CI->getArgOperand(1);
        break;
      case DXIL::OpCode::LinAlgMatrixStoreToMemory:
        WritePtr = CI->getArgOperand(2);
        break;
      case DXIL::OpCode::LinAlgMatrixAccumulateToMemory:
        ReadPtr = WritePtr = CI->getArgOperand(2);
        break;
      default:
        break;
      }
    }
    IFTBOOL(!I->mayWriteToMemory() || WritePtr || isa<CallInst>(I),
            DXC_E_GENERAL_INTERNAL_ERROR);
    if (ReadPtr)
      Join(D, Read(ReadPtr));
    if (WritePtr)
      Write(WritePtr, D);
    JoinValue(I, D);
    if (isa<ReturnInst>(I))
      Join(Result->Return, D);
    if (DxilInst_StoreOutput(I) || DxilInst_StorePatchConstant(I) ||
        DxilInst_StoreVertexOutput(I) || DxilInst_StorePrimitiveOutput(I))
      JoinOutput(I, D);
  }
  // Publish only a completed summary. The call graph was checked for cycles.
  auto *Complete = Result.get();
  m_Summaries[F] = std::move(Result);
  return *Complete;
}

void DxilViewIdStateBuilder::CreateViewIdSets(
    const std::unordered_map<unsigned, InstructionSetType>
        &ContributingInstructions,
    OutputsDependentOnViewIdType &OutputsDependentOnViewId,
    InputsContributingToOutputType &InputsContributingToOutputs, bool bPC) {
  const ShaderModel *pSM = m_pModule->GetShaderModel();

  for (auto &itOut : ContributingInstructions) {
    unsigned outIdx = itOut.first;
    for (Instruction *pInst : itOut.second) {
      // Set output dependence on ViewId.
      if (DxilInst_ViewID VID = DxilInst_ViewID(pInst)) {
        DXASSERT(m_bUsesViewId, "otherwise, DxilModule flag not set properly");
        OutputsDependentOnViewId[outIdx] = true;
        continue;
      }

      // Start setting output dependence on inputs.
      DxilSignatureElement *pSigElem = nullptr;
      bool bLoadOutputCPInHS = false;
      unsigned inpId = (unsigned)-1;
      int startRow = Semantic::kUndefinedRow, endRow = Semantic::kUndefinedRow;
      unsigned col = (unsigned)-1;
      if (DxilInst_LoadInput LI = DxilInst_LoadInput(pInst)) {
        GetUnsignedVal(LI.get_inputSigId(), &inpId);
        GetUnsignedVal(LI.get_colIndex(), &col);
        GetUnsignedVal(LI.get_rowIndex(), (uint32_t *)&startRow);
        pSigElem = &m_pModule->GetInputSignature().GetElement(inpId);
        if (pSM->IsDS() && bPC) {
          pSigElem = nullptr;
        }
      } else if (DxilInst_LoadOutputControlPoint LOCP =
                     DxilInst_LoadOutputControlPoint(pInst)) {
        GetUnsignedVal(LOCP.get_inputSigId(), &inpId);
        GetUnsignedVal(LOCP.get_col(), &col);
        GetUnsignedVal(LOCP.get_row(), (uint32_t *)&startRow);
        if (pSM->IsHS()) {
          pSigElem = &m_pModule->GetOutputSignature().GetElement(inpId);
          bLoadOutputCPInHS = true;
        } else if (pSM->IsDS()) {
          if (!bPC) {
            pSigElem = &m_pModule->GetInputSignature().GetElement(inpId);
          }
        } else {
          DXASSERT_NOMSG(false);
        }
      } else if (DxilInst_LoadPatchConstant LPC =
                     DxilInst_LoadPatchConstant(pInst)) {
        if (pSM->IsDS() && bPC) {
          GetUnsignedVal(LPC.get_inputSigId(), &inpId);
          GetUnsignedVal(LPC.get_col(), &col);
          GetUnsignedVal(LPC.get_row(), (uint32_t *)&startRow);
          pSigElem =
              &m_pModule->GetPatchConstOrPrimSignature().GetElement(inpId);
        }
      } else {
        continue;
      }

      // Finalize setting output dependence on inputs.
      if (pSigElem && pSigElem->IsAllocated()) {
        if (startRow != Semantic::kUndefinedRow) {
          endRow = startRow;
        } else {
          // The entire column contributes to output.
          startRow = 0;
          endRow = pSigElem->GetRows() - 1;
        }

        auto &ContributingInputs = InputsContributingToOutputs[outIdx];
        for (int row = startRow; row <= endRow; row++) {
          unsigned index = GetLinearIndex(*pSigElem, row, col);
          if (!bLoadOutputCPInHS) {
            ContributingInputs.emplace(index);
          } else {
            // This HS patch-constant output depends on an input value of
            // LoadOutputControlPoint that is the output value of the HS main
            // (control-point) function. Transitively update this
            // (patch-constant) output dependence on main (control-point)
            // output.
            DXASSERT_NOMSG(&OutputsDependentOnViewId ==
                           &m_PCOrPrimOutputsDependentOnViewId);
            OutputsDependentOnViewId[outIdx] =
                OutputsDependentOnViewId[outIdx] ||
                m_OutputsDependentOnViewId[0][index];

            const auto it = m_InputsContributingToOutputs[0].find(index);
            if (it != m_InputsContributingToOutputs[0].end()) {
              const std::set<unsigned>
                  &LoadOutputCPInputsContributingToOutputs = it->second;
              ContributingInputs.insert(
                  LoadOutputCPInputsContributingToOutputs.begin(),
                  LoadOutputCPInputsContributingToOutputs.end());
            }
          }
        }
      }
    }
  }
}

unsigned DxilViewIdStateBuilder::GetLinearIndex(DxilSignatureElement &SigElem,
                                                int row, unsigned col) const {
  DXASSERT_NOMSG(row >= 0 && col < kNumComps &&
                 SigElem.GetStartRow() != Semantic::kUndefinedRow);
  unsigned idx = (((unsigned)row) + SigElem.GetStartRow()) * kNumComps + col +
                 SigElem.GetStartCol();
  DXASSERT_NOMSG(idx < kMaxSigScalars);
  (void)kMaxSigScalars;
  return idx;
}

void DxilViewIdStateBuilder::UpdateDynamicIndexUsageState() const {
  UpdateDynamicIndexUsageStateForSig(m_pModule->GetInputSignature(),
                                     m_InpSigDynIdxElems);
  UpdateDynamicIndexUsageStateForSig(m_pModule->GetOutputSignature(),
                                     m_OutSigDynIdxElems);
  UpdateDynamicIndexUsageStateForSig(m_pModule->GetPatchConstOrPrimSignature(),
                                     m_PCSigDynIdxElems);
}

void DxilViewIdStateBuilder::UpdateDynamicIndexUsageStateForSig(
    DxilSignature &Sig, const DynamicallyIndexedElemsType &DynIdxElems) const {
  for (auto it : DynIdxElems) {
    unsigned id = it.first;
    unsigned mask = it.second;
    DxilSignatureElement &E = Sig.GetElement(id);
    E.SetDynIdxCompMask(mask);
  }
}

namespace {
class ComputeViewIdState : public ModulePass {
public:
  static char ID; // Pass ID, replacement for typeid

  ComputeViewIdState();

  bool runOnModule(Module &M) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override;

  void print(raw_ostream &o, const Module *M) const override {
    DxilModule &DxilModule = M->GetDxilModule();
    const ShaderModel *pSM = DxilModule.GetShaderModel();
    if (pSM->IsCS() || pSM->IsLib())
      return;

    auto &SerializedViewIdState = DxilModule.GetSerializedViewIdState();
    DxilViewIdState ViewIdState(&DxilModule);
    ViewIdState.Deserialize(SerializedViewIdState.data(),
                            SerializedViewIdState.size());
    ViewIdState.PrintSets(errs());
  }
};
} // namespace

char ComputeViewIdState::ID = 0;

INITIALIZE_PASS_BEGIN(ComputeViewIdState, "viewid-state",
                      "Compute information related to ViewID", true, true)
INITIALIZE_PASS_END(ComputeViewIdState, "viewid-state",
                    "Compute information related to ViewID", true, true)

ComputeViewIdState::ComputeViewIdState() : ModulePass(ID) {}

bool ComputeViewIdState::runOnModule(Module &M) {
  DxilModule &DxilModule = M.GetOrCreateDxilModule();
  const ShaderModel *pSM = DxilModule.GetShaderModel();
  if (!pSM->IsCS() && !pSM->IsLib()) {
    DxilViewIdState ViewIdState(&DxilModule);
    DxilViewIdStateBuilder Builder(ViewIdState, &DxilModule);
    Builder.Compute();
    // Serialize viewidstate.
    ViewIdState.Serialize();
    auto &TmpSerialized = ViewIdState.GetSerialized();
    // Copy serilized viewidstate.
    auto &SerializedViewIdState = DxilModule.GetSerializedViewIdState();
    SerializedViewIdState.clear();
    SerializedViewIdState.resize(TmpSerialized.size());
    SerializedViewIdState.assign(TmpSerialized.begin(), TmpSerialized.end());
    return true;
  }
  return false;
}

void ComputeViewIdState::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
}

namespace llvm {

ModulePass *createComputeViewIdStatePass() { return new ComputeViewIdState(); }

} // end of namespace llvm
