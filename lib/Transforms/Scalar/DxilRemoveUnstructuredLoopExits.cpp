//===- DxilRemoveUnstructuredLoopExits.cpp - Make unrolled loops structured ---===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
//
// Loops that look like the following when unrolled becomes unstructured:
//
//      for(;;) {
//        if (a) {
//          if (b) {
//            exit_code_0;
//            break;       // Unstructured loop exit
//          }
//
//          code_0;
//
//          if (c) {
//            if (d) {
//              exit_code_1;
//              break;    // Unstructured loop exit
//            }
//            code_1;
//          }
//
//          code_2;
//
//          ...
//        }
//
//        code_3;
//
//        if (exit)
//          break;
//      }
//      
//
// This pass transforms the loop into the following form:
//
//      bool broke_0 = false;
//      bool broke_1 = false;
//
//      for(;;) {
//        if (a) {
//          if (b) {
//            broke_0 = true;       // Break flag
//          }
//
//          if (!broke_0) {
//            code_0;
//          }
//
//          if (!broke_0) {
//            if (c) {
//              if (d) {
//                broke_1 = true;   // Break flag
//              }
//              if (!broke_1) {
//                code_1;
//              }
//            }
//
//            if (!broke_1) {
//              code_2;
//            }
//          }
//
//          ...
//        }
//
//        if (!broke_0) {
//          break;
//        }
//
//        if (!broke_1) {
//          break;
//        }
//
//        code_3;
//
//        if (exit)
//          break;
//      }
//
//      if (broke_0) {
//        exit_code_0;
//      }
//
//      if (broke_1) {
//        exit_code_1;
//      }
//
// Essentially it hoists the exit branch out of the loop.
//
// This function should be called any time before a function is unrolled to
// avoid generating unstructured code.
//
// There are several limitations at the moment:
//
//   - if code_0, code_1, etc has any loops in there, this transform
//     does not take place. Since the values that flow out of the conditions
//     are phi of undef, I do not want to risk the loops not exiting.
//
//   - code_0, code_1, etc, become conditional only when there are
//     side effects in there. This doesn't impact code correctness,
//     but the code will execute for one iteration even if the exit condition
//     is met.
//
//   - If there are values used by exit_code that isn't defined in the 
//     loop header (or anywhere that doesn't dominate the loop latch)
//     this transformation does not take place.
//
// These limitations can be fixed in the future as needed.
//
//===----------------------------------------------------------------------===//

#include "llvm/Analysis/LoopPass.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"
#include "llvm/ADT/SetVector.h"
#include "dxc/HLSL/DxilNoops.h"

#include <unordered_map>
#include <unordered_set>

#include "DxilRemoveUnstructuredLoopExits.h"

using namespace llvm;

static bool IsNoop(Instruction *inst) {
  if (CallInst *ci = dyn_cast<CallInst>(inst)) {
    if (Function *f = ci->getCalledFunction()) {
      return f->getName() == hlsl::kNoopName;
    }
  }
  return false;
}

static BasicBlock *GetExitBlockForExitingBlock(Loop *L, BasicBlock *exiting_block) {
  BranchInst *br = dyn_cast<BranchInst>(exiting_block->getTerminator());
  assert(L->contains(exiting_block));
  assert(br->isConditional());
  BasicBlock *result = L->contains(br->getSuccessor(0)) ? br->getSuccessor(1) : br->getSuccessor(0);
  assert(!L->contains(result));
  return result;
}

