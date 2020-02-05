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

#include "clang/SPIRV/SpirvInstruction.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallVector.h"

namespace clang {
namespace spirv {

class SpirvFunction;
class SpirvVisitor;

struct ExtensionComparisonInfo {
  static inline SpirvExtension *getEmptyKey() { return nullptr; }
  static inline SpirvExtension *getTombstoneKey() { return nullptr; }
  static unsigned getHashValue(const SpirvExtension *ext) {
    return llvm::hash_combine(ext->getExtensionName());
  }
  static bool isEqual(SpirvExtension *LHS, SpirvExtension *RHS) {
    // Either both are null, or both should have the same underlying extension.
    return (LHS == RHS) || (LHS && RHS && *LHS == *RHS);
  }
};

struct DecorationComparisonInfo {
  static inline SpirvDecoration *getEmptyKey() { return nullptr; }
  static inline SpirvDecoration *getTombstoneKey() { return nullptr; }
  static unsigned getHashValue(const SpirvDecoration *decor) {
    return llvm::hash_combine(decor->getTarget(),
                              static_cast<uint32_t>(decor->getDecoration()));
  }
  static bool isEqual(SpirvDecoration *LHS, SpirvDecoration *RHS) {
    // Either both are null, or both should have the same underlying decoration.
    return (LHS == RHS) || (LHS && RHS && *LHS == *RHS);
  }
};

struct CapabilityComparisonInfo {
  static inline SpirvCapability *getEmptyKey() { return nullptr; }
  static inline SpirvCapability *getTombstoneKey() { return nullptr; }
  static unsigned getHashValue(const SpirvCapability *cap) {
    return llvm::hash_combine(static_cast<uint32_t>(cap->getCapability()));
  }
  static bool isEqual(SpirvCapability *LHS, SpirvCapability *RHS) {
    // Either both are null, or both should have the same underlying capability.
    return (LHS == RHS) || (LHS && RHS && *LHS == *RHS);
  }
};

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
  bool invokeVisitor(Visitor *, bool reverseOrder = false);

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

  // Returns the extended instruction set with the given name if already added
  // to the module. Returns nullptr otherwise.
  SpirvExtInstImport *getExtInstSet(llvm::StringRef name);

  // Adds a variable to the module.
  void addVariable(SpirvVariable *);

  // Adds a decoration to the module.
  void addDecoration(SpirvDecoration *);

  // Adds a constant to the module.
  void addConstant(SpirvConstant *);

  // Adds the debug source to the module.
  void addSource(SpirvSource *);

  // Adds the given DebugInfoNone, DebugOperation, and DebugExpression
  // instruction to debugNone, debugOp, and debugExpr, respectively.
  void addDebugInfo(SpirvDebugInfoNone *none) { debugNone = none; }
  void addDebugInfo(SpirvDebugOperation *);
  void addDebugInfo(SpirvDebugExpression *);

  // Adds the given DebugSource and DebugCompilationUnit instruction to
  // debugSources and debugCompUnits, respectively.
  void addDebugInfo(SpirvDebugSource *);
  void addDebugInfo(SpirvDebugCompilationUnit *);

  // Adds the given DebugTypeXXX instruction to debugTypes. Since each type is
  // defined in a parent lexical scope, we must keep type under parent.
  void addDebugInfo(SpirvDebugType *type);

  // Adds the given DebugGlobalVariable instruction to debugVariables.
  void addDebugInfo(SpirvDebugGlobalVariable *);

  // Adds the given DebugFunction instruction to debugLexicalScopes.
  void addDebugInfo(SpirvDebugFunction *);

  // Adds the given DebugLocalVariable instruction to debugVariables.
  void addDebugInfo(SpirvDebugLocalVariable *);

  // Adds the given DebugLexicalBlock instruction to debugLexicalScopes.
  void addDebugInfo(SpirvDebugLexicalBlock *);

  // Adds the given debug info instruction to debugInfo.
  void addDebugInfo(SpirvDebugInstruction *);

  // Adds the given OpModuleProcessed to the module.
  void addModuleProcessed(SpirvModuleProcessed *);

private:
  // Handle visitors for debug info instructions.
  bool invokeVisitorDebugInfo(Visitor *, bool reverseOrder = false);

