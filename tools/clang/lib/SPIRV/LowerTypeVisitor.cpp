//===--- LowerTypeVisitor.cpp - AST type to SPIR-V type impl -----*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/SPIRV/LowerTypeVisitor.h"
#include "clang/AST/Attr.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/HlslTypes.h"
#include "clang/SPIRV/AstTypeProbe.h"
#include "clang/SPIRV/SpirvFunction.h"

namespace {
/// Returns the :packoffset() annotation on the given decl. Returns nullptr if
/// the decl does not have one.
hlsl::ConstantPacking *getPackOffset(const clang::NamedDecl *decl) {
  for (auto *annotation : decl->getUnusualAnnotations())
    if (auto *packing = llvm::dyn_cast<hlsl::ConstantPacking>(annotation))
      return packing;
  return nullptr;
}

/// The alignment for 4-component float vectors.
constexpr uint32_t kStd140Vec4Alignment = 16u;

/// Rounds the given value up to the given power of 2.
inline uint32_t roundToPow2(uint32_t val, uint32_t pow2) {
  assert(pow2 != 0);
  return (val + pow2 - 1) & ~(pow2 - 1);
}

/// Returns true if the given vector type (of the given size) crosses the
/// 4-component vector boundary if placed at the given offset.
bool improperStraddle(clang::QualType type, int size, int offset) {
  assert(clang::spirv::isVectorType(type));
  return size <= 16 ? offset / 16 != (offset + size - 1) / 16
                    : offset % 16 != 0;
}

} // end anonymous namespace

