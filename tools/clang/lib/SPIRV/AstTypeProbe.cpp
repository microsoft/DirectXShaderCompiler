//===--- TypeProbe.cpp - Static functions for probing QualType ---*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/SPIRV/AstTypeProbe.h"
#include "clang/AST/Decl.h"
#include "clang/AST/HlslTypes.h"

namespace clang {
namespace spirv {

std::string getAstTypeName(QualType type) {
  {
    QualType ty = {};
    if (isScalarType(type, &ty))
      if (const auto *builtinType = ty->getAs<BuiltinType>())
        switch (builtinType->getKind()) {
        case BuiltinType::Void:
          return "void";
        case BuiltinType::Bool:
          return "bool";
        case BuiltinType::Int:
          return "int";
        case BuiltinType::UInt:
          return "uint";
        case BuiltinType::Float:
          return "float";
        case BuiltinType::Double:
          return "double";
        case BuiltinType::LongLong:
          return "int64";
        case BuiltinType::ULongLong:
          return "uint64";
        case BuiltinType::Short:
          return "short";
        case BuiltinType::UShort:
          return "ushort";
        case BuiltinType::Half:
        case BuiltinType::HalfFloat:
          return "half";
        case BuiltinType::Min12Int:
          return "min12int";
        case BuiltinType::Min16Int:
          return "min16int";
        case BuiltinType::Min16UInt:
          return "min16uint";
        case BuiltinType::Min16Float:
          return "min16float";
        case BuiltinType::Min10Float:
          return "min10float";
        default:
          return "";
        }
  }

  {
    QualType elemType = {};
    uint32_t elemCount = {};
    if (isVectorType(type, &elemType, &elemCount))
      return "v" + std::to_string(elemCount) + getAstTypeName(elemType);
  }

  {
    QualType elemType = {};
    uint32_t rowCount = 0, colCount = 0;
    if (isMxNMatrix(type, &elemType, &rowCount, &colCount))
      return "mat" + std::to_string(rowCount) + "v" + std::to_string(colCount) +
             getAstTypeName(elemType);
  }

  if (const auto *structType = type->getAs<RecordType>())
    return structType->getDecl()->getName();

  return "";
}

bool isScalarType(QualType type, QualType *scalarType) {
  bool isScalar = false;
  QualType ty = {};

  if (type->isBuiltinType()) {
    isScalar = true;
    ty = type;
  } else if (hlsl::IsHLSLVecType(type) && hlsl::GetHLSLVecSize(type) == 1) {
    isScalar = true;
    ty = hlsl::GetHLSLVecElementType(type);
  } else if (const auto *extVecType =
                 dyn_cast<ExtVectorType>(type.getTypePtr())) {
    if (extVecType->getNumElements() == 1) {
      isScalar = true;
      ty = extVecType->getElementType();
    }
  } else if (is1x1Matrix(type)) {
    isScalar = true;
    ty = hlsl::GetHLSLMatElementType(type);
  }

  if (isScalar && scalarType)
    *scalarType = ty;

  return isScalar;
}

bool isVectorType(QualType type, QualType *elemType, uint32_t *elemCount) {
  bool isVec = false;
  QualType ty = {};
  uint32_t count = 0;

  if (hlsl::IsHLSLVecType(type)) {
    ty = hlsl::GetHLSLVecElementType(type);
    count = hlsl::GetHLSLVecSize(type);
    isVec = count > 1;
  } else if (const auto *extVecType =
                 dyn_cast<ExtVectorType>(type.getTypePtr())) {
    ty = extVecType->getElementType();
    count = extVecType->getNumElements();
    isVec = count > 1;
  } else if (hlsl::IsHLSLMatType(type)) {
    uint32_t rowCount = 0, colCount = 0;
    hlsl::GetHLSLMatRowColCount(type, rowCount, colCount);

    ty = hlsl::GetHLSLMatElementType(type);
    count = rowCount == 1 ? colCount : rowCount;
    isVec = (rowCount == 1) != (colCount == 1);
  }

  if (isVec) {
    if (elemType)
      *elemType = ty;
    if (elemCount)
      *elemCount = count;
  }
  return isVec;
}

bool is1x1Matrix(QualType type, QualType *elemType) {
  if (!hlsl::IsHLSLMatType(type))
    return false;

  uint32_t rowCount = 0, colCount = 0;
  hlsl::GetHLSLMatRowColCount(type, rowCount, colCount);

  if (rowCount == 1 && colCount == 1) {
    if (elemType)
      *elemType = hlsl::GetHLSLMatElementType(type);
    return true;
  }

  return false;
}

bool is1xNMatrix(QualType type, QualType *elemType, uint32_t *elemCount) {
  if (!hlsl::IsHLSLMatType(type))
    return false;

  uint32_t rowCount = 0, colCount = 0;
  hlsl::GetHLSLMatRowColCount(type, rowCount, colCount);

  if (rowCount == 1 && colCount > 1) {
    if (elemType)
      *elemType = hlsl::GetHLSLMatElementType(type);
    if (elemCount)
      *elemCount = colCount;
    return true;
  }

  return false;
}

bool isMx1Matrix(QualType type, QualType *elemType, uint32_t *elemCount) {
  if (!hlsl::IsHLSLMatType(type))
    return false;

  uint32_t rowCount = 0, colCount = 0;
  hlsl::GetHLSLMatRowColCount(type, rowCount, colCount);

  if (rowCount > 1 && colCount == 1) {
    if (elemType)
      *elemType = hlsl::GetHLSLMatElementType(type);
    if (elemCount)
      *elemCount = rowCount;
    return true;
  }

  return false;
}

bool isMxNMatrix(QualType type, QualType *elemType, uint32_t *numRows,
                 uint32_t *numCols) {
  if (!hlsl::IsHLSLMatType(type))
    return false;

  uint32_t rowCount = 0, colCount = 0;
  hlsl::GetHLSLMatRowColCount(type, rowCount, colCount);

  if (rowCount > 1 && colCount > 1) {
    if (elemType)
      *elemType = hlsl::GetHLSLMatElementType(type);
    if (numRows)
      *numRows = rowCount;
    if (numCols)
      *numCols = colCount;
    return true;
  }

  return false;
}

bool isOrContainsAKindOfStructuredOrByteBuffer(QualType type) {
  if (const RecordType *recordType = type->getAs<RecordType>()) {
    StringRef name = recordType->getDecl()->getName();
    if (name == "StructuredBuffer" || name == "RWStructuredBuffer" ||
        name == "ByteAddressBuffer" || name == "RWByteAddressBuffer" ||
        name == "AppendStructuredBuffer" || name == "ConsumeStructuredBuffer")
      return true;

    for (const auto *field : recordType->getDecl()->fields()) {
      if (isOrContainsAKindOfStructuredOrByteBuffer(field->getType()))
        return true;
    }
  }
  return false;
}

bool isSubpassInput(QualType type) {
  if (const auto *rt = type->getAs<RecordType>())
    return rt->getDecl()->getName() == "SubpassInput";

  return false;
}

bool isSubpassInputMS(QualType type) {
  if (const auto *rt = type->getAs<RecordType>())
    return rt->getDecl()->getName() == "SubpassInputMS";

  return false;
}

bool isConstantTextureBuffer(const Decl *decl) {
  if (const auto *bufferDecl = dyn_cast<HLSLBufferDecl>(decl->getDeclContext()))
    // Make sure we are not returning true for VarDecls inside cbuffer/tbuffer.
    return bufferDecl->isConstantBufferView();

  return false;
}

bool isResourceType(const ValueDecl *decl) {
  if (isConstantTextureBuffer(decl))
    return true;

  QualType declType = decl->getType();

  // Deprive the arrayness to see the element type
  while (declType->isArrayType()) {
    declType = declType->getAsArrayTypeUnsafe()->getElementType();
  }

  if (isSubpassInput(declType) || isSubpassInputMS(declType))
    return true;

  return hlsl::IsHLSLResourceType(declType);
}

bool isAKindOfStructuredOrByteBuffer(QualType type) {
  // Strip outer arrayness first
  while (type->isArrayType())
    type = type->getAsArrayTypeUnsafe()->getElementType();

  if (const RecordType *recordType = type->getAs<RecordType>()) {
    StringRef name = recordType->getDecl()->getName();
    return name == "StructuredBuffer" || name == "RWStructuredBuffer" ||
           name == "ByteAddressBuffer" || name == "RWByteAddressBuffer" ||
           name == "AppendStructuredBuffer" ||
           name == "ConsumeStructuredBuffer";
  }
  return false;
}

} // namespace spirv
} // namespace clang
