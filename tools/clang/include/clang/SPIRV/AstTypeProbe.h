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

#include "clang/AST/Decl.h"
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

/// Returns true if the given type is or contains any kind of structured-buffer
/// or byte-address-buffer.
bool isOrContainsAKindOfStructuredOrByteBuffer(QualType type);

/// \brief Returns true if the given type is SubpassInput.
bool isSubpassInput(QualType);

/// \brief Returns true if the given type is SubpassInputMS.
bool isSubpassInputMS(QualType);

/// \brief Returns true if the decl is of ConstantBuffer/TextureBuffer type.
bool isConstantTextureBuffer(const Decl *decl);

/// \brief Returns true if the decl will have a SPIR-V resource type.
///
/// Note that this function covers the following HLSL types:
/// * ConstantBuffer/TextureBuffer
/// * Various structured buffers
/// * (RW)ByteAddressBuffer
/// * SubpassInput(MS)
bool isResourceType(const ValueDecl *decl);

/// \brief Returns true if the given type is the HLSL (RW)StructuredBuffer,
/// (RW)ByteAddressBuffer, or {Append|Consume}StructuredBuffer.
bool isAKindOfStructuredOrByteBuffer(QualType type);

/// Returns true if the given type is or contains a 16-bit type.
/// The caller must also specify whether 16-bit types have been enabled via
/// command line options.
bool isOrContains16BitType(QualType type, bool enable16BitTypesOption);

/// NOTE: This method doesn't handle Literal types correctly at the moment.
///
/// Note: This method will be deprecated once resolving of literal types are
/// moved to a dedicated pass.
///
/// \brief Returns the realized bitwidth of the given type when represented in
/// SPIR-V. Panics if the given type is not a scalar, a vector/matrix of float
/// or integer, or an array of them. In case of vectors, it returns the
/// realized SPIR-V bitwidth of the vector elements.
uint32_t getElementSpirvBitwidth(const ASTContext &astContext, QualType type,
                                 bool is16BitTypeEnabled);

/// Returns true if the two types can be treated as the same scalar
/// type, which means they have the same canonical type, regardless of
/// constnesss and literalness.
bool canTreatAsSameScalarType(QualType type1, QualType type2);

/// \brief Returns true if the two types are the same scalar or vector type,
/// regardless of constness and literalness.
bool isSameScalarOrVecType(QualType type1, QualType type2);

  /// \brief Returns true if the two types are the same type, regardless of
  /// constness and literalness.
bool isSameType(const ASTContext &, QualType type1, QualType type2);

/// Returns true if all members in structType are of the same element
/// type and can be fit into a 4-component vector. Writes element type and
/// count to *elemType and *elemCount if not nullptr. Otherwise, emit errors
/// explaining why not.
bool canFitIntoOneRegister(QualType structType, QualType *elemType,
                           uint32_t *elemCount = nullptr);

/// Returns the element type of the given type. The given type may be a scalar
/// type, vector type, matrix type, or array type. It may also be a struct with
/// members that can fit into a register. In such case, the result would be the
/// struct member type.
QualType getElementType(QualType type);

QualType getTypeWithCustomBitwidth(const ASTContext &, QualType type,
                                   uint32_t bitwidth);

/// Returns true if the given type is a matrix or an array of matrices.
bool isMatrixOrArrayOfMatrix(const ASTContext &, QualType type);

/// Returns true if the given type is a LitInt or LitFloat type or a vector of
/// them. Returns false otherwise.
bool isLitTypeOrVecOfLitType(QualType type);

} // namespace spirv
} // namespace clang

#endif // LLVM_CLANG_SPIRV_TYPEPROBE_H