namespace clang {
namespace spirv {

bool LowerTypeVisitor::visit(SpirvFunction *fn, Phase phase) {
  if (phase == Visitor::Phase::Init) {
    // Lower the function return type.
    const SpirvType *spirvReturnType =
        lowerType(fn->getAstReturnType(), SpirvLayoutRule::Void,
                  /*SourceLocation*/ {});
    fn->setReturnType(const_cast<SpirvType *>(spirvReturnType));

    // Lower the SPIR-V function type if necessary.
    fn->setFunctionType(const_cast<SpirvType *>(
        lowerType(fn->getFunctionType(), SpirvLayoutRule::Void,
                  fn->getSourceLocation())));
  }
  return true;
}

bool LowerTypeVisitor::visitInstruction(SpirvInstruction *instr) {
  const QualType astType = instr->getAstResultType();
  const SpirvType *hybridType = instr->getResultType();

  // Lower QualType to SpirvType
  if (astType != QualType({})) {
    const SpirvType *spirvType =
        lowerType(astType, instr->getLayoutRule(), instr->getSourceLocation());
    instr->setResultType(spirvType);
  }
  // Lower Hybrid type to SpirvType
  else if (hybridType) {
    const SpirvType *spirvType = lowerType(hybridType, instr->getLayoutRule(),
                                           instr->getSourceLocation());
    instr->setResultType(spirvType);
  }

  // Instruction-specific type updates

  const auto *resultType = instr->getResultType();
  switch (instr->getopcode()) {
  case spv::Op::OpSampledImage: {
    // Wrap the image type in sampled image type if necessary.
    if (!isa<SampledImageType>(resultType)) {
      assert(isa<ImageType>(resultType));
      instr->setResultType(
          spvContext.getSampledImageType(cast<ImageType>(resultType)));
    }
    break;
  }
  // Variables must have a pointer type.
  case spv::Op::OpVariable: {
    const SpirvType *pointerType =
        spvContext.getPointerType(resultType, instr->getStorageClass());
    instr->setResultType(pointerType);
    break;
  }
  // OpImageTexelPointer's result type must be a pointer with image storage
  // class.
  case spv::Op::OpImageTexelPointer: {
    const SpirvType *pointerType =
        spvContext.getPointerType(resultType, spv::StorageClass::Image);
    instr->setResultType(pointerType);
    break;
  }
  // Sparse image operations return a sparse residency struct.
  case spv::Op::OpImageSparseSampleImplicitLod:
  case spv::Op::OpImageSparseSampleExplicitLod:
  case spv::Op::OpImageSparseSampleDrefImplicitLod:
  case spv::Op::OpImageSparseSampleDrefExplicitLod:
  case spv::Op::OpImageSparseFetch:
  case spv::Op::OpImageSparseGather:
  case spv::Op::OpImageSparseDrefGather:
  case spv::Op::OpImageSparseRead: {
    const auto *uintType = spvContext.getUIntType(32);
    const auto *sparseResidencyStruct = spvContext.getStructType(
        {StructType::FieldInfo(uintType, "Residency.Code"),
         StructType::FieldInfo(resultType, "Result.Type")},
        "SparseResidencyStruct");
    instr->setResultType(sparseResidencyStruct);
    break;
  }
  default:
    break;
  }

  // The instruction does not have a result-type, so nothing to do.
  return true;
}

const SpirvType *LowerTypeVisitor::lowerType(const SpirvType *type,
                                             SpirvLayoutRule rule,
                                             SourceLocation loc) {
  if (const auto *hybridPointer = dyn_cast<HybridPointerType>(type)) {
    const QualType pointeeType = hybridPointer->getPointeeType();
    const SpirvType *pointeeSpirvType = lowerType(pointeeType, rule, loc);
    return spvContext.getPointerType(pointeeSpirvType,
                                     hybridPointer->getStorageClass());
  } else if (const auto *hybridSampledImage =
                 dyn_cast<HybridSampledImageType>(type)) {
    const QualType imageAstType = hybridSampledImage->getImageType();
    const SpirvType *imageSpirvType = lowerType(imageAstType, rule, loc);
    assert(isa<ImageType>(imageSpirvType));
    return spvContext.getSampledImageType(cast<ImageType>(imageSpirvType));
  } else if (const auto *hybridFn = dyn_cast<HybridFunctionType>(type)) {
    // Lower the return type.
    const QualType astReturnType = hybridFn->getAstReturnType();
    const SpirvType *spirvReturnType = lowerType(astReturnType, rule, loc);

    // Go over all params. If any of them is hybrid, lower it.
    std::vector<const SpirvType *> paramTypes;
    for (auto *paramType : hybridFn->getParamTypes()) {
      if (const auto *hybridParam = dyn_cast<HybridType>(paramType)) {
        paramTypes.push_back(lowerType(hybridParam, rule, loc));
      } else {
        paramTypes.push_back(paramType);
      }
    }

    return spvContext.getFunctionType(spirvReturnType, paramTypes);
  } else if (const auto *hybridStruct = dyn_cast<HybridStructType>(type)) {
    // lower all fields of the struct.
    auto loweredFields =
        populateLayoutInformation(hybridStruct->getFields(), rule);
    return spvContext.getStructType(
        loweredFields, hybridStruct->getStructName(),
        hybridStruct->isReadOnly(), hybridStruct->getInterfaceType());
  }
  // Void, bool, int, float cannot be further lowered.
  // Matrices cannot contain hybrid types. Only matrices of scalars are valid.
  // sampledType in image types can only be numberical type.
  // Sampler types cannot be further lowered.
  // SampledImage types cannot be further lowered.
  else if (isa<VoidType>(type) || isa<ScalarType>(type) ||
           isa<MatrixType>(type) || isa<ImageType>(type) ||
           isa<SamplerType>(type) || isa<SampledImageType>(type)) {
    return type;
  }
  // Vectors could contain a hybrid type
  else if (const auto *vecType = dyn_cast<VectorType>(type)) {
    const auto *loweredElemType =
        lowerType(vecType->getElementType(), rule, loc);
    // If vector didn't contain any hybrid types, return itself.
    if (vecType->getElementType() == loweredElemType)
      return vecType;
    return spvContext.getVectorType(loweredElemType,
                                    vecType->getElementCount());
  }
  // Arrays could contain a hybrid type
  else if (const auto *arrType = dyn_cast<ArrayType>(type)) {
    const auto *loweredElemType =
        lowerType(arrType->getElementType(), rule, loc);
    // If array didn't contain any hybrid types, return itself.
    if (arrType->getElementType() == loweredElemType)
      return arrType;

    return spvContext.getArrayType(loweredElemType, arrType->getElementCount(),
                                   arrType->getStride());
  }
  // Runtime arrays could contain a hybrid type
  else if (const auto *raType = dyn_cast<RuntimeArrayType>(type)) {
    const auto *loweredElemType =
        lowerType(raType->getElementType(), rule, loc);
    // If runtime array didn't contain any hybrid types, return itself.
    if (raType->getElementType() == loweredElemType)
      return raType;
    return spvContext.getRuntimeArrayType(loweredElemType, raType->getStride());
  }
  // Struct types could contain a hybrid type
  else if (const auto *structType = dyn_cast<StructType>(type)) {
    // Struct types can not contain hybrid types.
    return structType;
  }
  // Pointer types could point to a hybrid type.
  else if (const auto *ptrType = dyn_cast<SpirvPointerType>(type)) {
    const auto *loweredPointee =
        lowerType(ptrType->getPointeeType(), rule, loc);
    // If the pointer type didn't point to any hybrid type, return itself.
    if (ptrType->getPointeeType() == loweredPointee)
      return ptrType;

    return spvContext.getPointerType(loweredPointee,
                                     ptrType->getStorageClass());
  }
  // Function types may have a parameter or return type that is hybrid.
  else if (const auto *fnType = dyn_cast<FunctionType>(type)) {
    const auto *loweredRetType = lowerType(fnType->getReturnType(), rule, loc);
    bool wasLowered = fnType->getReturnType() != loweredRetType;
    llvm::SmallVector<const SpirvType *, 4> loweredParams;
    const auto &paramTypes = fnType->getParamTypes();
    for (auto *paramType : paramTypes) {
      const auto *loweredParamType = lowerType(paramType, rule, loc);
      loweredParams.push_back(loweredParamType);
      if (loweredParamType != paramType) {
        wasLowered = true;
      }
    }
    // If the function type didn't include any hybrid types, return itself.
    if (!wasLowered) {
      return fnType;
    }

    return spvContext.getFunctionType(loweredRetType, loweredParams);
  }

  llvm_unreachable("lowering of hybrid type not implemented");
}

const SpirvType *LowerTypeVisitor::lowerType(QualType type,
                                             SpirvLayoutRule rule,
                                             SourceLocation srcLoc) {
  const auto desugaredType = desugarType(type);

  if (desugaredType != type) {
    const auto *spvType = lowerType(desugaredType, rule, srcLoc);
    // Clear matrix majorness potentially set by previous desugarType() calls.
    // This field will only be set when we were saying a matrix type. And the
    // above lowerType() call already takes the majorness into consideration.
    // So should be fine to clear now.
    typeMatMajorAttr = llvm::None;
    return spvType;
  }

  { // Primitive types
    QualType ty = {};
    if (isScalarType(type, &ty)) {
      if (const auto *builtinType = ty->getAs<BuiltinType>()) {
        const bool use16Bit = getCodeGenOptions().enable16BitTypes;

        // Cases sorted roughly according to frequency in source code
        switch (builtinType->getKind()) {
          // 32-bit types
        case BuiltinType::Float:
          // The HalfFloat AST type is just an alias for the Float AST type
          // and is always 32-bit. The HLSL half keyword is translated to
          // HalfFloat if -enable-16bit-types is false.
        case BuiltinType::HalfFloat:
          return spvContext.getFloatType(32);
        case BuiltinType::Int:
          return spvContext.getSIntType(32);
        case BuiltinType::UInt:
          return spvContext.getUIntType(32);

          // void and bool
        case BuiltinType::Void:
          return spvContext.getVoidType();
        case BuiltinType::Bool:
          // According to the SPIR-V spec, there is no physical size or bit
          // pattern defined for boolean type. Therefore an unsigned integer
          // is used to represent booleans when layout is required.
          if (rule == SpirvLayoutRule::Void)
            return spvContext.getBoolType();
          else
            return spvContext.getUIntType(32);

          // 64-bit types
        case BuiltinType::Double:
          return spvContext.getFloatType(64);
        case BuiltinType::LongLong:
          return spvContext.getSIntType(64);
        case BuiltinType::ULongLong:
          return spvContext.getUIntType(64);

          // 16-bit types
          // The Half AST type is always 16-bit. The HLSL half keyword is
          // translated to Half if -enable-16bit-types is true.
        case BuiltinType::Half:
          return spvContext.getFloatType(16);
        case BuiltinType::Short: // int16_t
          return spvContext.getSIntType(16);
        case BuiltinType::UShort: // uint16_t
          return spvContext.getUIntType(16);

          // Relaxed precision types
        case BuiltinType::Min10Float:
        case BuiltinType::Min16Float:
          return spvContext.getFloatType(use16Bit ? 16 : 32);
        case BuiltinType::Min12Int:
        case BuiltinType::Min16Int:
          return spvContext.getSIntType(use16Bit ? 16 : 32);
        case BuiltinType::Min16UInt:
          return spvContext.getUIntType(use16Bit ? 16 : 32);

        // Literal types.
        case BuiltinType::LitInt:
        case BuiltinType::LitFloat: {
          // TODO: analyze adjacent instructions for type hints
          emitError("TODO: literal int/float", srcLoc);
          return spvContext.getVoidType();

        default:
          emitError("primitive type %0 unimplemented", srcLoc)
              << builtinType->getTypeClassName();
          return spvContext.getVoidType();
        }
        }
      }
    }
  }

  // AST vector/matrix types are TypedefType of TemplateSpecializationType. We
  // handle them via HLSL type inspection functions.

  { // Vector types
    QualType elemType = {};
    uint32_t elemCount = {};
    if (isVectorType(type, &elemType, &elemCount))
      return spvContext.getVectorType(lowerType(elemType, rule, srcLoc),
                                      elemCount);
  }

  { // Matrix types
    QualType elemType = {};
    uint32_t rowCount = 0, colCount = 0;
    if (isMxNMatrix(type, &elemType, &rowCount, &colCount)) {
      const auto *vecType =
          spvContext.getVectorType(lowerType(elemType, rule, srcLoc), colCount);

      // Non-float matrices are represented as an array of vectors.
      if (!elemType->isFloatingType()) {
        llvm::Optional<uint32_t> arrayStride = llvm::None;
        // If there is a layout rule, we need array stride information.
        if (rule != SpirvLayoutRule::Void) {
          uint32_t stride = 0;
          (void)getAlignmentAndSize(type, rule, &stride);
          arrayStride = stride;
        }

        // This return type is ArrayType.
        return spvContext.getArrayType(vecType, rowCount, arrayStride);
      }

      return spvContext.getMatrixType(vecType, rowCount);
    }
  }

  // Struct type
  if (const auto *structType = type->getAs<RecordType>()) {
    const auto *decl = structType->getDecl();

    // HLSL resource types are also represented as RecordType in the AST.
    // (ClassTemplateSpecializationDecl is a subclass of CXXRecordDecl, which
    // is then a subclass of RecordDecl.) So we need to check them before
    // checking the general struct type.
    if (const auto *spvType = lowerResourceType(type, rule, srcLoc))
      return spvType;

    // Collect all fields' information.
    llvm::SmallVector<HybridStructType::FieldInfo, 8> fields;

    // If this struct is derived from some other struct, place an implicit
    // field at the very beginning for the base struct.
    if (const auto *cxxDecl = dyn_cast<CXXRecordDecl>(decl)) {
      for (const auto base : cxxDecl->bases()) {
        fields.push_back(HybridStructType::FieldInfo(base.getType()));
      }
    }

    // Create fields for all members of this struct
    for (const auto *field : decl->fields()) {
      fields.push_back(HybridStructType::FieldInfo(
          field->getType(), field->getName(),
          /*vkoffset*/ field->getAttr<VKOffsetAttr>(),
          /*packoffset*/ getPackOffset(field)));
    }

    auto loweredFields = populateLayoutInformation(fields, rule);

    return spvContext.getStructType(loweredFields, decl->getName());
  }

  // Array type
  if (const auto *arrayType = astContext.getAsArrayType(type)) {
    const auto elemType = arrayType->getElementType();
    const auto *loweredElemType =
        lowerType(arrayType->getElementType(), rule, srcLoc);
    llvm::Optional<uint32_t> arrayStride = llvm::None;

    if (rule != SpirvLayoutRule::Void &&
        // We won't have stride information for structured/byte buffers since
        // they contain runtime arrays.
        !isAKindOfStructuredOrByteBuffer(elemType)) {
      uint32_t stride = 0;
      (void)getAlignmentAndSize(type, rule, &stride);
      arrayStride = stride;
    }

    if (const auto *caType = astContext.getAsConstantArrayType(type)) {
      const auto size = static_cast<uint32_t>(caType->getSize().getZExtValue());
      return spvContext.getArrayType(loweredElemType, size, arrayStride);
    }

    assert(type->isIncompleteArrayType());
    return spvContext.getRuntimeArrayType(loweredElemType, arrayStride);
  }

  // Reference types
  if (const auto *refType = type->getAs<ReferenceType>()) {
    // Note: Pointer/reference types are disallowed in HLSL source code.
    // Although developers cannot use them directly, they are generated into
    // the AST by out/inout parameter modifiers in function signatures.
    // We already pass function arguments via pointers to tempoary local
    // variables. So it should be fine to drop the pointer type and treat it
    // as the underlying pointee type here.
    return lowerType(refType->getPointeeType(), rule, srcLoc);
  }

  // Pointer types
  if (const auto *ptrType = type->getAs<PointerType>()) {
    // The this object in a struct member function is of pointer type.
    return lowerType(ptrType->getPointeeType(), rule, srcLoc);
  }

  emitError("lower type %0 unimplemented", srcLoc) << type->getTypeClassName();
  type->dump();
  return 0;
}

const SpirvType *LowerTypeVisitor::lowerResourceType(QualType type,
                                                     SpirvLayoutRule rule,
                                                     SourceLocation srcLoc) {
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
      const bool isMS = (name == "Texture2DMS" || name == "Texture2DMSArray");
      const auto sampledType = hlsl::GetHLSLResourceResultType(type);
      return spvContext.getImageType(
          lowerType(getElementType(sampledType), rule, srcLoc), dim,
          ImageType::WithDepth::Unknown, isArray, isMS,
          ImageType::WithSampler::Yes, spv::ImageFormat::Unknown);
    }

