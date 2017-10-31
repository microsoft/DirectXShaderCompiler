//===--- DeclResultIdMapper.cpp - DeclResultIdMapper impl --------*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "DeclResultIdMapper.h"

#include <algorithm>
#include <cstring>
#include <unordered_map>

#include "dxc/HLSL/DxilConstants.h"
#include "dxc/HLSL/DxilTypeSystem.h"
#include "clang/AST/Expr.h"
#include "clang/AST/HlslTypes.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "llvm/ADT/SmallBitVector.h"
#include "llvm/ADT/StringSet.h"

namespace clang {
namespace spirv {

namespace {
/// \brief Returns true if the given decl has a semantic string attached and
/// writes the info to *semanticStr, *semantic, and *semanticIndex.
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

/// \brief Returns the stage variable's register assignment for the given Decl.
const hlsl::RegisterAssignment *getResourceBinding(const NamedDecl *decl) {
  for (auto *annotation : decl->getUnusualAnnotations()) {
    if (auto *reg = dyn_cast<hlsl::RegisterAssignment>(annotation)) {
      return reg;
    }
  }
  return nullptr;
}

/// \brief Returns the resource category for the given type.
ResourceVar::Category getResourceCategory(QualType type) {
  if (TypeTranslator::isTexture(type) || TypeTranslator::isRWTexture(type))
    return ResourceVar::Category::Image;
  if (TypeTranslator::isSampler(type))
    return ResourceVar::Category::Sampler;
  return ResourceVar::Category::Other;
}

/// \brief Returns true if the given declaration has a primitive type qualifier.
/// Returns false otherwise.
bool hasGSPrimitiveTypeQualifier(const Decl *decl) {
  return (decl->hasAttr<HLSLTriangleAttr>() ||
          decl->hasAttr<HLSLTriangleAdjAttr>() ||
          decl->hasAttr<HLSLPointAttr>() || decl->hasAttr<HLSLLineAdjAttr>() ||
          decl->hasAttr<HLSLLineAttr>());
}

/// \brief Deduces the parameter qualifier for the given decl.
hlsl::DxilParamInputQual deduceParamQual(const DeclaratorDecl *decl,
                                         bool asInput) {
  const auto type = decl->getType();

  if (hlsl::IsHLSLInputPatchType(type))
    return hlsl::DxilParamInputQual::InputPatch;
  if (hlsl::IsHLSLOutputPatchType(type))
    return hlsl::DxilParamInputQual::OutputPatch;
  // TODO: Add support for multiple output streams.
  if (hlsl::IsHLSLStreamOutputType(type))
    return hlsl::DxilParamInputQual::OutStream0;

  // The inputs to the geometry shader that have a primitive type qualifier
  // must use 'InputPrimitive'.
  if (hasGSPrimitiveTypeQualifier(decl))
    return hlsl::DxilParamInputQual::InputPrimitive;

  return asInput ? hlsl::DxilParamInputQual::In : hlsl::DxilParamInputQual::Out;
}

const hlsl::SigPoint *deduceSigPoint(const DeclaratorDecl *decl, bool asInput,
                                     const hlsl::ShaderModel::Kind kind,
                                     bool isPatchConstant) {
  return hlsl::SigPoint::GetSigPoint(hlsl::SigPointFromInputQual(
      deduceParamQual(decl, asInput), kind, isPatchConstant));
}

/// Returns the type of the given decl. If the given decl is a FunctionDecl,
/// returns its result type.
inline QualType getTypeOrFnRetType(const DeclaratorDecl *decl) {
  if (const auto *funcDecl = dyn_cast<FunctionDecl>(decl)) {
    return funcDecl->getReturnType();
  }
  return decl->getType();
}
} // anonymous namespace

GlPerVertex::GlPerVertex(const hlsl::ShaderModel &sm, ASTContext &context,
                         ModuleBuilder &builder, TypeTranslator &translator)
    : shaderModel(sm), astContext(context), theBuilder(builder),
      typeTranslator(translator), inIsGrouped(true), outIsGrouped(true),
      inBlockVar(0), outBlockVar(0), inClipVar(0), inCullVar(0), outClipVar(0),
      outCullVar(0), inArraySize(0), outArraySize(0), inClipArraySize(1),
      outClipArraySize(1), inCullArraySize(1), outCullArraySize(1) {}

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
      const auto *structDecl =
          cast<RecordType>(baseType.getTypePtr())->getDecl();
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

    emitError("semantic string missing for shader %select{output|input}0 "
              "variable '%1'",
              decl->getLocStart())
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