static bool RemoveUnstructuredLoopExitsIteration(BasicBlock *exiting_block, Loop *L, LoopInfo *LI, DominatorTree *DT) {

  LLVMContext &ctx = L->getHeader()->getContext();
  Type *i1Ty = Type::getInt1Ty(ctx);

  BasicBlock *exit_block = GetExitBlockForExitingBlock(L, exiting_block);

  // If there's more than one predecessors for this exit block, don't risk it.
  if (!exit_block->getSinglePredecessor())
    return false;

  {
    BasicBlock *latch = L->getLoopLatch();
    BasicBlock *latch_exit = GetExitBlockForExitingBlock(L, latch);

    // If the latch and the exiting block go to the same place, then we probably already fixed this exit.
    if (exit_block == latch_exit) {
      return false;
    }

    for (Instruction &I : *exit_block) {
      if (PHINode *phi = dyn_cast<PHINode>(&I)) {
        // If there are values flowing out of the loop into the exit_block,
        // if any of those values do not dominate the latch, they would need
        // to be propagated to the latch, which we don't do right now.
        //
        if (Instruction *value = dyn_cast<Instruction>(phi->getIncomingValueForBlock(exiting_block))) {
          if (!DT->dominates(value, latch)) {
            return false;
          }
        }
      }
      else {
        break;
      }
    }
  }

  BranchInst *exiting_br = cast<BranchInst>(exiting_block->getTerminator());
  Value *exit_cond = exiting_br->getCondition();

  Value *exit_cond_dominates_latch = nullptr;
  BasicBlock *new_exiting_block = nullptr;
  SmallVector<std::pair<BasicBlock *, Value *>, 4> blocks_with_side_effect;
  bool give_up = false;
  std::unordered_map<BasicBlock *, PHINode *> cached_phis;

  // Use a worklist to propagate the exit condition from within the block
  {
    Value *false_value = ConstantInt::getFalse(i1Ty);

    struct Propagate_Data {
      BasicBlock *bb;
      Value *exit_cond;
    };

    std::unordered_set<BasicBlock *> seen;
    SmallVector<Propagate_Data, 4> work_list;

    work_list.push_back({ exiting_block, exit_cond, });
    seen.insert(exiting_block);

    BasicBlock *latch = L->getLoopLatch();

    for (unsigned i = 0; i < work_list.size(); i++) {
      Propagate_Data data = work_list[i];

      BasicBlock *bb = data.bb;

      // Don't continue to propagate when we hit the latch
      if (bb == latch && DT->dominates(bb, latch)) {
        exit_cond_dominates_latch = data.exit_cond;
        new_exiting_block = bb;
        continue;
      }

      // Do not include the exiting block itself in this calculation
      if (i != 0) {
        // If this block is part of an inner loop... Give up for now.
        if (LI->getLoopFor(data.bb) != L) {
          give_up = true;
        }
        // Otherwise just remember the blocks with side effects (including the latch)
        else {
          for (Instruction &I : *bb) {
            if (I.mayReadOrWriteMemory() && !IsNoop(&I)) {
              blocks_with_side_effect.push_back({ bb, data.exit_cond });
              break;
            }
          }
        }
      } // If this is not the first iteration

      for (BasicBlock *succ : llvm::successors(bb)) {
        if (!L->contains(succ))
          continue;

        PHINode *phi = cached_phis[succ];
        if (!phi) {
          phi = PHINode::Create(i1Ty, 2, "dx.struct_exit.exit_cond", &*succ->begin());
          for (BasicBlock *pred : llvm::predecessors(succ)) {
            phi->addIncoming(false_value, pred);
          }
          cached_phis[succ] = phi;
        }

        for (unsigned i = 0; i < phi->getNumIncomingValues(); i++) {
          if (phi->getIncomingBlock(i) == bb) {
            phi->setIncomingValue(i, data.exit_cond);
            break;
          }
        }

        if (!seen.count(succ)) {
          work_list.push_back({ succ, phi, });
          seen.insert(succ);
        }

      } // for each succ
    } // for each in worklist
  } // if exit condition is an instruction

  if (give_up) {
    for (std::pair<BasicBlock *, PHINode *> pair : cached_phis) {
      if (pair.second)
        pair.second->dropAllReferences();
    }
    for (std::pair<BasicBlock *, PHINode *> pair : cached_phis) {
      if (pair.second)
        pair.second->eraseFromParent();
    }
    return false;
  }

  // Make the exiting block not exit.
  {
    BasicBlock *non_exiting_block = exiting_br->getSuccessor(exiting_br->getSuccessor(0) == exit_block ? 1 : 0);
    BranchInst::Create(non_exiting_block, exiting_block);
    exiting_br->eraseFromParent();
    exiting_br = nullptr;
  }

  // If bb has side effect, split it into 3 basic blocks, where its body is
  // gated behind if (!exit_cond)
  for (std::pair<BasicBlock *, Value *> data : blocks_with_side_effect) {
    BasicBlock *bb = data.first;
    Value *exit_cond = data.second;

    BasicBlock *body = bb->splitBasicBlock(bb->getFirstNonPHI());
    body->setName("dx.struct_exit.cond_body");
    BasicBlock *end = body->splitBasicBlock(body->getTerminator());
    end->setName("dx.struct_exit.cond_end");

    bb->getTerminator()->eraseFromParent();
    BranchInst::Create(end, body, exit_cond, bb);

    for (Instruction &inst : *body) {
      PHINode *phi = nullptr;

      for (User *user : inst.users()) {
        Instruction *user_inst = dyn_cast<Instruction>(user);
        if (!user_inst)
          continue;

        if (user_inst->getParent() != body) {
          if (!phi) {
            phi = PHINode::Create(inst.getType(), 2, "", &*end->begin());
            phi->addIncoming(UndefValue::get(inst.getType()), bb);
            phi->addIncoming(&inst, body);
          }

          user_inst->replaceUsesOfWith(&inst, phi);
        }
      } // For each user of inst of body
    } // For each inst in body

    L->addBasicBlockToLoop(body, *LI);
    L->addBasicBlockToLoop(end, *LI);

  } // For each bb with side effect

  assert(exit_cond_dominates_latch);
  assert(new_exiting_block);

  // Split the block where we're now exiting from, and branch to latch exit
  BasicBlock *latch_exit = GetExitBlockForExitingBlock(L, L->getLoopLatch());
  StringRef old_name = new_exiting_block->getName();
  BasicBlock *new_not_exiting_block = new_exiting_block->splitBasicBlock(new_exiting_block->getFirstNonPHI());
  new_exiting_block->setName("dx.struct_exit.new_exiting");
  new_not_exiting_block->setName(old_name);
  L->addBasicBlockToLoop(new_not_exiting_block, *LI);

  new_exiting_block->getTerminator()->eraseFromParent();
  BranchInst::Create(latch_exit, new_not_exiting_block, exit_cond_dominates_latch, new_exiting_block);

  // Split the latch exit, since it's going to branch to the real exit block
  BasicBlock *post_exit_location = latch_exit->splitBasicBlock(latch_exit->getFirstNonPHI());
  // If latch exit is part of an outer loop, add its split in there too.
  if (Loop *outer_loop = LI->getLoopFor(latch_exit)) {
    outer_loop->addBasicBlockToLoop(post_exit_location, *LI);
  }
  // If the original exit block is part of an outer loop, then latch exit (which is the
  // new exit block) must be part of it, since all blocks that branch to within
  // a loop must be part of that loop structure.
  else if (Loop *outer_loop = LI->getLoopFor(exit_block)) {
    outer_loop->addBasicBlockToLoop(latch_exit, *LI);
  }

  // Since now new exiting block is branching to latch exit, its phis need to be updated.
  for (Instruction &inst : *latch_exit) {
    PHINode *phi = dyn_cast<PHINode>(&inst);
    if (!phi)
      break;
    phi->addIncoming(UndefValue::get(phi->getType()), new_exiting_block);
  }

  unsigned latch_exit_num_predecessors = 0;
  for (BasicBlock *pred : llvm::predecessors(latch_exit)) {
    (void)pred;
    latch_exit_num_predecessors++;
  }

  // Make exit condition visible
  PHINode *exit_cond_lcssa = PHINode::Create(exit_cond_dominates_latch->getType(), latch_exit_num_predecessors, "dx.struct_exit.exit_cond_lcssa", latch_exit->begin());
  for (BasicBlock *pred : llvm::predecessors(latch_exit)) {
    if (pred == new_exiting_block) {
      exit_cond_lcssa->addIncoming(exit_cond_dominates_latch, pred);
    }
    else {
      exit_cond_lcssa->addIncoming(ConstantInt::getFalse(exit_cond_lcssa->getType()), pred);
    }
  }

  // Take the exit outside the loop.
  latch_exit->getTerminator()->eraseFromParent();
  BranchInst::Create(exit_block, post_exit_location, exit_cond_lcssa, latch_exit);

  // Fix the phi's in the real exit block, and insert phis in the latch exit to maintain
  // lcssa form.
  for (Instruction &inst : *exit_block) {
    PHINode *phi = dyn_cast<PHINode>(&inst);
    if (!phi)
      break;

    for (unsigned i = 0; i < phi->getNumIncomingValues(); i++) {
      if (phi->getIncomingBlock(i) == exiting_block) {
        phi->setIncomingBlock(i, latch_exit);

        PHINode *lcssa_phi = PHINode::Create(phi->getType(), latch_exit_num_predecessors, "dx.struct_exit.lcssa_phi", latch_exit->begin());
        for (BasicBlock *pred : llvm::predecessors(latch_exit)) {
          if (pred == new_exiting_block) {
            lcssa_phi->addIncoming(phi->getIncomingValue(i), new_exiting_block);
          }
          else {
            lcssa_phi->addIncoming(UndefValue::get(lcssa_phi->getType()), pred);
          }
        }

        phi->setIncomingValue(i, lcssa_phi);
      }
    }
  }

  DT->recalculate(*L->getHeader()->getParent());
  assert(L->isLCSSAForm(*DT));

  return true;
}

