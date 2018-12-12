//===- DxilLoopUnroll.cpp - Special Unroll for Constant Values ------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
//
// Special loop unroll routine for creating mandatory constant values and
// loops that have exits.
//
// Overview of algorithm:
// 
// 1. Identify a set of blocks to unroll.
//
//    LLVM's concept of loop excludes exit blocks, which are blocks that no
//    longer have a path to the loop latch. However, some exit blocks in HLSL
//    also need to be unrolled. For example:
//
//        [unroll]
//        for (uint i = 0; i < 4; i++)
//        {
//          if (...)
//          {
//            // This block here is an exit block, since it's.
//            // guaranteed to exit the loop.
//            ...
//            a[i] = ...; // Indexing requires unroll.
//            return;
//          }
//        }
//
//
// 2. Create LCSSA based on the new loop boundary.
//
//    See LCSSA.cpp for more details. It creates trivial PHI nodes for any
//    outgoing values of the loop at the exit blocks, so when the loop body
//    gets cloned, the outgoing values can be added to those PHI nodes easily.
//
//    We are using a modified LCSSA routine here because we are including some
//    of the original exit blocks in the unroll.
//
//
// 3. Unroll the loop until we succeed.
//
//    Unlike LLVM, we do not try to find a loop count before unrolling.
//    Instead, we unroll to find a constant terminal condition. Give up when we
//    fail to do so.
//
//
//===----------------------------------------------------------------------===//

#include "llvm/Pass.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Analysis/InstructionSimplify.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Analysis/InstructionSimplify.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/Transforms/Utils/UnrollLoop.h"
#include "llvm/Transforms/Utils/SSAUpdater.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/Transforms/Utils/PromoteMemToReg.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/PredIteratorCache.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"
#include "llvm/ADT/SetVector.h"

#include "dxc/DXIL/DxilUtil.h"
#include "dxc/HLSL/HLModule.h"

using namespace llvm;
using namespace hlsl;

// Copied over from LoopUnroll.cpp - RemapInstruction()
static inline void RemapInstruction(Instruction *I,
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


namespace {

class DxilLoopUnroll : public LoopPass {
public:
  static char ID;

  std::unordered_set<Function *> CleanedUpAlloca;
  unsigned MaxIterationAttempt = 0;

  DxilLoopUnroll(unsigned MaxIterationAttempt = 128) :
    LoopPass(ID),
    MaxIterationAttempt(MaxIterationAttempt)
  {
    initializeDxilLoopUnrollPass(*PassRegistry::getPassRegistry());
  }
  const char *getPassName() const override { return "Dxil Loop Unroll"; }
  bool runOnLoop(Loop *L, LPPassManager &LPM) override;
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<LoopInfoWrapperPass>();
    AU.addPreserved<LoopInfoWrapperPass>();
    AU.addRequiredID(LoopSimplifyID);
    AU.addRequired<AssumptionCacheTracker>();
    AU.addRequired<DominatorTreeWrapperPass>();
    AU.addPreserved<DominatorTreeWrapperPass>();
  }
};

char DxilLoopUnroll::ID;

static void FailLoopUnroll(bool WarnOnly, Loop *L, const char *Message) {
  DebugLoc DL = L->getStartLoc();
  LLVMContext &Ctx = L->getHeader()->getContext();

  if (WarnOnly) {
    if (DL.get())
      Ctx.emitWarning(hlsl::dxilutil::FormatMessageAtLocation(DL, Message));
    else
      Ctx.emitWarning(hlsl::dxilutil::FormatMessageWithoutLocation(Message));
  }
  else {
    if (DL.get())
      Ctx.emitError(hlsl::dxilutil::FormatMessageAtLocation(DL, Message));
    else
      Ctx.emitError(hlsl::dxilutil::FormatMessageWithoutLocation(Message));
  }
}

struct LoopIteration {
  SmallVector<BasicBlock *, 16> Body;
  BasicBlock *Latch = nullptr;
  BasicBlock *Header = nullptr;
  ValueToValueMapTy VarMap;
  SetVector<BasicBlock *> Extended; // Blocks that are included in the clone that are not in the core loop body.
  LoopIteration() {}
};

static bool GetConstantI1(Value *V, bool *Val=nullptr) {
  if (ConstantInt *C = dyn_cast<ConstantInt>(V)) {
    if (V->getType()->isIntegerTy(1)) {
      if (Val)
        *Val = (bool)C->getLimitedValue();
      return true;
    }
  }
  return false;
}

// Copied from llvm::SimplifyInstructionsInBlock
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
#if 0 // HLSL Change
    MadeChange |= RecursivelyDeleteTriviallyDeadInstructions(Inst, TLI);
#endif // HLSL Change
    if (BIHandle != BI)
      BI = BB->begin();
  }
  return MadeChange;
}

