///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// ComputeViewIdSets.h                                                       //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Computes output registers dependent on ViewID.                            //
// Computes sets of input registers on which output registers depend.        //
// Computes which input/output shapes are dynamically indexed.               //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once
#include "llvm/Pass.h"
#include "dxc/HLSL/ControlDependence.h"
#include "llvm/Support/GenericDomTree.h"

#include <memory>
#include <bitset>
#include <unordered_set>
#include <unordered_map>
#include <set>
#include <map>

namespace llvm {
  class Module;
  class Function;
  class BasicBlock;
  class Instruction;
  class ReturnInst;
  class Value;
  class PHINode;
  class AnalysisUsage;
  class CallGraph;
  class CallGraphNode;
  class ModulePass;
  class raw_ostream;
}

namespace hlsl {

class DxilModule;
class DxilSignature;
class DxilSignatureElement;

class DxilViewIdState {
  static const unsigned kNumComps = 4;
  static const unsigned kMaxSigScalars = 32*4;
public:
  using OutputsDependentOnViewIdType = std::bitset<kMaxSigScalars>;
  using InputsContributingToOutputType = std::map<unsigned, std::set<unsigned>>;

  DxilViewIdState(DxilModule *pDxilModule);

  unsigned getNumInputSigScalars() const;
  unsigned getNumOutputSigScalars(unsigned StreamId) const;
  unsigned getNumPCSigScalars() const;
  const OutputsDependentOnViewIdType &getOutputsDependentOnViewId(unsigned StreamId) const;
  const OutputsDependentOnViewIdType &getPCOutputsDependentOnViewId() const;
  const InputsContributingToOutputType &getInputsContributingToOutputs(unsigned StreamId) const;
  const InputsContributingToOutputType &getInputsContributingToPCOutputs() const;
  const InputsContributingToOutputType &getPCInputsContributingToOutputs() const;

  void Compute();
  void Serialize();
  const std::vector<unsigned> &GetSerialized();
  const std::vector<unsigned> &GetSerialized() const;   // returns previously serialized data
  void Deserialize(const unsigned *pData, unsigned DataSizeInUINTs);
  void PrintSets(llvm::raw_ostream &OS);

private:
  static const unsigned kNumStreams = 4;

  DxilModule *m_pModule;

  bool m_bUsesViewId = false;

  unsigned m_NumInputSigScalars  = 0;
  unsigned m_NumOutputSigScalars[kNumStreams] = {0,0,0,0};
  unsigned m_NumPCSigScalars     = 0;

  // Dynamically indexed components of signature elements.
  using DynamicallyIndexedElemsType = std::unordered_map<unsigned, unsigned>;
  DynamicallyIndexedElemsType m_InpSigDynIdxElems;
  DynamicallyIndexedElemsType m_OutSigDynIdxElems;
  DynamicallyIndexedElemsType m_PCSigDynIdxElems;

  // Set of scalar outputs dependent on ViewID.
  OutputsDependentOnViewIdType m_OutputsDependentOnViewId[kNumStreams];
  OutputsDependentOnViewIdType m_PCOutputsDependentOnViewId;

  // Set of scalar inputs contributing to computation of scalar outputs.
  InputsContributingToOutputType m_InputsContributingToOutputs[kNumStreams];
  InputsContributingToOutputType m_InputsContributingToPCOutputs; // HS PC only.
  InputsContributingToOutputType m_PCInputsContributingToOutputs; // DS only.

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
    std::unordered_map<unsigned, InstructionSetType> ContributingInstructions[kNumStreams];

    void Clear();
  };

  EntryInfo m_Entry;
  EntryInfo m_PCEntry;

  // Information per function.
  using FunctionReturnSet = std::unordered_set<llvm::ReturnInst *>;
  struct FuncInfo {
    FunctionReturnSet Returns;
    ControlDependence CtrlDep;
    std::unique_ptr<llvm::DominatorTreeBase<llvm::BasicBlock> > pDomTree;
    void Clear();
  };

