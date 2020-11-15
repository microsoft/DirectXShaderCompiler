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
#include "dxc/DXIL/DxilMetadataHelper.h"
#include "dxc/DXIL/DxilModule.h"
#include "dxc/DXIL/DxilOperations.h"
#include "dxc/DXIL/DxilUtil.h"
#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_os_ostream.h"
#include "dxc/Support/Global.h"

#include <algorithm>
#include <string>
#include <stack>
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

    struct TraceRayCallSite {
      // Trace ray call sites can have identical source locations, but have distinct code flow based on how the shaders call them.
      // For example having macros and/or helper functions that take different code paths, but eventually call the exact same
      // trace call.  This is hard to represent easily to the user so call sites can be numbered and store string information
      // displying the call stack to reach the trace ray call.
      // ie:   [shader("raygeneration")]
      //                  |- ...
      //                  |-helper func1
      //                  |       -> trace ray
      //                  |-funcA
      //                  |       -> helper func2
      //                  |               -> trace ray
      //                  |- ...
      std::string CallStack; // Full call stack leading to the TraceRay call.
      CallInst *CI; // The call instruction for trace ray; used for mapping live values to distinct call instructions.
    };

    // Sort live values by name.
    struct LiveValueCompare {
      inline bool operator() (LiveValueInst &lhs, LiveValueInst &rhs) {
        // Case insensitive compare.
#ifdef _WIN32
        const auto result = std::mismatch(lhs.Name.cbegin(), lhs.Name.cend(), rhs.Name.cbegin(), rhs.Name.cend(),
          [](const unsigned char lhs, const unsigned char rhs) {return tolower(lhs) == tolower(rhs); });
        return result.second != rhs.Name.cend() && (result.first == lhs.Name.cend() || tolower(*result.first) < tolower(*result.second));
#else
        const size_t lhs_size = lhs.Name.length();
        const size_t rhs_size = rhs.Name.length();
        for (size_t i = 0; i < lhs_size, i < rhs_size; ++i)
        {
          if (tolower(rhs.Name[i]) == tolower(lhs.Name[i]))
            continue;
          if (tolower(rhs.Name[i]) < tolower(lhs.Name[i]))
            return false;
          else
            return true;
        }
        return true;
#endif // _WIN32
      }
    };

    Module* m_module = nullptr;
    DxilModule* m_DM = nullptr;
    // Set of trace ray call sites.
    std::vector<TraceRayCallSite> m_callSites;
    // Potential live values per trace call.
    MapVector<CallInst *, std::vector<Instruction *>> m_spillsPerTraceCall;
    // Sorted list of live values per trace call, with name information.
    MapVector<CallInst *, std::vector<LiveValueInst>> m_liveValuesPerTraceCall;
    // Root path to use for source location information.
    static std::string m_rootPath;
    // Output file name for the 'pretty' report.
    std::string m_outputFile;

  public:
    LiveValueAnalysis(StringRef LiveValueAnalysisOutputFile = "");
    ~LiveValueAnalysis() override;
    static char ID;

    bool runOnModule(Module &) override;

    void addUniqueLocation(DILocation *L, SetVector<DILocation *> &Locations);
    void analyzeCFG();
    void analyzeLiveValues(CallInst *TraceCall, SetVector<Instruction *> &LiveValues, SetVector<std::pair<Instruction *, Instruction *>> &PredCfgEdges);
    bool callSiteExists(CallInst *CI);
    bool canRemat(Instruction *Inst);
    void determineRematSpillSet(CallInst *TraceCall,
      SetVector<Instruction *> &RematSet,
      SetVector<Instruction *> &SpillSet,
      SetVector<Instruction *> &LiveValues,
      SetVector<std::pair<Instruction *, Instruction *>> &PredCfgEdges);
    void determineValueName(DIType *diType, int64_t &BitOffset, std::string &Name);
    DbgValueInst * findDbgValueInst(Instruction *Inst);
    Instruction * findMetadata(Instruction *Inst);
    Instruction * findPhiIncomingMetadata(PHINode * Phi);
    std::string formatSourceLocation(const std::string &Location, int LineNumber);
    void formatOutput(raw_string_ostream &PrettyStr, raw_string_ostream &VSStr);
    std::string getCallStackForCallSite(const CallInst *CI);
    void getFormatLengths(SetVector<DILocation *> &UseLocations, uint32_t &MaxFuncLength, uint32_t &MaxPathLength);
    void getPhiIncomingLocations(PHINode *I, SetVector<DILocation *> &PILocations);
    void getUseDefinitions(CallInst *CI, SetVector<Instruction *> &LiveValues, SetVector<Value *> &ConditionsKnownFalse, SetVector<Value *> &ConditionsKnownTrue);
    void getUseLocationsForInstruction(Instruction *I, SetVector<DILocation *> &UseLocations);
    bool isLoadRematerializable(CallInst *call);
    void outputPhiIncomingMetadata(PHINode * Phi, raw_string_ostream &PrettyStr, raw_string_ostream &VSStr);
    void outputSourceLocationsPretty(SetVector<DILocation *> &UseLocations, uint32_t maxPathLength, uint32_t maxFuncLength, raw_string_ostream &PrettyStr);
    void outputSourceLocationsVS(SetVector<DILocation *> &UseLocations, uint32_t maxPathLength, uint32_t maxFuncLength, raw_string_ostream &VSStr);
    void processBranch(TerminatorInst *TI,
      SetVector<Value *> &ConditionsKnownFalse,
      SetVector<Value *> &ConditionsKnownTrue,
      SetVector<std::pair<Instruction *, Instruction *>> &VisitedEdges,
      SetVector<Instruction *> &DefsSeen,
      std::vector<std::tuple<BasicBlock *, Instruction *, SetVector<Instruction *>>> &Worklist);
    std::string translateValueToName(DbgValueInst *DVI, Instruction *I);

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