static bool IsMarkedFullUnroll(Loop *L) {
  if (MDNode *LoopID = L->getLoopID())
    return GetUnrollMetadata(LoopID, "llvm.loop.unroll.full");
  return false;
}

static bool HasSuccessorsInLoop(BasicBlock *BB, Loop *L) {
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

// Copied and modified from LCSSA.cpp
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
#if 0 // HLSL Change
  ++NumLCSSA; // We are applying the transformation
#endif // HLSL Change
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

// Copied from LCSSA.cpp
static bool blockDominatesAnExit(BasicBlock *BB,
                     DominatorTree &DT,
                     const SmallVectorImpl<BasicBlock *> &ExitBlocks) {
  DomTreeNode *DomNode = DT.getNode(BB);
  for (BasicBlock *Exit : ExitBlocks)
    if (DT.dominates(DomNode, DT.getNode(Exit)))
      return true;
  return false;
};

// Copied from LCSSA.cpp
//
// We need to recreate the LCSSA form since our loop boundary is potentially different from
// the canonical one.
static bool CreateLCSSA(SetVector<BasicBlock *> &Body, const SmallVectorImpl<BasicBlock *> &ExitBlocks, Loop *L, DominatorTree &DT, LoopInfo *LI) {

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

      Changed |= processInstruction(Body, *L, *I, DT, ExitBlocks, PredCache, LI);
    }
  }

  return Changed;
}

static void FindProblemBlocks(BasicBlock *Header, const SmallVectorImpl<BasicBlock *> &BlocksInLoop, std::unordered_set<BasicBlock *> &ProblemBlocks) {
  SmallVector<Instruction *, 16> WorkList;

  std::unordered_set<BasicBlock *> BlocksInLoopSet(BlocksInLoop.begin(), BlocksInLoop.end());
  std::unordered_set<Instruction *> InstructionsSeen;

  for (Instruction &I : *Header) {
    PHINode *PN = dyn_cast<PHINode>(&I);
    if (!PN)
      break;
    WorkList.push_back(PN);
    InstructionsSeen.insert(PN);
  }

  while (WorkList.size()) {
    Instruction *I = WorkList.pop_back_val();

    if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(I)) {
      Type *EltType = GEP->getType()->getPointerElementType();

      // NOTE: This is a very convservative in the following conditions:
      // - constant global resource arrays with external linkage (these can be
      //   dynamically accessed)
      // - global resource arrays or alloca resource arrays, as long as all
      //   writes come from the same original resource definition (which can
      //   also be an array).
      //
      // We may want to make this more precise in the future if it becomes a
      // problem.
      //
      if (hlsl::dxilutil::IsHLSLObjectType(EltType)) {
        ProblemBlocks.insert(GEP->getParent());
        continue; // Stop Propagating
      }
    }

    for (User *U : I->users()) {
      if (Instruction *UserI = dyn_cast<Instruction>(U)) {
        if (!InstructionsSeen.count(UserI) &&
          BlocksInLoopSet.count(UserI->getParent()))
        {
          InstructionsSeen.insert(UserI);
          WorkList.push_back(UserI);
        }
      }
    }
  }
}

static bool ContainsFloatingPointType(Type *Ty) {
  if (Ty->isFloatingPointTy()) {
    return true;
  }
  else if (Ty->isArrayTy()) {
    return ContainsFloatingPointType(Ty->getArrayElementType());
  }
  else if (Ty->isVectorTy()) {
    return ContainsFloatingPointType(Ty->getVectorElementType());
  }
  else if (Ty->isStructTy()) {
    for (unsigned i = 0, NumStructElms = Ty->getStructNumElements(); i < NumStructElms; i++) {
      if (ContainsFloatingPointType(Ty->getStructElementType(i)))
        return true;
    }
  }
  return false;
}

