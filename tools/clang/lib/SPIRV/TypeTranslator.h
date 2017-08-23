//===--- TypeTranslator.h - AST type to SPIR-V type translator ---*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_SPIRV_TYPETRANSLATOR_H
#define LLVM_CLANG_LIB_SPIRV_TYPETRANSLATOR_H

#include <utility>

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
  TypeTranslator(ASTContext &context, ModuleBuilder &builder,
                 DiagnosticsEngine &diag)
      : astContext(context), theBuilder(builder), diags(diag) {}

  /// \brief Generates the corresponding SPIR-V type for the given Clang
  /// frontend type and returns the type's <result-id>. On failure, reports
  /// the error and returns 0. If decorateLayout is true, layout decorations
  /// (Offset, MatrixStride, ArrayStride, RowMajor, ColMajor) will be attached
  /// to the struct or array types. If decorateLayout is true and type is a
  /// matrix or array of matrix type, isRowMajor will indicate whether it is
  /// decorated with row_major in the source code.
  ///
  /// The translation is recursive; all the types that the target type depends
  /// on will be generated and all with layout decorations (if decorateLayout
  /// is true).
  uint32_t translateType(QualType type, bool decorateLayout = false,
                         bool isRowMajor = false);

  /// \brief Returns true if the given type is the HLSL ByteAddressBufferType.
  bool isByteAddressBuffer(QualType type);

  /// \brief Returns true if the given type is the HLSL RWByteAddressBufferType.
  bool isRWByteAddressBuffer(QualType type);

  /// \brief Returns true if the given type will be translated into a SPIR-V
  /// scalar type. This includes normal scalar types, vectors of size 1, and
  /// 1x1 matrices. If scalarType is not nullptr, writes the scalar type to
  /// *scalarType.
  static bool isScalarType(QualType type, QualType *scalarType = nullptr);

  /// \breif Returns true if the given type will be translated into a SPIR-V
  /// vector type. This includes normal types (either ExtVectorType or HLSL
  /// vector type) with more than one elements and matrices with exactly one
  /// row or one column. Writes the element type and count into *elementType and
  /// *count respectively if they are not nullptr.
  static bool isVectorType(QualType type, QualType *elemType = nullptr,
                           uint32_t *count = nullptr);

  /// \brief Returns true if the given type is a 1x1 matrix type.
  /// If elemType is not nullptr, writes the element type to *elemType.
  static bool is1x1Matrix(QualType type, QualType *elemType = nullptr);

  /// \brief Returns true if the given type is a 1xN (N > 1) matrix type.
  /// If elemType is not nullptr, writes the element type to *elemType.
  /// If count is not nullptr, writes the value of N into *count.
  static bool is1xNMatrix(QualType type, QualType *elemType = nullptr,
                          uint32_t *count = nullptr);

  /// \brief Returns true if the given type is a Mx1 (M > 1) matrix type.
  /// If elemType is not nullptr, writes the element type to *elemType.
  /// If count is not nullptr, writes the value of M into *count.
  static bool isMx1Matrix(QualType type, QualType *elemType = nullptr,
                          uint32_t *count = nullptr);

  /// \brief returns true if the given type is a matrix with more than 1 row and
  /// more than 1 column.
  /// If elemType is not nullptr, writes the element type to *elemType.
  /// If rowCount is not nullptr, writes the number of rows (M) into *rowCount.
  /// If colCount is not nullptr, writes the number of cols (N) into *colCount.
  static bool isMxNMatrix(QualType type, QualType *elemType = nullptr,
                          uint32_t *rowCount = nullptr,
                          uint32_t *colCount = nullptr);

  /// \brief Returns true if the given type is a SPIR-V acceptable matrix type,
  /// i.e., with floating point elements and greater than 1 row and column
  /// counts.
  static bool isSpirvAcceptableMatrixType(QualType type);

  /// \brief Returns the the element type for the given scalar/vector/matrix
  /// type. Returns empty QualType for other cases.
  QualType getElementType(QualType type);

  /// \brief Generates the corresponding SPIR-V vector type for the given Clang
  /// frontend matrix type's vector component and returns the <result-id>.
  ///
  /// This method will panic if the given matrix type is not a SPIR-V acceptable
  /// matrix type.
  uint32_t getComponentVectorType(QualType matrixType);

  /// \brief Generates layout decorations (Offset, MatrixStride, RowMajor,
  /// ColMajor) for the given type.
  ///
  /// This method is not recursive; it only handles the top-level member/field
  /// of the given DeclContext. Besides, it does not handle ArrayStride, which
  /// according to the spec, must be attached to the array type itself instead
  /// of a struct member.
  llvm::SmallVector<const Decoration *, 4>
  getLayoutDecorations(const DeclContext *decl);

private:
  /// \brief Wrapper method to create an error message and report it
  /// in the diagnostic engine associated with this consumer.
  template <unsigned N> DiagnosticBuilder emitError(const char (&message)[N]) {
    const auto diagId =
        diags.getCustomDiagID(clang::DiagnosticsEngine::Error, message);
    return diags.Report(diagId);
  }

  /// \brief Translates the given HLSL resource type into its SPIR-V
  /// instructions and returns the <result-id>. Returns 0 on failure.
  uint32_t translateResourceType(QualType type);

  /// \brief Returns the alignment and size in bytes for the given type
  /// according to std140.

  /// If the type is an array/matrix type, writes the array/matrix stride to
  /// stride. If the type is a matrix, isRowMajor will be used to indicate
  /// whether it is labelled as row_major in the source code.
  ///
  /// Note that the size returned is not exactly how many bytes the type
  /// will occupy in memory; rather it is used in conjunction with alignment
  /// to get the next available location (alignment + size), which means
  /// size contains post-paddings required by the given type.
  std::pair<uint32_t, uint32_t>
  getAlignmentAndSize(QualType type, uint32_t *stride, bool isRowMajor);

  /// \bried For the given sampled type, returns the corresponding image format
  /// that can be used to create an image object.
  spv::ImageFormat translateSampledTypeToImageFormat(QualType type);

private:
  ASTContext &astContext;
  ModuleBuilder &theBuilder;
  DiagnosticsEngine &diags;
};

} // end namespace spirv
} // end namespace clang

#endif