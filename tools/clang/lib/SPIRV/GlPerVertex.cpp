//===--- GlPerVertex.cpp - GlPerVertex implementation ------------*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "GlPerVertex.h"

#include <algorithm>

#include "clang/AST/Attr.h"
#include "clang/AST/HlslTypes.h"

namespace clang {
namespace spirv {

namespace {
/// \brief Returns true if the given decl has a semantic string attached and
/// writes the info to *semanticStr, *semantic, and *semanticIndex.
// TODO: duplication! Same as the one in DeclResultIdMapper.cpp
bool getStageVarSemantic(const NamedDecl *decl, llvm::StringRef *semanticStr,
                         const hlsl::Semantic **semantic,
                         uint32_t *semanticIndex) {
  for (auto *annotation : decl->getUnusualAnnotations()) {
    if (auto *sema = dyn_cast<hlsl::SemanticDecl>(annotation)) {
      *semanticStr = sema->SemanticName;
      llvm::StringRef semanticName;
      hlsl::Semantic::DecomposeNameAndIndex(*semanticStr, &semanticName,
                                            semanticIndex);
      *semantic = hlsl::Semantic::GetByName(semanticName);
      return true;
    }
  }
  return false;
}

/// Returns the type of the given decl. If the given decl is a FunctionDecl,
/// returns its result type.
inline QualType getTypeOrFnRetType(const DeclaratorDecl *decl) {
  if (const auto *funcDecl = dyn_cast<FunctionDecl>(decl)) {
    return funcDecl->getReturnType();
  }
  return decl->getType();
}

/// Returns true if the given declaration has a primitive type qualifier.
/// Returns false otherwise.
inline bool hasGSPrimitiveTypeQualifier(const DeclaratorDecl *decl) {
  return decl->hasAttr<HLSLTriangleAttr>() ||
         decl->hasAttr<HLSLTriangleAdjAttr>() ||
         decl->hasAttr<HLSLPointAttr>() || decl->hasAttr<HLSLLineAttr>() ||
         decl->hasAttr<HLSLLineAdjAttr>();
}
} // anonymous namespace

GlPerVertex::GlPerVertex(const hlsl::ShaderModel &sm, ASTContext &context,
                         ModuleBuilder &builder, TypeTranslator &translator,
                         bool negateY)
    : shaderModel(sm), astContext(context), theBuilder(builder),
      typeTranslator(translator), invertY(negateY), inIsGrouped(true),
      outIsGrouped(true), inBlockVar(0), outBlockVar(0), inClipVar(0),
      inCullVar(0), outClipVar(0), outCullVar(0), inArraySize(0),
      outArraySize(0), inClipArraySize(1), outClipArraySize(1),
      inCullArraySize(1), outCullArraySize(1) {}

void GlPerVertex::generateVars(uint32_t inArrayLen, uint32_t outArrayLen) {
  // Calling this method twice is an internal error.
  assert(inBlockVar == 0);
  assert(outBlockVar == 0);

  inArraySize = inArrayLen;
  outArraySize = outArrayLen;

  switch (shaderModel.GetKind()) {
  case hlsl::ShaderModel::Kind::Vertex:
    outBlockVar = createBlockVar(/*asInput=*/false, 0);
    break;
  case hlsl::ShaderModel::Kind::Hull:
    inBlockVar = createBlockVar(/*asInput=*/true, inArraySize);
    outBlockVar = createBlockVar(/*asInput=*/false, outArraySize);
    break;
  case hlsl::ShaderModel::Kind::Domain:
    inBlockVar = createBlockVar(/*asInput=*/true, inArraySize);
    outBlockVar = createBlockVar(/*asInput=*/false, 0);
    break;
  case hlsl::ShaderModel::Kind::Geometry:
    inBlockVar = createBlockVar(/*asInput=*/true, inArraySize);
    if (!outClipType.empty())
      outClipVar = createClipDistanceVar(/*asInput=*/false, outClipArraySize);
    if (!outCullType.empty())
      outCullVar = createCullDistanceVar(/*asInput=*/false, outCullArraySize);
    outIsGrouped = false;
    break;
  case hlsl::ShaderModel::Kind::Pixel:
    if (!inClipType.empty())
      inClipVar = createClipDistanceVar(/*asInput=*/true, inClipArraySize);
    if (!inCullType.empty())
      inCullVar = createCullDistanceVar(/*asInput=*/true, inCullArraySize);
    inIsGrouped = false;
    break;
  }
}

llvm::SmallVector<uint32_t, 4> GlPerVertex::getStageInVars() const {
  llvm::SmallVector<uint32_t, 4> vars;
  if (inIsGrouped) {
    if (inBlockVar)
      vars.push_back(inBlockVar);
  } else {
    if (inClipVar)
      vars.push_back(inClipVar);
    if (inCullVar)
      vars.push_back(inCullVar);
  }

  return vars;
}

llvm::SmallVector<uint32_t, 4> GlPerVertex::getStageOutVars() const {
  llvm::SmallVector<uint32_t, 4> vars;
  if (outIsGrouped) {
    if (outBlockVar)
      vars.push_back(outBlockVar);
  } else {
    if (outClipVar)
      vars.push_back(outClipVar);
    if (outCullVar)
      vars.push_back(outCullVar);
  }

  return vars;
}

void GlPerVertex::requireCapabilityIfNecessary() {
  if (!inClipType.empty() || !outClipType.empty())
    theBuilder.requireCapability(spv::Capability::ClipDistance);

  if (!inCullType.empty() || !outCullType.empty())
    theBuilder.requireCapability(spv::Capability::CullDistance);
}

bool GlPerVertex::recordClipCullDistanceDecl(const DeclaratorDecl *decl,
                                             bool asInput) {
  const QualType type = getTypeOrFnRetType(decl);

  if (type->isVoidType())
    return true;

  return doClipCullDistanceDecl(decl, type, asInput);
}

bool GlPerVertex::doClipCullDistanceDecl(const DeclaratorDecl *decl,
                                         QualType baseType, bool asInput) {

  llvm::StringRef semanticStr;
  const hlsl::Semantic *semantic = {};
  uint32_t semanticIndex = {};

  if (!getStageVarSemantic(decl, &semanticStr, &semantic, &semanticIndex)) {
    if (baseType->isStructureType()) {
      const auto *structDecl = baseType->getAs<RecordType>()->getDecl();
      // Go through each field to see if there is any usage of
      // SV_ClipDistance/SV_CullDistance.
      for (const auto *field : structDecl->fields()) {
        if (!doClipCullDistanceDecl(field, field->getType(), asInput))
          return false;
      }
      return true;
    }

    // For these HS/DS/GS specific data types, semantic strings are attached
    // to the underlying struct's fields.
    if (hlsl::IsHLSLInputPatchType(baseType)) {
      return doClipCullDistanceDecl(
          decl, hlsl::GetHLSLInputPatchElementType(baseType), asInput);
    }
    if (hlsl::IsHLSLOutputPatchType(baseType)) {
      return doClipCullDistanceDecl(
          decl, hlsl::GetHLSLOutputPatchElementType(baseType), asInput);
    }

    if (hlsl::IsHLSLStreamOutputType(baseType)) {
      return doClipCullDistanceDecl(
          decl, hlsl::GetHLSLOutputPatchElementType(baseType), asInput);
    }
    if (hasGSPrimitiveTypeQualifier(decl)) {
      // GS inputs have an additional arrayness that we should remove to check
      // the underlying type instead.
      baseType = astContext.getAsConstantArrayType(baseType)->getElementType();
      return doClipCullDistanceDecl(decl, baseType, asInput);
    }

    emitError("semantic string missing for shader %select{output|input}0 "
              "variable '%1'",
              decl->getLocation())
        << asInput << decl->getName();
    return false;
  }

  // Semantic string is attched to this decl directly

  // Select the corresponding data member to update
  SemanticIndexToTypeMap *typeMap = nullptr;
  uint32_t *blockArraySize = asInput ? &inArraySize : &outArraySize;
  bool isCull = false;

  switch (semantic->GetKind()) {
  case hlsl::Semantic::Kind::ClipDistance:
    typeMap = asInput ? &inClipType : &outClipType;
    break;
  case hlsl::Semantic::Kind::CullDistance:
    typeMap = asInput ? &inCullType : &outCullType;
    isCull = true;
    break;
  default:
    // Annotated with something other than SV_ClipDistance or SV_CullDistance.
    // We don't care about such cases.
    return true;
  }

  // Parameters marked as inout has reference type.
  if (baseType->isReferenceType())
    baseType = baseType->getPointeeType();

  if (baseType->isFloatingType() || hlsl::IsHLSLVecType(baseType)) {
    (*typeMap)[semanticIndex] = baseType;
    return true;
  }

  if (baseType->isConstantArrayType()) {
    if (shaderModel.IsHS() || shaderModel.IsDS() || shaderModel.IsGS()) {
      // Ignore the outermost arrayness and check the inner type to be
      // (vector of) floats

      const auto *arrayType = astContext.getAsConstantArrayType(baseType);

      // TODO: handle extra large array size?
      if (*blockArraySize !=
          static_cast<uint32_t>(arrayType->getSize().getZExtValue())) {
        emitError("inconsistent array size for shader %select{output|input}0 "
                  "variable '%1'",
                  decl->getLocStart())
            << asInput << decl->getName();
        return false;
      }

      const QualType elemType = arrayType->getElementType();

      if (elemType->isFloatingType() || hlsl::IsHLSLVecType(elemType)) {
        (*typeMap)[semanticIndex] = elemType;
        return true;
      }

      emitError("elements for %select{SV_ClipDistance|SV_CullDistance}0 "
                "variable '%1' must be (vector of) floats",
                decl->getLocStart())
          << isCull << decl->getName();
      return false;
    }

    emitError("%select{SV_ClipDistance|SV_CullDistance}0 variable '%1' not "
              "allowed to be of array type",
              decl->getLocStart())
        << isCull << decl->getName();
    return false;
  }

  emitError("incorrect type for %select{SV_ClipDistance|SV_CullDistance}0 "
            "variable '%1'",
            decl->getLocStart())
      << isCull << decl->getName();
  return false;
}

void GlPerVertex::calculateClipCullDistanceArraySize() {
  // Updates the offset map and array size for the given input/output
  // SV_ClipDistance/SV_CullDistance.
  const auto updateSizeAndOffset = [](const SemanticIndexToTypeMap &typeMap,
                                      SemanticIndexToArrayOffsetMap *offsetMap,
                                      uint32_t *totalSize) {
    // If no usage of SV_ClipDistance/SV_CullDistance was recorded,just
    // return. This will keep the size defaulted to 1.
    if (typeMap.empty())
      return;

    *totalSize = 0;

    // Collect all indices and sort them
    llvm::SmallVector<uint32_t, 8> indices;
    for (const auto &kv : typeMap)
      indices.push_back(kv.first);
    std::sort(indices.begin(), indices.end(), std::less<uint32_t>());

    for (uint32_t index : indices) {
      const auto type = typeMap.find(index)->second;
      QualType elemType = {};
      uint32_t count = 0;

      if (TypeTranslator::isScalarType(type)) {
        (*offsetMap)[index] = (*totalSize)++;
      } else if (TypeTranslator::isVectorType(type, &elemType, &count)) {
        (*offsetMap)[index] = *totalSize;
        *totalSize += count;
      } else {
        llvm_unreachable("SV_ClipDistance/SV_CullDistance not float or "
                         "vector of float case sneaked in");
      }
    }
  };

  updateSizeAndOffset(inClipType, &inClipOffset, &inClipArraySize);
  updateSizeAndOffset(inCullType, &inCullOffset, &inCullArraySize);
  updateSizeAndOffset(outClipType, &outClipOffset, &outClipArraySize);
  updateSizeAndOffset(outCullType, &outCullOffset, &outCullArraySize);
}

uint32_t GlPerVertex::createBlockVar(bool asInput, uint32_t arraySize) {
  const llvm::StringRef typeName = "type.gl_PerVertex";
  spv::StorageClass sc = spv::StorageClass::Input;
  llvm::StringRef varName = "gl_PerVertexIn";
  uint32_t clipSize = inClipArraySize;
  uint32_t cullSize = inCullArraySize;

  if (!asInput) {
    sc = spv::StorageClass::Output;
    varName = "gl_PerVertexOut";
    clipSize = outClipArraySize;
    cullSize = outCullArraySize;
  }

  uint32_t typeId =
      typeTranslator.getGlPerVertexStruct(clipSize, cullSize, typeName);

  // Handle the extra arrayness over the block
  if (arraySize != 0) {
    const uint32_t arraySizeId = theBuilder.getConstantUint32(arraySize);
    typeId = theBuilder.getArrayType(typeId, arraySizeId);
  }

  return theBuilder.addStageIOVar(typeId, sc, varName);
}

uint32_t GlPerVertex::createPositionVar(bool asInput) {
  const uint32_t type = theBuilder.getVecType(theBuilder.getFloat32Type(), 4);
  const spv::StorageClass sc =
      asInput ? spv::StorageClass::Input : spv::StorageClass::Output;
  // Special handling here. Requesting Position for input means we are in
  // PS, which should use FragCoord instead of Position.
  assert(asInput ? shaderModel.IsPS() : true);
  const spv::BuiltIn builtin =
      asInput ? spv::BuiltIn::FragCoord : spv::BuiltIn::Position;

  return theBuilder.addStageBuiltinVar(type, sc, builtin);
}

uint32_t GlPerVertex::createClipDistanceVar(bool asInput, uint32_t arraySize) {
  const uint32_t type = theBuilder.getArrayType(
      theBuilder.getFloat32Type(), theBuilder.getConstantUint32(arraySize));
  spv::StorageClass sc =
      asInput ? spv::StorageClass::Input : spv::StorageClass::Output;

  return theBuilder.addStageBuiltinVar(type, sc, spv::BuiltIn::ClipDistance);
}

uint32_t GlPerVertex::createCullDistanceVar(bool asInput, uint32_t arraySize) {
  const uint32_t type = theBuilder.getArrayType(
      theBuilder.getFloat32Type(), theBuilder.getConstantUint32(arraySize));
  spv::StorageClass sc =
      asInput ? spv::StorageClass::Input : spv::StorageClass::Output;

  return theBuilder.addStageBuiltinVar(type, sc, spv::BuiltIn::CullDistance);
}

bool GlPerVertex::tryToAccess(hlsl::SigPoint::Kind sigPointKind,
                              hlsl::Semantic::Kind semanticKind,
                              uint32_t semanticIndex,
                              llvm::Optional<uint32_t> invocationId,
                              uint32_t *value, bool noWriteBack) {
  // invocationId should only be used for HSPCOut.
  assert(invocationId.hasValue() ? sigPointKind == hlsl::SigPoint::Kind::HSCPOut
                                 : true);

  switch (semanticKind) {
  case hlsl::Semantic::Kind::Position:
  case hlsl::Semantic::Kind::ClipDistance:
  case hlsl::Semantic::Kind::CullDistance:
    // gl_PerVertex only cares about these builtins.
    break;
  default:
    return false; // Fall back to the normal path
  }

  switch (sigPointKind) {
  case hlsl::SigPoint::Kind::PSIn:
    // We don't handle stand-alone Position builtin in this class.
    if (semanticKind == hlsl::Semantic::Kind::Position)
      return false; // Fall back to the normal path

    // Fall through

  case hlsl::SigPoint::Kind::HSCPIn:
  case hlsl::SigPoint::Kind::DSCPIn:
  case hlsl::SigPoint::Kind::GSVIn:
    return readField(semanticKind, semanticIndex, value);

  case hlsl::SigPoint::Kind::GSOut:
    // We don't handle stand-alone Position builtin in this class.
    if (semanticKind == hlsl::Semantic::Kind::Position)
      return false; // Fall back to the normal path

    // Fall through

  case hlsl::SigPoint::Kind::VSOut:
  case hlsl::SigPoint::Kind::HSCPOut:
  case hlsl::SigPoint::Kind::DSOut:
    if (noWriteBack)
      return true;

    return writeField(semanticKind, semanticIndex, invocationId, value);
  }

  return false;
}

bool GlPerVertex::tryToAccessPointSize(hlsl::SigPoint::Kind sigPointKind,
                                       llvm::Optional<uint32_t> invocation,
                                       uint32_t *value, bool noWriteBack) {
  switch (sigPointKind) {
  case hlsl::SigPoint::Kind::HSCPIn:
  case hlsl::SigPoint::Kind::DSCPIn:
  case hlsl::SigPoint::Kind::GSVIn:
    *value = readPositionOrPointSize(/*isPosition=*/false);
    return true;
  case hlsl::SigPoint::Kind::VSOut:
  case hlsl::SigPoint::Kind::HSCPOut:
  case hlsl::SigPoint::Kind::DSOut:
    writePositionOrPointSize(/*isPosition=*/false, invocation, *value);
    return true;
  }

  return false; // Fall back to normal path: GSOut
}

uint32_t GlPerVertex::readPositionOrPointSize(bool isPosition) const {
  // We do not handle stand-alone Position/PointSize builtin here.
  assert(inIsGrouped);

  // The PointSize builtin is always of float type.
  // The Position builtin is always of float4 type.
  const uint32_t f32Type = theBuilder.getFloat32Type();
  const uint32_t fieldType =
      isPosition ? theBuilder.getVecType(f32Type, 4) : f32Type;
  const uint32_t ptrType =
      theBuilder.getPointerType(fieldType, spv::StorageClass::Input);
  const uint32_t fieldIndex = theBuilder.getConstantUint32(isPosition ? 0 : 1);

  if (inArraySize == 0) {
    // The input builtin block is a single block. Only need one index to
    // locate the Position/PointSize builtin.
    const uint32_t ptr =
        theBuilder.createAccessChain(ptrType, inBlockVar, {fieldIndex});
    return theBuilder.createLoad(fieldType, ptr);
  }

  // The input builtin block is an array of blocks, which means we need to
  // read an array of float4 from an array of structs.

  llvm::SmallVector<uint32_t, 8> elements;
  for (uint32_t i = 0; i < inArraySize; ++i) {
    const uint32_t arrayIndex = theBuilder.getConstantUint32(i);
    // Get pointer into the array of structs. We need two indices to locate
    // the Position/PointSize builtin now: the first one is the array index,
    // and the second one is the struct index.
    const uint32_t ptr = theBuilder.createAccessChain(ptrType, inBlockVar,
                                                      {arrayIndex, fieldIndex});
    elements.push_back(theBuilder.createLoad(fieldType, ptr));
  }
  // Construct a new array of float4/float for the Position/PointSize builtins
  const uint32_t arrayType = theBuilder.getArrayType(
      fieldType, theBuilder.getConstantUint32(inArraySize));
  return theBuilder.createCompositeConstruct(arrayType, elements);
}

uint32_t GlPerVertex::readClipCullArrayAsType(bool isClip, uint32_t offset,
                                              QualType asType) const {
  const uint32_t clipCullIndex = isClip ? 2 : 3;

  // The ClipDistance/CullDistance is always an float array. We are accessing
  // it using pointers, which should be of pointer to float type.
  const uint32_t f32Type = theBuilder.getFloat32Type();
  const uint32_t ptrType =
      theBuilder.getPointerType(f32Type, spv::StorageClass::Input);

  if (inArraySize == 0) {
    // The input builtin block is a single block. Only need two indices to
    // locate the array segment for this SV_ClipDistance/SV_CullDistance
    // variable: one is the index in the gl_PerVertex struct, the other is
    // the start offset within the float array.
    QualType elemType = {};
    uint32_t count = {};

    if (TypeTranslator::isScalarType(asType)) {
      const uint32_t offsetId = theBuilder.getConstantUint32(offset);
      uint32_t ptr = 0;

      if (inIsGrouped) {
        ptr = theBuilder.createAccessChain(
            ptrType, inBlockVar,
            {theBuilder.getConstantUint32(clipCullIndex), offsetId});
      } else {
        ptr = theBuilder.createAccessChain(
            ptrType, clipCullIndex == 2 ? inClipVar : inCullVar, {offsetId});
      }
      return theBuilder.createLoad(f32Type, ptr);
    }

    if (TypeTranslator::isVectorType(asType, &elemType, &count)) {
      // The target SV_ClipDistance/SV_CullDistance variable is of vector
      // type, then we need to construct a vector out of float array elements.
      llvm::SmallVector<uint32_t, 4> elements;
      for (uint32_t i = 0; i < count; ++i) {
        // Read elements sequentially from the float array
        const uint32_t offsetId = theBuilder.getConstantUint32(offset + i);
        uint32_t ptr = 0;

        if (inIsGrouped) {
          ptr = theBuilder.createAccessChain(
              ptrType, inBlockVar,
              {theBuilder.getConstantUint32(clipCullIndex), offsetId});
        } else {
          ptr = theBuilder.createAccessChain(
              ptrType, clipCullIndex == 2 ? inClipVar : inCullVar, {offsetId});
        }
        elements.push_back(theBuilder.createLoad(f32Type, ptr));
      }
      return theBuilder.createCompositeConstruct(
          theBuilder.getVecType(f32Type, count), elements);
    }

    llvm_unreachable("SV_ClipDistance/SV_CullDistance not float or vector of "
                     "float case sneaked in");
  }

  // The input builtin block is an array of block, which means we need to
  // return an array of ClipDistance/CullDistance values from an array of
  // struct. For this case, we need three indices to locate the element to
  // read: the first one for indexing into the block array, the second one
  // for indexing into the gl_PerVertex struct, and the third one for reading
  // the correct element in the float array for ClipDistance/CullDistance.

  assert(inIsGrouped); // Separated builtins won't have the extra arrayness.

  llvm::SmallVector<uint32_t, 8> arrayElements;
  QualType elemType = {};
  uint32_t count = {};
  uint32_t arrayType = {};
  uint32_t arraySize = theBuilder.getConstantUint32(inArraySize);

  if (TypeTranslator::isScalarType(asType)) {
    arrayType = theBuilder.getArrayType(f32Type, arraySize);
    for (uint32_t i = 0; i < inArraySize; ++i) {
      const uint32_t ptr = theBuilder.createAccessChain(
          ptrType, inBlockVar,
          {theBuilder.getConstantUint32(i), // Block array index
           theBuilder.getConstantUint32(clipCullIndex),
           theBuilder.getConstantUint32(offset)});
      arrayElements.push_back(theBuilder.createLoad(f32Type, ptr));
    }
  } else if (TypeTranslator::isVectorType(asType, &elemType, &count)) {
    arrayType = theBuilder.getArrayType(theBuilder.getVecType(f32Type, count),
                                        arraySize);
    for (uint32_t i = 0; i < inArraySize; ++i) {
      // For each gl_PerVertex block, we need to read a vector from it.
      llvm::SmallVector<uint32_t, 4> vecElements;
      for (uint32_t j = 0; j < count; ++j) {
        const uint32_t ptr = theBuilder.createAccessChain(
            ptrType, inBlockVar,
            {theBuilder.getConstantUint32(i), // Block array index
             theBuilder.getConstantUint32(clipCullIndex),
             // Read elements sequentially from the float array
             theBuilder.getConstantUint32(offset + j)});
        vecElements.push_back(theBuilder.createLoad(f32Type, ptr));
      }
      arrayElements.push_back(theBuilder.createCompositeConstruct(
          theBuilder.getVecType(f32Type, count), vecElements));
    }
  } else {
    llvm_unreachable("SV_ClipDistance/SV_CullDistance not float or vector of "
                     "float case sneaked in");
  }

  return theBuilder.createCompositeConstruct(arrayType, arrayElements);
};

bool GlPerVertex::readField(hlsl::Semantic::Kind semanticKind,
                            uint32_t semanticIndex, uint32_t *value) {
  switch (semanticKind) {
  case hlsl::Semantic::Kind::Position:
    *value = readPositionOrPointSize(/*isPosition=*/true);
    return true;
  case hlsl::Semantic::Kind::ClipDistance: {
    const auto offsetIter = inClipOffset.find(semanticIndex);
    const auto typeIter = inClipType.find(semanticIndex);
    // We should have recorded all these semantics before.
    assert(offsetIter != inClipOffset.end());
    assert(typeIter != inClipType.end());
    *value = readClipCullArrayAsType(/*isClip=*/true, offsetIter->second,
                                     typeIter->second);
    return true;
  }
  case hlsl::Semantic::Kind::CullDistance: {
    const auto offsetIter = inCullOffset.find(semanticIndex);
    const auto typeIter = inCullType.find(semanticIndex);
    // We should have recorded all these semantics before.
    assert(offsetIter != inCullOffset.end());
    assert(typeIter != inCullType.end());
    *value = readClipCullArrayAsType(/*isClip=*/false, offsetIter->second,
                                     typeIter->second);
    return true;
  }
  }
  return false;
}

void GlPerVertex::writePositionOrPointSize(
    bool isPosition, llvm::Optional<uint32_t> invocationId, uint32_t value) {
  // We do not handle stand-alone Position/PointSize builtin here.
  assert(outIsGrouped);

  // The Position builtin is always of float4 type.
  // The PointSize builtin is always of float type.
  const uint32_t f32Type = theBuilder.getFloat32Type();
  const uint32_t fieldType =
      isPosition ? theBuilder.getVecType(f32Type, 4) : f32Type;
  const uint32_t ptrType =
      theBuilder.getPointerType(fieldType, spv::StorageClass::Output);
  const uint32_t fieldIndex = theBuilder.getConstantUint32(isPosition ? 0 : 1);

  if (outArraySize == 0) {
    // The input builtin block is a single block. Only need one index to
    // locate the Position/PointSize builtin.
    const uint32_t ptr =
        theBuilder.createAccessChain(ptrType, outBlockVar, {fieldIndex});

    if (isPosition && invertY) {
      if (shaderModel.IsVS() || shaderModel.IsDS()) {
        const auto oldY =
            theBuilder.createCompositeExtract(f32Type, value, {1});
        const auto newY =
            theBuilder.createUnaryOp(spv::Op::OpFNegate, f32Type, oldY);
        value = theBuilder.createCompositeInsert(fieldType, value, {1}, newY);
      }
    }

    theBuilder.createStore(ptr, value);
    return;
  }

  // Writing to an array only happens in HSCPOut.
  assert(shaderModel.IsHS());
  // And we are only writing to the array element with InvocationId as index.
  assert(invocationId.hasValue());

  // The input builtin block is an array of blocks, which means we need to
  // to write a float4 to each gl_PerVertex in the array.

  const uint32_t arrayIndex = invocationId.getValue();
  // Get pointer into the array of structs. We need two indices to locate
  // the Position/PointSize builtin now: the first one is the array index,
  // and the second one is the struct index.
  const uint32_t ptr = theBuilder.createAccessChain(ptrType, outBlockVar,
                                                    {arrayIndex, fieldIndex});

  theBuilder.createStore(ptr, value);
}

void GlPerVertex::writeClipCullArrayFromType(
    llvm::Optional<uint32_t> invocationId, bool isClip, uint32_t offset,
    QualType fromType, uint32_t fromValue) const {
  const uint32_t clipCullIndex = isClip ? 2 : 3;

  // The ClipDistance/CullDistance is always an float array. We are accessing
  // it using pointers, which should be of pointer to float type.
  const uint32_t f32Type = theBuilder.getFloat32Type();
  const uint32_t ptrType =
      theBuilder.getPointerType(f32Type, spv::StorageClass::Output);

  if (outArraySize == 0) {
    // The input builtin block is a single block. Only need two indices to
    // locate the array segment for this SV_ClipDistance/SV_CullDistance
    // variable: one is the index in the gl_PerVertex struct, the other is
    // the start offset within the float array.
    QualType elemType = {};
    uint32_t count = {};

    if (TypeTranslator::isScalarType(fromType)) {
      const uint32_t offsetId = theBuilder.getConstantUint32(offset);
      uint32_t ptr = 0;

      if (outIsGrouped) {
        ptr = theBuilder.createAccessChain(
            ptrType, outBlockVar,
            {theBuilder.getConstantUint32(clipCullIndex), offsetId});
      } else {
        ptr = theBuilder.createAccessChain(
            ptrType, clipCullIndex == 2 ? outClipVar : outCullVar, {offsetId});
      }
      theBuilder.createStore(ptr, fromValue);
      return;
    }

    if (TypeTranslator::isVectorType(fromType, &elemType, &count)) {
      // The target SV_ClipDistance/SV_CullDistance variable is of vector
      // type. We need to write each component in the vector out.
      for (uint32_t i = 0; i < count; ++i) {
        // Write elements sequentially into the float array
        const uint32_t offsetId = theBuilder.getConstantUint32(offset + i);
        uint32_t ptr = 0;

        if (outIsGrouped) {
          ptr = theBuilder.createAccessChain(
              ptrType, outBlockVar,
              {theBuilder.getConstantUint32(clipCullIndex), offsetId});
        } else {
          ptr = theBuilder.createAccessChain(
              ptrType, clipCullIndex == 2 ? outClipVar : outCullVar,
              {offsetId});
        }
        const uint32_t subValue =
            theBuilder.createCompositeExtract(f32Type, fromValue, {i});
        theBuilder.createStore(ptr, subValue);
      }
      return;
    }

    llvm_unreachable("SV_ClipDistance/SV_CullDistance not float or vector of "
                     "float case sneaked in");
    return;
  }

  assert(outIsGrouped); // Separated builtins won't have the extra arrayness.

  // Writing to an array only happens in HSCPOut.
  assert(shaderModel.IsHS());
  // And we are only writing to the array element with InvocationId as index.
  assert(invocationId.hasValue());

  // The output builtin block is an array of block, which means we need to
  // write an array of ClipDistance/CullDistance values into an array of
  // struct. For this case, we need three indices to locate the position to
  // write: the first one for indexing into the block array, the second one
  // for indexing into the gl_PerVertex struct, and the third one for the
  // correct element in the float array for ClipDistance/CullDistance.

  uint32_t arrayIndex = invocationId.getValue();
  QualType elemType = {};
  uint32_t count = {};

  if (TypeTranslator::isScalarType(fromType)) {
    const uint32_t ptr = theBuilder.createAccessChain(
        ptrType, outBlockVar,
        {arrayIndex, // Block array index
         theBuilder.getConstantUint32(clipCullIndex),
         theBuilder.getConstantUint32(offset)});
    theBuilder.createStore(ptr, fromValue);
    return;
  }

  if (TypeTranslator::isVectorType(fromType, &elemType, &count)) {
    // For each gl_PerVertex block, we need to write a vector into it.
    for (uint32_t i = 0; i < count; ++i) {
      const uint32_t ptr = theBuilder.createAccessChain(
          ptrType, outBlockVar,
          {arrayIndex, // Block array index
           theBuilder.getConstantUint32(clipCullIndex),
           // Write elements sequentially into the float array
           theBuilder.getConstantUint32(offset + i)});
      const uint32_t subValue =
          theBuilder.createCompositeExtract(f32Type, fromValue, {i});
      theBuilder.createStore(ptr, subValue);
    }
    return;
  }

  llvm_unreachable("SV_ClipDistance/SV_CullDistance not float or vector of "
                   "float case sneaked in");
};

bool GlPerVertex::writeField(hlsl::Semantic::Kind semanticKind,
                             uint32_t semanticIndex,
                             llvm::Optional<uint32_t> invocationId,
                             uint32_t *value) {
  // Similar to the writing logic in DeclResultIdMapper::createStageVars():
  //
  // Unlike reading, which may require us to read stand-alone builtins and
  // stage input variables and compose an array of structs out of them,
  // it happens that we don't need to write an array of structs in a bunch
  // for all shader stages:
  //
  // * VS: output is a single struct, without extra arrayness
  // * HS: output is an array of structs, with extra arrayness,
  //       but we only write to the struct at the InvocationID index
  // * DS: output is a single struct, without extra arrayness
  // * GS: output is controlled by OpEmitVertex, one vertex per time
  //
  // The interesting shader stage is HS. We need the InvocationID to write
  // out the value to the correct array element.
  switch (semanticKind) {
  case hlsl::Semantic::Kind::Position:
    writePositionOrPointSize(/*isPosition=*/true, invocationId, *value);
    return true;
  case hlsl::Semantic::Kind::ClipDistance: {
    const auto offsetIter = outClipOffset.find(semanticIndex);
    const auto typeIter = outClipType.find(semanticIndex);
    // We should have recorded all these semantics before.
    assert(offsetIter != outClipOffset.end());
    assert(typeIter != outClipType.end());
    writeClipCullArrayFromType(invocationId, /*isClip=*/true,
                               offsetIter->second, typeIter->second, *value);
    return true;
  }
  case hlsl::Semantic::Kind::CullDistance: {
    const auto offsetIter = outCullOffset.find(semanticIndex);
    const auto typeIter = outCullType.find(semanticIndex);
    // We should have recorded all these semantics before.
    assert(offsetIter != outCullOffset.end());
    assert(typeIter != outCullType.end());
    writeClipCullArrayFromType(invocationId, /*isClip=*/false,
                               offsetIter->second, typeIter->second, *value);
    return true;
  }
  }
  return false;
}

} // end namespace spirv
} // end namespace clang
