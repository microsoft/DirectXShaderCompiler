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
constexpr uint32_t gClipDistanceIndex = 0;
constexpr uint32_t gCullDistanceIndex = 1;

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
                         ModuleBuilder &builder, TypeTranslator &translator)
    : shaderModel(sm), astContext(context), theBuilder(builder),
      typeTranslator(translator), inClipVar(0), inCullVar(0), outClipVar(0),
      outCullVar(0), inArraySize(0), outArraySize(0), inClipArraySize(1),
      outClipArraySize(1), inCullArraySize(1), outCullArraySize(1),
      inSemanticStrs(2, ""), outSemanticStrs(2, "") {}

void GlPerVertex::generateVars(uint32_t inArrayLen, uint32_t outArrayLen) {
  inArraySize = inArrayLen;
  outArraySize = outArrayLen;

  if (!inClipType.empty())
    inClipVar = createClipCullDistanceVar(/*asInput=*/true, /*isClip=*/true,
                                          inClipArraySize);
  if (!inCullType.empty())
    inCullVar = createClipCullDistanceVar(/*asInput=*/true, /*isClip=*/false,
                                          inCullArraySize);
  if (!outClipType.empty())
    outClipVar = createClipCullDistanceVar(/*asInput=*/false, /*isClip=*/true,
                                           outClipArraySize);
  if (!outCullType.empty())
    outCullVar = createClipCullDistanceVar(/*asInput=*/false, /*isClip=*/false,
                                           outCullArraySize);
}

llvm::SmallVector<uint32_t, 2> GlPerVertex::getStageInVars() const {
  llvm::SmallVector<uint32_t, 2> vars;

  if (inClipVar)
    vars.push_back(inClipVar);
  if (inCullVar)
    vars.push_back(inCullVar);

  return vars;
}

llvm::SmallVector<uint32_t, 2> GlPerVertex::getStageOutVars() const {
  llvm::SmallVector<uint32_t, 2> vars;

  if (outClipVar)
    vars.push_back(outClipVar);
  if (outCullVar)
    vars.push_back(outCullVar);

  return vars;
}

void GlPerVertex::requireCapabilityIfNecessary() {
  if (!inClipType.empty() || !outClipType.empty())
    theBuilder.requireCapability(spv::Capability::ClipDistance);

  if (!inCullType.empty() || !outCullType.empty())
    theBuilder.requireCapability(spv::Capability::CullDistance);
}

bool GlPerVertex::recordGlPerVertexDeclFacts(const DeclaratorDecl *decl,
                                             bool asInput) {
  const QualType type = getTypeOrFnRetType(decl);

  if (type->isVoidType())
    return true;

  return doGlPerVertexFacts(decl, type, asInput);
}

