
#include "llvm/Pass.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/Transforms/Utils/UnrollLoop.h"
#include "llvm/Support/Debug.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/PredIteratorCache.h"
#include "llvm/Analysis/InstructionSimplify.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/SSAUpdater.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/Transforms/Utils/PromoteMemToReg.h"
#include "llvm/Analysis/AssumptionCache.h"

#include "dxc/DXIL/DxilUtil.h"
#include "dxc/Support/Global.h"
#include "dxc/DXIL/DxilOperations.h"
#include "dxc/DXIL/DxilModule.h"
#include "dxc/HLSL/HLModule.h"
#include "dxc/DXIL/DxilInstructions.h"

#include <set>
#include <unordered_map>

using namespace llvm;
using namespace hlsl;

namespace {

static std::string GetBlockName(BasicBlock *BB) {
  return BB->getName();
}

template<typename T>
static std::string DumpValue(T *V) {
  std::string Val;
  raw_string_ostream OS(Val);
  OS << *V;
  OS.flush();
  return Val;
}

// Replace this with the stock llvm one.
static void Unroll_RemapInstruction(Instruction *I,
                                    ValueToValueMapTy &VMap) {
  for (unsigned op = 0, E = I->getNumOperands(); op != E; ++op) {
    Value *Op = I->getOperand(op);
    ValueToValueMapTy::iterator It = VMap.find(Op);
    if (It != VMap.end())
      I->setOperand(op, It->second);
  }

  if (PHINode *PN = dyn_cast<PHINode>(I)) {
    for (unsigned i = 0, e = PN->getNumIncomingValues(); i != e; ++i) {
      ValueToValueMapTy::iterator It = VMap.find(PN->getIncomingBlock(i));
      if (It != VMap.end())
        PN->setIncomingBlock(i, cast<BasicBlock>(It->second));
    }
  }
}


class DxilLoopUnroll : public LoopPass {
public:
  static char ID;
  LoopInfo *LI = nullptr;
  bool CleandupAllocas = false;
  DxilLoopUnroll() : LoopPass(ID) {}
  bool runOnLoop(Loop *L, LPPassManager &LPM) override;
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    //AU.addRequired<AssumptionCacheTracker>();
    AU.addRequired<LoopInfoWrapperPass>();
    AU.addPreserved<LoopInfoWrapperPass>();
    AU.addRequiredID(LoopSimplifyID);
    AU.addRequired<AssumptionCacheTracker>();

    AU.addRequired<DominatorTreeWrapperPass>();
    //AU.addPreservedID(LoopSimplifyID);
    //AU.addRequiredID(LCSSAID);
    //AU.addPreservedID(LCSSAID);
    //AU.addRequired<ScalarEvolution>();
    //AU.addPreserved<ScalarEvolution>();
    //AU.addRequired<TargetTransformInfoWrapperPass>();
    // FIXME: Loop unroll requires LCSSA. And LCSSA requires dom info.
    // If loop unroll does not preserve dom info then LCSSA pass on next
    // loop will receive invalid dom info.
    // For now, recreate dom info, if loop is unrolled.
    //AU.addPreserved<DominatorTreeWrapperPass>();
  }
};

char DxilLoopUnroll::ID;

static bool SimplifyPHIs(BasicBlock *BB) {
  bool Changed = false;
  SmallVector<Instruction *, 16> Removed;
  for (Instruction &I : *BB) {
    PHINode *PN = dyn_cast<PHINode>(&I);
    if (!PN)
      continue;

    if (PN->getNumIncomingValues() == 1) {
      Value *V = PN->getIncomingValue(0);
      PN->replaceAllUsesWith(V);
      Removed.push_back(PN);
      Changed = true;
    }
  }

  for (Instruction *I : Removed)
    I->eraseFromParent();

  return Changed;
}

static void FindAllDataDependency(Instruction *I, const SetVector<BasicBlock *> &Blocks, SetVector<Instruction *> &Dependencies) {
  for (User *U : I->users()) {
    if (Instruction *UserI = dyn_cast<Instruction>(U)) {
      if (Dependencies.count(UserI))
        continue;
      if (!Blocks.count(UserI->getParent()))
        continue;
      Dependencies.insert(UserI);
      FindAllDataDependency(UserI, Blocks, Dependencies);
    }
  }
}

