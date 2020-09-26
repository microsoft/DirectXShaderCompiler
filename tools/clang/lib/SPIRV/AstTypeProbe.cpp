//===--- TypeProbe.cpp - Static functions for probing QualType ---*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/SPIRV/AstTypeProbe.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Attr.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/HlslTypes.h"

namespace {
template <unsigned N>
clang::DiagnosticBuilder emitError(const clang::ASTContext &astContext,
                                   const char (&message)[N],
                                   clang::SourceLocation srcLoc = {}) {
  const auto diagId = astContext.getDiagnostics().getCustomDiagID(
      clang::DiagnosticsEngine::Error, message);
  return astContext.getDiagnostics().Report(srcLoc, diagId);
}
} // namespace

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

  if (type->isBuiltinType() || isEnumType(type)) {
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

bool isScalarOrVectorType(QualType type, QualType *elemType,
                          uint32_t *elemCount) {
  if (isScalarType(type, elemType)) {
    if (elemCount)
      *elemCount = 1;
    return true;
  }

  return isVectorType(type, elemType, elemCount);
}

bool isConstantArrayType(const ASTContext &astContext, QualType type) {
  return astContext.getAsConstantArrayType(type) != nullptr;
}

bool isEnumType(QualType type) {
  if (isa<EnumType>(type.getTypePtr()))
    return true;

  if (const auto *elaboratedType = type->getAs<ElaboratedType>())
    if (isa<EnumType>(elaboratedType->desugar().getTypePtr()))
      return true;

  return false;
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

bool isConstantBuffer(clang::QualType type) {
  // Strip outer arrayness first
  while (type->isArrayType())
    type = type->getAsArrayTypeUnsafe()->getElementType();
  if (const RecordType *RT = type->getAs<RecordType>()) {
    StringRef name = RT->getDecl()->getName();
    return name == "ConstantBuffer";
  }
  return false;
}

bool isTextureBuffer(clang::QualType type) {
  // Strip outer arrayness first
  while (type->isArrayType())
    type = type->getAsArrayTypeUnsafe()->getElementType();
  if (const RecordType *RT = type->getAs<RecordType>()) {
    StringRef name = RT->getDecl()->getName();
    return name == "TextureBuffer";
  }
  return false;
}

bool isConstantTextureBuffer(QualType type) {
  return isConstantBuffer(type) || isTextureBuffer(type);
}

bool isResourceType(const ValueDecl *decl) {
  QualType declType = decl->getType();

  // Deprive the arrayness to see the element type
  while (declType->isArrayType()) {
    declType = declType->getAsArrayTypeUnsafe()->getElementType();
  }

  if (isSubpassInput(declType) || isSubpassInputMS(declType))
    return true;

  return hlsl::IsHLSLResourceType(declType);
}

bool isOrContains16BitType(QualType type, bool enable16BitTypesOption) {
  // Primitive types
  {
    QualType ty = {};
    if (isScalarType(type, &ty)) {
      if (const auto *builtinType = ty->getAs<BuiltinType>()) {
        switch (builtinType->getKind()) {
        case BuiltinType::Min12Int:
        case BuiltinType::Min16Int:
        case BuiltinType::Min16UInt:
        case BuiltinType::Min10Float:
        case BuiltinType::Min16Float:
          return enable16BitTypesOption;
        // the 'Half' enum always represents 16-bit and 'HalfFloat' always
        // represents 32-bit floats.
        // int16_t and uint16_t map to Short and UShort
        case BuiltinType::Short:
        case BuiltinType::UShort:
        case BuiltinType::Half:
          return true;
        default:
          return false;
        }
      }
    }
  }

  // Vector types
  {
    QualType elemType = {};
    if (isVectorType(type, &elemType))
      return isOrContains16BitType(elemType, enable16BitTypesOption);
  }

  // Matrix types
  {
    QualType elemType = {};
    if (isMxNMatrix(type, &elemType)) {
      return isOrContains16BitType(elemType, enable16BitTypesOption);
    }
  }

  // Struct type
  if (const auto *structType = type->getAs<RecordType>()) {
    const auto *decl = structType->getDecl();

    for (const auto *field : decl->fields()) {
      if (isOrContains16BitType(field->getType(), enable16BitTypesOption))
        return true;
    }

    return false;
  }

  // Array type
  if (const auto *arrayType = type->getAsArrayTypeUnsafe()) {
    return isOrContains16BitType(arrayType->getElementType(),
                                 enable16BitTypesOption);
  }

  // Reference types
  if (const auto *refType = type->getAs<ReferenceType>()) {
    return isOrContains16BitType(refType->getPointeeType(),
                                 enable16BitTypesOption);
  }

  // Pointer types
  if (const auto *ptrType = type->getAs<PointerType>()) {
    return isOrContains16BitType(ptrType->getPointeeType(),
                                 enable16BitTypesOption);
  }

  if (const auto *typedefType = type->getAs<TypedefType>()) {
    return isOrContains16BitType(typedefType->desugar(),
                                 enable16BitTypesOption);
  }

  llvm_unreachable("checking 16-bit type unimplemented");
  return 0;
}

uint32_t getElementSpirvBitwidth(const ASTContext &astContext, QualType type,
                                 bool is16BitTypeEnabled) {
  const auto canonicalType = type.getCanonicalType();
  if (canonicalType != type)
    return getElementSpirvBitwidth(astContext, canonicalType,
                                   is16BitTypeEnabled);

  // Vector types
  {
    QualType elemType = {};
    if (isVectorType(type, &elemType))
      return getElementSpirvBitwidth(astContext, elemType, is16BitTypeEnabled);
  }

  // Matrix types
  if (hlsl::IsHLSLMatType(type))
    return getElementSpirvBitwidth(
        astContext, hlsl::GetHLSLMatElementType(type), is16BitTypeEnabled);

  // Array types
  if (const auto *arrayType = type->getAsArrayTypeUnsafe()) {
    return getElementSpirvBitwidth(astContext, arrayType->getElementType(),
                                   is16BitTypeEnabled);
  }

  // Typedefs
  if (const auto *typedefType = type->getAs<TypedefType>())
    return getElementSpirvBitwidth(astContext, typedefType->desugar(),
                                   is16BitTypeEnabled);

  // Reference types
  if (const auto *refType = type->getAs<ReferenceType>())
    return getElementSpirvBitwidth(astContext, refType->getPointeeType(),
                                   is16BitTypeEnabled);

  // Pointer types
  if (const auto *ptrType = type->getAs<PointerType>())
    return getElementSpirvBitwidth(astContext, ptrType->getPointeeType(),
                                   is16BitTypeEnabled);

  // Enum types
  if (isEnumType(type))
    return 32;

  // Scalar types
  QualType ty = {};
  const bool isScalar = isScalarType(type, &ty);
  assert(isScalar);
  (void)isScalar;
  if (const auto *builtinType = ty->getAs<BuiltinType>()) {
    switch (builtinType->getKind()) {
    case BuiltinType::Bool:
    case BuiltinType::Int:
    case BuiltinType::UInt:
    case BuiltinType::Float:
      return 32;
    case BuiltinType::Double:
    case BuiltinType::LongLong:
    case BuiltinType::ULongLong:
      return 64;
    // Half builtin type is always 16-bit. The HLSL 'half' keyword is translated
    // to 'Half' enum if -enable-16bit-types is true.
    // int16_t and uint16_t map to Short and UShort
    case BuiltinType::Half:
    case BuiltinType::Short:
    case BuiltinType::UShort:
      return 16;
    // HalfFloat builtin type is just an alias for Float builtin type and is
    // always 32-bit. The HLSL 'half' keyword is translated to 'HalfFloat' enum
    // if -enable-16bit-types is false.
    case BuiltinType::HalfFloat:
      return 32;
    // The following types are treated as 16-bit if '-enable-16bit-types' option
    // is enabled. They are treated as 32-bit otherwise.
    case BuiltinType::Min12Int:
    case BuiltinType::Min16Int:
    case BuiltinType::Min16UInt:
    case BuiltinType::Min16Float:
    case BuiltinType::Min10Float: {
      return is16BitTypeEnabled ? 16 : 32;
    }
    case BuiltinType::LitFloat: {
      return 64;
    }
    case BuiltinType::LitInt: {
      return 64;
    }
    default:
      // Other builtin types are either not relevant to bitcount or not in HLSL.
      break;
    }
  }
  llvm_unreachable("invalid type passed to getElementSpirvBitwidth");
}

bool canTreatAsSameScalarType(QualType type1, QualType type2) {
  // Treat const int/float the same as const int/float
  type1.removeLocalConst();
  type2.removeLocalConst();

  return (type1.getCanonicalType() == type2.getCanonicalType()) ||
         // Treat 'literal float' and 'float' as the same
         (type1->isSpecificBuiltinType(BuiltinType::LitFloat) &&
          type2->isFloatingType()) ||
         (type2->isSpecificBuiltinType(BuiltinType::LitFloat) &&
          type1->isFloatingType()) ||
         // Treat 'literal int' and 'int'/'uint' as the same
         (type1->isSpecificBuiltinType(BuiltinType::LitInt) &&
          type2->isIntegerType() &&
          // Disallow boolean types
          !type2->isSpecificBuiltinType(BuiltinType::Bool)) ||
         (type2->isSpecificBuiltinType(BuiltinType::LitInt) &&
          type1->isIntegerType() &&
          // Disallow boolean types
          !type1->isSpecificBuiltinType(BuiltinType::Bool));
}

bool canFitIntoOneRegister(const ASTContext &astContext, QualType structType,
                           QualType *elemType, uint32_t *elemCount) {
  if (structType->getAsStructureType() == nullptr)
    return false;

  const auto *structDecl = structType->getAsStructureType()->getDecl();
  QualType firstElemType;
  uint32_t totalCount = 0;

  for (const auto *field : structDecl->fields()) {
    QualType type;
    uint32_t count = 1;

    if (isScalarType(field->getType(), &type) ||
        isVectorType(field->getType(), &type, &count)) {
      if (firstElemType.isNull()) {
        firstElemType = type;
      } else {
        if (!canTreatAsSameScalarType(firstElemType, type)) {
          emitError(astContext,
                    "all struct members should have the same element type for "
                    "resource template instantiation",
                    structDecl->getLocation());
          return false;
        }
      }
      totalCount += count;
    } else {
      emitError(
          astContext,
          "unsupported struct element type for resource template instantiation",
          structDecl->getLocation());
      return false;
    }
  }

  if (totalCount > 4) {
    emitError(
        astContext,
        "resource template element type %0 cannot fit into four 32-bit scalars",
        structDecl->getLocation())
        << structType;
    return false;
  }

  if (elemType)
    *elemType = firstElemType;
  if (elemCount)
    *elemCount = totalCount;
  return true;
}

QualType getElementType(const ASTContext &astContext, QualType type) {
  QualType elemType = {};
  if (isScalarType(type, &elemType) || isVectorType(type, &elemType) ||
      isMxNMatrix(type, &elemType) ||
      canFitIntoOneRegister(astContext, type, &elemType)) {
    return elemType;
  }

  if (const auto *arrType = dyn_cast<ConstantArrayType>(type)) {
    return arrType->getElementType();
  }

  assert(false && "unsupported resource type parameter");
  return type;
}

QualType getTypeWithCustomBitwidth(const ASTContext &ctx, QualType type,
                                   uint32_t bitwidth) {
  // Cases where the given type is a vector of float/int.
  {
    QualType elemType = {};
    uint32_t elemCount = 0;
    const bool isVec = isVectorType(type, &elemType, &elemCount);
    if (isVec) {
      return ctx.getExtVectorType(
          getTypeWithCustomBitwidth(ctx, elemType, bitwidth), elemCount);
    }
  }

  // Scalar cases.
  assert(!type->isBooleanType());
  assert(type->isIntegerType() || type->isFloatingType());
  if (type->isFloatingType()) {
    switch (bitwidth) {
    case 16:
      return ctx.HalfTy;
    case 32:
      return ctx.FloatTy;
    case 64:
      return ctx.DoubleTy;
    }
  }
  if (type->isSignedIntegerType()) {
    switch (bitwidth) {
    case 16:
      return ctx.ShortTy;
    case 32:
      return ctx.IntTy;
    case 64:
      return ctx.LongLongTy;
    }
  }
  if (type->isUnsignedIntegerType()) {
    switch (bitwidth) {
    case 16:
      return ctx.UnsignedShortTy;
    case 32:
      return ctx.UnsignedIntTy;
    case 64:
      return ctx.UnsignedLongLongTy;
    }
  }
  llvm_unreachable(
      "invalid type or bitwidth passed to getTypeWithCustomBitwidth");
}

bool isMatrixOrArrayOfMatrix(const ASTContext &context, QualType type) {
  if (isMxNMatrix(type)) {
    return true;
  }

  if (const auto *arrayType = context.getAsArrayType(type))
    return isMatrixOrArrayOfMatrix(context, arrayType->getElementType());

  return false;
}

bool isLitTypeOrVecOfLitType(QualType type) {
  if (type == QualType())
    return false;

  if (type->isSpecificBuiltinType(BuiltinType::LitInt) ||
      type->isSpecificBuiltinType(BuiltinType::LitFloat))
    return true;

  // For vector cases
  {
    QualType elemType = {};
    uint32_t elemCount = 0;
    if (isVectorType(type, &elemType, &elemCount))
      return isLitTypeOrVecOfLitType(elemType);
  }

  return false;
}

bool isSameScalarOrVecType(QualType type1, QualType type2) {
  { // Scalar types
    QualType scalarType1 = {}, scalarType2 = {};
    if (isScalarType(type1, &scalarType1) && isScalarType(type2, &scalarType2))
      return canTreatAsSameScalarType(scalarType1, scalarType2);
  }

  { // Vector types
    QualType elemType1 = {}, elemType2 = {};
    uint32_t count1 = {}, count2 = {};
    if (isVectorType(type1, &elemType1, &count1) &&
        isVectorType(type2, &elemType2, &count2))
      return count1 == count2 && canTreatAsSameScalarType(elemType1, elemType2);
  }

  return false;
}

bool isSameType(const ASTContext &astContext, QualType type1, QualType type2) {
  if (isSameScalarOrVecType(type1, type2))
    return true;

  type1.removeLocalConst();
  type2.removeLocalConst();

  { // Matrix types
    QualType elemType1 = {}, elemType2 = {};
    uint32_t row1 = 0, row2 = 0, col1 = 0, col2 = 0;
    if (isMxNMatrix(type1, &elemType1, &row1, &col1) &&
        isMxNMatrix(type2, &elemType2, &row2, &col2))
      return row1 == row2 && col1 == col2 &&
             canTreatAsSameScalarType(elemType1, elemType2);
  }

  { // Array types
    if (const auto *arrType1 = astContext.getAsConstantArrayType(type1))
      if (const auto *arrType2 = astContext.getAsConstantArrayType(type2))
        return hlsl::GetArraySize(type1) == hlsl::GetArraySize(type2) &&
               isSameType(astContext, arrType1->getElementType(),
                          arrType2->getElementType());
  }

  { // Two structures with identical fields
    if (const auto *structType1 = type1->getAs<RecordType>()) {
      if (const auto *structType2 = type2->getAs<RecordType>()) {
        llvm::SmallVector<QualType, 4> fieldTypes1;
        llvm::SmallVector<QualType, 4> fieldTypes2;
        for (const auto *field : structType1->getDecl()->fields())
          fieldTypes1.push_back(field->getType());
        for (const auto *field : structType2->getDecl()->fields())
          fieldTypes2.push_back(field->getType());
        // Note: We currently do NOT consider such cases as equal types:
        // struct s1 { int x; int y; }
        // struct s2 { int2 x; }
        // Therefore if two structs have different number of members, we
        // consider them different.
        if (fieldTypes1.size() != fieldTypes2.size())
          return false;
        for (size_t i = 0; i < fieldTypes1.size(); ++i)
          if (!isSameType(astContext, fieldTypes1[i], fieldTypes2[i]))
            return false;
        return true;
      }
    }
  }

  // TODO: support other types if needed

  return false;
}

QualType desugarType(QualType type, llvm::Optional<bool> *isRowMajor) {
  if (const auto *attrType = type->getAs<AttributedType>()) {
    switch (auto kind = attrType->getAttrKind()) {
    // HLSL row-major is SPIR-V col-major
    case AttributedType::attr_hlsl_row_major:
      *isRowMajor = false;
      break;
    // HLSL col-major is SPIR-V row-major
    case AttributedType::attr_hlsl_column_major:
      *isRowMajor = true;
      break;
    default:
      // Only looking matrix majorness attributes.
      break;
    }
    return desugarType(attrType->getLocallyUnqualifiedSingleStepDesugaredType(),
                       isRowMajor);
  }

  if (const auto *typedefType = type->getAs<TypedefType>()) {
    return desugarType(typedefType->desugar(), isRowMajor);
  }

  return type;
}

bool isRowMajorMatrix(const SpirvCodeGenOptions &spvOptions, QualType type) {
  // SPIR-V row-major is HLSL col-major and SPIR-V col-major is HLSL row-major.
  bool attrRowMajor = false;
  if (hlsl::HasHLSLMatOrientation(type, &attrRowMajor))
    return !attrRowMajor;

  // If it is a templated type the attribute may have been applied to the
  // underlying type. For example: StructuredBuffer<row_major float2x3>
  if (const auto *tst = dyn_cast<clang::TemplateSpecializationType>(type)) {
    if (tst->getNumArgs() >= 1) {
      auto args = tst->getArgs();
      auto templateArgument = args[0];
      auto templateArgumentType = templateArgument.getAsType();
      return isRowMajorMatrix(spvOptions, templateArgumentType);
    }
  }

  return !spvOptions.defaultRowMajor;
}

bool isStructuredBuffer(QualType type) {
  const auto *recordType = type->getAs<RecordType>();
  if (!recordType)
    return false;
  const auto name = recordType->getDecl()->getName();
  return name == "StructuredBuffer" || name == "RWStructuredBuffer";
}

bool isNonWritableStructuredBuffer(QualType type) {
  const auto *recordType = type->getAs<RecordType>();
  if (!recordType)
    return false;
  const auto name = recordType->getDecl()->getName();
  return name == "StructuredBuffer";
}

bool isByteAddressBuffer(QualType type) {
  if (const auto *rt = type->getAs<RecordType>()) {
    return rt->getDecl()->getName() == "ByteAddressBuffer";
  }
  return false;
}

bool isRWBuffer(QualType type) {
  if (const auto *rt = type->getAs<RecordType>()) {
    return rt->getDecl()->getName() == "RWBuffer";
  }
  return false;
}

bool isBuffer(QualType type) {
  if (const auto *rt = type->getAs<RecordType>()) {
    return rt->getDecl()->getName() == "Buffer";
  }
  return false;
}

bool isRWTexture(QualType type) {
  if (const auto *rt = type->getAs<RecordType>()) {
    const auto name = rt->getDecl()->getName();
    if (name == "RWTexture1D" || name == "RWTexture1DArray" ||
        name == "RWTexture2D" || name == "RWTexture2DArray" ||
        name == "RWTexture3D")
      return true;
  }
  return false;
}

bool isTexture(QualType type) {
  if (const auto *rt = type->getAs<RecordType>()) {
    const auto name = rt->getDecl()->getName();
    if (name == "Texture1D" || name == "Texture1DArray" ||
        name == "Texture2D" || name == "Texture2DArray" ||
        name == "Texture2DMS" || name == "Texture2DMSArray" ||
        name == "TextureCube" || name == "TextureCubeArray" ||
        name == "Texture3D")
      return true;
  }
  return false;
}

bool isTextureMS(QualType type) {
  if (const auto *rt = type->getAs<RecordType>()) {
    const auto name = rt->getDecl()->getName();
    if (name == "Texture2DMS" || name == "Texture2DMSArray")
      return true;
  }
  return false;
}

bool isSampler(QualType type) {
  if (const auto *rt = type->getAs<RecordType>()) {
    const auto name = rt->getDecl()->getName();
    if (name == "SamplerState" || name == "SamplerComparisonState")
      return true;
  }
  return false;
}

bool isRWByteAddressBuffer(QualType type) {
  if (const auto *rt = type->getAs<RecordType>()) {
    return rt->getDecl()->getName() == "RWByteAddressBuffer";
  }
  return false;
}

bool isAppendStructuredBuffer(QualType type) {
  const auto *recordType = type->getAs<RecordType>();
  if (!recordType)
    return false;
  const auto name = recordType->getDecl()->getName();
  return name == "AppendStructuredBuffer";
}

bool isConsumeStructuredBuffer(QualType type) {
  const auto *recordType = type->getAs<RecordType>();
  if (!recordType)
    return false;
  const auto name = recordType->getDecl()->getName();
  return name == "ConsumeStructuredBuffer";
}

bool isRWAppendConsumeSBuffer(QualType type) {
  if (const RecordType *recordType = type->getAs<RecordType>()) {
    StringRef name = recordType->getDecl()->getName();
    return name == "RWStructuredBuffer" || name == "AppendStructuredBuffer" ||
           name == "ConsumeStructuredBuffer";
  }
  return false;
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

bool isOpaqueType(QualType type) {
  if (const auto *recordType = type->getAs<RecordType>()) {
    const auto name = recordType->getDecl()->getName();

    if (name == "Texture1D" || name == "RWTexture1D")
      return true;
    if (name == "Texture2D" || name == "RWTexture2D")
      return true;
    if (name == "Texture2DMS" || name == "RWTexture2DMS")
      return true;
    if (name == "Texture3D" || name == "RWTexture3D")
      return true;
    if (name == "TextureCube" || name == "RWTextureCube")
      return true;

    if (name == "Texture1DArray" || name == "RWTexture1DArray")
      return true;
    if (name == "Texture2DArray" || name == "RWTexture2DArray")
      return true;
    if (name == "Texture2DMSArray" || name == "RWTexture2DMSArray")
      return true;
    if (name == "TextureCubeArray" || name == "RWTextureCubeArray")
      return true;

    if (name == "Buffer" || name == "RWBuffer")
      return true;

    if (name == "SamplerState" || name == "SamplerComparisonState")
      return true;

    if (name == "RaytracingAccelerationStructure")
      return true;

    if (name == "RayQuery")
      return true;
  }
  return false;
}

std::string getHlslResourceTypeName(QualType type) {
  if (type.isNull())
    return "";

  // Strip outer arrayness first
  while (type->isArrayType())
    type = type->getAsArrayTypeUnsafe()->getElementType();

  if (const RecordType *recordType = type->getAs<RecordType>()) {
    StringRef name = recordType->getDecl()->getName();
    if (name == "StructuredBuffer" || name == "RWStructuredBuffer" ||
        name == "ByteAddressBuffer" || name == "RWByteAddressBuffer" ||
        name == "AppendStructuredBuffer" || name == "ConsumeStructuredBuffer" ||
        name == "Texture1D" || name == "Texture2D" || name == "Texture3D" ||
        name == "TextureCube" || name == "Texture1DArray" ||
        name == "Texture2DArray" || name == "Texture2DMS" ||
        name == "Texture2DMSArray" || name == "TextureCubeArray" ||
        name == "RWTexture1D" || name == "RWTexture2D" ||
        name == "RWTexture3D" || name == "RWTexture1DArray" ||
        name == "RWTexture2DArray" || name == "Buffer" || name == "RWBuffer" ||
        name == "SubpassInput" || name == "SubpassInputMS" ||
        name == "InputPatch" || name == "OutputPatch") {
      return name;
    }
  }

  return "";
}

bool isOpaqueStructType(QualType type) {
  if (isOpaqueType(type))
    return false;

  if (const auto *recordType = type->getAs<RecordType>())
    for (const auto *field : recordType->getDecl()->decls())
      if (const auto *fieldDecl = dyn_cast<FieldDecl>(field))
        if (isOpaqueType(fieldDecl->getType()) ||
            isOpaqueStructType(fieldDecl->getType()))
          return true;

  return false;
}

bool isOpaqueArrayType(QualType type) {
  if (const auto *arrayType = type->getAsArrayTypeUnsafe())
    return isOpaqueType(arrayType->getElementType());
  return false;
}

bool isRelaxedPrecisionType(QualType type, const SpirvCodeGenOptions &opts) {
  if (type.isNull())
    return false;

  // Primitive types
  {
    QualType ty = {};
    if (isScalarType(type, &ty))
      if (const auto *builtinType = ty->getAs<BuiltinType>())
        switch (builtinType->getKind()) {
        case BuiltinType::Min12Int:
        case BuiltinType::Min16Int:
        case BuiltinType::Min16UInt:
        case BuiltinType::Min16Float:
        case BuiltinType::Min10Float: {
          // If '-enable-16bit-types' options is enabled, these types are
          // translated to real 16-bit type, and therefore are not
          // RelaxedPrecision.
          // If the options is not enabled, these types are translated to 32-bit
          // types with the added RelaxedPrecision decoration.
          return !opts.enable16BitTypes;
        default:
          // Filter switch only interested in relaxed precision eligible types.
          break;
        }
        }
  }

  // Vector & Matrix types could use relaxed precision based on their element
  // type.
  {
    QualType elemType = {};
    if (isVectorType(type, &elemType) || isMxNMatrix(type, &elemType))
      return isRelaxedPrecisionType(elemType, opts);
  }

  // Images with RelaxedPrecision sampled type.
  if (const auto *recordType = type->getAs<RecordType>()) {
    const llvm::StringRef name = recordType->getDecl()->getName();
    if (name == "Texture1D" || name == "Texture2D" || name == "Texture3D" ||
        name == "TextureCube" || name == "Texture1DArray" ||
        name == "Texture2DArray" || name == "Texture2DMS" ||
        name == "Texture2DMSArray" || name == "TextureCubeArray" ||
        name == "RWTexture1D" || name == "RWTexture2D" ||
        name == "RWTexture3D" || name == "RWTexture1DArray" ||
        name == "RWTexture2DArray" || name == "Buffer" || name == "RWBuffer" ||
        name == "SubpassInput" || name == "SubpassInputMS") {
      const auto sampledType = hlsl::GetHLSLResourceResultType(type);
      return isRelaxedPrecisionType(sampledType, opts);
    }
  }

  return false;
}

/// Returns true if the given type is a bool or vector of bool type.
bool isBoolOrVecOfBoolType(QualType type) {
  QualType elemType = {};
  return (isScalarType(type, &elemType) || isVectorType(type, &elemType)) &&
         elemType->isBooleanType();
}

/// Returns true if the given type is a signed integer or vector of signed
/// integer type.
bool isSintOrVecOfSintType(QualType type) {
  if (isEnumType(type))
    return true;

  QualType elemType = {};
  return (isScalarType(type, &elemType) || isVectorType(type, &elemType)) &&
         elemType->isSignedIntegerType();
}

/// Returns true if the given type is an unsigned integer or vector of unsigned
/// integer type.
bool isUintOrVecOfUintType(QualType type) {
  QualType elemType = {};
  return (isScalarType(type, &elemType) || isVectorType(type, &elemType)) &&
         elemType->isUnsignedIntegerType();
}

/// Returns true if the given type is a float or vector of float type.
bool isFloatOrVecOfFloatType(QualType type) {
  QualType elemType = {};
  return (isScalarType(type, &elemType) || isVectorType(type, &elemType)) &&
         elemType->isFloatingType();
}

/// Returns true if the given type is a bool or vector/matrix of bool type.
bool isBoolOrVecMatOfBoolType(QualType type) {
  return isBoolOrVecOfBoolType(type) ||
         (hlsl::IsHLSLMatType(type) &&
          hlsl::GetHLSLMatElementType(type)->isBooleanType());
}

/// Returns true if the given type is a signed integer or vector/matrix of
/// signed integer type.
bool isSintOrVecMatOfSintType(QualType type) {
  return isSintOrVecOfSintType(type) ||
         (hlsl::IsHLSLMatType(type) &&
          hlsl::GetHLSLMatElementType(type)->isSignedIntegerType());
}

/// Returns true if the given type is an unsigned integer or vector/matrix of
/// unsigned integer type.
bool isUintOrVecMatOfUintType(QualType type) {
  return isUintOrVecOfUintType(type) ||
         (hlsl::IsHLSLMatType(type) &&
          hlsl::GetHLSLMatElementType(type)->isUnsignedIntegerType());
}

/// Returns true if the given type is a float or vector/matrix of float type.
bool isFloatOrVecMatOfFloatType(QualType type) {
  return isFloatOrVecOfFloatType(type) ||
         (hlsl::IsHLSLMatType(type) &&
          hlsl::GetHLSLMatElementType(type)->isFloatingType());
}

bool isOrContainsNonFpColMajorMatrix(const ASTContext &astContext,
                                     const SpirvCodeGenOptions &spirvOptions,
                                     QualType type, const Decl *decl) {
  const auto isColMajorDecl = [&spirvOptions](const Decl *decl) {
    return decl->hasAttr<clang::HLSLColumnMajorAttr>() ||
           (!decl->hasAttr<clang::HLSLRowMajorAttr>() &&
            !spirvOptions.defaultRowMajor);
  };

  QualType elemType = {};
  if (isMxNMatrix(type, &elemType) && !elemType->isFloatingType()) {
    return isColMajorDecl(decl);
  }

  if (const auto *arrayType = astContext.getAsConstantArrayType(type)) {
    if (isMxNMatrix(arrayType->getElementType(), &elemType) &&
        !elemType->isFloatingType())
      return isColMajorDecl(decl);
    if (const auto *structType =
            arrayType->getElementType()->getAs<RecordType>()) {
      return isOrContainsNonFpColMajorMatrix(astContext, spirvOptions,
                                             arrayType->getElementType(),
                                             structType->getDecl());
    }
  }

  if (const auto *structType = type->getAs<RecordType>()) {
    const auto *decl = structType->getDecl();
    for (const auto *field : decl->fields()) {
      if (isOrContainsNonFpColMajorMatrix(astContext, spirvOptions,
                                          field->getType(), field))
        return true;
    }
  }

  return false;
}

bool isStringType(QualType type) {
  return hlsl::IsStringType(type) || hlsl::IsStringLiteralType(type);
}

bool isBindlessOpaqueArray(QualType type) {
  return !type.isNull() && isOpaqueArrayType(type) &&
         !type->isConstantArrayType();
}

QualType getComponentVectorType(const ASTContext &astContext,
                                QualType matrixType) {
  assert(isMxNMatrix(matrixType));

  const QualType elemType = hlsl::GetHLSLMatElementType(matrixType);
  uint32_t rowCount = 0, colCount = 0;
  hlsl::GetHLSLMatRowColCount(matrixType, rowCount, colCount);
  return astContext.getExtVectorType(elemType, colCount);
}

QualType getHLSLMatrixType(ASTContext &astContext, Sema &S,
                           ClassTemplateDecl *templateDecl, QualType elemType,
                           int rows, int columns) {
  const SourceLocation noLoc;
  TemplateArgument templateArgs[3] = {
      TemplateArgument(elemType),
      TemplateArgument(
          astContext,
          llvm::APSInt(
              llvm::APInt(astContext.getIntWidth(astContext.IntTy), rows),
              false),
          astContext.IntTy),
      TemplateArgument(
          astContext,
          llvm::APSInt(
              llvm::APInt(astContext.getIntWidth(astContext.IntTy), columns),
              false),
          astContext.IntTy)};

  SmallVector<TemplateArgument, 4> args;
  args.push_back(templateArgs[0]);
  args.push_back(templateArgs[1]);
  args.push_back(templateArgs[2]);

  DeclContext *currentDeclContext = astContext.getTranslationUnitDecl();
  SmallVector<TemplateArgument, 3> templateArgsForDecl;

  for (const TemplateArgument &Arg : templateArgs) {
    if (Arg.getKind() == TemplateArgument::Type) {
      // the class template need to use CanonicalType
      templateArgsForDecl.emplace_back(
          TemplateArgument(Arg.getAsType().getCanonicalType()));
    } else
      templateArgsForDecl.emplace_back(Arg);
  }

  // First, try looking up existing specialization
  void *insertPos = nullptr;
  ClassTemplateSpecializationDecl *specializationDecl =
      templateDecl->findSpecialization(templateArgsForDecl, insertPos);

  if (specializationDecl) {
    // Instantiate the class template if not done yet.
    if (specializationDecl->getInstantiatedFrom().isNull()) {
      S.InstantiateClassTemplateSpecialization(
          noLoc, specializationDecl,
          TemplateSpecializationKind::TSK_ImplicitInstantiation, true);
    }
    return astContext.getTemplateSpecializationType(
        TemplateName(templateDecl), args.data(), args.size(),
        astContext.getTypeDeclType(specializationDecl));
  }

  specializationDecl = ClassTemplateSpecializationDecl::Create(
      astContext, TagDecl::TagKind::TTK_Class, currentDeclContext, noLoc, noLoc,
      templateDecl, templateArgsForDecl.data(), templateArgsForDecl.size(),
      nullptr);
  S.InstantiateClassTemplateSpecialization(
      noLoc, specializationDecl,
      TemplateSpecializationKind::TSK_ImplicitInstantiation, true);
  templateDecl->AddSpecialization(specializationDecl, insertPos);
  specializationDecl->setImplicit(true);

  QualType canonType = astContext.getTypeDeclType(specializationDecl);
  TemplateArgumentListInfo templateArgumentList(noLoc, noLoc);
  TemplateArgumentLocInfo noTemplateArgumentLocInfo;

  for (unsigned i = 0; i < args.size(); i++) {
    templateArgumentList.addArgument(
        TemplateArgumentLoc(args[i], noTemplateArgumentLocInfo));
  }

  return astContext.getTemplateSpecializationType(
      TemplateName(templateDecl), templateArgumentList, canonType);
}

bool isResourceOnlyStructure(QualType type) {
  // Remove arrayness if needed.
  while (type->isArrayType())
    type = type->getAsArrayTypeUnsafe()->getElementType();

  if (const auto *structType = type->getAs<RecordType>()) {
    for (const auto *field : structType->getDecl()->fields()) {
      // isResourceType does remove arrayness for the field if needed.
      if (!isResourceType(field) &&
          !isResourceOnlyStructure(field->getType())) {
        return false;
      }
    }
    return true;
  }

  return false;
}

bool isStructureContainingResources(QualType type) {
  // Remove arrayness if needed.
  while (type->isArrayType())
    type = type->getAsArrayTypeUnsafe()->getElementType();

  if (const auto *structType = type->getAs<RecordType>()) {
    for (const auto *field : structType->getDecl()->fields()) {
      // isStructureContainingResources and isResourceType functions both remove
      // arrayness for the field if needed.
      if (isStructureContainingResources(field->getType()) ||
          isResourceType(field)) {
        return true;
      }
    }
  }
  return false;
}

bool isStructureContainingNonResources(QualType type) {
  // Remove arrayness if needed.
  while (type->isArrayType())
    type = type->getAsArrayTypeUnsafe()->getElementType();

  if (const auto *structType = type->getAs<RecordType>()) {
    for (const auto *field : structType->getDecl()->fields()) {
      // isStructureContainingNonResources and isResourceType functions both
      // remove arrayness for the field if needed.
      if (isStructureContainingNonResources(field->getType()) ||
          !isResourceType(field)) {
        return true;
      }
    }
  }
  return false;
}

bool isStructureContainingMixOfResourcesAndNonResources(QualType type) {
  return isStructureContainingResources(type) &&
         isStructureContainingNonResources(type);
}

bool isStructureContainingAnyKindOfBuffer(QualType type) {
  // Remove arrayness if needed.
  while (type->isArrayType())
    type = type->getAsArrayTypeUnsafe()->getElementType();

  if (const auto *structType = type->getAs<RecordType>()) {
    for (const auto *field : structType->getDecl()->fields()) {
      auto fieldType = field->getType();
      // Remove arrayness if needed.
      while (fieldType->isArrayType())
        fieldType = fieldType->getAsArrayTypeUnsafe()->getElementType();
      if (isAKindOfStructuredOrByteBuffer(fieldType) ||
          isConstantTextureBuffer(fieldType) ||
          isStructureContainingAnyKindOfBuffer(fieldType)) {
        return true;
      }
    }
  }
  return false;
}

} // namespace spirv
} // namespace clang