    // There is no RWTexture3DArray
    if ((dim = spv::Dim::Dim1D, isArray = false, name == "RWTexture1D") ||
        (dim = spv::Dim::Dim2D, isArray = false, name == "RWTexture2D") ||
        (dim = spv::Dim::Dim3D, isArray = false, name == "RWTexture3D") ||
        (dim = spv::Dim::Dim1D, isArray = true, name == "RWTexture1DArray") ||
        (dim = spv::Dim::Dim2D, isArray = true, name == "RWTexture2DArray")) {
      const auto sampledType = hlsl::GetHLSLResourceResultType(type);
      const auto format =
          translateSampledTypeToImageFormat(sampledType, srcLoc);
      return spvContext.getImageType(
          lowerType(getElementType(sampledType), rule, srcLoc), dim,
          ImageType::WithDepth::Unknown, isArray,
          /*isMultiSampled=*/false, /*sampled=*/ImageType::WithSampler::No,
          format);
    }
  }

  // Sampler types
  if (name == "SamplerState" || name == "SamplerComparisonState") {
    return spvContext.getSamplerType();
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
    if (rule == SpirvLayoutRule::Void) {
      asAlias = true;
      rule = getCodeGenOptions().sBufferLayoutRule;
    }

    const auto s = hlsl::GetHLSLResourceResultType(type);
    const auto *structType = lowerType(s, rule, srcLoc);
    std::string structName;
    const auto innerType = hlsl::GetHLSLResourceResultType(type);
    if (innerType->isStructureType())
      structName = innerType->getAs<RecordType>()->getDecl()->getName();
    else
      structName = getAstTypeName(innerType);

    uint32_t size = 0, stride = 0;
    std::tie(std::ignore, size) = getAlignmentAndSize(s, rule, &stride);

    // We have a runtime array of structures. So:
    // The stride of the runtime array is the size of the struct.
    const auto *raType = spvContext.getRuntimeArrayType(structType, size);
    const bool isReadOnly = (name == "StructuredBuffer");

    // Attach majorness decoration if this is a *StructuredBuffer<matrix>.
    llvm::Optional<bool> isRowMajor =
        isMxNMatrix(s) ? llvm::Optional<bool>(isRowMajorMatrix(s)) : llvm::None;

    const std::string typeName = "type." + name.str() + "." + structName;
    const auto *valType = spvContext.getStructType(
        {StructType::FieldInfo(raType, /*name*/ "", /*offset*/ 0,
                               /*matrixStride*/ llvm::None, isRowMajor)},
        typeName, isReadOnly, StructInterfaceType::StorageBuffer);

    if (asAlias) {
      // All structured buffers are in the Uniform storage class.
      return spvContext.getPointerType(valType, spv::StorageClass::Uniform);
    }

    return valType;
  }

