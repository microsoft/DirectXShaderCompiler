//===---------- DxilValueCache.cpp - Dxil Constant Value Cache ------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Utility to compute and cache constant values for instructions.
//


#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/CFG.h"
#include "llvm/Support/Debug.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Analysis/InstructionSimplify.h"
#include "llvm/Analysis/ConstantFolding.h"
#include "llvm/ADT/Statistic.h"

#include "llvm/Analysis/DxilValueCache.h"

#include <unordered_set>

#define DEBUG_TYPE "dxil-value-cache"

using namespace llvm;

static
bool IsConstantTrue(const Value *V) {
  if (const ConstantInt *C = dyn_cast<ConstantInt>(V))
    return C->getLimitedValue() != 0;
  return false;
}
static
bool IsConstantFalse(const Value *V) {
  if (const ConstantInt *C = dyn_cast<ConstantInt>(V))
    return C->getLimitedValue() == 0;
  return false;
}

static
bool IsEntryBlock(const BasicBlock *BB) {
  return BB == &BB->getParent()->getEntryBlock();
}

void DxilValueCache::MarkAlwaysReachable(BasicBlock *BB) {
  ValueMap.Set(BB, ConstantInt::get(Type::getInt1Ty(BB->getContext()), 1));
}
void DxilValueCache::MarkUnreachable(BasicBlock *BB) {
  ValueMap.Set(BB, ConstantInt::get(Type::getInt1Ty(BB->getContext()), 0));
}

bool DxilValueCache::IsAlwaysReachable_(BasicBlock *BB) {
  if (Value *V = ValueMap.Get(BB))
    if (IsConstantTrue(V))
      return true;
  return false;
}

bool DxilValueCache::MayBranchTo(BasicBlock *A, BasicBlock *B) {
  BranchInst *Br = dyn_cast<BranchInst>(A->getTerminator());
  if (!Br) return false;

  if (Br->isUnconditional() && Br->getSuccessor(0) == B)
    return true;

  if (ConstantInt *C = dyn_cast<ConstantInt>(TryGetCachedValue(Br->getCondition()))) {
    unsigned SuccIndex = C->getLimitedValue() != 0 ? 0 : 1;
    return Br->getSuccessor(SuccIndex) == B;
  }
  return true;
}

bool DxilValueCache::IsUnreachable_(BasicBlock *BB) {
  if (Value *V = ValueMap.Get(BB))
    if (IsConstantFalse(V))
      return true;
  return false;
}

Value *DxilValueCache::ProcessAndSimplify_PHI(Instruction *I, DominatorTree *DT) {
  PHINode *PN = cast<PHINode>(I);
  BasicBlock *SoleIncoming = nullptr;

  bool Unreachable = true;
  Value *Simplified = nullptr;
  for (unsigned i = 0; i < PN->getNumIncomingValues(); i++) {
    BasicBlock *PredBB = PN->getIncomingBlock(i);
    if (IsUnreachable_(PredBB))
      continue;

    Unreachable = false;

    if (MayBranchTo(PredBB, PN->getParent())) {
      if (SoleIncoming) {
        SoleIncoming = nullptr;
        break;
      }
      SoleIncoming = PredBB;
    }
  }

  if (Unreachable) {
    return UndefValue::get(I->getType());
  }

  if (SoleIncoming) {
    Value *V = TryGetCachedValue(PN->getIncomingValueForBlock(SoleIncoming));
    if (isa<Constant>(V))
      Simplified = V;
    else if (Instruction *I = dyn_cast<Instruction>(V)) {
      // If this is an instruction, we have to make sure it
      // dominates this PHI.
      // There are several conditions that qualify:
      //   1. There's only one predecessor
      //   2. If the instruction is in the entry block, then it must dominate
      //   3. If we are provided with a Dominator tree, and it decides that
      //      it dominates.
      if (PN->getNumIncomingValues() == 1 ||
        IsEntryBlock(I->getParent()) ||
        (DT && DT->dominates(I, PN)))
      {
        Simplified = I;
      }
    }
  }

  // If we coulnd't deduce it, run the LLVM stock simplification to see
  // if we could do anything.
  if (!Simplified)
    Simplified = llvm::SimplifyInstruction(I, I->getModule()->getDataLayout());

  // One last step, to check if we have anything cached for whatever we
  // simplified to.
  if (Simplified)
    Simplified = TryGetCachedValue(Simplified);

  return Simplified;
}