static void FindAllDataDependency(Instruction *I, std::set<Instruction *> &Set, std::set<BasicBlock *> &Blocks) {
  for (User *U : I->users()) {
    if (PHINode *PN = dyn_cast<PHINode>(U)) {
      continue;
    }
    else if (Instruction *UserI = dyn_cast<Instruction>(U)) {
      if (!Set.count(UserI)) {
        Set.insert(UserI);
        Blocks.insert(UserI->getParent());
        FindAllDataDependency(UserI, Set, Blocks);
      }
    }
  }
}

struct ClonedIteration {
  SmallVector<BasicBlock *, 16> Body;
  BasicBlock *Latch = nullptr;
  BasicBlock *Header = nullptr;
  ValueToValueMapTy VarMap;
  SetVector<BasicBlock *> Extended;

  ClonedIteration(const ClonedIteration &o) {
    Body = o.Body;
    Latch = o.Latch;
    Header = o.Header;
    Extended = o.Extended;
    for (ValueToValueMapTy::const_iterator It = o.VarMap.begin(), End = o.VarMap.end(); It != End; It++)
      VarMap[It->first] = It->second;
  }
  ClonedIteration() {}
};

static void ReplaceUsersIn(BasicBlock *BB, Value *Old, Value *New) {
  SmallVector<Use *, 16> Uses;
  for (Use &U : Old->uses()) {
    if (Instruction *I = dyn_cast<Instruction>(U.getUser())) {
      if (I->getParent() == BB) {
        Uses.push_back(&U);
      }
    }
  }
  
  for (Use *U : Uses) {
    U->set(New);
  }
}

static bool IsConstantI1(Value *V, bool *Val=nullptr) {
  if (ConstantInt *C = dyn_cast<ConstantInt>(V)) {
    if (V->getType() == Type::getInt1Ty(V->getContext())) {
      if (Val)
        *Val = (bool)C->getLimitedValue();
      return true;
    }
  }
  return false;
}

// Figure out what to do with this.
static bool SimplifyInstructionsInBlock_NoDelete(BasicBlock *BB,
                                       const TargetLibraryInfo *TLI) {
  bool MadeChange = false;

#ifndef NDEBUG
  // In debug builds, ensure that the terminator of the block is never replaced
  // or deleted by these simplifications. The idea of simplification is that it
  // cannot introduce new instructions, and there is no way to replace the
  // terminator of a block without introducing a new instruction.
  AssertingVH<Instruction> TerminatorVH(--BB->end());
#endif

  for (BasicBlock::iterator BI = BB->begin(), E = --BB->end(); BI != E; ) {
    assert(!BI->isTerminator());
    Instruction *Inst = BI++;

    WeakVH BIHandle(BI);
    if (recursivelySimplifyInstruction(Inst, TLI)) {
      MadeChange = true;
      if (BIHandle != BI)
        BI = BB->begin();
      continue;
    }

//    MadeChange |= RecursivelyDeleteTriviallyDeadInstructions(Inst, TLI);
    if (BIHandle != BI)
      BI = BB->begin();
  }
  return MadeChange;
}

static bool IsLoopInvariant(Value *V, Loop *L) {
  if (L->isLoopInvariant(V)) {
    return true;
  }
  if (PHINode *PN = dyn_cast<PHINode>(V)) {
    if (PN->getNumIncomingValues() == 0) {
      return IsLoopInvariant(PN->getIncomingValue(0), L);
    }
  }
  return false;
}

static bool HasUnrollElements(BasicBlock *BB, Loop *L) {
  for (Instruction &I : *BB) {
    if (LoadInst *Load = dyn_cast<LoadInst>(&I)) {
      Value *PtrV = Load->getPointerOperand();
      if (hlsl::dxilutil::IsHLSLObjectType(PtrV->getType()->getPointerElementType())) {
        if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(PtrV)) {
          if (GEP->hasAllConstantIndices())
            continue;
          for (auto It = GEP->idx_begin(); It != GEP->idx_end(); It++) {
            Value *Idx = *It;
            if (!IsLoopInvariant(Idx, L)) {
              return true;
            }
          }
        }
      }
    }
  }
  return false;
}

bool IsMarkedFullUnroll(Loop *L) {
  if (MDNode *LoopID = L->getLoopID())
    return GetUnrollMetadata(LoopID, "llvm.loop.unroll.full");
  return false;
}

static bool HeuristicallyDetermineUnrollNecessary(Loop *L) {
  for (BasicBlock *BB : L->getBlocks()) {
    if (HasUnrollElements(BB, L))
      return true;
  }
  SmallVector<BasicBlock *, 16> ExitBlocks;
  L->getExitBlocks(ExitBlocks);
  for (BasicBlock *BB : ExitBlocks)
    if (HasUnrollElements(BB, L))
      return true;
  return false;
}