bool GlPerVertex::doGlPerVertexFacts(const DeclaratorDecl *decl,
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
        if (!doGlPerVertexFacts(field, field->getType(), asInput))
          return false;
      }
      return true;
    }

    // For these HS/DS/GS specific data types, semantic strings are attached
    // to the underlying struct's fields.
    if (hlsl::IsHLSLInputPatchType(baseType)) {
      return doGlPerVertexFacts(
          decl, hlsl::GetHLSLInputPatchElementType(baseType), asInput);
    }
    if (hlsl::IsHLSLOutputPatchType(baseType)) {
      return doGlPerVertexFacts(
          decl, hlsl::GetHLSLOutputPatchElementType(baseType), asInput);
    }

    if (hlsl::IsHLSLStreamOutputType(baseType)) {
      return doGlPerVertexFacts(
          decl, hlsl::GetHLSLOutputPatchElementType(baseType), asInput);
    }
    if (hasGSPrimitiveTypeQualifier(decl)) {
      // GS inputs have an additional arrayness that we should remove to check
      // the underlying type instead.
      baseType = astContext.getAsConstantArrayType(baseType)->getElementType();
      return doGlPerVertexFacts(decl, baseType, asInput);
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
  auto *semanticStrs = asInput ? &inSemanticStrs : &outSemanticStrs;
  uint32_t index = kSemanticStrCount;

  switch (semantic->GetKind()) {
  case hlsl::Semantic::Kind::ClipDistance:
    typeMap = asInput ? &inClipType : &outClipType;
    index = gClipDistanceIndex;
    break;
  case hlsl::Semantic::Kind::CullDistance:
    typeMap = asInput ? &inCullType : &outCullType;
    isCull = true;
    index = gCullDistanceIndex;
    break;
  }

  // Remember the semantic strings provided by the developer so that we can
  // emit OpDecorate* instructions properly for them
  if (index < kSemanticStrCount) {
    if ((*semanticStrs)[index].empty())
      (*semanticStrs)[index] = semanticStr;
    // We can have multiple ClipDistance/CullDistance semantics mapping to the
    // same variable. For those cases, it is not appropriate to use any one of
    // them as the semantic. Use the standard one without index.
    else if (index == gClipDistanceIndex)
      (*semanticStrs)[index] = "SV_ClipDistance";
    else if (index == gCullDistanceIndex)
      (*semanticStrs)[index] = "SV_CullDistance";
  }

  if (index < gClipDistanceIndex || index > gCullDistanceIndex) {
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

uint32_t GlPerVertex::createClipCullDistanceVar(bool asInput, bool isClip,
                                                uint32_t arraySize) {
  uint32_t type = theBuilder.getArrayType(
      theBuilder.getFloat32Type(), theBuilder.getConstantUint32(arraySize));
  if (asInput && inArraySize != 0) {
    type = theBuilder.getArrayType(type,
                                   theBuilder.getConstantUint32(inArraySize));
  } else if (!asInput && outArraySize != 0) {
    type = theBuilder.getArrayType(type,
                                   theBuilder.getConstantUint32(outArraySize));
  }

  spv::StorageClass sc =
      asInput ? spv::StorageClass::Input : spv::StorageClass::Output;

  auto id = theBuilder.addStageBuiltinVar(type, sc,
                                          isClip ? spv::BuiltIn::ClipDistance
                                                 : spv::BuiltIn::CullDistance);
  const auto index = isClip ? gClipDistanceIndex : gCullDistanceIndex;
  theBuilder.decorateHlslSemantic(id, asInput ? inSemanticStrs[index]
                                              : outSemanticStrs[index]);
  return id;
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
  case hlsl::Semantic::Kind::ClipDistance:
  case hlsl::Semantic::Kind::CullDistance:
    // gl_PerVertex only cares about these builtins.
    break;
  default:
    return false; // Fall back to the normal path
  }

  switch (sigPointKind) {
  case hlsl::SigPoint::Kind::PSIn:
  case hlsl::SigPoint::Kind::HSCPIn:
  case hlsl::SigPoint::Kind::DSCPIn:
  case hlsl::SigPoint::Kind::GSVIn:
    return readField(semanticKind, semanticIndex, value);

  case hlsl::SigPoint::Kind::GSOut:
  case hlsl::SigPoint::Kind::VSOut:
  case hlsl::SigPoint::Kind::HSCPOut:
  case hlsl::SigPoint::Kind::DSOut:
    if (noWriteBack)
      return true;

    return writeField(semanticKind, semanticIndex, invocationId, value);
  }

  return false;
}

uint32_t GlPerVertex::readClipCullArrayAsType(bool isClip, uint32_t offset,
                                              QualType asType) const {
  const uint32_t clipCullVar = isClip ? inClipVar : inCullVar;

  // The ClipDistance/CullDistance is always an float array. We are accessing
  // it using pointers, which should be of pointer to float type.
  const uint32_t f32Type = theBuilder.getFloat32Type();
  const uint32_t ptrType =
      theBuilder.getPointerType(f32Type, spv::StorageClass::Input);

  if (inArraySize == 0) {
    // The input builtin does not have extra arrayness. Only need one index
    // to locate the array segment for this SV_ClipDistance/SV_CullDistance
    // variable: the start offset within the float array.
    QualType elemType = {};
    uint32_t count = {};

    if (TypeTranslator::isScalarType(asType)) {
      const uint32_t offsetId = theBuilder.getConstantUint32(offset);
      const uint32_t ptr =
          theBuilder.createAccessChain(ptrType, clipCullVar, {offsetId});
      return theBuilder.createLoad(f32Type, ptr);
    }

    if (TypeTranslator::isVectorType(asType, &elemType, &count)) {
      // The target SV_ClipDistance/SV_CullDistance variable is of vector
      // type, then we need to construct a vector out of float array elements.
      llvm::SmallVector<uint32_t, 4> elements;
      for (uint32_t i = 0; i < count; ++i) {
        // Read elements sequentially from the float array
        const uint32_t offsetId = theBuilder.getConstantUint32(offset + i);
        const uint32_t ptr =
            theBuilder.createAccessChain(ptrType, clipCullVar, {offsetId});
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

  llvm::SmallVector<uint32_t, 8> arrayElements;
  QualType elemType = {};
  uint32_t count = {};
  uint32_t arrayType = {};
  uint32_t arraySize = theBuilder.getConstantUint32(inArraySize);

  if (TypeTranslator::isScalarType(asType)) {
    arrayType = theBuilder.getArrayType(f32Type, arraySize);
    for (uint32_t i = 0; i < inArraySize; ++i) {
      const uint32_t ptr = theBuilder.createAccessChain(
          ptrType, clipCullVar,
          {theBuilder.getConstantUint32(i), // Block array index
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
            ptrType, clipCullVar,
            // Block array index
            {theBuilder.getConstantUint32(i),
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
}

bool GlPerVertex::readField(hlsl::Semantic::Kind semanticKind,
                            uint32_t semanticIndex, uint32_t *value) {
  switch (semanticKind) {
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

void GlPerVertex::writeClipCullArrayFromType(
    llvm::Optional<uint32_t> invocationId, bool isClip, uint32_t offset,
    QualType fromType, uint32_t fromValue) const {
  const uint32_t clipCullVar = isClip ? outClipVar : outCullVar;

  // The ClipDistance/CullDistance is always an float array. We are accessing
  // it using pointers, which should be of pointer to float type.
  const uint32_t f32Type = theBuilder.getFloat32Type();
  const uint32_t ptrType =
      theBuilder.getPointerType(f32Type, spv::StorageClass::Output);

  if (outArraySize == 0) {
    // The output builtin does not have extra arrayness. Only need one index
    // to locate the array segment for this SV_ClipDistance/SV_CullDistance
    // variable: the start offset within the float array.
    QualType elemType = {};
    uint32_t count = {};

    if (TypeTranslator::isScalarType(fromType)) {
      const uint32_t offsetId = theBuilder.getConstantUint32(offset);
      const uint32_t ptr =
          theBuilder.createAccessChain(ptrType, clipCullVar, {offsetId});
      theBuilder.createStore(ptr, fromValue);
      return;
    }

    if (TypeTranslator::isVectorType(fromType, &elemType, &count)) {
      // The target SV_ClipDistance/SV_CullDistance variable is of vector
      // type. We need to write each component in the vector out.
      for (uint32_t i = 0; i < count; ++i) {
        // Write elements sequentially into the float array
        const uint32_t offsetId = theBuilder.getConstantUint32(offset + i);
        const uint32_t ptr =
            theBuilder.createAccessChain(ptrType, clipCullVar, {offsetId});
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
    const uint32_t ptr =
        theBuilder.createAccessChain(ptrType, clipCullVar,
                                     {arrayIndex, // Block array index
                                      theBuilder.getConstantUint32(offset)});
    theBuilder.createStore(ptr, fromValue);
    return;
  }

  if (TypeTranslator::isVectorType(fromType, &elemType, &count)) {
    // For each gl_PerVertex block, we need to write a vector into it.
    for (uint32_t i = 0; i < count; ++i) {
      const uint32_t ptr = theBuilder.createAccessChain(
          ptrType, clipCullVar,
          // Block array index
          {arrayIndex,
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
}

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