    // TODO: We are doing extra queries here. Should also report error if the
    // index is > 9.
    for (uint32_t i = 0; i < 10; ++i) {
      const auto found = typeMap.find(i);
      if (found != typeMap.end()) {
        QualType elemType = {};
        uint32_t count = 0;

        if (TypeTranslator::isScalarType(found->second)) {
          (*offsetMap)[i] = (*totalSize)++;
        } else if (TypeTranslator::isVectorType(found->second, &elemType,
                                                &count)) {
          (*offsetMap)[i] = *totalSize;
          *totalSize += count;
        } else {
          llvm_unreachable("SV_ClipDistance/SV_CullDistance not float or "
                           "vector of float case sneaked in");
        }
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

bool GlPerVertex::tryToAccess(hlsl::Semantic::Kind semanticKind,
                              uint32_t semanticIndex,
                              llvm::Optional<uint32_t> invocationId,
                              uint32_t *value,
                              hlsl::SigPoint::Kind sigPointKind) {
  // invocationId should only be used for HSPCOut.
  assert(invocationId.hasValue() ? sigPointKind == hlsl::SigPoint::Kind::HSCPOut
                                 : true);

  switch (sigPointKind) {
  case hlsl::SigPoint::Kind::HSCPIn:
  case hlsl::SigPoint::Kind::DSCPIn:
  case hlsl::SigPoint::Kind::GSVIn:
    return readField(semanticKind, semanticIndex, value);
  case hlsl::SigPoint::Kind::PSIn:
    // We don't handle stand-alone Position builtin in this class.
    return semanticKind == hlsl::Semantic::Kind::Position
               ? 0 // Fall back to the normal path
               : readField(semanticKind, semanticIndex, value);
  case hlsl::SigPoint::Kind::VSOut:
  case hlsl::SigPoint::Kind::HSCPOut:
  case hlsl::SigPoint::Kind::DSOut:
    return writeField(semanticKind, semanticIndex, invocationId, value);
  case hlsl::SigPoint::Kind::GSOut:
    // We don't handle stand-alone Position builtin in this class.
    return semanticKind == hlsl::Semantic::Kind::Position
               ? 0 // Fall back to the normal path
               : writeField(semanticKind, semanticIndex, invocationId, value);
  }

  return false;
}

uint32_t GlPerVertex::readPosition() const {
  assert(inIsGrouped); // We do not handle stand-alone Position builtin here.

  // The Position builtin is always of float4 type.
  const uint32_t fieldType =
      theBuilder.getVecType(theBuilder.getFloat32Type(), 4);
  const uint32_t ptrType =
      theBuilder.getPointerType(fieldType, spv::StorageClass::Input);
  const uint32_t fieldIndex = theBuilder.getConstantUint32(0);

  if (inArraySize == 0) {
    // The input builtin block is a single block. Only need one index to
    // locate the Position builtin.
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
    // the Position builtin now: the first one is the array index, and the
    // second one is the struct index.
    const uint32_t ptr = theBuilder.createAccessChain(ptrType, inBlockVar,
                                                      {arrayIndex, fieldIndex});
    elements.push_back(theBuilder.createLoad(fieldType, ptr));
  }
  // Construct a new array of float4 for the Position builtins
  const uint32_t arrayType = theBuilder.getArrayType(
      fieldType, theBuilder.getConstantUint32(inArraySize));
  return theBuilder.createCompositeConstruct(arrayType, elements);
}

uint32_t GlPerVertex::readClipCullArrayAsType(uint32_t clipCullIndex,
                                              uint32_t offset,
                                              QualType asType) const {
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
    *value = readPosition();
    return true;
  case hlsl::Semantic::Kind::ClipDistance: {
    const auto offsetIter = inClipOffset.find(semanticIndex);
    const auto typeIter = inClipType.find(semanticIndex);
    // We should have recorded all these semantics before.
    assert(offsetIter != inClipOffset.end());
    assert(typeIter != inClipType.end());
    *value = readClipCullArrayAsType(2, offsetIter->second, typeIter->second);
    return true;
  }
  case hlsl::Semantic::Kind::CullDistance: {
    const auto offsetIter = inCullOffset.find(semanticIndex);
    const auto typeIter = inCullType.find(semanticIndex);
    // We should have recorded all these semantics before.
    assert(offsetIter != inCullOffset.end());
    assert(typeIter != inCullType.end());
    *value = readClipCullArrayAsType(3, offsetIter->second, typeIter->second);
    return true;
  }
  }
  return false;
}

void GlPerVertex::writePosition(llvm::Optional<uint32_t> invocationId,
                                uint32_t value) const {
  assert(outIsGrouped); // We do not handle stand-alone Position builtin here.

  // The Position builtin is always of float4 type.
  const uint32_t fieldType =
      theBuilder.getVecType(theBuilder.getFloat32Type(), 4);
  const uint32_t ptrType =
      theBuilder.getPointerType(fieldType, spv::StorageClass::Output);
  const uint32_t fieldIndex = theBuilder.getConstantUint32(0);

  if (outArraySize == 0) {
    // The input builtin block is a single block. Only need one index to
    // locate the Position builtin.
    const uint32_t ptr =
        theBuilder.createAccessChain(ptrType, outBlockVar, {fieldIndex});
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
  // the Position builtin now: the first one is the array index, and the
  // second one is the struct index.
  const uint32_t ptr = theBuilder.createAccessChain(ptrType, outBlockVar,
                                                    {arrayIndex, fieldIndex});
  theBuilder.createStore(ptr, value);
}

void GlPerVertex::writeClipCullArrayFromType(
    llvm::Optional<uint32_t> invocationId, uint32_t clipCullIndex,
    uint32_t offset, QualType fromType, uint32_t fromValue) const {
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
    writePosition(invocationId, *value);
    return true;
  case hlsl::Semantic::Kind::ClipDistance: {
    const auto offsetIter = outClipOffset.find(semanticIndex);
    const auto typeIter = outClipType.find(semanticIndex);
    // We should have recorded all these semantics before.
    assert(offsetIter != outClipOffset.end());
    assert(typeIter != outClipType.end());
    writeClipCullArrayFromType(invocationId, 2, offsetIter->second,
                               typeIter->second, *value);
    return true;
  }
  case hlsl::Semantic::Kind::CullDistance: {
    const auto offsetIter = outCullOffset.find(semanticIndex);
    const auto typeIter = outCullType.find(semanticIndex);
    // We should have recorded all these semantics before.
    assert(offsetIter != outCullOffset.end());
    assert(typeIter != outCullType.end());
    writeClipCullArrayFromType(invocationId, 3, offsetIter->second,
                               typeIter->second, *value);
    return true;
  }
  }
  return false;
}

bool DeclResultIdMapper::createStageOutputVar(const DeclaratorDecl *decl,
                                              uint32_t storedValue,
                                              bool isPatchConstant) {
  QualType type = getTypeOrFnRetType(decl);

  // Output stream types (PointStream, LineStream, TriangleStream) are
  // translated as their underlying struct types.
  if (hlsl::IsHLSLStreamOutputType(type))
    type = hlsl::GetHLSLResourceResultType(type);

  const auto *sigPoint = deduceSigPoint(decl, /*asInput=*/false,
                                        shaderModel.GetKind(), isPatchConstant);

  return createStageVars(decl, sigPoint, /*asInput=*/false, type,
                         /*arraySize=*/0, llvm::None, &storedValue, "out.var");
}

bool DeclResultIdMapper::createStageOutputVar(const DeclaratorDecl *decl,
                                              uint32_t arraySize,
                                              uint32_t invocationId,
                                              uint32_t storedValue) {
  assert(shaderModel.IsHS());

  QualType type = getTypeOrFnRetType(decl);

  const auto *sigPoint =
      hlsl::SigPoint::GetSigPoint(hlsl::DXIL::SigPointKind::HSCPOut);

  return createStageVars(decl, sigPoint, /*asInput=*/false, type, arraySize,
                         llvm::Optional<uint32_t>(invocationId), &storedValue,
                         "out.var");
}

bool DeclResultIdMapper::createStageInputVar(const ParmVarDecl *paramDecl,
                                             uint32_t *loadedValue,
                                             bool isPatchConstant) {
  uint32_t arraySize = 0;
  QualType type = paramDecl->getType();

  // Deprive the outermost arrayness for HS/DS/GS and use arraySize
  // to convey that information
  if (hlsl::IsHLSLInputPatchType(type)) {
    arraySize = hlsl::GetHLSLInputPatchCount(type);
    type = hlsl::GetHLSLInputPatchElementType(type);
  } else if (hlsl::IsHLSLOutputPatchType(type)) {
    arraySize = hlsl::GetHLSLOutputPatchCount(type);
    type = hlsl::GetHLSLOutputPatchElementType(type);
  }

  const auto *sigPoint = deduceSigPoint(paramDecl, /*asInput=*/true,
                                        shaderModel.GetKind(), isPatchConstant);

  return createStageVars(paramDecl, sigPoint, /*asInput=*/true, type, arraySize,
                         llvm::None, loadedValue, "in.var");
}

const DeclResultIdMapper::DeclSpirvInfo *
DeclResultIdMapper::getDeclSpirvInfo(const NamedDecl *decl) const {
  auto it = astDecls.find(decl);
  if (it != astDecls.end())
    return &it->second;

  return nullptr;
}

SpirvEvalInfo DeclResultIdMapper::getDeclResultId(const NamedDecl *decl) {
  if (const auto *info = getDeclSpirvInfo(decl))
    if (info->indexInCTBuffer >= 0) {
      // If this is a VarDecl inside a HLSLBufferDecl, we need to do an extra
      // OpAccessChain to get the pointer to the variable since we created
      // a single variable for the whole buffer object.

      const uint32_t varType = typeTranslator.translateType(
          // Should only have VarDecls in a HLSLBufferDecl.
          cast<VarDecl>(decl)->getType(),
          // We need to set decorateLayout here to avoid creating SPIR-V
          // instructions for the current type without decorations.
          // According to the Vulkan spec, cbuffer should follow standrad
          // uniform buffer layout, which GLSL std140 rules statisfies.
          LayoutRule::GLSLStd140);

      const uint32_t elemId = theBuilder.createAccessChain(
          theBuilder.getPointerType(varType, info->storageClass),
          info->resultId, {theBuilder.getConstantInt32(info->indexInCTBuffer)});

      return {elemId, info->storageClass, info->layoutRule};
    } else {
      return *info;
    }

  assert(false && "found unregistered decl");
  return 0;
}

uint32_t DeclResultIdMapper::createFnParam(uint32_t paramType,
                                           const ParmVarDecl *param) {
  const uint32_t id = theBuilder.addFnParam(paramType, param->getName());
  astDecls[param] = {id, spv::StorageClass::Function};

  return id;
}

uint32_t DeclResultIdMapper::createFnVar(uint32_t varType, const VarDecl *var,
                                         llvm::Optional<uint32_t> init) {
  const uint32_t id = theBuilder.addFnVar(varType, var->getName(), init);
  astDecls[var] = {id, spv::StorageClass::Function};

  return id;
}

uint32_t DeclResultIdMapper::createFileVar(uint32_t varType, const VarDecl *var,
                                           llvm::Optional<uint32_t> init) {
  const uint32_t id = theBuilder.addModuleVar(
      varType, spv::StorageClass::Private, var->getName(), init);
  astDecls[var] = {id, spv::StorageClass::Private};

  return id;
}

uint32_t DeclResultIdMapper::createExternVar(const VarDecl *var) {
  auto storageClass = spv::StorageClass::UniformConstant;
  auto rule = LayoutRule::Void;
  bool isACSBuffer = false; // Whether its {Append|Consume}StructuredBuffer

  // TODO: Figure out other cases where the storage class should be Uniform.
  if (auto *t = var->getType()->getAs<RecordType>()) {
    const llvm::StringRef typeName = t->getDecl()->getName();
    if (typeName == "StructuredBuffer" || typeName == "RWStructuredBuffer" ||
        typeName == "ByteAddressBuffer" || typeName == "RWByteAddressBuffer" ||
        typeName == "AppendStructuredBuffer" ||
        typeName == "ConsumeStructuredBuffer") {
      // These types are all translated into OpTypeStruct with BufferBlock
      // decoration. They should follow standard storage buffer layout,
      // which GLSL std430 rules statisfies.
      storageClass = spv::StorageClass::Uniform;
      rule = LayoutRule::GLSLStd430;
      isACSBuffer =
          typeName.startswith("Append") || typeName.startswith("Consume");
    }
  }

  const auto varType = typeTranslator.translateType(var->getType(), rule);
  const uint32_t id = theBuilder.addModuleVar(varType, storageClass,
                                              var->getName(), llvm::None);
  astDecls[var] = {id, storageClass, rule};

  const auto *regAttr = getResourceBinding(var);
  const auto *bindingAttr = var->getAttr<VKBindingAttr>();
  const auto *counterBindingAttr = var->getAttr<VKCounterBindingAttr>();

  resourceVars.emplace_back(id, getResourceCategory(var->getType()), regAttr,
                            bindingAttr, counterBindingAttr);

  if (isACSBuffer) {
    // For {Append|Consume}StructuredBuffer, we need to always create another
    // variable for its associated counter.
    createCounterVar(var);
  }

  return id;
}

uint32_t
DeclResultIdMapper::createVarOfExplicitLayoutStruct(const DeclContext *decl,
                                                    llvm::StringRef typeName,
                                                    llvm::StringRef varName) {
  // Collect the type and name for each field
  llvm::SmallVector<uint32_t, 4> fieldTypes;
  llvm::SmallVector<llvm::StringRef, 4> fieldNames;
  for (const auto *subDecl : decl->decls()) {
    // Ignore implicit generated struct declarations/constructors/destructors.
    if (subDecl->isImplicit())
      continue;

    // The field can only be FieldDecl (for normal structs) or VarDecl (for
    // HLSLBufferDecls).
    assert(isa<VarDecl>(subDecl) || isa<FieldDecl>(subDecl));
    const auto *declDecl = cast<DeclaratorDecl>(subDecl);
    // All fields are qualified with const. It will affect the debug name.
    // We don't need it here.
    auto varType = declDecl->getType();
    varType.removeLocalConst();

    fieldTypes.push_back(
        typeTranslator.translateType(varType, LayoutRule::GLSLStd140,
                                     declDecl->hasAttr<HLSLRowMajorAttr>()));
    fieldNames.push_back(declDecl->getName());
  }

  // Get the type for the whole buffer
  // cbuffers are translated into OpTypeStruct with Block decoration. They
  // should follow standard uniform buffer layout according to the Vulkan spec.
  // GLSL std140 rules satisfies.
  auto decorations =
      typeTranslator.getLayoutDecorations(decl, LayoutRule::GLSLStd140);
  decorations.push_back(Decoration::getBlock(*theBuilder.getSPIRVContext()));
  const uint32_t structType =
      theBuilder.getStructType(fieldTypes, typeName, fieldNames, decorations);

  // Create the variable for the whole buffer
  return theBuilder.addModuleVar(structType, spv::StorageClass::Uniform,
                                 varName);
}

uint32_t DeclResultIdMapper::createCTBuffer(const HLSLBufferDecl *decl) {
  const std::string structName = "type." + decl->getName().str();
  const std::string varName = "var." + decl->getName().str();
  const uint32_t bufferVar =
      createVarOfExplicitLayoutStruct(decl, structName, varName);

  // We still register all VarDecls seperately here. All the VarDecls are
  // mapped to the <result-id> of the buffer object, which means when querying
  // querying the <result-id> for a certain VarDecl, we need to do an extra
  // OpAccessChain.
  int index = 0;
  for (const auto *subDecl : decl->decls()) {
    const auto *varDecl = cast<VarDecl>(subDecl);
    // TODO: std140 rules may not suit tbuffers.
    astDecls[varDecl] = {bufferVar, spv::StorageClass::Uniform,
                         LayoutRule::GLSLStd140, index++};
  }
  resourceVars.emplace_back(
      bufferVar, ResourceVar::Category::Other, getResourceBinding(decl),
      decl->getAttr<VKBindingAttr>(), decl->getAttr<VKCounterBindingAttr>());

  return bufferVar;
}

uint32_t DeclResultIdMapper::createCTBuffer(const VarDecl *decl) {
  const auto *recordType = decl->getType()->getAs<RecordType>();
  assert(recordType);
  const auto *context = cast<HLSLBufferDecl>(decl->getDeclContext());
  const bool isCBuffer = context->isCBuffer();

  const std::string structName =
      "type." + std::string(isCBuffer ? "ConstantBuffer." : "TextureBuffer") +
      recordType->getDecl()->getName().str();
  const uint32_t bufferVar = createVarOfExplicitLayoutStruct(
      recordType->getDecl(), structName, decl->getName());

  // We register the VarDecl here.
  // TODO: std140 rules may not suit tbuffers.
  astDecls[decl] = {bufferVar, spv::StorageClass::Uniform,
                    LayoutRule::GLSLStd140};
  resourceVars.emplace_back(
      bufferVar, ResourceVar::Category::Other, getResourceBinding(context),
      decl->getAttr<VKBindingAttr>(), decl->getAttr<VKCounterBindingAttr>());

  return bufferVar;
}

uint32_t DeclResultIdMapper::getOrRegisterFnResultId(const FunctionDecl *fn) {
  if (const auto *info = getDeclSpirvInfo(fn))
    return info->resultId;

  const uint32_t id = theBuilder.getSPIRVContext()->takeNextId();
  astDecls[fn] = {id, spv::StorageClass::Function};

  return id;
}

uint32_t DeclResultIdMapper::getOrCreateCounterId(const ValueDecl *decl) {
  const auto counter = counterVars.find(decl);
  if (counter != counterVars.end())
    return counter->second;
  return createCounterVar(decl);
}

uint32_t DeclResultIdMapper::createCounterVar(const ValueDecl *decl) {
  const auto *info = getDeclSpirvInfo(decl);
  const uint32_t counterType = typeTranslator.getACSBufferCounter();
  const std::string counterName = "counter.var." + decl->getName().str();
  const uint32_t counterId =
      theBuilder.addModuleVar(counterType, info->storageClass, counterName);

  resourceVars.emplace_back(counterId, ResourceVar::Category::Other,
                            getResourceBinding(decl),
                            decl->getAttr<VKBindingAttr>(),
                            decl->getAttr<VKCounterBindingAttr>(), true);
  return counterVars[decl] = counterId;
}

std::vector<uint32_t> DeclResultIdMapper::collectStageVars() const {
  std::vector<uint32_t> vars;

  for (auto var : glPerVertex.getStageInVars())
    vars.push_back(var);
  for (auto var : glPerVertex.getStageOutVars())
    vars.push_back(var);

  for (const auto &var : stageVars)
    vars.push_back(var.getSpirvId());

  return vars;
}

namespace {
/// A class for managing stage input/output locations to avoid duplicate uses of
/// the same location.
class LocationSet {
public:
  /// Maximum number of locations supported
  // Typically we won't have that many stage input or output variables.
  // Using 64 should be fine here.
  const static uint32_t kMaxLoc = 64;

  LocationSet() : usedLocs(kMaxLoc, false), nextLoc(0) {}

  /// Uses the given location.
  void useLoc(uint32_t loc) { usedLocs.set(loc); }

  /// Uses the next available location.
  uint32_t useNextLoc() {
    while (usedLocs[nextLoc])
      nextLoc++;
    usedLocs.set(nextLoc);
    return nextLoc++;
  }

  /// Returns true if the given location number is already used.
  bool isLocUsed(uint32_t loc) { return usedLocs[loc]; }

private:
  llvm::SmallBitVector usedLocs; ///< All previously used locations
  uint32_t nextLoc;              ///< Next available location
};

/// A class for managing resource bindings to avoid duplicate uses of the same
/// set and binding number.
class BindingSet {
public:
  /// Tries to use the given set and binding number. Returns true if possible,
  /// false otherwise.
  bool tryToUseBinding(uint32_t binding, uint32_t set,
                       ResourceVar::Category category) {
    const auto cat = static_cast<uint32_t>(category);
    // Note that we will create the entry for binding in bindings[set] here.
    // But that should not have bad effects since it defaults to zero.
    if ((usedBindings[set][binding] & cat) == 0) {
      usedBindings[set][binding] |= cat;
      return true;
    }
    return false;
  }

  /// Uses the next avaiable binding number in set 0.
  uint32_t useNextBinding(uint32_t set, ResourceVar::Category category) {
    auto &binding = usedBindings[set];
    auto &next = nextBindings[set];
    while (binding.count(next))
      ++next;
    binding[next] = static_cast<uint32_t>(category);
    return next++;
  }

private:
  ///< set number -> (binding number -> resource category)
  llvm::DenseMap<uint32_t, llvm::DenseMap<uint32_t, uint32_t>> usedBindings;
  ///< set number -> next available binding number
  llvm::DenseMap<uint32_t, uint32_t> nextBindings;
};
} // namespace

bool DeclResultIdMapper::checkSemanticDuplication(bool forInput) {
  llvm::StringSet<> seenSemantics;
  bool success = true;
  for (const auto &var : stageVars) {
    auto s = var.getSemanticStr();

    if (forInput && var.getSigPoint()->IsInput()) {
      if (seenSemantics.count(s)) {
        emitError("input semantic '%0' used more than once") << s;
        success = false;
      }
      seenSemantics.insert(s);
    } else if (!forInput && var.getSigPoint()->IsOutput()) {
      if (seenSemantics.count(s)) {
        emitError("output semantic '%0' used more than once") << s;
        success = false;
      }
      seenSemantics.insert(s);
    }
  }

  return success;
}

bool DeclResultIdMapper::finalizeStageIOLocations(bool forInput) {
  if (!checkSemanticDuplication(forInput))
    return false;

  // Returns false if the given StageVar is an input/output variable without
  // explicit location assignment. Otherwise, returns true.
  const auto locAssigned = [forInput, this](const StageVar &v) {
    if (forInput == isInputStorageClass(v))
      // No need to assign location for builtins. Treat as assigned.
      return v.isSpirvBuitin() || v.getLocationAttr() != nullptr;
    // For the ones we don't care, treat as assigned.
    return true;
  };

  // If we have explicit location specified for all input/output variables,
  // use them instead assign by ourselves.
  if (std::all_of(stageVars.begin(), stageVars.end(), locAssigned)) {
    LocationSet locSet;
    bool noError = true;

    for (const auto &var : stageVars) {
      // Skip those stage variables we are not handling for this call
      if (forInput != isInputStorageClass(var))
        continue;

      // Skip builtins
      if (var.isSpirvBuitin())
        continue;

      const auto *attr = var.getLocationAttr();
      const auto loc = attr->getNumber();
      const auto attrLoc = attr->getLocation(); // Attr source code location

      if (loc >= LocationSet::kMaxLoc) {
        emitError("stage %select{output|input}0 location #%1 too large",
                  attrLoc)
            << forInput << loc;
        return false;
      }

      // Make sure the same location is not assigned more than once
      if (locSet.isLocUsed(loc)) {
        emitError("stage %select{output|input}0 location #%1 already assigned",
                  attrLoc)
            << forInput << loc;
        noError = false;
      }
      locSet.useLoc(loc);

      theBuilder.decorateLocation(var.getSpirvId(), loc);
    }

    return noError;
  }

  std::vector<const StageVar *> vars;
  LocationSet locSet;

  for (const auto &var : stageVars) {
    if (forInput != isInputStorageClass(var))
      continue;

    if (!var.isSpirvBuitin()) {
      if (var.getLocationAttr() != nullptr) {
        // We have checked that not all of the stage variables have explicit
        // location assignment.
        emitError("partial explicit stage %select{output|input}0 location "
                  "assignment via [[vk::location(X)]] unsupported")
            << forInput;
        return false;
      }

      // Only SV_Target, SV_Depth, SV_DepthLessEqual, SV_DepthGreaterEqual,
      // SV_StencilRef, SV_Coverage are allowed in the pixel shader.
      // Arbitrary semantics are disallowed in pixel shader.
      if (var.getSemantic() &&
          var.getSemantic()->GetKind() == hlsl::Semantic::Kind::Target) {
        theBuilder.decorateLocation(var.getSpirvId(), var.getSemanticIndex());
        locSet.useLoc(var.getSemanticIndex());
      } else {
        vars.push_back(&var);
      }
    }
  }

  if (spirvOptions.stageIoOrder == "alpha") {
    // Sort stage input/output variables alphabetically
    std::sort(vars.begin(), vars.end(),
              [](const StageVar *a, const StageVar *b) {
                return a->getSemanticStr() < b->getSemanticStr();
              });
  }

  for (const auto *var : vars)
    theBuilder.decorateLocation(var->getSpirvId(), locSet.useNextLoc());

  return true;
}

namespace {
/// A class for maintaining the binding number shift requested for descriptor
/// sets.
class BindingShiftMapper {
public:
  explicit BindingShiftMapper(const llvm::SmallVectorImpl<uint32_t> &shifts)
      : masterShift(0) {
    assert(shifts.size() % 2 == 0);
    for (uint32_t i = 0; i < shifts.size(); i += 2)
      perSetShift[shifts[i + 1]] = shifts[i];
  }

  /// Returns the shift amount for the given set.
  uint32_t getShiftForSet(uint32_t set) const {
    const auto found = perSetShift.find(set);
    if (found != perSetShift.end())
      return found->second;
    return masterShift;
  }

private:
  uint32_t masterShift; /// Shift amount applies to all sets.
  llvm::DenseMap<uint32_t, uint32_t> perSetShift;
};
}

bool DeclResultIdMapper::decorateResourceBindings() {
  // For normal resource, we support 3 approaches of setting binding numbers:
  // - m1: [[vk::binding(...)]]
  // - m2: :register(...)
  // - m3: None
  //
  // For associated counters, we support 2 approaches:
  // - c1: [[vk::counter_binding(...)]
  // - c2: None
  //
  // In combination, we need to handle 9 cases:
  // - 3 cases for nomral resoures (m1, m2, m3)
  // - 6 cases for associated counters (mX * cY)
  //
  // In the following order:
  // - m1, mX * c1
  // - m2
  // - m3, mX * c2

  BindingSet bindingSet;
  bool noError = true;

  // Tries to decorate the given varId of the given category with set number
  // setNo, binding number bindingNo. Emits error on failure.
  const auto tryToDecorate = [this, &bindingSet, &noError](
      const uint32_t varId, const uint32_t setNo, const uint32_t bindingNo,
      const ResourceVar::Category cat, SourceLocation loc) {
    if (bindingSet.tryToUseBinding(bindingNo, setNo, cat)) {
      theBuilder.decorateDSetBinding(varId, setNo, bindingNo);
    } else {
      emitError("resource binding #%0 in descriptor set #%1 already assigned",
                loc)
          << bindingNo << setNo;
      noError = false;
    }
  };

  for (const auto &var : resourceVars) {
    if (var.isCounter()) {
      if (const auto *vkCBinding = var.getCounterBinding()) {
        // Process mX * c1
        uint32_t set = 0;
        if (const auto *vkBinding = var.getBinding())
          set = vkBinding->getSet();
        if (const auto *reg = var.getRegister())
          set = reg->RegisterSpace;

        tryToDecorate(var.getSpirvId(), set, vkCBinding->getBinding(),
                      var.getCategory(), vkCBinding->getLocation());
      }
    } else {
      if (const auto *vkBinding = var.getBinding()) {
        // Process m1
        tryToDecorate(var.getSpirvId(), vkBinding->getSet(),
                      vkBinding->getBinding(), var.getCategory(),
                      vkBinding->getLocation());
      }
    }
  }

  BindingShiftMapper bShiftMapper(spirvOptions.bShift);
  BindingShiftMapper tShiftMapper(spirvOptions.tShift);
  BindingShiftMapper sShiftMapper(spirvOptions.sShift);
  BindingShiftMapper uShiftMapper(spirvOptions.uShift);

  // Process m2
  for (const auto &var : resourceVars)
    if (!var.isCounter() && !var.getBinding())
      if (const auto *reg = var.getRegister()) {
        const uint32_t set = reg->RegisterSpace;
        uint32_t binding = reg->RegisterNumber;
        switch (reg->RegisterType) {
        case 'b':
          binding += bShiftMapper.getShiftForSet(set);
          break;
        case 't':
          binding += tShiftMapper.getShiftForSet(set);
          break;
        case 's':
          binding += sShiftMapper.getShiftForSet(set);
          break;
        case 'u':
          binding += uShiftMapper.getShiftForSet(set);
          break;
        case 'c':
          // For setting packing offset. Does not affect binding.
          break;
        default:
          llvm_unreachable("unknown register type found");
        }

        tryToDecorate(var.getSpirvId(), set, binding, var.getCategory(),
                      reg->Loc);
      }

  for (const auto &var : resourceVars) {
    const auto cat = var.getCategory();
    if (var.isCounter()) {
      if (!var.getCounterBinding()) {
        // Process mX * c2
        uint32_t set = 0;
        if (const auto *vkBinding = var.getBinding())
          set = vkBinding->getSet();
        else if (const auto *reg = var.getRegister())
          set = reg->RegisterSpace;

        theBuilder.decorateDSetBinding(var.getSpirvId(), set,
                                       bindingSet.useNextBinding(set, cat));
      }
    } else if (!var.getBinding() && !var.getRegister()) {
      // Process m3
      theBuilder.decorateDSetBinding(var.getSpirvId(), 0,
                                     bindingSet.useNextBinding(0, cat));
    }
  }

  return noError;
}

bool DeclResultIdMapper::createStageVars(
    const DeclaratorDecl *decl, const hlsl::SigPoint *sigPoint, bool asInput,
    QualType type, uint32_t arraySize, llvm::Optional<uint32_t> invocationId,
    uint32_t *value, const llvm::Twine &namePrefix) {
  // invocationId should only be used for handling HS per-vertex output.
  assert(invocationId.hasValue() ? shaderModel.IsHS() : true);
  assert(invocationId.hasValue() ? !asInput : true);
  assert(invocationId.hasValue() ? arraySize != 0 : true);

  if (type->isVoidType()) {
    // No stage variables will be created for void type.
    return true;
  }

  uint32_t typeId = typeTranslator.translateType(type);
  llvm::StringRef semanticStr;
  const hlsl::Semantic *semantic = {};
  uint32_t semanticIndex = {};

  if (getStageVarSemantic(decl, &semanticStr, &semantic, &semanticIndex)) {
    // Found semantic attached directly to this Decl. This means we need to
    // map this decl to a single stage variable.

    // Error out when the given semantic is invalid in this shader model
    if (hlsl::SigPoint::GetInterpretation(
            semantic->GetKind(), sigPoint->GetKind(), shaderModel.GetMajor(),
            shaderModel.GetMinor()) ==
        hlsl::DXIL::SemanticInterpretationKind::NA) {
      emitError("invalid semantic %0 for shader model %1")
          << semanticStr << shaderModel.GetName();
      return false;
    }

    // Special handling of certain mapping between HLSL semantics and
    // SPIR-V builtin:
    // * SV_Position/SV_CullDistance/SV_ClipDistance should be grouped into the
    //   gl_PerVertex struct in vertex processing stages.
    // * SV_DomainLocation can refer to a float2, whereas TessCoord is a float3.
    //   To ensure SPIR-V validity, we must create a float3 and  extract a
    //   float2 from it before passing it to the main function.
    if (glPerVertex.tryToAccess(semantic->GetKind(), semanticIndex,
                                invocationId, value, sigPoint->GetKind()))
      return true;

    if (semantic->GetKind() == hlsl::Semantic::Kind::DomainLocation)
      typeId = theBuilder.getVecType(theBuilder.getFloat32Type(), 3);

    // Handle the extra arrayness
    const uint32_t elementTypeId = typeId;
    if (arraySize != 0)
      typeId = theBuilder.getArrayType(typeId,
                                       theBuilder.getConstantUint32(arraySize));

    StageVar stageVar(sigPoint, semanticStr, semantic, semanticIndex, typeId);
    llvm::Twine name = namePrefix + "." + semanticStr;
    const uint32_t varId = createSpirvStageVar(&stageVar, name);

    if (varId == 0)
      return false;

    stageVar.setSpirvId(varId);
    stageVar.setLocationAttr(decl->getAttr<VKLocationAttr>());
    stageVars.push_back(stageVar);

    // TODO: the following may not be correct?
    if (sigPoint->GetSignatureKind() ==
        hlsl::DXIL::SignatureKind::PatchConstant)
      theBuilder.decorate(varId, spv::Decoration::Patch);

    // Decorate with interpolation modes for pixel shader input variables
    if (shaderModel.IsPS() && sigPoint->IsInput())
      decoratePSInterpolationMode(decl, type, varId);

    if (asInput) {
      *value = theBuilder.createLoad(typeId, varId);
    } else {
      uint32_t ptr = varId;
      if (invocationId.hasValue()) {
        // Special handling of HS ouput, for which we write to only one element
        // in the per-vertex data array: the one indexed by  SV_ControlPointID.
        const uint32_t ptrType =
            theBuilder.getPointerType(elementTypeId, spv::StorageClass::Output);
        const uint32_t index = invocationId.getValue();
        ptr = theBuilder.createAccessChain(ptrType, varId, index);
      }
      theBuilder.createStore(ptr, *value);
    }

    return true;
  }

  // If the decl itself doesn't have semantic string attached, it should be
  // a struct having all its fields with semantic strings.
  if (!type->isStructureType()) {
    emitError("semantic string missing for shader %select{output|input}0 "
              "variable '%1'",
              decl->getLocStart())
        << asInput << decl->getName();
    return false;
  }

  const auto *structDecl = cast<RecordType>(type.getTypePtr())->getDecl();

  if (asInput) {
    // If this decl translates into multiple stage input variables, we need to
    // load their values into a composite.
    llvm::SmallVector<uint32_t, 4> subValues;

    for (const auto *field : structDecl->fields()) {
      uint32_t subValue = 0;
      if (!createStageVars(field, sigPoint, asInput, field->getType(),
                           arraySize, invocationId, &subValue, namePrefix))
        return false;
      subValues.push_back(subValue);
    }

    if (arraySize == 0) {
      *value = theBuilder.createCompositeConstruct(typeId, subValues);
      return true;
    }

    // Handle the extra level of arrayness.

    // We need to return an array of structs. But we get arrays of fields
    // from visiting all fields. So now we need to extract all the elements
    // at the same index of each field arrays and compose a new struct out
    // of them.
    const uint32_t structType = typeTranslator.translateType(type);
    const uint32_t arrayType = theBuilder.getArrayType(
        structType, theBuilder.getConstantUint32(arraySize));
    llvm::SmallVector<uint32_t, 16> arrayElements;

    for (uint32_t arrayIndex = 0; arrayIndex < arraySize; ++arrayIndex) {
      llvm::SmallVector<uint32_t, 8> fields;

      // Extract the element at index arrayIndex from each field
      for (const auto *field : structDecl->fields()) {
        const uint32_t fieldType =
            typeTranslator.translateType(field->getType());
        fields.push_back(theBuilder.createCompositeExtract(
            fieldType, subValues[field->getFieldIndex()], {arrayIndex}));
      }
      // Compose a new struct out of them
      arrayElements.push_back(
          theBuilder.createCompositeConstruct(structType, fields));
    }

    *value = theBuilder.createCompositeConstruct(arrayType, arrayElements);
  } else {
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
    for (const auto *field : structDecl->fields()) {
      const uint32_t fieldType = typeTranslator.translateType(field->getType());
      uint32_t subValue = theBuilder.createCompositeExtract(
          fieldType, *value, {field->getFieldIndex()});
      if (!createStageVars(field, sigPoint, asInput, field->getType(),
                           arraySize, invocationId, &subValue, namePrefix))
        return false;
    }
  }

  return true;
}

void DeclResultIdMapper::decoratePSInterpolationMode(const DeclaratorDecl *decl,
                                                     QualType type,
                                                     uint32_t varId) {
  const QualType elemType = typeTranslator.getElementType(type);

  if (elemType->isBooleanType() || elemType->isIntegerType()) {
    // TODO: Probably we can call hlsl::ValidateSignatureElement() for the
    // following check.
    if (decl->getAttr<HLSLLinearAttr>() || decl->getAttr<HLSLCentroidAttr>() ||
        decl->getAttr<HLSLNoPerspectiveAttr>() ||
        decl->getAttr<HLSLSampleAttr>()) {
      emitError("only nointerpolation mode allowed for integer input "
                "parameters in pixel shader",
                decl->getLocation());
    } else {
      theBuilder.decorate(varId, spv::Decoration::Flat);
    }
  } else {
    // Do nothing for HLSLLinearAttr since its the default
    // Attributes can be used together. So cannot use else if.
    if (decl->getAttr<HLSLCentroidAttr>())
      theBuilder.decorate(varId, spv::Decoration::Centroid);
    if (decl->getAttr<HLSLNoInterpolationAttr>())
      theBuilder.decorate(varId, spv::Decoration::Flat);
    if (decl->getAttr<HLSLNoPerspectiveAttr>())
      theBuilder.decorate(varId, spv::Decoration::NoPerspective);
    if (decl->getAttr<HLSLSampleAttr>()) {
      theBuilder.requireCapability(spv::Capability::SampleRateShading);
      theBuilder.decorate(varId, spv::Decoration::Sample);
    }
  }
}

uint32_t DeclResultIdMapper::createSpirvStageVar(StageVar *stageVar,
                                                 const llvm::Twine &name) {
  using spv::BuiltIn;
  const auto sigPoint = stageVar->getSigPoint();
  const auto semanticKind = stageVar->getSemantic()->GetKind();
  const auto sigPointKind = sigPoint->GetKind();
  const uint32_t type = stageVar->getSpirvTypeId();

  spv::StorageClass sc = getStorageClassForSigPoint(sigPoint);
  if (sc == spv::StorageClass::Max)
    return 0;
  stageVar->setStorageClass(sc);

  // The following translation assumes that semantic validity in the current
  // shader model is already checked, so it only covers valid SigPoints for
  // each semantic.
  switch (semanticKind) {
  // According to DXIL spec, the Position SV can be used by all SigPoints
  // other than PCIn, HSIn, GSIn, PSOut, CSIn.
  // According to Vulkan spec, the Position BuiltIn can only be used
  // by VSOut, HS/DS/GS In/Out.
  case hlsl::Semantic::Kind::Position: {
    switch (sigPointKind) {
    case hlsl::SigPoint::Kind::VSIn:
    case hlsl::SigPoint::Kind::PCOut:
    case hlsl::SigPoint::Kind::DSIn:
      return theBuilder.addStageIOVar(type, sc, name.str());
    case hlsl::SigPoint::Kind::VSOut:
    case hlsl::SigPoint::Kind::HSCPIn:
    case hlsl::SigPoint::Kind::HSCPOut:
    case hlsl::SigPoint::Kind::DSCPIn:
    case hlsl::SigPoint::Kind::DSOut:
    case hlsl::SigPoint::Kind::GSVIn:
      llvm_unreachable("should be handled in gl_PerVertex struct");
    case hlsl::SigPoint::Kind::GSOut:
      stageVar->setIsSpirvBuiltin();
      return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::Position);
    case hlsl::SigPoint::Kind::PSIn:
      stageVar->setIsSpirvBuiltin();
      return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::FragCoord);
    default:
      llvm_unreachable("invalid usage of SV_Position sneaked in");
    }
  }
  // According to DXIL spec, the VertexID SV can only be used by VSIn.
  case hlsl::Semantic::Kind::VertexID:
    stageVar->setIsSpirvBuiltin();
    return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::VertexIndex);
  // According to DXIL spec, the InstanceID SV can  be used by VSIn, VSOut,
  // HSCPIn, HSCPOut, DSCPIn, DSOut, GSVIn, GSOut, PSIn.
  // According to Vulkan spec, the InstanceIndex BuitIn can only be used by
  // VSIn.
  case hlsl::Semantic::Kind::InstanceID: {
    switch (sigPointKind) {
    case hlsl::SigPoint::Kind::VSIn:
      stageVar->setIsSpirvBuiltin();
      return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::InstanceIndex);
    case hlsl::SigPoint::Kind::VSOut:
      return theBuilder.addStageIOVar(type, sc, name.str());
    case hlsl::SigPoint::Kind::PSIn:
      return theBuilder.addStageIOVar(type, sc, name.str());
    default:
      emitError("semantic InstanceID for SigPoint %0 unimplemented yet")
          << sigPoint->GetName();
      break;
    }
  }
  // According to DXIL spec, the Depth{|GreaterEqual|LessEqual} SV can only be
  // used by PSOut.
  case hlsl::Semantic::Kind::Depth:
  case hlsl::Semantic::Kind::DepthGreaterEqual:
  case hlsl::Semantic::Kind::DepthLessEqual: {
    stageVar->setIsSpirvBuiltin();
    if (semanticKind == hlsl::Semantic::Kind::DepthGreaterEqual)
      theBuilder.addExecutionMode(entryFunctionId,
                                  spv::ExecutionMode::DepthGreater, {});
    else if (semanticKind == hlsl::Semantic::Kind::DepthLessEqual)
      theBuilder.addExecutionMode(entryFunctionId,
                                  spv::ExecutionMode::DepthLess, {});
    return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::FragDepth);
  }
  // According to DXIL spec, the ClipDistance/CullDistance SV can be used by
  // all
  // SigPoints other than PCIn, HSIn, GSIn, PSOut, CSIn.
  // According to Vulkan spec, the ClipDistance/CullDistance BuiltIn can only
  // be
  // used by VSOut, HS/DS/GS In/Out.
  case hlsl::Semantic::Kind::ClipDistance:
  case hlsl::Semantic::Kind::CullDistance: {
    switch (sigPointKind) {
    case hlsl::SigPoint::Kind::VSIn:
    case hlsl::SigPoint::Kind::PCOut:
    case hlsl::SigPoint::Kind::DSIn:
      return theBuilder.addStageIOVar(type, sc, name.str());
    case hlsl::SigPoint::Kind::VSOut:
    case hlsl::SigPoint::Kind::HSCPIn:
    case hlsl::SigPoint::Kind::HSCPOut:
    case hlsl::SigPoint::Kind::DSCPIn:
    case hlsl::SigPoint::Kind::DSOut:
    case hlsl::SigPoint::Kind::GSVIn:
    case hlsl::SigPoint::Kind::GSOut:
    case hlsl::SigPoint::Kind::PSIn:
      llvm_unreachable("should be handled in gl_PerVertex struct");
    default:
      llvm_unreachable(
          "invalid usage of SV_ClipDistance/SV_CullDistance sneaked in");
    }
  }
  // According to DXIL spec, the IsFrontFace SV can only be used by GSOut and
  // PSIn.
  // According to Vulkan spec, the FrontFacing BuitIn can only be used in
  // PSIn.
  case hlsl::Semantic::Kind::IsFrontFace: {
    switch (sigPointKind) {
    case hlsl::SigPoint::Kind::PSIn:
      stageVar->setIsSpirvBuiltin();
      return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::FrontFacing);
    default:
      emitError("semantic IsFrontFace for SigPoint %0 unimplemented yet")
          << sigPoint->GetName();
      break;
    }
  }
  // According to DXIL spec, the Target SV can only be used by PSOut.
  // There is no corresponding builtin decoration in SPIR-V. So generate
  // normal
  // Vulkan stage input/output variables.
  case hlsl::Semantic::Kind::Target:
  // An arbitrary semantic is defined by users. Generate normal Vulkan stage
  // input/output variables.
  case hlsl::Semantic::Kind::Arbitrary: {
    return theBuilder.addStageIOVar(type, sc, name.str());
    // TODO: patch constant function in hull shader
  }
  case hlsl::Semantic::Kind::DispatchThreadID: {
    stageVar->setIsSpirvBuiltin();
    return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::GlobalInvocationId);
  }
  case hlsl::Semantic::Kind::GroupID: {
    stageVar->setIsSpirvBuiltin();
    return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::WorkgroupId);
  }
  case hlsl::Semantic::Kind::GroupThreadID: {
    stageVar->setIsSpirvBuiltin();
    return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::LocalInvocationId);
  }
  case hlsl::Semantic::Kind::GroupIndex: {
    stageVar->setIsSpirvBuiltin();
    return theBuilder.addStageBuiltinVar(type, sc,
                                         BuiltIn::LocalInvocationIndex);
  }
  case hlsl::Semantic::Kind::OutputControlPointID: {
    stageVar->setIsSpirvBuiltin();
    return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::InvocationId);
  }
  case hlsl::Semantic::Kind::PrimitiveID: {
    stageVar->setIsSpirvBuiltin();
    return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::PrimitiveId);
  }
  case hlsl::Semantic::Kind::TessFactor: {
    stageVar->setIsSpirvBuiltin();
    return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::TessLevelOuter);
  }
  case hlsl::Semantic::Kind::InsideTessFactor: {
    stageVar->setIsSpirvBuiltin();
    return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::TessLevelInner);
  }
  case hlsl::Semantic::Kind::DomainLocation: {
    stageVar->setIsSpirvBuiltin();
    return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::TessCoord);
  }
  default:
    emitError("semantic %0 unimplemented yet")
        << stageVar->getSemantic()->GetName();
    break;
  }

  return 0;
}