Value *DxilValueCache::ProcessAndSimpilfy_Br(Instruction *I, DominatorTree *DT) {

  // The *only* reason we're paying special attention to the
  // branch inst, is to mark certain Basic Blocks as always
  // reachable or unreachable.

  BranchInst *Br = cast<BranchInst>(I);

  BasicBlock *BB = Br->getParent();
  if (Br->isConditional()) {

    BasicBlock *TrueSucc = Br->getSuccessor(0);
    BasicBlock *FalseSucc = Br->getSuccessor(1);

    Value *Cond = TryGetCachedValue(Br->getCondition());

    if (IsUnreachable_(BB)) {
      MarkUnreachable(FalseSucc);
      MarkUnreachable(TrueSucc);
    }
    else if (IsConstantTrue(Cond)) {
      if (IsAlwaysReachable_(BB)) {
        MarkAlwaysReachable(TrueSucc);
      }
      if (FalseSucc->getSinglePredecessor())
        MarkUnreachable(FalseSucc);
    }
    else if (IsConstantFalse(Cond)) {
      if (IsAlwaysReachable_(BB)) {
        MarkAlwaysReachable(FalseSucc);
      }
      if (TrueSucc->getSinglePredecessor())
        MarkUnreachable(TrueSucc);
    }
  }
  else {
    BasicBlock *Succ = Br->getSuccessor(0);
    if (IsAlwaysReachable_(BB))
      MarkAlwaysReachable(Succ);
    else if (Succ->getSinglePredecessor() && IsUnreachable_(BB))
      MarkUnreachable(Succ);
  }

  return nullptr;
}

Value *DxilValueCache::ProcessAndSimpilfy_Load(Instruction *I, DominatorTree *DT) {
  LoadInst *LI = cast<LoadInst>(I);
  Value *V = TryGetCachedValue(LI->getPointerOperand());
  if (Constant *ConstPtr = dyn_cast<Constant>(V)) {
    const DataLayout &DL = I->getModule()->getDataLayout();
    return llvm::ConstantFoldLoadFromConstPtr(ConstPtr, DL);
  }
  return nullptr;
}

Value *DxilValueCache::SimplifyAndCacheResult(Instruction *I, DominatorTree *DT) {

  const DataLayout &DL = I->getModule()->getDataLayout();

  Value *Simplified = nullptr;
  if (Instruction::Br == I->getOpcode()) {
    Simplified = ProcessAndSimpilfy_Br(I, DT);
  }
  else if (Instruction::PHI == I->getOpcode()) {
    Simplified = ProcessAndSimplify_PHI(I, DT);
  }
  else if (Instruction::Load == I->getOpcode()) {
    Simplified = ProcessAndSimpilfy_Load(I, DT);
  }
  // The rest of the checks use LLVM stock simplifications
  else if (I->isBinaryOp()) {
    Simplified =
      llvm::SimplifyBinOp(
        I->getOpcode(),
        TryGetCachedValue(I->getOperand(0)),
        TryGetCachedValue(I->getOperand(1)),
        DL);
  }
  else if (GetElementPtrInst *Gep = dyn_cast<GetElementPtrInst>(I)) {
    SmallVector<Value *, 4> Values;
    for (Value *V : Gep->operand_values()) {
      Values.push_back(TryGetCachedValue(V));
    }
    Simplified =
      llvm::SimplifyGEPInst(Values, DL, nullptr, DT, nullptr, nullptr);
  }
  else if (CmpInst *Cmp = dyn_cast<CmpInst>(I)) {
    Simplified =
      llvm::SimplifyCmpInst(Cmp->getPredicate(),
        TryGetCachedValue(I->getOperand(0)),
        TryGetCachedValue(I->getOperand(1)),
        DL);
  }
  else if (SelectInst *Select = dyn_cast<SelectInst>(I)) {
    Simplified = 
      llvm::SimplifySelectInst(
        TryGetCachedValue(Select->getCondition()),
        TryGetCachedValue(Select->getTrueValue()),
        TryGetCachedValue(Select->getFalseValue()),
        DL
      );
  }
  else if (ExtractElementInst *IE = dyn_cast<ExtractElementInst>(I)) {
    Simplified =
      llvm::SimplifyExtractElementInst(
        TryGetCachedValue(IE->getVectorOperand()),
        TryGetCachedValue(IE->getIndexOperand()),
        DL, nullptr, DT);
  }
  else if (CastInst *Cast = dyn_cast<CastInst>(I)) {
    Simplified =
      llvm::SimplifyCastInst(
        Cast->getOpcode(),
        TryGetCachedValue(Cast->getOperand(0)),
        Cast->getType(), DL);
  }

  if (Simplified && isa<Constant>(Simplified))
    ValueMap.Set(I, Simplified);

  return Simplified;
}

