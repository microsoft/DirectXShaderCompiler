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

#define DXILVIEWID_DBG   1

#define DEBUG_TYPE "viewid"


static bool GetUnsignedVal(Value *V, uint32_t *pValue) {
  ConstantInt *CI = dyn_cast<ConstantInt>(V);
  if (!CI) return false;
  uint64_t u = CI->getZExtValue();
  if (u > UINT32_MAX) return false;
  *pValue = (uint32_t)u;
  return true;
}

void DxilViewIdState::Compute(Module *pModule) {
  if (m_pModule) Clear();
  m_pModule = &pModule->GetOrCreateDxilModule();

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
  CreateViewIdSets();

#if DXILVIEWID_DBG
  PrintSets(dbgs());
#endif
}

void DxilViewIdState::PrintSets(llvm::raw_ostream &OS) {
  OS << "ViewId and OutputInput sets for shader '" << m_pModule->GetEntryFunction()->getName() << "'\n";
  OS << "Number of inputs: " << m_NumInputSigScalars  << 
               ", outputs: " << m_NumOutputSigScalars << 
            ", patchconst: " << m_NumPCSigScalars     << "\n";
  OS << "Outputs dependent on ViewId: { ";
  bool bFirst = true;
  for (unsigned i = 0; i < m_NumOutputSigScalars; i++) {
    if (m_OutputsDependentOnViewId[i]) {
      if (!bFirst) OS << ", ";
      OS << i;
      bFirst = false;
    }
  }
  OS << " }\n";
  OS << "Inputs contributing to computation of outputs:\n";
  for (auto &it : m_InputsContributingToOutputs) {
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
  m_pModule = nullptr;
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
        if (row == Semantic::kUndefinedRow) {
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
      CollectValuesContributingToOutputRec(pContributingValue, ContributingInstructions);
    } else {
      // Dynamically indexed output.
      InstructionSetType ContributingInstructions;
      CollectValuesContributingToOutputRec(pContributingValue, ContributingInstructions);

      for (int row = startRow; row <= endRow; row++) {
        unsigned index = GetLinearIndex(SigElem, row, col);
        InstructionSetType NewSet;
        std::set_union(Entry.ContributingInstructions[index].begin(),
                       Entry.ContributingInstructions[index].end(),
                       ContributingInstructions.begin(),
                       ContributingInstructions.end(),
                       std::inserter(NewSet, NewSet.end()));
        Entry.ContributingInstructions[index].swap(NewSet);
      }
    }
  }
}

