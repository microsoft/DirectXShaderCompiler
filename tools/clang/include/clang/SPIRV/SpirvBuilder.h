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

private:
  /// \brief Map from basic blocks' <label-id> to their objects.
  ///
  /// We need MapVector here to remember the order of insertion. Order matters
  /// here since, for example, we'll know for sure the first basic block is
  /// the entry block.
  using OrderedBasicBlockMap = llvm::MapVector<uint32_t, SpirvBasicBlock *>;

  SpirvContext &context; ///< From which we allocate various SPIR-V object

  SpirvModule *module;              ///< The current module being built
  SpirvFunction *function;          ///< The current function being built
  OrderedBasicBlockMap basicBlocks; ///< The current basic block being built
};

} // end namespace spirv
} // end namespace clang

#endif // LLVM_CLANG_SPIRV_SPIRVBUILDER_H