  // Handle visitors for lexical scope debug info instructions.
  bool invokeVisitorDebugLexicalScope(Visitor *visitor,
                                      SpirvDebugInstruction *scope,
                                      bool reverseOrder = false);

private:
  // Use a set for storing capabilities. This will ensure there are no duplicate
  // capabilities. Although the set stores pointers, the provided
  // CapabilityComparisonInfo compares the SpirvCapability objects, not the
  // pointers.
  llvm::SetVector<SpirvCapability *, std::vector<SpirvCapability *>,
                  llvm::DenseSet<SpirvCapability *, CapabilityComparisonInfo>>
      capabilities;

  // Use a set for storing extensions. This will ensure there are no duplicate
  // extensions. Although the set stores pointers, the provided
  // ExtensionComparisonInfo compares the SpirvExtension objects, not the
  // pointers.
  llvm::SetVector<SpirvExtension *, std::vector<SpirvExtension *>,
                  llvm::DenseSet<SpirvExtension *, ExtensionComparisonInfo>>
      extensions;

  llvm::SmallVector<SpirvExtInstImport *, 1> extInstSets;
  SpirvMemoryModel *memoryModel;
  llvm::SmallVector<SpirvEntryPoint *, 1> entryPoints;
  llvm::SmallVector<SpirvExecutionMode *, 4> executionModes;
  std::vector<SpirvSource *> sources;
  std::vector<SpirvModuleProcessed *> moduleProcesses;

  // Use a set for storing decoration. This will ensure that we don't apply the
  // same decoration to the same target more than once. Although the set stores
  // pointers, the provided DecorationComparisonInfo compares the
  // SpirvDecoration objects, not the pointers.
  llvm::SetVector<SpirvDecoration *, std::vector<SpirvDecoration *>,
                  llvm::DenseSet<SpirvDecoration *, DecorationComparisonInfo>>
      decorations;

  std::vector<SpirvConstant *> constants;
  std::vector<SpirvVariable *> variables;
  std::vector<SpirvFunction *> functions;

  // We should emit debug info instructions in the order of
  // 1. DebugOperation
  // 2. DebugExpression
  // 3. For each DebugSource
  // 4. DebugCompilationUnit
  // 5. DebugTypeXXX
  // 6. DebugGlobalVariable
  // 7. For each DebugFunction
  //    7.1. DebugTypeXXX defined in this function
  //    7.2. DebugGlobalVariable or DebugLocalVariable defined in
  //         this function
  //    7.3. For each DebugLexicalBlock defined in this function
  //         7.3.1. DebugTypeXXX defined in block
  //         7.3.2. DebugLocalVariable defined in block
  //
  // If we do not follow this order, generated SPIR-V code would
  // be invalid because of forward references.

  // Keep DebugOperation and DebugExpression.
  SpirvDebugInfoNone *debugNone;
  std::vector<SpirvDebugOperation *> debugOp;
  std::vector<SpirvDebugExpression *> debugExpr;

  // Keep DebugSource and DebugCompilationUnit.
  std::vector<SpirvDebugSource *> debugSources;
  std::vector<SpirvDebugCompilationUnit *> debugCompUnits;

  // Keep DebugTypeXXX. Each debug type has a lexical scope parent
  // where it is defined. This is a map between a lexical scope
  // parent and types defined in it.
  llvm::DenseMap<SpirvDebugInstruction *, std::vector<SpirvDebugType *>>
      debugTypes;

  // Keep DebugGlobalVariable and DebugLocalVariable.
  llvm::DenseMap<SpirvDebugInstruction *, std::vector<SpirvDebugInstruction *>>
      debugVariables;

  // Keep DebugFunction and DebugLexicalBlock. Each scope has a
  // lexical scope parent where it is defined. This is a map
  // between a lexical scope parent and scopes defined in it.
  llvm::DenseMap<SpirvDebugInstruction *, std::vector<SpirvDebugInstruction *>>
      debugLexicalScopes;

  // All other debug info instructions that are not sensitive about the layout.
  // Note that DebugScope, DebugNoScope, DebugDeclare, DebugValue must be
  // included in functions.
  std::vector<SpirvDebugInstruction *> debugInfo;
};

} // end namespace spirv
} // end namespace clang

#endif // LLVM_CLANG_SPIRV_SPIRVMODULE_H
