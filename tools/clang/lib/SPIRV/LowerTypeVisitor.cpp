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
    std::vector<StructType::FieldInfo> structFields;
    for (auto field : hybridStruct->getFields()) {
      const SpirvType *fieldSpirvType = lowerType(field.astType, rule, loc);
      llvm::Optional<bool> isRowMajor = isRowMajorMatrix(field.astType);
      structFields.push_back(
          StructType::FieldInfo(fieldSpirvType, field.name, field.vkOffsetAttr,
                                field.packOffsetAttr, isRowMajor));
    }
    return spvContext.getStructType(structFields, hybridStruct->getStructName(),
                                    hybridStruct->isReadOnly(),
                                    hybridStruct->getInterfaceType());
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
                                   arrType->hasRowMajorElement());
  }
  // Runtime arrays could contain a hybrid type
  else if (const auto *raType = dyn_cast<RuntimeArrayType>(type)) {
    const auto *loweredElemType =
        lowerType(raType->getElementType(), rule, loc);
    // If runtime array didn't contain any hybrid types, return itself.
    if (raType->getElementType() == loweredElemType)
      return arrType;
    return spvContext.getRuntimeArrayType(loweredElemType);
  }
  // Struct types could contain a hybrid type
  else if (const auto *structType = dyn_cast<StructType>(type)) {
    const auto &fields = structType->getFields();
    llvm::SmallVector<StructType::FieldInfo, 4> loweredFields;
    bool wasLowered = false;
    for (auto &field : fields) {
      const auto *loweredFieldType = lowerType(field.type, rule, loc);
      if (loweredFieldType != field.type) {
        wasLowered = true;
        loweredFields.push_back(StructType::FieldInfo(
            loweredFieldType, field.name, field.vkOffsetAttr,
            field.packOffsetAttr, field.isRowMajor));
      } else {
        loweredFields.push_back(field);
      }
    }
    // If the struct didn't contain any hybrid types, return itself.
    if (!wasLowered)
      return structType;

    return spvContext.getStructType(loweredFields, structType->getStructName(),
                                    structType->isReadOnly(),
                                    structType->getInterfaceType());
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
        // This return type is ArrayType
        // This is an array of vectors. No majorness information needed.
        return spvContext.getArrayType(vecType, rowCount,
                                       /*rowMajorElem*/ llvm::None);
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
    llvm::SmallVector<StructType::FieldInfo, 8> fields;

    // If this struct is derived from some other struct, place an implicit
    // field at the very beginning for the base struct.
    if (const auto *cxxDecl = dyn_cast<CXXRecordDecl>(decl))
      for (const auto base : cxxDecl->bases()) {
        fields.push_back(
            StructType::FieldInfo(lowerType(base.getType(), rule, srcLoc)));
      }

    // Create fields for all members of this struct
    for (const auto *field : decl->fields()) {
      const SpirvType *fieldType = lowerType(field->getType(), rule, srcLoc);
      llvm::Optional<bool> isRowMajor = isRowMajorMatrix(field->getType());
      fields.push_back(StructType::FieldInfo(
          fieldType, field->getName(),
          /*vkoffset*/ field->getAttr<VKOffsetAttr>(),
          /*packoffset*/ getPackOffset(field), isRowMajor));
    }

    return spvContext.getStructType(fields, decl->getName());
  }

  // Array type
  if (const auto *arrayType = astContext.getAsArrayType(type)) {
    const auto *elemType = lowerType(arrayType->getElementType(), rule, srcLoc);

    if (const auto *caType = astContext.getAsConstantArrayType(type)) {
      const auto size = static_cast<uint32_t>(caType->getSize().getZExtValue());
      llvm::Optional<bool> isRowMajor = isRowMajorMatrix(type);
      return spvContext.getArrayType(elemType, size, isRowMajor);
    }

    assert(type->isIncompleteArrayType());
    return spvContext.getRuntimeArrayType(elemType);
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

    const auto *raType = spvContext.getRuntimeArrayType(structType);
    const bool isReadOnly = (name == "StructuredBuffer");

    const std::string typeName = "type." + name.str() + "." + structName;
    const auto *valType = spvContext.getStructType(
        {StructType::FieldInfo(raType)}, typeName, isReadOnly,
        StructInterfaceType::StorageBuffer);

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
    llvm::Optional<bool> isRowMajor = isRowMajorMatrix(type);
    return spvContext.getArrayType(lowerType(elemType, rule, srcLoc), elemCount,
                                   isRowMajor);
  }
  // OutputPatch
  if (name == "OutputPatch") {
    const auto elemType = hlsl::GetHLSLOutputPatchElementType(type);
    const auto elemCount = hlsl::GetHLSLOutputPatchCount(type);
    llvm::Optional<bool> isRowMajor = isRowMajorMatrix(type);
    return spvContext.getArrayType(lowerType(elemType, rule, srcLoc),
                                   elemCount, isRowMajor);
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
    return desugarType(
        attrType->getLocallyUnqualifiedSingleStepDesugaredType());
  }

  if (const auto *typedefType = type->getAs<TypedefType>()) {
    return desugarType(typedefType->desugar());
  }

  return type;
}

llvm::Optional<bool>
LowerTypeVisitor::isHLSLRowMajorMatrix(QualType type) const {
  if (const auto *attrType = type->getAs<AttributedType>()) {
    switch (auto kind = attrType->getAttrKind()) {
    case AttributedType::attr_hlsl_row_major:
      return true;
    case AttributedType::attr_hlsl_column_major:
      return false;
      break;
    default:
      break;
    }
  }
  if (const auto *typedefType = type->getAs<TypedefType>()) {
    return isHLSLRowMajorMatrix(typedefType->desugar());
  }
  if (const auto *arrayType = astContext.getAsArrayType(type)) {
    return isHLSLRowMajorMatrix(arrayType->getElementType());
  }
  return getCodeGenOptions().defaultRowMajor;
}

llvm::Optional<bool> LowerTypeVisitor::isRowMajorMatrix(QualType type) const {
  // Row/Col majorness only applies to matrices or array of matrices.
  if (!isMatrixOrArrayOfMatrix(astContext, type))
    return llvm::None;

  const auto hlslRowMajor = isHLSLRowMajorMatrix(type);
  if (!hlslRowMajor.hasValue())
    return hlslRowMajor;

  return !hlslRowMajor.getValue();
}

} // namespace spirv
} // namespace clang