// Live Value Analysis:
// This pass creates two distinct reports that detail the estimated live values surrounding
// each TraceRay call site seen in the DXIL module. These live values take into consideration
// rematerialization (at least a simple yes/no with no cost analysis), control flow, phi nodes,
// and is intended to operate on final optimized DXIL. The information presented by these reports is
// aiming to provide developers with a good approximation of what live values could be seen after IHV
// driver compilation and give a reasonable estimate of how it might impact their runtime performance.
//
// The two distinct report outputs are called 'Pretty' and 'VS'.  'Pretty' is the human readable version that
// is a text output file based on the user provided name while 'VS' is the Visual Studio console debug output
// that allows users to double-click line information for easy shader source navigation.
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
  m_DM = &M.GetDxilModule();

  // Find any define for LIVE_VALUE_REPORT_ROOT in order to build absolute path
  // locations in the LVA Visual Studio (VS) console report.
  // Full file paths allows for source linking by double-clicking the
  // Visual Studio console report.
  // Look for the debuginfo compile unit and then process Live Value Report if
  // it exists, otherwise exit the pass.
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

    // Analyze the CFG for TraceRay call sites and live values.
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

    // Echo the VS report (Visual Studio source linking) to VS debugger and console,
    // cut into chunks as it won't handle a massive string.
    const size_t outputSize = VSStr.str().length();
    const size_t blockSize = 1024;
    for (size_t i = 0; i < outputSize;) {
      OutputDebugStringA(VSStr.str().substr(i, blockSize).c_str());
      i += blockSize;
    }
  }

  // No changes.
  return false;
}

bool LiveValueAnalysis::callSiteExists(CallInst *CI) {
  // Check the set of call sites for this CallInst.
  for (TraceRayCallSite &TRCS : m_callSites) {
    if (TRCS.CI == CI)
      return true;
  }
  return false;
}

void LiveValueAnalysis::addUniqueLocation(DILocation *L, SetVector<DILocation *> &Locations) {
  // Manually search for duplicate locations and add if unique.
  // DILocations can be distinct but their source location is not.
  bool bFound = false;
  DIScope *scope = L->getScope();
  for (auto existingLoc : Locations) {
    // Do not consider column since that is not represented in the LVA report output.
    if (existingLoc->getScope() == scope && existingLoc->getLine() == L->getLine()) {
      bFound = true;
      break;
    }
  }

  if (!bFound) {
    Locations.insert(L);
  }
}

