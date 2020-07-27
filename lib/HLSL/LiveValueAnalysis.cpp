///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// LiveValueAnalysis.cpp                                                     //
// Copyright (C) 2020 NVIDIA Corporation.  All rights reserved.              //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Live Value Analysis pass for HLSL.                                        //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/HLSL/LiveValueAnalysis.h"
#include "dxc/HLSL/HLOperations.h"
#include "dxc/DXIL/DxilUtil.h"
#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_os_ostream.h"
#include "dxc/DXIL/DxilMetadataHelper.h"
#include "dxc/Support/Global.h"

#include <algorithm>
#include <string>
#include <sstream>
#include <fstream>
#include <iomanip>

using namespace llvm;
using namespace hlsl;

namespace llvm {

  class LiveValueAnalysis : public ModulePass {

  private:

    struct LiveValueInst {
      // Name of the live value associated to the instruction.
      std::string   Name;
      Instruction  *Inst;
    };

    struct CallSiteCompare {
      inline bool operator() (const CallInst *lhs, const CallInst *rhs) const {
        std::string lhsStr;
        std::string rhsStr;
        if (const llvm::DebugLoc &debugInfo = lhs->getDebugLoc()) {
          lhsStr = formatSourceLocation(debugInfo->getFilename(), debugInfo->getLine());
        }
        if (const llvm::DebugLoc &debugInfo = rhs->getDebugLoc()) {
          rhsStr = formatSourceLocation(debugInfo->getFilename(), debugInfo->getLine());
        }
        // Case insensitive compare.
        const auto result = std::mismatch(lhsStr.cbegin(), lhsStr.cend(), rhsStr.cbegin(), rhsStr.cend(), [](const unsigned char lhsStr, const unsigned char rhsStr) {return tolower(lhsStr) == tolower(rhsStr); });
        return result.second != rhsStr.cend() && (result.first == lhsStr.cend() || tolower(*result.first) < tolower(*result.second));
      }
    };

    struct LiveValueCompare {
      inline bool operator() (LiveValueInst &lhs, LiveValueInst &rhs) {
        // Case insensitive compare.
        const auto result = std::mismatch(lhs.Name.cbegin(), lhs.Name.cend(), rhs.Name.cbegin(), rhs.Name.cend(), [](const unsigned char lhs, const unsigned char rhs) {return tolower(lhs) == tolower(rhs); });
        return result.second != rhs.Name.cend() && (result.first == lhs.Name.cend() || tolower(*result.first) < tolower(*result.second));
      }
    };

    Module* m_module = nullptr;
    // Set of trace ray call sites.
    std::set<CallInst *, CallSiteCompare> m_callSites;
    // Potential live values per trace call.
    MapVector<CallInst *, std::vector<Instruction *>> m_spillsPerTraceCall;
    // Sorted list of live values per trace call, with name information.
    MapVector<CallInst *, std::vector<LiveValueInst>> m_liveValuesPerTraceCall;
    static std::string m_rootPath;
    std::string m_outputFile;

  public:
    LiveValueAnalysis(StringRef LiveValueAnalysisOutputFile = "");
    ~LiveValueAnalysis() override;
    static char ID;

    bool runOnModule(Module &) override;

    void analyzeCFG();
    void analyzeLiveValues(CallInst *TraceCall, SetVector<Instruction *> &LiveValues, SetVector<std::pair<Instruction *, Instruction *>> &PredCfgEdges);
    void analyzeLiveValueOptimizations(CallInst *TraceCall, SetVector<Instruction *> &LiveValues, SetVector<std::pair<Instruction *, Instruction *>> &PredCfgEdges);
    bool canRemat(Instruction *Inst, CallInst *TraceCall);
    void determinePhisSpillSet(std::vector<PHINode *> &Phis, SetVector<Instruction *> &RematSet, SetVector<Instruction *> &SpillSet);
    void determineRematSpillSet(CallInst *TraceCall,
      SetVector<Instruction *> &RematSet,
      SetVector<Instruction *> &SpillSet,
      SetVector<Instruction *> &LiveValues,
      SetVector<std::pair<Instruction *, Instruction *>> &PredCfgEdges);
    void determineValueName(DIType *diType, int64_t &BitOffset, std::string &Name);
    static std::string formatSourceLocation(const std::string &Location, int LineNumber);
    void formatOutput(raw_string_ostream &PrettyStr, raw_string_ostream &VSStr);
    void getFormatLengths(SetVector<DILocation *> &UseLocations, uint32_t &MaxFuncLength, uint32_t &MaxPathLength);
    void getUseDefinitions(CallInst *CI, SetVector<Instruction *> &LiveValues, SetVector<Value *> &ConditionsKnownFalse, SetVector<Value *> &ConditionsKnownTrue);
    void getUseLocationsForInstruction(Instruction *I, CallInst *CI, SetVector<DILocation *> &UseLocations);
    void outputSourceLocationsPretty(SetVector<DILocation *> &UseLocations, uint32_t maxPathLength, uint32_t maxFuncLength, raw_string_ostream &PrettyStr);
    void outputSourceLocationsVS(SetVector<DILocation *> &UseLocations, uint32_t maxPathLength, uint32_t maxFuncLength, raw_string_ostream &VSStr);
    void processBranch(TerminatorInst *TI,
      SetVector<Value *> &ConditionsKnownFalse,
      SetVector<Value *> &ConditionsKnownTrue,
      SetVector<std::pair<Instruction *, Instruction *>> &VisitedEdges,
      SetVector<Instruction *> &DefsSeen,
      std::vector<std::tuple<BasicBlock *, Instruction *, SetVector<Instruction *>>> &Worklist);
    std::string translateValueToName(DbgValueInst *DVI, Instruction *I);
    bool isLoadRematerializable(CallInst *call);

  };

} // End llvm namespace

const char *kTraceRayOpName = "dx.op.traceRay";
const char inlinePrefix[] = "  -->inlined at ";
std::string LiveValueAnalysis::m_rootPath = "";

ModulePass *llvm::createLiveValueAnalysisPass(StringRef LiveValueAnalysisOutputFile) {
  return new LiveValueAnalysis(LiveValueAnalysisOutputFile);
}

// Register this pass...
char LiveValueAnalysis::ID = 0;
INITIALIZE_PASS(LiveValueAnalysis, "hlsl-lva", "Live Value Analysis and reporting for DXR", false, true)