STATISTIC(StaleValuesEncountered, "Stale Values Encountered");

bool DxilValueCache::WeakValueMap::Seen(Value *V) {
  auto FindIt = Map.find(V);
  if (FindIt == Map.end())
    return false;

  auto &Entry = FindIt->second;
  if (Entry.IsStale())
    return false;
  return Entry.Value;
}

Value *DxilValueCache::WeakValueMap::Get(Value *V) {
  auto FindIt = Map.find(V);
  if (FindIt == Map.end())
    return nullptr;

  auto &Entry = FindIt->second;
  if (Entry.IsStale())
    return nullptr;

  Value *Result = Entry.Value;
  if (Result == GetSentinel(V->getContext()))
    return nullptr;

  return Result;
}

void DxilValueCache::WeakValueMap::SetSentinel(Value *Key) {
  Map[Key].Set(Key, GetSentinel(Key->getContext()));
}

Value *DxilValueCache::WeakValueMap::GetSentinel(LLVMContext &Ctx) {
  if (!Sentinel) {
    Sentinel.reset( PHINode::Create(Type::getInt1Ty(Ctx), 0) );
  }
  return Sentinel.get();
}

void DxilValueCache::WeakValueMap::ResetUnknowns() {
  if (!Sentinel)
    return;
  for (auto it = Map.begin(); it != Map.end(); it++) {
    if (it->second.Value == Sentinel.get())
      it->second.Value = nullptr;
  }
}

LLVM_DUMP_METHOD
void DxilValueCache::WeakValueMap::dump() const {
  for (auto It = Map.begin(), E = Map.end(); It != E; It++) {
    const Value *Key = It->first;
    if (It->second.IsStale())
      continue;
    const Value *V = It->second.Value;
    bool IsSentinel = Sentinel && V == Sentinel.get();
    if (const BasicBlock *BB = dyn_cast<BasicBlock>(Key)) {
      dbgs() << "[BB]" << BB->getName() << " -> ";
      if (IsSentinel)
        dbgs() << "NO_VALUE";
      else {
        if (IsConstantTrue(V))
          dbgs() << "Always Reachable!";
        else if (IsConstantFalse(V))
          dbgs() << "Never Reachable!";
      }
    }
    else {
      dbgs() << Key->getName() << " -> ";
      if (IsSentinel)
        dbgs() << "NO_VALUE";
      else
        dbgs() << *V;
    }
    dbgs() << "\n";
  }
}

void DxilValueCache::WeakValueMap::Set(Value *Key, Value *V) {
  Map[Key].Set(Key, V);
}

// If there's a cached value, return it. Otherwise, return
// the value itself.
Value *DxilValueCache::TryGetCachedValue(Value *V) {
  if (Value *Simplified = ValueMap.Get(V))
    return Simplified;
  return V;
}

DxilValueCache::DxilValueCache() : ImmutablePass(ID) {
  initializeDxilValueCachePass(*PassRegistry::getPassRegistry());
}

const char *DxilValueCache::getPassName() const {
  return "Dxil Value Cache";
}

Value *DxilValueCache::GetValue(Value *V, DominatorTree *DT) {
  if (Value *NewV = ValueMap.Get(V))
    return NewV;
  return ProcessValue(V, DT);
}

Constant *DxilValueCache::GetConstValue(Value *V, DominatorTree *DT) {
  if (Value *NewV = GetValue(V))
    return dyn_cast<Constant>(NewV);
  return nullptr;
}

