//===-- SpirvFunction.h - SPIR-V Function ---------------------*- C++ -*---===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_CLANG_SPIRV_SPIRVFUNCTION_H
#define LLVM_CLANG_SPIRV_SPIRVFUNCTION_H

#include <vector>

#include "clang/SPIRV/SpirvBasicBlock.h"
#include "clang/SPIRV/SpirvInstruction.h"
#include "llvm/ADT/SmallVector.h"

namespace clang {
namespace spirv {

/// The class representing a SPIR-V function in memory.
class SpirvFunction {
public:
  SpirvFunction(QualType type, uint32_t id, spv::FunctionControlMask);
  ~SpirvFunction() = default;

  // Forbid copy construction and assignment
  SpirvFunction(const SpirvFunction &) = delete;
  SpirvFunction &operator=(const SpirvFunction &) = delete;

  // Forbid move construction and assignment
  SpirvFunction(SpirvFunction &&) = delete;
  SpirvFunction &operator=(SpirvFunction &&) = delete;

private:
  QualType functionType;                    ///< This function's type
  uint32_t functionId;                      ///< This function's <result-id>
  spv::FunctionControlMask functionControl; ///< SPIR-V function control

  /// Parameters to this function.
  llvm::SmallVector<SpirvFunctionParameter *, 8> parameters;

  /// Variables defined in this function.
  ///
  /// Local variables inside a function should be defined at the beginning
  /// of the entry basic block. This serves as a temporary place for holding
  /// these variables.
  std::vector<SpirvVariable *> variables;

  /// Basic blocks inside this function.
  std::vector<SpirvBasicBlock *> basicBlocks;
};

} // end namespace spirv
} // end namespace clang

#endif // LLVM_CLANG_SPIRV_SPIRVFUNCTION_H