LiveValueAnalysis::LiveValueAnalysis(StringRef LiveValueAnalysisOutputFile)
  : ModulePass(ID) {
  m_outputFile = LiveValueAnalysisOutputFile;
}

LiveValueAnalysis::~LiveValueAnalysis() {
}

bool LiveValueAnalysis::runOnModule(Module &M) {

  const char kRootPathPrefix[] = "LIVE_VALUE_REPORT_ROOT=";
  // Length of prefix to find, minus the null terminator.
  const size_t RootPrefixLength = sizeof(kRootPathPrefix) - 1;
  // PrettyReport is a more readable file output.
  // VSReport is a source linking debug console report in Visual Studio.
  std::string PrettyReport;
  std::string VSReport;
  raw_string_ostream PrettyStr(PrettyReport);
  raw_string_ostream VSStr(VSReport);

  m_module = &M;

  // Find any define for LIVE_VALUE_REPORT_ROOT in order to build absolute path
  // locations in the LVA Visual Studio (VS) console report.
  // Look for the debuginfo compile unit and then process Live Value Report if
  // it exists
  NamedMDNode *NMD = m_module->getNamedMetadata("llvm.dbg.cu");
  if (NMD) {
    llvm::NamedMDNode *Defines = m_module->getNamedMetadata(DxilMDHelper::kDxilSourceDefinesMDName);
    if (!Defines) {
      Defines = m_module->getNamedMetadata("llvm.dbg.defines");
    }
    if (Defines) {
      for (unsigned i = 0, e = Defines->getNumOperands(); i != e; ++i) {
        MDNode *node = Defines->getOperand(i);

        if ((node->getNumOperands() > 0) &&
          node->getOperand(0)->getMetadataID() == Metadata::MDStringKind) {
          MDString *str = cast<MDString>(node->getOperand(0));
          if (str->getString().find(kRootPathPrefix) != StringRef::npos) {
            m_rootPath = str->getString().substr(RootPrefixLength, str->getString().size() - RootPrefixLength);
          }
          // Standarize backslashes
          std::replace(m_rootPath.begin(), m_rootPath.end(), '\\', '/');
        }
      }
    }

    // Analyze the CFG for TraceRay call sites.
    analyzeCFG();
    // Format the output reports.
    formatOutput(PrettyStr, VSStr);

    PrettyStr.flush();
    VSStr.flush();

    // Optionally output the Pretty report to file.
    if (!m_outputFile.empty()) {

      // Standarize backslashes
      std::replace(m_outputFile.begin(), m_outputFile.end(), '\\', '/');
      std::ofstream outFile;
      outFile.exceptions(std::ofstream::failbit);
      try {
        outFile.open(m_outputFile);
        outFile << PrettyStr.str();
        outFile.close();
      }
      catch (...) {
        if (!outFile.eof()) {
          fprintf(
            stderr,
            "Error: Exception occurred while opening the Live Value output file %s\n",
            m_outputFile.c_str());
          return false;
        }
      }
    }

    // Echo Visual Studio (source linking) to VS debugger and console,
    // cut into chunks as it won't handle a massive string.
    const size_t outputSize = VSStr.str().length();
    const size_t blockSize = 1024;
    for (int i = 0; i < outputSize;) {
      OutputDebugStringA(VSStr.str().substr(i, blockSize).c_str());
      i += blockSize;
    }
  }

  // No changes.
  return false;
}

bool LiveValueAnalysis::isLoadRematerializable(CallInst *Call) {
  // A load intrinsic should be rematerializable if it comes from a CBV or SRV.
  CallInst* createHandle = dyn_cast<CallInst>(Call->getArgOperand(1));
  DXASSERT(createHandle, "Expected a valid CallInst.");
  DXASSERT(createHandle->getCalledFunction()->getName().startswith("dx.op.createHandle"), "Expected dx.op.createHandle opcode.");
  ConstantInt* resourceClass = dyn_cast<ConstantInt>(createHandle->getArgOperand(HLOperandIndex::kCreateHandleResourceOpIdx));

  if ((nullptr != resourceClass) && (resourceClass->getZExtValue() == (uint64_t)DXIL::ResourceClass::CBuffer
    || resourceClass->getZExtValue() == (uint64_t)DXIL::ResourceClass::SRV)) {
    return true;
  }
  else {
    // CreateHandle loading a global variable will be constant and suitable for remat.
    Value *Res = createHandle->getArgOperand(HLOperandIndex::kCreateHandleResourceOpIdx);
    if (LoadInst *LdRes = dyn_cast<LoadInst>(Res)) {
      if (GlobalVariable *GV = dyn_cast<GlobalVariable>(LdRes->getOperand(0)))
        return true;
    }   
  }

  return false;
}

// Rematerialization logic should imply basic to moderate optimizations wrt live values.
bool LiveValueAnalysis::canRemat(Instruction *I, CallInst *TraceCall) {
  
  if (isa<GetElementPtrInst>(I))
    return true;
  else if (isa<AllocaInst>(I))
    return true;

  if (CallInst *call = dyn_cast<CallInst>(I)) {
    if (dxilutil::IsLoadIntrinsic(call) && isLoadRematerializable(call))
      return true;
  }

  // Inspect the instruction in more detail to see if it is possible to rematerialize.
  if (dxilutil::IsRematerializable(I)) {
    return true;
  }

  // Inspect load instructions to see if they are used by subsequent load operations that could all be rematerialized.
  if (Instruction::Load == I->getOpcode())
  {
    // Global variables will be invariably constant and suitable for remat.
    if (GlobalVariable *GV = dyn_cast<GlobalVariable>(I->getOperand(0)))
      return true;
  }

  return false;
}

// Full path locations can be complicated and some projects have strange behavior. Customize accordingly.
std::string LiveValueAnalysis::formatSourceLocation(const std::string& Location, int LineNumber)
{
  std::string fullPath = "";

  // Only add the root path if one does not already exist for Location; assuming ':' exists in the path.
  if (Location.find(':') == std::string::npos)
    fullPath = m_rootPath;

  fullPath += Location + "(" + std::to_string(LineNumber) + ")";

  std::replace(fullPath.begin(), fullPath.end(), '\\', '/');

  return fullPath;
}

