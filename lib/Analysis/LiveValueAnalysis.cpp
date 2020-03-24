

#include "llvm/Analysis/LiveValueAnalysis.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "dxc/DXIL/DxilMetadataHelper.h"
#include "dxc/Support/Global.h"

#include <fstream>

using namespace llvm;

// Register this pass...
char LiveValueAnalysis::ID = 0;
INITIALIZE_PASS(LiveValueAnalysis, "lva", "Live Value Analysis and reporting for DXR", false, true)

ModulePass *llvm::createLiveValueAnalysisPass(StringRef LiveValueAnalysisOutputFile) {
  return new LiveValueAnalysis(LiveValueAnalysisOutputFile);
}

LiveValueAnalysis::LiveValueAnalysis(StringRef LiveValueAnalysisOutputFile)
  : ModulePass(ID) {
  initializeLiveValueAnalysisPass(*PassRegistry::getPassRegistry());
  m_outputFile = LiveValueAnalysisOutputFile;
}

LiveValueAnalysis::~LiveValueAnalysis() {
}

bool LiveValueAnalysis::runOnModule(Module &M) {

  std::string PrettyReport;
  std::string VSReport;
  raw_string_ostream PrettyStr(PrettyReport);
  raw_string_ostream VSStr(VSReport);

  m_module = &M;

  // Find any define for LIVE_VALUE_REPORT_ROOT in order to build absolute path
  // locations in the LVA report.
  std::string RootPath = "";
  // Look for the debuginfo compile unit and then process Live Value Report if
  // it exists
  NamedMDNode *NMD = m_module->getNamedMetadata("llvm.dbg.cu");
  if (NMD) {
    llvm::NamedMDNode *Defines = m_module->getNamedMetadata(
      hlsl::DxilMDHelper::kDxilSourceDefinesMDName);
    if (!Defines) {
      Defines = m_module->getNamedMetadata("llvm.dbg.defines");
    }
    if (Defines) {
      for (unsigned i = 0, e = Defines->getNumOperands(); i != e; ++i) {
        MDNode *node = Defines->getOperand(i);

        if ((node->getNumOperands() > 0) &&
          node->getOperand(0)->getMetadataID() == Metadata::MDStringKind) {
          MDString *str = dyn_cast<MDString>(node->getOperand(0));
          if (str->getString().find("LIVE_VALUE_REPORT_ROOT=") != StringRef::npos) {
            RootPath = str->getString().substr(23, str->getString().size() - 23);
          }
          // Standarize backslashes
          std::replace(RootPath.begin(), RootPath.end(), '\\', '/');
        }
      }
    }

    // Analyze the CFG for TraceRay callsites.
    analyzeCFG();
    // Format the output reports.
    formatOutput( PrettyStr, VSStr );

    // Optionally output the Pretty report to file.
    if (!m_outputFile.empty()) {
      // Standarize backslashes
      std::replace(m_outputFile.begin(), m_outputFile.end(), '\\', '/');
      std::ofstream outFile;
      outFile.exceptions(std::ofstream::failbit);
      try {
        outFile.open( m_outputFile );
        outFile << PrettyStr.str();
        outFile.close();
      } catch (...) {
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

    PrettyStr.close();
    VSStr.close();
  }

  // No changes.
  return false;
}

template<class T, class C>
std::vector<T> LiveValueAnalysis::sortInstructions(C& c)
{
  std::vector<T> sorted;

  for (T inst : c)
    sorted.push_back(inst);

  if (sorted.empty())
    return sorted;

  DominatorTree DT;
  DT.recalculate(*sorted[0]->getParent()->getParent());

  std::sort(sorted.begin(), sorted.end(), [&] (T a, T b) -> bool {
    if (DT.dominates(a->getParent(), b->getParent()))
      return true;
    else if (DT.dominates(b->getParent(), a->getParent()))
      return false;

    Function::iterator itA( a->getParent() );
    Function::iterator itB( b->getParent() );
    size_t ad = std::distance( a->getParent()->getParent()->begin(), itA );
    size_t bd = std::distance( a->getParent()->getParent()->begin(), itB );

    return ad < bd;
  });

  return sorted;
}

bool LiveValueAnalysis::usesUavResource(CallInst* call) {
  CallInst* createHandle = dyn_cast<CallInst>(call->getArgOperand(1));
  assert(createHandle);
  assert(createHandle->getCalledFunction()->getName().startswith("dx.op.createHandle"));
  ConstantInt* resourceClass = dyn_cast<ConstantInt>(createHandle->getArgOperand(1));
  if( (nullptr != resourceClass) && (resourceClass->getZExtValue() == 1) )
    return true;

  return false;
}

// Rematerialization logic of future compilation should imply basic to moderate optimizations wrt live values.
bool LiveValueAnalysis::canRemat(Instruction* inst) {
  if( isa<GetElementPtrInst>( inst ) )
    return true;
  else if( isa<AllocaInst>( inst ) )
    return true;
  else if( isa<LoadInst>( inst ) )
    return true;

  if( CallInst* call = dyn_cast<CallInst>(inst) ) {
    if( call->getCalledFunction()->getName().startswith( "dx.op.bufferLoad" ) && !usesUavResource( call ) )
      return true;
    else if (call->getCalledFunction()->getName().startswith("dx.op.textureLoad") && !usesUavResource(call))
      return true;
    else if (call->getCalledFunction()->getName().startswith("dx.op.rawBufferLoad") && !usesUavResource(call))
      return true;
    else if (call->getCalledFunction()->getName().startswith("dx.op.createHandleForLib"))
      return true;
  }

  return false;
}

// Full path locations can be complicated and some projects have strange behavior. Customize accordingly.
std::string LiveValueAnalysis::formatSourceLocation( const std::string& m_rootPath, const std::string& Location, int lineNumber )
{
  std::string fullPath = "";

  // Only add the root path if one does not already exist for Location.
  if( Location.find( ':' ) == StringRef::npos )
    fullPath = m_rootPath;

  fullPath += Location + "(" + std::to_string(lineNumber) + ")"; 

  std::replace(fullPath.begin(), fullPath.end(), '\\', '/');

  return fullPath;
}

void LiveValueAnalysis::analyzeCFG() {
  
  // Find TraceRay call sites.
  for( Function &main : *m_module ) {
    for( Function &F : *m_module ) {

      if( !F.getName().startswith( "dx.op.traceRay" ) )
        continue;

      for( User *user : F.users() ) {
        CallInst *call = dyn_cast<CallInst>(user);
        if( call && call->getParent()->getParent() == &main )
          m_callSites.insert( call );
      }
    }
  }

  // Determine live values across each call.
  std::set<BasicBlock *> phiSelectorSpills;

  // Prepare Debug Info.
  DebugInfoFinder DIF;
  DIF.processModule(*m_module);

  for (CallInst *callSite : m_callSites) {
    // Determine CFG edges that can lead to this call site.
    std::set<std::pair<Instruction *, Instruction *>> predCfgEdges;
    {
      BasicBlock *BB = callSite->getParent();
      std::vector<BasicBlock *> worklist;

      for (;;) {
        for (auto i = llvm::pred_begin(BB); i != pred_end(BB); ++i) {
          BasicBlock *pred = *i;
          std::pair<Instruction *, Instruction *> edge =
            std::make_pair(pred->getTerminator(), BB->begin());

          if (!predCfgEdges.count(edge)) {
            predCfgEdges.insert(edge);
            worklist.push_back(pred);
          }
        }

        if (worklist.empty())
          break;

        BB = worklist.back();
        worklist.pop_back();
      }
    }

    // Determine known branch conditions based on CFG edges.
    // Prunes pred CFG edges based on invariants iteratively.
    std::set<Value *> conditionsKnownTrue;
    std::set<Value *> conditionsKnownFalse;

    for (;;) {
      bool change = false;
      for (std::pair<Instruction *, Instruction *> edge :
        std::set<std::pair<Instruction *, Instruction *>>(predCfgEdges)) {
        BranchInst *branch = dyn_cast<BranchInst>(edge.first);

        if (!branch || !branch->isConditional())
          continue;

        assert(branch->getNumSuccessors() == 2);

        Instruction *taken = branch->getSuccessor(0)->begin();
        Instruction *notTaken = branch->getSuccessor(1)->begin();

        std::pair<Instruction *, Instruction *> takenEdge =
          std::make_pair(edge.first, taken);
        std::pair<Instruction *, Instruction *> notTakenEdge =
          std::make_pair(edge.first, notTaken);

        int count =
          predCfgEdges.count(takenEdge) + predCfgEdges.count(notTakenEdge);

        if (!count)
          continue;

        if (count == 2) {
          if (conditionsKnownTrue.count(branch->getCondition())) {
            predCfgEdges.erase(notTakenEdge);
            change = true;
          }
          if (conditionsKnownFalse.count(branch->getCondition())) {
            predCfgEdges.erase(takenEdge);
            change = true;
          }
          continue;
        }

        if (predCfgEdges.count(takenEdge)) {
          if (!conditionsKnownTrue.count(branch->getCondition())) {
            conditionsKnownTrue.insert(branch->getCondition());
            change = true;
          }
        } else {
          if (!conditionsKnownFalse.count(branch->getCondition())) {
            conditionsKnownFalse.insert(branch->getCondition());
            change = true;
          }
        }
      }

      std::vector<PHINode *> phiConditions;

      for (Value *condition : conditionsKnownTrue) {
        PHINode *phi = dyn_cast<PHINode>(condition);
        if (phi)
          phiConditions.push_back(phi);
      }

      for (Value *condition : conditionsKnownFalse) {
        PHINode *phi = dyn_cast<PHINode>(condition);
        if (phi)
          phiConditions.push_back(phi);
      }

      // Propagate PHI conditions to incoming if only a single incoming.
      for (PHINode *phi : phiConditions) {
        unsigned numIncoming = 0;
        Value *incomingValue = nullptr;

        for (unsigned i = 0; i < phi->getNumIncomingValues(); ++i) {
          std::pair<Instruction *, Instruction *> edge =
            std::make_pair(phi->getIncomingBlock(i)->getTerminator(),
              phi->getParent()->begin());

          // This was defined before the trace call.
          // So we can check predCfgEdges for valid incoming values.
          if (predCfgEdges.count(edge)) {
            ++numIncoming;
            incomingValue = phi->getIncomingValue(i);
          }
        }

        assert(numIncoming);

        if (numIncoming == 1) {
          if (conditionsKnownFalse.count(incomingValue) ==
            conditionsKnownFalse.count(phi) &&
            conditionsKnownTrue.count(incomingValue) ==
            conditionsKnownTrue.count(phi))
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
        assert(!conditionsKnownFalse.count(condition));

      for (Value *condition : conditionsKnownFalse)
        assert(!conditionsKnownTrue.count(condition));
#endif

      if (!change)
        break;
    }

    // Traverse each control flow edge separately and gather up seen defs along
    // the way. By keeping track of the from-block, we can select the right
    // incoming value at PHIs.
    std::set<Instruction *> liveValues;
    {
      Instruction *inst = callSite->getNextNode();

      BasicBlock *fromBlock = nullptr;
      std::set<Instruction *> defsSeen;
      std::set<std::pair<Instruction *, Instruction *>> visitedEdges;
      std::vector<
        std::tuple<BasicBlock *, Instruction *, std::set<Instruction *>>>
        worklist;

      defsSeen.insert(callSite);

      for (;;) {
        if (PHINode *phi = dyn_cast<PHINode>(inst)) {
          for (unsigned i = 0; i < phi->getNumIncomingValues(); ++i) {
            if (phi->getIncomingBlock(i) != fromBlock)
              continue;

            Instruction *use_inst =
              dyn_cast<Instruction>(phi->getIncomingValue(i));
            if (!use_inst)
              continue;

            if (!defsSeen.count(use_inst)) {
              // Use without a preceding def must be a live value.
              liveValues.insert(use_inst);
            }
          }
        } else {
          for (unsigned i = 0; i < inst->getNumOperands(); ++i) {
            Instruction *use_inst = dyn_cast<Instruction>(inst->getOperand(i));
            if (!use_inst)
              continue;

            if (!defsSeen.count(use_inst)) {
              // Use without a preceding def must be a live value.
              liveValues.insert(use_inst);
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
          BranchInst *branch = dyn_cast<BranchInst>(terminator);

          if (branch && branch->isConditional()) {
            // Conditional branch. Check against known conditions.
            assert(branch->getNumSuccessors() == 2);

            Instruction *taken = branch->getSuccessor(0)->begin();
            Instruction *notTaken = branch->getSuccessor(1)->begin();

            std::pair<Instruction *, Instruction *> takenEdge =
              std::make_pair(branch, taken);
            std::pair<Instruction *, Instruction *> notTakenEdge =
              std::make_pair(branch, notTaken);

            if (!conditionsKnownFalse.count(branch->getCondition())) {
              if (!visitedEdges.count(takenEdge)) {
                visitedEdges.insert(takenEdge);
                worklist.push_back(
                  std::make_tuple(inst->getParent(), taken, defsSeen));
              }
            }

            if (!conditionsKnownTrue.count(branch->getCondition())) {
              if (!visitedEdges.count(notTakenEdge)) {
                visitedEdges.insert(notTakenEdge);
                worklist.push_back(
                  std::make_tuple(inst->getParent(), notTaken, defsSeen));
              }
            }
          } else {
            for (unsigned i = 0, e = terminator->getNumSuccessors(); i < e;
              ++i) {
              Instruction *successor = terminator->getSuccessor(i)->begin();
              std::pair<Instruction *, Instruction *> edge =
                std::make_pair(inst, successor);

              if (!visitedEdges.count(edge)) {
                visitedEdges.insert(edge);
                worklist.push_back(
                  std::make_tuple(inst->getParent(), successor, defsSeen));
              }
            }
          }

          if (worklist.empty())
            break;

          fromBlock = std::get<0>(worklist.back());
          inst = std::get<1>(worklist.back());
          defsSeen = std::move(std::get<2>(worklist.back()));
          worklist.pop_back();
          continue;
        }

        inst = inst->getNextNode();
      }
    }

    // Determine initial sets of remat/spill.
    std::set<Instruction *> rematSet;
    std::set<Instruction *> spillSet;
    {
      std::vector<Instruction *> worklist;
      std::set<Instruction *> visited;

      for (Instruction *inst : liveValues) {
        visited.insert(inst);
        worklist.push_back(inst);
      }

      while (!worklist.empty()) {
        Instruction *inst = worklist.back();
        worklist.pop_back();

        if (!canRemat(inst)) {
          spillSet.insert(inst);
          continue;
        }

        rematSet.insert(inst);

        if (PHINode *phi = dyn_cast<PHINode>(inst)) {
          for (unsigned i = 0; i < phi->getNumIncomingValues(); ++i) {
            std::pair<Instruction *, Instruction *> edge =
              std::make_pair(phi->getIncomingBlock(i)->getTerminator(),
                phi->getParent()->begin());

            // This is a live value, i.e., something that was defined BEFORE the
            // trace call. So we can check predCfgEdges for valid incoming
            // values.
            if (predCfgEdges.count(edge)) {
              Instruction *use_inst =
                dyn_cast<Instruction>(phi->getIncomingValue(i));
              if (use_inst && !visited.count(use_inst)) {
                visited.insert(use_inst);
                worklist.push_back(use_inst);
              }

              BranchInst *branch = dyn_cast<BranchInst>(
                phi->getIncomingBlock(i)->getTerminator());
              if (branch && branch->isConditional() &&
                isa<Instruction>(branch->getCondition()) &&
                !visited.count(cast<Instruction>(branch->getCondition()))) {
                visited.insert(cast<Instruction>(branch->getCondition()));
                worklist.push_back(cast<Instruction>(branch->getCondition()));
              }
            }
          }
        } else {
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

    // Save off initial spill set.
    std::set<Instruction *> initialSpillSet = spillSet;

    // Move looped PHIs without intervening spill to spill set.
    std::vector<PHINode *> phis;

    for (Instruction *inst : rematSet) {
      PHINode *phi = dyn_cast<PHINode>(inst);
      if (!phi)
        continue;
      phis.push_back(phi);
    }

    // Prefer to spill user-named variables, so start with them.
    std::vector<PHINode *> sortedPhis = sortInstructions<PHINode *>(phis);

    std::stable_sort(
      sortedPhis.begin(), sortedPhis.end(), [](PHINode *a, PHINode *b) {
      StringRef aName = a->getName();
      StringRef bName = b->getName();

      bool aIsUserName =
        !aName.empty() && ((aName[0] >= 'a' && aName[0] <= 'z') ||
        (aName[0] >= 'A' && aName[0] <= 'Z'));
      bool bIsUserName =
        !bName.empty() && ((bName[0] >= 'a' && bName[0] <= 'z') ||
        (bName[0] >= 'A' && bName[0] <= 'Z'));

      if (aIsUserName == bIsUserName)
        return false;

      return aIsUserName;
    });

    // We're sorting the phis. Otherwise this step isn't deterministic.
    for (PHINode *phi : sortedPhis) {
      bool loopWithoutSpill = false;

      std::vector<Instruction *> worklist;
      std::set<Instruction *> visited;

      bool first = true;
      worklist.push_back(phi);

      while (!worklist.empty()) {
        Instruction *inst = worklist.back();
        worklist.pop_back();

        if (first) {
          first = false;
        } else {
          if (inst == phi) {
            loopWithoutSpill = true;
            continue;
          }
        }

        if (PHINode *phi = dyn_cast<PHINode>(inst)) {
          for (unsigned i = 0; i < phi->getNumIncomingValues(); ++i) {
            BranchInst *branch =
              dyn_cast<BranchInst>(phi->getIncomingBlock(i)->getTerminator());
            if (!branch || !branch->isConditional())
              continue;

            Instruction *condition =
              dyn_cast<Instruction>(branch->getCondition());

            if (condition && !visited.count(condition)) {
              visited.insert(condition);

              if (rematSet.count(condition))
                worklist.push_back(condition);
            }
          }
        }

        for (unsigned i = 0; i < inst->getNumOperands(); ++i) {
          Instruction *use_inst = dyn_cast<Instruction>(inst->getOperand(i));
          if (!use_inst)
            continue;

          if (!spillSet.count(use_inst) && !rematSet.count(use_inst))
            continue;

          if (!visited.count(use_inst)) {
            visited.insert(use_inst);

            if (rematSet.count(use_inst))
              worklist.push_back(use_inst);
          }
        }
      }

      if (loopWithoutSpill) {
        rematSet.erase(phi);
        spillSet.insert(phi);
      }
    }

    // Backtrack live values to non-remat instructions.
    std::set<Instruction *> nonRematLiveValues;
    {
      std::vector<Instruction *> worklist;
      std::set<Instruction *> visited;

      for (Instruction *inst : liveValues) {
        visited.insert(inst);
        worklist.push_back(inst);
      }

      while (!worklist.empty()) {
        Instruction *inst = worklist.back();
        worklist.pop_back();

        assert(rematSet.count(inst) + spillSet.count(inst) == 1);

        if (spillSet.count(inst)) {
          nonRematLiveValues.insert(inst);
          continue;
        }

        if (PHINode *phi = dyn_cast<PHINode>(inst)) {
          // We can remat PHI if we can reconstruct which BB we came from.
          // This is a live value, i.e., something that was defined BEFORE the
          // trace call. So we can check predCfgEdges.
          unsigned numValidEdges = 0;
          for (unsigned i = 0; i < phi->getNumIncomingValues(); ++i) {
            std::pair<Instruction *, Instruction *> edge =
              std::make_pair(phi->getIncomingBlock(i)->getTerminator(),
                phi->getParent()->begin());

            if (predCfgEdges.count(edge)) {
              ++numValidEdges;
            }
          }

          assert(numValidEdges);

          if (numValidEdges == 1) {
            // Only a single valid edge. The PHI must have one value.
            for (unsigned i = 0; i < phi->getNumIncomingValues(); ++i) {
              std::pair<Instruction *, Instruction *> edge =
                std::make_pair(phi->getIncomingBlock(i)->getTerminator(),
                  phi->getParent()->begin());

              if (predCfgEdges.count(edge)) {
                Instruction *use_inst =
                  dyn_cast<Instruction>(phi->getIncomingValue(i));
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

          // Spill selector for block.
          phiSelectorSpills.insert(phi->getParent());

          // Add dependencies to work list.
          for (unsigned i = 0; i < phi->getNumIncomingValues(); ++i) {
            std::pair<Instruction *, Instruction *> edge =
              std::make_pair(phi->getIncomingBlock(i)->getTerminator(),
                phi->getParent()->begin());

            if (predCfgEdges.count(edge)) {
              Instruction *use_inst =
                dyn_cast<Instruction>(phi->getIncomingValue(i));
              if (!use_inst)
                continue;

              if (visited.count(use_inst))
                continue;

              visited.insert(use_inst);
              worklist.push_back(use_inst);
            }
          }
        } else {
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

    std::vector<Instruction *> &spills = m_spillsPerTraceCall[callSite];

    for (Instruction *inst : nonRematLiveValues)
      spills.push_back(inst);
  }
}

std::string LiveValueAnalysis::translateValueToName( DbgValueInst *DVI, Instruction *I ) {
  
  std::ostringstream NameStr;
  DILocalVariable *Var = DVI->getVariable();
  DILocation *Loc = DVI->getDebugLoc();
  Metadata *VarTypeMD = Var->getRawType();
  std::string VarName = Var->getName();
  std::string OrigName = I->getName();
  int extractElement = 0;
  if (isa<ExtractValueInst>(I)) {
    extractElement = dyn_cast<ExtractValueInst>(I)->getIndices()[0];
    I = dyn_cast<Instruction>(I->getOperand(0));
  }

  NameStr << VarName;

  DIType *VarType = dyn_cast_or_null<DIDerivedType>(VarTypeMD);
  bool isStruct = false;
  if (isa<DIDerivedType>(VarTypeMD)) {
    VarType = dyn_cast_or_null<DIType>(
      dyn_cast<DIDerivedType>(VarType)->getRawBaseType());
    while (llvm::dwarf::DW_TAG_reference_type ==
      VarType->getTag()) {
      VarType = dyn_cast_or_null<DIType>(
        dyn_cast<DIDerivedType>(VarType)->getRawBaseType());
    }
  } else if (isa<DICompositeType>(VarTypeMD)) {
    VarType = dyn_cast<DICompositeType>(VarTypeMD);
    isStruct =
      llvm::dwarf::DW_TAG_structure_type == VarType->getTag();
  } else if (isa<DIBasicType>(VarTypeMD))
    VarType = dyn_cast<DIBasicType>(VarTypeMD);

  assert(VarType);

  // Inspect the llvm variable name for possible element.
  // TODO Find a better/safer solution? One problem is variable
  // names differ between Debug and Release compiles with Release
  // names usually adopting "i.xxxxx" so only the first digit
  // after ".i" can be assumed to correspond to an element index.
  int pos = 0;
  int memberIdx = -1;
  int elementIdx = -1;
  if (isStruct) {
    pos = VarName.find('.', 0);
    // VarName may be the base type and not the original
    // source instruction leading to the live value, so check
    // OrigName as well.
    if (pos < 0) {
      pos = OrigName.find(".i", 0);
      memberIdx = atoi(
        OrigName.substr(pos + 1, OrigName.find(".i", pos + 2))
        .c_str());
    } else {
      memberIdx = atoi(
        VarName.substr(pos + 1, VarName.find('.', pos + 1))
        .c_str());
    }
  }
  pos = VarName.find(".i", 0);
  if (pos >= 0) {
    while ('.' == VarName[pos + 2] && pos > 0) {
      pos = VarName.find(".i", pos + 2);
    }
    elementIdx =
      atoi(VarName.substr(pos + 2, 1).c_str());
  }
  // VarName may be the base type and not the original source
  // instruction leading to the live value, so check OrigName
  // as well.
  else if (pos < 0 && OrigName.length() > 0) {
    pos = OrigName.find(".i", 0);
    if (pos >= 0)
      elementIdx =
      atoi(OrigName.substr(pos + 2, 1).c_str());
  }

  // Type size of 0 indicates a reference.
  if (memberIdx >= 0) {
    DICompositeType *CompType =
      dyn_cast<DICompositeType>(VarTypeMD);
    DIDerivedType *MemberData = dyn_cast<DIDerivedType>(
      CompType->getElements()[memberIdx]);
    NameStr << "." << MemberData->getName().str();
  }

  // If there is no elementIdx found, but this is an extract
  // element instruction we can use that element instead.
  if (elementIdx < 0 && extractElement >= 0)
    elementIdx = extractElement;

  if (elementIdx >= 0) {
    DIDerivedType *MemberData = nullptr;
    DIType *ElementData = nullptr;
    DICompositeType *CompType =
      dyn_cast_or_null<DICompositeType>(VarTypeMD);
    if (CompType) {
      MemberData = dyn_cast<DIDerivedType>(
        CompType->getElements()[memberIdx]);
    } else
      MemberData = dyn_cast_or_null<DIDerivedType>(VarTypeMD);

    if (MemberData) {
      Metadata *MD = MemberData->getRawBaseType();
      ElementData = dyn_cast_or_null<DIDerivedType>(MD);

      if (isa<DIDerivedType>(MD) &&
        isa<DIDerivedType>(
          dyn_cast<DIDerivedType>(ElementData)
          ->getRawBaseType())) {
        DIDerivedType *DT = dyn_cast_or_null<DIDerivedType>(
          dyn_cast<DIDerivedType>(ElementData)
          ->getRawBaseType());
        while (llvm::dwarf::DW_TAG_reference_type ==
          DT->getTag() ||
          llvm::dwarf::DW_TAG_restrict_type ==
          DT->getTag()) {
          DT = dyn_cast<DIDerivedType>(DT->getRawBaseType());
        }
        if (llvm::dwarf::DW_TAG_structure_type == DT->getTag())
          NameStr << "." << DT->getName().str();
        DICompositeType *CompType =
          dyn_cast_or_null<DICompositeType>(
            DT->getRawBaseType());
        if (nullptr != CompType && CompType->getElements()) {
          ElementData = dyn_cast<DIType>(
            CompType->getElements()[elementIdx]);
        } else {
          ElementData = DT;
        }
      } else if (isa<DIDerivedType>(MD) &&
        isa<DICompositeType>(
          dyn_cast<DIDerivedType>(ElementData)
          ->getRawBaseType())) {
        ElementData = dyn_cast<DIType>(
          dyn_cast<DICompositeType>(
            dyn_cast<DIDerivedType>(ElementData)
            ->getRawBaseType())
          ->getElements()[elementIdx]);
        MD = dyn_cast<DIDerivedType>(ElementData)
          ->getRawBaseType();

        if (MD) {
          while (isa<DICompositeType>(MD)) {
            NameStr << "." << ElementData->getName().str();
            ElementData = dyn_cast<DIType>(
              dyn_cast<DICompositeType>(MD)
              ->getElements()[elementIdx]);
            MD = dyn_cast<DIDerivedType>(ElementData)
              ->getRawBaseType();
          }
        }
      } else if (isa<DICompositeType>(MD) &&
        isa<DIType>(dyn_cast<DICompositeType>(MD)
          ->getElements()[elementIdx])) {
        ElementData =
          dyn_cast<DIType>(dyn_cast<DICompositeType>(MD)
            ->getElements()[elementIdx]);

        MD = dyn_cast<DIDerivedType>(ElementData)
          ->getRawBaseType();
        if (MD) {
          while (isa<DICompositeType>(MD)) {
            NameStr << "." << ElementData->getName().str();
            ElementData = dyn_cast<DIType>(
              dyn_cast<DICompositeType>(MD)
              ->getElements()[elementIdx]);
            MD = dyn_cast<DIDerivedType>(ElementData)
              ->getRawBaseType();
          }
        }
      }
    } else {
      if (isa<DIBasicType>(VarTypeMD))
        ElementData = nullptr;
    }
    if (ElementData)
      NameStr << "." << ElementData->getName().str();
  }

  NameStr << " (" << VarType->getName().str();
  NameStr << "): "
    << formatSourceLocation(m_rootPath,
      Loc->getFilename().str(),
      Loc->getLine())
    << "\n";

  return NameStr.str();
}

void LiveValueAnalysis::formatOutput(raw_string_ostream &PrettyStr, raw_string_ostream &VSStr) {

  for (CallInst *callSite : sortInstructions<CallInst *>(m_callSites)) {
    std::ostringstream Spacer;
    {
      if (const llvm::DebugLoc &debugInfo = callSite->getDebugLoc()) {
        std::string filePath = debugInfo->getFilename();
        int line = debugInfo->getLine();
        std::string fullPath =
          formatSourceLocation(m_rootPath, filePath, line);
        PrettyStr << "Live State for TRACE call at " << fullPath << "\n";
        VSStr << "Live State for TRACE call at " << fullPath << "\n";
        Spacer << std::setfill('-') << std::setw(fullPath.length() + 30)
          << std::right << "\n";
        PrettyStr << Spacer.str();
        VSStr << Spacer.str();
      } else {
        break;
      }
    }
    size_t regs = 0;
    size_t detected = 0;
    std::vector<Instruction*> DetectedInstr;
    for (Instruction *I : m_spillsPerTraceCall[callSite]) {
      // Only count the values for which metadata exists.
      // TODO: Report count of values for those which are not used metadata?
      // this can lead to missing live values in some cases.
      if (isa<ExtractValueInst>(I)) {
        I = dyn_cast<Instruction>(I->getOperand(0));
      }
      if (I->isUsedByMetadata()) {
        regs++;
      }
      else {
        DetectedInstr.push_back(I);
      }
      detected++;
    }
    if( detected != regs )
    {
      PrettyStr << "Detected " << (int)detected << " live values but only " << (int)regs << " are used by metadata\n";
      VSStr << "Detected " << (int)detected << " live values but only " << (int)regs << " are used by metadata\n";
      for( auto I : DetectedInstr ) {
        I->print( PrettyStr );
        I->print( VSStr );
        PrettyStr << "\n";
        VSStr << "\n";
      }
    }
    if (const llvm::DebugLoc &debugInfo = callSite->getDebugLoc()) {
      PrettyStr << "Total 32-bit registers: " << (int)regs << "\n";
      VSStr << "Total 32-bit registers: " << (int)regs << "\n";
    }

    std::ostringstream TmpStr;
    TmpStr << "--LIVE VALUES" << std::setfill('-')
      << std::setw(Spacer.str().length() - 13) << std::right << "\n";
    PrettyStr << TmpStr.str();
    VSStr << TmpStr.str();
    for (Instruction *I :
      sortInstructions<Instruction *>(m_spillsPerTraceCall[callSite])) {
      TmpStr.str("");
      {
        if (isa<ExtractValueInst>(I)) {
          I = dyn_cast<Instruction>(I->getOperand(0));
        }

        if (I->isUsedByMetadata()) {
          unsigned int totalLineWidth = 0;

          if (auto *L = LocalAsMetadata::getIfExists(I)) {
            if (auto *MDV = MetadataAsValue::getIfExists(I->getContext(), L)) {
              for (User *U : MDV->users()) {
                if (DbgValueInst *DVI = dyn_cast<DbgValueInst>(U)) {

                  TmpStr << translateValueToName( DVI, I );

                  // Output USE locations
                  std::vector<PHINode *> Phis;
                  std::set<PHINode *> VisitedPhis;
                  std::set<DILocation *> UseLocations;
                  while (1) {
                    for (User *U : I->users()) {
                      if (auto Inst = dyn_cast<Instruction>(U)) {
                        if (DILocation *Loc = Inst->getDebugLoc()) {
                          // Manually search for duplication location;
                          // DILocations can be distinct but their source
                          // location is not
                          bool bFound = false;
                          DIScope *scope = Loc->getScope();
                          for (auto L : UseLocations) {
                            if (L->getScope() == scope) {
                              if (L->getLine() == Loc->getLine() &&
                                L->getColumn() == Loc->getColumn()) {
                                bFound = true;
                                break;
                              }
                            }
                          }

                          if (!bFound) {
                            UseLocations.insert(Loc);

                            // Traverse each control flow edge separately and
                            // gather up seen defs along the way. By keeping
                            // track of the from-block, we can select the
                            // right incoming value at PHIs.
                            {
                              Instruction *inst = callSite->getNextNode();

                              std::set<Instruction *> defsSeen;
                              defsSeen.insert(inst);

                              BasicBlock *fromBlock = nullptr;
                              std::set<std::pair<Instruction *, Instruction *>>
                                visitedEdges;
                              std::set<Value *> conditionsKnownTrue;
                              std::set<Value *> conditionsKnownFalse;
                              std::vector<
                                std::tuple<BasicBlock *, Instruction *,
                                std::set<Instruction *>>>
                                worklist;

                              for (;;) {

                                if (isa<DbgValueInst>(inst)) {
                                  inst = inst->getNextNode();
                                  continue;
                                }

                                if (inst == Inst) {
                                  // Found the use location.
                                  break;
                                }

                                if (TerminatorInst *terminator =
                                  dyn_cast<TerminatorInst>(inst)) {
                                  BranchInst *branch =
                                    dyn_cast<BranchInst>(terminator);

                                  if (branch && branch->isConditional()) {
                                    // Conditional branch. Check against known
                                    // conditions.
                                    assert(branch->getNumSuccessors() == 2);

                                    Instruction *taken =
                                      branch->getSuccessor(0)->begin();
                                    Instruction *notTaken =
                                      branch->getSuccessor(1)->begin();

                                    std::pair<Instruction *, Instruction *>
                                      takenEdge =
                                      std::make_pair(branch, taken);
                                    std::pair<Instruction *, Instruction *>
                                      notTakenEdge =
                                      std::make_pair(branch, notTaken);

                                    if (!conditionsKnownFalse.count(
                                      branch->getCondition())) {
                                      if (!visitedEdges.count(takenEdge)) {
                                        visitedEdges.insert(takenEdge);
                                        worklist.push_back(
                                          std::make_tuple(inst->getParent(),
                                            taken, defsSeen));
                                      }
                                    }

                                    if (!conditionsKnownTrue.count(
                                      branch->getCondition())) {
                                      if (!visitedEdges.count(notTakenEdge)) {
                                        visitedEdges.insert(notTakenEdge);
                                        worklist.push_back(std::make_tuple(
                                          inst->getParent(), notTaken,
                                          defsSeen));
                                      }
                                    }
                                  } else {
                                    for (unsigned
                                      i = 0,
                                      e = terminator->getNumSuccessors();
                                      i < e; ++i) {
                                      Instruction *successor =
                                        terminator->getSuccessor(i)->begin();
                                      std::pair<Instruction *, Instruction *>
                                        edge =
                                        std::make_pair(inst, successor);

                                      if (!visitedEdges.count(edge)) {
                                        visitedEdges.insert(edge);
                                        worklist.push_back(std::make_tuple(
                                          inst->getParent(), successor,
                                          defsSeen));
                                      }
                                    }
                                  }

                                  if (worklist.empty())
                                    break;

                                  fromBlock = std::get<0>(worklist.back());
                                  inst = std::get<1>(worklist.back());
                                  defsSeen =
                                    std::move(std::get<2>(worklist.back()));
                                  worklist.pop_back();
                                  continue;
                                }

                                inst = inst->getNextNode();
                              }
                            }
                          }
                        } else {
                          if (isa<PHINode>(U)) {
                            PHINode *NewPhi = dyn_cast<PHINode>(U);
                            if (VisitedPhis.insert(NewPhi).second) {
                              Phis.push_back(NewPhi);
                            }
                          }
                        }
                      }
                    }

                    if (Phis.empty()) {
                      break;
                    } else {
                      I = Phis.back();
                      Phis.pop_back();
                    }
                  }
                  TmpStr << "Use Locations:\n";
                  PrettyStr << TmpStr.str();
                  VSStr << TmpStr.str();

                  // Find max path length to format horizontal dividers in the output.
                  unsigned int maxPathLength = 0;
                  unsigned int maxFuncLength = 0;
                  for (auto Loc : UseLocations) {
                    std::string Path =
                      formatSourceLocation(m_rootPath, Loc->getFilename().str(),
                        Loc->getLine());
                    if (Path.length() > maxPathLength)
                      maxPathLength = Path.length();
                    if (Loc->getScope()
                      ->getSubprogram()
                      ->getName()
                      .str()
                      .length() > maxFuncLength)
                      maxFuncLength = Loc->getScope()
                      ->getSubprogram()
                      ->getName()
                      .str()
                      .length();
                    if (DILocation *InLoc = Loc->getInlinedAt()) {
                      std::string Path = formatSourceLocation(
                        m_rootPath, InLoc->getFilename().str(),
                        InLoc->getLine());
                      if (Path.length() > maxPathLength)
                        maxPathLength = Path.length();
                      if (InLoc->getScope()
                        ->getSubprogram()
                        ->getName()
                        .str()
                        .length() > maxFuncLength)
                        maxFuncLength = InLoc->getScope()
                        ->getSubprogram()
                        ->getName()
                        .str()
                        .length();
                      while (DILocation *NestedInLoc = InLoc->getInlinedAt()) {
                        InLoc = NestedInLoc;
                        std::string Path = formatSourceLocation(
                          m_rootPath, InLoc->getFilename().str(),
                          InLoc->getLine());
                        if (Path.length() > maxPathLength)
                          maxPathLength = Path.length();
                        if (InLoc->getScope()
                          ->getSubprogram()
                          ->getName()
                          .str()
                          .length() > maxFuncLength)
                          maxFuncLength = InLoc->getScope()
                          ->getSubprogram()
                          ->getName()
                          .str()
                          .length();
                      }
                    }
                  }
                  // Pad with spaces for inline text, etc
                  maxFuncLength += 2;
                  maxPathLength += 19;
                  totalLineWidth = maxFuncLength + maxPathLength;
                  // Output use locations
                  std::ostringstream LocationStr;
                  std::string FileName;
                  std::string FuncName;

                  // Output PrettyPrint version
                  for (auto Loc : UseLocations) {
                    FileName =
                      formatSourceLocation(m_rootPath, Loc->getFilename().str(),
                        Loc->getLine());
                    FuncName =
                      "(" +
                      Loc->getScope()->getSubprogram()->getName().str() + ")";
                    LocationStr << std::setfill('_') << std::setw(maxPathLength)
                      << std::left << FileName
                      << std::setw(maxFuncLength) << std::right
                      << FuncName << "\n";
                    if (DILocation *InLoc = Loc->getInlinedAt()) {
                      FileName = "  -->inlined at " +
                        formatSourceLocation(
                          m_rootPath, InLoc->getFilename().str(),
                          InLoc->getLine());
                      FuncName =
                        "(" +
                        InLoc->getScope()->getSubprogram()->getName().str() +
                        ")";
                      LocationStr << std::setfill('_')
                        << std::setw(maxPathLength) << std::left
                        << FileName << std::setw(maxFuncLength)
                        << std::right << FuncName << "\n";
                      while (DILocation *NestedInLoc = InLoc->getInlinedAt()) {
                        FileName =
                          "  -->inlined at " +
                          formatSourceLocation(
                            m_rootPath, NestedInLoc->getFilename().str(),
                            NestedInLoc->getLine());
                        FuncName = "(" +
                          NestedInLoc->getScope()
                          ->getSubprogram()
                          ->getName()
                          .str() +
                          ")";
                        LocationStr << std::setfill('_')
                          << std::setw(maxPathLength) << std::left
                          << FileName << std::setw(maxFuncLength)
                          << std::right << FuncName << "\n";
                        InLoc = NestedInLoc;
                      }
                    }
                  }
                  PrettyStr << LocationStr.str();
                  LocationStr.str("");
                  // Output VS source linking version
                  for (auto Loc : UseLocations) {
                    FileName =
                      formatSourceLocation(m_rootPath, Loc->getFilename().str(),
                        Loc->getLine());
                    LocationStr << FileName << ": (" + Loc->getScope()->getSubprogram()->getName().str() + ")\n";
                    if (DILocation *InLoc = Loc->getInlinedAt()) {
                      LocationStr << "inlined at:\n";
                      FileName = formatSourceLocation( m_rootPath, InLoc->getFilename().str(), InLoc->getLine());
                      LocationStr << ">" + FileName << ": (" + InLoc->getScope()->getSubprogram()->getName().str() + ")\n";
                      while (DILocation *NestedInLoc = InLoc->getInlinedAt()) {
                        FileName = formatSourceLocation(m_rootPath, NestedInLoc->getFilename().str(), NestedInLoc->getLine());
                        LocationStr << ">" + FileName << ": (" + InLoc->getScope()->getSubprogram()->getName().str() + ")\n";
                        InLoc = NestedInLoc;
                      }
                    }
                  }
                  VSStr << LocationStr.str();
                }
                // Found the debug info for the live value, now exit the loop.
                break;
              }
              std::ostringstream Spacer;
              Spacer << std::setfill('-') << std::setw(totalLineWidth + 1)
                << std::right << "\n";
              PrettyStr << Spacer.str();
              VSStr << Spacer.str();
            }
          }
        }
      }
    }
  }
}