// Determine if a load instruction can be rematerialized.
bool LiveValueAnalysis::isLoadRematerializable(CallInst *Call) {
  CallInst* createHandle = dyn_cast<CallInst>(Call->getArgOperand(HLOperandIndex::kCreateHandleResourceOpIdx));
  DXASSERT(createHandle, "Expected a valid CallInst.");
  DXIL::OpCode DxilOp = OP::GetDxilOpFuncCallInst(Call);

  Value *ResClass = nullptr;

  switch (DxilOp) {
  case OP::OpCode::AnnotateHandle:
    ResClass = Call->getArgOperand(HLOperandIndex::kAnnotateHandleResourceClassOpIdx);
    break;
  case OP::OpCode::CreateHandle:
    ResClass = Call->getArgOperand(HLOperandIndex::kCreateHandleResourceOpIdx);
    break;
  case OP::OpCode::BufferLoad:
  case OP::OpCode::CBufferLoad:
  case OP::OpCode::CBufferLoadLegacy:
    if (LoadInst *LdRes = dyn_cast<LoadInst>(createHandle->getArgOperand(HLOperandIndex::kCreateHandleResourceOpIdx))) {
      if (GlobalVariable *GV = dyn_cast<GlobalVariable>(LdRes->getOperand(0)))
        return true;
    }
    break;
  case OP::OpCode::TextureLoad:
    if (CallInst *CI = dyn_cast<CallInst>(createHandle->getArgOperand(HLOperandIndex::kCreateHandleResourceOpIdx))) {
      if (Value *Res = dyn_cast<Value>(CI->getOperand(HLOperandIndex::kCreateHandleResourceOpIdx))) {
        return true;
      }
    }
    else if (LoadInst *LI = dyn_cast<LoadInst>(createHandle->getArgOperand(DXIL::OperandIndex::kCreateHandleForLibResOpIdx))) {
      // If library handle, compare with UAVs.
      Value *resType = LI->getOperand(0);

      for (auto &&res : m_DM->GetUAVs()) {
        // UAVs cannot be rematerialized.
        if (res->GetGlobalSymbol() == resType) {
          return false;
        }
      }
      return true;
    }
    break;
  case OP::OpCode::RawBufferLoad:
  case OP::OpCode::SampleLevel:
    return true;
  default:
    DXASSERT(0, "Unhandled DXIL opcode.");
    break;
  }

  ConstantInt* resourceClass = dyn_cast<ConstantInt>(ResClass);

  if (resourceClass) {
    DXIL::ResourceClass dxilClass = static_cast<DXIL::ResourceClass>(resourceClass->getZExtValue());
    // Loading from a UAV cannot be rematerialized.
    if (dxilClass == DXIL::ResourceClass::UAV) {
      return false;
    }
    if (dxilClass == DXIL::ResourceClass::CBuffer ||
      dxilClass == DXIL::ResourceClass::SRV ||
      dxilClass == DXIL::ResourceClass::Sampler) {
      return true;
    }
  }

  // If no evidence was found we assume this load instruction cannot be rematerialized.
  return false;
}

// Rematerialization logic should imply basic to moderate optimizations of live values.
// No consideration is given to rematerializing cost and this is intentional.
// If rematerializing is possible then the instruction is considered rematerializable.
bool LiveValueAnalysis::canRemat(Instruction *I) {

  if (isa<GetElementPtrInst>(I))
    return true;
  else if (isa<AllocaInst>(I))
    return true;

  if (CallInst *call = dyn_cast<CallInst>(I)) {
    if (dxilutil::IsLoadIntrinsic(call) && isLoadRematerializable(call))
      return true;
    // Check for annotations added to handles.
    if (call->getCalledFunction()->getName().startswith("dx.op.annotateHandle") && isLoadRematerializable(call))
      return true;
  }

  // Inspect the instruction in more detail to see if it is possible to rematerialize.
  if (dxilutil::IsRematerializable(I)) {
    return true;
  }

  if (Instruction::Load == I->getOpcode())
  {
    // Global variables will be invariably constant and suitable for remat.
    if (GlobalVariable *GV = dyn_cast<GlobalVariable>(I->getOperand(0)))
      return true;
  }

  return false;
}

// Visual Studio source linking requires a full path with proper formatting.
std::string LiveValueAnalysis::formatSourceLocation(const std::string& Location, int LineNumber) {
  std::string fullPath = "";

  // Only add the root path if one does not already exist for Location; assuming ':' exists in the path.
  if (Location.find(':') == std::string::npos)
    fullPath = m_rootPath;

  fullPath += Location + "(" + std::to_string(LineNumber) + ")";

  std::replace(fullPath.begin(), fullPath.end(), '\\', '/');

  return fullPath;
}