static bool HasSuccessorsInLoop(BasicBlock *BB, Loop *L) {
  bool PartOfOuterLoop = false;
  for (BasicBlock *Succ : successors(BB)) {
    if (L->contains(Succ)) {
      return true;
    }
  }
  return false;
}

static void DetachFromSuccessors(BasicBlock *BB) {
  SmallVector<BasicBlock *, 16> Successors(succ_begin(BB), succ_end(BB));
  for (BasicBlock *Succ : Successors) {
    Succ->removePredecessor(BB);
  }
}

/// Return true if the specified block is in the list.
static bool isExitBlock(BasicBlock *BB,
                        const SmallVectorImpl<BasicBlock *> &ExitBlocks) {
  for (unsigned i = 0, e = ExitBlocks.size(); i != e; ++i)
    if (ExitBlocks[i] == BB)
      return true;
  return false;
}

static bool processInstruction(SetVector<BasicBlock *> &Body, Loop &L, Instruction &Inst, DominatorTree &DT, // HLSL Change
                               const SmallVectorImpl<BasicBlock *> &ExitBlocks,
                               PredIteratorCache &PredCache, LoopInfo *LI) {

  SmallVector<Use *, 16> UsesToRewrite;

  BasicBlock *InstBB = Inst.getParent();

  for (Use &U : Inst.uses()) {
    Instruction *User = cast<Instruction>(U.getUser());
    BasicBlock *UserBB = User->getParent();
    if (PHINode *PN = dyn_cast<PHINode>(User))
      UserBB = PN->getIncomingBlock(U);

    if (InstBB != UserBB && /*!L.contains(UserBB)*/!Body.count(UserBB)) // HLSL Change
      UsesToRewrite.push_back(&U);
  }

  // If there are no uses outside the loop, exit with no change.
  if (UsesToRewrite.empty())
    return false;
#if 0
  ++NumLCSSA; // We are applying the transformation
#endif
  // Invoke instructions are special in that their result value is not available
  // along their unwind edge. The code below tests to see whether DomBB
  // dominates
  // the value, so adjust DomBB to the normal destination block, which is
  // effectively where the value is first usable.
  BasicBlock *DomBB = Inst.getParent();
  if (InvokeInst *Inv = dyn_cast<InvokeInst>(&Inst))
    DomBB = Inv->getNormalDest();

  DomTreeNode *DomNode = DT.getNode(DomBB);

  SmallVector<PHINode *, 16> AddedPHIs;
  SmallVector<PHINode *, 8> PostProcessPHIs;

  SSAUpdater SSAUpdate;
  SSAUpdate.Initialize(Inst.getType(), Inst.getName());

  // Insert the LCSSA phi's into all of the exit blocks dominated by the
  // value, and add them to the Phi's map.
  for (SmallVectorImpl<BasicBlock *>::const_iterator BBI = ExitBlocks.begin(),
                                                     BBE = ExitBlocks.end();
       BBI != BBE; ++BBI) {
    BasicBlock *ExitBB = *BBI;
    if (!DT.dominates(DomNode, DT.getNode(ExitBB)))
      continue;

    // If we already inserted something for this BB, don't reprocess it.
    if (SSAUpdate.HasValueForBlock(ExitBB))
      continue;

    PHINode *PN = PHINode::Create(Inst.getType(), PredCache.size(ExitBB),
                                  Inst.getName() + ".lcssa", ExitBB->begin());

    // Add inputs from inside the loop for this PHI.
    for (BasicBlock *Pred : PredCache.get(ExitBB)) {
      PN->addIncoming(&Inst, Pred);

      // If the exit block has a predecessor not within the loop, arrange for
      // the incoming value use corresponding to that predecessor to be
      // rewritten in terms of a different LCSSA PHI.
      if (/*!L.contains(Pred)*/ !Body.count(Pred)) // HLSL Change
        UsesToRewrite.push_back(
            &PN->getOperandUse(PN->getOperandNumForIncomingValue(
                 PN->getNumIncomingValues() - 1)));
    }

    AddedPHIs.push_back(PN);

    // Remember that this phi makes the value alive in this block.
    SSAUpdate.AddAvailableValue(ExitBB, PN);

    // LoopSimplify might fail to simplify some loops (e.g. when indirect
    // branches are involved). In such situations, it might happen that an exit
    // for Loop L1 is the header of a disjoint Loop L2. Thus, when we create
    // PHIs in such an exit block, we are also inserting PHIs into L2's header.
    // This could break LCSSA form for L2 because these inserted PHIs can also
    // have uses outside of L2. Remember all PHIs in such situation as to
    // revisit than later on. FIXME: Remove this if indirectbr support into
    // LoopSimplify gets improved.
    if (auto *OtherLoop = LI->getLoopFor(ExitBB))
      if (!L.contains(OtherLoop))
        PostProcessPHIs.push_back(PN);
  }

  // Rewrite all uses outside the loop in terms of the new PHIs we just
  // inserted.
  for (unsigned i = 0, e = UsesToRewrite.size(); i != e; ++i) {
    // If this use is in an exit block, rewrite to use the newly inserted PHI.
    // This is required for correctness because SSAUpdate doesn't handle uses in
    // the same block.  It assumes the PHI we inserted is at the end of the
    // block.
    Instruction *User = cast<Instruction>(UsesToRewrite[i]->getUser());
    BasicBlock *UserBB = User->getParent();
    if (PHINode *PN = dyn_cast<PHINode>(User))
      UserBB = PN->getIncomingBlock(*UsesToRewrite[i]);

    if (isa<PHINode>(UserBB->begin()) && isExitBlock(UserBB, ExitBlocks)) {
      // Tell the VHs that the uses changed. This updates SCEV's caches.
      if (UsesToRewrite[i]->get()->hasValueHandle())
        ValueHandleBase::ValueIsRAUWd(*UsesToRewrite[i], UserBB->begin());
      UsesToRewrite[i]->set(UserBB->begin());
      continue;
    }

    // Otherwise, do full PHI insertion.
    SSAUpdate.RewriteUse(*UsesToRewrite[i]);
  }

  // Post process PHI instructions that were inserted into another disjoint loop
  // and update their exits properly.
  for (auto *I : PostProcessPHIs) {
    if (I->use_empty())
      continue;

    BasicBlock *PHIBB = I->getParent();
    Loop *OtherLoop = LI->getLoopFor(PHIBB);
    SmallVector<BasicBlock *, 8> EBs;
    OtherLoop->getExitBlocks(EBs);
    if (EBs.empty())
      continue;

    // Recurse and re-process each PHI instruction. FIXME: we should really
    // convert this entire thing to a worklist approach where we process a
    // vector of instructions...
    processInstruction(Body, *OtherLoop, *I, DT, EBs, PredCache, LI);
  }

  // Remove PHI nodes that did not have any uses rewritten.
  for (unsigned i = 0, e = AddedPHIs.size(); i != e; ++i) {
    if (AddedPHIs[i]->use_empty())
      AddedPHIs[i]->eraseFromParent();
  }

  return true;

}