  std::unordered_map<llvm::Function *, std::unique_ptr<FuncInfo>> m_FuncInfo;

  // Cache of decls (global/alloca) reaching a pointer value.
  using ValueSetType = std::unordered_set<llvm::Value *>;
  std::unordered_map<llvm::Value *, ValueSetType> m_ReachingDeclsCache;
  // Cache of stores for each decl.
  std::unordered_map<llvm::Value *, ValueSetType> m_StoresPerDeclCache;

  // Serialized form.
  std::vector<unsigned> m_SerializedState;

  void Clear();
  void DetermineMaxPackedLocation(DxilSignature &DxilSig, unsigned *pMaxSigLoc, unsigned NumStreams);
  void ComputeReachableFunctionsRec(llvm::CallGraph &CG, llvm::CallGraphNode *pNode, FunctionSetType &FuncSet);
  void AnalyzeFunctions(EntryInfo &Entry);
  void CollectValuesContributingToOutputs(EntryInfo &Entry);
  void CollectValuesContributingToOutputRec(EntryInfo &Entry,
                                            llvm::Value *pContributingValue,
                                            InstructionSetType &ContributingInstructions);
  void CollectPhiCFValuesContributingToOutputRec(llvm::PHINode *pPhi,
                                                 EntryInfo &Entry,
                                                 InstructionSetType &ContributingInstructions);
  const ValueSetType &CollectReachingDecls(llvm::Value *pValue);
  void CollectReachingDeclsRec(llvm::Value *pValue, ValueSetType &ReachingDecls, ValueSetType &Visited);
  const ValueSetType &CollectStores(llvm::Value *pValue);
  void CollectStoresRec(llvm::Value *pValue, ValueSetType &Stores, ValueSetType &Visited);
  void UpdateDynamicIndexUsageState() const;
  void CreateViewIdSets(const std::unordered_map<unsigned, InstructionSetType> &ContributingInstructions,
                        OutputsDependentOnViewIdType &OutputsDependentOnViewId,
                        InputsContributingToOutputType &InputsContributingToOutputs, bool bPC);

  void UpdateDynamicIndexUsageStateForSig(DxilSignature &Sig, const DynamicallyIndexedElemsType &DynIdxElems) const;
  void SerializeOutputsDependentOnViewId(unsigned NumOutputs, 
                                         const OutputsDependentOnViewIdType &OutputsDependentOnViewId,
                                         unsigned *&pData);
  void SerializeInputsContributingToOutput(unsigned NumInputs, unsigned NumOutputs,
                                           const InputsContributingToOutputType &InputsContributingToOutputs,
                                           unsigned *&pData);
  unsigned DeserializeOutputsDependentOnViewId(unsigned NumOutputs, 
                                               OutputsDependentOnViewIdType &OutputsDependentOnViewId,
                                               const unsigned *pData, unsigned DataSize);
  unsigned DeserializeInputsContributingToOutput(unsigned NumInputs, unsigned NumOutputs,
                                                 InputsContributingToOutputType &InputsContributingToOutputs,
                                                 const unsigned *pData, unsigned DataSize);
  unsigned GetLinearIndex(DxilSignatureElement &SigElem, int row, unsigned col) const;

  void PrintOutputsDependentOnViewId(llvm::raw_ostream &OS,
                                     llvm::StringRef SetName, unsigned NumOutputs,
                                     const OutputsDependentOnViewIdType &OutputsDependentOnViewId);
  void PrintInputsContributingToOutputs(llvm::raw_ostream &OS,
                                        llvm::StringRef InputSetName, llvm::StringRef OutputSetName,
                                        const InputsContributingToOutputType &InputsContributingToOutputs);
};

} // end of hlsl namespace


namespace llvm {

class ComputeViewIdState : public ModulePass {
public:
  static char ID; // Pass ID, replacement for typeid

  ComputeViewIdState();

  bool runOnModule(Module &M) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override;
};

void initializeComputeViewIdStatePass(llvm::PassRegistry &);
llvm::ModulePass *createComputeViewIdStatePass();

} // end of llvm namespace
