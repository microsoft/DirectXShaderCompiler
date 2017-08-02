//===--- TypeTranslator.h - AST type to SPIR-V type translator ---*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_SPIRV_TYPETRANSLATOR_H
#define LLVM_CLANG_SPIRV_TYPETRANSLATOR_H

#include "clang/AST/Type.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/SPIRV/ModuleBuilder.h"

namespace clang {
namespace spirv {

/// The class responsible to translate Clang frontend types into SPIR-V type
/// instructions.
///
/// SPIR-V type instructions generated during translation will be emitted to
/// the SPIR-V module builder passed into the constructor.
/// Warnings and errors during the translation will be reported to the
/// DiagnosticEngine passed into the constructor.
class TypeTranslator {
public:
  TypeTranslator(ModuleBuilder &builder, DiagnosticsEngine &diag)
      : theBuilder(builder), diags(diag) {}

  /// \brief Generates the corresponding SPIR-V type for the given Clang
  /// frontend type and returns the type's <result-id>. On failure, reports
  /// the error and returns 0.
  ///
  /// The translation is recursive; all the types that the target type depends
  /// on will be generated.
  uint32_t translateType(QualType type);

  /// \brief Returns true if the givne type is a 1x1 matrix type.
  static bool is1x1MatrixType(QualType type);

  /// \brief Returns true if the givne type is a 1xN (N > 1) matrix type.
  static bool is1xNMatrixType(QualType type);

  /// \brief Returns true if the given type is a SPIR-V acceptable matrix type,
  /// i.e., with floating point elements and greater than 1 row and column
  /// counts.
  static bool isSpirvAcceptableMatrixType(QualType type);

  /// \brief Generates the corresponding SPIR-V vector type for the given Clang
  /// frontend matrix type's vector component and returns the <result-id>.
  ///
  /// This method will panic if the given matrix type is not a SPIR-V acceptable
  /// matrix type.
  uint32_t getComponentVectorType(QualType matrixType);

private:
  /// \brief Wrapper method to create an error message and report it
  /// in the diagnostic engine associated with this consumer.
  template <unsigned N> DiagnosticBuilder emitError(const char (&message)[N]) {
    const auto diagId =
        diags.getCustomDiagID(clang::DiagnosticsEngine::Error, message);
    return diags.Report(diagId);
  }

private:
  ModuleBuilder &theBuilder;
  DiagnosticsEngine &diags;
};

} // end namespace spirv
} // end namespace clang

#endif