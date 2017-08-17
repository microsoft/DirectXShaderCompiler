//===--- TypeTranslator.cpp - TypeTranslator implementation ------*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "TypeTranslator.h"

#include "dxc/HLSL/DxilConstants.h"
#include "clang/AST/HlslTypes.h"

namespace clang {
namespace spirv {

uint32_t TypeTranslator::translateType(QualType type) {
  // Try to translate the canonical type first
  const auto canonicalType = type.getCanonicalType();
  if (canonicalType != type)
    return translateType(canonicalType);

  // Primitive types
  {
    QualType ty = {};
    if (isScalarType(type, &ty))
      if (const auto *builtinType = cast<BuiltinType>(ty.getTypePtr()))
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

  // Typedefs
  if (const auto *typedefType = type->getAs<TypedefType>()) {
    return translateType(typedefType->desugar());
  }

  // Reference types
  if (const auto *refType = type->getAs<ReferenceType>()) {
    // Note: Pointer/reference types are disallowed in HLSL source code.
    // Although developers cannot use them directly, they are generated into
    // the AST by out/inout parameter modifiers in function signatures.
    // We already pass function arguments via pointers to tempoary local
    // variables. So it should be fine to drop the pointer type and treat it
    // as the underlying pointee type here.
    return translateType(refType->getPointeeType());
  }

  // In AST, vector/matrix types are TypedefType of TemplateSpecializationType.
  // We handle them via HLSL type inspection functions.

  // Vector types
  {
    QualType elemType = {};
    uint32_t elemCount = {};
    if (TypeTranslator::isVectorType(type, &elemType, &elemCount)) {
      // In SPIR-V, vectors must have two or more elements. So translate vectors
      // of size 1 into the underlying primitive types directly.
      if (elemCount == 1) {
        return translateType(elemType);
      }
      return theBuilder.getVecType(translateType(elemType), elemCount);
    }
  }

  // Matrix types
  if (hlsl::IsHLSLMatType(type)) {
    // The other cases should already be handled in the above.
    assert(isMxNMatrix(type));

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

    // HLSL matrices are row major, while SPIR-V matrices are column major.
    // We are mapping what HLSL semantically mean a row into a column here.
    const uint32_t vecType = theBuilder.getVecType(elemType, colCount);
    return theBuilder.getMatType(vecType, rowCount);
  }

  // Struct type
  if (const auto *structType = type->getAs<RecordType>()) {
    const auto *decl = structType->getDecl();

    // HLSL resource types are also represented as RecordType in the AST.
    // (ClassTemplateSpecializationDecl is a subclass of CXXRecordDecl, which is
    // then a subclass of RecordDecl.) So we need to check them before checking
    // the general struct type.
    if (const auto id = translateResourceType(type))
      return id;

    // Collect all fields' types and names.
    llvm::SmallVector<uint32_t, 4> fieldTypes;
    llvm::SmallVector<llvm::StringRef, 4> fieldNames;
    for (const auto *field : decl->fields()) {
      fieldTypes.push_back(translateType(field->getType()));
      fieldNames.push_back(field->getName());
    }

    return theBuilder.getStructType(fieldTypes, decl->getName(), fieldNames);
  }

  if (const auto *arrayType = astContext.getAsConstantArrayType(type)) {
    const uint32_t elemType = translateType(arrayType->getElementType());
    // TODO: handle extra large array size?
    const auto size =
        static_cast<uint32_t>(arrayType->getSize().getZExtValue());
    return theBuilder.getArrayType(elemType,
                                   theBuilder.getConstantUint32(size));
  }

  emitError("Type '%0' is not supported yet.") << type->getTypeClassName();
  return 0;
}

bool TypeTranslator::isScalarType(QualType type, QualType *scalarType) {
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

bool TypeTranslator::isVectorType(QualType type, QualType *elemType,
                                  uint32_t *elemCount) {
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

bool TypeTranslator::is1x1Matrix(QualType type, QualType *elemType) {
  if (!hlsl::IsHLSLMatType(type))
    return false;

  uint32_t rowCount = 0, colCount = 0;
  hlsl::GetHLSLMatRowColCount(type, rowCount, colCount);

  const bool is1x1 = rowCount == 1 && colCount == 1;

  if (!is1x1)
    return false;

  if (elemType)
    *elemType = hlsl::GetHLSLMatElementType(type);
  return true;
}

bool TypeTranslator::is1xNMatrix(QualType type, QualType *elemType,
                                 uint32_t *count) {
  if (!hlsl::IsHLSLMatType(type))
    return false;

  uint32_t rowCount = 0, colCount = 0;
  hlsl::GetHLSLMatRowColCount(type, rowCount, colCount);

  const bool is1xN = rowCount == 1 && colCount > 1;

  if (!is1xN)
    return false;

  if (elemType)
    *elemType = hlsl::GetHLSLMatElementType(type);
  if (count)
    *count = colCount;
  return true;
}

bool TypeTranslator::isMx1Matrix(QualType type, QualType *elemType,
                                 uint32_t *count) {
  if (!hlsl::IsHLSLMatType(type))
    return false;

  uint32_t rowCount = 0, colCount = 0;
  hlsl::GetHLSLMatRowColCount(type, rowCount, colCount);

  const bool isMx1 = rowCount > 1 && colCount == 1;

  if (!isMx1)
    return false;

  if (elemType)
    *elemType = hlsl::GetHLSLMatElementType(type);
  if (count)
    *count = rowCount;
  return true;
}

bool TypeTranslator::isMxNMatrix(QualType type, QualType *elemType,
                                 uint32_t *numRows, uint32_t *numCols) {
  if (!hlsl::IsHLSLMatType(type))
    return false;

  uint32_t rowCount = 0, colCount = 0;
  hlsl::GetHLSLMatRowColCount(type, rowCount, colCount);

  const bool isMxN = rowCount > 1 && colCount > 1;

  if (!isMxN)
    return false;

  if (elemType)
    *elemType = hlsl::GetHLSLMatElementType(type);
  if (numRows)
    *numRows = rowCount;
  if (numCols)
    *numCols = colCount;
  return true;
}

bool TypeTranslator::isSpirvAcceptableMatrixType(QualType type) {
  QualType elemType = {};
  return isMxNMatrix(type, &elemType) && elemType->isFloatingType();
}

QualType TypeTranslator::getElementType(QualType type) {
  QualType elemType = {};
  (void)(isScalarType(type, &elemType) || isVectorType(type, &elemType) ||
         isMxNMatrix(type, &elemType));
  return elemType;
}

uint32_t TypeTranslator::getComponentVectorType(QualType matrixType) {
  assert(isSpirvAcceptableMatrixType(matrixType));

  const uint32_t elemType =
      translateType(hlsl::GetHLSLMatElementType(matrixType));

  uint32_t rowCount = 0, colCount = 0;
  hlsl::GetHLSLMatRowColCount(matrixType, rowCount, colCount);

  return theBuilder.getVecType(elemType, colCount);
}

uint32_t TypeTranslator::translateResourceType(QualType type) {
  const auto *recordType = type->getAs<RecordType>();
  assert(recordType);
  const llvm::StringRef name = recordType->getDecl()->getName();

  // TODO: avoid string comparison once hlsl::IsHLSLResouceType() does that.

  { // Texture types
    spv::Dim dim = {};
    bool isArray = {};

    if ((dim = spv::Dim::Dim1D, isArray = false, name == "Texture1D") ||
        (dim = spv::Dim::Dim2D, isArray = false, name == "Texture2D") ||
        (dim = spv::Dim::Dim3D, isArray = false, name == "Texture3D") ||
        (dim = spv::Dim::Cube, isArray = false, name == "TextureCube") ||
        (dim = spv::Dim::Dim1D, isArray = true, name == "Texture1DArray") ||
        (dim = spv::Dim::Dim2D, isArray = true, name == "Texture2DArray") ||
        // There is no Texture3DArray
        (dim = spv::Dim::Cube, isArray = true, name == "TextureCubeArray")) {
      if (dim == spv::Dim::Dim1D)
        theBuilder.requireCapability(spv::Capability::Sampled1D);
      const auto sampledType = hlsl::GetHLSLResourceResultType(type);
      return theBuilder.getImageType(translateType(getElementType(sampledType)),
                                     dim, isArray);
    }
  }

  // Sampler types
  if (name == "SamplerState" || name == "SamplerComparisonState") {
    return theBuilder.getSamplerType();
  }

  return 0;
}

} // end namespace spirv
} // end namespace clang