// Determine the max path and function lenghts for formatting text output.
void LiveValueAnalysis::getFormatLengths(SetVector<DILocation *> &UseLocations, uint32_t &maxFuncLength, uint32_t &maxPathLength)
{
  for (auto Loc : UseLocations) {
    std::string Path = formatSourceLocation(Loc->getFilename().str(), Loc->getLine());
    if (Path.length() > maxPathLength)
      maxPathLength = Path.length();
    if (Loc->getScope()->getSubprogram()->getName().str().length() > maxFuncLength)
      maxFuncLength = Loc->getScope()->getSubprogram()->getName().str().length();
    if (DILocation *InLoc = Loc->getInlinedAt()) {
      std::string Path = formatSourceLocation(InLoc->getFilename().str(), InLoc->getLine());
      if (Path.length() > maxPathLength)
        maxPathLength = Path.length();
      if (InLoc->getScope()->getSubprogram()->getName().str().length() > maxFuncLength)
        maxFuncLength = InLoc->getScope()->getSubprogram()->getName().str().length();
      while (DILocation *NestedInLoc = InLoc->getInlinedAt()) {
        InLoc = NestedInLoc;
        std::string Path = formatSourceLocation(InLoc->getFilename().str(), InLoc->getLine());
        if (Path.length() > maxPathLength)
          maxPathLength = Path.length();
        if (InLoc->getScope()->getSubprogram()->getName().str().length() > maxFuncLength)
          maxFuncLength = InLoc->getScope()->getSubprogram()->getName().str().length();
      }
    }
  }
}

// Determine the use locations for an instruction.
void LiveValueAnalysis::getUseLocationsForInstruction(Instruction *I, CallInst *CI, SetVector<DILocation *> &UseLocations)
{
  std::vector<PHINode *> Phis;
  // Track the phis we visit so that we do not duplicate them.
  SetVector<PHINode *> VisitedPhis;

  for (;;) {
    for (User *U : I->users()) {
      if (auto Inst = dyn_cast<Instruction>(U)) {
        if (DILocation *Loc = Inst->getDebugLoc()) {
          // Manually search for duplicate locations.
          // DILocations can be distinct but their source location is not.
          bool bFound = false;
          DIScope *scope = Loc->getScope();
          for (auto L : UseLocations) {
            if (L->getScope() == scope && L->getLine() == Loc->getLine() && L->getColumn() == Loc->getColumn()) {
              bFound = true;
              break;
            }
          }

          if (!bFound) {
            UseLocations.insert(Loc);
          }
        }
        else {
          // Check for a phi node that will need to be checked for use locations.
          if (isa<PHINode>(U)) {
            PHINode *NewPhi = dyn_cast<PHINode>(U);
            if (VisitedPhis.insert(NewPhi)) {
              Phis.push_back(NewPhi);
            }
          }
        }
      }
    }

    // Process any phis in the use list.
    if (Phis.empty()) {
      break;
    }
    else {
      I = Phis.back();
      Phis.pop_back();
    }
  }
}

void LiveValueAnalysis::processBranch(TerminatorInst *TI,
  SetVector<Value *> &ConditionsKnownFalse,
  SetVector<Value *> &ConditionsKnownTrue,
  SetVector<std::pair<Instruction *, Instruction *>> &VisitedEdges,
  SetVector<Instruction *> &DefsSeen,
  std::vector<std::tuple<BasicBlock *, Instruction *, SetVector<Instruction *>>> &Worklist)
{
  Instruction *inst = dyn_cast<Instruction>(TI);
  BranchInst *branch = dyn_cast<BranchInst>(TI);

  if (branch && branch->isConditional()) {
    // Conditional branch. Check against known conditions.
    DXASSERT(branch->getNumSuccessors() == 2, "Expected two successors.");

    Instruction *IfTrue = branch->getSuccessor(0)->begin();
    Instruction *IfFalse = branch->getSuccessor(1)->begin();

    std::pair<Instruction *, Instruction *> trueEdge = std::make_pair(branch, IfTrue);
    std::pair<Instruction *, Instruction *> falseEdge = std::make_pair(branch, IfFalse);

    if (!ConditionsKnownFalse.count(branch->getCondition())) {
      if (VisitedEdges.insert(trueEdge)) {
        Worklist.push_back(std::make_tuple(inst->getParent(), IfTrue, DefsSeen));
      }
    }

    if (!ConditionsKnownTrue.count(branch->getCondition())) {
      if (VisitedEdges.insert(falseEdge)) {
        Worklist.push_back(std::make_tuple(inst->getParent(), IfFalse, DefsSeen));
      }
    }
  }
  else {
    for (unsigned i = 0, e = TI->getNumSuccessors(); i < e; ++i) {
      Instruction *successor = TI->getSuccessor(i)->begin();
      std::pair<Instruction *, Instruction *> edge = std::make_pair(inst, successor);
      if (VisitedEdges.insert(edge)) {
        Worklist.push_back(std::make_tuple(inst->getParent(), successor, DefsSeen));
      }
    }
  }
}

void LiveValueAnalysis::getUseDefinitions(CallInst *CI, SetVector<Instruction *> &LiveValues, SetVector<Value *> &ConditionsKnownFalse, SetVector<Value *> &ConditionsKnownTrue)
{
  // Traverse each control flow edge separately and gather up seen defs along
  // the way. By keeping track of the from-block, we can select the right
  // incoming value at PHIs.
  Instruction *inst = CI->getNextNode();

  BasicBlock *fromBlock = nullptr;
  // Definitions seen.
  SetVector<Instruction *> defsSeen;
  SetVector<std::pair<Instruction *, Instruction *>> visitedEdges;
  // Worklist of each block, entrypoint, and definitions that block has seen.
  std::vector<std::tuple<BasicBlock *, Instruction *, SetVector<Instruction *>>> worklist;

  defsSeen.insert(CI);

  for (;;) {
    if (PHINode *phi = dyn_cast<PHINode>(inst)) {
      for (unsigned i = 0; i < phi->getNumIncomingValues(); ++i) {

        // Skip this phi value if it does not match the block we came from.
        if (phi->getIncomingBlock(i) != fromBlock)
          continue;

        Instruction *use_inst = dyn_cast<Instruction>(phi->getIncomingValue(i));
        if (!use_inst)
          continue;

        if (!defsSeen.count(use_inst)) {
          // Use without a preceding def must be a live value.
          LiveValues.insert(use_inst);
        }
      }
    }
    else {
      for (unsigned i = 0; i < inst->getNumOperands(); ++i) {
        Instruction *use_inst = dyn_cast<Instruction>(inst->getOperand(i));
        if (!use_inst)
          continue;

        if (!defsSeen.count(use_inst)) {
          // Use without a preceding def must be a live value.
          LiveValues.insert(use_inst);
        }
      }
    }

    defsSeen.insert(inst);

    // Don't count instructions past another call site.
    // They will initiate spilling as needed independently.
    if (isa<CallInst>(inst) && m_callSites.count(cast<CallInst>(inst))) {
      if (worklist.empty())
        break;

      fromBlock = std::get<0>(worklist.back());
      inst = std::get<1>(worklist.back());
      defsSeen = std::move(std::get<2>(worklist.back()));
      worklist.pop_back();
      continue;
    }

    if (TerminatorInst *terminator = dyn_cast<TerminatorInst>(inst)) {
      // Add the branch to this worklist.
      processBranch(terminator, ConditionsKnownFalse, ConditionsKnownTrue, visitedEdges, defsSeen, worklist);

      if (worklist.empty())
        break;

      // Process the next block in the worklist.
      fromBlock = std::get<0>(worklist.back());
      inst = std::get<1>(worklist.back());
      defsSeen = std::move(std::get<2>(worklist.back()));
      worklist.pop_back();
      continue;
    }
    // Process the next instruction.
    inst = inst->getNextNode();
  }

}