static bool blockDominatesAnExit(BasicBlock *BB,
                     DominatorTree &DT,
                     const SmallVectorImpl<BasicBlock *> &ExitBlocks) {
  DomTreeNode *DomNode = DT.getNode(BB);
  for (BasicBlock *Exit : ExitBlocks)
    if (DT.dominates(DomNode, DT.getNode(Exit)))
      return true;
  return false;
};

// We need to recreate the LCSSA form since our loop boundary is potentially different from
// the canonical one.
static bool CreateLCSSA(SetVector<BasicBlock *> &Body, SetVector<BasicBlock *> &ExitBlockSet, Loop *L, DominatorTree &DT, LoopInfo *LI) {
  /*
  std::set<BasicBlock *> ExitBlockSet;
  for (BasicBlock *BB : Body) {
    for (BasicBlock *Succ : successors(BB)) {
      if (!Body.count(Succ)) {
        ExitBlockSet.insert(Succ);
      }
    }
  }*/
  SmallVector<BasicBlock *, 4> ExitBlocks(ExitBlockSet.begin(), ExitBlockSet.end());

  PredIteratorCache PredCache;
  bool Changed = false;
  // Look at all the instructions in the loop, checking to see if they have uses
  // outside the loop.  If so, rewrite those uses.
  for (SetVector<BasicBlock *>::iterator BBI = Body.begin(), BBE = Body.end();
       BBI != BBE; ++BBI) {
    BasicBlock *BB = *BBI;

    // For large loops, avoid use-scanning by using dominance information:  In
    // particular, if a block does not dominate any of the loop exits, then none
    // of the values defined in the block could be used outside the loop.
    if (!blockDominatesAnExit(BB, DT, ExitBlocks))
      continue;

    for (BasicBlock::iterator I = BB->begin(), E = BB->end(); I != E; ++I) {
      // Reject two common cases fast: instructions with no uses (like stores)
      // and instructions with one use that is in the same block as this.
      if (I->use_empty() ||
          (I->hasOneUse() && I->user_back()->getParent() == BB &&
           !isa<PHINode>(I->user_back())))
        continue;

      Instruction *Inst = &*I;
      Changed |= processInstruction(Body, *L, *I, DT, ExitBlocks, PredCache, LI);
    }
  }

  return Changed;
}

