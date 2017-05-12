///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// ViewIdAnalysis.cpp                                                        //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/HLSL/ComputeViewIdState.h"
#include "dxc/Support/Global.h"
#include "dxc/HLSL/DxilModule.h"
#include "dxc/HLSL/DxilOperations.h"
#include "dxc/HLSL/DxilInstructions.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Operator.h"
#include "llvm/Pass.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Support/Debug.h"
#include "llvm/IR/CFG.h"
#include "llvm/Analysis/CallGraph.h"

#include <algorithm>

using namespace llvm;
using namespace llvm::legacy;
using namespace hlsl;
using llvm::legacy::PassManager;
using llvm::legacy::FunctionPassManager;
using std::vector;

#define DXILVIEWID_DBG   1

#define DEBUG_TYPE "viewid"


DxilViewIdState::DxilViewIdState(DxilModule *pDxilModule) : m_pModule(pDxilModule) {}
unsigned DxilViewIdState::getNumInputSigScalars() const   { return m_NumInputSigScalars; }
unsigned DxilViewIdState::getNumOutputSigScalars() const  { return m_NumOutputSigScalars; }
unsigned DxilViewIdState::getNumPCSigScalars() const      { return m_NumPCSigScalars; }
const DxilViewIdState::OutputsDependentOnViewIdType   &DxilViewIdState::getOutputsDependentOnViewId() const      { return m_OutputsDependentOnViewId; }
const DxilViewIdState::OutputsDependentOnViewIdType   &DxilViewIdState::getPCOutputsDependentOnViewId() const    { return m_PCOutputsDependentOnViewId; }
const DxilViewIdState::InputsContributingToOutputType &DxilViewIdState::getInputsContributingToOutputs() const   { return m_InputsContributingToOutputs; }
const DxilViewIdState::InputsContributingToOutputType &DxilViewIdState::getInputsContributingToPCOutputs() const { return m_InputsContributingToPCOutputs; }
const DxilViewIdState::InputsContributingToOutputType &DxilViewIdState::getPCInputsContributingToOutputs() const { return m_PCInputsContributingToOutputs; }

void DxilViewIdState::Compute() {
  Clear();

  // 1. Traverse signature MD to determine max packed location.
  DetermineMaxPackedLocation(m_pModule->GetInputSignature(), m_NumInputSigScalars);
  DetermineMaxPackedLocation(m_pModule->GetOutputSignature(), m_NumOutputSigScalars);
  DetermineMaxPackedLocation(m_pModule->GetPatchConstantSignature(), m_NumPCSigScalars);

  // 2. Collect sets of functions reachable from main and pc entries.
  CallGraphAnalysis CGA;
  CallGraph CG = CGA.run(m_pModule->GetModule());
  m_Entry.pEntryFunc = m_pModule->GetEntryFunction();
  m_PCEntry.pEntryFunc = m_pModule->GetPatchConstantFunction();
  ComputeReachableFunctionsRec(CG, CG[m_Entry.pEntryFunc], m_Entry.Functions);
  if (m_PCEntry.pEntryFunc) {
    ComputeReachableFunctionsRec(CG, CG[m_PCEntry.pEntryFunc], m_PCEntry.Functions);
  }

  // 3. Determine shape components that are dynamically accesses and collect all sig outputs.
  AnalyzeFunctions(m_Entry);
  AnalyzeFunctions(m_PCEntry);

  // 4. Collect sets of values contributing to outputs.
  CollectValuesContributingToOutputs(m_Entry);
  CollectValuesContributingToOutputs(m_PCEntry);

  // 5. Construct dependency sets.
  const ShaderModel *pSM = m_pModule->GetShaderModel();
  CreateViewIdSets(m_Entry, m_OutputsDependentOnViewId, m_InputsContributingToOutputs, false);
  if (pSM->IsHS()) {
    CreateViewIdSets(m_PCEntry, m_PCOutputsDependentOnViewId, m_InputsContributingToPCOutputs, true);
  } else if (pSM->IsDS()) {
    CreateViewIdSets(m_PCEntry, m_OutputsDependentOnViewId, m_PCInputsContributingToOutputs, true);
  }

  // 6. Update dynamically indexed input/output component masks.
  UpdateDynamicIndexUsageState();

#if DXILVIEWID_DBG
  PrintSets(dbgs());
#endif
}