  // ByteAddressBuffer types.
  if (name == "ByteAddressBuffer") {
    const auto *bufferType =
        spvContext.getByteAddressBufferType(/*isRW*/ false);
    if (rule == SpirvLayoutRule::Void) {
      // All byte address buffers are in the Uniform storage class.
      return spvContext.getPointerType(bufferType, spv::StorageClass::Uniform);
    }
    return bufferType;
  }
  // RWByteAddressBuffer types.
  if (name == "RWByteAddressBuffer") {
    const auto *bufferType = spvContext.getByteAddressBufferType(/*isRW*/ true);
    if (rule == SpirvLayoutRule::Void) {
      // All byte address buffers are in the Uniform storage class.
      return spvContext.getPointerType(bufferType, spv::StorageClass::Uniform);
    }
    return bufferType;
  }

  // Buffer and RWBuffer types
  if (name == "Buffer" || name == "RWBuffer") {
    const auto sampledType = hlsl::GetHLSLResourceResultType(type);
    if (sampledType->isStructureType() && name.startswith("RW")) {
      // Note: actually fxc supports RWBuffer over struct types. However, the
      // struct member must fit into a 4-component vector and writing to a
      // RWBuffer element must write all components. This is a feature that
      // are rarely used by developers. We just emit an error saying not
      // supported for now.
      emitError("cannot instantiate RWBuffer with struct type %0", srcLoc)
          << sampledType;
      return 0;
    }
    const auto format = translateSampledTypeToImageFormat(sampledType, srcLoc);
    return spvContext.getImageType(
        lowerType(getElementType(sampledType), rule, srcLoc), spv::Dim::Buffer,
        ImageType::WithDepth::Unknown,
        /*isArrayed=*/false, /*isMultiSampled=*/false,
        /*sampled*/ name == "Buffer" ? ImageType::WithSampler::Yes
                                     : ImageType::WithSampler::No,
        format);
  }

