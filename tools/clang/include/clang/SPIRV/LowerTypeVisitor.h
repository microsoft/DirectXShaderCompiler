//===--- LowerTypeVisitor.h - AST type to SPIR-V type visitor ----*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_SPIRV_LOWERTYPEVISITOR_H
#define LLVM_CLANG_LIB_SPIRV_LOWERTYPEVISITOR_H

#include "clang/AST/ASTContext.h"
#include "clang/SPIRV/SPIRVContext.h"
#include "clang/SPIRV/SpirvVisitor.h"
#include "llvm/ADT/Optional.h"

namespace clang {
namespace spirv {

/// The class responsible to translate Clang frontend types into SPIR-V types.
class LowerTypeVisitor : public Visitor {
public:
  LowerTypeVisitor(ASTContext &astCtx, SpirvContext &spvCtx,
                   const SpirvCodeGenOptions &opts)
      : Visitor(opts, spvCtx), astContext(astCtx), spvContext(spvCtx) {}

  // Visiting different SPIR-V constructs.
  bool visit(SpirvModule *, Phase) { return true; }
  bool visit(SpirvFunction *, Phase);
  bool visit(SpirvBasicBlock *, Phase) { return true; }

  // Custom visitor for variables. Variables must have a pointer result-type.
  bool visit(SpirvVariable *);

  // Custom visitor for function parameters. We use pointer type for function
  // parameters.
  bool visit(SpirvFunctionParameter *);

  // Custom visitor for OpSampledImage. The result type of OpSampledImage should
  // be OpTypeSampledImage, but instruction stores the QualType for the
  // underlying image.
  bool visit(SpirvSampledImage *);

  // Custom visitor for sparse image operations: the result type must be the
  // Sparse Residency Struct.
  bool visit(SpirvImageOp *);

  /// The "sink" visit function for all instructions.
  ///
  /// By default, all other visit instructions redirect to this visit function.
  /// So that you want override this visit function to handle all instructions,
  /// regardless of their polymorphism.
  bool visitInstruction(SpirvInstruction *instr);

private:
  /// Emits error to the diagnostic engine associated with this visitor.
  template <unsigned N>
  DiagnosticBuilder emitError(const char (&message)[N], SourceLocation srcLoc) {
    const auto diagId = astContext.getDiagnostics().getCustomDiagID(
        clang::DiagnosticsEngine::Error, message);
    return astContext.getDiagnostics().Report(srcLoc, diagId);
  }

  /// Lowers the given AST QualType into the corresponding SPIR-V type.
  ///
  /// The lowering is recursive; all the types that the target type depends
  /// on will be created in SpirvContext.
  const SpirvType *lowerType(QualType type, SpirvLayoutRule, SourceLocation);
  /// Lowers the given Hybrid type into a SPIR-V type.
  ///
  /// Uses the above lowerType method to lower the QualType components of hybrid
  /// types.
  const SpirvType *lowerType(const SpirvType *, SpirvLayoutRule,
                             SourceLocation);

  /// Lowers the given HLSL resource type into its SPIR-V type.
  const SpirvType *lowerResourceType(QualType type, SpirvLayoutRule rule,
                                     SourceLocation);

  /// For the given sampled type, returns the corresponding image format
  /// that can be used to create an image object.
  spv::ImageFormat translateSampledTypeToImageFormat(QualType sampledType,
                                                     SourceLocation);

  /// Strips the attributes and typedefs from the given type and returns the
  /// desugared one.
  ///
  /// This method will update internal bookkeeping regarding matrix majorness.
  QualType desugarType(QualType type);

  /// Returns true if type is a HLSL row-major matrix, either with explicit
  /// attribute or implicit command-line option.
  bool isRowMajorMatrix(QualType type) const;

private:
  ASTContext &astContext;   /// AST context
  SpirvContext &spvContext; /// SPIR-V context

  /// A place to keep the matrix majorness attributes so that we can retrieve
  /// the information when really processing the desugared matrix type.
  ///
  /// This is needed because the majorness attribute is decorated on a
  /// TypedefType (i.e., floatMxN) of the real matrix type (i.e., matrix<elem,
  /// row, col>). When we reach the desugared matrix type, this information
  /// is already gone.
  llvm::Optional<AttributedType::Kind> typeMatMajorAttr;
};

} // end namespace spirv
} // end namespace clang

#endif // LLVM_CLANG_LIB_SPIRV_LOWERTYPEVISITOR_H
