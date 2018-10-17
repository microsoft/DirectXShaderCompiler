//===-- SpirvBuilder.h - SPIR-V Builder -----------------------*- C++ -*---===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_CLANG_SPIRV_SPIRVBUILDER_H
#define LLVM_CLANG_SPIRV_SPIRVBUILDER_H

#include "clang/SPIRV/SPIRVContext.h"
#include "clang/SPIRV/SpirvBasicBlock.h"
#include "clang/SPIRV/SpirvFunction.h"
#include "clang/SPIRV/SpirvInstruction.h"
#include "clang/SPIRV/SpirvModule.h"
#include "llvm/ADT/MapVector.h"

namespace clang {
namespace spirv {

/// The SPIR-V in-memory representation builder class.
///
/// This class exports API for constructing SPIR-V in-memory representation
/// interactively. Under the hood, it allocates SPIR-V entity objects from
/// SpirvContext and wires them up into a connected structured representation.
///
/// At any time, there can only exist at most one function under building;
/// but there can exist multiple basic blocks under construction.
///
/// Call `getModule()` to get the SPIR-V words after finishing building the
/// module.
class SpirvBuilder {
public:
  explicit SpirvBuilder(SpirvContext &c);
  ~SpirvBuilder() = default;

  // Forbid copy construction and assignment
  SpirvBuilder(const SpirvBuilder &) = delete;
  SpirvBuilder &operator=(const SpirvBuilder &) = delete;

  // Forbid move construction and assignment
  SpirvBuilder(SpirvBuilder &&) = delete;
  SpirvBuilder &operator=(SpirvBuilder &&) = delete;

  /// Returns the SPIR-V module being built.
  SpirvModule *getModule() { return module; }

  // === Function and Basic Block ===

  /// \brief Begins building a SPIR-V function by allocating a SpirvFunction
  /// object. Returns the pointer for the function on success. Returns nullptr
  /// on failure.
  ///
  /// At any time, there can only exist at most one function under building.
  SpirvFunction *beginFunction(QualType returnType, SourceLocation,
                               llvm::StringRef name = "");

  /// \brief Creates and registers a function parameter of the given pointer
  /// type in the current function and returns its pointer.
  SpirvFunctionParameter *addFnParam(QualType ptrType, SourceLocation,
                                     llvm::StringRef name = "");

  /// \brief Creates a local variable of the given type in the current
  /// function and returns it.
  ///
  /// The corresponding pointer type of the given type will be constructed in
  /// this method for the variable itself.
  SpirvVariable *addFnVar(QualType valueType, SourceLocation,
                          llvm::StringRef name = "",
                          SpirvInstruction *init = nullptr);

  /// \brief Ends building of the current function. All basic blocks constructed
  /// from the beginning or after ending the previous function will be collected
  /// into this function.
  void endFunction();

  /// \brief Creates a SPIR-V basic block. On success, returns the <label-id>
  /// for the basic block. On failure, returns zero.
  SpirvBasicBlock *createBasicBlock(llvm::StringRef name);

  /// \brief Adds the basic block with the given label as a successor to the
  /// current basic block.
  void addSuccessor(SpirvBasicBlock *successorBB);

  /// \brief Sets the merge target to the given basic block.
  /// The caller must make sure the current basic block contains an
  /// OpSelectionMerge or OpLoopMerge instruction.
  void setMergeTarget(SpirvBasicBlock *mergeLabel);

  /// \brief Sets the continue target to the given basic block.
  /// The caller must make sure the current basic block contains an
  /// OpLoopMerge instruction.
  void setContinueTarget(SpirvBasicBlock *continueLabel);

  /// \brief Returns true if the current basic block inserting into is
  /// terminated.
  inline bool isCurrentBasicBlockTerminated() const {
    return insertPoint && insertPoint->hasTerminator();
  }

  /// \brief Sets insertion point to the given basic block.
  inline void setInsertPoint(SpirvBasicBlock *bb) { insertPoint = bb; }

private:
  SpirvContext &context; ///< From which we allocate various SPIR-V object

  SpirvModule *module;          ///< The current module being built
  SpirvFunction *function;      ///< The current function being built
  SpirvBasicBlock *insertPoint; ///< The current basic block being built

  /// \brief List of basic blocks being built.
  ///
  /// We need a vector here to remember the order of insertion. Order matters
  /// here since, for example, we'll know for sure the first basic block is
  /// the entry block.
  std::vector<SpirvBasicBlock *> basicBlocks;
};

} // end namespace spirv
} // end namespace clang

#endif // LLVM_CLANG_SPIRV_SPIRVBUILDER_H