void LiveValueAnalysis::analyzeLiveValues(CallInst *TraceCall, SetVector<Instruction *> &LiveValues, SetVector<std::pair<Instruction *, Instruction *>> &PredCfgEdges) {

  // Determine CFG edges that can lead to this call site.
  BasicBlock *BB = TraceCall->getParent();
  SmallVector<BasicBlock*, 16> worklist;
  worklist.push_back(BB);

  while (!worklist.empty()) {
    BB = worklist.pop_back_val();
    for (auto i = llvm::pred_begin(BB); i != pred_end(BB); ++i) {
      BasicBlock *pred = *i;
      std::pair<Instruction *, Instruction *> edge = std::make_pair(pred->getTerminator(), BB->begin());

      if (PredCfgEdges.insert(edge)) {
        worklist.push_back(pred);
      }
    }
  }

  // Determine known branch conditions based on CFG edges.
  // Prunes pred CFG edges based on invariants iteratively.
  SetVector<Value *> conditionsKnownTrue;
  SetVector<Value *> conditionsKnownFalse;

  for (;;) {
    bool change = false;
    for (std::pair<Instruction *, Instruction *> edge : PredCfgEdges) {
      BranchInst *branch = dyn_cast<BranchInst>(edge.first);

      // TODO: Extend this to switch statements?

      // Skip if there is no branch to consider.
      if (!branch || !branch->isConditional())
        continue;

      // We always expect two successors to process.
      DXASSERT(branch->getNumSuccessors() == 2, "Expected two successors.");

      Instruction *IfTrue = branch->getSuccessor(0)->begin();
      Instruction *IfFalse = branch->getSuccessor(1)->begin();

      std::pair<Instruction *, Instruction *> trueEdge = std::make_pair(edge.first, IfTrue);
      std::pair<Instruction *, Instruction *> falseEdge = std::make_pair(edge.first, IfFalse);

      int count = PredCfgEdges.count(trueEdge) + PredCfgEdges.count(falseEdge);

      if (!count)
        continue;

      if (count == 2) {
        if (conditionsKnownTrue.count(branch->getCondition())) {
          PredCfgEdges.remove(falseEdge);
          change = true;
        }
        if (conditionsKnownFalse.count(branch->getCondition())) {
          PredCfgEdges.remove(trueEdge);
          change = true;
        }
        continue;
      }

      if (PredCfgEdges.count(trueEdge)) {
        if (conditionsKnownTrue.insert(branch->getCondition())) {
          change = true;
        }
      }
      else {
        if (conditionsKnownFalse.insert(branch->getCondition())) {
          change = true;
        }
      }
    }

    std::vector<PHINode *> phiConditions;

    for (Value *condition : conditionsKnownTrue) {
      if (PHINode *phi = dyn_cast<PHINode>(condition))
        phiConditions.push_back(phi);
    }

    for (Value *condition : conditionsKnownFalse) {
      if (PHINode *phi = dyn_cast<PHINode>(condition))
        phiConditions.push_back(phi);
    }

    // Propagate PHI conditions to incoming if only a single incoming.
    for (PHINode *phi : phiConditions) {
      unsigned numIncoming = 0;
      Value *incomingValue = nullptr;

      for (unsigned i = 0; i < phi->getNumIncomingValues(); ++i) {
        std::pair<Instruction *, Instruction *> edge = std::make_pair(phi->getIncomingBlock(i)->getTerminator(), phi->getParent()->begin());

        // This was defined before the trace call.
        // So we can check PredCfgEdges for valid incoming values.
        if (PredCfgEdges.count(edge)) {
          ++numIncoming;
          incomingValue = phi->getIncomingValue(i);
        }
      }

      DXASSERT(numIncoming, "Expected greater than 0 incoming values.");

      // If there is only one incoming phi condition we know it must be taken.
      if (numIncoming == 1) {
        if (conditionsKnownFalse.count(incomingValue) == conditionsKnownFalse.count(phi) &&
          conditionsKnownTrue.count(incomingValue) == conditionsKnownTrue.count(phi))
          continue;


        change = true;

        if (conditionsKnownTrue.count(phi))
          conditionsKnownTrue.insert(incomingValue);
        else
          conditionsKnownFalse.insert(incomingValue);
      }
    }
#if !defined(NDEBUG)
    for (Value *condition : conditionsKnownTrue)
      DXASSERT(!conditionsKnownFalse.count(condition), "Expected no false conditions from a true condition.");

    for (Value *condition : conditionsKnownFalse)
      DXASSERT(!conditionsKnownTrue.count(condition), "Expected no true conditions from a false condition.");
#endif

    if (!change)
      break;
  }

  getUseDefinitions(TraceCall, LiveValues, conditionsKnownFalse, conditionsKnownTrue);

}