  // InputPatch
  if (name == "InputPatch") {
    const auto elemType = hlsl::GetHLSLInputPatchElementType(type);
    const auto elemCount = hlsl::GetHLSLInputPatchCount(type);
    return spvContext.getArrayType(lowerType(elemType, rule, srcLoc), elemCount,
                                   /*ArrayStride*/ llvm::None);
  }
  // OutputPatch
  if (name == "OutputPatch") {
    const auto elemType = hlsl::GetHLSLOutputPatchElementType(type);
    const auto elemCount = hlsl::GetHLSLOutputPatchCount(type);
    return spvContext.getArrayType(lowerType(elemType, rule, srcLoc), elemCount,
                                   /*ArrayStride*/ llvm::None);
  }
  // Output stream objects (TriangleStream, LineStream, and PointStream)
  if (name == "TriangleStream" || name == "LineStream" ||
      name == "PointStream") {
    return lowerType(hlsl::GetHLSLResourceResultType(type), rule, srcLoc);
  }

  if (name == "SubpassInput" || name == "SubpassInputMS") {
    const auto sampledType = hlsl::GetHLSLResourceResultType(type);
    return spvContext.getImageType(
        lowerType(getElementType(sampledType), rule, srcLoc),
        spv::Dim::SubpassData, ImageType::WithDepth::Unknown,
        /*isArrayed=*/false,
        /*isMultipleSampled=*/name == "SubpassInputMS",
        ImageType::WithSampler::No, spv::ImageFormat::Unknown);
  }

