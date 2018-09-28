//===-- SpirvModule.h - SPIR-V Module -------------------------*- C++ -*---===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_CLANG_SPIRV_SPIRVMODULE_H
#define LLVM_CLANG_SPIRV_SPIRVMODULE_H

#include <vector>

#include "clang/SPIRV/SpirvFunction.h"
#include "clang/SPIRV/SpirvInstruction.h"
#include "llvm/ADT/SmallVector.h"

namespace clang {
namespace spirv {

class SpirvVisitor;

// TODO: flesh this out
class SpirvTypeConstant;

/// The class representing a SPIR-V module in memory.
///
/// A SPIR-V module contains two main parts: instructions for "metadata" (e.g.,
/// required capabilities and used types) and instructions for shader logic.
/// The former consists of the instructions before the function section in
/// SPIR-V logical layout; while the later is what are in the function section.
///
/// The SpirvBuilder class should be used to gradually build up the second part.
/// After the SpirvBuilder completes its tasks, the first part should be filled
/// out by traversing the second part built by the SpirvBuilder.
///
/// This representation is a just a minimal collection of SPIR-V entities;
/// it does not provide much sanity check over the integrity among the enclosed
/// entities, which modifying classes should be responsible for.
class SpirvModule {
public:
  SpirvModule();
  ~SpirvModule() = default;

  // Forbid copy construction and assignment
  SpirvModule(const SpirvModule &) = delete;
  SpirvModule &operator=(const SpirvModule &) = delete;

  // Forbid move construction and assignment
  SpirvModule(SpirvModule &&) = delete;
  SpirvModule &operator=(SpirvModule &&) = delete;

  // Handle SPIR-V module visitors.
  bool visit(Visitor *);

private:
  uint32_t bound; ///< The <result-id> bound: the next unused one

  // "Metadata" instructions
  llvm::SmallVector<SpirvCapability *, 8> capabilities;
  llvm::SmallVector<SpirvExtension *, 4> extensions;
  llvm::SmallVector<SpirvExtInstImport *, 1> extInstSets;
  SpirvMemoryModel *memoryModel;
  llvm::SmallVector<SpirvEntryPoint *, 1> entryPoints;
  llvm::SmallVector<SpirvExecutionMode *, 4> executionModes;
  SpirvSource *debugSource;
  std::vector<SpirvName *> debugNames;
  std::vector<SpirvDecoration *> decorations;
  std::vector<SpirvTypeConstant *> typeConstants;
  std::vector<SpirvVariable *> variables;

  // Shader logic instructions
  std::vector<SpirvFunction *> functions;
};

} // end namespace spirv
} // end namespace clang

#endif // LLVM_CLANG_SPIRV_SPIRVMODULE_H