bool DxilValueCache::IsAlwaysReachable(BasicBlock *BB, DominatorTree *DT) {
  ProcessValue(BB, DT);
  return IsAlwaysReachable_(BB);
}

bool DxilValueCache::IsUnreachable(BasicBlock *BB, DominatorTree *DT) {
  ProcessValue(BB, DT);
  return IsUnreachable_(BB);
}

LLVM_DUMP_METHOD
void DxilValueCache::dump() const {
  ValueMap.dump();
}

void DxilValueCache::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
}

Value *DxilValueCache::ProcessValue(Value *NewV, DominatorTree *DT) {

  Value *Result = nullptr;

  SmallVector<Value *, 16> WorkList;

  // Although we accept all values for convenience, we only process
  // Instructions.
  if (Instruction *I = dyn_cast<Instruction>(NewV)) {
    WorkList.push_back(I);
  }
  else if (BasicBlock *BB = dyn_cast<BasicBlock>(NewV)) {
    WorkList.push_back(BB->getTerminator());
    WorkList.push_back(BB);
  }
  else {
    return nullptr;
  }

  // Unconditionally process this one instruction, whether we've seen
  // it or not. The simplification might be able to do something to
  // simplify it even when we don't have its value cached.


  // This is a basic DFS setup.
  while (WorkList.size()) {
    Value *V = WorkList.back();

    // If we haven't seen this value, go in and push things it depends on
    // into the worklist.
    if (!ValueMap.Seen(V)) {
      ValueMap.SetSentinel(V);
      if (Instruction *I = dyn_cast<Instruction>(V)) {

        for (Use &U : I->operands()) {
          Instruction *UseI = dyn_cast<Instruction>(U.get());
          if (!UseI)
            continue;
          if (!ValueMap.Seen(UseI))
            WorkList.push_back(UseI);
        }

        if (PHINode *PN = dyn_cast<PHINode>(I)) {
          for (unsigned i = 0; i < PN->getNumIncomingValues(); i++) {
            BasicBlock *BB = PN->getIncomingBlock(i);
            TerminatorInst *Term = BB->getTerminator();
            if (!ValueMap.Seen(Term))
              WorkList.push_back(Term);
            if (!ValueMap.Seen(BB))
              WorkList.push_back(BB);
          }
        }
      }
      else if (BasicBlock *BB = dyn_cast<BasicBlock>(V)) {
        if (IsEntryBlock(BB)) {
          MarkAlwaysReachable(BB);
        }
        for (pred_iterator PI = pred_begin(BB), E = pred_end(BB); PI != E; PI++) {
          BasicBlock *PredBB = *PI;
          TerminatorInst *Term = PredBB->getTerminator();
          if (!ValueMap.Seen(Term))
            WorkList.push_back(Term);
          if (!ValueMap.Seen(PredBB))
            WorkList.push_back(PredBB);
        }
      }
    }
    // If we've seen this values, all its dependencies must have been processed
    // as well.
    else {
      WorkList.pop_back();
      if (Instruction *I = dyn_cast<Instruction>(V)) {
        Value *SimplifiedValue = SimplifyAndCacheResult(I, DT);
        // Set the result if this is the input inst.
        // SimplifyInst may not have cached the value
        // so we return it directly.
        if (I == NewV)
          Result = SimplifiedValue;
      }
      else if (BasicBlock *BB = dyn_cast<BasicBlock>(V)) {
        // Deduce the basic block's reachability based on
        // other analysis.
        if (!IsEntryBlock(BB)) {
          bool AllNeverReachable = true;
          for (pred_iterator PI = pred_begin(BB), E = pred_end(BB); PI != E; PI++) {
            if (!IsUnreachable_(BB)) {
              AllNeverReachable = false;
              break;
            }
          }
          if (AllNeverReachable)
            MarkUnreachable(BB);
        }

      }
    }
  }

  return Result;
}

char DxilValueCache::ID;

Pass *llvm::createDxilValueCachePass() {
  return new DxilValueCache();
}

INITIALIZE_PASS(DxilValueCache, DEBUG_TYPE, "Dxil Value Cache", false, false)

