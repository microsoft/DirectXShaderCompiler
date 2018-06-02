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
inline uint32_t roundToPow2(uint32_t val, uint32_t pow2) {
  assert(pow2 != 0);
  return (val + pow2 - 1) & ~(pow2 - 1);
}

/// Returns true if the given vector type (of the given size) crosses the
/// 4-component vector boundary if placed at the given offset.
bool improperStraddle(QualType type, int size, int offset) {
  assert(TypeTranslator::isVectorType(type));
  return size <= 16 ? offset / 16 != (offset + size - 1) / 16
                    : offset % 16 != 0;
}

// From https://github.com/Microsoft/DirectXShaderCompiler/pull/1032.
// TODO: use that after it is landed.
bool hasHLSLMatOrientation(QualType type, bool *pIsRowMajor) {
  const AttributedType *AT = type->getAs<AttributedType>();
  while (AT) {
    AttributedType::Kind kind = AT->getAttrKind();
    switch (kind) {
    case AttributedType::attr_hlsl_row_major:
      if (pIsRowMajor)
        *pIsRowMajor = true;
      return true;
    case AttributedType::attr_hlsl_column_major:
      if (pIsRowMajor)
        *pIsRowMajor = false;
      return true;
    }
    AT = AT->getLocallyUnqualifiedSingleStepDesugaredType()
             ->getAs<AttributedType>();
  }
  return false;
}

/// Returns the :packoffset() annotation on the given decl. Returns nullptr if
/// the decl does not have one.
const hlsl::ConstantPacking *getPackOffset(const NamedDecl *decl) {
  for (auto *annotation : decl->getUnusualAnnotations())
    if (auto *packing = dyn_cast<hlsl::ConstantPacking>(annotation))
      return packing;
  return nullptr;
}

} // anonymous namespace

