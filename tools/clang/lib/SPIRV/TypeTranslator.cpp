//===--- TypeTranslator.cpp - TypeTranslator implementation ------*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/SPIRV/TypeTranslator.h"
#include "clang/AST/HlslTypes.h"

namespace clang {
namespace spirv {

uint32_t TypeTranslator::translateType(QualType type) {
  // Try to translate the canonical type first
  const auto canonicalType = type.getCanonicalType();
  if (canonicalType != type)
    return translateType(canonicalType);

  const auto *typePtr = type.getTypePtr();

  // Primitive types
  if (const auto *builtinType = dyn_cast<BuiltinType>(typePtr)) {
    switch (builtinType->getKind()) {
    case BuiltinType::Void:
      return theBuilder.getVoidType();
    case BuiltinType::Bool:
      return theBuilder.getBoolType();
    case BuiltinType::Int:
      return theBuilder.getInt32Type();
    case BuiltinType::UInt:
      return theBuilder.getUint32Type();
    case BuiltinType::Float:
      return theBuilder.getFloat32Type();
    default:
      emitError("Primitive type '%0' is not supported yet.")
          << builtinType->getTypeClassName();
      return 0;
    }
  }

  if (const auto *typedefType = dyn_cast<TypedefType>(typePtr)) {
    return translateType(typedefType->desugar());
  }

  // In AST, vector/matrix types are TypedefType of TemplateSpecializationType.
  // We handle them via HLSL type inspection functions.

  if (hlsl::IsHLSLVecType(type)) {
    const auto elemType = hlsl::GetHLSLVecElementType(type);
    const auto elemCount = hlsl::GetHLSLVecSize(type);
    // In SPIR-V, vectors must have two or more elements. So translate vectors
    // of size 1 into the underlying primitive types directly.
    if (elemCount == 1) {
      return translateType(elemType);
    }
    return theBuilder.getVecType(translateType(elemType), elemCount);
  }

  if (hlsl::IsHLSLMatType(type)) {
    const auto elemTy = hlsl::GetHLSLMatElementType(type);
    // NOTE: According to Item "Data rules" of SPIR-V Spec 2.16.1 "Universal
    // Validation Rules":
    //   Matrix types can only be parameterized with floating-point types.
    //
    // So we need special handling of non-fp matrices, probably by emulating
    // them using other types. But for now just disable them.
    if (!elemTy->isFloatingType()) {
      emitError("Non-floating-point matrices not supported yet");
      return 0;
    }
    const auto elemType = translateType(elemTy);

    uint32_t rowCount = 0, colCount = 0;
    hlsl::GetHLSLMatRowColCount(type, rowCount, colCount);

    // In SPIR-V, matrices must have two or more columns.
    // Handle degenerated cases first.

    if (rowCount == 1 && colCount == 1)
      return elemType;

    if (rowCount == 1)
      return theBuilder.getVecType(elemType, colCount);

    if (colCount == 1)
      return theBuilder.getVecType(elemType, rowCount);

    // HLSL matrices are row major, while SPIR-V matrices are column major.
    // We are mapping what HLSL semantically mean a row into a column here.
    const uint32_t vecType = theBuilder.getVecType(elemType, colCount);
    return theBuilder.getMatType(vecType, rowCount);
  }

  // Struct type
  if (const auto *structType = dyn_cast<RecordType>(typePtr)) {
    const auto *decl = structType->getDecl();

    // Collect all fields' types.
    std::vector<uint32_t> fieldTypes;
    for (const auto *field : decl->fields()) {
      fieldTypes.push_back(translateType(field->getType()));
    }

    return theBuilder.getStructType(fieldTypes);
  }

  emitError("Type '%0' is not supported yet.") << type->getTypeClassName();
  return 0;
}

bool TypeTranslator::is1x1MatrixType(QualType type) {
  if (!hlsl::IsHLSLMatType(type))
    return false;

  uint32_t rowCount = 0, colCount = 0;
  hlsl::GetHLSLMatRowColCount(type, rowCount, colCount);

  return rowCount == 1 && colCount == 1;
}

bool TypeTranslator::is1xNMatrixType(QualType type) {
  if (!hlsl::IsHLSLMatType(type))
    return false;

  uint32_t rowCount = 0, colCount = 0;
  hlsl::GetHLSLMatRowColCount(type, rowCount, colCount);

  return rowCount == 1 && colCount > 1;
}

/// Returns true if the given type is a SPIR-V acceptable matrix type, i.e.,
/// with floating point elements and greater than 1 row and column counts.
bool TypeTranslator::isSpirvAcceptableMatrixType(QualType type) {
  if (!hlsl::IsHLSLMatType(type))
    return false;

  const auto elemType = hlsl::GetHLSLMatElementType(type);
  if (!elemType->isFloatingType())
    return false;

  uint32_t rowCount = 0, colCount = 0;
  hlsl::GetHLSLMatRowColCount(type, rowCount, colCount);
  return rowCount > 1 && colCount > 1;
}

uint32_t TypeTranslator::getComponentVectorType(QualType matrixType) {
  assert(isSpirvAcceptableMatrixType(matrixType));

  const uint32_t elemType =
      translateType(hlsl::GetHLSLMatElementType(matrixType));

  uint32_t rowCount = 0, colCount = 0;
  hlsl::GetHLSLMatRowColCount(matrixType, rowCount, colCount);

  return theBuilder.getVecType(elemType, colCount);
}

} // end namespace spirv
} // end namespace clang