void DxilViewIdState::PrintSets(llvm::raw_ostream &OS) {
  const ShaderModel *pSM = m_pModule->GetShaderModel();
  OS << "ViewId state: \n";
  OS << "Number of inputs: " << m_NumInputSigScalars  << 
               ", outputs: " << m_NumOutputSigScalars << 
            ", patchconst: " << m_NumPCSigScalars     << "\n";
  PrintOutputsDependentOnViewId(OS, "Outputs", m_NumOutputSigScalars, m_OutputsDependentOnViewId);
  if (pSM->IsHS()) {
    PrintOutputsDependentOnViewId(OS, "PCOutputs", m_NumPCSigScalars, m_PCOutputsDependentOnViewId);
  }

  PrintInputsContributingToOutputs(OS, "Inputs", "Outputs", m_InputsContributingToOutputs);
  if (pSM->IsHS()) {
    PrintInputsContributingToOutputs(OS, "Inputs", "PCOutputs", m_InputsContributingToPCOutputs);
  } else if (pSM->IsDS()) {
    PrintInputsContributingToOutputs(OS, "PCInputs", "Outputs", m_PCInputsContributingToOutputs);
  }
  OS << "\n";
}

void DxilViewIdState::PrintOutputsDependentOnViewId(llvm::raw_ostream &OS,
                                                    llvm::StringRef SetName,
                                                    unsigned NumOutputs,
                                                    const OutputsDependentOnViewIdType &OutputsDependentOnViewId) {
  OS << SetName << " dependent on ViewId: { ";
  bool bFirst = true;
  for (unsigned i = 0; i < NumOutputs; i++) {
    if (OutputsDependentOnViewId[i]) {
      if (!bFirst) OS << ", ";
      OS << i;
      bFirst = false;
    }
  }
  OS << " }\n";
}

void DxilViewIdState::PrintInputsContributingToOutputs(llvm::raw_ostream &OS,
                                                       llvm::StringRef InputSetName,
                                                       llvm::StringRef OutputSetName,
                                                       const InputsContributingToOutputType &InputsContributingToOutputs) {
  OS << InputSetName << " contributing to computation of " << OutputSetName << ":\n";
  for (auto &it : InputsContributingToOutputs) {
    unsigned outIdx = it.first;
    auto &Inputs = it.second;
    OS << "output " << outIdx << " depends on inputs: { ";
    bool bFirst = true;
    for (unsigned i : Inputs) {
      if (!bFirst) OS << ", ";
      OS << i;
      bFirst = false;
    }
    OS << " }\n";
  }
}

void DxilViewIdState::Clear() {
  m_NumInputSigScalars  = 0;
  m_NumOutputSigScalars = 0;
  m_NumPCSigScalars     = 0;
  m_InpSigDynIdxElems.clear();
  m_OutSigDynIdxElems.clear();
  m_PCSigDynIdxElems.clear();
  m_OutputsDependentOnViewId.reset();
  m_PCOutputsDependentOnViewId.reset();
  m_InputsContributingToOutputs.clear();
  m_InputsContributingToPCOutputs.clear();
  m_PCInputsContributingToOutputs.clear();
  m_Entry.Clear();
  m_PCEntry.Clear();
  m_FuncInfo.clear();
  m_ReachingDeclsCache.clear();
  m_SerializedState.clear();
}

void DxilViewIdState::EntryInfo::Clear() {
  pEntryFunc = nullptr;
  Functions.clear();
  Outputs.clear();
  ContributingInstructions.clear();
}

void DxilViewIdState::FuncInfo::Clear() {
  Returns.clear();
  CtrlDep.Clear();
}

void DxilViewIdState::DetermineMaxPackedLocation(DxilSignature &DxilSig, unsigned &MaxSigLoc) {
  if (&DxilSig == nullptr) return;

  MaxSigLoc = 0;
  for (auto &E : DxilSig.GetElements()) {
    if (E->GetStartRow() == Semantic::kUndefinedRow) continue;

    unsigned endLoc = GetLinearIndex(*E, E->GetRows() - 1, E->GetCols() - 1);
    MaxSigLoc = std::max(MaxSigLoc, endLoc + 1);
    E->GetCols();
  }
}

void DxilViewIdState::ComputeReachableFunctionsRec(CallGraph &CG, CallGraphNode *pNode, FunctionSetType &FuncSet) {
  Function *F = pNode->getFunction();
  // Accumulate only functions with bodies.
  if (F->empty()) return;
  auto itIns = FuncSet.emplace(F);
  DXASSERT_NOMSG(itIns.second);
  for (auto it = pNode->begin(), itEnd = pNode->end(); it != itEnd; ++it) {
    CallGraphNode *pSuccNode = it->second;
    ComputeReachableFunctionsRec(CG, pSuccNode, FuncSet);
  }
}