void LiveValueAnalysis::determineRematSpillSet(CallInst *TraceCall, SetVector<Instruction *> &RematSet, SetVector<Instruction *> &SpillSet, SetVector<Instruction *> &LiveValues, SetVector<std::pair<Instruction *, Instruction *>> &PredCfgEdges) {

  std::vector<Instruction *> worklist;
  SetVector<Instruction *> visited;

  for (Instruction *inst : LiveValues) {
    visited.insert(inst);
    worklist.push_back(inst);
  }

  while (!worklist.empty()) {
    Instruction *inst = worklist.back();
    worklist.pop_back();

    if (!canRemat(inst, TraceCall)) {
      SpillSet.insert(inst);
      continue;
    }

    RematSet.insert(inst);

    if (PHINode *phi = dyn_cast<PHINode>(inst)) {
      for (unsigned i = 0; i < phi->getNumIncomingValues(); ++i) {
        std::pair<Instruction *, Instruction *> edge = std::make_pair(phi->getIncomingBlock(i)->getTerminator(), phi->getParent()->begin());

        // This is a live value, i.e., something that was defined BEFORE the
        // trace call. So we can check PredCfgEdges for valid incoming
        // values.
        if (PredCfgEdges.count(edge)) {
          Instruction *use_inst = dyn_cast<Instruction>(phi->getIncomingValue(i));
          if (use_inst && !visited.count(use_inst)) {
            visited.insert(use_inst);
            worklist.push_back(use_inst);
          }

          BranchInst *branch = dyn_cast<BranchInst>(phi->getIncomingBlock(i)->getTerminator());
          if (branch && branch->isConditional() &&
            isa<Instruction>(branch->getCondition()) &&
            !visited.count(cast<Instruction>(branch->getCondition()))) {
            visited.insert(cast<Instruction>(branch->getCondition()));
            worklist.push_back(cast<Instruction>(branch->getCondition()));
          }
        }
      }
    }
    else {
      for (unsigned i = 0; i < inst->getNumOperands(); ++i) {
        Instruction *use_inst = dyn_cast<Instruction>(inst->getOperand(i));
        if (!use_inst)
          continue;

        if (!visited.count(use_inst)) {
          visited.insert(use_inst);
          worklist.push_back(use_inst);
        }
      }
    }
  }
}

void LiveValueAnalysis::determinePhisSpillSet(std::vector<PHINode *> &Phis, SetVector<Instruction *> &RematSet, SetVector<Instruction *> &SpillSet) {

  for (Instruction *inst : RematSet) {
    if (PHINode *phi = dyn_cast<PHINode>(inst))
      Phis.push_back(phi);
  }

  if (Phis.empty())
    return;

  for (PHINode *phi : Phis) {
    bool loopWithoutSpill = false;

    std::vector<Instruction *> worklist;
    SetVector<Instruction *> visited;

    bool first = true;
    worklist.push_back(phi);

    while (!worklist.empty()) {
      Instruction *inst = worklist.back();
      worklist.pop_back();

      if (first) {
        first = false;
      }
      else {
        if (inst == phi) {
          loopWithoutSpill = true;
          continue;
        }
      }

      if (PHINode *phi = dyn_cast<PHINode>(inst)) {
        for (unsigned i = 0; i < phi->getNumIncomingValues(); ++i) {
          BranchInst *branch = dyn_cast<BranchInst>(phi->getIncomingBlock(i)->getTerminator());
          if (!branch || !branch->isConditional())
            continue;

          Instruction *condition = dyn_cast<Instruction>(branch->getCondition());

          if (condition && !visited.count(condition)) {
            visited.insert(condition);

            if (RematSet.count(condition))
              worklist.push_back(condition);
          }
        }
      }

      for (unsigned i = 0; i < inst->getNumOperands(); ++i) {
        Instruction *use_inst = dyn_cast<Instruction>(inst->getOperand(i));
        if (!use_inst)
          continue;

        if (!SpillSet.count(use_inst) && !RematSet.count(use_inst))
          continue;

        if (!visited.count(use_inst)) {
          visited.insert(use_inst);

          if (RematSet.count(use_inst))
            worklist.push_back(use_inst);
        }
      }
    }

    if (loopWithoutSpill) {
      RematSet.remove(phi);
      SpillSet.insert(phi);
    }
  }
}

void LiveValueAnalysis::analyzeLiveValueOptimizations(CallInst *TraceCall, SetVector<Instruction *> &LiveValues, SetVector<std::pair<Instruction *, Instruction *>> &PredCfgEdges) {

  // Determine initial sets of remat/spill.
  SetVector<Instruction *> rematSet;
  SetVector<Instruction *> spillSet;
  determineRematSpillSet(TraceCall, rematSet, spillSet, LiveValues, PredCfgEdges);

  // Move looped PHIs without intervening spill to spill set.
  std::vector<PHINode *> phis;
  determinePhisSpillSet(phis, rematSet, spillSet);

  // Backtrack live values to non-remat instructions.
  SetVector<Instruction *> nonRematLiveValues;
  {
    SetVector<Instruction *> visited(LiveValues.begin(), LiveValues.end());
    std::vector<Instruction *> worklist;
    worklist.insert(worklist.end(), LiveValues.begin(), LiveValues.end());

    while (!worklist.empty()) {
      Instruction *inst = worklist.back();
      worklist.pop_back();

      DXASSERT(rematSet.count(inst) + spillSet.count(inst) == 1, "Expected an instruction to exist in either the remat or spill set, but not both or none.");

      if (spillSet.count(inst)) {
        nonRematLiveValues.insert(inst);
        continue;
      }

      if (PHINode *phi = dyn_cast<PHINode>(inst)) {
        // We can remat PHI if we can reconstruct which BB we came from.
        // This is a live value, i.e., something that was defined BEFORE the
        // trace call. So we can check PredCfgEdges.
        unsigned numValidEdges = 0;
        for (unsigned i = 0; i < phi->getNumIncomingValues(); ++i) {
          std::pair<Instruction *, Instruction *> edge = std::make_pair(phi->getIncomingBlock(i)->getTerminator(), phi->getParent()->begin());

          if (PredCfgEdges.count(edge)) {
            ++numValidEdges;
          }
        }

        DXASSERT(numValidEdges, "Expected greater than 0 valid edges.");

        if (numValidEdges == 1) {
          // Only a single valid edge. The PHI must have one value.
          for (unsigned i = 0; i < phi->getNumIncomingValues(); ++i) {
            std::pair<Instruction *, Instruction *> edge = std::make_pair(phi->getIncomingBlock(i)->getTerminator(), phi->getParent()->begin());

            if (PredCfgEdges.count(edge)) {
              Instruction *use_inst = dyn_cast<Instruction>(phi->getIncomingValue(i));
              if (!use_inst)
                continue;

              if (!visited.count(use_inst)) {
                visited.insert(use_inst);
                worklist.push_back(use_inst);
              }
            }
          }
          continue;
        }

        // Add dependencies to work list.
        for (unsigned i = 0; i < phi->getNumIncomingValues(); ++i) {
          std::pair<Instruction *, Instruction *> edge = std::make_pair(phi->getIncomingBlock(i)->getTerminator(), phi->getParent()->begin());

          if (PredCfgEdges.count(edge)) {
            Instruction *use_inst = dyn_cast<Instruction>(phi->getIncomingValue(i));
            if (!use_inst)
              continue;

            if (visited.count(use_inst))
              continue;

            visited.insert(use_inst);
            worklist.push_back(use_inst);
          }
        }
      }
      else {
        for (unsigned i = 0; i < inst->getNumOperands(); ++i) {
          Instruction *use_inst = dyn_cast<Instruction>(inst->getOperand(i));
          if (!use_inst)
            continue;

          if (visited.count(use_inst))
            continue;

          visited.insert(use_inst);
          worklist.push_back(use_inst);
        }
      }
    }
  }

  // Store the spills.
  m_spillsPerTraceCall[TraceCall].insert(m_spillsPerTraceCall[TraceCall].end(), nonRematLiveValues.begin(), nonRematLiveValues.end());
}

