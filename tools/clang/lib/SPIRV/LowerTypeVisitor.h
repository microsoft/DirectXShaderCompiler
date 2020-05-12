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

#include "AlignmentSizeCalculator.h"
#include "clang/AST/ASTContext.h"
#include "clang/SPIRV/SpirvBuilder.h"
#include "clang/SPIRV/SpirvContext.h"
#include "clang/SPIRV/SpirvVisitor.h"
#include "llvm/ADT/Optional.h"

namespace clang {
namespace spirv {

/// The class responsible to translate Clang frontend types into SPIR-V types.
class LowerTypeVisitor : public Visitor {
public:
  LowerTypeVisitor(ASTContext &astCtx, SpirvContext &spvCtx,
                   const SpirvCodeGenOptions &opts, SpirvBuilder &builder,
                   SpirvExtInstImport *debugExt)
      : Visitor(opts, spvCtx), astContext(astCtx), spvContext(spvCtx),
        spvBuilder(builder), alignmentCalc(astCtx, opts),
        debugExtInstSet(debugExt) {}

  // Visiting different SPIR-V constructs.
  bool visit(SpirvModule *, Phase) { return true; }
  bool visit(SpirvFunction *, Phase);
  bool visit(SpirvBasicBlock *, Phase) { return true; }

  /// The "sink" visit function for all instructions.
  ///
  /// By default, all other visit instructions redirect to this visit function.
  /// So that you want override this visit function to handle all instructions,
  /// regardless of their polymorphism.
  bool visitInstruction(SpirvInstruction *instr);

private:
  /// Emits error to the diagnostic engine associated with this visitor.
  template <unsigned N>
  DiagnosticBuilder emitError(const char (&message)[N],
                              SourceLocation srcLoc = {}) {
    const auto diagId = astContext.getDiagnostics().getCustomDiagID(
        clang::DiagnosticsEngine::Error, message);
    return astContext.getDiagnostics().Report(srcLoc, diagId);
  }

  /// Lowers the given AST QualType into the corresponding SPIR-V type.
  ///
  /// The lowering is recursive; all the types that the target type depends
  /// on will be created in SpirvContext.
  const SpirvType *lowerType(QualType type, SpirvLayoutRule,
                             llvm::Optional<bool> isRowMajor, SourceLocation);
  /// Lowers the given Hybrid type into a SPIR-V type.
  ///
  /// Uses the above lowerType method to lower the QualType components of hybrid
  /// types.
  const SpirvType *lowerType(const SpirvType *, SpirvLayoutRule,
                             SourceLocation);

  /// Lowers the given HLSL resource type into its SPIR-V type.
  ///
  /// Returns the lowered SpirvType and its underlying SpirvType.
  std::pair<const SpirvType *, const SpirvType *>
  lowerResourceType(QualType type, SpirvLayoutRule rule, SourceLocation);

  /// For the given sampled type, returns the corresponding image format
  /// that can be used to create an image object.
  spv::ImageFormat translateSampledTypeToImageFormat(QualType sampledType,
                                                     SourceLocation);

  /// Generate debug info of a function.
  ///
  /// When there is no function call for decl, SpirvEmitter does not
  /// generate function definition nor function info. However, if it is
  /// a member method of a struct/class, we need to generate its debug
  /// info.
  SpirvDebugFunction *generateFunctionInfo(const CXXMethodDecl *decl,
                                           SpirvDebugInstruction *parent);

private:
  /// Calculates all layout information needed for the given structure fields.
  /// Returns the lowered field info vector.
  /// In other words: lowers the HybridStructType field information to
  /// StructType field information.
  llvm::SmallVector<StructType::FieldInfo, 4>
  populateLayoutInformation(llvm::ArrayRef<HybridStructType::FieldInfo> fields,
                            SpirvLayoutRule rule);

  /// Generate rich debug info of a composite type from a QualType (RecordType).
  SpirvDebugTypeComposite *lowerDebugTypeComposite(
      const RecordType *structType, const SpirvType *spirvType,
      llvm::SmallVector<StructType::FieldInfo, 4> &fields, bool isResourceType);

private:
  ASTContext &astContext;                /// AST context
  SpirvContext &spvContext;              /// SPIR-V context
  SpirvBuilder &spvBuilder;              /// SPIR-V builder
  AlignmentSizeCalculator alignmentCalc; /// alignment calculator

  // TODO: Adding a pointer the debugInfoExtInstSet in the LowerTypeVisitor
  // class is too intrusive. Clean it up.
  SpirvExtInstImport *debugExtInstSet; /// Pointer to
                                       /// OpenCLDebugInfoExtInstSet

  llvm::SmallSet<const Decl *, 4> visitedRecordDecl; /// A set of already
                                                     /// visited RecordDecl
};

} // end namespace spirv
} // end namespace clang

#endif // LLVM_CLANG_LIB_SPIRV_LOWERTYPEVISITOR_H
