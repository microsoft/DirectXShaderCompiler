//===--- LowerTypeVisitor.cpp - AST type to SPIR-V type impl -----*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include <sstream>

#include "DebugTypeVisitor.h"
#include "clang/SPIRV/SpirvBuilder.h"
#include "clang/SPIRV/SpirvModule.h"

namespace clang {
namespace spirv {

SpirvDebugInstruction *
DebugTypeVisitor::lowerToDebugType(const SpirvType *spirvType) {
  SpirvDebugInstruction *debugType = nullptr;

  switch (spirvType->getKind()) {
  case SpirvType::TK_Bool: {
    llvm::StringRef name = "bool";
    // TODO: Should we use 1 bit for booleans or 32 bits?
    uint32_t size = 32;
    // TODO: Use enums rather than uint32_t.
    uint32_t encoding = 2u;
    debugType = spvContext.getDebugTypeBasic(spirvType, name, size, encoding);
    break;
  }
  case SpirvType::TK_Integer: {
    auto *intType = dyn_cast<IntegerType>(spirvType);
    const uint32_t size = intType->getBitwidth();
    const bool isSigned = intType->isSignedInt();
    // TODO: Use enums rather than uint32_t.
    uint32_t encoding = isSigned ? 4u : 6u;
    std::string debugName = "";
    if (size == 32) {
      debugName = isSigned ? "int" : "uint";
    } else {
      std::ostringstream stream;
      stream << (isSigned ? "int" : "uint") << size << "_t";
      debugName = stream.str();
    }
    debugType =
        spvContext.getDebugTypeBasic(spirvType, debugName, size, encoding);
    break;
  }
  case SpirvType::TK_Float: {
    auto *floatType = dyn_cast<FloatType>(spirvType);
    const uint32_t size = floatType->getBitwidth();
    // TODO: Use enums rather than uint32_t.
    uint32_t encoding = 3u;
    std::string debugName = "";
    if (size == 32) {
      debugName = "float";
    } else {
      std::ostringstream stream;
      stream << "float" << size << "_t";
      debugName = stream.str();
    }
    debugType =
        spvContext.getDebugTypeBasic(spirvType, debugName, size, encoding);
    break;
  }
  case SpirvType::TK_Array: {
    auto *arrType = dyn_cast<ArrayType>(spirvType);
    SpirvDebugInstruction *elemDebugType =
        lowerToDebugType(arrType->getElementType());
    debugType = spvContext.getDebugTypeArray(spirvType, elemDebugType,
                                             {arrType->getElementCount()});
    break;
  }
  case SpirvType::TK_Vector: {
    auto *vecType = dyn_cast<VectorType>(spirvType);
    SpirvDebugInstruction *elemDebugType =
        lowerToDebugType(vecType->getElementType());
    debugType = spvContext.getDebugTypeArray(spirvType, elemDebugType,
                                             vecType->getElementCount());
    break;
  }
  case SpirvType::TK_Function: {
    auto *fnType = dyn_cast<FunctionType>(spirvType);
    SpirvDebugInstruction *returnType =
        lowerToDebugType(fnType->getReturnType());
    llvm::SmallVector<SpirvDebugInstruction *, 4> params;
    for (const auto *paramType : fnType->getParamTypes())
      params.push_back(lowerToDebugType(paramType));
    // TODO: Add mechanism to properly calculate the flags.
    // The info needed probably resides in clang::FunctionDecl.
    // This info can either be stored in the SpirvFunction class. Or,
    // alternatively the info can be stored in the SpirvContext.
    const uint32_t flags = 0;
    debugType =
        spvContext.getDebugTypeFunction(spirvType, flags, returnType, params);
    break;
  }
  }

  spvModule->addDebugInfo(debugType);
  return debugType;
}

bool DebugTypeVisitor::visitInstruction(SpirvDebugInstruction *instr) {

  // Set the result type of debug instructions to OpTypeVoid.
  // According to the OpenCL.DebugInfo.100 spec, all debug instructions are
  // OpExtInst with result type of void.
  instr->setAstResultType(astContext.VoidTy);
  instr->setResultType(spvContext.getVoidType());
  instr->setInstructionSet(spvBuilder.getOpenCLDebugInfoExtInstSet());

  // The following instructions are the only debug instructions that contain a
  // debug type:
  // DebugGlobalVariable
  // DebugLocalVariable
  // DebugFunction
  // DebugFunctionDeclaration
  // TODO: We currently don't have a SpirvDebugFunctionDeclaration class. Add
  // one if needed.
  if (isa<SpirvDebugGlobalVariable>(instr) ||
      isa<SpirvDebugLocalVariable>(instr) || isa<SpirvDebugFunction>(instr)) {
    const SpirvType *spirvType = instr->getResultType();
    if (spirvType) {
      SpirvDebugInstruction *debugType = lowerToDebugType(spirvType);
      instr->setDebugType(debugType);
    }
  }

  return true;
}

} // namespace spirv
} // namespace clang