spv::StorageClass
DeclResultIdMapper::getStorageClassForSigPoint(const hlsl::SigPoint *sigPoint) {
  // This translation is done based on the HLSL reference (see docs/dxil.rst).
  const auto sigPointKind = sigPoint->GetKind();
  const auto signatureKind = sigPoint->GetSignatureKind();
  spv::StorageClass sc = spv::StorageClass::Max;
  switch (signatureKind) {
  case hlsl::DXIL::SignatureKind::Input:
    sc = spv::StorageClass::Input;
    break;
  case hlsl::DXIL::SignatureKind::Output:
    sc = spv::StorageClass::Output;
    break;
  case hlsl::DXIL::SignatureKind::Invalid: {
    // There are some special cases in HLSL (See docs/dxil.rst):
    // SignatureKind is "invalid" for PCIn, HSIn, GSIn, and CSIn.
    switch (sigPointKind) {
    case hlsl::DXIL::SigPointKind::PCIn:
    case hlsl::DXIL::SigPointKind::HSIn:
    case hlsl::DXIL::SigPointKind::GSIn:
    case hlsl::DXIL::SigPointKind::CSIn:
      sc = spv::StorageClass::Input;
      break;
    default:
      emitError("Found invalid SigPoint kind for a semantic.");
    }
    break;
  }
  case hlsl::DXIL::SignatureKind::PatchConstant: {
    // There are some special cases in HLSL (See docs/dxil.rst):
    // SignatureKind is "PatchConstant" for PCOut and DSIn.
    switch (sigPointKind) {
    case hlsl::DXIL::SigPointKind::PCOut:
      // Patch Constant Output (Output of Hull which is passed to Domain).
      sc = spv::StorageClass::Output;
      break;
    case hlsl::DXIL::SigPointKind::DSIn:
      // Domain Shader regular input - Patch Constant data plus system values.
      sc = spv::StorageClass::Input;
      break;
    default:
      emitError("Found invalid SigPoint kind for a semantic.");
    }
    break;
  }
  default:
    emitError("Found invalid SignatureKind for semantic.");
  }
  return sc;
}

} // end namespace spirv
} // end namespace clang