// Determine the max path and function lengths for formatting text output for readability.
void LiveValueAnalysis::getFormatLengths(SetVector<DILocation *> &UseLocations, uint32_t &maxFuncLength, uint32_t &maxPathLength) {
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

// Determine the call stack of a TraceRay call instruction.
// This is necessary for the report so users can differentiate call stacks when the actual
// TraceRay call site in source code is the same location (ie: when using helper functions).
std::string LiveValueAnalysis::getCallStackForCallSite(const CallInst *CI) {
  std::stack<std::string> callStack;

  if (const llvm::DebugLoc &debugInfo = CI->getDebugLoc()) {
    callStack.push(">" + formatSourceLocation(debugInfo->getFilename(), debugInfo->getLine()) + "\n");
    if (DILocation *InLoc = debugInfo->getInlinedAt()) {
      callStack.push(">" + formatSourceLocation(InLoc->getFilename(), InLoc->getLine()) + "\n");
      while (DILocation *NestedInLoc = InLoc->getInlinedAt()) {
        callStack.push(">" + formatSourceLocation(NestedInLoc->getFilename(), NestedInLoc->getLine()) + "\n");
        InLoc = NestedInLoc;
      }
    }
  }
  else {
    DXASSERT(false, "Expected debug location for CallInst.");
  }

  std::string callStackString;
  while (!callStack.empty()) {
    callStackString += callStack.top();
    callStack.pop();
  }

  return callStackString;
}

// Determine the location of instructions connected to a phi's incoming.
void LiveValueAnalysis::getPhiIncomingLocations(PHINode *Phi, SetVector<DILocation *> &PILocations) {
  std::vector<PHINode *> Phis;
  // Track the phis we visit so that we do not duplicate them.
  SetVector<PHINode *> VisitedPhis;

  for (;;) {
    for (unsigned i = 0; i < Phi->getNumIncomingValues(); ++i) {
      Instruction *use_inst = dyn_cast<Instruction>(Phi->getIncomingValue(i));
      if (!use_inst)
        continue;
      if (DILocation *Loc = use_inst->getDebugLoc()) {
        addUniqueLocation(Loc, PILocations);
      }
      // Check for a phi node that will need to be checked for use locations.
      if (isa<PHINode>(use_inst)) {
        PHINode *NewPhi = dyn_cast<PHINode>(use_inst);
        if (VisitedPhis.insert(NewPhi)) {
          Phis.push_back(NewPhi);
        }
      }
    }

    // Process any phis in the list.
    if (Phis.empty()) {
      break;
    }
    else {
      Phi = Phis.back();
      Phis.pop_back();
    }
  }
}

// Determine the use locations of an instruction. This gives more detail to the user and finds source locations where a live value
// is used to help in their decision for optimization/removal.
void LiveValueAnalysis::getUseLocationsForInstruction(Instruction *I, SetVector<DILocation *> &UseLocations) {
  std::vector<PHINode *> Phis;
  // Track the phis we visit so that we do not duplicate them.
  SetVector<PHINode *> VisitedPhis;

  for (;;) {
    for (User *U : I->users()) {
      if (auto Inst = dyn_cast<Instruction>(U)) {
        if (DILocation *Loc = Inst->getDebugLoc()) {
          addUniqueLocation(Loc, UseLocations);
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

// Examine a branch instruction and add seen definitions based on the conditions and
// provided the edge has not already been visited. This also updates the visited edges and worklist.
void LiveValueAnalysis::processBranch(TerminatorInst *TI,
  SetVector<Value *> &ConditionsKnownFalse,
  SetVector<Value *> &ConditionsKnownTrue,
  SetVector<std::pair<Instruction *, Instruction *>> &VisitedEdges,
  SetVector<Instruction *> &DefsSeen,
  std::vector<std::tuple<BasicBlock *, Instruction *, SetVector<Instruction *>>> &Worklist) {
  Instruction *inst = dyn_cast<Instruction>(TI);
  BranchInst *branch = dyn_cast<BranchInst>(TI);

  if (branch && branch->isConditional()) {
    // Conditional branch. Check against known conditions.
    DXASSERT(branch->getNumSuccessors() == 2, "Expected two successors.");

    Instruction *IfTrue = branch->getSuccessor(0)->begin();
    Instruction *IfFalse = branch->getSuccessor(1)->begin();

    std::pair<Instruction *, Instruction *> trueEdge = std::make_pair(branch, IfTrue);
    std::pair<Instruction *, Instruction *> falseEdge = std::make_pair(branch, IfFalse);

    // Check the branch against conditions known to determine where the seen defintions belong.
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
      // If this edge has not been visited add it to the worklist.
      if (VisitedEdges.insert(edge)) {
        Worklist.push_back(std::make_tuple(inst->getParent(), successor, DefsSeen));
      }
    }
  }
}

// Traverse each control flow edge separately and gather up seen definitions along
// the way. By keeping track of the from-block, we can select the right incoming value at phi nodes.
void LiveValueAnalysis::getUseDefinitions(CallInst *CI, SetVector<Instruction *> &LiveValues, SetVector<Value *> &ConditionsKnownFalse, SetVector<Value *> &ConditionsKnownTrue) {
  Instruction *inst = CI->getNextNode();

  BasicBlock *fromBlock = nullptr;
  // Definitions seen.
  SetVector<Instruction *> defsSeen;
  // Track the edges we have already visited.
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
    if (isa<CallInst>(inst) && callSiteExists(cast<CallInst>(inst))) {
      if (worklist.empty())
        break;

      fromBlock = std::get<0>(worklist.back());
      inst = std::get<1>(worklist.back());
      defsSeen = std::move(std::get<2>(worklist.back()));
      worklist.pop_back();
      continue;
    }

    if (TerminatorInst *terminator = dyn_cast<TerminatorInst>(inst)) {
      // Add conditions within this branch to the worklist provided the edges have not been visited.
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

// Analyze the CFG and find edges that can lead to this trace ray call site.
void LiveValueAnalysis::analyzeLiveValues(CallInst *TraceCall, SetVector<Instruction *> &LiveValues, SetVector<std::pair<Instruction *, Instruction *>> &PredCfgEdges) {

  BasicBlock *BB = TraceCall->getParent(); // Starting point is the trace ray call block.
  SmallVector<BasicBlock*, 16> worklist; // Work list of blocks connecting potential live values to a trace ray call.
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

    // Propagate phi conditions to incoming if only a single incoming.
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

// Determine the spill set of live values that can be considered for rematerialization.
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

    // If the instruction cannot be rematerialized add it to the spill set.
    if (!canRemat(inst)) {
      SpillSet.insert(inst);
      continue;
    }

    // The instruction is rematerializable so add it to the remat set.
    RematSet.insert(inst);

    // If this is a phi node we want to check the incomings for remat.
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

// Analyze the CFG to find trace ray calls and their potential live values.
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
        if (call && call->getParent()->getParent() == &main) {
          TraceRayCallSite TRCS = { getCallStackForCallSite(call), call };
          m_callSites.push_back(TRCS);
        }
      }
    }
  }

  for (TraceRayCallSite &TRCS : m_callSites) {
    // Analyze live values and consider optimziations that could reasonably remove those live values further down the compilation path.
    // Collect the live values and a vector of CFG edges that lead to potential live values.
    SetVector<Instruction *> liveValues;
    SetVector<std::pair<Instruction *, Instruction *>> predCfgEdges;
    analyzeLiveValues(TRCS.CI, liveValues, predCfgEdges);
    // Determine sets of remat and spill based on the potential live values.
    SetVector<Instruction *> rematSet;
    SetVector<Instruction *> spillSet;
    determineRematSpillSet(TRCS.CI, rematSet, spillSet, liveValues, predCfgEdges);
    // The remat could be used in a separate report, or debug info for this pass. Comparing remat sets in regression tests could be useful.
    // Store the spills for this trace ray call.
    m_spillsPerTraceCall[TRCS.CI].insert(m_spillsPerTraceCall[TRCS.CI].end(), spillSet.begin(), spillSet.end());
  }
}

void LiveValueAnalysis::determineValueName(DIType *diType, int64_t &BitOffset, std::string &Name) {
  // Find the data member name of a derived type matching the bit offset.
  // Note that we are not interested in DIBasicType as it's better to present more interesting type names in the report.
  if (auto* CT = dyn_cast<DICompositeType>(diType))
  {
    switch (diType->getTag())
    {
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
    DICompositeType *CompTy = nullptr;

    // If size is 0 check the base type for a Composite type.
    if (size == 0)
    {
      CompTy = dyn_cast_or_null<DICompositeType>(DT->getRawBaseType());
      while ((DT = dyn_cast_or_null<DIDerivedType>(DT->getRawBaseType())))
      {
        CompTy = dyn_cast_or_null<DICompositeType>(DT->getRawBaseType());
      }
      if (CompTy)
      {
        // Traverse inside this composite type.
        determineValueName(CompTy, BitOffset, Name);
      }
    }
    // The derived type has been found when its size is within the bit offset. Potentially a composite type which will need to be traversed.
    else if (((BitOffset - size) < 0))
    {
      Name += DT->getName();
      CompTy = dyn_cast_or_null<DICompositeType>(DT->getRawBaseType());
      while ((DT = dyn_cast_or_null<DIDerivedType>(DT->getRawBaseType())))
      {
        CompTy = dyn_cast_or_null<DICompositeType>(DT->getRawBaseType());
      }
      if (CompTy)
      {
        // Traverse inside this composite type, using '.' for the new member name.
        Name += ".";
        determineValueName(CompTy, BitOffset, Name);
      }
    }
    // Deduct the size from the bit offset and keep traversing.
    BitOffset -= size;
  }
}

// Create a string with the name and source location of the given instruction.
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

// Find the DbgValueInst for the given instruction or one of its users, if it exists.
DbgValueInst * LiveValueAnalysis::findDbgValueInst(Instruction *Inst) {

  if (auto *L = LocalAsMetadata::getIfExists(Inst)) {
    if (auto *MDV = MetadataAsValue::getIfExists(Inst->getContext(), L)) {
      for (User *U : MDV->users()) {
        if (DbgValueInst *DVI = dyn_cast<DbgValueInst>(U)) {
          if (!DVI->getVariable()->isArtificial())
            return DVI;
        }
      }
    }
  }

  return nullptr;
}

// Find metadata using the given instruction or one of its users, if it exists.
Instruction * LiveValueAnalysis::findMetadata(Instruction *Inst) {

  if (!Inst)
    return nullptr;

  for (User * U : Inst->users()) {
    Instruction * I = dyn_cast<Instruction>(U);
    if (I)
    {
      if (I->isUsedByMetadata()) {
        return I;
      }
      Instruction *MDInst = findMetadata(I);
      if (MDInst)
        return MDInst;
    }
  }

  return nullptr;
}

// Find the instruction connected to a phi and its incomings in order to get debuginfo metadata, if it exists.
Instruction * LiveValueAnalysis::findPhiIncomingMetadata(PHINode * Phi) {

  for (unsigned i = 0; i < Phi->getNumIncomingValues(); ++i) {
    Instruction *incoming = dyn_cast<Instruction>(Phi->getIncomingValue(i));
    if (!incoming)
      continue;

    if (incoming->isUsedByMetadata()) {
      if (auto *DVI = findDbgValueInst(incoming)) {
        return incoming;
      }
    }
    else {
      // Search the incoming locations if no metadata name has yet been found.
      // Track the phis we visit so that we do not duplicate them.
      std::vector<PHINode *> Phis;
      SetVector<PHINode *> VisitedPhis;

      for (;;) {
        for (unsigned i = 0; i < Phi->getNumIncomingValues(); ++i) {
          Instruction *use_inst = dyn_cast<Instruction>(Phi->getIncomingValue(i));
          if (!use_inst)
            continue;
          if (use_inst->isUsedByMetadata()) {
            if (auto *DVI = findDbgValueInst(use_inst)) {
              return use_inst;
            }
          }
          else {
            // Check users for metadata.
            for (User *U : use_inst->users()) {
              Instruction *NestedU = dyn_cast<Instruction>(U);
              if (!NestedU)
                continue;
              if (NestedU->isUsedByMetadata()) {
                if (auto *DVI = findDbgValueInst(NestedU)) {
                  return NestedU;
                }
              }
            }
          }
          // Check for a phi node that will need to be checked for metadata.
          if (isa<PHINode>(use_inst)) {
            PHINode *NewPhi = dyn_cast<PHINode>(use_inst);
            if (VisitedPhis.insert(NewPhi)) {
              Phis.push_back(NewPhi);
            }
          }
        }
        // Process any phis in the list.
        if (Phis.empty()) {
          break;
        }
        else {
          Phi = Phis.back();
          Phis.pop_back();
        }
      }
    }
  }

  return nullptr;
}

// Output to the VS and Pretty text streams all of the phi and its incomings use locations.
void LiveValueAnalysis::outputPhiIncomingMetadata(PHINode * Phi, raw_string_ostream &PrettyStr, raw_string_ostream &VSStr) {
  
  // It is possible to find a phi node that has no debuginfo metadata attached to its users or incomings.
  // If no metadata can be found to produce the value's name we will use "[conditional variable]" in the report.
  std::string phiName = "[conditional variable] ";
  bool bFoundName = false;
  SetVector<DILocation *> phiLocations; // locations where the phi is used.
  SetVector<DILocation *> incomingLocations; // locations where the phi's incoming are used.

  for (User *U : Phi->users()) {
    if (Instruction *I = dyn_cast<Instruction>(U)) {
      if (const llvm::DebugLoc &debugInfo = I->getDebugLoc()) {
        addUniqueLocation(debugInfo, phiLocations);
      }
    }
  }

  for (unsigned i = 0; i < Phi->getNumIncomingValues(); ++i) {
    Instruction *incoming = dyn_cast<Instruction>(Phi->getIncomingValue(i));
    if (!incoming)
      continue;

    if (PHINode *incomingPhi = dyn_cast<PHINode>(incoming)) {
      // Get locations for all instructions connected to the phi incoming.
      SetVector<DILocation *> UseLocations;
      getPhiIncomingLocations(incomingPhi, UseLocations);
      incomingLocations.insert(UseLocations.begin(), UseLocations.end());
    }
    else {
      // Get use locations.
      SetVector<DILocation *> UseLocations;
      getUseLocationsForInstruction(incoming, UseLocations);
      incomingLocations.insert(UseLocations.begin(), UseLocations.end());
    }

    if (!bFoundName) {
      if (incoming->isUsedByMetadata()) {
        if (auto *DVI = findDbgValueInst(incoming)) {
          phiName = translateValueToName(DVI, incoming);
          bFoundName = true;
        }
      }
      else {
        // Search the incoming locations if no metadata name has yet been found.
        std::vector<PHINode *> Phis;
        // Track the phis we visit so that we do not duplicate them.
        SetVector<PHINode *> VisitedPhis;

        for (;;) {
          for (unsigned i = 0; i < Phi->getNumIncomingValues(); ++i) {
            Instruction *use_inst = dyn_cast<Instruction>(Phi->getIncomingValue(i));
            if (!use_inst)
              continue;
            if (use_inst->isUsedByMetadata()) {
              if (auto *DVI = findDbgValueInst(use_inst)) {
                phiName = translateValueToName(DVI, use_inst);
                bFoundName = true;
              }
            }
            else {
              // Check users for metadata.
              for (User *U : use_inst->users()) {
                Instruction *NestedU = dyn_cast<Instruction>(U);
                if (!NestedU)
                  continue;
                if (NestedU->isUsedByMetadata()) {
                  if (auto *DVI = findDbgValueInst(NestedU)) {
                    phiName = translateValueToName(DVI, NestedU);
                    bFoundName = true;
                  }
                }
              }
            }
            // Check for a phi node that will need to be checked for use locations.
            if (isa<PHINode>(use_inst)) {
              PHINode *NewPhi = dyn_cast<PHINode>(use_inst);
              if (VisitedPhis.insert(NewPhi)) {
                Phis.push_back(NewPhi);
              }
            }
          }
          // Process any phis in the list.
          if (Phis.empty()) {
            break;
          }
          else {
            Phi = Phis.back();
            Phis.pop_back();
          }
        }
      }
    }
  }

  PrettyStr << phiName << "Use Locations:\n";
  VSStr << phiName << "Use Locations:\n";

  // Find max path length to format horizontal dividers in the output.
  uint32_t maxPathLength = 0;
  uint32_t maxFuncLength = 0;
  getFormatLengths(incomingLocations, maxFuncLength, maxPathLength);

  // Pad with spaces for inline text, brackets, etc. in the prettyprint output formatting.
  maxFuncLength += 4;
  maxPathLength += sizeof(inlinePrefix);
  // Output use locations
  outputSourceLocationsPretty(incomingLocations, maxPathLength, maxFuncLength, PrettyStr);
  outputSourceLocationsVS(incomingLocations, maxPathLength, maxFuncLength, VSStr);

  PrettyStr << "\n";
  VSStr << "\n";
}

// Format and output a list of source locations to the Pretty report.
void LiveValueAnalysis::outputSourceLocationsPretty(SetVector<DILocation *> &UseLocations, uint32_t maxPathLength, uint32_t maxFuncLength, raw_string_ostream &PrettyStr) {
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

// Format and output a list of source locations to the VS report.
void LiveValueAnalysis::outputSourceLocationsVS(SetVector<DILocation *> &UseLocations, uint32_t maxPathLength, uint32_t maxFuncLength, raw_string_ostream &VSStr) {
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

// Format the live value output into two distinct reports: Pretty and VS.
void LiveValueAnalysis::formatOutput(raw_string_ostream &PrettyStr, raw_string_ostream &VSStr) {
  std::string callSiteTitle;
  unsigned int callNumber = 1; // Incrementing counter for TraceRay call sites.
  std::string HeaderText;
  std::string fullPath;
  raw_string_ostream TmpStr(HeaderText);

  TmpStr << "===========================================\n";
  TmpStr << "== TRACE RAY CALL SITES WITH LIVE VALUES ==\n";
  TmpStr << "===========================================\n";

  for (TraceRayCallSite &TRCS : m_callSites) {
    CallInst * callSite = TRCS.CI;

    if (const llvm::DebugLoc &debugInfo = callSite->getDebugLoc()) {
      // Prepare a map of the live value instructions and their associated value names.
      for (Instruction *I : (m_spillsPerTraceCall[callSite])) {

        bool foundMetadata = false;

        if (isa<ExtractValueInst>(I)) {
          I = dyn_cast<Instruction>(dyn_cast<ExtractValueInst>(I)->getAggregateOperand());
        }

        // Add to the live values list even if it's not used by metadata so that we can track more accurately. Only filter out artifical values.
        LiveValueInst LVI = { "", I };

        if (I->isUsedByMetadata()) {
          if (auto *DVI = findDbgValueInst(I)) {
            LVI.Name = translateValueToName(DVI, I);
            foundMetadata = true;
          }
        }
        else
        {

          // If this instruction has no metadata check its users.
          for (User *U : I->users()) {
            if (Instruction *UserI = dyn_cast<Instruction>(U)) {
              if (UserI->isUsedByMetadata()) {
                if (auto *DVI = findDbgValueInst(UserI)) {
                  LVI.Name = translateValueToName(DVI, UserI);
                  LVI.Inst = UserI;
                  foundMetadata = true;
                  break;
                }
              }
            }
          }
          if (!foundMetadata) {
            if (isa<PHINode>(I)) {
              if (Instruction *phiInfo = findPhiIncomingMetadata(dyn_cast<PHINode>(I))) {
                if (auto *DVI = findDbgValueInst(phiInfo)) {
                  LVI.Name = translateValueToName(DVI, phiInfo);
                  LVI.Inst = phiInfo;
                  foundMetadata = true;
                }
              }
            }

            Instruction *MDInst = findMetadata(I);
            // If a new instruction was found that is used by metadata update the LVI.
            if (MDInst)
            {
              LVI.Inst = MDInst;

              // Find the name.
              if (MDInst->isUsedByMetadata()) {
                if (auto *DVI = findDbgValueInst(MDInst)) {
                  LVI.Name = translateValueToName(DVI, MDInst);
                }
              }
              else
              {
                // If this instruction has no metadata check its users.
                for (User *U : I->users()) {
                  if (Instruction *UserI = dyn_cast<Instruction>(U)) {
                    if (UserI->isUsedByMetadata()) {
                      if (auto *DVI = findDbgValueInst(UserI)) {
                        LVI.Name = translateValueToName(DVI, UserI);
                        LVI.Inst = UserI;
                        break;
                      }
                    }
                  }
                }
              }
            }
          }
        }
        // Add live value only with valid metadata.
        if (foundMetadata) {
          m_liveValuesPerTraceCall[callSite].push_back(LVI);
        }
      }
      TmpStr << "TraceRay call #" << std::to_string(callNumber) << " incurs " << m_liveValuesPerTraceCall[callSite].size() << " 32-bit registers\n";
      TmpStr << TRCS.CallStack << "\n";

      // Sort the order of live values per call site by name.
      std::sort(m_liveValuesPerTraceCall[callSite].begin(), m_liveValuesPerTraceCall[callSite].end(), LiveValueCompare());
      callNumber++;
    }
  }
  TmpStr << "\n";

  // Copy to PrettyStr VSStr.
  VSStr << TmpStr.str();
  PrettyStr << TmpStr.str();
  callNumber = 1;

  for (TraceRayCallSite &TRCS : m_callSites) {
    callSiteTitle = "Live State for TraceRay call #" + std::to_string(callNumber++) + "\n";
    std::ostringstream Spacer;
    TmpStr.str().clear();
    CallInst *callSite = TRCS.CI;
    // Horizontal spacer using '='
    Spacer << std::setfill('=') << std::setw(60) << std::right << "\n";
    TmpStr << Spacer.str();
    TmpStr << callSiteTitle;
    TmpStr << TRCS.CallStack;
    TmpStr << Spacer.str();

    size_t regs = 0;
    // Test to help detect missing metadata for potential live values.
    // Only count the values for which metadata exists.
    // TODO: Report count of values for those which are not used metadata?
    // this can lead to missing live values in some cases.
    std::vector<Instruction*> DetectedInstr; // Detected instructions, but with no user metadata.
    for (LiveValueInst &LVI : m_liveValuesPerTraceCall[callSite]) {
      Instruction *I = LVI.Inst;
      if (I && (I->isUsedByMetadata() || isa<PHINode>(I))) {
        // Live value that has valid metadata.
        regs++;
      }
      else {
        DetectedInstr.push_back(I);
      }
    }
#if !defined(NDEBUG)
    // Debug check to find missing metadata on live values.
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
#endif // DEBUG
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
        if (auto *DVI = findDbgValueInst(I)) {
          TmpStr << translateValueToName(DVI, I);

          // Get use locations.
          SetVector<DILocation *> UseLocations;
          getUseLocationsForInstruction(I, UseLocations);

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
          PrettyStr << "\n";
          VSStr << "\n";
        }
      }
      else if (isa<PHINode>(I)) {
        // A fallback to output a phi for which no metadata connection could be found.
        outputPhiIncomingMetadata(dyn_cast<PHINode>(I), PrettyStr, VSStr);
      }
    }
  }
}