  return nullptr;
}

spv::ImageFormat
LowerTypeVisitor::translateSampledTypeToImageFormat(QualType sampledType,
                                                    SourceLocation srcLoc) {
  uint32_t elemCount = 1;
  QualType ty = {};
  if (isScalarType(sampledType, &ty) ||
      isVectorType(sampledType, &ty, &elemCount) ||
      canFitIntoOneRegister(sampledType, &ty, &elemCount)) {
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
      default:
        // Other sampled types unimplemented or irrelevant.
        break;
      }
    }
  }
  emitError(
      "cannot translate resource type parameter %0 to proper image format",
      srcLoc)
      << sampledType;

  return spv::ImageFormat::Unknown;
}

QualType LowerTypeVisitor::desugarType(QualType type) {
  if (const auto *attrType = type->getAs<AttributedType>()) {
    switch (auto kind = attrType->getAttrKind()) {
    case AttributedType::attr_hlsl_row_major:
    case AttributedType::attr_hlsl_column_major:
      typeMatMajorAttr = kind;
      break;
    default:
      // Only matrices should apply to typeMatMajorAttr.
      break;
    }
    return desugarType(
        attrType->getLocallyUnqualifiedSingleStepDesugaredType());
  }

  if (const auto *typedefType = type->getAs<TypedefType>()) {
    return desugarType(typedefType->desugar());
  }

  return type;
}

bool LowerTypeVisitor::isHLSLRowMajorMatrix(QualType type) const {
  // The type passed in may not be desugared. Check attributes on itself first.
  bool attrRowMajor = false;
  if (hlsl::HasHLSLMatOrientation(type, &attrRowMajor))
    return attrRowMajor;

  // Use the majorness info we recorded before.
  if (typeMatMajorAttr.hasValue()) {
    switch (typeMatMajorAttr.getValue()) {
    case AttributedType::attr_hlsl_row_major:
      return true;
    case AttributedType::attr_hlsl_column_major:
      return false;
    default:
      // Only oriented matrices are relevant.
      break;
    }
  }
  return spvOptions.defaultRowMajor;
}