static bool GetUnsignedVal(Value *V, uint32_t *pValue) {
  ConstantInt *CI = dyn_cast<ConstantInt>(V);
  if (!CI) return false;
  uint64_t u = CI->getZExtValue();
  if (u > UINT32_MAX) return false;
  *pValue = (uint32_t)u;
  return true;
}

void DxilViewIdState::AnalyzeFunctions(EntryInfo &Entry) {
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

      for (auto itInst = BB->begin(), endInst = BB->end(); itInst != endInst; ++itInst) {
        if (ReturnInst *RI = dyn_cast<ReturnInst>(itInst)) {
          pFuncInfo->Returns.emplace(RI);
          continue;
        }

        CallInst *CI = dyn_cast<CallInst>(itInst);
        if (!CI) continue;

        DynamicallyIndexedElemsType *pDynIdxElems = nullptr;
        int row = Semantic::kUndefinedRow;
        unsigned id, col;
        if (DxilInst_LoadInput LI = DxilInst_LoadInput(CI)) {
          pDynIdxElems = &m_InpSigDynIdxElems;
          IFTBOOL(GetUnsignedVal(LI.get_inputSigId(), &id), DXC_E_GENERAL_INTERNAL_ERROR);
          GetUnsignedVal(LI.get_rowIndex(), (uint32_t*)&row);
          IFTBOOL(GetUnsignedVal(LI.get_colIndex(), &col), DXC_E_GENERAL_INTERNAL_ERROR);
        } else if (DxilInst_StoreOutput SO = DxilInst_StoreOutput(CI)) {
          pDynIdxElems = &m_OutSigDynIdxElems;
          IFTBOOL(GetUnsignedVal(SO.get_outputtSigId(), &id), DXC_E_GENERAL_INTERNAL_ERROR);
          GetUnsignedVal(SO.get_rowIndex(), (uint32_t*)&row);
          IFTBOOL(GetUnsignedVal(SO.get_colIndex(), &col), DXC_E_GENERAL_INTERNAL_ERROR);
          Entry.Outputs.emplace(CI);
        } else if (DxilInst_LoadPatchConstant LPC = DxilInst_LoadPatchConstant(CI)) {
          if (m_pModule->GetShaderModel()->IsDS()) {
            pDynIdxElems = &m_PCSigDynIdxElems;
            IFTBOOL(GetUnsignedVal(LPC.get_inputSigId(), &id), DXC_E_GENERAL_INTERNAL_ERROR);
            GetUnsignedVal(LPC.get_row(), (uint32_t*)&row);
            IFTBOOL(GetUnsignedVal(LPC.get_col(), &col), DXC_E_GENERAL_INTERNAL_ERROR);
          } else {
            // Do nothing. This is an internal helper function for DXBC-2-DXIL converter.
            DXASSERT_NOMSG(m_pModule->GetShaderModel()->IsHS());
          }
        } else if (DxilInst_StorePatchConstant SPC = DxilInst_StorePatchConstant(CI)) {
          pDynIdxElems = &m_PCSigDynIdxElems;
          IFTBOOL(GetUnsignedVal(SPC.get_outputSigID(), &id), DXC_E_GENERAL_INTERNAL_ERROR);
          GetUnsignedVal(SPC.get_row(), (uint32_t*)&row);
          IFTBOOL(GetUnsignedVal(SPC.get_col(), &col), DXC_E_GENERAL_INTERNAL_ERROR);
          Entry.Outputs.emplace(CI);
        } else if (DxilInst_LoadOutputControlPoint LOCP = DxilInst_LoadOutputControlPoint(CI)) {
          if (m_pModule->GetShaderModel()->IsDS()) {
            pDynIdxElems = &m_InpSigDynIdxElems;
            IFTBOOL(GetUnsignedVal(LOCP.get_inputSigId(), &id), DXC_E_GENERAL_INTERNAL_ERROR);
            GetUnsignedVal(LOCP.get_row(), (uint32_t*)&row);
            IFTBOOL(GetUnsignedVal(LOCP.get_col(), &col), DXC_E_GENERAL_INTERNAL_ERROR);
          } else if (m_pModule->GetShaderModel()->IsHS()) {
            // Do nothings, as the information has been captured by the output signature of CP entry.
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

void DxilViewIdState::CollectValuesContributingToOutputs(EntryInfo &Entry) {
  for (auto *CI : Entry.Outputs) {  // CI = call instruction
    DxilSignature *pDxilSig = nullptr;
    Value *pContributingValue = nullptr;
    unsigned id = (unsigned)-1;
    int startRow = Semantic::kUndefinedRow, endRow = Semantic::kUndefinedRow;
    unsigned col = (unsigned)-1;
    if (DxilInst_StoreOutput SO = DxilInst_StoreOutput(CI)) {
      pDxilSig = &m_pModule->GetOutputSignature();
      pContributingValue = SO.get_value();
      GetUnsignedVal(SO.get_outputtSigId(), &id);
      GetUnsignedVal(SO.get_colIndex(), &col);
      GetUnsignedVal(SO.get_rowIndex(), (uint32_t*)&startRow);
    } else if (DxilInst_StorePatchConstant SPC = DxilInst_StorePatchConstant(CI)) {
      pDxilSig = &m_pModule->GetPatchConstantSignature();
      pContributingValue = SPC.get_value();
      GetUnsignedVal(SPC.get_outputSigID(), &id);
      GetUnsignedVal(SPC.get_row(), (uint32_t*)&startRow);
      GetUnsignedVal(SPC.get_col(), &col);
    } else {
      IFT(DXC_E_GENERAL_INTERNAL_ERROR);
    }

    DxilSignatureElement &SigElem = pDxilSig->GetElement(id);
    if (!SigElem.IsAllocated())
      continue;

    if (startRow != Semantic::kUndefinedRow) {
      endRow = startRow;
    } else {
      // The entire column is affected by value.
      DXASSERT_NOMSG(SigElem.GetID() == id && SigElem.GetStartRow() != Semantic::kUndefinedRow);
      startRow = 0;
      endRow = SigElem.GetRows() - 1;
    }

    if (startRow == endRow) {
      // Scalar or indexable with known index.
      unsigned index = GetLinearIndex(SigElem, startRow, col);
      InstructionSetType &ContributingInstructions = Entry.ContributingInstructions[index];
      CollectValuesContributingToOutputRec(Entry, pContributingValue, ContributingInstructions);
    } else {
      // Dynamically indexed output.
      InstructionSetType ContributingInstructions;
      CollectValuesContributingToOutputRec(Entry, pContributingValue, ContributingInstructions);

      for (int row = startRow; row <= endRow; row++) {
        unsigned index = GetLinearIndex(SigElem, row, col);
        Entry.ContributingInstructions[index].insert(ContributingInstructions.begin(), ContributingInstructions.end());
        //QQQ
#if 0
        auto it = Entry.ContributingInstructions.emplace(index, InstructionSetType{});
        if (it.second) {
          it.first->second = ContributingInstructions;
        } else {
          InstructionSetType NewSet;
          std::set_union(it.first->second.begin(),
                         it.first->second.end(),
                         ContributingInstructions.begin(),
                         ContributingInstructions.end(),
                         std::inserter(NewSet, NewSet.end()));
          it.first->second.swap(NewSet);
        }
#endif
      }
    }
  }
}

void DxilViewIdState::CollectValuesContributingToOutputRec(EntryInfo &Entry,
                                                           Value *pContributingValue,
                                                           InstructionSetType &ContributingInstructions) {
  if (Argument *pArg = dyn_cast<Argument>(pContributingValue)) {
    // This must be a leftover signature argument of an entry function.
    DXASSERT_NOMSG(Entry.pEntryFunc == m_pModule->GetEntryFunction() ||
                   Entry.pEntryFunc == m_pModule->GetPatchConstantFunction());
    return;
  }

  Instruction *pContributingInst = dyn_cast<Instruction>(pContributingValue);
  if (pContributingInst == nullptr) {
    // Can be literal constant, global decl, branch target.
    DXASSERT_NOMSG(isa<Constant>(pContributingValue) || isa<BasicBlock>(pContributingValue));
    return;
  }

  auto itInst = ContributingInstructions.emplace(pContributingInst);
  // Already visited instruction.
  if (!itInst.second) return;

  // Handle special cases.
  if (PHINode *phi = dyn_cast<PHINode>(pContributingInst)) {
    // For constant operands of phi, handle control dependence on incident edge.
    unsigned iOp = 0;
    for (auto it = phi->block_begin(), endIt = phi->block_end(); it != endIt; ++it, iOp++) {
      Value *O = phi->getOperand(iOp);
      if (isa<Constant>(O)) {
        BasicBlock *pPredBB = *it;
        CollectValuesContributingToOutputRec(Entry, pPredBB->getTerminator(), ContributingInstructions);
      }
    }
  } else if (isa<LoadInst>(pContributingInst) || 
             isa<AtomicCmpXchgInst>(pContributingInst) ||
             isa<AtomicRMWInst>(pContributingInst)) {
    Value *pPtrValue = pContributingInst->getOperand(0);
    DXASSERT_NOMSG(pPtrValue->getType()->isPointerTy());
    const ValueSetType &ReachingDecls = CollectReachingDecls(pPtrValue);
    DXASSERT_NOMSG(ReachingDecls.size() > 0);
    for (Value *pDeclValue : ReachingDecls) {
      const ValueSetType &Stores = CollectStores(pDeclValue);
      for (Value *V : Stores) {
        CollectValuesContributingToOutputRec(Entry, V, ContributingInstructions);
      }
    }
  } else if (CallInst *CI = dyn_cast<CallInst>(pContributingInst)) {
    if (!hlsl::OP::IsDxilOpFuncCallInst(CI)) {
      Function *F = CI->getCalledFunction();
      if (!F->empty()) {
        // Return value of a user function.
        if (Entry.Functions.find(F) != Entry.Functions.end()) {
          const FuncInfo &FI = *m_FuncInfo[F];
          for (ReturnInst *pRetInst : FI.Returns) {
            CollectValuesContributingToOutputRec(Entry, pRetInst, ContributingInstructions);
          }
        }
      }
    }
  }

  // Handle instruction inputs.
  unsigned NumOps = pContributingInst->getNumOperands();
  for (unsigned i = 0; i < NumOps; i++) {
    Value *O = pContributingInst->getOperand(i);
    CollectValuesContributingToOutputRec(Entry, O, ContributingInstructions);
  }

  // Handle control dependence of this instruction BB.
  BasicBlock *pBB = pContributingInst->getParent();
  Function *F = pBB->getParent();
  FuncInfo *pFuncInfo = m_FuncInfo[F].get();
  const BasicBlockSet &CtrlDepSet = pFuncInfo->CtrlDep.GetCDBlocks(pBB);
  for (BasicBlock *B : CtrlDepSet) {
    CollectValuesContributingToOutputRec(Entry, B->getTerminator(), ContributingInstructions);
  }
}

const DxilViewIdState::ValueSetType &DxilViewIdState::CollectReachingDecls(Value *pValue) {
  auto it = m_ReachingDeclsCache.emplace(pValue, ValueSetType());
  if (it.second) {
    // We have not seen this value before.
    ValueSetType Visited;
    CollectReachingDeclsRec(pValue, it.first->second, Visited);
  }
  return it.first->second;
}

void DxilViewIdState::CollectReachingDeclsRec(Value *pValue, ValueSetType &ReachingDecls, ValueSetType &Visited) {
  if (Visited.find(pValue) != Visited.end())
    return;

  bool bInitialValue = Visited.size() == 0;
  Visited.emplace(pValue);

  if (!bInitialValue) {
    auto it = m_ReachingDeclsCache.find(pValue);
    if (it != m_ReachingDeclsCache.end()) {
      //QQQ
#if 0
      ValueSetType NewSet;
      std::set_union(ReachingDecls.begin(), ReachingDecls.end(),
                     it->second.begin(), it->second.end(),
                     std::inserter(NewSet, NewSet.end()));
      ReachingDecls.swap(NewSet);
#endif
      ReachingDecls.insert(it->second.begin(), it->second.end());
      return;
    }
  }

  if (GlobalVariable *GV = dyn_cast<GlobalVariable>(pValue)) {
    ReachingDecls.emplace(pValue);
    return;
  }

  if (GetElementPtrInst *pGepInst = dyn_cast<GetElementPtrInst>(pValue)) {
    Value *pPtrValue = pGepInst->getPointerOperand();
    CollectReachingDeclsRec(pPtrValue, ReachingDecls, Visited);
  } else if (GEPOperator *pGepOp = dyn_cast<GEPOperator>(pValue)) {
    Value *pPtrValue = pGepOp->getPointerOperand();
    CollectReachingDeclsRec(pPtrValue, ReachingDecls, Visited);
  } else if (AllocaInst *AI = dyn_cast<AllocaInst>(pValue)) {
    ReachingDecls.emplace(pValue);
  } else if (PHINode *phi = dyn_cast<PHINode>(pValue)) {
    for (Value *pPtrValue : phi->operands()) {
      CollectReachingDeclsRec(pPtrValue, ReachingDecls, Visited);
    }
  } else if (Argument *pArg = dyn_cast<Argument>(pValue)) {
    ReachingDecls.emplace(pValue);
  } else {
    IFT(DXC_E_GENERAL_INTERNAL_ERROR);
  }
}

const DxilViewIdState::ValueSetType &DxilViewIdState::CollectStores(llvm::Value *pValue) {
  auto it = m_StoresPerDeclCache.emplace(pValue, ValueSetType());
  if (it.second) {
    // We have not seen this value before.
    ValueSetType Visited;
    CollectStoresRec(pValue, it.first->second, Visited);
  }
  return it.first->second;
}

void DxilViewIdState::CollectStoresRec(llvm::Value *pValue, ValueSetType &Stores, ValueSetType &Visited) {
  if (Visited.find(pValue) != Visited.end())
    return;

  bool bInitialValue = Visited.size() == 0;
  Visited.emplace(pValue);

  if (!bInitialValue) {
    auto it = m_StoresPerDeclCache.find(pValue);
    if (it != m_StoresPerDeclCache.end()) {
      //QQQ
#if 0
      ValueSetType NewSet;
      std::set_union(Stores.begin(), Stores.end(),
                     it->second.begin(), it->second.end(),
                     std::inserter(NewSet, NewSet.end()));
      Stores.swap(NewSet);
#endif
      Stores.insert(it->second.begin(), it->second.end());
      return;
    }
  }

  if (isa<LoadInst>(pValue)) {
    return;
  } else if (isa<StoreInst>(pValue) ||
             isa<AtomicCmpXchgInst>(pValue) ||
             isa<AtomicRMWInst>(pValue)) {
    Stores.emplace(pValue);
    return;
  }

  for (auto *U : pValue->users()) {
    CollectStoresRec(U, Stores, Visited);
  }
}

void DxilViewIdState::CreateViewIdSets(EntryInfo &Entry, 
                                       OutputsDependentOnViewIdType &OutputsDependentOnViewId,
                                       InputsContributingToOutputType &InputsContributingToOutputs,
                                       bool bPC) {
  const ShaderModel *pSM = m_pModule->GetShaderModel();

  for (auto &itOut : Entry.ContributingInstructions) {
    unsigned outIdx = itOut.first;
    InstructionSetType &ContributingInstrunction = itOut.second;
    for (Instruction *pInst : ContributingInstrunction) {
      // Set output dependence on ViewId.
      if (DxilInst_ViewID VID = DxilInst_ViewID(pInst)) {
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
        GetUnsignedVal(LI.get_rowIndex(), (uint32_t*)&startRow);
        pSigElem = &m_pModule->GetInputSignature().GetElement(inpId);
      } else if (DxilInst_LoadOutputControlPoint LOCP = DxilInst_LoadOutputControlPoint(pInst)) {
        GetUnsignedVal(LOCP.get_inputSigId(), &inpId);
        GetUnsignedVal(LOCP.get_col(), &col);
        GetUnsignedVal(LOCP.get_row(), (uint32_t*)&startRow);
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
      } else if (DxilInst_LoadPatchConstant LPC = DxilInst_LoadPatchConstant(pInst)) {
        if (pSM->IsDS() && bPC) {
          GetUnsignedVal(LPC.get_inputSigId(), &inpId);
          GetUnsignedVal(LPC.get_col(), &col);
          GetUnsignedVal(LPC.get_row(), (uint32_t*)&startRow);
          pSigElem = &m_pModule->GetPatchConstantSignature().GetElement(inpId);
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
            // This HS patch-constant output depends on an input value of LoadOutputControlPoint
            // that is the output value of the HS main (control-point) function.
            // Transitively update this (patch-constant) output dependence on main (control-point) output.
            DXASSERT_NOMSG(&OutputsDependentOnViewId == &m_PCOutputsDependentOnViewId);
            OutputsDependentOnViewId[outIdx] = OutputsDependentOnViewId[outIdx] || m_OutputsDependentOnViewId[index];

            const auto it = m_InputsContributingToOutputs.find(index);
            if (it != m_InputsContributingToOutputs.end()) {
              const std::set<unsigned> &LoadOutputCPInputsContributingToOutputs = it->second;
              //QQQ
#if 0
              std::set<unsigned> NewSet;
              std::set_union(ContributingInputs.begin(),
                             ContributingInputs.end(),
                             LoadOutputCPInputsContributingToOutputs.begin(),
                             LoadOutputCPInputsContributingToOutputs.end(),
                             std::inserter(NewSet, NewSet.end()));
              ContributingInputs.swap(NewSet);
#endif
              ContributingInputs.insert(LoadOutputCPInputsContributingToOutputs.begin(),
                                        LoadOutputCPInputsContributingToOutputs.end());
            }
          }
        }
      }
    }
  }
}

unsigned DxilViewIdState::GetLinearIndex(DxilSignatureElement &SigElem, int row, unsigned col) const {
  DXASSERT_NOMSG(row >= 0 && col < kNumComps && SigElem.GetStartRow() != Semantic::kUndefinedRow);
  unsigned idx = (((unsigned)row) + SigElem.GetStartRow())*kNumComps + col + SigElem.GetStartCol();
  return idx;
}

void DxilViewIdState::UpdateDynamicIndexUsageState() const {
  UpdateDynamicIndexUsageStateForSig(m_pModule->GetInputSignature(), m_InpSigDynIdxElems);
  UpdateDynamicIndexUsageStateForSig(m_pModule->GetOutputSignature(), m_OutSigDynIdxElems);
  UpdateDynamicIndexUsageStateForSig(m_pModule->GetPatchConstantSignature(), m_PCSigDynIdxElems);
}

void DxilViewIdState::UpdateDynamicIndexUsageStateForSig(DxilSignature &Sig,
                                                         const DynamicallyIndexedElemsType &DynIdxElems) const {
  for (auto it : DynIdxElems) {
    unsigned id = it.first;
    unsigned mask = it.second;
    DxilSignatureElement &E = Sig.GetElement(id);
    E.SetDynIdxCompMask(mask);
  }
}

static unsigned RoundUpToUINT(unsigned x) {
  return (x + 31)/32;
}

void DxilViewIdState::Serialize() {
  if (!m_SerializedState.empty())
    return;

  const ShaderModel *pSM = m_pModule->GetShaderModel();
  unsigned NumOutUINTs = RoundUpToUINT(m_NumOutputSigScalars);
  unsigned NumPCUINTs = RoundUpToUINT(m_NumPCSigScalars);
  unsigned Size1 = 2 + NumOutUINTs * (1 + m_NumInputSigScalars);
  unsigned Size2 = 0;
  if (pSM->IsHS()) {
    Size2 = 2 + NumPCUINTs * (1 + m_NumInputSigScalars);
  } else if (pSM->IsDS()) {
    Size2 = 2 + NumOutUINTs * (1 + m_NumPCSigScalars);
  }
  unsigned Size = Size1 + Size2;

  m_SerializedState.resize(Size);
  std::fill(m_SerializedState.begin(), m_SerializedState.end(), 0u);

  unsigned *pData = &m_SerializedState[0];
  Serialize1(m_NumInputSigScalars, m_NumOutputSigScalars,
             m_OutputsDependentOnViewId, m_InputsContributingToOutputs, pData);
  DXASSERT_NOMSG(pData == (&m_SerializedState[0] + Size1));

  if (pSM->IsHS()) {
    Serialize1(m_NumInputSigScalars, m_NumPCSigScalars,
               m_PCOutputsDependentOnViewId, m_InputsContributingToPCOutputs, pData);
  } else if (pSM->IsDS()) {
    Serialize1(m_NumPCSigScalars, m_NumOutputSigScalars,
               m_OutputsDependentOnViewId, m_PCInputsContributingToOutputs, pData);
  }
  DXASSERT_NOMSG(pData == (&m_SerializedState[0] + Size));
}
const vector<unsigned> &DxilViewIdState::GetSerialized() {
  if (m_SerializedState.empty())
    Serialize();
  return m_SerializedState;
}
const vector<unsigned> &DxilViewIdState::GetSerialized() const {
  return m_SerializedState;
}

void DxilViewIdState::Serialize1(unsigned NumInputs, unsigned NumOutputs,
                                 const OutputsDependentOnViewIdType &OutputsDependentOnViewId,
                                 const InputsContributingToOutputType &InputsContributingToOutputs,
                                 unsigned *&pData) {
  unsigned NumOutUINTs = RoundUpToUINT(NumOutputs);

  unsigned *p = pData;
  *p++ = NumInputs;
  *p++ = NumOutputs;

  // Serialize output dependence on ViewId.
  for (unsigned i = 0; i < NumOutUINTs; i++) {
    unsigned x = 0;
    for (unsigned j = 0; j < std::min(32u, NumOutputs - 32u*i); j++) {
      if (OutputsDependentOnViewId[i*32 + j]) {
        x |= (1u << j);
      }
    }
    *p++ = x;
  }

  // Serialize output dependence on inputs.
  for (unsigned outputIdx = 0; outputIdx < NumOutputs; outputIdx++) {
    auto it = InputsContributingToOutputs.find(outputIdx);
    if (it != InputsContributingToOutputs.end()) {
      for (unsigned inputIdx : it->second) {
        unsigned w = outputIdx / 32;
        unsigned b = outputIdx % 32;
        p[inputIdx*NumOutUINTs + w] |= (1u << b);
      }
    }
  }
  p += NumInputs * NumOutUINTs;

  pData = p;
}

void DxilViewIdState::Deserialize(const unsigned *pData, unsigned DataSize) {
  Clear();
  m_SerializedState.resize(DataSize);
  memcpy(m_SerializedState.data(), pData, DataSize * 4);

  const ShaderModel *pSM = m_pModule->GetShaderModel();
  unsigned ConsumedUINTs;
  ConsumedUINTs = Deserialize1(pData, DataSize, m_NumInputSigScalars, m_NumOutputSigScalars,
                               m_OutputsDependentOnViewId, m_InputsContributingToOutputs);

  if (ConsumedUINTs < DataSize) {
    unsigned t = 0;
    if (pSM->IsHS()) {
      unsigned NumInputs;
      t = Deserialize1(&pData[ConsumedUINTs], DataSize-ConsumedUINTs, NumInputs, m_NumPCSigScalars,
                       m_PCOutputsDependentOnViewId, m_InputsContributingToPCOutputs);
      DXASSERT_NOMSG(NumInputs == m_NumInputSigScalars);
    } else if (pSM->IsDS()) {
      unsigned NumOutputs;
      OutputsDependentOnViewIdType OutputsDependentOnViewId;
      t = Deserialize1(&pData[ConsumedUINTs], DataSize-ConsumedUINTs,
                       m_NumPCSigScalars, NumOutputs,
                       OutputsDependentOnViewId, m_PCInputsContributingToOutputs);
      DXASSERT_NOMSG(NumOutputs == m_NumOutputSigScalars);
      DXASSERT_NOMSG(OutputsDependentOnViewId == m_OutputsDependentOnViewId);
    }
    ConsumedUINTs += t;
  }
  DXASSERT_NOMSG(ConsumedUINTs == DataSize);
}

unsigned DxilViewIdState::Deserialize1(const unsigned *pData, unsigned DataSize,
                                       unsigned &NumInputs, unsigned &NumOutputs,
                                       OutputsDependentOnViewIdType &OutputsDependentOnViewId,
                                       InputsContributingToOutputType &InputsContributingToOutputs) {
  IFTBOOL(DataSize >= 2, DXC_E_GENERAL_INTERNAL_ERROR);
  const unsigned *p = pData;
  NumInputs  = *p++;
  NumOutputs = *p++;
  unsigned NumOutUINTs = RoundUpToUINT(NumOutputs);

  unsigned Size = 2 + NumOutUINTs * (1 +  NumInputs);
  IFTBOOL(Size <= DataSize, DXC_E_GENERAL_INTERNAL_ERROR);

  // Deserialize output dependence on ViewId.
  for (unsigned i = 0; i < NumOutUINTs; i++) {
    unsigned x = *p++;
    for (unsigned j = 0; j < std::min(32u, NumOutputs - 32u*i); j++) {
      if (x & (1u << j)) {
        OutputsDependentOnViewId[i*32 + j] = true;
      }
    }
  }

  // Deserialize output dependence on inputs.
  for (unsigned inputIdx = 0; inputIdx < NumInputs; inputIdx++) {
    for (unsigned outputIdx = 0; outputIdx < NumOutputs; outputIdx++) {
      unsigned w = outputIdx / 32;
      unsigned b = outputIdx % 32;
      if (p[inputIdx*NumOutUINTs + w] & (1u << b)) {
        InputsContributingToOutputs[outputIdx].insert(inputIdx);
      }
    }
  }

  return Size;
}


char ComputeViewIdState::ID = 0;

INITIALIZE_PASS_BEGIN(ComputeViewIdState, "viewid_state",
                "Compute information related to ViewID", true, true)
INITIALIZE_PASS_END(ComputeViewIdState, "viewid_state",
                "Compute information related to ViewID", true, true)

ComputeViewIdState::ComputeViewIdState() : ModulePass(ID) {
}

bool ComputeViewIdState::runOnModule(Module &M) {
  DxilModule &DxilModule = M.GetOrCreateDxilModule();
  const ShaderModel *pSM = DxilModule.GetShaderModel();
  if (!pSM->IsCS()) {
    DxilViewIdState &ViewIdState = DxilModule.GetViewIdState();
    ViewIdState.Compute();
    return true;
  }
  return false;
}

void ComputeViewIdState::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
}


namespace llvm {

ModulePass *createComputeViewIdStatePass() {
  return new ComputeViewIdState();
}

} // end of namespace llvm
