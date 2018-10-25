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
  bool invokeVisitor(Visitor *);

  // Add a function to the list of module functions.
  void addFunction(SpirvFunction *);

  // Add a capability to the list of module capabilities.
  void addCapability(SpirvCapability *cap);

  // Set the memory model of the module.
  void setMemoryModel(SpirvMemoryModel *model);

  // Add an entry point to the module.
  void addEntryPoint(SpirvEntryPoint *);

  // Adds an execution mode to the module.
  void addExecutionMode(SpirvExecutionMode *);

  // Adds an extension to the module.
  void addExtension(SpirvExtension *);

  // Adds an extended instruction set to the module.
  void addExtInstSet(SpirvExtInstImport *);

  // Returns the GLSL extended instruction set if already added to the module.
  // Returns nullptr otherwise.
  SpirvExtInstImport *getGLSLExtInstSet();

  // Adds a variable to the module.
  void addVariable(SpirvVariable *);

  // Adds a decoration to the module.
  void addDecoration(SpirvDecoration *);

  void setShaderModelVersion(uint32_t v) { shaderModelVersion = v; }
  void setSourceFileName(llvm::StringRef name) { sourceFileName = name; }
  void setSourceFileContent(llvm::StringRef c) { sourceFileContent = c; }
  void setBound(uint32_t b) { bound = b; }

private:
  uint32_t bound;
  uint32_t shaderModelVersion;

  // "Metadata" instructions
  llvm::SmallVector<SpirvCapability *, 8> capabilities;
  llvm::SmallVector<SpirvExtension *, 4> extensions;
  llvm::SmallVector<SpirvExtInstImport *, 1> extInstSets;
  SpirvMemoryModel *memoryModel;
  llvm::SmallVector<SpirvEntryPoint *, 1> entryPoints;
  llvm::SmallVector<SpirvExecutionMode *, 4> executionModes;
  SpirvSource *debugSource;
  std::vector<SpirvDecoration *> decorations;
  std::vector<SpirvTypeConstant *> typeConstants;
  std::vector<SpirvVariable *> variables;

  // Shader logic instructions
  std::vector<SpirvFunction *> functions;
  std::string sourceFileName;
  std::string sourceFileContent;
};

} // end namespace spirv
} // end namespace clang

#endif // LLVM_CLANG_SPIRV_SPIRVMODULE_H