bool LowerTypeVisitor::isRowMajorMatrix(QualType type) const {
  return !isHLSLRowMajorMatrix(type);
}

llvm::SmallVector<StructType::FieldInfo, 4>
LowerTypeVisitor::populateLayoutInformation(
    llvm::ArrayRef<HybridStructType::FieldInfo> fields, SpirvLayoutRule rule) {

  // The resulting vector of fields with proper layout information.
  llvm::SmallVector<StructType::FieldInfo, 4> loweredFields;

  uint32_t offset = 0;
  for (const auto field : fields) {
    // The field can only be FieldDecl (for normal structs) or VarDecl (for
    // HLSLBufferDecls).
    auto fieldType = field.astType;

    // Lower the field type fist. This call will populate proper matrix
    // majorness information.
    StructType::FieldInfo loweredField(lowerType(fieldType, rule, {}),
                                       field.name);

    // We only need layout information for strcutres with non-void layout rule.
    if (rule == SpirvLayoutRule::Void) {
      loweredFields.push_back(loweredField);
      continue;
    }

    uint32_t memberAlignment = 0, memberSize = 0, stride = 0;
    std::tie(memberAlignment, memberSize) =
        getAlignmentAndSize(fieldType, rule, &stride);

    // The next avaiable location after layouting the previos members
    const uint32_t nextLoc = offset;

    if (rule == SpirvLayoutRule::RelaxedGLSLStd140 ||
        rule == SpirvLayoutRule::RelaxedGLSLStd430 ||
        rule == SpirvLayoutRule::FxcCTBuffer) {
      alignUsingHLSLRelaxedLayout(fieldType, memberSize, memberAlignment,
                                  &offset);
    } else {
      offset = roundToPow2(offset, memberAlignment);
    }

    // The vk::offset attribute takes precedence over all.
    if (field.vkOffsetAttr) {
      offset = field.vkOffsetAttr->getOffset();
    }
    // The :packoffset() annotation takes precedence over normal layout
    // calculation.
    else if (field.packOffsetAttr) {
      const uint32_t packOffset = field.packOffsetAttr->Subcomponent * 16 +
                                  field.packOffsetAttr->ComponentOffset * 4;
      // Do minimal check to make sure the offset specified by packoffset does
      // not cause overlap.
      if (packOffset < nextLoc) {
        emitError("packoffset caused overlap with previous members",
                  field.packOffsetAttr->Loc);
      } else {
        offset = packOffset;
      }
    }

    // Each structure-type member must have an Offset Decoration.
    loweredField.offset = offset;
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

      loweredField.matrixStride = stride;
      loweredField.isRowMajor = isRowMajorMatrix(fieldType);
    }

    loweredFields.push_back(loweredField);
  }

  return loweredFields;
}

void LowerTypeVisitor::alignUsingHLSLRelaxedLayout(QualType fieldType,
                                                   uint32_t fieldSize,
                                                   uint32_t fieldAlignment,
                                                   uint32_t *currentOffset) {
  QualType vecElemType = {};
  const bool fieldIsVecType = isVectorType(fieldType, &vecElemType);

  // Adjust according to HLSL relaxed layout rules.
  // Aligning vectors as their element types so that we can pack a float
  // and a float3 tightly together.
  if (fieldIsVecType) {
    uint32_t scalarAlignment = 0;
    std::tie(scalarAlignment, std::ignore) =
        getAlignmentAndSize(vecElemType, SpirvLayoutRule::Void, nullptr);
    if (scalarAlignment <= 4)
      fieldAlignment = scalarAlignment;
  }

  *currentOffset = roundToPow2(*currentOffset, fieldAlignment);

  // Adjust according to HLSL relaxed layout rules.
  // Bump to 4-component vector alignment if there is a bad straddle
  if (fieldIsVecType &&
      improperStraddle(fieldType, fieldSize, *currentOffset)) {
    fieldAlignment = kStd140Vec4Alignment;
    *currentOffset = roundToPow2(*currentOffset, fieldAlignment);
  }
}

