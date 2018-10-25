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
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"

namespace clang {
namespace spirv {

class SpirvVisitor;

/// The class representing a SPIR-V function in memory.
class SpirvFunction {
public:
  SpirvFunction(QualType type, uint32_t id, spv::FunctionControlMask,
                SourceLocation, llvm::StringRef name = "");
  ~SpirvFunction() = default;

  // Forbid copy construction and assignment
  SpirvFunction(const SpirvFunction &) = delete;
  SpirvFunction &operator=(const SpirvFunction &) = delete;

  // Forbid move construction and assignment
  SpirvFunction(SpirvFunction &&) = delete;
  SpirvFunction &operator=(SpirvFunction &&) = delete;

  // Handle SPIR-V function visitors.
  bool invokeVisitor(Visitor *);

  // TODO: The responsibility of assigning the result-id of a function shouldn't
  // be on the function itself.
  uint32_t getResultId() const { return functionId; }

  // TODO: There should be a pass for lowering QualType to SPIR-V type,
  // and this method should be able to return the result-id of the SPIR-V type.
  // Both the return type of the function as well as the SPIR-V "function type"
  // are needed. SPIR-V function type (obtained by OpFunctionType) includes both
  // the return type as well as argument types.
  uint32_t getReturnTypeId() const { return returnTypeId; }
  void setReturnTypeId(uint32_t id) { returnTypeId = id; }

  // Sets the lowered (SPIR-V) function type.
  void setReturnType(SpirvType *type) { returnType = type; }
  // Returns the lowered (SPIR-V) function type.
  const SpirvType *getReturnType() const { return returnType; }

  // Sets the SPIR-V type of the function
  void setFunctionType(FunctionType *type) { fnType = type; }
  // Returns the SPIR-V type of the function
  FunctionType *getFunctionType() const { return fnType; }

  // Sets the result-id of the OpTypeFunction
  void setFunctionTypeId(uint32_t id) { fnTypeId = id; }
  // Returns the result-id of the OpTypeFunction
  uint32_t getFunctionTypeId() const { return fnTypeId; }

  void setFunctionName(llvm::StringRef name) { functionName = name; }
  llvm::StringRef getFunctionName() const { return functionName; }

  void addParameter(SpirvFunctionParameter *);
  void addVariable(SpirvVariable *);
  void addBasicBlock(SpirvBasicBlock *);

private:
  uint32_t functionId; ///< This function's <result-id>

  QualType astReturnType; ///< The return type
  SpirvType *returnType;  ///< The lowered return type
  uint32_t returnTypeId;  ///< result-id for the return type

  FunctionType *fnType; ///< The SPIR-V function type
  uint32_t fnTypeId;    ///< result-id for the SPIR-V function type

  spv::FunctionControlMask functionControl; ///< SPIR-V function control
  SourceLocation functionLoc;               ///< Location in source code
  std::string functionName;                 ///< This function's name

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
