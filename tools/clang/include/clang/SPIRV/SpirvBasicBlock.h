//===-- SpirvBasicBlock.h - SPIR-V Basic Block ----------------*- C++ -*---===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_CLANG_SPIRV_SPIRVBASICBLOCK_H
#define LLVM_CLANG_SPIRV_SPIRVBASICBLOCK_H

#include <string>
#include <vector>

#include "clang/SPIRV/SpirvInstruction.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/ilist.h"
#include "llvm/ADT/ilist_node.h"

namespace clang {
namespace spirv {

class SpirvVisitor;

class SpirvInstructionNode : public llvm::ilist_node<SpirvInstructionNode> {
public:
  SpirvInstructionNode() : instruction(nullptr) {}
  SpirvInstructionNode(SpirvInstruction *instr) : instruction(instr) {}

  SpirvInstruction *instruction;
};

/// The class representing a SPIR-V basic block in memory.
class SpirvBasicBlock {
public:
  SpirvBasicBlock(llvm::StringRef name);
  ~SpirvBasicBlock() = default;

  // Forbid copy construction and assignment
  SpirvBasicBlock(const SpirvBasicBlock &) = delete;
  SpirvBasicBlock &operator=(const SpirvBasicBlock &) = delete;

  // Forbid move construction and assignment
  SpirvBasicBlock(SpirvBasicBlock &&) = delete;
  SpirvBasicBlock &operator=(SpirvBasicBlock &&) = delete;

  /// Returns the label's <result-id> of this basic block.
  uint32_t getResultId() const { return labelId; }
  void setResultId(uint32_t id) { labelId = id; }

  /// Returns the debug name of this basic block.
  llvm::StringRef getName() const { return labelName; }

  /// Sets the merge target for this basic block.
  ///
  /// The caller must make sure this basic block contains an OpSelectionMerge
  /// or OpLoopMerge instruction.
  void setMergeTarget(SpirvBasicBlock *target) { mergeTarget = target; }
  /// Returns the merge target if this basic block contains an OpSelectionMerge
  /// or OpLoopMerge instruction. Returns nullptr otherwise.
  SpirvBasicBlock *getMergeTarget() const { return mergeTarget; }

  /// Sets the continue target to the given basic block.
  ///
  /// The caller must make sure this basic block contains an OpLoopMerge
  /// instruction.
  void setContinueTarget(SpirvBasicBlock *target) { continueTarget = target; }
  /// Returns the continue target if this basic block contains an
  /// OpLoopMerge instruction. Returns nullptr otherwise.
  SpirvBasicBlock *getContinueTarget() const { return continueTarget; }

  /// Adds an instruction to the vector of instructions belonging to this basic
  /// block.
  void addInstruction(SpirvInstruction *inst) { instructions.push_back(inst); }

  /// Returns true if the last instruction in this basic block is a termination
  /// instruction.
  bool hasTerminator() const;

  /// Handle SPIR-V basic block visitors.
  /// If a basic block is the first basic block in a function, it must include
  /// all the variable definitions of the entire function.
  bool invokeVisitor(Visitor *, llvm::ArrayRef<SpirvVariable *> vars,
                     bool reverseOrder = false);

  /// \brief Adds the given basic block as a successsor to this basic block.
  void addSuccessor(SpirvBasicBlock *bb);

  /// Returns the list of successors of this basic block.
  llvm::ArrayRef<SpirvBasicBlock *> getSuccessors() const { return successors; }

private:
  uint32_t labelId;      ///< The label's <result-id>
  std::string labelName; ///< The label's debug name

  /// Instructions belonging to this basic block.
  llvm::ilist<SpirvInstructionNode> instructions;

  /// Successors to this basic block.
  llvm::SmallVector<SpirvBasicBlock *, 2> successors;

  /// The corresponding merge targets if this basic block is a header
  /// block of structured control flow.
  SpirvBasicBlock *mergeTarget;
  /// The continue merge targets if this basic block is a header block
  /// of structured control flow.
  SpirvBasicBlock *continueTarget;
};

} // end namespace spirv
} // end namespace clang

#endif // LLVM_CLANG_SPIRV_SPIRVBASICBLOCK_H
