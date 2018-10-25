//===-- SpirvType.cpp - SPIR-V Type Hierarchy -------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//===----------------------------------------------------------------------===//
//
//  This file implements the in-memory representation of SPIR-V instructions.
//
//===----------------------------------------------------------------------===//

#include "clang/SPIRV/SpirvType.h"

namespace clang {
namespace spirv {

bool ScalarType::classof(const SpirvType *t) {
  switch (t->getKind()) {
  case TK_Bool:
  case TK_Integer:
  case TK_Float:
    return true;
  default:
    break;
  }
  return false;
}

MatrixType::MatrixType(const VectorType *vecType, uint32_t vecCount,
                       bool rowMajor)
    : SpirvType(TK_Matrix), vectorType(vecType), vectorCount(vecCount),
      isRowMajor(rowMajor) {}

bool MatrixType::operator==(const MatrixType &that) const {
  return vectorType == that.vectorType && vectorCount == that.vectorCount &&
         isRowMajor == that.isRowMajor;
}

ImageType::ImageType(const NumericalType *type, spv::Dim dim, WithDepth depth,
                     bool arrayed, bool ms, WithSampler sampled,
                     spv::ImageFormat format)
    : SpirvType(TK_Image), sampledType(type), dimension(dim), imageDepth(depth),
      isArrayed(arrayed), isMultiSampled(ms), isSampled(sampled),
      imageFormat(format) {}

bool ImageType::operator==(const ImageType &that) const {
  return sampledType == that.sampledType && dimension == that.dimension &&
         isArrayed == that.isArrayed && isMultiSampled == that.isMultiSampled &&
         isSampled == that.isSampled && imageFormat == that.imageFormat;
}

StructType::StructType(llvm::ArrayRef<const SpirvType *> memberTypes,
                       llvm::StringRef name,
                       llvm::ArrayRef<llvm::StringRef> memberNames,
                       bool isReadOnly)
    : SpirvType(TK_Struct), structName(name),
      fieldTypes(memberTypes.begin(), memberTypes.end()),
      fieldNames(memberNames.begin(), memberNames.end()), readOnly(isReadOnly) {
}

bool StructType::operator==(const StructType &that) const {
  return structName == that.structName && fieldTypes == that.fieldTypes &&
         fieldNames == that.fieldNames && readOnly == that.readOnly;
}

} // namespace spirv
} // namespace clang
