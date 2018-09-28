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

namespace clang {
namespace spirv {

class SpirvVisitor;

/// The class representing a SPIR-V basic block in memory.
class SpirvBasicBlock {
public:
  SpirvBasicBlock(uint32_t id, llvm::StringRef name);
  ~SpirvBasicBlock() = default;

  // Forbid copy construction and assignment
  SpirvBasicBlock(const SpirvBasicBlock &) = delete;
  SpirvBasicBlock &operator=(const SpirvBasicBlock &) = delete;

  // Forbid move construction and assignment
  SpirvBasicBlock(SpirvBasicBlock &&) = delete;
  SpirvBasicBlock &operator=(SpirvBasicBlock &&) = delete;

  /// Returns the label's <result-id> of this basic block.
  uint32_t getLabelId() const { return labelId; }

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

  /// Returns true if the last instruction in this basic block is a termination
  /// instruction.
  bool hasTerminator() const;

  /// Handle SPIR-V basic block visitors.
  bool invokeVisitor(Visitor *);

private:
  uint32_t labelId;      ///< The label's <result-id>
  std::string labelName; ///< The label's debug name

  /// Variables defined in this basic block.
  ///
  /// Local variables inside a function should be defined at the beginning
  /// of the entry basic block. So this field will only be used by entry
  /// basic blocks.
  std::vector<SpirvVariable *> variables;
  /// Instructions belonging to this basic block.
  std::vector<SpirvInstruction *> instructions;

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