bool hlsl::RemoveUnstructuredLoopExits(llvm::Loop *L, llvm::LoopInfo *LI, llvm::DominatorTree *DT, std::unordered_set<llvm::BasicBlock *> *exclude_set) {
  
  bool changed = false;

  if (!L->isLCSSAForm(*DT))
    return false;

  // Give up if loop is not rotated somehow
  if (BasicBlock *latch = L->getLoopLatch()) {
    if (!cast<BranchInst>(latch->getTerminator())->isConditional())
      return false;
  }
  // Give up if there's not a single latch
  else {
    return false;
  }

  for (;;) {
    // Recompute exiting block every time, since they could change between
    // iterations
    llvm::SmallVector<BasicBlock *, 4> exiting_blocks;
    L->getExitingBlocks(exiting_blocks);

    bool local_changed = false;
    for (BasicBlock *exiting_block : exiting_blocks) {
      auto latch = L->getLoopLatch();
      if (latch == exiting_block)
        continue;

      if (exclude_set && exclude_set->count(GetExitBlockForExitingBlock(L, exiting_block)))
        continue;

      // As soon as we got a success, break and start a new iteration, since
      // exiting blocks could have changed.
      local_changed = RemoveUnstructuredLoopExitsIteration(exiting_block, L, LI, DT);
      if (local_changed) {
        break;
      }
    }

    changed |= local_changed;
    if (!local_changed) {
      break;
    }
  }

  return changed;
}