static bool IsInExitBlocks(Instruction *I, SmallVectorImpl<BasicBlock *> &Exits) {
  for (BasicBlock *BB : Exits)
    if (I->getParent() == BB)
      return true;
  return false;
}

static void FindAllocas(Value *V, SetVector<AllocaInst *> &Insts) {
  if (AllocaInst *AI = dyn_cast<AllocaInst>(V)) {
    Insts.insert(AI);
  }
  else if (GEPOperator *GEP = dyn_cast<GEPOperator>(V)) {
    FindAllocas(GEP->getPointerOperand(), Insts);
  }
  else if (StoreInst *StoreI = dyn_cast<StoreInst>(V)) {
    FindAllocas(StoreI->getPointerOperand(), Insts);
  }
  else if (LoadInst *LoadI = dyn_cast<LoadInst>(V)) {
    FindAllocas(LoadI->getPointerOperand(), Insts);
  }
}

static Instruction *GetNonConstIdx(Value *V) {
  if (PHINode *PN = dyn_cast<PHINode>(V)) {
    if (PN->getNumIncomingValues() == 1) {
      return GetNonConstIdx(PN->getIncomingValue(0));
    }
    return PN;
  }
  else if (Instruction *I = dyn_cast<Instruction>(V)) {
    return I;
  }
  return nullptr;
}

static bool IsProblemBlock(BasicBlock *BB, const SetVector<Instruction *> &LoopDependencies) {
  for (Instruction &I : *BB) {
    if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(&I)) {
      for (auto IdxIt = GEP->idx_begin(); IdxIt != GEP->idx_end(); IdxIt++) {
        Value *Idx = *IdxIt;
        if (Instruction *NonConstIdx = GetNonConstIdx(Idx)) {
          if (LoopDependencies.count(NonConstIdx))
            return true;
        }
      }
    }
  }
  return false;
}

static void FindProblemUsers(BasicBlock *Header, const SetVector<BasicBlock *> &BlocksInLoop, SetVector<BasicBlock *> &ProblemBlocks) {
  SetVector<Instruction *> LoopDependencies;
  SmallVector<PHINode *, 8> HeaderPHI;
  for (Instruction &I : *Header) {
    PHINode *PN = dyn_cast<PHINode>(&I);
    if (!PN)
      break;
    FindAllDataDependency(PN, BlocksInLoop, LoopDependencies);
  }

  for (BasicBlock *BB : BlocksInLoop) {
    if (IsProblemBlock(BB, LoopDependencies))
      ProblemBlocks.insert(BB);
  }
}