static bool Mem2Reg(Function &F, DominatorTree &DT, AssumptionCache &AC) {
  BasicBlock &BB = F.getEntryBlock();  // Get the entry node for the function
  bool Changed  = false;
  std::vector<AllocaInst*> Allocas;
  while (1) {
    Allocas.clear();

    // Find allocas that are safe to promote, by looking at all instructions in
    // the entry node
    for (BasicBlock::iterator I = BB.begin(), E = --BB.end(); I != E; ++I)
      if (AllocaInst *AI = dyn_cast<AllocaInst>(I))       // Is it an alloca?
        if (isAllocaPromotable(AI) &&
          (!HLModule::HasPreciseAttributeWithMetadata(AI) || !ContainsFloatingPointType(AI->getAllocatedType())))
          Allocas.push_back(AI);

    if (Allocas.empty()) break;

    PromoteMemToReg(Allocas, DT, nullptr, &AC);
    Changed = true;
  }

  return Changed;
}


bool DxilLoopUnroll::runOnLoop(Loop *L, LPPassManager &LPM) {

  // If the loop is not marked as [unroll], don't do anything.
  if (!IsMarkedFullUnroll(L))
    return false;

  if (!L->isSafeToClone())
    return false;

  Function *F = L->getHeader()->getParent();
  bool OnlyWarnOnFail = false;
  if (F->getParent()->HasHLModule()) {
    HLModule &HM = F->getParent()->GetHLModule();
    OnlyWarnOnFail = HM.GetHLOptions().bFXCCompatMode;
  }

  // Analysis passes
  DominatorTree *DT = &getAnalysis<DominatorTreeWrapperPass>().getDomTree();
  AssumptionCache *AC =
    &getAnalysis<AssumptionCacheTracker>().getAssumptionCache(*F);
  LoopInfo *LI = &getAnalysis<LoopInfoWrapperPass>().getLoopInfo();

  Loop *OuterL = L->getParentLoop();
  BasicBlock *Latch = L->getLoopLatch();
  BasicBlock *Header = L->getHeader();
  BasicBlock *Predecessor = L->getLoopPredecessor();

  // Quit if we don't have a single latch block or predecessor
  if (!Latch || !Predecessor) {
    return false;
  }

  // If the loop exit condition is not in the latch, then the loop is not rotated. Give up.
  if (!cast<BranchInst>(Latch->getTerminator())->isConditional()) {
    return false;
  }

  // Promote alloca's
  if (!CleanedUpAlloca.count(F)) {
    CleanedUpAlloca.insert(F);
    Mem2Reg(*F, *DT, *AC);
  }

  SmallVector<BasicBlock *, 16> ExitBlocks;
  L->getExitBlocks(ExitBlocks);
  std::unordered_set<BasicBlock *> ExitBlockSet(ExitBlocks.begin(), ExitBlocks.end());

  SmallVector<BasicBlock *, 16> BlocksInLoop; // Set of blocks including both body and exits
  BlocksInLoop.append(L->getBlocks().begin(), L->getBlocks().end());
  BlocksInLoop.append(ExitBlocks.begin(), ExitBlocks.end());

  // Heuristically find blocks that likely need to be unrolled
  std::unordered_set<BasicBlock *> ProblemBlocks;
  FindProblemBlocks(L->getHeader(), BlocksInLoop, ProblemBlocks);

  // Keep track of the PHI nodes at the header.
  SmallVector<PHINode *, 16> PHIs;
  for (auto it = Header->begin(); it != Header->end(); it++) {
    if (PHINode *PN = dyn_cast<PHINode>(it)) {
      PHIs.push_back(PN);
    }
    else {
      break;
    }
  }

  SetVector<BasicBlock *> ToBeCloned; // List of blocks that will be cloned.
  for (BasicBlock *BB : L->getBlocks()) // Include the body right away
    ToBeCloned.insert(BB);

  // Find the exit blocks that also need to be included
  // in the unroll.
  SmallVector<BasicBlock *, 8> NewExits; // New set of exit blocks as boundaries for LCSSA
  SmallVector<BasicBlock *, 8> FakeExits; // Set of blocks created to allow cloning original exit blocks.
  for (BasicBlock *BB : ExitBlocks) {
    bool CloneThisExitBlock = ProblemBlocks.count(BB);

    if (CloneThisExitBlock) {
      ToBeCloned.insert(BB);

      // If we are cloning this basic block, we must create a new exit
      // block for inserting LCSSA PHI nodes.
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

      NewExits.push_back(FakeExit);
      FakeExits.push_back(FakeExit);

      // Update Dom tree with new exit
      if (!DT->getNode(FakeExit))
        DT->addNewBlock(FakeExit, BB);
    }
    else {
      // If we are not including this exit block in the unroll,
      // use it for LCSSA as normal.
      NewExits.push_back(BB);
    }
  }

  // Simplify the PHI nodes that have single incoming value. The original LCSSA form
  // (if exists) does not necessarily work for our unroll because we may be unrolling
  // from a different boundary.
  for (BasicBlock *BB : BlocksInLoop)
    hlsl::dxilutil::SimplifyTrivialPHIs(BB);

  // Re-establish LCSSA form to get ready for unrolling.
  CreateLCSSA(ToBeCloned, NewExits, L, *DT, LI);

  SmallVector<std::unique_ptr<LoopIteration>, 16> Iterations; // List of cloned iterations
  bool Succeeded = false;

  for (unsigned IterationI = 0; IterationI < this->MaxIterationAttempt; IterationI++) {

    LoopIteration *PrevIteration = nullptr;
    if (Iterations.size())
      PrevIteration = Iterations.back().get();
    Iterations.push_back(llvm::make_unique<LoopIteration>());
    LoopIteration &CurIteration = *Iterations.back().get();

    // Clone the blocks.
    for (BasicBlock *BB : ToBeCloned) {

      BasicBlock *ClonedBB = CloneBasicBlock(BB, CurIteration.VarMap);
      CurIteration.VarMap[BB] = ClonedBB;
      ClonedBB->insertInto(F, Header);

      if (ExitBlockSet.count(BB))
        CurIteration.Extended.insert(ClonedBB);

      CurIteration.Body.push_back(ClonedBB);

      // Identify the special blocks.
      if (BB == Latch) {
        CurIteration.Latch = ClonedBB;
      }
      if (BB == Header) {
        CurIteration.Header = ClonedBB;
      }
    }

    for (BasicBlock *BB : ToBeCloned) {
      BasicBlock *ClonedBB = cast<BasicBlock>(CurIteration.VarMap[BB]);
      // If branching to outside of the loop, need to update the
      // phi nodes there to include new values.
      for (BasicBlock *Succ : successors(ClonedBB)) {
        if (ToBeCloned.count(Succ))
          continue;
        for (Instruction &I : *Succ) {
          PHINode *PN = dyn_cast<PHINode>(&I);
          if (!PN)
            break;

          // Find the incoming value for this new block. If there is an entry
          // for this block in the map, then it was defined in the loop, use it.
          // Otherwise it came from outside the loop.
          Value *OldIncoming = PN->getIncomingValueForBlock(BB);
          Value *NewIncoming = OldIncoming;
          ValueToValueMapTy::iterator Itor = CurIteration.VarMap.find(OldIncoming);
          if (Itor != CurIteration.VarMap.end())
            NewIncoming = Itor->second;
          PN->addIncoming(NewIncoming, ClonedBB);
        }
      }
    }

    // Remap the instructions inside of cloned blocks.
    for (BasicBlock *BB : CurIteration.Body) {
      for (Instruction &I : *BB) {
        ::RemapInstruction(&I, CurIteration.VarMap);
      }
    }

    // If this is the first block
    if (!PrevIteration) {
      // Replace the phi nodes in the clone block with the values coming
      // from outside of the loop
      for (PHINode *PN : PHIs) {
        PHINode *ClonedPN = cast<PHINode>(CurIteration.VarMap[PN]);
        Value *ReplacementVal = ClonedPN->getIncomingValueForBlock(Predecessor);
        ClonedPN->replaceAllUsesWith(ReplacementVal);
        ClonedPN->eraseFromParent();
        CurIteration.VarMap[PN] = ReplacementVal;
      }
    }
    else {
      // Replace the phi nodes with the value defined INSIDE the previous iteration.
      for (PHINode *PN : PHIs) {
        PHINode *ClonedPN = cast<PHINode>(CurIteration.VarMap[PN]);
        Value *ReplacementVal = PrevIteration->VarMap[PN->getIncomingValueForBlock(Latch)];
        ClonedPN->replaceAllUsesWith(ReplacementVal);
        ClonedPN->eraseFromParent();
        CurIteration.VarMap[PN] = ReplacementVal;
      }

      // Make the latch of the previous iteration branch to the header
      // of this new iteration.
      if (BranchInst *BI = dyn_cast<BranchInst>(PrevIteration->Latch->getTerminator())) {
        for (unsigned i = 0; i < BI->getNumSuccessors(); i++) {
          if (BI->getSuccessor(i) == PrevIteration->Header) {
            BI->setSuccessor(i, CurIteration.Header);
            break;
          }
        }
      }
    }

    // Simplify instructions in the cloned blocks to create
    // constant exit conditions.
    for (BasicBlock *ClonedBB : CurIteration.Body)
      SimplifyInstructionsInBlock_NoDelete(ClonedBB, NULL);

    // Check exit condition to see if we fully unrolled the loop
    if (BranchInst *BI = dyn_cast<BranchInst>(CurIteration.Latch->getTerminator())) {
      bool Cond = false;
      if (GetConstantI1(BI->getCondition(), &Cond)) {
        if (BI->getSuccessor(Cond ? 1 : 0) == CurIteration.Header) {
          Succeeded = true;
          break;
        }
      }
    }
  }

  if (Succeeded) {
    LoopIteration &FirstIteration = *Iterations.front().get();
    // Make the predecessor branch to the first new header.
    {
      BranchInst *BI = cast<BranchInst>(Predecessor->getTerminator());
      for (unsigned i = 0, NumSucc = BI->getNumSuccessors(); i < NumSucc; i++) {
        if (BI->getSuccessor(i) == Header) {
          BI->setSuccessor(i, FirstIteration.Header);
        }
      }
    }

    if (OuterL) {
      // Core body blocks need to be added to outer loop
      for (size_t i = 0; i < Iterations.size(); i++) {
        LoopIteration &Iteration = *Iterations[i].get();
        for (BasicBlock *BB : Iteration.Body) {
          if (!Iteration.Extended.count(BB)) {
            OuterL->addBasicBlockToLoop(BB, *LI);
          }
        }
      }

      // Our newly created exit blocks may need to be added to outer loop
      for (BasicBlock *BB : FakeExits) {
        if (HasSuccessorsInLoop(BB, OuterL))
          OuterL->addBasicBlockToLoop(BB, *LI);
      }

      // Cloned exit blocks may need to be added to outer loop
      for (size_t i = 0; i < Iterations.size(); i++) {
        LoopIteration &Iteration = *Iterations[i].get();
        for (BasicBlock *BB : Iteration.Extended) {
          if (HasSuccessorsInLoop(BB, OuterL))
            OuterL->addBasicBlockToLoop(BB, *LI);
        }
      }
    }

    // Remove the original blocks that we've cloned from all loops.
    for (BasicBlock *BB : ToBeCloned)
      LI->removeBlock(BB);

    LPM.deleteLoopFromQueue(L);

    // Remove dead blocks.
    for (BasicBlock *BB : ToBeCloned)
      DetachFromSuccessors(BB);
    for (BasicBlock *BB : ToBeCloned)
      BB->dropAllReferences();
    for (BasicBlock *BB : ToBeCloned)
      BB->eraseFromParent();

    if (OuterL) {
      // This process may have created multiple back edges for the
      // parent loop. Simplify to keep it well-formed.
      simplifyLoop(OuterL, DT, LI, this, nullptr, nullptr, AC);
    }

    return true;
  }

  // If we were unsuccessful in unrolling the loop
  else {
    FailLoopUnroll(OnlyWarnOnFail, L, "Could not unroll loop.");

    // Remove all the cloned blocks
    for (std::unique_ptr<LoopIteration> &Ptr : Iterations) {
      LoopIteration &Iteration = *Ptr.get();
      for (BasicBlock *BB : Iteration.Body)
        DetachFromSuccessors(BB);
    }
    for (std::unique_ptr<LoopIteration> &Ptr : Iterations) {
      LoopIteration &Iteration = *Ptr.get();
      for (BasicBlock *BB : Iteration.Body)
        BB->dropAllReferences();
    }
    for (std::unique_ptr<LoopIteration> &Ptr : Iterations) {
      LoopIteration &Iteration = *Ptr.get();
      for (BasicBlock *BB : Iteration.Body)
        BB->eraseFromParent();
    }

    return false;
  }
}

}

Pass *llvm::createDxilLoopUnrollPass(unsigned MaxIterationAttempt) {
  return new DxilLoopUnroll(MaxIterationAttempt);
}

INITIALIZE_PASS(DxilLoopUnroll, "dxil-loop-unroll", "Dxil Unroll loops", false, false)
