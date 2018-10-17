//===-- TypeProbe.h - Static functions for probing QualType -----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//===----------------------------------------------------------------------===//
#ifndef LLVM_CLANG_SPIRV_TYPEPROBE_H
#define LLVM_CLANG_SPIRV_TYPEPROBE_H

#include <string>

#include "clang/AST/Type.h"

namespace clang {
namespace spirv {

/// Returns a string name for the given type.
std::string getAstTypeName(QualType type);

/// Returns true if the given type will be translated into a SPIR-V scalar type.
///
/// This includes normal scalar types, vectors of size 1, and 1x1 matrices.
///
/// If scalarType is not nullptr, writes the scalar type to *scalarType.
bool isScalarType(QualType type, QualType *scalarType = nullptr);

/// Returns true if the given type will be translated into a SPIR-V vector type.
///
/// This includes normal vector types (either ExtVectorType or HLSL vector type)
/// with more than one elements and matrices with exactly one row or one column.
///
/// Writes the element type and count into *elementType and *count respectively
/// if they are not nullptr.
bool isVectorType(QualType type, QualType *elemType = nullptr,
                  uint32_t *elemCount = nullptr);

/// Returns true if the given type is a 1x1 matrix type.
///
/// If elemType is not nullptr, writes the element type to *elemType.
bool is1x1Matrix(QualType type, QualType *elemType = nullptr);

/// Returns true if the given type is a 1xN (N > 1) matrix type.
///
/// If elemType is not nullptr, writes the element type to *elemType.
/// If count is not nullptr, writes the value of N into *count.
bool is1xNMatrix(QualType type, QualType *elemType = nullptr,
                 uint32_t *count = nullptr);

/// Returns true if the given type is a Mx1 (M > 1) matrix type.
///
/// If elemType is not nullptr, writes the element type to *elemType.
/// If count is not nullptr, writes the value of M into *count.
bool isMx1Matrix(QualType type, QualType *elemType = nullptr,
                 uint32_t *count = nullptr);

/// Returns true if the given type is a matrix with more than 1 row and
/// more than 1 column.
///
/// If elemType is not nullptr, writes the element type to *elemType.
/// If rowCount is not nullptr, writes the number of rows (M) into *rowCount.
/// If colCount is not nullptr, writes the number of cols (N) into *colCount.
bool isMxNMatrix(QualType type, QualType *elemType = nullptr,
                 uint32_t *rowCount = nullptr, uint32_t *colCount = nullptr);

} // namespace spirv
} // namespace clang

#endif // LLVM_CLANG_SPIRV_TYPEPROBE_H