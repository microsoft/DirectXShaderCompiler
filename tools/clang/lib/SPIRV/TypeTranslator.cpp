//===--- TypeTranslator.cpp - TypeTranslator implementation ------*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "TypeTranslator.h"

#include <algorithm>
#include <tuple>

#include "dxc/HLSL/DxilConstants.h"
#include "clang/AST/Attr.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/HlslTypes.h"

namespace clang {
namespace spirv {

namespace {
/// The alignment for 4-component float vectors.
constexpr uint32_t kStd140Vec4Alignment = 16u;

/// Returns true if the given value is a power of 2.
inline bool isPow2(int val) { return (val & (val - 1)) == 0; }

/// Rounds the given value up to the given power of 2.
inline void roundToPow2(uint32_t *val, uint32_t pow2) {
  assert(pow2 != 0);
  *val = (*val + pow2 - 1) & ~(pow2 - 1);
}
} // anonymous namespace

bool TypeTranslator::isRelaxedPrecisionType(QualType type) {
  // Primitive types
  {
    QualType ty = {};
    if (isScalarType(type, &ty))
      if (const auto *builtinType = ty->getAs<BuiltinType>())
        switch (builtinType->getKind()) {
        case BuiltinType::Short:
        case BuiltinType::UShort:
        case BuiltinType::Min12Int:
        case BuiltinType::Min10Float:
        case BuiltinType::Half:
          return true;
        }
  }

  // Vector & Matrix types could use relaxed precision based on their element
  // type.
  {
    QualType elemType = {};
    if (isVectorType(type, &elemType) || isMxNMatrix(type, &elemType))
      return isRelaxedPrecisionType(elemType);
  }

  return false;
}

bool TypeTranslator::isOpaqueType(QualType type) {
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
  }
  return false;
}

bool TypeTranslator::isOpaqueStructType(QualType type) {
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

void TypeTranslator::pushIntendedLiteralType(QualType type) {
  QualType elemType = {};
  if (isVectorType(type, &elemType)) {
    type = elemType;
  } else if (isMxNMatrix(type, &elemType)) {
    type = elemType;
  }
  assert(!type->isSpecificBuiltinType(BuiltinType::LitInt) &&
         !type->isSpecificBuiltinType(BuiltinType::LitFloat));
  intendedLiteralTypes.push(type);
}

QualType TypeTranslator::getIntendedLiteralType(QualType type) {
  if (!intendedLiteralTypes.empty())
    return intendedLiteralTypes.top();

  // We don't have any useful hints, return the given type itself.
  return type;
}

void TypeTranslator::popIntendedLiteralType() {
  if (!intendedLiteralTypes.empty())
    intendedLiteralTypes.pop();
}

uint32_t TypeTranslator::translateType(QualType type, LayoutRule rule,
                                       bool isRowMajor) {
  // We can only apply row_major to matrices or arrays of matrices.
  if (isRowMajor)
    assert(isMxNMatrix(type) || type->isArrayType());

  // Try to translate the canonical type first
  const auto canonicalType = type.getCanonicalType();
  if (canonicalType != type)
    return translateType(canonicalType, rule, isRowMajor);

  // Primitive types
  {
    QualType ty = {};
    if (isScalarType(type, &ty)) {
      if (const auto *builtinType = ty->getAs<BuiltinType>()) {
        switch (builtinType->getKind()) {
        case BuiltinType::Void:
          return theBuilder.getVoidType();
        case BuiltinType::Bool:
          return theBuilder.getBoolType();
          // int, min16int (short), and min12int are all translated to 32-bit
          // signed integers in SPIR-V.
        case BuiltinType::Int:
        case BuiltinType::Short:
        case BuiltinType::Min12Int:
          return theBuilder.getInt32Type();
          // uint and min16uint (ushort) are both translated to 32-bit unsigned
          // integers in SPIR-V.
        case BuiltinType::UShort:
        case BuiltinType::UInt:
          return theBuilder.getUint32Type();
        case BuiltinType::LongLong:
          return theBuilder.getInt64Type();
        case BuiltinType::ULongLong:
          return theBuilder.getUint64Type();
          // float, min16float (half), and min10float are all translated to
          // 32-bit float in SPIR-V.
        case BuiltinType::Float:
        case BuiltinType::Half:
        case BuiltinType::Min10Float:
          return theBuilder.getFloat32Type();
        case BuiltinType::Double:
          return theBuilder.getFloat64Type();
        case BuiltinType::LitFloat: {
          // First try to see if there are any hints about how this literal type
          // is going to be used. If so, use the hint.
          if (getIntendedLiteralType(ty) != ty) {
            return translateType(getIntendedLiteralType(ty));
          }

          const auto &semantics = astContext.getFloatTypeSemantics(type);
          const auto bitwidth = llvm::APFloat::getSizeInBits(semantics);
          if (bitwidth <= 32)
            return theBuilder.getFloat32Type();
          else
            return theBuilder.getFloat64Type();
        }
        case BuiltinType::LitInt: {
          // First try to see if there are any hints about how this literal type
          // is going to be used. If so, use the hint.
          if (getIntendedLiteralType(ty) != ty) {
            return translateType(getIntendedLiteralType(ty));
          }

          const auto bitwidth = astContext.getIntWidth(type);
          // All integer variants with bitwidth larger than 32 are represented
          // as 64-bit int in SPIR-V.
          // All integer variants with bitwidth of 32 or less are represented as
          // 32-bit int in SPIR-V.
          if (type->isSignedIntegerType())
            return bitwidth > 32 ? theBuilder.getInt64Type()
                                 : theBuilder.getInt32Type();
          else
            return bitwidth > 32 ? theBuilder.getUint64Type()
                                 : theBuilder.getUint32Type();
        }
        default:
          emitError("primitive type %0 unimplemented")
              << builtinType->getTypeClassName();
          return 0;
        }
      }
    }
  }

  // Typedefs
  if (const auto *typedefType = type->getAs<TypedefType>())
    return translateType(typedefType->desugar(), rule, isRowMajor);

  // Reference types
  if (const auto *refType = type->getAs<ReferenceType>()) {
    // Note: Pointer/reference types are disallowed in HLSL source code.
    // Although developers cannot use them directly, they are generated into
    // the AST by out/inout parameter modifiers in function signatures.
    // We already pass function arguments via pointers to tempoary local
    // variables. So it should be fine to drop the pointer type and treat it
    // as the underlying pointee type here.
    return translateType(refType->getPointeeType(), rule, isRowMajor);
  }

  // Pointer types
  if (const auto *ptrType = type->getAs<PointerType>()) {
    // The this object in a struct member function is of pointer type.
    return translateType(ptrType->getPointeeType(), rule, isRowMajor);
  }

  // In AST, vector/matrix types are TypedefType of TemplateSpecializationType.
  // We handle them via HLSL type inspection functions.

  // Vector types
  {
    QualType elemType = {};
    uint32_t elemCount = {};
    if (isVectorType(type, &elemType, &elemCount))
      return theBuilder.getVecType(translateType(elemType), elemCount);
  }

  // Matrix types
  {
    QualType elemType = {};
    uint32_t rowCount = 0, colCount = 0;
    if (isMxNMatrix(type, &elemType, &rowCount, &colCount)) {
      // NOTE: According to Item "Data rules" of SPIR-V Spec 2.16.1 "Universal
      // Validation Rules":
      //   Matrix types can only be parameterized with floating-point types.
      //
      // So we need special handling of non-fp matrices, probably by emulating
      // them using other types. But for now just disable them.
      if (!elemType->isFloatingType()) {
        emitError("Non-floating-point matrices not supported yet");
        return 0;
      }

      // HLSL matrices are row major, while SPIR-V matrices are column major.
      // We are mapping what HLSL semantically mean a row into a column here.
      const uint32_t vecType =
          theBuilder.getVecType(translateType(elemType), colCount);
      return theBuilder.getMatType(vecType, rowCount);
    }
  }

  // Struct type
  if (const auto *structType = type->getAs<RecordType>()) {
    const auto *decl = structType->getDecl();

    // HLSL resource types are also represented as RecordType in the AST.
    // (ClassTemplateSpecializationDecl is a subclass of CXXRecordDecl, which is
    // then a subclass of RecordDecl.) So we need to check them before checking
    // the general struct type.
    if (const auto id = translateResourceType(type, rule))
      return id;

    // Collect all fields' types and names.
    llvm::SmallVector<uint32_t, 4> fieldTypes;
    llvm::SmallVector<llvm::StringRef, 4> fieldNames;
    for (const auto *field : decl->fields()) {
      fieldTypes.push_back(translateType(field->getType(), rule,
                                         field->hasAttr<HLSLRowMajorAttr>()));
      fieldNames.push_back(field->getName());
    }

    llvm::SmallVector<const Decoration *, 4> decorations;
    if (rule != LayoutRule::Void) {
      decorations = getLayoutDecorations(decl, rule);
    }

    return theBuilder.getStructType(fieldTypes, decl->getName(), fieldNames,
                                    decorations);
  }

  if (const auto *arrayType = astContext.getAsConstantArrayType(type)) {
    const uint32_t elemType =
        translateType(arrayType->getElementType(), rule, isRowMajor);
    // TODO: handle extra large array size?
    const auto size =
        static_cast<uint32_t>(arrayType->getSize().getZExtValue());

    llvm::SmallVector<const Decoration *, 4> decorations;
    if (rule != LayoutRule::Void) {
      uint32_t stride = 0;
      (void)getAlignmentAndSize(type, rule, isRowMajor, &stride);
      decorations.push_back(
          Decoration::getArrayStride(*theBuilder.getSPIRVContext(), stride));
    }

    return theBuilder.getArrayType(elemType, theBuilder.getConstantUint32(size),
                                   decorations);
  }

  emitError("type %0 unimplemented") << type->getTypeClassName();
  type->dump();
  return 0;
}

uint32_t TypeTranslator::getACSBufferCounter() {
  auto &context = *theBuilder.getSPIRVContext();
  const uint32_t i32Type = theBuilder.getInt32Type();

  llvm::SmallVector<const Decoration *, 4> decorations;
  decorations.push_back(Decoration::getOffset(context, 0, 0));
  decorations.push_back(Decoration::getBufferBlock(context));

  return theBuilder.getStructType(i32Type, "type.ACSBuffer.counter", {},
                                  decorations);
}

uint32_t TypeTranslator::getGlPerVertexStruct(uint32_t clipArraySize,
                                              uint32_t cullArraySize,
                                              llvm::StringRef name) {
  const uint32_t f32Type = theBuilder.getFloat32Type();
  const uint32_t v4f32Type = theBuilder.getVecType(f32Type, 4);
  const uint32_t clipType = theBuilder.getArrayType(
      f32Type, theBuilder.getConstantUint32(clipArraySize));
  const uint32_t cullType = theBuilder.getArrayType(
      f32Type, theBuilder.getConstantUint32(cullArraySize));

  auto &ctx = *theBuilder.getSPIRVContext();
  llvm::SmallVector<const Decoration *, 1> decorations;

  decorations.push_back(Decoration::getBuiltIn(ctx, spv::BuiltIn::Position, 0));
  decorations.push_back(
      Decoration::getBuiltIn(ctx, spv::BuiltIn::PointSize, 1));
  decorations.push_back(
      Decoration::getBuiltIn(ctx, spv::BuiltIn::ClipDistance, 2));
  decorations.push_back(
      Decoration::getBuiltIn(ctx, spv::BuiltIn::CullDistance, 3));
  decorations.push_back(Decoration::getBlock(ctx));

  return theBuilder.getStructType({v4f32Type, f32Type, clipType, cullType},
                                  name, {}, decorations);
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

bool TypeTranslator::isRWByteAddressBuffer(QualType type) {
  if (const auto *rt = type->getAs<RecordType>()) {
    return rt->getDecl()->getName() == "RWByteAddressBuffer";
  }
  return false;
}

bool TypeTranslator::isAppendStructuredBuffer(QualType type) {
  const auto *recordType = type->getAs<RecordType>();
  if (!recordType)
    return false;
  const auto name = recordType->getDecl()->getName();
  return name == "AppendStructuredBuffer";
}

bool TypeTranslator::isConsumeStructuredBuffer(QualType type) {
  const auto *recordType = type->getAs<RecordType>();
  if (!recordType)
    return false;
  const auto name = recordType->getDecl()->getName();
  return name == "ConsumeStructuredBuffer";
}

bool TypeTranslator::isRWAppendConsumeSBuffer(QualType type) {
  if (const RecordType *recordType = type->getAs<RecordType>()) {
    StringRef name = recordType->getDecl()->getName();
    return name == "RWStructuredBuffer" || name == "AppendStructuredBuffer" ||
           name == "ConsumeStructuredBuffer";
  }
  return false;
}

bool TypeTranslator::isAKindOfStructuredOrByteBuffer(QualType type) {
  if (const RecordType *recordType = type->getAs<RecordType>()) {
    StringRef name = recordType->getDecl()->getName();
    return name == "StructuredBuffer" || name == "RWStructuredBuffer" ||
           name == "ByteAddressBuffer" || name == "RWByteAddressBuffer" ||
           name == "AppendStructuredBuffer" ||
           name == "ConsumeStructuredBuffer";
  }
  return false;
}

bool TypeTranslator::isStructuredBuffer(QualType type) {
  const auto *recordType = type->getAs<RecordType>();
  if (!recordType)
    return false;
  const auto name = recordType->getDecl()->getName();
  return name == "StructuredBuffer" || name == "RWStructuredBuffer";
}

bool TypeTranslator::isByteAddressBuffer(QualType type) {
  if (const auto *rt = type->getAs<RecordType>()) {
    return rt->getDecl()->getName() == "ByteAddressBuffer";
  }
  return false;
}

bool TypeTranslator::isRWBuffer(QualType type) {
  if (const auto *rt = type->getAs<RecordType>()) {
    return rt->getDecl()->getName() == "RWBuffer";
  }
  return false;
}

bool TypeTranslator::isBuffer(QualType type) {
  if (const auto *rt = type->getAs<RecordType>()) {
    return rt->getDecl()->getName() == "Buffer";
  }
  return false;
}

bool TypeTranslator::isRWTexture(QualType type) {
  if (const auto *rt = type->getAs<RecordType>()) {
    const auto name = rt->getDecl()->getName();
    if (name == "RWTexture1D" || name == "RWTexture1DArray" ||
        name == "RWTexture2D" || name == "RWTexture2DArray" ||
        name == "RWTexture3D")
      return true;
  }
  return false;
}

bool TypeTranslator::isTexture(QualType type) {
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

bool TypeTranslator::isTextureMS(QualType type) {
  if (const auto *rt = type->getAs<RecordType>()) {
    const auto name = rt->getDecl()->getName();
    if (name == "Texture2DMS" || name == "Texture2DMSArray")
      return true;
  }
  return false;
}

bool TypeTranslator::isSampler(QualType type) {
  if (const auto *rt = type->getAs<RecordType>()) {
    const auto name = rt->getDecl()->getName();
    if (name == "SamplerState" || name == "SamplerComparisonState")
      return true;
  }
  return false;
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

spv::Capability
TypeTranslator::getCapabilityForStorageImageReadWrite(QualType type) {
  if (const auto *rt = type->getAs<RecordType>()) {
    const auto name = rt->getDecl()->getName();
    // RWBuffer translates into OpTypeImage Buffer with Sampled = 2
    if (name == "RWBuffer")
      return spv::Capability::ImageBuffer;
    // RWBuffer translates into OpTypeImage 1D with Sampled = 2
    if (name == "RWTexture1D" || name == "RWTexture1DArray")
      return spv::Capability::Image1D;
  }
  return spv::Capability::Max;
}

llvm::SmallVector<const Decoration *, 4>
TypeTranslator::getLayoutDecorations(const DeclContext *decl, LayoutRule rule) {
  const auto spirvContext = theBuilder.getSPIRVContext();
  llvm::SmallVector<const Decoration *, 4> decorations;
  uint32_t offset = 0, index = 0;

  for (const auto *field : decl->decls()) {
    // Ignore implicit generated struct declarations/constructors/destructors.
    if (field->isImplicit())
      continue;

    // The field can only be FieldDecl (for normal structs) or VarDecl (for
    // HLSLBufferDecls).
    auto fieldType = cast<DeclaratorDecl>(field)->getType();
    const bool isRowMajor = field->hasAttr<HLSLRowMajorAttr>();

    uint32_t memberAlignment = 0, memberSize = 0, stride = 0;
    std::tie(memberAlignment, memberSize) =
        getAlignmentAndSize(fieldType, rule, isRowMajor, &stride);

    // Each structure-type member must have an Offset Decoration.
    const auto *offsetAttr = field->getAttr<VKOffsetAttr>();
    if (offsetAttr)
        offset = offsetAttr->getOffset();
    else
        roundToPow2(&offset, memberAlignment);
    decorations.push_back(Decoration::getOffset(*spirvContext, offset, index));
    offset += memberSize;

    // Each structure-type member that is a matrix or array-of-matrices must be
    // decorated with
    // * A MatrixStride decoration, and
    // * one of the RowMajor or ColMajor Decorations.
    if (const auto *arrayType = astContext.getAsConstantArrayType(fieldType)) {
      // We have an array of matrices as a field, we need to decorate
      // MatrixStride on the field. So skip possible arrays here.
      fieldType = arrayType->getElementType();
    }
    if (isMxNMatrix(fieldType)) {
      memberAlignment = memberSize = stride = 0;
      std::tie(memberAlignment, memberSize) =
          getAlignmentAndSize(fieldType, rule, isRowMajor, &stride);

      decorations.push_back(
          Decoration::getMatrixStride(*spirvContext, stride, index));

      // We need to swap the RowMajor and ColMajor decorations since HLSL
      // matrices are conceptually row-major while SPIR-V are conceptually
      // column-major.
      if (isRowMajor) {
        decorations.push_back(Decoration::getColMajor(*spirvContext, index));
      } else {
        // If the source code has neither row_major nor column_major annotated,
        // it should be treated as column_major since that's the default.
        decorations.push_back(Decoration::getRowMajor(*spirvContext, index));
      }
    }

    ++index;
  }

  return decorations;
}

uint32_t TypeTranslator::translateResourceType(QualType type, LayoutRule rule) {
  // Resource types are either represented like C struct or C++ class in the
  // AST. Samplers are represented like C struct, so isStructureType() will
  // return true for it; textures are represented like C++ class, so
  // isClassType() will return true for it.

  assert(type->isStructureOrClassType());

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
        (dim = spv::Dim::Dim2D, isArray = false, name == "Texture2DMS") ||
        (dim = spv::Dim::Dim2D, isArray = true, name == "Texture2DMSArray") ||
        // There is no Texture3DArray
        (dim = spv::Dim::Cube, isArray = true, name == "TextureCubeArray")) {
      const auto isMS = (name == "Texture2DMS" || name == "Texture2DMSArray");
      const auto sampledType = hlsl::GetHLSLResourceResultType(type);
      return theBuilder.getImageType(translateType(getElementType(sampledType)),
                                     dim, /*depth*/ 0, isArray, isMS);
    }

    // There is no RWTexture3DArray
    if ((dim = spv::Dim::Dim1D, isArray = false, name == "RWTexture1D") ||
        (dim = spv::Dim::Dim2D, isArray = false, name == "RWTexture2D") ||
        (dim = spv::Dim::Dim3D, isArray = false, name == "RWTexture3D") ||
        (dim = spv::Dim::Dim1D, isArray = true, name == "RWTexture1DArray") ||
        (dim = spv::Dim::Dim2D, isArray = true, name == "RWTexture2DArray")) {
      const auto sampledType = hlsl::GetHLSLResourceResultType(type);
      const auto format = translateSampledTypeToImageFormat(sampledType);
      return theBuilder.getImageType(translateType(getElementType(sampledType)),
                                     dim, /*depth*/ 0, isArray, /*MS*/ 0,
                                     /*Sampled*/ 2u, format);
    }
  }

  // Sampler types
  if (name == "SamplerState" || name == "SamplerComparisonState") {
    return theBuilder.getSamplerType();
  }

  if (name == "StructuredBuffer" || name == "RWStructuredBuffer" ||
      name == "AppendStructuredBuffer" || name == "ConsumeStructuredBuffer") {
    auto &context = *theBuilder.getSPIRVContext();
    // StructureBuffer<S> will be translated into an OpTypeStruct with one
    // field, which is an OpTypeRuntimeArray of OpTypeStruct (S).

    const auto s = hlsl::GetHLSLResourceResultType(type);
    const uint32_t structType = translateType(s, rule);
    std::string structName;
    const auto innerType = hlsl::GetHLSLResourceResultType(type);
    if (innerType->isStructureType())
      structName = innerType->getAs<RecordType>()->getDecl()->getName();
    else
      structName = getName(innerType);

    llvm::SmallVector<const Decoration *, 4> decorations;
    // The stride for the runtime array is the size of S.
    uint32_t size = 0, stride = 0;
    std::tie(std::ignore, size) =
        getAlignmentAndSize(s, rule, /*isRowMajor*/ false, &stride);
    decorations.push_back(Decoration::getArrayStride(context, size));
    const uint32_t raType =
        theBuilder.getRuntimeArrayType(structType, decorations);

    decorations.clear();
    decorations.push_back(Decoration::getOffset(context, 0, 0));
    if (name == "StructuredBuffer")
      decorations.push_back(Decoration::getNonWritable(context, 0));
    decorations.push_back(Decoration::getBufferBlock(context));
    const std::string typeName = "type." + name.str() + "." + structName;
    return theBuilder.getStructType(raType, typeName, {}, decorations);
  }

  // ByteAddressBuffer types.
  if (name == "ByteAddressBuffer") {
    return theBuilder.getByteAddressBufferType(/*isRW*/ false);
  }
  // RWByteAddressBuffer types.
  if (name == "RWByteAddressBuffer") {
    return theBuilder.getByteAddressBufferType(/*isRW*/ true);
  }

  // Buffer and RWBuffer types
  if (name == "Buffer" || name == "RWBuffer") {
    theBuilder.requireCapability(spv::Capability::SampledBuffer);
    const auto sampledType = hlsl::GetHLSLResourceResultType(type);
    const auto format = translateSampledTypeToImageFormat(sampledType);
    return theBuilder.getImageType(
        translateType(getElementType(sampledType)), spv::Dim::Buffer,
        /*depth*/ 0, /*isArray*/ 0, /*ms*/ 0,
        /*sampled*/ name == "Buffer" ? 1 : 2, format);
  }

  // InputPatch
  if (name == "InputPatch") {
    const auto elemType = hlsl::GetHLSLInputPatchElementType(type);
    const auto elemCount = hlsl::GetHLSLInputPatchCount(type);
    const uint32_t elemTypeId = translateType(elemType);
    const uint32_t elemCountId = theBuilder.getConstantUint32(elemCount);
    return theBuilder.getArrayType(elemTypeId, elemCountId);
  }
  // OutputPatch
  if (name == "OutputPatch") {
    const auto elemType = hlsl::GetHLSLOutputPatchElementType(type);
    const auto elemCount = hlsl::GetHLSLOutputPatchCount(type);
    const uint32_t elemTypeId = translateType(elemType);
    const uint32_t elemCountId = theBuilder.getConstantUint32(elemCount);
    return theBuilder.getArrayType(elemTypeId, elemCountId);
  }
  // Output stream objects (TriangleStream, LineStream, and PointStream)
  if (name == "TriangleStream" || name == "LineStream" ||
      name == "PointStream") {
    return translateType(hlsl::GetHLSLResourceResultType(type), rule);
  }

  return 0;
}

spv::ImageFormat
TypeTranslator::translateSampledTypeToImageFormat(QualType sampledType) {
  uint32_t elemCount = 1;
  QualType ty = {};
  if (isScalarType(sampledType, &ty) ||
      isVectorType(sampledType, &ty, &elemCount)) {
    if (const auto *builtinType = ty->getAs<BuiltinType>()) {
      switch (builtinType->getKind()) {
      case BuiltinType::Int:
        return elemCount == 1 ? spv::ImageFormat::R32i
                              : elemCount == 2 ? spv::ImageFormat::Rg32i
                                               : spv::ImageFormat::Rgba32i;
      case BuiltinType::UInt:
        return elemCount == 1 ? spv::ImageFormat::R32ui
                              : elemCount == 2 ? spv::ImageFormat::Rg32ui
                                               : spv::ImageFormat::Rgba32ui;
      case BuiltinType::Float:
        return elemCount == 1 ? spv::ImageFormat::R32f
                              : elemCount == 2 ? spv::ImageFormat::Rg32f
                                               : spv::ImageFormat::Rgba32f;
      }
    }
  }
  emitError("resource type %0 unimplemented") << sampledType.getAsString();
  return spv::ImageFormat::Unknown;
}

std::pair<uint32_t, uint32_t>
TypeTranslator::getAlignmentAndSize(QualType type, LayoutRule rule,
                                    const bool isRowMajor, uint32_t *stride) {
  // std140 layout rules:

  // 1. If the member is a scalar consuming N basic machine units, the base
  //    alignment is N.
  //
  // 2. If the member is a two- or four-component vector with components
  //    consuming N basic machine units, the base alignment is 2N or 4N,
  //    respectively.
  //
  // 3. If the member is a three-component vector with components consuming N
  //    basic machine units, the base alignment is 4N.
  //
  // 4. If the member is an array of scalars or vectors, the base alignment and
  //    array stride are set to match the base alignment of a single array
  //    element, according to rules (1), (2), and (3), and rounded up to the
  //    base alignment of a vec4. The array may have padding at the end; the
  //    base offset of the member following the array is rounded up to the next
  //    multiple of the base alignment.
  //
  // 5. If the member is a column-major matrix with C columns and R rows, the
  //    matrix is stored identically to an array of C column vectors with R
  //    components each, according to rule (4).
  //
  // 6. If the member is an array of S column-major matrices with C columns and
  //    R rows, the matrix is stored identically to a row of S X C column
  //    vectors with R components each, according to rule (4).
  //
  // 7. If the member is a row-major matrix with C columns and R rows, the
  //    matrix is stored identically to an array of R row vectors with C
  //    components each, according to rule (4).
  //
  // 8. If the member is an array of S row-major matrices with C columns and R
  //    rows, the matrix is stored identically to a row of S X R row vectors
  //    with C
  //    components each, according to rule (4).
  //
  // 9. If the member is a structure, the base alignment of the structure is N,
  //    where N is the largest base alignment value of any of its members, and
  //    rounded up to the base alignment of a vec4. The individual members of
  //    this substructure are then assigned offsets by applying this set of
  //    rules recursively, where the base offset of the first member of the
  //    sub-structure is equal to the aligned offset of the structure. The
  //    structure may have padding at the end; the base offset of the member
  //    following the sub-structure is rounded up to the next multiple of the
  //    base alignment of the structure.
  //
  // 10. If the member is an array of S structures, the S elements of the array
  //     are laid out in order, according to rule (9).
  const auto canonicalType = type.getCanonicalType();
  if (canonicalType != type)
    return getAlignmentAndSize(canonicalType, rule, isRowMajor, stride);

  if (const auto *typedefType = type->getAs<TypedefType>())
    return getAlignmentAndSize(typedefType->desugar(), rule, isRowMajor,
                               stride);

  { // Rule 1
    QualType ty = {};
    if (isScalarType(type, &ty))
      if (const auto *builtinType = ty->getAs<BuiltinType>())
        switch (builtinType->getKind()) {
        case BuiltinType::Void:
          return {0, 0};
        case BuiltinType::Bool:
        case BuiltinType::Int:
        case BuiltinType::UInt:
        case BuiltinType::Float:
          return {4, 4};
        default:
          emitError("primitive type %0 unimplemented")
              << builtinType->getTypeClassName();
          return {0, 0};
        }
  }

  { // Rule 2 and 3
    QualType elemType = {};
    uint32_t elemCount = {};
    if (isVectorType(type, &elemType, &elemCount)) {
      uint32_t size = 0;
      std::tie(std::ignore, size) =
          getAlignmentAndSize(elemType, rule, isRowMajor, stride);

      return {(elemCount == 3 ? 4 : elemCount) * size, elemCount * size};
    }
  }

  { // Rule 5 and 7
    QualType elemType = {};
    uint32_t rowCount = 0, colCount = 0;
    if (isMxNMatrix(type, &elemType, &rowCount, &colCount)) {
      uint32_t alignment = 0, size = 0;
      std::tie(alignment, std::ignore) =
          getAlignmentAndSize(elemType, rule, isRowMajor, stride);

      // Matrices are treated as arrays of vectors:
      // The base alignment and array stride are set to match the base alignment
      // of a single array element, according to rules 1, 2, and 3, and rounded
      // up to the base alignment of a vec4.
      const uint32_t vecStorageSize = isRowMajor ? colCount : rowCount;
      alignment *= (vecStorageSize == 3 ? 4 : vecStorageSize);
      if (rule == LayoutRule::GLSLStd140) {
        roundToPow2(&alignment, kStd140Vec4Alignment);
      }
      *stride = alignment;
      size = (isRowMajor ? rowCount : colCount) * alignment;

      return {alignment, size};
    }
  }

  // Rule 9
  if (const auto *structType = type->getAs<RecordType>()) {
    // Special case for handling empty structs, whose size is 0 and has no
    // requirement over alignment (thus 1).
    if (structType->getDecl()->field_empty())
      return {1, 0};

    uint32_t maxAlignment = 0;
    uint32_t structSize = 0;

    for (const auto *field : structType->getDecl()->fields()) {
      uint32_t memberAlignment = 0, memberSize = 0;
      std::tie(memberAlignment, memberSize) = getAlignmentAndSize(
          field->getType(), rule, field->hasAttr<HLSLRowMajorAttr>(), stride);

      // The base alignment of the structure is N, where N is the largest
      // base alignment value of any of its members...
      maxAlignment = std::max(maxAlignment, memberAlignment);
      roundToPow2(&structSize, memberAlignment);
      structSize += memberSize;
    }

    if (rule == LayoutRule::GLSLStd140) {
      // ... and rounded up to the base alignment of a vec4.
      roundToPow2(&maxAlignment, kStd140Vec4Alignment);
    }
    // The base offset of the member following the sub-structure is rounded up
    // to the next multiple of the base alignment of the structure.
    roundToPow2(&structSize, maxAlignment);
    return {maxAlignment, structSize};
  }

  // Rule 4, 6, 8, and 10
  if (const auto *arrayType = astContext.getAsConstantArrayType(type)) {
    uint32_t alignment = 0, size = 0;
    std::tie(alignment, size) = getAlignmentAndSize(arrayType->getElementType(),
                                                    rule, isRowMajor, stride);

    if (rule == LayoutRule::GLSLStd140) {
      // The base alignment and array stride are set to match the base alignment
      // of a single array element, according to rules 1, 2, and 3, and rounded
      // up to the base alignment of a vec4.
      roundToPow2(&alignment, kStd140Vec4Alignment);
    }
    // Need to round size up considering stride for scalar types
    roundToPow2(&size, alignment);
    *stride = size; // Use size instead of alignment here for Rule 10
    // TODO: handle extra large array size?
    size *= static_cast<uint32_t>(arrayType->getSize().getZExtValue());
    // The base offset of the member following the array is rounded up to the
    // next multiple of the base alignment.
    roundToPow2(&size, alignment);

    return {alignment, size};
  }

  emitError("type %0 unimplemented") << type->getTypeClassName();
  return {0, 0};
}

std::string TypeTranslator::getName(QualType type) {
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
        default:
          return "";
        }
  }

  {
    QualType elemType = {};
    uint32_t elemCount = {};
    if (isVectorType(type, &elemType, &elemCount))
      return "v" + std::to_string(elemCount) + getName(elemType);
  }

  {
    QualType elemType = {};
    uint32_t rowCount = 0, colCount = 0;
    if (isMxNMatrix(type, &elemType, &rowCount, &colCount))
      return "mat" + std::to_string(rowCount) + "v" + std::to_string(colCount) +
             getName(elemType);
  }

  if (const auto *structType = type->getAs<RecordType>())
    return structType->getDecl()->getName();

  return "";
}

} // end namespace spirv
} // end namespace clang