void DxilViewIdState::CollectValuesContributingToOutputRec(Value *pContributingValue,
                                                           InstructionSetType &ContributingInstructions) {
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
        CollectValuesContributingToOutputRec(pPredBB->getTerminator(), ContributingInstructions);
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
        CollectValuesContributingToOutputRec(V, ContributingInstructions);
      }
    }
    // TODO: UAVs
  } else if (LlvmInst_Call CI = LlvmInst_Call(pContributingInst)) {
    if (!hlsl::OP::IsDxilOpFuncCallInst(CI.Instr)) {
      // Return value of a user function.
      DXASSERT_NOMSG(false);
    }
  }

  // Handle instruction inputs.
  unsigned NumOps = pContributingInst->getNumOperands();
  for (unsigned i = 0; i < NumOps; i++) {
    Value *O = pContributingInst->getOperand(i);
    CollectValuesContributingToOutputRec(O, ContributingInstructions);
  }

  // Handle control dependence of this instruction BB.
  BasicBlock *pBB = pContributingInst->getParent();
  Function *F = pBB->getParent();
  FuncInfo *pFuncInfo = m_FuncInfo[F].get();
  const BasicBlockSet &CtrlDepSet = pFuncInfo->CtrlDep.GetCDBlocks(pBB);
  for (BasicBlock *B : CtrlDepSet) {
    CollectValuesContributingToOutputRec(B->getTerminator(), ContributingInstructions);
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
      ValueSetType NewSet;
      std::set_union(ReachingDecls.begin(), ReachingDecls.end(),
                     it->second.begin(), it->second.end(),
                     std::inserter(NewSet, NewSet.end()));
      ReachingDecls.swap(NewSet);
      return;
    }
  }

  if (GlobalVariable *GV = dyn_cast<GlobalVariable>(pValue)) {
    ReachingDecls.emplace(pValue);
    return;
  }

  if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(pValue)) {
    Value *pPtrValue = GEP->getPointerOperand();
    CollectReachingDeclsRec(pPtrValue, ReachingDecls, Visited);
  } else if (AllocaInst *AI = dyn_cast<AllocaInst>(pValue)) {
    ReachingDecls.emplace(pValue);
  } else if (PHINode *phi = dyn_cast<PHINode>(pValue)) {
    for (Value *pPtrValue : phi->operands()) {
      CollectReachingDeclsRec(pPtrValue, ReachingDecls, Visited);
    }
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
      ValueSetType NewSet;
      std::set_union(Stores.begin(), Stores.end(),
                     it->second.begin(), it->second.end(),
                     std::inserter(NewSet, NewSet.end()));
      Stores.swap(NewSet);
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

void DxilViewIdState::CreateViewIdSets() {
  // Inputs-to-outputs.
  for (auto &itOut : m_Entry.ContributingInstructions) {
    unsigned outIdx = itOut.first;
    InstructionSetType &ContributingInstrunction = itOut.second;
    for (Instruction *pInst : ContributingInstrunction) {
      if (DxilInst_LoadInput LI = DxilInst_LoadInput(pInst)) {
        // Update set of inputs that contribute to computation of this output.
        unsigned inpId = (unsigned)-1;
        int startRow = Semantic::kUndefinedRow, endRow = Semantic::kUndefinedRow;
        unsigned col = (unsigned)-1;
        GetUnsignedVal(LI.get_inputSigId(), &inpId);
        GetUnsignedVal(LI.get_colIndex(), &col);
        GetUnsignedVal(LI.get_rowIndex(), (uint32_t*)&startRow);

        DxilSignatureElement &SigElem = m_pModule->GetInputSignature().GetElement(inpId);
        if (startRow != Semantic::kUndefinedRow) {
          endRow = startRow;
        } else {
          // The entire column contributes to output.
          startRow = 0;
          endRow = SigElem.GetRows() - 1;
        }

        auto &ContributingInputs = m_InputsContributingToOutputs[outIdx];
        for (int row = startRow; row <= endRow; row++) {
          unsigned index = GetLinearIndex(SigElem, row, col);
          ContributingInputs.emplace(index);
        }
      } else if (DxilInst_ViewID VID = DxilInst_ViewID(pInst)) {
        // Set that output depends on ViewId.
        m_OutputsDependentOnViewId[outIdx] = true;
      }
    }
  }

  // TODO: HS, DS, in HS propagate output deps to inputs in PC phase.
}

unsigned DxilViewIdState::GetLinearIndex(DxilSignatureElement &SigElem, int row, unsigned col) const {
  DXASSERT_NOMSG(row >= 0 && col < kNumComps && SigElem.GetStartRow() != Semantic::kUndefinedRow);
  unsigned idx = (((unsigned)row) + SigElem.GetStartRow())*kNumComps + col + SigElem.GetStartCol();
  return idx;
}


char ComputeViewIdState::ID = 0;

INITIALIZE_PASS_BEGIN(ComputeViewIdState, "viewid_state",
                "Compute information related to ViewID", true, true)
INITIALIZE_PASS_END(ComputeViewIdState, "viewid_state",
                "Compute information related to ViewID", true, true)

ComputeViewIdState::ComputeViewIdState() : ModulePass(ID) {
}

bool ComputeViewIdState::runOnModule(Module &M) {
  DxilViewIdState ViewIdState;
  ViewIdState.Compute(&M);
  return false;
}


namespace llvm {

ModulePass *createComputeViewIdStatePass() {
  return new ComputeViewIdState();
}

} // end of namespace llvm
