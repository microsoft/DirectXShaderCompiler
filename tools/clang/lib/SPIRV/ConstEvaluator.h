//===-------- ConstEvaluator.h ----- Translate Constants --------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//===----------------------------------------------------------------------===//
//
//  This file defines methods for translating AST expressions to SPIR-V
//  constants.
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_CLANG_SPIRV_CONSTEVALUATOR_H
#define LLVM_CLANG_SPIRV_CONSTEVALUATOR_H

#include "clang/AST/ASTContext.h"
#include "clang/SPIRV/SpirvBuilder.h"
#include "llvm/ADT/APInt.h"

namespace clang {
namespace spirv {

class ConstEvaluator {
public:
  ConstEvaluator(ASTContext &astContext, SpirvBuilder &spvBuilder)
      : astContext(astContext), spvBuilder(spvBuilder) {}
  SpirvConstant *translateAPInt(const llvm::APInt &intValue,
                                const QualType targetType,
                                bool isSpecConstantMode);
  SpirvConstant *translateAPFloat(llvm::APFloat floatValue, QualType targetType,
                                  bool isSpecConstantMode);
  SpirvConstant *tryToEvaluateAsInt32(const llvm::APInt &intValue,
                                      bool isSigned);
  SpirvConstant *tryToEvaluateAsFloat32(const llvm::APFloat &floatValue,
                                        bool isSpecConstantMode);
  SpirvConstant *tryToEvaluateAsConst(const Expr *expr,
                                      bool isSpecConstantMode);

private:
  SpirvConstant *translateAPValue(const APValue &value,
                                  const QualType targetType,
                                  bool isSpecConstantMode);

  /// Emits error to the diagnostic engine associated with the AST context.
  template <unsigned N>
  DiagnosticBuilder emitError(const char (&message)[N],
                              SourceLocation srcLoc = {}) {
    const auto diagId = astContext.getDiagnostics().getCustomDiagID(
        clang::DiagnosticsEngine::Error, message);
    return astContext.getDiagnostics().Report(srcLoc, diagId);
  }

  ASTContext &astContext;
  SpirvBuilder &spvBuilder;
};

} // namespace spirv
} // namespace clang

#endif // LLVM_CLANG_SPIRV_CONSTEVALUATOR_H