void LiveValueAnalysis::analyzeCFG() {

  // Prepare Debug Info.
  DebugInfoFinder DIF;
  DIF.processModule(*m_module);

  // Find TraceRay call sites.
  for (Function &main : *m_module) {
    for (Function &F : *m_module) {

      if (!F.getName().startswith(kTraceRayOpName))
        continue;

      for (User *user : F.users()) {
        CallInst *call = dyn_cast<CallInst>(user);
        if (call && call->getParent()->getParent() == &main)
          m_callSites.insert(call);
      }
    }
  }

  for (CallInst *CI : m_callSites) {
    // Analyze live values and consider optimziations that could reasonably remove those live values further down the compilation path.
    // Collect the live values and a vector of CFG edges that lead to potential live values.
    SetVector<Instruction *> liveValues;
    SetVector<std::pair<Instruction *, Instruction *>> predCfgEdges;
    analyzeLiveValues(CI, liveValues, predCfgEdges);
    analyzeLiveValueOptimizations(CI, liveValues, predCfgEdges);
  }
}

bool isNumber(const std::string &str) {
  for (int i = 0; i < str.length(); i++)
    if (!isdigit(str[i]))
      return false;
  return true;
}

void LiveValueAnalysis::determineValueName(DIType *diType, int64_t &BitOffset, std::string &Name)
{
  // Find the data member name of a derived type matching the bit offset.
  // Note that we are not interested in DIBasicType.
  if (auto* CT = dyn_cast<DICompositeType>(diType))
  {
    switch (diType->getTag())
    {
      // TODO: Not sure if arrays are handled properly.
    case dwarf::DW_TAG_array_type:
    {
      DINodeArray elements = CT->getElements();
      unsigned arraySize = 1;
      for (auto const& node : elements)
      {
        if (DISubrange* SR = dyn_cast<DISubrange>(node))
        {
          arraySize *= SR->getCount();
        }
      }
      const DITypeIdentifierMap EmptyMap;
      DIType *BT = CT->getBaseType().resolve(EmptyMap);
      for (unsigned i = 0; i < arraySize; ++i) {
        determineValueName(BT, BitOffset, Name);
        // Found the element matching the BitOffset.
        if (BitOffset < 0)
          break;
      }
    }
    break;
    case dwarf::DW_TAG_class_type:
    case dwarf::DW_TAG_structure_type:
      // Search each element.
      for (auto const& node : CT->getElements())
      {
        if (DIType* subType = dyn_cast<DIType>(node))
        {
          determineValueName(subType, BitOffset, Name);
        }
        // Found the element matching the BitOffset.
        if (BitOffset < 0)
          break;
      }
      break;
    default:

      break;
    }
  }
  else if (auto *DT = dyn_cast<DIDerivedType>(diType))
  {
    int64_t size = (int64_t)DT->getSizeInBits();
    DIType *SubTy = nullptr;
    DICompositeType *CompTy = nullptr;

    // If size is 0 check the base type for a Composite type.
    if (size == 0)
    {
      CompTy = dyn_cast_or_null<DICompositeType>(DT->getRawBaseType());
      while (DT = dyn_cast_or_null<DIDerivedType>(DT->getRawBaseType()))
      {
        CompTy = dyn_cast_or_null<DICompositeType>(DT->getRawBaseType());
      }
      if (CompTy)
      {
        // Traverse inside this composite type, using '.' for the new member name.
        determineValueName(CompTy, BitOffset, Name);
      }
    }
    // The derived type has been found when its size is within the bit offset. Potentially a composite type which will need to be traversed.
    else if (((BitOffset - size) < 0))
    {
      Name += DT->getName();
      while (DT = dyn_cast_or_null<DIDerivedType>(DT->getRawBaseType()))
      {
        SubTy = dyn_cast_or_null<DIType>(DT->getRawBaseType());
      }
      if (SubTy && isa<DICompositeType>(SubTy))
      {
        // Traverse inside this composite type, using '.' for the new member name.
        Name += ".";
        determineValueName(SubTy, BitOffset, Name);
      }
    }
    // Deduct the size from the bit offset and keep traversing.
    BitOffset -= size;
  }
}