bool TypeTranslator::isRelaxedPrecisionType(QualType type,
                                            const EmitSPIRVOptions &opts) {
  // Primitive types
  {
    QualType ty = {};
    if (isScalarType(type, &ty))
      if (const auto *builtinType = ty->getAs<BuiltinType>())
        switch (builtinType->getKind()) {
        // TODO: Figure out why 'min16float' and 'half' share an enum.
        // 'half' should not get RelaxedPrecision decoration, but due to the
        // shared enum, we currently do so.
        case BuiltinType::Half:
        case BuiltinType::Short:
        case BuiltinType::UShort:
        case BuiltinType::Min12Int:
        case BuiltinType::Min10Float: {
          // If '-enable-16bit-types' options is enabled, these types are
          // translated to real 16-bit type, and therefore are not
          // RelaxedPrecision.
          // If the options is not enabled, these types are translated to 32-bit
          // types with the added RelaxedPrecision decoration.
          return !opts.enable16BitTypes;
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

bool TypeTranslator::isOpaqueArrayType(QualType type) {
  if (const auto *arrayType = type->getAsArrayTypeUnsafe())
    return isOpaqueType(arrayType->getElementType());
  return false;
}

void TypeTranslator::LiteralTypeHint::setHint(QualType ty) {
  // You can set hint only once for each object.
  assert(type == QualType());
  type = ty;
  translator.pushIntendedLiteralType(type);
}

TypeTranslator::LiteralTypeHint::LiteralTypeHint(TypeTranslator &t)
    : translator(t), type({}) {}

TypeTranslator::LiteralTypeHint::LiteralTypeHint(TypeTranslator &t, QualType ty)
    : translator(t), type(ty) {
  if (!isLiteralType(type))
    translator.pushIntendedLiteralType(type);
}

TypeTranslator::LiteralTypeHint::~LiteralTypeHint() {
  if (type != QualType() && !isLiteralType(type))
    translator.popIntendedLiteralType();
}

bool TypeTranslator::LiteralTypeHint::isLiteralType(QualType type) {
  if (type->isSpecificBuiltinType(BuiltinType::LitInt) ||
      type->isSpecificBuiltinType(BuiltinType::LitFloat))
    return true;

  // For cases such as 'vector<literal int, 2>' or 'vector<literal float, 1>'
  if (hlsl::IsHLSLVecType(type))
    return isLiteralType(hlsl::GetHLSLVecElementType(type));

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
  if (!intendedLiteralTypes.empty()) {
    // If the stack is not empty, there is potentially a useful hint about how a
    // given literal should be translated.
    //
    // However, a hint should not be returned blindly. It is possible that casts
    // are occuring. For Example:
    //
    //   TU
    //    |_ n1: <IntegralToFloating> float
    //       |_ n2: ConditionalOperator 'literal int'
    //          |_ n3: cond, bool
    //          |_ n4: 'literal int' 2
    //          |_ n5: 'literal int' 3
    //
    // When evaluating the return type of ConditionalOperator, we shouldn't
    // provide 'float' as hint. The cast AST node should take care of that.
    // In the above example, we have no hints about how '2' or '3' should be
    // used.
    QualType potentialHint = intendedLiteralTypes.top();
    const bool hintIsInteger =
        potentialHint->isIntegerType() && !potentialHint->isBooleanType();
    const bool hintIsFloating = potentialHint->isFloatingType();
    const bool isDifferentBasicType =
        (type->isSpecificBuiltinType(BuiltinType::LitInt) && !hintIsInteger) ||
        (type->isSpecificBuiltinType(BuiltinType::LitFloat) && !hintIsFloating);

    if (!isDifferentBasicType)
      return intendedLiteralTypes.top();
  }

  // We don't have any useful hints, return the given type itself.
  return type;
}

void TypeTranslator::popIntendedLiteralType() {
  assert(!intendedLiteralTypes.empty());
  intendedLiteralTypes.pop();
}

uint32_t TypeTranslator::getLocationCount(QualType type) {
  // See Vulkan spec 14.1.4. Location Assignment for the complete set of rules.

  const auto canonicalType = type.getCanonicalType();
  if (canonicalType != type)
    return getLocationCount(canonicalType);

  // Inputs and outputs of the following types consume a single interface
  // location:
  // * 16-bit scalar and vector types, and
  // * 32-bit scalar and vector types, and
  // * 64-bit scalar and 2-component vector types.

  // 64-bit three- and four- component vectors consume two consecutive
  // locations.

  // Primitive types
  if (isScalarType(type))
    return 1;

  // Vector types
  {
    QualType elemType = {};
    uint32_t elemCount = {};
    if (isVectorType(type, &elemType, &elemCount)) {
      const auto *builtinType = elemType->getAs<BuiltinType>();
      switch (builtinType->getKind()) {
      case BuiltinType::Double:
      case BuiltinType::LongLong:
      case BuiltinType::ULongLong:
        if (elemCount >= 3)
          return 2;
      }
      return 1;
    }
  }

  // If the declared input or output is an n * m 16- , 32- or 64- bit matrix,
  // it will be assigned multiple locations starting with the location
  // specified. The number of locations assigned for each matrix will be the
  // same as for an n-element array of m-component vectors.

  // Matrix types
  {
    QualType elemType = {};
    uint32_t rowCount = 0, colCount = 0;
    if (isMxNMatrix(type, &elemType, &rowCount, &colCount))
      return getLocationCount(astContext.getExtVectorType(elemType, colCount)) *
             rowCount;
  }

  // Typedefs
  if (const auto *typedefType = type->getAs<TypedefType>())
    return getLocationCount(typedefType->desugar());

  // Reference types
  if (const auto *refType = type->getAs<ReferenceType>())
    return getLocationCount(refType->getPointeeType());

  // Pointer types
  if (const auto *ptrType = type->getAs<PointerType>())
    return getLocationCount(ptrType->getPointeeType());

  // If a declared input or output is an array of size n and each element takes
  // m locations, it will be assigned m * n consecutive locations starting with
  // the location specified.

  // Array types
  if (const auto *arrayType = astContext.getAsConstantArrayType(type))
    return getLocationCount(arrayType->getElementType()) *
           static_cast<uint32_t>(arrayType->getSize().getZExtValue());

  // Struct type
  if (const auto *structType = type->getAs<RecordType>()) {
    assert(false && "all structs should already be flattened");
    return 0;
  }

  emitError(
      "calculating number of occupied locations for type %0 unimplemented")
      << type;
  return 0;
}

uint32_t TypeTranslator::getTypeWithCustomBitwidth(QualType type,
                                                   uint32_t bitwidth) {
  // Cases where the given type is a vector of float/int.
  {
    QualType elemType = {};
    uint32_t elemCount = 0;
    const bool isVec = isVectorType(type, &elemType, &elemCount);
    if (isVec) {
      return theBuilder.getVecType(
          getTypeWithCustomBitwidth(elemType, bitwidth), elemCount);
    }
  }

  // Scalar cases.
  assert(!type->isBooleanType());
  assert(type->isIntegerType() || type->isFloatingType());
  if (type->isFloatingType()) {
    switch (bitwidth) {
    case 16:
      return theBuilder.getFloat16Type();
    case 32:
      return theBuilder.getFloat32Type();
    case 64:
      return theBuilder.getFloat64Type();
    }
  }
  if (type->isSignedIntegerType()) {
    switch (bitwidth) {
    case 16:
      return theBuilder.getInt16Type();
    case 32:
      return theBuilder.getInt32Type();
    case 64:
      return theBuilder.getInt64Type();
    }
  }
  if (type->isUnsignedIntegerType()) {
    switch (bitwidth) {
    case 16:
      return theBuilder.getUint16Type();
    case 32:
      return theBuilder.getUint32Type();
    case 64:
      return theBuilder.getUint64Type();
    }
  }
  llvm_unreachable(
      "invalid type or bitwidth passed to getTypeWithCustomBitwidth");
}

uint32_t TypeTranslator::getElementSpirvBitwidth(QualType type) {
  const auto canonicalType = type.getCanonicalType();
  if (canonicalType != type)
    return getElementSpirvBitwidth(canonicalType);

  // Vector types
  {
    QualType elemType = {};
    if (isVectorType(type, &elemType))
      return getElementSpirvBitwidth(elemType);
  }

  // Matrix types
  if (hlsl::IsHLSLMatType(type))
    return getElementSpirvBitwidth(hlsl::GetHLSLMatElementType(type));

  // Array types
  if (const auto *arrayType = type->getAsArrayTypeUnsafe()) {
    return getElementSpirvBitwidth(arrayType->getElementType());
  }

  // Typedefs
  if (const auto *typedefType = type->getAs<TypedefType>())
    return getLocationCount(typedefType->desugar());

  // Reference types
  if (const auto *refType = type->getAs<ReferenceType>())
    return getLocationCount(refType->getPointeeType());

  // Pointer types
  if (const auto *ptrType = type->getAs<PointerType>())
    return getLocationCount(ptrType->getPointeeType());

  // Scalar types
  QualType ty = {};
  const bool isScalar = isScalarType(type, &ty);
  assert(isScalar);
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
    // min16int (short), ushort, min12int, half, and min10float are treated as
    // 16-bit if '-enable-16bit-types' option is enabled. They are treated as
    // 32-bit otherwise.
    case BuiltinType::Short:
    case BuiltinType::UShort:
    case BuiltinType::Min12Int:
    case BuiltinType::Half:
    case BuiltinType::Min10Float: {
      return spirvOptions.enable16BitTypes ? 16 : 32;
    }
    case BuiltinType::LitFloat: {
      // First try to see if there are any hints about how this literal type
      // is going to be used. If so, use the hint.
      if (getIntendedLiteralType(ty) != ty) {
        return getElementSpirvBitwidth(getIntendedLiteralType(ty));
      }

      const auto &semantics = astContext.getFloatTypeSemantics(type);
      const auto bitwidth = llvm::APFloat::getSizeInBits(semantics);
      return bitwidth <= 32 ? 32 : 64;
    }
    case BuiltinType::LitInt: {
      // First try to see if there are any hints about how this literal type
      // is going to be used. If so, use the hint.
      if (getIntendedLiteralType(ty) != ty) {
        return getElementSpirvBitwidth(getIntendedLiteralType(ty));
      }

      const auto bitwidth = astContext.getIntWidth(type);
      // All integer variants with bitwidth larger than 32 are represented
      // as 64-bit int in SPIR-V.
      // All integer variants with bitwidth of 32 or less are represented as
      // 32-bit int in SPIR-V.
      return bitwidth > 32 ? 64 : 32;
    }
    }
  }
  llvm_unreachable("invalid type passed to getElementSpirvBitwidth");
}

uint32_t TypeTranslator::translateType(QualType type, LayoutRule rule) {
  const auto desugaredType = desugarType(type);
  if (desugaredType != type) {
    const auto id = translateType(desugaredType, rule);
    // Clear potentially set matrix majorness info
    typeMatMajorAttr = llvm::None;
    return id;
  }

  // Primitive types
  {
    QualType ty = {};
    if (isScalarType(type, &ty)) {
      if (const auto *builtinType = ty->getAs<BuiltinType>()) {
        switch (builtinType->getKind()) {
        case BuiltinType::Void:
          return theBuilder.getVoidType();
        case BuiltinType::Bool: {
          // According to the SPIR-V Spec: There is no physical size or bit
          // pattern defined for boolean type. Therefore an unsigned integer is
          // used to represent booleans when layout is required.
          if (rule == LayoutRule::Void)
            return theBuilder.getBoolType();
          else
            return theBuilder.getUint32Type();
        }
        // All the ints
        case BuiltinType::Int:
        case BuiltinType::UInt:
        case BuiltinType::Short:
        case BuiltinType::Min12Int:
        case BuiltinType::UShort:
        case BuiltinType::LongLong:
        case BuiltinType::ULongLong:
        // All the floats
        case BuiltinType::Float:
        case BuiltinType::Double:
        case BuiltinType::Half:
        case BuiltinType::Min10Float: {
          const auto bitwidth = getElementSpirvBitwidth(ty);
          return getTypeWithCustomBitwidth(ty, bitwidth);
        }
        // Literal types. First try to resolve them using hints.
        case BuiltinType::LitInt:
        case BuiltinType::LitFloat: {
          if (getIntendedLiteralType(ty) != ty) {
            return translateType(getIntendedLiteralType(ty));
          }
          const auto bitwidth = getElementSpirvBitwidth(ty);
          return getTypeWithCustomBitwidth(ty, bitwidth);
        }
        default:
          emitError("primitive type %0 unimplemented")
              << builtinType->getTypeClassName();
          return 0;
        }
      }
    }
  }

  // Reference types
  if (const auto *refType = type->getAs<ReferenceType>()) {
    // Note: Pointer/reference types are disallowed in HLSL source code.
    // Although developers cannot use them directly, they are generated into
    // the AST by out/inout parameter modifiers in function signatures.
    // We already pass function arguments via pointers to tempoary local
    // variables. So it should be fine to drop the pointer type and treat it
    // as the underlying pointee type here.
    return translateType(refType->getPointeeType(), rule);
  }

  // Pointer types
  if (const auto *ptrType = type->getAs<PointerType>()) {
    // The this object in a struct member function is of pointer type.
    return translateType(ptrType->getPointeeType(), rule);
  }

  // In AST, vector/matrix types are TypedefType of TemplateSpecializationType.
  // We handle them via HLSL type inspection functions.

  // Vector types
  {
    QualType elemType = {};
    uint32_t elemCount = {};
    if (isVectorType(type, &elemType, &elemCount))
      return theBuilder.getVecType(translateType(elemType, rule), elemCount);
  }

  // Matrix types
  {
    QualType elemType = {};
    uint32_t rowCount = 0, colCount = 0;
    if (isMxNMatrix(type, &elemType, &rowCount, &colCount)) {
      // HLSL matrices are row major, while SPIR-V matrices are column major.
      // We are mapping what HLSL semantically mean a row into a column here.
      const uint32_t vecType =
          theBuilder.getVecType(translateType(elemType, rule), colCount);

      // If the matrix element type is not float, it is represented as an array
      // of vectors, and should therefore have the ArrayStride decoration.
      llvm::SmallVector<const Decoration *, 4> decorations;
      if (!elemType->isFloatingType() && rule != LayoutRule::Void) {
        uint32_t stride = 0;
        (void)getAlignmentAndSize(type, rule, &stride);
        decorations.push_back(
            Decoration::getArrayStride(*theBuilder.getSPIRVContext(), stride));
      }

      return theBuilder.getMatType(elemType, vecType, rowCount, decorations);
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

    // If this struct is derived from some other struct, place an implicit field
    // at the very beginning for the base struct.
    if (const auto *cxxDecl = dyn_cast<CXXRecordDecl>(decl))
      for (const auto base : cxxDecl->bases()) {
        fieldTypes.push_back(translateType(base.getType(), rule));
        fieldNames.push_back("");
      }

    // Create fields for all members of this struct
    for (const auto *field : decl->fields()) {
      fieldTypes.push_back(translateType(field->getType(), rule));
      fieldNames.push_back(field->getName());
    }

    llvm::SmallVector<const Decoration *, 4> decorations;
    if (rule != LayoutRule::Void) {
      decorations = getLayoutDecorations(collectDeclsInDeclContext(decl), rule);
    }

    return theBuilder.getStructType(fieldTypes, decl->getName(), fieldNames,
                                    decorations);
  }

  // Array type
  if (const auto *arrayType = astContext.getAsArrayType(type)) {
    const auto elemType = arrayType->getElementType();
    const uint32_t elemTypeId = translateType(elemType, rule);

    llvm::SmallVector<const Decoration *, 4> decorations;
    if (rule != LayoutRule::Void &&
        // We won't have stride information for structured/byte buffers since
        // they contain runtime arrays.
        !isAKindOfStructuredOrByteBuffer(elemType)) {
      uint32_t stride = 0;
      (void)getAlignmentAndSize(type, rule, &stride);
      decorations.push_back(
          Decoration::getArrayStride(*theBuilder.getSPIRVContext(), stride));
    }

    if (const auto *caType = astContext.getAsConstantArrayType(type)) {
      const auto size = static_cast<uint32_t>(caType->getSize().getZExtValue());
      return theBuilder.getArrayType(
          elemTypeId, theBuilder.getConstantUint32(size), decorations);
    } else {
      assert(type->isIncompleteArrayType());
      // Runtime arrays of resources needs additional capability.
      if (hlsl::IsHLSLResourceType(arrayType->getElementType())) {
        theBuilder.addExtension(Extension::EXT_descriptor_indexing,
                                "runtime array of resources", {});
        theBuilder.requireCapability(
            spv::Capability::RuntimeDescriptorArrayEXT);
      }
      return theBuilder.getRuntimeArrayType(elemTypeId, decorations);
    }
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

bool TypeTranslator::isOrContainsAKindOfStructuredOrByteBuffer(QualType type) {
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

bool TypeTranslator::isOrContains16BitType(QualType type) {
  // Primitive types
  {
    QualType ty = {};
    if (isScalarType(type, &ty)) {
      if (const auto *builtinType = ty->getAs<BuiltinType>()) {
        switch (builtinType->getKind()) {
        case BuiltinType::Short:
        case BuiltinType::UShort:
        case BuiltinType::Min12Int:
        case BuiltinType::Half:
        case BuiltinType::Min10Float: {
          return spirvOptions.enable16BitTypes;
        }
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
      return isOrContains16BitType(elemType);
  }

  // Matrix types
  {
    QualType elemType = {};
    if (isMxNMatrix(type, &elemType)) {
      return isOrContains16BitType(elemType);
    }
  }

  // Struct type
  if (const auto *structType = type->getAs<RecordType>()) {
    const auto *decl = structType->getDecl();

    for (const auto *field : decl->fields()) {
      if (isOrContains16BitType(field->getType()))
        return true;
    }

    return false;
  }

  // Array type
  if (const auto *arrayType = type->getAsArrayTypeUnsafe()) {
    return isOrContains16BitType(arrayType->getElementType());
  }

  // Reference types
  if (const auto *refType = type->getAs<ReferenceType>()) {
    return isOrContains16BitType(refType->getPointeeType());
  }

  // Pointer types
  if (const auto *ptrType = type->getAs<PointerType>()) {
    return isOrContains16BitType(ptrType->getPointeeType());
  }

  if (const auto *typedefType = type->getAs<TypedefType>()) {
    return isOrContains16BitType(typedefType->desugar());
  }

  emitError("checking 16-bit type for %0 unimplemented")
      << type->getTypeClassName();
  type->dump();
  return 0;
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

bool TypeTranslator::isSubpassInput(QualType type) {
  if (const auto *rt = type->getAs<RecordType>())
    return rt->getDecl()->getName() == "SubpassInput";

  return false;
}

bool TypeTranslator::isSubpassInputMS(QualType type) {
  if (const auto *rt = type->getAs<RecordType>())
    return rt->getDecl()->getName() == "SubpassInputMS";

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

bool TypeTranslator::isOrContainsNonFpColMajorMatrix(QualType type,
                                                     const Decl *decl) const {
  const auto isColMajorDecl = [this](const Decl *decl) {
    return decl->hasAttr<HLSLColumnMajorAttr>() ||
           (!decl->hasAttr<HLSLRowMajorAttr>() && !spirvOptions.defaultRowMajor);
  };

  QualType elemType = {};
  if (isMxNMatrix(type, &elemType) && !elemType->isFloatingType()) {
    return isColMajorDecl(decl);
  }

  if (const auto *arrayType = astContext.getAsConstantArrayType(type)) {
    if (isMxNMatrix(arrayType->getElementType(), &elemType) &&
        !elemType->isFloatingType())
      return isColMajorDecl(decl);
  }

  if (const auto *structType = type->getAs<RecordType>()) {
    const auto *decl = structType->getDecl();
    for (const auto *field : decl->fields()) {
      if (isOrContainsNonFpColMajorMatrix(field->getType(), field))
        return true;
    }
  }

  return false;
}

bool TypeTranslator::isConstantTextureBuffer(const Decl *decl) {
  if (const auto *bufferDecl = dyn_cast<HLSLBufferDecl>(decl->getDeclContext()))
    // Make sure we are not returning true for VarDecls inside cbuffer/tbuffer.
    return bufferDecl->isConstantBufferView();

  return false;
}

bool TypeTranslator::isResourceType(const ValueDecl *decl) {
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

bool TypeTranslator::isRowMajorMatrix(QualType type) const {
  // The type passed in may not be desugared. Check attributes on itself first.
  bool attrRowMajor = false;
  if (hasHLSLMatOrientation(type, &attrRowMajor))
    return attrRowMajor;

  // Use the majorness info we recorded before.
  if (typeMatMajorAttr.hasValue()) {
    switch (typeMatMajorAttr.getValue()) {
    case AttributedType::attr_hlsl_row_major:
      return true;
    case AttributedType::attr_hlsl_column_major:
      return false;
    }
  }

  return spirvOptions.defaultRowMajor;
}

bool TypeTranslator::canTreatAsSameScalarType(QualType type1, QualType type2) {
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

bool TypeTranslator::isSameScalarOrVecType(QualType type1, QualType type2) {
  { // Scalar types
    QualType scalarType1 = {}, scalarType2 = {};
    if (TypeTranslator::isScalarType(type1, &scalarType1) &&
        TypeTranslator::isScalarType(type2, &scalarType2))
      return canTreatAsSameScalarType(scalarType1, scalarType2);
  }

  { // Vector types
    QualType elemType1 = {}, elemType2 = {};
    uint32_t count1 = {}, count2 = {};
    if (TypeTranslator::isVectorType(type1, &elemType1, &count1) &&
        TypeTranslator::isVectorType(type2, &elemType2, &count2))
      return count1 == count2 && canTreatAsSameScalarType(elemType1, elemType2);
  }

  return false;
}

bool TypeTranslator::isSameType(QualType type1, QualType type2) {
  if (isSameScalarOrVecType(type1, type2))
    return true;

  type1.removeLocalConst();
  type2.removeLocalConst();

  { // Matrix types
    QualType elemType1 = {}, elemType2 = {};
    uint32_t row1 = 0, row2 = 0, col1 = 0, col2 = 0;
    if (TypeTranslator::isMxNMatrix(type1, &elemType1, &row1, &col1) &&
        TypeTranslator::isMxNMatrix(type2, &elemType2, &row2, &col2))
      return row1 == row2 && col1 == col2 &&
             canTreatAsSameScalarType(elemType1, elemType2);
  }

  { // Array types
    if (const auto *arrType1 = astContext.getAsConstantArrayType(type1))
      if (const auto *arrType2 = astContext.getAsConstantArrayType(type2))
        return hlsl::GetArraySize(type1) == hlsl::GetArraySize(type2) &&
               isSameType(arrType1->getElementType(),
                          arrType2->getElementType());
  }

  // TODO: support other types if needed

  return false;
}

QualType TypeTranslator::getElementType(QualType type) {
  QualType elemType = {};
  if (isScalarType(type, &elemType) || isVectorType(type, &elemType) ||
      isMxNMatrix(type, &elemType)) {
  } else if (const auto *arrType = astContext.getAsConstantArrayType(type)) {
    elemType = arrType->getElementType();
  } else {
    assert(false && "unhandled type");
  }
  return elemType;
}

uint32_t TypeTranslator::getComponentVectorType(QualType matrixType) {
  assert(isMxNMatrix(matrixType));

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

bool TypeTranslator::shouldSkipInStructLayout(const Decl *decl) {
  // Ignore implicit generated struct declarations/constructors/destructors
  if (decl->isImplicit())
    return true;
  // Ignore embedded type decls
  if (isa<TypeDecl>(decl))
    return true;
  // Ignore embeded function decls
  if (isa<FunctionDecl>(decl))
    return true;
  // Ignore empty decls
  if (isa<EmptyDecl>(decl))
    return true;

  // For the $Globals cbuffer, we only care about externally-visiable
  // non-resource-type variables. The rest should be filtered out.

  const auto *declContext = decl->getDeclContext();

  // Special check for ConstantBuffer/TextureBuffer, whose DeclContext is a
  // HLSLBufferDecl. So that we need to check the HLSLBufferDecl's parent decl
  // to check whether this is a ConstantBuffer/TextureBuffer defined in the
  // global namespace.
  // Note that we should not be seeing ConstantBuffer/TextureBuffer for normal
  // cbuffer/tbuffer or push constant blocks. So this case should only happen
  // for $Globals cbuffer.
  if (isConstantTextureBuffer(decl) &&
      declContext->getLexicalParent()->isTranslationUnit())
    return true;

  // $Globals' "struct" is the TranslationUnit, so we should ignore resources
  // in the TranslationUnit "struct" and its child namespaces.
  if (declContext->isTranslationUnit() || declContext->isNamespace()) {
    // External visibility
    if (const auto *declDecl = dyn_cast<DeclaratorDecl>(decl))
      if (!declDecl->hasExternalFormalLinkage())
        return true;

    // cbuffer/tbuffer
    if (isa<HLSLBufferDecl>(decl))
      return true;

    // Other resource types
    if (const auto *valueDecl = dyn_cast<ValueDecl>(decl))
      if (isResourceType(valueDecl))
        return true;
  }

  return false;
}

llvm::SmallVector<const Decoration *, 4> TypeTranslator::getLayoutDecorations(
    const llvm::SmallVector<const Decl *, 4> &decls, LayoutRule rule) {
  const auto spirvContext = theBuilder.getSPIRVContext();
  llvm::SmallVector<const Decoration *, 4> decorations;
  uint32_t offset = 0, index = 0;
  for (const auto *decl : decls) {
    // The field can only be FieldDecl (for normal structs) or VarDecl (for
    // HLSLBufferDecls).
    const auto *declDecl = cast<DeclaratorDecl>(decl);
    auto fieldType = declDecl->getType();

    uint32_t memberAlignment = 0, memberSize = 0, stride = 0;
    std::tie(memberAlignment, memberSize) =
        getAlignmentAndSize(fieldType, rule, &stride);

    // The next avaiable location after layouting the previos members
    const uint32_t nextLoc = offset;

    if (rule == LayoutRule::RelaxedGLSLStd140 ||
        rule == LayoutRule::RelaxedGLSLStd430 ||
        rule == LayoutRule::FxcCTBuffer)
      alignUsingHLSLRelaxedLayout(fieldType, memberSize, &memberAlignment,
                                  &offset);
    else
      offset = roundToPow2(offset, memberAlignment);

    // The vk::offset attribute takes precedence over all.
    if (const auto *offsetAttr = decl->getAttr<VKOffsetAttr>()) {
      offset = offsetAttr->getOffset();
    }
    // The :packoffset() annotation takes precedence over normal layout
    // calculation.
    else if (const auto *pack = getPackOffset(declDecl)) {
      const uint32_t packOffset =
          pack->Subcomponent * 16 + pack->ComponentOffset * 4;
      // Do minimal check to make sure the offset specified by packoffset does
      // not cause overlap.
      if (packOffset < nextLoc) {
        emitError("packoffset caused overlap with previous members", pack->Loc);
      } else {
        offset = packOffset;
      }
    }

    // Each structure-type member must have an Offset Decoration.
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

    // Non-floating point matrices are represented as arrays of vectors, and
    // therefore ColMajor and RowMajor decorations should not be applied to
    // them.
    QualType elemType = {};
    if (isMxNMatrix(fieldType, &elemType) && elemType->isFloatingType()) {
      memberAlignment = memberSize = stride = 0;
      std::tie(memberAlignment, memberSize) =
          getAlignmentAndSize(fieldType, rule, &stride);

      decorations.push_back(
          Decoration::getMatrixStride(*spirvContext, stride, index));

      // We need to swap the RowMajor and ColMajor decorations since HLSL
      // matrices are conceptually row-major while SPIR-V are conceptually
      // column-major.
      if (isRowMajorMatrix(fieldType)) {
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

void TypeTranslator::collectDeclsInNamespace(
    const NamespaceDecl *nsDecl, llvm::SmallVector<const Decl *, 4> *decls) {
  for (const auto *decl : nsDecl->decls()) {
    collectDeclsInField(decl, decls);
  }
}

void TypeTranslator::collectDeclsInField(
    const Decl *field, llvm::SmallVector<const Decl *, 4> *decls) {

  // Case of nested namespaces.
  if (const auto *nsDecl = dyn_cast<NamespaceDecl>(field)) {
    collectDeclsInNamespace(nsDecl, decls);
  }

  if (shouldSkipInStructLayout(field))
    return;

  if (!isa<DeclaratorDecl>(field)) {
    return;
  }

  decls->push_back(field);
}

llvm::SmallVector<const Decl *, 4>
TypeTranslator::collectDeclsInDeclContext(const DeclContext *declContext) {
  llvm::SmallVector<const Decl *, 4> decls;
  for (const auto *field : declContext->decls()) {
    collectDeclsInField(field, &decls);
  }
  return decls;
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
                                     dim, /*depth*/ 2, isArray, isMS);
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
                                     dim, /*depth*/ 2, isArray, /*MS*/ 0,
                                     /*Sampled*/ 2u, format);
    }
  }

  // Sampler types
  if (name == "SamplerState" || name == "SamplerComparisonState") {
    return theBuilder.getSamplerType();
  }

  if (name == "StructuredBuffer" || name == "RWStructuredBuffer" ||
      name == "AppendStructuredBuffer" || name == "ConsumeStructuredBuffer") {
    // StructureBuffer<S> will be translated into an OpTypeStruct with one
    // field, which is an OpTypeRuntimeArray of OpTypeStruct (S).

    // If layout rule is void, it means these resource types are used for
    // declaring local resources, which should be created as alias variables.
    // The aliased-to variable should surely be in the Uniform storage class,
    // which has layout decorations.
    bool asAlias = false;
    if (rule == LayoutRule::Void) {
      asAlias = true;
      rule = spirvOptions.sBufferLayoutRule;
    }

    auto &context = *theBuilder.getSPIRVContext();
    const auto s = hlsl::GetHLSLResourceResultType(type);
    const uint32_t structType = translateType(s, rule);
    std::string structName;
    const auto innerType = hlsl::GetHLSLResourceResultType(type);
    if (innerType->isStructureType())
      structName = innerType->getAs<RecordType>()->getDecl()->getName();
    else
      structName = getName(innerType);

    const bool isRowMajor = isRowMajorMatrix(s);
    llvm::SmallVector<const Decoration *, 4> decorations;

    // The stride for the runtime array is the size of S.
    uint32_t size = 0, stride = 0;
    std::tie(std::ignore, size) = getAlignmentAndSize(s, rule, &stride);
    decorations.push_back(Decoration::getArrayStride(context, size));
    const uint32_t raType =
        theBuilder.getRuntimeArrayType(structType, decorations);

    decorations.clear();

    // Attach majorness decoration if this is a *StructuredBuffer<matrix>.
    if (TypeTranslator::isMxNMatrix(s))
      decorations.push_back(isRowMajor ? Decoration::getColMajor(context, 0)
                                       : Decoration::getRowMajor(context, 0));

    decorations.push_back(Decoration::getOffset(context, 0, 0));
    if (name == "StructuredBuffer")
      decorations.push_back(Decoration::getNonWritable(context, 0));
    decorations.push_back(Decoration::getBufferBlock(context));
    const std::string typeName = "type." + name.str() + "." + structName;
    const auto valType =
        theBuilder.getStructType(raType, typeName, {}, decorations);

    if (asAlias) {
      // All structured buffers are in the Uniform storage class.
      return theBuilder.getPointerType(valType, spv::StorageClass::Uniform);
    } else {
      return valType;
    }
  }

  // ByteAddressBuffer types.
  if (name == "ByteAddressBuffer") {
    const auto bufferType = theBuilder.getByteAddressBufferType(/*isRW*/ false);
    if (rule == LayoutRule::Void) {
      // All byte address buffers are in the Uniform storage class.
      return theBuilder.getPointerType(bufferType, spv::StorageClass::Uniform);
    } else {
      return bufferType;
    }
  }
  // RWByteAddressBuffer types.
  if (name == "RWByteAddressBuffer") {
    const auto bufferType = theBuilder.getByteAddressBufferType(/*isRW*/ true);
    if (rule == LayoutRule::Void) {
      // All byte address buffers are in the Uniform storage class.
      return theBuilder.getPointerType(bufferType, spv::StorageClass::Uniform);
    } else {
      return bufferType;
    }
  }

  // Buffer and RWBuffer types
  if (name == "Buffer" || name == "RWBuffer") {
    theBuilder.requireCapability(spv::Capability::SampledBuffer);
    const auto sampledType = hlsl::GetHLSLResourceResultType(type);
    const auto format = translateSampledTypeToImageFormat(sampledType);
    return theBuilder.getImageType(
        translateType(getElementType(sampledType)), spv::Dim::Buffer,
        /*depth*/ 2, /*isArray*/ 0, /*ms*/ 0,
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

  if (name == "SubpassInput" || name == "SubpassInputMS") {
    const auto sampledType = hlsl::GetHLSLResourceResultType(type);
    return theBuilder.getImageType(
        translateType(getElementType(sampledType)), spv::Dim::SubpassData,
        /*depth*/ 2, /*isArray*/ false, /*ms*/ name == "SubpassInputMS",
        /*sampled*/ 2);
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

void TypeTranslator::alignUsingHLSLRelaxedLayout(QualType fieldType,
                                                 uint32_t fieldSize,
                                                 uint32_t *fieldAlignment,
                                                 uint32_t *currentOffset) {
  QualType vecElemType = {};
  const bool fieldIsVecType = isVectorType(fieldType, &vecElemType);

  // Adjust according to HLSL relaxed layout rules.
  // Aligning vectors as their element types so that we can pack a float
  // and a float3 tightly together.
  if (fieldIsVecType) {
    uint32_t scalarAlignment = 0;
    std::tie(scalarAlignment, std::ignore) =
        getAlignmentAndSize(vecElemType, LayoutRule::Void, nullptr);
    if (scalarAlignment <= 4)
      *fieldAlignment = scalarAlignment;
  }

  *currentOffset = roundToPow2(*currentOffset, *fieldAlignment);

  // Adjust according to HLSL relaxed layout rules.
  // Bump to 4-component vector alignment if there is a bad straddle
  if (fieldIsVecType &&
      improperStraddle(fieldType, fieldSize, *currentOffset)) {
    *fieldAlignment = kStd140Vec4Alignment;
    *currentOffset = roundToPow2(*currentOffset, *fieldAlignment);
  }
}

std::pair<uint32_t, uint32_t>
TypeTranslator::getAlignmentAndSize(QualType type, LayoutRule rule,
                                    uint32_t *stride) {
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
  //    with C components each, according to rule (4).
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
  //
  // This method supports multiple layout rules, all of them modifying the
  // std140 rules listed above:
  //
  // std430:
  // - Array base alignment and stride does not need to be rounded up to a
  //   multiple of 16.
  // - Struct base alignment does not need to be rounded up to a multiple of 16.
  //
  // Relaxed std140/std430:
  // - Vector base alignment is set as its element type's base alignment.
  //
  // FxcCTBuffer:
  // - Vector base alignment is set as its element type's base alignment.
  // - Arrays/structs do not need to have padding at the end; arrays/structs do
  //   not affect the base offset of the member following them.
  // - Struct base alignment does not need to be rounded up to a multiple of 16.
  //
  // FxcSBuffer:
  // - Vector/matrix/array base alignment is set as its element type's base
  //   alignment.
  // - Arrays/structs do not need to have padding at the end; arrays/structs do
  //   not affect the base offset of the member following them.
  // - Struct base alignment does not need to be rounded up to a multiple of 16.

  const auto desugaredType = desugarType(type);
  if (desugaredType != type) {
    const auto id = getAlignmentAndSize(desugaredType, rule, stride);
    // Clear potentially set matrix majorness info
    typeMatMajorAttr = llvm::None;
    return id;
  }

  { // Rule 1
    QualType ty = {};
    if (isScalarType(type, &ty))
      if (const auto *builtinType = ty->getAs<BuiltinType>())
        switch (builtinType->getKind()) {
        case BuiltinType::Bool:
        case BuiltinType::Int:
        case BuiltinType::UInt:
        case BuiltinType::Float:
          return {4, 4};
        case BuiltinType::Double:
        case BuiltinType::LongLong:
        case BuiltinType::ULongLong:
          return {8, 8};
        case BuiltinType::Short:
        case BuiltinType::UShort:
        case BuiltinType::Min12Int:
        case BuiltinType::Half:
        case BuiltinType::Min10Float: {
          if (spirvOptions.enable16BitTypes)
            return {2, 2};
          else
            return {4, 4};
        }
        default:
          emitError("alignment and size calculation for type %0 unimplemented")
              << type;
          return {0, 0};
        }
  }

  { // Rule 2 and 3
    QualType elemType = {};
    uint32_t elemCount = {};
    if (isVectorType(type, &elemType, &elemCount)) {
      uint32_t alignment = 0, size = 0;
      std::tie(alignment, size) = getAlignmentAndSize(elemType, rule, stride);
      // Use element alignment for fxc rules
      if (rule != LayoutRule::FxcCTBuffer && rule != LayoutRule::FxcSBuffer)
        alignment = (elemCount == 3 ? 4 : elemCount) * size;

      return {alignment, elemCount * size};
    }
  }

  { // Rule 5 and 7
    QualType elemType = {};
    uint32_t rowCount = 0, colCount = 0;
    if (isMxNMatrix(type, &elemType, &rowCount, &colCount)) {
      uint32_t alignment = 0, size = 0;
      std::tie(alignment, size) = getAlignmentAndSize(elemType, rule, stride);

      // Matrices are treated as arrays of vectors:
      // The base alignment and array stride are set to match the base alignment
      // of a single array element, according to rules 1, 2, and 3, and rounded
      // up to the base alignment of a vec4.
      bool isRowMajor = isRowMajorMatrix(type);

      const uint32_t vecStorageSize = isRowMajor ? colCount : rowCount;

      if (rule == LayoutRule::FxcSBuffer) {
        *stride = vecStorageSize * size;
        // Use element alignment for fxc structured buffers
        return {alignment, rowCount * colCount * size};
      }

      alignment *= (vecStorageSize == 3 ? 4 : vecStorageSize);
      if (rule == LayoutRule::GLSLStd140 ||
          rule == LayoutRule::RelaxedGLSLStd140 ||
          rule == LayoutRule::FxcCTBuffer) {
        alignment = roundToPow2(alignment, kStd140Vec4Alignment);
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
      std::tie(memberAlignment, memberSize) =
          getAlignmentAndSize(field->getType(), rule, stride);

      if (rule == LayoutRule::RelaxedGLSLStd140 ||
          rule == LayoutRule::RelaxedGLSLStd430 ||
          rule == LayoutRule::FxcCTBuffer)
        alignUsingHLSLRelaxedLayout(field->getType(), memberSize,
                                    &memberAlignment, &structSize);
      else
        structSize = roundToPow2(structSize, memberAlignment);

      // The base alignment of the structure is N, where N is the largest
      // base alignment value of any of its members...
      maxAlignment = std::max(maxAlignment, memberAlignment);
      structSize += memberSize;
    }

    if (rule == LayoutRule::GLSLStd140 ||
        rule == LayoutRule::RelaxedGLSLStd140) {
      // ... and rounded up to the base alignment of a vec4.
      maxAlignment = roundToPow2(maxAlignment, kStd140Vec4Alignment);
    }

    if (rule != LayoutRule::FxcCTBuffer && rule != LayoutRule::FxcSBuffer) {
      // The base offset of the member following the sub-structure is rounded up
      // to the next multiple of the base alignment of the structure.
      structSize = roundToPow2(structSize, maxAlignment);
    }
    return {maxAlignment, structSize};
  }

  // Rule 4, 6, 8, and 10
  if (const auto *arrayType = astContext.getAsConstantArrayType(type)) {
    const auto elemCount = arrayType->getSize().getZExtValue();
    uint32_t alignment = 0, size = 0;
    std::tie(alignment, size) =
        getAlignmentAndSize(arrayType->getElementType(), rule, stride);

    if (rule == LayoutRule::FxcSBuffer) {
      *stride = size;
      // Use element alignment for fxc structured buffers
      return {alignment, size * elemCount};
    }

    if (rule == LayoutRule::GLSLStd140 ||
        rule == LayoutRule::RelaxedGLSLStd140 ||
        rule == LayoutRule::FxcCTBuffer) {
      // The base alignment and array stride are set to match the base alignment
      // of a single array element, according to rules 1, 2, and 3, and rounded
      // up to the base alignment of a vec4.
      alignment = roundToPow2(alignment, kStd140Vec4Alignment);
    }
    if (rule == LayoutRule::FxcCTBuffer) {
      // In fxc cbuffer/tbuffer packing rules, arrays does not affect the data
      // packing after it. But we still need to make sure paddings are inserted
      // internally if necessary.
      *stride = roundToPow2(size, alignment);
      size += *stride * (elemCount - 1);
    } else {
      // Need to round size up considering stride for scalar types
      size = roundToPow2(size, alignment);
      *stride = size; // Use size instead of alignment here for Rule 10
      size *= elemCount;
      // The base offset of the member following the array is rounded up to the
      // next multiple of the base alignment.
      size = roundToPow2(size, alignment);
    }

    return {alignment, size};
  }

  emitError("alignment and size calculation for type %0 unimplemented") << type;
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
          return "half";
        case BuiltinType::Min12Int:
          return "min12int";
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

QualType TypeTranslator::desugarType(QualType type) {
  if (const auto *attrType = type->getAs<AttributedType>()) {
    switch (auto kind = attrType->getAttrKind()) {
    case AttributedType::attr_hlsl_row_major:
    case AttributedType::attr_hlsl_column_major:
      typeMatMajorAttr = kind;
    }
    return desugarType(
        attrType->getLocallyUnqualifiedSingleStepDesugaredType());
  }

  if (const auto *typedefType = type->getAs<TypedefType>()) {
    return desugarType(typedefType->desugar());
  }

  return type;
}

} // end namespace spirv
} // end namespace clang