std::pair<uint32_t, uint32_t>
LowerTypeVisitor::getAlignmentAndSize(QualType type, SpirvLayoutRule rule,
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
  //
  // FxcSBuffer:
  // - Vector/matrix/array base alignment is set as its element type's base
  //   alignment.
  // - Arrays/structs do not need to have padding at the end; arrays/structs do
  //   not affect the base offset of the member following them.
  // - Struct base alignment does not need to be rounded up to a multiple of 16.

  const auto desugaredType = desugarType(type);
  if (desugaredType != type) {
    auto result = getAlignmentAndSize(desugaredType, rule, stride);
    // Clear potentially set matrix majorness info
    typeMatMajorAttr = llvm::None;
    return result;
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
        case BuiltinType::Min12Int:
        case BuiltinType::Min16Int:
        case BuiltinType::Min16UInt:
        case BuiltinType::Min16Float:
        case BuiltinType::Min10Float: {
          if (spvOptions.enable16BitTypes)
            return {2, 2};
          else
            return {4, 4};
        }
        // the 'Half' enum always represents 16-bit floats.
        // int16_t and uint16_t map to Short and UShort.
        case BuiltinType::Short:
        case BuiltinType::UShort:
        case BuiltinType::Half:
          return {2, 2};
        // 'HalfFloat' always represents 32-bit floats.
        case BuiltinType::HalfFloat:
          return {4, 4};
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
      if (rule != SpirvLayoutRule::FxcCTBuffer &&
          rule != SpirvLayoutRule::FxcSBuffer)
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

      const uint32_t vecStorageSize = isRowMajor ? rowCount : colCount;

      if (rule == SpirvLayoutRule::FxcSBuffer) {
        *stride = vecStorageSize * size;
        // Use element alignment for fxc structured buffers
        return {alignment, rowCount * colCount * size};
      }

      alignment *= (vecStorageSize == 3 ? 4 : vecStorageSize);
      if (rule == SpirvLayoutRule::GLSLStd140 ||
          rule == SpirvLayoutRule::RelaxedGLSLStd140 ||
          rule == SpirvLayoutRule::FxcCTBuffer) {
        alignment = roundToPow2(alignment, kStd140Vec4Alignment);
      }
      *stride = alignment;
      size = (isRowMajor ? colCount : rowCount) * alignment;

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

      if (rule == SpirvLayoutRule::RelaxedGLSLStd140 ||
          rule == SpirvLayoutRule::RelaxedGLSLStd430 ||
          rule == SpirvLayoutRule::FxcCTBuffer) {
        alignUsingHLSLRelaxedLayout(field->getType(), memberSize,
                                    memberAlignment, &structSize);
      } else {
        structSize = roundToPow2(structSize, memberAlignment);
      }

      // Reset the current offset to the one specified in the source code
      // if exists. It's debatable whether we should do sanity check here.
      // If the developers want manually control the layout, we leave
      // everything to them.
      if (const auto *offsetAttr = field->getAttr<VKOffsetAttr>()) {
        structSize = offsetAttr->getOffset();
      }

      // The base alignment of the structure is N, where N is the largest
      // base alignment value of any of its members...
      maxAlignment = std::max(maxAlignment, memberAlignment);
      structSize += memberSize;
    }

    if (rule == SpirvLayoutRule::GLSLStd140 ||
        rule == SpirvLayoutRule::RelaxedGLSLStd140 ||
        rule == SpirvLayoutRule::FxcCTBuffer) {
      // ... and rounded up to the base alignment of a vec4.
      maxAlignment = roundToPow2(maxAlignment, kStd140Vec4Alignment);
    }

    if (rule != SpirvLayoutRule::FxcCTBuffer &&
        rule != SpirvLayoutRule::FxcSBuffer) {
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

    if (rule == SpirvLayoutRule::FxcSBuffer) {
      *stride = size;
      // Use element alignment for fxc structured buffers
      return {alignment, size * elemCount};
    }

    if (rule == SpirvLayoutRule::GLSLStd140 ||
        rule == SpirvLayoutRule::RelaxedGLSLStd140 ||
        rule == SpirvLayoutRule::FxcCTBuffer) {
      // The base alignment and array stride are set to match the base alignment
      // of a single array element, according to rules 1, 2, and 3, and rounded
      // up to the base alignment of a vec4.
      alignment = roundToPow2(alignment, kStd140Vec4Alignment);
    }
    if (rule == SpirvLayoutRule::FxcCTBuffer) {
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

} // namespace spirv
} // namespace clang
