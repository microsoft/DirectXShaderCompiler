//===--- LowerTypeVisitor.cpp - AST type to SPIR-V type impl -----*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "LowerTypeVisitor.h"
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

/// Rounds the given value up to the given power of 2.
inline uint32_t roundToPow2(uint32_t val, uint32_t pow2) {
  assert(pow2 != 0);
  return (val + pow2 - 1) & ~(pow2 - 1);
}

} // end anonymous namespace

namespace clang {
namespace spirv {

bool LowerTypeVisitor::visit(SpirvFunction *fn, Phase phase) {
  if (phase == Visitor::Phase::Init) {
    // Lower the function return type.
    const SpirvType *spirvReturnType =
        lowerType(fn->getAstReturnType(), SpirvLayoutRule::Void,
                  /*isRowMajor*/ llvm::None,
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
        lowerType(astType, instr->getLayoutRule(), /*isRowMajor*/ llvm::None,
                  instr->getSourceLocation());
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
  // Variables and function parameters must have a pointer type.
  case spv::Op::OpFunctionParameter:
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
    const SpirvType *pointeeSpirvType =
        lowerType(pointeeType, rule, /*isRowMajor*/ llvm::None, loc);
    return spvContext.getPointerType(pointeeSpirvType,
                                     hybridPointer->getStorageClass());
  } else if (const auto *hybridSampledImage =
                 dyn_cast<HybridSampledImageType>(type)) {
    const QualType imageAstType = hybridSampledImage->getImageType();
    const SpirvType *imageSpirvType =
        lowerType(imageAstType, rule, /*isRowMajor*/ llvm::None, loc);
    assert(isa<ImageType>(imageSpirvType));
    return spvContext.getSampledImageType(cast<ImageType>(imageSpirvType));
  } else if (const auto *hybridFn = dyn_cast<HybridFunctionType>(type)) {
    // Lower the return type.
    const QualType astReturnType = hybridFn->getReturnType();
    const SpirvType *spirvReturnType =
        lowerType(astReturnType, rule, /*isRowMajor*/ llvm::None, loc);

    // Go over all params and lower them.
    std::vector<const SpirvType *> paramTypes;
    for (auto paramType : hybridFn->getParamTypes()) {
      const auto *spirvParamType =
          lowerType(paramType, rule, /*isRowMajor*/ llvm::None, loc);
      paramTypes.push_back(spvContext.getPointerType(
          spirvParamType, spv::StorageClass::Function));
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
  // FunctionType is not allowed to contain hybrid parameters or return type.
  // StructType is not allowed to contain any hybrid types.
  else if (isa<VoidType>(type) || isa<ScalarType>(type) ||
           isa<MatrixType>(type) || isa<ImageType>(type) ||
           isa<SamplerType>(type) || isa<SampledImageType>(type) ||
           isa<FunctionType>(type) || isa<StructType>(type)) {
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

  llvm_unreachable("lowering of hybrid type not implemented");
}

const SpirvType *LowerTypeVisitor::lowerType(QualType type,
                                             SpirvLayoutRule rule,
                                             llvm::Optional<bool> isRowMajor,
                                             SourceLocation srcLoc) {
  const auto desugaredType = desugarType(type, &isRowMajor);

  if (desugaredType != type) {
    const auto *spvType = lowerType(desugaredType, rule, isRowMajor, srcLoc);
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

        // All literal types should have been lowered to concrete types before
        // LowerTypeVisitor is invoked. However, if there are unused literals,
        // they will still have 'literal' type when we get to this point. Use
        // 32-bit width by default for these cases.
        // Example:
        // void main() { 1.0; 1; }
        case BuiltinType::LitInt:
          return type->isSignedIntegerType() ? spvContext.getSIntType(32)
                                             : spvContext.getUIntType(32);
        case BuiltinType::LitFloat: {
          return spvContext.getFloatType(32);

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
      return spvContext.getVectorType(
          lowerType(elemType, rule, isRowMajor, srcLoc), elemCount);
  }

  { // Matrix types
    QualType elemType = {};
    uint32_t rowCount = 0, colCount = 0;
    if (isMxNMatrix(type, &elemType, &rowCount, &colCount)) {
      const auto *vecType = spvContext.getVectorType(
          lowerType(elemType, rule, isRowMajor, srcLoc), colCount);

      // Non-float matrices are represented as an array of vectors.
      if (!elemType->isFloatingType()) {
        llvm::Optional<uint32_t> arrayStride = llvm::None;
        // If there is a layout rule, we need array stride information.
        if (rule != SpirvLayoutRule::Void) {
          uint32_t stride = 0;
          alignmentCalc.getAlignmentAndSize(type, rule, isRowMajor, &stride);
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
          /*packoffset*/ getPackOffset(field),
          /*RegisterAssignment*/ nullptr,
          /*isPrecise*/ field->hasAttr<HLSLPreciseAttr>()));
    }

    auto loweredFields = populateLayoutInformation(fields, rule);

    return spvContext.getStructType(loweredFields, decl->getName());
  }

  // Array type
  if (const auto *arrayType = astContext.getAsArrayType(type)) {
    const auto elemType = arrayType->getElementType();
    const auto *loweredElemType =
        lowerType(arrayType->getElementType(), rule, isRowMajor, srcLoc);
    llvm::Optional<uint32_t> arrayStride = llvm::None;

    if (rule != SpirvLayoutRule::Void &&
        // We won't have stride information for structured/byte buffers since
        // they contain runtime arrays.
        !isAKindOfStructuredOrByteBuffer(elemType)) {
      uint32_t stride = 0;
      alignmentCalc.getAlignmentAndSize(type, rule, isRowMajor, &stride);
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
    return lowerType(refType->getPointeeType(), rule, isRowMajor, srcLoc);
  }

  // Pointer types
  if (const auto *ptrType = type->getAs<PointerType>()) {
    // The this object in a struct member function is of pointer type.
    return lowerType(ptrType->getPointeeType(), rule, isRowMajor, srcLoc);
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
          lowerType(getElementType(astContext, sampledType), rule,
                    /*isRowMajor*/ llvm::None, srcLoc),
          dim, ImageType::WithDepth::Unknown, isArray, isMS,
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
          lowerType(getElementType(astContext, sampledType), rule,
                    /*isRowMajor*/ llvm::None, srcLoc),
          dim, ImageType::WithDepth::Unknown, isArray,
          /*isMultiSampled=*/false, /*sampled=*/ImageType::WithSampler::No,
          format);
    }
  }

  // Sampler types
  if (name == "SamplerState" || name == "SamplerComparisonState") {
    return spvContext.getSamplerType();
  }

  if (name == "RaytracingAccelerationStructure") {
    return spvContext.getAccelerationStructureTypeNV();
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

    // Get the underlying resource type.
    const auto s = hlsl::GetHLSLResourceResultType(type);

    // If the underlying type is a matrix, check majorness.
    llvm::Optional<bool> isRowMajor = llvm::None;
    if (isMxNMatrix(s))
      isRowMajor = isRowMajorMatrix(spvOptions, type);

    // Lower the underlying type.
    const auto *structType = lowerType(s, rule, isRowMajor, srcLoc);

    // Calculate memory alignment for the resource.
    uint32_t size = 0, stride = 0;
    std::tie(std::ignore, size) =
        alignmentCalc.getAlignmentAndSize(s, rule, isRowMajor, &stride);

    // We have a runtime array of structures. So:
    // The stride of the runtime array is the size of the struct.
    const auto *raType = spvContext.getRuntimeArrayType(structType, size);
    const bool isReadOnly = (name == "StructuredBuffer");

    // Attach matrix stride decorations if this is a *StructuredBuffer<matrix>.
    llvm::Optional<uint32_t> matrixStride = llvm::None;
    if (isMxNMatrix(s))
      matrixStride = stride;

    const std::string typeName = "type." + name.str() + "." + getAstTypeName(s);
    const auto *valType = spvContext.getStructType(
        {StructType::FieldInfo(raType, /*name*/ "", /*offset*/ 0, matrixStride,
                               isRowMajor)},
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
        lowerType(getElementType(astContext, sampledType), rule,
                  /*isRowMajor*/ llvm::None, srcLoc),
        spv::Dim::Buffer, ImageType::WithDepth::Unknown,
        /*isArrayed=*/false, /*isMultiSampled=*/false,
        /*sampled*/ name == "Buffer" ? ImageType::WithSampler::Yes
                                     : ImageType::WithSampler::No,
        format);
  }

  // InputPatch
  if (name == "InputPatch") {
    const auto elemType = hlsl::GetHLSLInputPatchElementType(type);
    const auto elemCount = hlsl::GetHLSLInputPatchCount(type);
    return spvContext.getArrayType(
        lowerType(elemType, rule, /*isRowMajor*/ llvm::None, srcLoc), elemCount,
        /*ArrayStride*/ llvm::None);
  }
  // OutputPatch
  if (name == "OutputPatch") {
    const auto elemType = hlsl::GetHLSLOutputPatchElementType(type);
    const auto elemCount = hlsl::GetHLSLOutputPatchCount(type);
    return spvContext.getArrayType(
        lowerType(elemType, rule, /*isRowMajor*/ llvm::None, srcLoc), elemCount,
        /*ArrayStride*/ llvm::None);
  }
  // Output stream objects (TriangleStream, LineStream, and PointStream)
  if (name == "TriangleStream" || name == "LineStream" ||
      name == "PointStream") {
    return lowerType(hlsl::GetHLSLResourceResultType(type), rule,
                     /*isRowMajor*/ llvm::None, srcLoc);
  }

  if (name == "SubpassInput" || name == "SubpassInputMS") {
    const auto sampledType = hlsl::GetHLSLResourceResultType(type);
    return spvContext.getImageType(
        lowerType(getElementType(astContext, sampledType), rule,
                  /*isRowMajor*/ llvm::None, srcLoc),
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
      canFitIntoOneRegister(astContext, sampledType, &ty, &elemCount)) {
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
      case BuiltinType::HalfFloat:
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

llvm::SmallVector<StructType::FieldInfo, 4>
LowerTypeVisitor::populateLayoutInformation(
    llvm::ArrayRef<HybridStructType::FieldInfo> fields, SpirvLayoutRule rule) {

  // The resulting vector of fields with proper layout information.
  llvm::SmallVector<StructType::FieldInfo, 4> loweredFields;
  llvm::SmallVector<StructType::FieldInfo, 4> result;

  using RegisterFieldPair =
      std::pair<uint32_t, const HybridStructType::FieldInfo *>;
  struct RegisterFieldPairLess {
    bool operator()(const RegisterFieldPair &obj1,
                    const RegisterFieldPair &obj2) const {
      return obj1.first < obj2.first;
    }
  };
  std::set<RegisterFieldPair, RegisterFieldPairLess> registerCSet;
  std::vector<const HybridStructType::FieldInfo *> sortedFields;
  llvm::DenseMap<const HybridStructType::FieldInfo *, uint32_t> fieldToIndexMap;

  // First, check to see if any of the structure members had 'register(c#)'
  // location semantics. If so, members that do not have the 'register(c#)'
  // assignment should be allocated after the *highest explicit address*.
  // Example:
  // float x : register(c10);   // Offset = 160 (10 * 16)
  // float y;                   // Offset = 164 (160 + 4)
  // float z: register(c1);     // Offset = 16  (1  * 16)
  for (const auto &field : fields)
    if (field.registerC)
      registerCSet.insert(
          RegisterFieldPair(field.registerC->RegisterNumber, &field));
  for (const auto &pair : registerCSet)
    sortedFields.push_back(pair.second);
  for (const auto &field : fields)
    if (!field.registerC)
      sortedFields.push_back(&field);

  uint32_t offset = 0;
  for (const auto *fieldPtr : sortedFields) {
    // The field can only be FieldDecl (for normal structs) or VarDecl (for
    // HLSLBufferDecls).
    const auto field = *fieldPtr;
    auto fieldType = field.astType;
    fieldToIndexMap[fieldPtr] = loweredFields.size();

    // Lower the field type fist. This call will populate proper matrix
    // majorness information.
    StructType::FieldInfo loweredField(
        lowerType(fieldType, rule, /*isRowMajor*/ llvm::None, {}), field.name);

    // Set RelaxedPrecision information for the lowered field.
    if (isRelaxedPrecisionType(fieldType, spvOptions)) {
      loweredField.isRelaxedPrecision = true;
    }

    // Set 'precise' information for the lowered field.
    if (field.isPrecise) {
      loweredField.isPrecise = true;
    }

    // We only need layout information for strcutres with non-void layout rule.
    if (rule == SpirvLayoutRule::Void) {
      loweredFields.push_back(loweredField);
      continue;
    }

    uint32_t memberAlignment = 0, memberSize = 0, stride = 0;
    std::tie(memberAlignment, memberSize) = alignmentCalc.getAlignmentAndSize(
        fieldType, rule, /*isRowMajor*/ llvm::None, &stride);

    // The next avaiable location after laying out the previous members
    const uint32_t nextLoc = offset;

    if (rule == SpirvLayoutRule::RelaxedGLSLStd140 ||
        rule == SpirvLayoutRule::RelaxedGLSLStd430 ||
        rule == SpirvLayoutRule::FxcCTBuffer) {
      alignmentCalc.alignUsingHLSLRelaxedLayout(fieldType, memberSize,
                                                memberAlignment, &offset);
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
    // The :register(c#) annotation takes precedence over normal layout
    // calculation.
    else if (field.registerC) {
      offset = 16 * field.registerC->RegisterNumber;
      // Do minimal check to make sure the offset specified by :register(c#)
      // does not cause overlap.
      if (offset < nextLoc) {
        emitError(
            "found offset overlap when processing register(c%0) assignment",
            field.registerC->Loc)
            << field.registerC->RegisterNumber;
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
      std::tie(memberAlignment, memberSize) = alignmentCalc.getAlignmentAndSize(
          fieldType, rule, /*isRowMajor*/ llvm::None, &stride);

      loweredField.matrixStride = stride;
      loweredField.isRowMajor = isRowMajorMatrix(spvOptions, fieldType);
    }

    loweredFields.push_back(loweredField);
  }

  // Re-order the sorted fields back to their original order.
  for (const auto &field : fields)
    result.push_back(loweredFields[fieldToIndexMap[&field]]);

  return result;
}

} // namespace spirv
} // namespace clang