std::string LiveValueAnalysis::translateValueToName(DbgValueInst *DVI, Instruction *I) {

  std::ostringstream NameStr;
  DILocalVariable *Var = DVI->getVariable();
  DILocation *Loc = DVI->getDebugLoc();
  Metadata *VarTypeMD = Var->getRawType();
  std::string VarName = Var->getName();
  std::string OrigName = I->getName();
  std::string ElementName = "";
  int64_t offset = 0;
  
  if (isa<ExtractValueInst>(I)) {
    I = dyn_cast<Instruction>(dyn_cast<ExtractValueInst>(I)->getAggregateOperand());
  }

  DIExpression *Expr = dyn_cast_or_null<DIExpression>(DVI->getRawExpression());
  if (Expr && Expr->isBitPiece()) {
    // Use the bit piece offset to determine the member data and its corresponding name.
    offset = Expr->getBitPieceOffset();
  }

  if (DIType *DITy = dyn_cast<DIType>(VarTypeMD))
    determineValueName(DITy, offset, ElementName);

  NameStr << VarName;
  // Append the element name if it exists.
  if (ElementName.length() > 0)
    NameStr << "." << ElementName;

  // Determine the type of this variable.
  DIType *VarType = dyn_cast_or_null<DIDerivedType>(VarTypeMD);
  if (isa<DIDerivedType>(VarTypeMD)) {
    // Get the raw type of a derived type.
    VarType = dyn_cast_or_null<DIType>(dyn_cast<DIDerivedType>(VarType)->getRawBaseType());
    DXASSERT(VarType, "Expected a valid DIType.");
    // Recurse through references to find the type.
    while (llvm::dwarf::DW_TAG_reference_type == VarType->getTag()) {
      VarType = dyn_cast_or_null<DIType>(dyn_cast<DIDerivedType>(VarType)->getRawBaseType());
    }
  }
  else if (isa<DICompositeType>(VarTypeMD)) {
    VarType = dyn_cast<DICompositeType>(VarTypeMD);
  }
  else if (isa<DIBasicType>(VarTypeMD))
    VarType = dyn_cast<DIBasicType>(VarTypeMD);

  DXASSERT(VarType, "Expected a valid DIType.");

  NameStr << " (" << VarType->getName().str() << "): " << formatSourceLocation(Loc->getFilename().str(), Loc->getLine()) << "\n";

  return NameStr.str();
}

void LiveValueAnalysis::outputSourceLocationsPretty(SetVector<DILocation *> &UseLocations, uint32_t maxPathLength, uint32_t maxFuncLength, raw_string_ostream &PrettyStr)
{
  std::ostringstream LocationStr;
  std::string FileName;
  std::string FuncName;

  // Output PrettyPrint version
  for (auto Loc : UseLocations) {
    FileName = formatSourceLocation(Loc->getFilename().str(), Loc->getLine());
    FuncName = "(" + Loc->getScope()->getSubprogram()->getName().str() + ")";
    LocationStr << std::setfill('.') << std::setw(maxPathLength) << std::left << FileName
      << std::setw(maxFuncLength) << std::right << FuncName << "\n";
    if (DILocation *InLoc = Loc->getInlinedAt()) {
      FileName = inlinePrefix + formatSourceLocation(InLoc->getFilename().str(), InLoc->getLine());
      FuncName = "(" + InLoc->getScope()->getSubprogram()->getName().str() + ")";
      LocationStr << std::setfill('.') << std::setw(maxPathLength) << std::left << FileName << std::setw(maxFuncLength) << std::right << FuncName << "\n";
      while (DILocation *NestedInLoc = InLoc->getInlinedAt()) {
        FileName = inlinePrefix + formatSourceLocation(NestedInLoc->getFilename().str(), NestedInLoc->getLine());
        FuncName = "(" + NestedInLoc->getScope()->getSubprogram()->getName().str() + ")";
        LocationStr << std::setfill('.') << std::setw(maxPathLength) << std::left << FileName << std::setw(maxFuncLength) << std::right << FuncName << "\n";
        InLoc = NestedInLoc;
      }
    }
  }
  PrettyStr << LocationStr.str();
}

void LiveValueAnalysis::outputSourceLocationsVS(SetVector<DILocation *> &UseLocations, uint32_t maxPathLength, uint32_t maxFuncLength, raw_string_ostream &VSStr)
{
  std::ostringstream LocationStr;
  std::string FileName;
  std::string FuncName;

  // Output VS source linking version
  for (auto Loc : UseLocations) {
    FileName = formatSourceLocation(Loc->getFilename().str(), Loc->getLine());
    LocationStr << FileName << ": (" + Loc->getScope()->getSubprogram()->getName().str() + ")\n";
    if (DILocation *InLoc = Loc->getInlinedAt()) {
      LocationStr << "inlined at:\n";
      FileName = formatSourceLocation(InLoc->getFilename().str(), InLoc->getLine());
      LocationStr << ">" + FileName << ": (" + InLoc->getScope()->getSubprogram()->getName().str() + ")\n";
      while (DILocation *NestedInLoc = InLoc->getInlinedAt()) {
        FileName = formatSourceLocation(NestedInLoc->getFilename().str(), NestedInLoc->getLine());
        LocationStr << ">" + FileName << ": (" + InLoc->getScope()->getSubprogram()->getName().str() + ")\n";
        InLoc = NestedInLoc;
      }
    }
  }
  VSStr << LocationStr.str();
}