bool DxilLoopUnroll::runOnLoop(Loop *L, LPPassManager &LPM) {
  Module *M = L->getHeader()->getModule();

  if (!CleandupAllocas) {
    CleandupAllocas = true;

    std::vector<AllocaInst*> Allocas;
    Function &F = *L->getBlocks().front()->getParent();

    BasicBlock &BB = F.getEntryBlock();  // Get the entry node for the function

    bool Changed  = false;

    DominatorTree &DT = getAnalysis<DominatorTreeWrapperPass>().getDomTree();
    AssumptionCache &AC =
        getAnalysis<AssumptionCacheTracker>().getAssumptionCache(F);

    while (1) {
      Allocas.clear();

      // Find allocas that are safe to promote, by looking at all instructions in
      // the entry node
      for (BasicBlock::iterator I = BB.begin(), E = --BB.end(); I != E; ++I)
        if (AllocaInst *AI = dyn_cast<AllocaInst>(I))       // Is it an alloca?
          if (isAllocaPromotable(AI) && !HLModule::HasPreciseAttributeWithMetadata(AI))
            Allocas.push_back(AI);

      if (Allocas.empty()) break;

      PromoteMemToReg(Allocas, DT, nullptr, &AC);
      //NumPromoted += Allocas.size();
      Changed = true;
    }
  }

  if (!L->isSafeToClone())
    return false;

  BasicBlock *Latch = L->getLoopLatch();
  BasicBlock *Header = L->getHeader();
  BasicBlock *Predecessor = L->getLoopPredecessor();

  // Quit if we don't have a single latch block or predecessor
  if (!Latch || !Predecessor)
    return false;

  // If the loop exit condition is not in the latch, then the loop is not rotated. Give up.
  if (!cast<BranchInst>(Latch->getTerminator())->isConditional())
    return false;

  SmallVector<BasicBlock *, 16> ExitBlocks;
  L->getExitBlocks(ExitBlocks);
  SetVector<BasicBlock *> ExitBlockSet(ExitBlocks.begin(), ExitBlocks.end());

  SmallVector<BasicBlock *, 16> BlocksInLoop(L->getBlocks().begin(), L->getBlocks().end());
  BlocksInLoop.append(ExitBlocks.begin(), ExitBlocks.end());
  SetVector<BasicBlock *> FullLoopSet(BlocksInLoop.begin(), BlocksInLoop.end());

  // Simplify the PHI nodes that have single incoming value. The original LCSSA form
  // (if exists) does not necessarily work for our unroll because we may be unrolling
  // from a different boundary.
  for (BasicBlock *BB : BlocksInLoop)
    SimplifyPHIs(BB);

  // Heuristically find blocks that likely need to be unrolled
  SetVector<BasicBlock *> ProblemBlocks;
  FindProblemUsers(L->getHeader(), FullLoopSet, ProblemBlocks);

  if (!IsMarkedFullUnroll(L) && !ProblemBlocks.size())
    return false;

  LoopInfo *LI = &getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
  DominatorTree *DT = &getAnalysis<DominatorTreeWrapperPass>().getDomTree(); // TODO: Update the Dom tree
  Function *F = L->getBlocks()[0]->getParent();

  // Keep track of the PHI nodes at the header.
  SmallVector<PHINode *, 16> PHIs;
  for (auto it = Header->begin(); it != Header->end(); it++) {
    if (PHINode *PN = dyn_cast<PHINode>(it)) {
      if (PN->getNumIncomingValues() != 2)
        return false;
      PHIs.push_back(PN);
    }
    else {
      break;
    }
  }

  SetVector<BasicBlock *> ToBeCloned;
  for (BasicBlock *BB : L->getBlocks())
    ToBeCloned.insert(BB);

  SetVector<BasicBlock *> NewExits; // We need to define a new set of exit blocks as boundaries for LCSSA
  SetVector<BasicBlock *> FakeExits; // Set of blocks created to allow cloning original exit blocks.
  for (BasicBlock *BB : ExitBlocks) {
    bool CloneThisExitBlock = ProblemBlocks.count(BB);

    if (CloneThisExitBlock) {
      ToBeCloned.insert(BB);

      BasicBlock *FakeExit = BasicBlock::Create(BB->getContext(), "loop.exit.new");
      F->getBasicBlockList().insert(BB, FakeExit);

      TerminatorInst *OldTerm = BB->getTerminator();
      OldTerm->removeFromParent();
      FakeExit->getInstList().push_back(OldTerm);

      BranchInst::Create(FakeExit, BB);
      for (BasicBlock *Succ : successors(FakeExit)) {
        for (Instruction &I : *Succ) {
          if (PHINode *PN = dyn_cast<PHINode>(&I)) {
            for (unsigned i = 0; i < PN->getNumIncomingValues(); i++) {
              if (PN->getIncomingBlock(i) == BB)
                PN->setIncomingBlock(i, FakeExit);
            }
          }
        }
      }

      NewExits.insert(FakeExit);
      FakeExits.insert(FakeExit);

      if (!DT->getNode(FakeExit))
        DT->addNewBlock(FakeExit, BB);
    }
    else {
      NewExits.insert(BB);
    }
  }

  SmallVector<ClonedIteration, 16> Clones;
  SmallVector<BasicBlock *, 16> ClonedBlocks;
  bool Succeeded = false;

  // Re-establish LCSSA form to get ready for unrolling.
  CreateLCSSA(ToBeCloned, NewExits, L, *DT, LI);

  std::unordered_map<BasicBlock *, BasicBlock *> CloneMap;
  std::unordered_map<BasicBlock *, BasicBlock *> ReverseCloneMap;
  for (int i = 0; i < 128; i++) { // TODO: Num of iterations
    ClonedBlocks.clear();
    CloneMap.clear();

    Clones.resize(Clones.size() + 1);
    ClonedIteration &Cloned = Clones.back();

    ClonedIteration *PrevIteration = nullptr;
    if (Clones.size() >= 2)
      PrevIteration = &Clones[Clones.size()-2];

    // Helper function for cloning a block
    auto CloneBlock = [Header, &ExitBlockSet, &ToBeCloned, &Cloned, &CloneMap, &ReverseCloneMap, &ClonedBlocks, F](BasicBlock *BB) { // TODO: Cleanup
      BasicBlock *ClonedBB = CloneBasicBlock(BB, Cloned.VarMap);
      ClonedBlocks.push_back(ClonedBB);
      ReverseCloneMap[ClonedBB] = BB;
      CloneMap[BB] = ClonedBB;
      ClonedBB->insertInto(F, Header);
      Cloned.VarMap[BB] = ClonedBB;

      if (ExitBlockSet.count(BB))
        Cloned.Extended.insert(ClonedBB);

      return ClonedBB;
    };

    for (BasicBlock *BB : ToBeCloned) {
      BasicBlock *ClonedBB = CloneBlock(BB);
      Cloned.Body.push_back(ClonedBB);
      if (BB == Latch) {
        Cloned.Latch = ClonedBB;
      }
      if (BB == Header) {
        Cloned.Header = ClonedBB;
      }
    }

    for (BasicBlock *ClonedBB : ClonedBlocks) {
      BasicBlock *BB = ReverseCloneMap[ClonedBB];
      // If branching to outside of the loop, need to update the
      // phi nodes there to include incoming values.
      for (BasicBlock *Succ : successors(ClonedBB)) {
        if (ToBeCloned.count(Succ))
          continue;
        for (Instruction &I : *Succ) {
          PHINode *PN = dyn_cast<PHINode>(&I);
          if (!PN)
            break;
          Value *OldIncoming = PN->getIncomingValueForBlock(BB);
          Value *NewIncoming = OldIncoming;
          if (Cloned.VarMap.count(OldIncoming)) { // TODO: Query once
            NewIncoming = Cloned.VarMap[OldIncoming];
          }
          PN->addIncoming(NewIncoming, ClonedBB);
        }
      }
    }


    for (BasicBlock *BB : ClonedBlocks) {
      for (Instruction &I : *BB) {
        Unroll_RemapInstruction(&I, Cloned.VarMap);
      }
    }

    for (int i = 0; i < ClonedBlocks.size(); i++) {
      BasicBlock *ClonedBB = ClonedBlocks[i];
      TerminatorInst *TI = ClonedBB->getTerminator();
      if (BranchInst *BI = dyn_cast<BranchInst>(TI)) {
        for (unsigned j = 0, NumSucc = BI->getNumSuccessors(); j < NumSucc; j++) {
          BasicBlock *OldSucc = BI->getSuccessor(j);
          if (CloneMap.count(OldSucc)) { // TODO: Do one query
            BI->setSuccessor(j, CloneMap[OldSucc]);
          }
        }
      }
    }

    // If this is the first block
    if (!PrevIteration) {
      // Replace the phi nodes in the clone block with the values coming
      // from outside of the loop
      for (PHINode *PN : PHIs) {
        PHINode *ClonedPN = cast<PHINode>(Cloned.VarMap[PN]);
        Value *ReplacementVal = ClonedPN->getIncomingValueForBlock(Predecessor);
        ClonedPN->replaceAllUsesWith(ReplacementVal);
        ClonedPN->eraseFromParent();
        Cloned.VarMap[PN] = ReplacementVal;
      }
    }
    else {
      for (PHINode *PN : PHIs) {
        PHINode *ClonedPN = cast<PHINode>(Cloned.VarMap[PN]);
        Value *ReplacementVal = PrevIteration->VarMap[PN->getIncomingValueForBlock(Latch)];
        ClonedPN->replaceAllUsesWith(ReplacementVal);
        ClonedPN->eraseFromParent();
        Cloned.VarMap[PN] = ReplacementVal;
      }

      // Make the latch of the previous iteration branch to the header
      // of this new iteration.
      if (BranchInst *BI = dyn_cast<BranchInst>(PrevIteration->Latch->getTerminator())) {
        for (unsigned i = 0; i < BI->getNumSuccessors(); i++) {
          if (BI->getSuccessor(i) == PrevIteration->Header) {
            BI->setSuccessor(i, Cloned.Header);
            break;
          }
        }
      }
    }

    for (int i = 0; i < ClonedBlocks.size(); i++) {
      BasicBlock *ClonedBB = ClonedBlocks[i];
      SimplifyInstructionsInBlock_NoDelete(ClonedBB, NULL);
    }

    if (BranchInst *BI = dyn_cast<BranchInst>(Cloned.Latch->getTerminator())) {
      bool Cond = false;
      if (!IsConstantI1(BI->getCondition(), &Cond)) {
        break;
      }
      if (!Cond && BI->getSuccessor(0) == Cloned.Header) {
        Succeeded = true;
        break;
      }
      else if (Cond && BI->getSuccessor(1) == Cloned.Header) {
        Succeeded = true;
        break;
      }
    }
  }

  if (Succeeded) {
    auto &FirstIteration = Clones.front();
    // Go through the predecessors of the old header and
    // make them branch to the new header.
    SmallVector<BasicBlock *, 8> Preds(pred_begin(Header), pred_end(Header));
    for (BasicBlock *PredBB : Preds) {
      if (L->contains(PredBB))
        continue;
      BranchInst *BI = cast<BranchInst>(PredBB->getTerminator());
      for (unsigned i = 0, NumSucc = BI->getNumSuccessors(); i < NumSucc; i++) {
        if (BI->getSuccessor(i) == Header) {
          BI->setSuccessor(i, FirstIteration.Header);
        }
      }
    }

    Loop *OuterL = L->getParentLoop();
    // If there's an outer loop, insert the new blocks
    // into
    if (OuterL) {
      for (size_t i = 0; i < Clones.size(); i++) {
        auto &Iteration = Clones[i];
        for (BasicBlock *BB : Iteration.Body) {
          if (!Iteration.Extended.count(BB))
            //if (i < Clones.size()-1 || HasSuccessorsInLoop(BB, OuterL)) // FIXME: Fix this. It still has return blocks being added to the outer loop.
            OuterL->addBasicBlockToLoop(BB, *LI);
        }
      }

      for (BasicBlock *BB : FakeExits) {
        if (HasSuccessorsInLoop(BB, OuterL)) // FIXME: Fix this. It still has return blocks being added to the outer loop.
          OuterL->addBasicBlockToLoop(BB, *LI);
      }

      for (size_t i = 0; i < Clones.size(); i++) {
        auto &Iteration = Clones[i];
        for (BasicBlock *BB : Iteration.Extended) {
          if (HasSuccessorsInLoop(BB, OuterL)) // FIXME: Fix this. It still has return blocks being added to the outer loop.
            OuterL->addBasicBlockToLoop(BB, *LI);
        }
      }

      // Remove the original blocks that we've cloned.
      for (BasicBlock *BB : ToBeCloned) {
        if (OuterL->contains(BB))
          OuterL->removeBlockFromLoop(BB);
      }
      // TODO: Simplify here, since outer loop is now weird (multiple latches etc).
      //simplifyLoop(OuterL, nullptr, LI, nullptr);
    }

    // Remove flattened loop from queue.
    // If there's an outer loop, this will also take care
    // of removing blocks.
    LPM.deleteLoopFromQueue(L); // TODO: Figure out the impact of this.
    // TODO: Update dominator tree

    for (BasicBlock *BB : ToBeCloned)
      DetachFromSuccessors(BB);
    for (BasicBlock *BB : ToBeCloned)
      BB->dropAllReferences();
    for (BasicBlock *BB : ToBeCloned)
      BB->eraseFromParent();

    DXASSERT(!llvm::verifyFunction(*F), "Failed verification");
    //std::string Dat = DumpValue(M);
    return true;
  }


  // If we were unsuccessful in unrolling the loop
  else {
    // Remove all the cloned blocks
    for (ClonedIteration &Iteration : Clones)
      for (BasicBlock *BB : Iteration.Body)
        DetachFromSuccessors(BB);
    for (ClonedIteration &Iteration : Clones)
      for (BasicBlock *BB : Iteration.Body)
        BB->dropAllReferences();
    for (ClonedIteration &Iteration : Clones)
      for (BasicBlock *BB : Iteration.Body)
        BB->eraseFromParent();

    L->verifyLoop();
    DXASSERT(!llvm::verifyFunction(*F), "Failed verification");
    return false;
  }
}

}

Pass *llvm::createDxilLoopUnrollPass() {
  return new DxilLoopUnroll();
}

INITIALIZE_PASS(DxilLoopUnroll, "dxil-loop-unroll", "Dxil Unroll loops", false, false)