void LiveValueAnalysis::formatOutput(raw_string_ostream &PrettyStr, raw_string_ostream &VSStr) {

  const char callsiteTitle[] = "Live State for TraceRay call at ";
  const char reportTitle[] = "Live State Summary";
  std::string HeaderText;
  std::string fullPath;
  raw_string_ostream TmpStr(HeaderText);

  TmpStr << "===========================================\n";
  TmpStr << "== TRACE RAY CALL SITES WITH LIVE VALUES ==\n";
  TmpStr << "===========================================\n";

  for (CallInst *callSite : m_callSites) {
    if (const llvm::DebugLoc &debugInfo = callSite->getDebugLoc()) {
      // Prepare a map of the live value instructions and their associated value names.
      for (Instruction *I : (m_spillsPerTraceCall[callSite])) {
        if (isa<ExtractValueInst>(I)) {
          I = dyn_cast<Instruction>(dyn_cast<ExtractValueInst>(I)->getAggregateOperand());
        }

        // Add to the live values list even if it's not used by metadata so that we can track more accurately. Only filter out artifical values.
        LiveValueInst LVI = { "", I };

        if (I->isUsedByMetadata()) {
          if (auto *L = LocalAsMetadata::getIfExists(I)) {
            if (auto *MDV = MetadataAsValue::getIfExists(I->getContext(), L)) {
              for (User *U : MDV->users()) {
                if (DbgValueInst *DVI = dyn_cast<DbgValueInst>(U)) {
                  // Add the value's name since we have found the metadata.
                  LVI.Name = translateValueToName(DVI, I);
                  // Filter out the artifical variables (should be benign?)
                  if (!DVI->getVariable()->isArtificial())
                  {
                    m_liveValuesPerTraceCall[callSite].push_back(LVI);
                  }
                  // Found the debug info for the live value, now exit the loop.
                  break;
                }
              }
            }
          }
        }
        else
        {
          bool foundMetadata = false;
          // If this instruction has no metadata check its users.
          for (User *U : I->users()) {
            if (Instruction *UserI = dyn_cast<Instruction>(U)) {
              if (UserI->isUsedByMetadata()) {
                if (auto *L = LocalAsMetadata::getIfExists(UserI)) {
                  if (auto *MDV = MetadataAsValue::getIfExists(UserI->getContext(), L)) {
                    for (User *MU : MDV->users()) {
                      if (DbgValueInst *DVI = dyn_cast<DbgValueInst>(MU)) {
                        // Add the value's name since we have found the metadata.
                        LVI.Name = translateValueToName(DVI, UserI);
                        // Filter out the artifical variables (should be benign?)
                        if (!DVI->getVariable()->isArtificial())
                        {
                          foundMetadata = true;
                          LVI.Inst = UserI;
                          m_liveValuesPerTraceCall[callSite].push_back(LVI);
                          // We just want the first non-artificial user with metadata since this
                          // should be closest to the original live value so we can break now.
                          break;
                        }
                      }
                    }
                  }
                }
              }
            }
          }
          if (!foundMetadata) {
            // If not used by metadata we still want to track this potential live value, even if we cannot determine its name.
            m_liveValuesPerTraceCall[callSite].push_back(LVI);
          }
        }
      }
      std::string filePath = debugInfo->getFilename();
      int line = debugInfo->getLine();
      fullPath = formatSourceLocation(filePath, line);
      TmpStr << fullPath << " incurs " << m_liveValuesPerTraceCall[callSite].size() << " 32-bit registers\n";

      // Sort the order of live values per call site by name.
      std::sort(m_liveValuesPerTraceCall[callSite].begin(), m_liveValuesPerTraceCall[callSite].end(), LiveValueCompare());
    }
  }
  TmpStr << "\n";

  // Copy to PrettyStr VSStr.
  VSStr << TmpStr.str();
  PrettyStr << TmpStr.str();

  for (CallInst *callSite : (m_callSites)) {
    std::ostringstream Spacer;
    TmpStr.str().clear();

    if (const llvm::DebugLoc &debugInfo = callSite->getDebugLoc()) {
      std::string filePath = debugInfo->getFilename();
      int line = debugInfo->getLine();
      fullPath = formatSourceLocation(filePath, line);
      Spacer << std::setfill('=') << std::setw(fullPath.length() + sizeof(callsiteTitle)) << std::right << "\n";
      TmpStr << Spacer.str();
      TmpStr << callsiteTitle << fullPath << "\n";
      TmpStr << Spacer.str();
    }
    else {
      break;
    }
    size_t regs = 0;
    // Test to help detect missing metadata for potential live values.
    // Only count the values for which metadata exists.
    // TODO: Report count of values for those which are not used metadata?
    // this can lead to missing live values in some cases.
    std::vector<Instruction*> DetectedInstr; // Detected instructions, but with no user metadata.
    for (LiveValueInst &LVI : m_liveValuesPerTraceCall[callSite]) {
      Instruction *I = LVI.Inst;
      if (I->isUsedByMetadata()) {
        // Live value that has valid metadata.
        regs++;
      }
      else {
        DetectedInstr.push_back(I);
      }
    }
    
    if (DetectedInstr.size())
    {
      TmpStr << "** DEBUG ************\n";
      TmpStr << "Detected " << (int)DetectedInstr.size() + (int)regs << " live values but only " << (int)regs << " are used by metadata\n";
      for (auto I : DetectedInstr) {
        I->print(TmpStr);
        TmpStr << "\n";
      }
      TmpStr << "*********************\n";
    }

    if (const llvm::DebugLoc &debugInfo = callSite->getDebugLoc()) {
      TmpStr << "Total 32-bit registers: " << (int)regs << "\n";
    }

    // Create a summary of the Live Values for this TraceRay call.
    TmpStr << "\n== LIVE VALUES SUMMARY ==\n";
    for (LiveValueInst &LVI : m_liveValuesPerTraceCall[callSite]) {
      TmpStr << LVI.Name;
    }

    // Create the Details title.
    TmpStr << "\n== DETAILS ==\n";
    // Copy to both output streams.
    VSStr << TmpStr.str();
    PrettyStr << TmpStr.str();

    for (LiveValueInst &LVI : (m_liveValuesPerTraceCall[callSite])) {
      Instruction *I = LVI.Inst;
      TmpStr.str().clear();

      // Find the metadata for the instruction, value, use locations, etc.
      if (I->isUsedByMetadata()) {
        if (auto *L = LocalAsMetadata::getIfExists(I)) {
          if (auto *MDV = MetadataAsValue::getIfExists(I->getContext(), L)) {
            for (User *U : MDV->users()) {
              if (DbgValueInst *DVI = dyn_cast<DbgValueInst>(U)) {
                
                if (DVI->getVariable()->isArtificial())
                  break;

                // Determine the name of this value.
                TmpStr << LVI.Name;

                // Get use locations.
                SetVector<DILocation *> UseLocations;
                getUseLocationsForInstruction(I, callSite, UseLocations);

                TmpStr << "Use Locations:\n";
                PrettyStr << TmpStr.str();
                // Copy the PrettyStr to the VSStr as the output diverges on the Use Locations formatting.
                // The VS text stream requires a very rigid format to support source lookup via mouse clicks in the debug console.
                VSStr << TmpStr.str();

                // Find max path length to format horizontal dividers in the output.
                uint32_t maxPathLength = 0;
                uint32_t maxFuncLength = 0;
                getFormatLengths(UseLocations, maxFuncLength, maxPathLength);

                // Pad with spaces for inline text, brackets, etc. in the prettyprint output formatting.
                maxFuncLength += 4;
                maxPathLength += sizeof(inlinePrefix);
                // Output use locations
                outputSourceLocationsPretty(UseLocations, maxPathLength, maxFuncLength, PrettyStr);
                outputSourceLocationsVS(UseLocations, maxPathLength, maxFuncLength, VSStr);
              }
              // Found the debug info for the live value, now exit the loop because any subsquent users only reveal duplicate information.
              PrettyStr << "\n";
              VSStr << "\n";
              break;
            }
          }
        }
      }
    }
  }
}
