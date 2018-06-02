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
#include <sstream>
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

/// \brief Returns the stage variable's register assignment for the given Decl.
const hlsl::RegisterAssignment *getResourceBinding(const NamedDecl *decl) {
  for (auto *annotation : decl->getUnusualAnnotations()) {
    if (auto *reg = dyn_cast<hlsl::RegisterAssignment>(annotation)) {
      return reg;
    }
  }
  return nullptr;
}

/// \brief Returns true if the given declaration has a primitive type qualifier.
/// Returns false otherwise.
inline bool hasGSPrimitiveTypeQualifier(const Decl *decl) {
  return decl->hasAttr<HLSLTriangleAttr>() ||
         decl->hasAttr<HLSLTriangleAdjAttr>() ||
         decl->hasAttr<HLSLPointAttr>() || decl->hasAttr<HLSLLineAttr>() ||
         decl->hasAttr<HLSLLineAdjAttr>();
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

/// \brief Deduces the HLSL SigPoint for the given decl appearing in the given
/// shader model.
const hlsl::SigPoint *deduceSigPoint(const DeclaratorDecl *decl, bool asInput,
                                     const hlsl::ShaderModel::Kind kind,
                                     bool forPCF) {
  return hlsl::SigPoint::GetSigPoint(hlsl::SigPointFromInputQual(
      deduceParamQual(decl, asInput), kind, forPCF));
}

/// Returns the type of the given decl. If the given decl is a FunctionDecl,
/// returns its result type.
inline QualType getTypeOrFnRetType(const DeclaratorDecl *decl) {
  if (const auto *funcDecl = dyn_cast<FunctionDecl>(decl)) {
    return funcDecl->getReturnType();
  }
  return decl->getType();
}

/// Returns the number of base classes if this type is a derived class/struct.
/// Returns zero otherwise.
inline uint32_t getNumBaseClasses(QualType type) {
  if (const auto *cxxDecl = type->getAsCXXRecordDecl())
    return cxxDecl->getNumBases();
  return 0;
}
} // anonymous namespace

std::string StageVar::getSemanticStr() const {
  // A special case for zero index, which is equivalent to no index.
  // Use what is in the source code.
  // TODO: this looks like a hack to make the current tests happy.
  // Should consider remove it and fix all tests.
  if (semanticInfo.index == 0)
    return semanticInfo.str;

  std::ostringstream ss;
  ss << semanticInfo.name.str() << semanticInfo.index;
  return ss.str();
}

uint32_t CounterIdAliasPair::get(ModuleBuilder &builder,
                                 TypeTranslator &translator) const {
  if (isAlias) {
    const uint32_t counterVarType = builder.getPointerType(
        translator.getACSBufferCounter(), spv::StorageClass::Uniform);
    return builder.createLoad(counterVarType, resultId);
  }
  return resultId;
}

const CounterIdAliasPair *
CounterVarFields::get(const llvm::SmallVectorImpl<uint32_t> &indices) const {
  for (const auto &field : fields)
    if (field.indices == indices)
      return &field.counterVar;
  return nullptr;
}

bool CounterVarFields::assign(const CounterVarFields &srcFields,
                              ModuleBuilder &builder,
                              TypeTranslator &translator) const {
  for (const auto &field : fields) {
    const auto *srcField = srcFields.get(field.indices);
    if (!srcField)
      return false;

    field.counterVar.assign(*srcField, builder, translator);
  }

  return true;
}

bool CounterVarFields::assign(const CounterVarFields &srcFields,
                              const llvm::SmallVector<uint32_t, 4> &dstPrefix,
                              const llvm::SmallVector<uint32_t, 4> &srcPrefix,
                              ModuleBuilder &builder,
                              TypeTranslator &translator) const {
  if (dstPrefix.empty() && srcPrefix.empty())
    return assign(srcFields, builder, translator);

  llvm::SmallVector<uint32_t, 4> srcIndices = srcPrefix;

  // If whole has the given prefix, appends all elements after the prefix in
  // whole to srcIndices.
  const auto applyDiff =
      [&srcIndices](const llvm::SmallVector<uint32_t, 4> &whole,
                    const llvm::SmallVector<uint32_t, 4> &prefix) -> bool {
    uint32_t i = 0;
    for (; i < prefix.size(); ++i)
      if (whole[i] != prefix[i]) {
        break;
      }
    if (i == prefix.size()) {
      for (; i < whole.size(); ++i)
        srcIndices.push_back(whole[i]);
      return true;
    }
    return false;
  };

  for (const auto &field : fields)
    if (applyDiff(field.indices, dstPrefix)) {
      const auto *srcField = srcFields.get(srcIndices);
      if (!srcField)
        return false;

      field.counterVar.assign(*srcField, builder, translator);
      for (uint32_t i = srcPrefix.size(); i < srcIndices.size(); ++i)
        srcIndices.pop_back();
    }

  return true;
}

SemanticInfo DeclResultIdMapper::getStageVarSemantic(const NamedDecl *decl) {
  for (auto *annotation : decl->getUnusualAnnotations()) {
    if (auto *sema = dyn_cast<hlsl::SemanticDecl>(annotation)) {
      llvm::StringRef semanticStr = sema->SemanticName;
      llvm::StringRef semanticName;
      uint32_t index = 0;
      hlsl::Semantic::DecomposeNameAndIndex(semanticStr, &semanticName, &index);
      const auto *semantic = hlsl::Semantic::GetByName(semanticName);
      return {semanticStr, semantic, semanticName, index, sema->Loc};
    }
  }
  return {};
}

bool DeclResultIdMapper::createStageOutputVar(const DeclaratorDecl *decl,
                                              uint32_t storedValue,
                                              bool forPCF) {
  QualType type = getTypeOrFnRetType(decl);

  // Output stream types (PointStream, LineStream, TriangleStream) are
  // translated as their underlying struct types.
  if (hlsl::IsHLSLStreamOutputType(type))
    type = hlsl::GetHLSLResourceResultType(type);

  const auto *sigPoint =
      deduceSigPoint(decl, /*asInput=*/false, shaderModel.GetKind(), forPCF);

  // HS output variables are created using the other overload. For the rest,
  // none of them should be created as arrays.
  assert(sigPoint->GetKind() != hlsl::DXIL::SigPointKind::HSCPOut);

  SemanticInfo inheritSemantic = {};

  return createStageVars(sigPoint, decl, /*asInput=*/false, type,
                         /*arraySize=*/0, "out.var", llvm::None, &storedValue,
                         // Write back of stage output variables in GS is
                         // manually controlled by .Append() intrinsic method,
                         // implemented in writeBackOutputStream(). So
                         // noWriteBack should be set to true for GS.
                         shaderModel.IsGS(), &inheritSemantic);
}

bool DeclResultIdMapper::createStageOutputVar(const DeclaratorDecl *decl,
                                              uint32_t arraySize,
                                              uint32_t invocationId,
                                              uint32_t storedValue) {
  assert(shaderModel.IsHS());

  QualType type = getTypeOrFnRetType(decl);

  const auto *sigPoint =
      hlsl::SigPoint::GetSigPoint(hlsl::DXIL::SigPointKind::HSCPOut);

  SemanticInfo inheritSemantic = {};

  return createStageVars(sigPoint, decl, /*asInput=*/false, type, arraySize,
                         "out.var", invocationId, &storedValue,
                         /*noWriteBack=*/false, &inheritSemantic);
}

bool DeclResultIdMapper::createStageInputVar(const ParmVarDecl *paramDecl,
                                             uint32_t *loadedValue,
                                             bool forPCF) {
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
  if (hasGSPrimitiveTypeQualifier(paramDecl)) {
    const auto *typeDecl = astContext.getAsConstantArrayType(type);
    arraySize = static_cast<uint32_t>(typeDecl->getSize().getZExtValue());
    type = typeDecl->getElementType();
  }

  const auto *sigPoint = deduceSigPoint(paramDecl, /*asInput=*/true,
                                        shaderModel.GetKind(), forPCF);

  SemanticInfo inheritSemantic = {};

  return createStageVars(sigPoint, paramDecl, /*asInput=*/true, type, arraySize,
                         "in.var", llvm::None, loadedValue,
                         /*noWriteBack=*/false, &inheritSemantic);
}

const DeclResultIdMapper::DeclSpirvInfo *
DeclResultIdMapper::getDeclSpirvInfo(const ValueDecl *decl) const {
  auto it = astDecls.find(decl);
  if (it != astDecls.end())
    return &it->second;

  return nullptr;
}

SpirvEvalInfo DeclResultIdMapper::getDeclEvalInfo(const ValueDecl *decl) {
  if (const auto *info = getDeclSpirvInfo(decl)) {
    if (info->indexInCTBuffer >= 0) {
      // If this is a VarDecl inside a HLSLBufferDecl, we need to do an extra
      // OpAccessChain to get the pointer to the variable since we created
      // a single variable for the whole buffer object.

      const uint32_t varType = typeTranslator.translateType(
          // Should only have VarDecls in a HLSLBufferDecl.
          cast<VarDecl>(decl)->getType(),
          // We need to set decorateLayout here to avoid creating SPIR-V
          // instructions for the current type without decorations.
          info->info.getLayoutRule());

      const uint32_t elemId = theBuilder.createAccessChain(
          theBuilder.getPointerType(varType, info->info.getStorageClass()),
          info->info, {theBuilder.getConstantInt32(info->indexInCTBuffer)});

      return info->info.substResultId(elemId);
    } else {
      return *info;
    }
  }

  emitFatalError("found unregistered decl", decl->getLocation())
      << decl->getName();
  emitNote("please file a bug report on "
           "https://github.com/Microsoft/DirectXShaderCompiler/issues with "
           "source code if possible",
           {});
  return 0;
}

uint32_t DeclResultIdMapper::createFnParam(const ParmVarDecl *param) {
  bool isAlias = false;
  auto &info = astDecls[param].info;
  const uint32_t type =
      getTypeAndCreateCounterForPotentialAliasVar(param, &isAlias, &info);
  const uint32_t ptrType =
      theBuilder.getPointerType(type, spv::StorageClass::Function);
  const uint32_t id = theBuilder.addFnParam(ptrType, param->getName());
  info.setResultId(id);

  return id;
}

void DeclResultIdMapper::createCounterVarForDecl(const DeclaratorDecl *decl) {
  const QualType declType = getTypeOrFnRetType(decl);

  if (!counterVars.count(decl) &&
      TypeTranslator::isRWAppendConsumeSBuffer(declType)) {
    createCounterVar(decl, /*declId=*/0, /*isAlias=*/true);
  } else if (!fieldCounterVars.count(decl) && declType->isStructureType() &&
             // Exclude other resource types which are represented as structs
             !hlsl::IsHLSLResourceType(declType)) {
    createFieldCounterVars(decl);
  }
}

SpirvEvalInfo DeclResultIdMapper::createFnVar(const VarDecl *var,
                                              llvm::Optional<uint32_t> init) {
  bool isAlias = false;
  auto &info = astDecls[var].info;
  const uint32_t type =
      getTypeAndCreateCounterForPotentialAliasVar(var, &isAlias, &info);
  const uint32_t id = theBuilder.addFnVar(type, var->getName(), init);
  info.setResultId(id);

  return info;
}

SpirvEvalInfo DeclResultIdMapper::createFileVar(const VarDecl *var,
                                                llvm::Optional<uint32_t> init) {
  bool isAlias = false;
  auto &info = astDecls[var].info;
  const uint32_t type =
      getTypeAndCreateCounterForPotentialAliasVar(var, &isAlias, &info);
  const uint32_t id = theBuilder.addModuleVar(type, spv::StorageClass::Private,
                                              var->getName(), init);
  info.setResultId(id).setStorageClass(spv::StorageClass::Private);

  return info;
}

SpirvEvalInfo DeclResultIdMapper::createExternVar(const VarDecl *var) {
  auto storageClass = spv::StorageClass::UniformConstant;
  auto rule = LayoutRule::Void;
  bool isACRWSBuffer = false; // Whether is {Append|Consume|RW}StructuredBuffer

  if (var->getAttr<HLSLGroupSharedAttr>()) {
    // For CS groupshared variables
    storageClass = spv::StorageClass::Workgroup;
  } else if (TypeTranslator::isResourceType(var)) {
    // See through the possible outer arrays
    QualType resourceType = var->getType();
    while (resourceType->isArrayType()) {
      resourceType = resourceType->getAsArrayTypeUnsafe()->getElementType();
    }

    const llvm::StringRef typeName =
        resourceType->getAs<RecordType>()->getDecl()->getName();

    // These types are all translated into OpTypeStruct with BufferBlock
    // decoration. They should follow standard storage buffer layout,
    // which GLSL std430 rules statisfies.
    if (typeName == "StructuredBuffer" || typeName == "ByteAddressBuffer" ||
        typeName == "RWByteAddressBuffer") {
      storageClass = spv::StorageClass::Uniform;
      rule = spirvOptions.sBufferLayoutRule;
    } else if (typeName == "RWStructuredBuffer" ||
               typeName == "AppendStructuredBuffer" ||
               typeName == "ConsumeStructuredBuffer") {
      storageClass = spv::StorageClass::Uniform;
      rule = spirvOptions.sBufferLayoutRule;
      isACRWSBuffer = true;
    }
  } else {
    // This is a stand-alone externally-visiable non-resource-type variable.
    // They should be grouped into the $Globals cbuffer. We create that cbuffer
    // and record all variables inside it upon seeing the first such variable.
    if (astDecls.count(var) == 0)
      createGlobalsCBuffer(var);

    return astDecls[var].info;
  }

  uint32_t varType = typeTranslator.translateType(var->getType(), rule);

  // Require corresponding capability for accessing 16-bit data.
  if (storageClass == spv::StorageClass::Uniform &&
      spirvOptions.enable16BitTypes &&
      typeTranslator.isOrContains16BitType(var->getType())) {
    theBuilder.addExtension(Extension::KHR_16bit_storage,
                            "16-bit types in resource", var->getLocation());
    theBuilder.requireCapability(spv::Capability::StorageUniformBufferBlock16);
  }

  const uint32_t id = theBuilder.addModuleVar(varType, storageClass,
                                              var->getName(), llvm::None);
  const auto info =
      SpirvEvalInfo(id).setStorageClass(storageClass).setLayoutRule(rule);
  astDecls[var] = info;

  // Variables in Workgroup do not need descriptor decorations.
  if (storageClass == spv::StorageClass::Workgroup)
    return info;

  const auto *regAttr = getResourceBinding(var);
  const auto *bindingAttr = var->getAttr<VKBindingAttr>();
  const auto *counterBindingAttr = var->getAttr<VKCounterBindingAttr>();

  resourceVars.emplace_back(id, regAttr, bindingAttr, counterBindingAttr);

  if (const auto *inputAttachment = var->getAttr<VKInputAttachmentIndexAttr>())
    theBuilder.decorateInputAttachmentIndex(id, inputAttachment->getIndex());

  if (isACRWSBuffer) {
    // For {Append|Consume|RW}StructuredBuffer, we need to always create another
    // variable for its associated counter.
    createCounterVar(var, id, /*isAlias=*/false);
  }

  return info;
}

uint32_t DeclResultIdMapper::getMatrixStructType(const VarDecl *matVar,
                                                 spv::StorageClass sc,
                                                 LayoutRule rule) {
  const auto matType = matVar->getType();
  assert(TypeTranslator::isMxNMatrix(matType));

  auto &context = *theBuilder.getSPIRVContext();
  llvm::SmallVector<const Decoration *, 4> decorations;
  const bool isRowMajor = typeTranslator.isRowMajorMatrix(matType);

  uint32_t stride;
  (void)typeTranslator.getAlignmentAndSize(matType, rule, &stride);
  decorations.push_back(Decoration::getOffset(context, 0, 0));
  decorations.push_back(Decoration::getMatrixStride(context, stride, 0));
  decorations.push_back(isRowMajor ? Decoration::getColMajor(context, 0)
                                   : Decoration::getRowMajor(context, 0));
  decorations.push_back(Decoration::getBlock(context));

  // Get the type for the wrapping struct
  const std::string structName = "type." + matVar->getName().str();
  return theBuilder.getStructType({typeTranslator.translateType(matType)},
                                  structName, {}, decorations);
}

uint32_t DeclResultIdMapper::createStructOrStructArrayVarOfExplicitLayout(
    const DeclContext *decl, int arraySize, const ContextUsageKind usageKind,
    llvm::StringRef typeName, llvm::StringRef varName) {
  // cbuffers are translated into OpTypeStruct with Block decoration.
  // tbuffers are translated into OpTypeStruct with BufferBlock decoration.
  // Push constants are translated into OpTypeStruct with Block decoration.
  //
  // Both cbuffers and tbuffers have the SPIR-V Uniform storage class.
  // Push constants have the SPIR-V PushConstant storage class.

  const bool forCBuffer = usageKind == ContextUsageKind::CBuffer;
  const bool forTBuffer = usageKind == ContextUsageKind::TBuffer;
  const bool forGlobals = usageKind == ContextUsageKind::Globals;
  const bool forPC = usageKind == ContextUsageKind::PushConstant;

  auto &context = *theBuilder.getSPIRVContext();
  const LayoutRule layoutRule =
      (forCBuffer || forGlobals)
          ? spirvOptions.cBufferLayoutRule
          : (forTBuffer ? spirvOptions.tBufferLayoutRule
                        : spirvOptions.sBufferLayoutRule);
  const auto *blockDec = forTBuffer ? Decoration::getBufferBlock(context)
                                    : Decoration::getBlock(context);

  const llvm::SmallVector<const Decl *, 4> &declGroup =
      typeTranslator.collectDeclsInDeclContext(decl);
  auto decorations = typeTranslator.getLayoutDecorations(declGroup, layoutRule);
  decorations.push_back(blockDec);

  // Collect the type and name for each field
  llvm::SmallVector<uint32_t, 4> fieldTypes;
  llvm::SmallVector<llvm::StringRef, 4> fieldNames;
  uint32_t fieldIndex = 0;
  for (const auto *subDecl : declGroup) {
    // The field can only be FieldDecl (for normal structs) or VarDecl (for
    // HLSLBufferDecls).
    assert(isa<VarDecl>(subDecl) || isa<FieldDecl>(subDecl));
    const auto *declDecl = cast<DeclaratorDecl>(subDecl);

    // All fields are qualified with const. It will affect the debug name.
    // We don't need it here.
    auto varType = declDecl->getType();
    varType.removeLocalConst();

    fieldTypes.push_back(typeTranslator.translateType(varType, layoutRule));
    fieldNames.push_back(declDecl->getName());

    // Require corresponding capability for accessing 16-bit data.
    if (spirvOptions.enable16BitTypes &&
        typeTranslator.isOrContains16BitType(varType)) {
      theBuilder.addExtension(Extension::KHR_16bit_storage,
                              "16-bit types in resource",
                              declDecl->getLocation());
      theBuilder.requireCapability(
          (forCBuffer || forGlobals)
              ? spv::Capability::StorageUniform16
              : forPC ? spv::Capability::StoragePushConstant16
                      : spv::Capability::StorageUniformBufferBlock16);
    }

    // tbuffer/TextureBuffers are non-writable SSBOs. OpMemberDecorate
    // NonWritable must be applied to all fields.
    if (forTBuffer) {
      decorations.push_back(Decoration::getNonWritable(
          *theBuilder.getSPIRVContext(), fieldIndex));
    }
    ++fieldIndex;
  }

  // Get the type for the whole struct
  uint32_t resultType =
      theBuilder.getStructType(fieldTypes, typeName, fieldNames, decorations);

  // Make an array if requested.
  if (arraySize > 0) {
    resultType = theBuilder.getArrayType(
        resultType, theBuilder.getConstantUint32(arraySize));
  } else if (arraySize == -1) {
    // Runtime arrays of cbuffer/tbuffer needs additional capability.
    theBuilder.addExtension(Extension::EXT_descriptor_indexing,
                            "runtime array of resources", {});
    theBuilder.requireCapability(spv::Capability::RuntimeDescriptorArrayEXT);
    resultType = theBuilder.getRuntimeArrayType(resultType);
  }

  // Register the <type-id> for this decl
  ctBufferPCTypeIds[decl] = resultType;

  const auto sc =
      forPC ? spv::StorageClass::PushConstant : spv::StorageClass::Uniform;

  // Create the variable for the whole struct / struct array.
  return theBuilder.addModuleVar(resultType, sc, varName);
}

uint32_t DeclResultIdMapper::createCTBuffer(const HLSLBufferDecl *decl) {
  const auto usageKind =
      decl->isCBuffer() ? ContextUsageKind::CBuffer : ContextUsageKind::TBuffer;
  const std::string structName = "type." + decl->getName().str();
  // The front-end does not allow arrays of cbuffer/tbuffer.
  const uint32_t bufferVar = createStructOrStructArrayVarOfExplicitLayout(
      decl, /*arraySize*/ 0, usageKind, structName, decl->getName());

  // We still register all VarDecls seperately here. All the VarDecls are
  // mapped to the <result-id> of the buffer object, which means when querying
  // querying the <result-id> for a certain VarDecl, we need to do an extra
  // OpAccessChain.
  int index = 0;
  for (const auto *subDecl : decl->decls()) {
    if (TypeTranslator::shouldSkipInStructLayout(subDecl))
      continue;

    const auto *varDecl = cast<VarDecl>(subDecl);
    astDecls[varDecl] =
        SpirvEvalInfo(bufferVar)
            .setStorageClass(spv::StorageClass::Uniform)
            .setLayoutRule(decl->isCBuffer() ? spirvOptions.cBufferLayoutRule
                                             : spirvOptions.tBufferLayoutRule);
    astDecls[varDecl].indexInCTBuffer = index++;
  }
  resourceVars.emplace_back(bufferVar, getResourceBinding(decl),
                            decl->getAttr<VKBindingAttr>(),
                            decl->getAttr<VKCounterBindingAttr>());

  return bufferVar;
}

uint32_t DeclResultIdMapper::createCTBuffer(const VarDecl *decl) {
  const RecordType *recordType = nullptr;
  int arraySize = 0;

  // In case we have an array of ConstantBuffer/TextureBuffer:
  if (const auto *arrayType = decl->getType()->getAsArrayTypeUnsafe()) {
    recordType = arrayType->getElementType()->getAs<RecordType>();
    if (const auto *caType =
            astContext.getAsConstantArrayType(decl->getType())) {
      arraySize = static_cast<uint32_t>(caType->getSize().getZExtValue());
    } else {
      arraySize = -1;
    }
  } else {
    recordType = decl->getType()->getAs<RecordType>();
  }
  if (!recordType) {
    emitError("constant/texture buffer type %0 unimplemented",
              decl->getLocStart())
        << decl->getType();
    return 0;
  }

  const auto *context = cast<HLSLBufferDecl>(decl->getDeclContext());
  const auto usageKind = context->isCBuffer() ? ContextUsageKind::CBuffer
                                              : ContextUsageKind::TBuffer;

  const char *ctBufferName =
      context->isCBuffer() ? "ConstantBuffer." : "TextureBuffer.";
  const std::string structName = "type." + std::string(ctBufferName) +
                                 recordType->getDecl()->getName().str();

  const uint32_t bufferVar = createStructOrStructArrayVarOfExplicitLayout(
      recordType->getDecl(), arraySize, usageKind, structName, decl->getName());

  // We register the VarDecl here.
  astDecls[decl] =
      SpirvEvalInfo(bufferVar)
          .setStorageClass(spv::StorageClass::Uniform)
          .setLayoutRule(context->isCBuffer() ? spirvOptions.cBufferLayoutRule
                                              : spirvOptions.tBufferLayoutRule);
  resourceVars.emplace_back(bufferVar, getResourceBinding(context),
                            decl->getAttr<VKBindingAttr>(),
                            decl->getAttr<VKCounterBindingAttr>());

  return bufferVar;
}

uint32_t DeclResultIdMapper::createPushConstant(const VarDecl *decl) {
  // The front-end errors out if non-struct type push constant is used.
  const auto *recordType = decl->getType()->getAs<RecordType>();
  assert(recordType);

  const std::string structName =
      "type.PushConstant." + recordType->getDecl()->getName().str();
  const uint32_t var = createStructOrStructArrayVarOfExplicitLayout(
      recordType->getDecl(), /*arraySize*/ 0, ContextUsageKind::PushConstant,
      structName, decl->getName());

  // Register the VarDecl
  astDecls[decl] = SpirvEvalInfo(var)
                       .setStorageClass(spv::StorageClass::PushConstant)
                       .setLayoutRule(spirvOptions.sBufferLayoutRule);
  // Do not push this variable into resourceVars since it does not need
  // descriptor set.

  return var;
}

void DeclResultIdMapper::createGlobalsCBuffer(const VarDecl *var) {
  if (astDecls.count(var) != 0)
    return;

  const auto *context = var->getTranslationUnitDecl();
  const uint32_t globals = createStructOrStructArrayVarOfExplicitLayout(
      context, /*arraySize*/ 0, ContextUsageKind::Globals, "type.$Globals",
      "$Globals");

  resourceVars.emplace_back(globals, nullptr, nullptr, nullptr);

  uint32_t index = 0;
  for (const auto *decl : typeTranslator.collectDeclsInDeclContext(context))
    if (const auto *varDecl = dyn_cast<VarDecl>(decl)) {
      if (!spirvOptions.noWarnIgnoredFeatures) {
        if (const auto *init = varDecl->getInit())
          emitWarning(
              "variable '%0' will be placed in $Globals so initializer ignored",
              init->getExprLoc())
              << var->getName() << init->getSourceRange();
      }
      if (const auto *attr = varDecl->getAttr<VKBindingAttr>()) {
        emitError("variable '%0' will be placed in $Globals so cannot have "
                  "vk::binding attribute",
                  attr->getLocation())
            << var->getName();
        return;
      }

      astDecls[varDecl] = SpirvEvalInfo(globals)
                              .setStorageClass(spv::StorageClass::Uniform)
                              .setLayoutRule(spirvOptions.cBufferLayoutRule);
      astDecls[varDecl].indexInCTBuffer = index++;
    }
}

uint32_t DeclResultIdMapper::getOrRegisterFnResultId(const FunctionDecl *fn) {
  if (const auto *info = getDeclSpirvInfo(fn))
    return info->info;

  auto &info = astDecls[fn].info;

  bool isAlias = false;
  const uint32_t type =
      getTypeAndCreateCounterForPotentialAliasVar(fn, &isAlias, &info);

  const uint32_t id = theBuilder.getSPIRVContext()->takeNextId();
  info.setResultId(id);
  // No need to dereference to get the pointer. Function returns that are
  // stand-alone aliases are already pointers to values. All other cases should
  // be normal rvalues.
  if (!isAlias ||
      !TypeTranslator::isAKindOfStructuredOrByteBuffer(fn->getReturnType()))
    info.setRValue();

  return id;
}

const CounterIdAliasPair *DeclResultIdMapper::getCounterIdAliasPair(
    const DeclaratorDecl *decl, const llvm::SmallVector<uint32_t, 4> *indices) {
  if (!decl)
    return nullptr;

  if (indices) {
    // Indices are provided. Walk through the fields of the decl.
    const auto counter = fieldCounterVars.find(decl);
    if (counter != fieldCounterVars.end())
      return counter->second.get(*indices);
  } else {
    // No indices. Check the stand-alone entities.
    const auto counter = counterVars.find(decl);
    if (counter != counterVars.end())
      return &counter->second;
  }

  return nullptr;
}

const CounterVarFields *
DeclResultIdMapper::getCounterVarFields(const DeclaratorDecl *decl) {
  if (!decl)
    return nullptr;

  const auto found = fieldCounterVars.find(decl);
  if (found != fieldCounterVars.end())
    return &found->second;

  return nullptr;
}

void DeclResultIdMapper::registerSpecConstant(const VarDecl *decl,
                                              uint32_t specConstant) {
  astDecls[decl].info.setResultId(specConstant).setRValue().setSpecConstant();
}

void DeclResultIdMapper::createCounterVar(
    const DeclaratorDecl *decl, uint32_t declId, bool isAlias,
    const llvm::SmallVector<uint32_t, 4> *indices) {
  std::string counterName = "counter.var." + decl->getName().str();
  if (indices) {
    // Append field indices to the name
    for (const auto index : *indices)
      counterName += "." + std::to_string(index);
  }

  uint32_t counterType = typeTranslator.getACSBufferCounter();
  // {RW|Append|Consume}StructuredBuffer are all in Uniform storage class.
  // Alias counter variables should be created into the Private storage class.
  const spv::StorageClass sc =
      isAlias ? spv::StorageClass::Private : spv::StorageClass::Uniform;

  if (isAlias) {
    // Apply an extra level of pointer for alias counter variable
    counterType =
        theBuilder.getPointerType(counterType, spv::StorageClass::Uniform);
  }

  const uint32_t counterId =
      theBuilder.addModuleVar(counterType, sc, counterName);

  if (!isAlias) {
    // Non-alias counter variables should be put in to resourceVars so that
    // descriptors can be allocated for them.
    resourceVars.emplace_back(counterId, getResourceBinding(decl),
                              decl->getAttr<VKBindingAttr>(),
                              decl->getAttr<VKCounterBindingAttr>(), true);
    assert(declId);
    theBuilder.decorateCounterBufferId(declId, counterId);
  }

  if (indices)
    fieldCounterVars[decl].append(*indices, counterId);
  else
    counterVars[decl] = {counterId, isAlias};
}

void DeclResultIdMapper::createFieldCounterVars(
    const DeclaratorDecl *rootDecl, const DeclaratorDecl *decl,
    llvm::SmallVector<uint32_t, 4> *indices) {
  const QualType type = getTypeOrFnRetType(decl);
  const auto *recordType = type->getAs<RecordType>();
  assert(recordType);
  const auto *recordDecl = recordType->getDecl();

  for (const auto *field : recordDecl->fields()) {
    // Build up the index chain
    indices->push_back(getNumBaseClasses(type) + field->getFieldIndex());

    const QualType fieldType = field->getType();
    if (TypeTranslator::isRWAppendConsumeSBuffer(fieldType))
      createCounterVar(rootDecl, /*declId=*/0, /*isAlias=*/true, indices);
    else if (fieldType->isStructureType() &&
             !hlsl::IsHLSLResourceType(fieldType))
      // Go recursively into all nested structs
      createFieldCounterVars(rootDecl, field, indices);

    indices->pop_back();
  }
}

uint32_t
DeclResultIdMapper::getCTBufferPushConstantTypeId(const DeclContext *decl) {
  const auto found = ctBufferPCTypeIds.find(decl);
  assert(found != ctBufferPCTypeIds.end());
  return found->second;
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
  /// Maximum number of indices supported
  const static uint32_t kMaxIndex = 2;
  /// Maximum number of locations supported
  // Typically we won't have that many stage input or output variables.
  // Using 64 should be fine here.
  const static uint32_t kMaxLoc = 64;

  LocationSet() {
    for (uint32_t i = 0; i < kMaxIndex; ++i) {
      usedLocs[i].resize(kMaxLoc);
      nextLoc[i] = 0;
    }
  }

  /// Uses the given location.
  void useLoc(uint32_t loc, uint32_t index = 0) {
    assert(index < kMaxIndex);
    usedLocs[index].set(loc);
  }

  /// Uses the next |count| available location.
  int useNextLocs(uint32_t count, uint32_t index = 0) {
    assert(index < kMaxIndex);
    auto &locs = usedLocs[index];
    auto &next = nextLoc[index];
    while (locs[next])
      next++;

    int toUse = next;

    for (uint32_t i = 0; i < count; ++i) {
      assert(!locs[next]);
      locs.set(next++);
    }

    return toUse;
  }

  /// Returns true if the given location number is already used.
  bool isLocUsed(uint32_t loc, uint32_t index = 0) {
    assert(index < kMaxIndex);
    return usedLocs[index][loc];
  }

private:
  llvm::SmallBitVector usedLocs[kMaxIndex]; ///< All previously used locations
  uint32_t nextLoc[kMaxIndex];              ///< Next available location
};

/// A class for managing resource bindings to avoid duplicate uses of the same
/// set and binding number.
class BindingSet {
public:
  /// Uses the given set and binding number.
  void useBinding(uint32_t binding, uint32_t set) {
    usedBindings[set].insert(binding);
  }

  /// Uses the next avaiable binding number in set 0.
  uint32_t useNextBinding(uint32_t set) {
    auto &binding = usedBindings[set];
    auto &next = nextBindings[set];
    while (binding.count(next))
      ++next;
    binding.insert(next);
    return next++;
  }

private:
  ///< set number -> set of used binding number
  llvm::DenseMap<uint32_t, llvm::DenseSet<uint32_t>> usedBindings;
  ///< set number -> next available binding number
  llvm::DenseMap<uint32_t, uint32_t> nextBindings;
};
} // namespace

bool DeclResultIdMapper::checkSemanticDuplication(bool forInput) {
  llvm::StringSet<> seenSemantics;
  bool success = true;
  for (const auto &var : stageVars) {
    auto s = var.getSemanticStr();

    if (s.empty()) {
      // We translate WaveGetLaneCount() and WaveGetLaneIndex() into builtin
      // variables. Those variables are inserted into the normal stage IO
      // processing pipeline, but with the semantics as empty strings.
      assert(var.isSpirvBuitin());
      continue;
    }

    if (forInput && var.getSigPoint()->IsInput()) {
      if (seenSemantics.count(s)) {
        emitError("input semantic '%0' used more than once", {}) << s;
        success = false;
      }
      seenSemantics.insert(s);
    } else if (!forInput && var.getSigPoint()->IsOutput()) {
      if (seenSemantics.count(s)) {
        emitError("output semantic '%0' used more than once", {}) << s;
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
      // Skip builtins & those stage variables we are not handling for this call
      if (var.isSpirvBuitin() || forInput != isInputStorageClass(var))
        continue;

      const auto *attr = var.getLocationAttr();
      const auto loc = attr->getNumber();
      const auto attrLoc = attr->getLocation(); // Attr source code location
      const auto idx = var.getIndexAttr() ? var.getIndexAttr()->getNumber() : 0;

      if (loc >= LocationSet::kMaxLoc) {
        emitError("stage %select{output|input}0 location #%1 too large",
                  attrLoc)
            << forInput << loc;
        return false;
      }

      // Make sure the same location is not assigned more than once
      if (locSet.isLocUsed(loc, idx)) {
        emitError("stage %select{output|input}0 location #%1 already assigned",
                  attrLoc)
            << forInput << loc;
        noError = false;
      }
      locSet.useLoc(loc, idx);

      theBuilder.decorateLocation(var.getSpirvId(), loc);
      if (var.getIndexAttr())
        theBuilder.decorateIndex(var.getSpirvId(), idx);
    }

    return noError;
  }

  std::vector<const StageVar *> vars;
  LocationSet locSet;

  for (const auto &var : stageVars) {
    if (var.isSpirvBuitin() || forInput != isInputStorageClass(var))
      continue;

    if (var.getLocationAttr()) {
      // We have checked that not all of the stage variables have explicit
      // location assignment.
      emitError("partial explicit stage %select{output|input}0 location "
                "assignment via vk::location(X) unsupported",
                {})
          << forInput;
      return false;
    }

    const auto &semaInfo = var.getSemanticInfo();

    // We should special rules for SV_Target: the location number comes from the
    // semantic string index.
    if (semaInfo.isTarget()) {
      theBuilder.decorateLocation(var.getSpirvId(), semaInfo.index);
      locSet.useLoc(semaInfo.index);
    } else {
      vars.push_back(&var);
    }
  }

  // If alphabetical ordering was requested, sort by semantic string.
  // Since HS includes 2 sets of outputs (patch-constant output and
  // OutputPatch), running into location mismatches between HS and DS is very
  // likely. In order to avoid location mismatches between HS and DS, use
  // alphabetical ordering.
  if (spirvOptions.stageIoOrder == "alpha" ||
      (!forInput && shaderModel.IsHS()) || (forInput && shaderModel.IsDS())) {
    // Sort stage input/output variables alphabetically
    std::sort(vars.begin(), vars.end(),
              [](const StageVar *a, const StageVar *b) {
                return a->getSemanticStr() < b->getSemanticStr();
              });
  }

  for (const auto *var : vars)
    theBuilder.decorateLocation(var->getSpirvId(),
                                locSet.useNextLocs(var->getLocationCount()));

  return true;
}

namespace {
/// A class for maintaining the binding number shift requested for descriptor
/// sets.
class BindingShiftMapper {
public:
  explicit BindingShiftMapper(const llvm::SmallVectorImpl<int32_t> &shifts)
      : masterShift(0) {
    assert(shifts.size() % 2 == 0);
    if (shifts.size() == 2 && shifts[1] == -1) {
      masterShift = shifts[0];
    } else {
      for (uint32_t i = 0; i < shifts.size(); i += 2)
        perSetShift[shifts[i + 1]] = shifts[i];
    }
  }

  /// Returns the shift amount for the given set.
  int32_t getShiftForSet(int32_t set) const {
    const auto found = perSetShift.find(set);
    if (found != perSetShift.end())
      return found->second;
    return masterShift;
  }

private:
  uint32_t masterShift; /// Shift amount applies to all sets.
  llvm::DenseMap<int32_t, int32_t> perSetShift;
};
} // namespace

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

  // Decorates the given varId of the given category with set number
  // setNo, binding number bindingNo. Ignores overlaps.
  const auto tryToDecorate = [this, &bindingSet](const uint32_t varId,
                                                 const uint32_t setNo,
                                                 const uint32_t bindingNo) {
    bindingSet.useBinding(bindingNo, setNo);
    theBuilder.decorateDSetBinding(varId, setNo, bindingNo);
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

        tryToDecorate(var.getSpirvId(), set, vkCBinding->getBinding());
      }
    } else {
      if (const auto *vkBinding = var.getBinding()) {
        // Process m1
        tryToDecorate(var.getSpirvId(), vkBinding->getSet(),
                      vkBinding->getBinding());
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

        tryToDecorate(var.getSpirvId(), set, binding);
      }

  for (const auto &var : resourceVars) {
    if (var.isCounter()) {
      if (!var.getCounterBinding()) {
        // Process mX * c2
        uint32_t set = 0;
        if (const auto *vkBinding = var.getBinding())
          set = vkBinding->getSet();
        else if (const auto *reg = var.getRegister())
          set = reg->RegisterSpace;

        theBuilder.decorateDSetBinding(var.getSpirvId(), set,
                                       bindingSet.useNextBinding(set));
      }
    } else if (!var.getBinding() && !var.getRegister()) {
      // Process m3
      theBuilder.decorateDSetBinding(var.getSpirvId(), 0,
                                     bindingSet.useNextBinding(0));
    }
  }

  return true;
}

bool DeclResultIdMapper::createStageVars(const hlsl::SigPoint *sigPoint,
                                         const NamedDecl *decl, bool asInput,
                                         QualType type, uint32_t arraySize,
                                         const llvm::StringRef namePrefix,
                                         llvm::Optional<uint32_t> invocationId,
                                         uint32_t *value, bool noWriteBack,
                                         SemanticInfo *inheritSemantic) {
  // invocationId should only be used for handling HS per-vertex output.
  if (invocationId.hasValue()) {
    assert(shaderModel.IsHS() && arraySize != 0 && !asInput);
  }

  assert(inheritSemantic);

  if (type->isVoidType()) {
    // No stage variables will be created for void type.
    return true;
  }

  uint32_t typeId = typeTranslator.translateType(type);

  // We have several cases regarding HLSL semantics to handle here:
  // * If the currrent decl inherits a semantic from some enclosing entity,
  //   use the inherited semantic no matter whether there is a semantic
  //   attached to the current decl.
  // * If there is no semantic to inherit,
  //   * If the current decl is a struct,
  //     * If the current decl has a semantic, all its members inhert this
  //       decl's semantic, with the index sequentially increasing;
  //     * If the current decl does not have a semantic, all its members
  //       should have semantics attached;
  //   * If the current decl is not a struct, it should have semantic attached.

  auto thisSemantic = getStageVarSemantic(decl);

  // Which semantic we should use for this decl
  auto *semanticToUse = &thisSemantic;

  // Enclosing semantics override internal ones
  if (inheritSemantic->isValid()) {
    if (thisSemantic.isValid()) {
      emitWarning(
          "internal semantic '%0' overridden by enclosing semantic '%1'",
          thisSemantic.loc)
          << thisSemantic.str << inheritSemantic->str;
    }
    semanticToUse = inheritSemantic;
  }

  if (semanticToUse->isValid() &&
      // Structs with attached semantics will be handled later.
      !type->isStructureType()) {
    // Found semantic attached directly to this Decl. This means we need to
    // map this decl to a single stage variable.

    if (!validateVKAttributes(decl))
      return false;

    const auto semanticKind = semanticToUse->getKind();

    // Error out when the given semantic is invalid in this shader model
    if (hlsl::SigPoint::GetInterpretation(semanticKind, sigPoint->GetKind(),
                                          shaderModel.GetMajor(),
                                          shaderModel.GetMinor()) ==
        hlsl::DXIL::SemanticInterpretationKind::NA) {
      emitError("invalid usage of semantic '%0' in shader profile %1",
                decl->getLocation())
          << semanticToUse->str << shaderModel.GetName();
      return false;
    }

    if (!validateVKBuiltins(decl, sigPoint))
      return false;

    const auto *builtinAttr = decl->getAttr<VKBuiltInAttr>();

    // Special handling of certain mappings between HLSL semantics and
    // SPIR-V builtins:
    // * SV_CullDistance/SV_ClipDistance are outsourced to GlPerVertex.
    // * SV_DomainLocation can refer to a float2, whereas TessCoord is a float3.
    //   To ensure SPIR-V validity, we must create a float3 and  extract a
    //   float2 from it before passing it to the main function.
    // * SV_TessFactor is an array of size 2 for isoline patch, array of size 3
    //   for tri patch, and array of size 4 for quad patch, but it must always
    //   be an array of size 4 in SPIR-V for Vulkan.
    // * SV_InsideTessFactor is a single float for tri patch, and an array of
    //   size 2 for a quad patch, but it must always be an array of size 2 in
    //   SPIR-V for Vulkan.
    // * SV_Coverage is an uint value, but the builtin it corresponds to,
    //   SampleMask, must be an array of integers.
    // * SV_InnerCoverage is an uint value, but the corresponding builtin,
    //   FullyCoveredEXT, must be an boolean value.
    // * SV_DispatchThreadID, SV_GroupThreadID, and SV_GroupID are allowed to be
    //   uint, uint2, or uint3, but the corresponding builtins
    //   (GlobalInvocationId, LocalInvocationId, WorkgroupId) must be a uint3.

    if (glPerVertex.tryToAccess(sigPoint->GetKind(), semanticKind,
                                semanticToUse->index, invocationId, value,
                                noWriteBack))
      return true;

    const uint32_t srcTypeId = typeId; // Variable type in source code
    uint32_t srcVecElemTypeId = 0;     // Variable element type if vector

    switch (semanticKind) {
    case hlsl::Semantic::Kind::DomainLocation:
      typeId = theBuilder.getVecType(theBuilder.getFloat32Type(), 3);
      break;
    case hlsl::Semantic::Kind::TessFactor:
      typeId = theBuilder.getArrayType(theBuilder.getFloat32Type(),
                                       theBuilder.getConstantUint32(4));
      break;
    case hlsl::Semantic::Kind::InsideTessFactor:
      typeId = theBuilder.getArrayType(theBuilder.getFloat32Type(),
                                       theBuilder.getConstantUint32(2));
      break;
    case hlsl::Semantic::Kind::Coverage:
      typeId = theBuilder.getArrayType(typeId, theBuilder.getConstantUint32(1));
      break;
    case hlsl::Semantic::Kind::InnerCoverage:
      typeId = theBuilder.getBoolType();
      break;
    case hlsl::Semantic::Kind::Barycentrics:
      typeId = theBuilder.getVecType(theBuilder.getFloat32Type(), 2);
      break;
    case hlsl::Semantic::Kind::DispatchThreadID:
    case hlsl::Semantic::Kind::GroupThreadID:
    case hlsl::Semantic::Kind::GroupID:
      // Keep the original integer signedness
      srcVecElemTypeId = typeTranslator.translateType(
          hlsl::IsHLSLVecType(type) ? hlsl::GetHLSLVecElementType(type) : type);
      typeId = theBuilder.getVecType(srcVecElemTypeId, 3);
      break;
    }

    // Handle the extra arrayness
    const uint32_t elementTypeId = typeId; // Array element's type
    if (arraySize != 0)
      typeId = theBuilder.getArrayType(typeId,
                                       theBuilder.getConstantUint32(arraySize));

    StageVar stageVar(
        sigPoint, *semanticToUse, builtinAttr, typeId,
        // For HS/DS/GS, we have already stripped the outmost arrayness on type.
        typeTranslator.getLocationCount(type));
    const auto name = namePrefix.str() + "." + stageVar.getSemanticStr();
    const uint32_t varId =
        createSpirvStageVar(&stageVar, decl, name, semanticToUse->loc);

    if (varId == 0)
      return false;

    stageVar.setSpirvId(varId);
    stageVar.setLocationAttr(decl->getAttr<VKLocationAttr>());
    stageVar.setIndexAttr(decl->getAttr<VKIndexAttr>());
    stageVars.push_back(stageVar);

    // Emit OpDecorate* instructions to link this stage variable with the HLSL
    // semantic it is created for
    theBuilder.decorateHlslSemantic(varId, stageVar.getSemanticInfo().str);

    // We have semantics attached to this decl, which means it must be a
    // function/parameter/variable. All are DeclaratorDecls.
    stageVarIds[cast<DeclaratorDecl>(decl)] = varId;

    // Mark that we have used one index for this semantic
    ++semanticToUse->index;

    // Require extension and capability if using 16-bit types
    if (typeTranslator.getElementSpirvBitwidth(type) == 16) {
      theBuilder.addExtension(Extension::KHR_16bit_storage,
                              "16-bit stage IO variables", decl->getLocation());
      theBuilder.requireCapability(spv::Capability::StorageInputOutput16);
    }

    // TODO: the following may not be correct?
    if (sigPoint->GetSignatureKind() ==
        hlsl::DXIL::SignatureKind::PatchConstant)
      theBuilder.decorate(varId, spv::Decoration::Patch);

    // Decorate with interpolation modes for pixel shader input variables
    if (shaderModel.IsPS() && sigPoint->IsInput() &&
        // BaryCoord*AMD buitins already encode the interpolation mode.
        semanticKind != hlsl::Semantic::Kind::Barycentrics)
      decoratePSInterpolationMode(decl, type, varId);

    if (asInput) {
      *value = theBuilder.createLoad(typeId, varId);

      // Fix ups for corner cases

      // Special handling of SV_TessFactor DS patch constant input.
      // TessLevelOuter is always an array of size 4 in SPIR-V, but
      // SV_TessFactor could be an array of size 2, 3, or 4 in HLSL. Only the
      // relevant indexes must be loaded.
      if (semanticKind == hlsl::Semantic::Kind::TessFactor &&
          hlsl::GetArraySize(type) != 4) {
        llvm::SmallVector<uint32_t, 4> components;
        const auto f32TypeId = theBuilder.getFloat32Type();
        const auto tessFactorSize = hlsl::GetArraySize(type);
        const auto arrType = theBuilder.getArrayType(
            f32TypeId, theBuilder.getConstantUint32(tessFactorSize));
        for (uint32_t i = 0; i < tessFactorSize; ++i)
          components.push_back(
              theBuilder.createCompositeExtract(f32TypeId, *value, {i}));
        *value = theBuilder.createCompositeConstruct(arrType, components);
      }
      // Special handling of SV_InsideTessFactor DS patch constant input.
      // TessLevelInner is always an array of size 2 in SPIR-V, but
      // SV_InsideTessFactor could be an array of size 1 (scalar) or size 2 in
      // HLSL. If SV_InsideTessFactor is a scalar, only extract index 0 of
      // TessLevelInner.
      else if (semanticKind == hlsl::Semantic::Kind::InsideTessFactor &&
               // Some developers use float[1] instead of a scalar float.
               (!type->isArrayType() || hlsl::GetArraySize(type) == 1)) {
        const auto f32Type = theBuilder.getFloat32Type();
        *value = theBuilder.createCompositeExtract(f32Type, *value, {0});
        if (type->isArrayType()) // float[1]
          *value = theBuilder.createCompositeConstruct(
              theBuilder.getArrayType(f32Type, theBuilder.getConstantUint32(1)),
              {*value});
      }
      // SV_DomainLocation can refer to a float2 or a float3, whereas TessCoord
      // is always a float3. To ensure SPIR-V validity, a float3 stage variable
      // is created, and we must extract a float2 from it before passing it to
      // the main function.
      else if (semanticKind == hlsl::Semantic::Kind::DomainLocation &&
               hlsl::GetHLSLVecSize(type) != 3) {
        const auto domainLocSize = hlsl::GetHLSLVecSize(type);
        *value = theBuilder.createVectorShuffle(
            theBuilder.getVecType(theBuilder.getFloat32Type(), domainLocSize),
            *value, *value, {0, 1});
      }
      // Special handling of SV_Coverage, which is an uint value. We need to
      // read SampleMask and extract its first element.
      else if (semanticKind == hlsl::Semantic::Kind::Coverage) {
        *value = theBuilder.createCompositeExtract(srcTypeId, *value, {0});
      }
      // Special handling of SV_InnerCoverage, which is an uint value. We need
      // to read FullyCoveredEXT, which is a boolean value, and convert it to an
      // uint value. According to D3D12 "Conservative Rasterization" doc: "The
      // Pixel Shader has a 32-bit scalar integer System Generate Value
      // available: InnerCoverage. This is a bit-field that has bit 0 from the
      // LSB set to 1 for a given conservatively rasterized pixel, only when
      // that pixel is guaranteed to be entirely inside the current primitive.
      // All other input register bits must be set to 0 when bit 0 is not set,
      // but are undefined when bit 0 is set to 1 (essentially, this bit-field
      // represents a Boolean value where false must be exactly 0, but true can
      // be any odd (i.e. bit 0 set) non-zero value)."
      else if (semanticKind == hlsl::Semantic::Kind::InnerCoverage) {
        const auto constOne = theBuilder.getConstantUint32(1);
        const auto constZero = theBuilder.getConstantUint32(0);
        *value = theBuilder.createSelect(theBuilder.getUint32Type(), *value,
                                         constOne, constZero);
      }
      // Special handling of SV_Barycentrics, which is a float3, but the
      // underlying stage input variable is a float2 (only provides the first
      // two components). Calculate the third element.
      else if (semanticKind == hlsl::Semantic::Kind::Barycentrics) {
        const auto f32Type = theBuilder.getFloat32Type();
        const auto x = theBuilder.createCompositeExtract(f32Type, *value, {0});
        const auto y = theBuilder.createCompositeExtract(f32Type, *value, {1});
        const auto xy =
            theBuilder.createBinaryOp(spv::Op::OpFAdd, f32Type, x, y);
        const auto z = theBuilder.createBinaryOp(
            spv::Op::OpFSub, f32Type, theBuilder.getConstantFloat32(1), xy);
        const auto v3f32Type = theBuilder.getVecType(f32Type, 3);

        *value = theBuilder.createCompositeConstruct(v3f32Type, {x, y, z});
      }
      // Special handling of SV_DispatchThreadID and SV_GroupThreadID, which may
      // be a uint or uint2, but the underlying stage input variable is a uint3.
      // The last component(s) should be discarded in needed.
      else if ((semanticKind == hlsl::Semantic::Kind::DispatchThreadID ||
                semanticKind == hlsl::Semantic::Kind::GroupThreadID ||
                semanticKind == hlsl::Semantic::Kind::GroupID) &&
               (!hlsl::IsHLSLVecType(type) ||
                hlsl::GetHLSLVecSize(type) != 3)) {
        assert(srcVecElemTypeId);
        const auto vecSize =
            hlsl::IsHLSLVecType(type) ? hlsl::GetHLSLVecSize(type) : 1;
        if (vecSize == 1)
          *value =
              theBuilder.createCompositeExtract(srcVecElemTypeId, *value, {0});
        else if (vecSize == 2)
          *value = theBuilder.createVectorShuffle(
              theBuilder.getVecType(srcVecElemTypeId, 2), *value, *value,
              {0, 1});
      }
    } else {
      if (noWriteBack)
        return true;

      // Negate SV_Position.y if requested
      if (semanticToUse->semantic->GetKind() == hlsl::Semantic::Kind::Position)
        *value = invertYIfRequested(*value);

      uint32_t ptr = varId;

      // Special handling of SV_TessFactor HS patch constant output.
      // TessLevelOuter is always an array of size 4 in SPIR-V, but
      // SV_TessFactor could be an array of size 2, 3, or 4 in HLSL. Only the
      // relevant indexes must be written to.
      if (semanticKind == hlsl::Semantic::Kind::TessFactor &&
          hlsl::GetArraySize(type) != 4) {
        const auto f32TypeId = theBuilder.getFloat32Type();
        const auto tessFactorSize = hlsl::GetArraySize(type);
        for (uint32_t i = 0; i < tessFactorSize; ++i) {
          const uint32_t ptrType =
              theBuilder.getPointerType(f32TypeId, spv::StorageClass::Output);
          ptr = theBuilder.createAccessChain(ptrType, varId,
                                             theBuilder.getConstantUint32(i));
          theBuilder.createStore(
              ptr, theBuilder.createCompositeExtract(f32TypeId, *value, i));
        }
      }
      // Special handling of SV_InsideTessFactor HS patch constant output.
      // TessLevelInner is always an array of size 2 in SPIR-V, but
      // SV_InsideTessFactor could be an array of size 1 (scalar) or size 2 in
      // HLSL. If SV_InsideTessFactor is a scalar, only write to index 0 of
      // TessLevelInner.
      else if (semanticKind == hlsl::Semantic::Kind::InsideTessFactor &&
               // Some developers use float[1] instead of a scalar float.
               (!type->isArrayType() || hlsl::GetArraySize(type) == 1)) {
        const auto f32Type = theBuilder.getFloat32Type();
        ptr = theBuilder.createAccessChain(
            theBuilder.getPointerType(f32Type, spv::StorageClass::Output),
            varId, theBuilder.getConstantUint32(0));
        if (type->isArrayType()) // float[1]
          *value = theBuilder.createCompositeExtract(f32Type, *value, {0});
        theBuilder.createStore(ptr, *value);
      }
      // Special handling of SV_Coverage, which is an unit value. We need to
      // write it to the first element in the SampleMask builtin.
      else if (semanticKind == hlsl::Semantic::Kind::Coverage) {
        ptr = theBuilder.createAccessChain(
            theBuilder.getPointerType(srcTypeId, spv::StorageClass::Output),
            varId, theBuilder.getConstantUint32(0));
        theBuilder.createStore(ptr, *value);
      }
      // Special handling of HS ouput, for which we write to only one
      // element in the per-vertex data array: the one indexed by
      // SV_ControlPointID.
      else if (invocationId.hasValue()) {
        const uint32_t ptrType =
            theBuilder.getPointerType(elementTypeId, spv::StorageClass::Output);
        const uint32_t index = invocationId.getValue();
        ptr = theBuilder.createAccessChain(ptrType, varId, index);
        theBuilder.createStore(ptr, *value);
      }
      // For all normal cases
      else {
        theBuilder.createStore(ptr, *value);
      }
    }

    return true;
  }

  // If the decl itself doesn't have semantic string attached and there is no
  // one to inherit, it should be a struct having all its fields with semantic
  // strings.
  if (!semanticToUse->isValid() && !type->isStructureType()) {
    emitError("semantic string missing for shader %select{output|input}0 "
              "variable '%1'",
              decl->getLocation())
        << asInput << decl->getName();
    return false;
  }

  const auto *structDecl = type->getAs<RecordType>()->getDecl();

  if (asInput) {
    // If this decl translates into multiple stage input variables, we need to
    // load their values into a composite.
    llvm::SmallVector<uint32_t, 4> subValues;

    // If we have base classes, we need to handle them first.
    if (const auto *cxxDecl = type->getAsCXXRecordDecl())
      for (auto base : cxxDecl->bases()) {
        uint32_t subValue = 0;
        if (!createStageVars(sigPoint, base.getType()->getAsCXXRecordDecl(),
                             asInput, base.getType(), arraySize, namePrefix,
                             invocationId, &subValue, noWriteBack,
                             semanticToUse))
          return false;
        subValues.push_back(subValue);
      }

    for (const auto *field : structDecl->fields()) {
      uint32_t subValue = 0;
      if (!createStageVars(sigPoint, field, asInput, field->getType(),
                           arraySize, namePrefix, invocationId, &subValue,
                           noWriteBack, semanticToUse))
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

      // If we have base classes, we need to handle them first.
      if (const auto *cxxDecl = type->getAsCXXRecordDecl()) {
        uint32_t baseIndex = 0;
        for (auto base : cxxDecl->bases()) {
          const auto baseType = typeTranslator.translateType(base.getType());
          fields.push_back(theBuilder.createCompositeExtract(
              baseType, subValues[baseIndex++], {arrayIndex}));
        }
      }

      // Extract the element at index arrayIndex from each field
      for (const auto *field : structDecl->fields()) {
        const uint32_t fieldType =
            typeTranslator.translateType(field->getType());
        fields.push_back(theBuilder.createCompositeExtract(
            fieldType,
            subValues[getNumBaseClasses(type) + field->getFieldIndex()],
            {arrayIndex}));
      }
      // Compose a new struct out of them
      arrayElements.push_back(
          theBuilder.createCompositeConstruct(structType, fields));
    }

    *value = theBuilder.createCompositeConstruct(arrayType, arrayElements);
  } else {
    // If we have base classes, we need to handle them first.
    if (const auto *cxxDecl = type->getAsCXXRecordDecl()) {
      uint32_t baseIndex = 0;
      for (auto base : cxxDecl->bases()) {
        uint32_t subValue = 0;
        if (!noWriteBack)
          subValue = theBuilder.createCompositeExtract(
              typeTranslator.translateType(base.getType()), *value,
              {baseIndex++});

        if (!createStageVars(sigPoint, base.getType()->getAsCXXRecordDecl(),
                             asInput, base.getType(), arraySize, namePrefix,
                             invocationId, &subValue, noWriteBack,
                             semanticToUse))
          return false;
      }
    }

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
      uint32_t subValue = 0;
      if (!noWriteBack)
        subValue = theBuilder.createCompositeExtract(
            fieldType, *value,
            {getNumBaseClasses(type) + field->getFieldIndex()});

      if (!createStageVars(sigPoint, field, asInput, field->getType(),
                           arraySize, namePrefix, invocationId, &subValue,
                           noWriteBack, semanticToUse))
        return false;
    }
  }

  return true;
}

bool DeclResultIdMapper::writeBackOutputStream(const NamedDecl *decl,
                                               QualType type, uint32_t value) {
  assert(shaderModel.IsGS()); // Only for GS use

  if (hlsl::IsHLSLStreamOutputType(type))
    type = hlsl::GetHLSLResourceResultType(type);
  if (hasGSPrimitiveTypeQualifier(decl))
    type = astContext.getAsConstantArrayType(type)->getElementType();

  auto semanticInfo = getStageVarSemantic(decl);

  if (semanticInfo.isValid()) {
    // Found semantic attached directly to this Decl. Write the value for this
    // Decl to the corresponding stage output variable.

    const uint32_t srcTypeId = typeTranslator.translateType(type);

    // Handle SV_Position, SV_ClipDistance, and SV_CullDistance
    if (glPerVertex.tryToAccess(
            hlsl::DXIL::SigPointKind::GSOut, semanticInfo.semantic->GetKind(),
            semanticInfo.index, llvm::None, &value, /*noWriteBack=*/false))
      return true;

    // Query the <result-id> for the stage output variable generated out
    // of this decl.
    // We have semantic string attached to this decl; therefore, it must be a
    // DeclaratorDecl.
    const auto found = stageVarIds.find(cast<DeclaratorDecl>(decl));

    // We should have recorded its stage output variable previously.
    assert(found != stageVarIds.end());

    // Negate SV_Position.y if requested
    if (semanticInfo.semantic->GetKind() == hlsl::Semantic::Kind::Position)
      value = invertYIfRequested(value);

    theBuilder.createStore(found->second, value);
    return true;
  }

  // If the decl itself doesn't have semantic string attached, it should be
  // a struct having all its fields with semantic strings.
  if (!type->isStructureType()) {
    emitError("semantic string missing for shader output variable '%0'",
              decl->getLocation())
        << decl->getName();
    return false;
  }

  // If we have base classes, we need to handle them first.
  if (const auto *cxxDecl = type->getAsCXXRecordDecl()) {
    uint32_t baseIndex = 0;
    for (auto base : cxxDecl->bases()) {
      const auto baseType = typeTranslator.translateType(base.getType());
      const auto subValue =
          theBuilder.createCompositeExtract(baseType, value, {baseIndex++});

      if (!writeBackOutputStream(base.getType()->getAsCXXRecordDecl(),
                                 base.getType(), subValue))
        return false;
    }
  }

  const auto *structDecl = type->getAs<RecordType>()->getDecl();

  // Write out each field
  for (const auto *field : structDecl->fields()) {
    const uint32_t fieldType = typeTranslator.translateType(field->getType());
    const uint32_t subValue = theBuilder.createCompositeExtract(
        fieldType, value, {getNumBaseClasses(type) + field->getFieldIndex()});

    if (!writeBackOutputStream(field, field->getType(), subValue))
      return false;
  }

  return true;
}

uint32_t DeclResultIdMapper::invertYIfRequested(uint32_t position) {
  // Negate SV_Position.y if requested
  if (spirvOptions.invertY) {
    const auto f32Type = theBuilder.getFloat32Type();
    const auto v4f32Type = theBuilder.getVecType(f32Type, 4);
    const auto oldY = theBuilder.createCompositeExtract(f32Type, position, {1});
    const auto newY =
        theBuilder.createUnaryOp(spv::Op::OpFNegate, f32Type, oldY);
    position = theBuilder.createCompositeInsert(v4f32Type, position, {1}, newY);
  }
  return position;
}

void DeclResultIdMapper::decoratePSInterpolationMode(const NamedDecl *decl,
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

uint32_t DeclResultIdMapper::getBuiltinVar(spv::BuiltIn builtIn) {
  // Guarantee uniqueness
  switch (builtIn) {
  case spv::BuiltIn::SubgroupSize:
    if (laneCountBuiltinId)
      return laneCountBuiltinId;
    break;
  case spv::BuiltIn::SubgroupLocalInvocationId:
    if (laneIndexBuiltinId)
      return laneIndexBuiltinId;
    break;
  default:
    // Only allow the two cases we know about
    assert(false && "unsupported builtin case");
    return 0;
  }

  theBuilder.requireCapability(spv::Capability::GroupNonUniform);

  uint32_t type = theBuilder.getUint32Type();

  // Create a dummy StageVar for this builtin variable
  const uint32_t varId =
      theBuilder.addStageBuiltinVar(type, spv::StorageClass::Input, builtIn);

  const hlsl::SigPoint *sigPoint =
      hlsl::SigPoint::GetSigPoint(hlsl::SigPointFromInputQual(
          hlsl::DxilParamInputQual::In, shaderModel.GetKind(),
          /*isPatchConstant=*/false));

  StageVar stageVar(sigPoint, /*semaInfo=*/{}, /*builtinAttr=*/nullptr, type,
                    /*locCount=*/0);

  stageVar.setIsSpirvBuiltin();
  stageVar.setSpirvId(varId);
  stageVars.push_back(stageVar);

  switch (builtIn) {
  case spv::BuiltIn::SubgroupSize:
    laneCountBuiltinId = varId;
    break;
  case spv::BuiltIn::SubgroupLocalInvocationId:
    laneIndexBuiltinId = varId;
    break;
  }

  return varId;
}

uint32_t DeclResultIdMapper::createSpirvStageVar(StageVar *stageVar,
                                                 const NamedDecl *decl,
                                                 const llvm::StringRef name,
                                                 SourceLocation srcLoc) {
  using spv::BuiltIn;

  const auto sigPoint = stageVar->getSigPoint();
  const auto semanticKind = stageVar->getSemanticInfo().getKind();
  const auto sigPointKind = sigPoint->GetKind();
  const uint32_t type = stageVar->getSpirvTypeId();

  spv::StorageClass sc = getStorageClassForSigPoint(sigPoint);
  if (sc == spv::StorageClass::Max)
    return 0;
  stageVar->setStorageClass(sc);

  // [[vk::builtin(...)]] takes precedence.
  if (const auto *builtinAttr = stageVar->getBuiltInAttr()) {
    const auto spvBuiltIn =
        llvm::StringSwitch<BuiltIn>(builtinAttr->getBuiltIn())
            .Case("PointSize", BuiltIn::PointSize)
            .Case("HelperInvocation", BuiltIn::HelperInvocation)
            .Case("BaseVertex", BuiltIn::BaseVertex)
            .Case("BaseInstance", BuiltIn::BaseInstance)
            .Case("DrawIndex", BuiltIn::DrawIndex)
            .Case("DeviceIndex", BuiltIn::DeviceIndex)
            .Default(BuiltIn::Max);

    assert(spvBuiltIn != BuiltIn::Max); // The frontend should guarantee this.

    switch (spvBuiltIn) {
    case BuiltIn::BaseVertex:
    case BuiltIn::BaseInstance:
    case BuiltIn::DrawIndex:
      theBuilder.addExtension(Extension::KHR_shader_draw_parameters,
                              builtinAttr->getBuiltIn(),
                              builtinAttr->getLocation());
      theBuilder.requireCapability(spv::Capability::DrawParameters);
      break;
    case BuiltIn::DeviceIndex:
      theBuilder.addExtension(Extension::KHR_device_group,
                              stageVar->getSemanticStr(), srcLoc);
      theBuilder.requireCapability(spv::Capability::DeviceGroup);
      break;
    }

    return theBuilder.addStageBuiltinVar(type, sc, spvBuiltIn);
  }

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
  // According to Vulkan spec, the VertexIndex BuiltIn can only be used by
  // VSIn.
  case hlsl::Semantic::Kind::VertexID: {
    stageVar->setIsSpirvBuiltin();
    return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::VertexIndex);
  }
  // According to DXIL spec, the InstanceID SV can be used by VSIn, VSOut,
  // HSCPIn, HSCPOut, DSCPIn, DSOut, GSVIn, GSOut, PSIn.
  // According to Vulkan spec, the InstanceIndex BuitIn can only be used by
  // VSIn.
  case hlsl::Semantic::Kind::InstanceID: {
    switch (sigPointKind) {
    case hlsl::SigPoint::Kind::VSIn:
      stageVar->setIsSpirvBuiltin();
      return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::InstanceIndex);
    case hlsl::SigPoint::Kind::VSOut:
    case hlsl::SigPoint::Kind::HSCPIn:
    case hlsl::SigPoint::Kind::HSCPOut:
    case hlsl::SigPoint::Kind::DSCPIn:
    case hlsl::SigPoint::Kind::DSOut:
    case hlsl::SigPoint::Kind::GSVIn:
    case hlsl::SigPoint::Kind::GSOut:
    case hlsl::SigPoint::Kind::PSIn:
      return theBuilder.addStageIOVar(type, sc, name.str());
    default:
      llvm_unreachable("invalid usage of SV_InstanceID sneaked in");
    }
  }
  // According to DXIL spec, the Depth{|GreaterEqual|LessEqual} SV can only be
  // used by PSOut.
  // According to Vulkan spec, the FragDepth BuiltIn can only be used by PSOut.
  case hlsl::Semantic::Kind::Depth:
  case hlsl::Semantic::Kind::DepthGreaterEqual:
  case hlsl::Semantic::Kind::DepthLessEqual: {
    stageVar->setIsSpirvBuiltin();
    // Vulkan requires the DepthReplacing execution mode to write to FragDepth.
    theBuilder.addExecutionMode(entryFunctionId,
                                spv::ExecutionMode::DepthReplacing, {});
    if (semanticKind == hlsl::Semantic::Kind::DepthGreaterEqual)
      theBuilder.addExecutionMode(entryFunctionId,
                                  spv::ExecutionMode::DepthGreater, {});
    else if (semanticKind == hlsl::Semantic::Kind::DepthLessEqual)
      theBuilder.addExecutionMode(entryFunctionId,
                                  spv::ExecutionMode::DepthLess, {});
    return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::FragDepth);
  }
  // According to DXIL spec, the ClipDistance/CullDistance SV can be used by all
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
  // According to Vulkan spec, the FrontFacing BuitIn can only be used in PSIn.
  case hlsl::Semantic::Kind::IsFrontFace: {
    switch (sigPointKind) {
    case hlsl::SigPoint::Kind::GSOut:
      return theBuilder.addStageIOVar(type, sc, name.str());
    case hlsl::SigPoint::Kind::PSIn:
      stageVar->setIsSpirvBuiltin();
      return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::FrontFacing);
    default:
      llvm_unreachable("invalid usage of SV_IsFrontFace sneaked in");
    }
  }
  // According to DXIL spec, the Target SV can only be used by PSOut.
  // There is no corresponding builtin decoration in SPIR-V. So generate normal
  // Vulkan stage input/output variables.
  case hlsl::Semantic::Kind::Target:
  // An arbitrary semantic is defined by users. Generate normal Vulkan stage
  // input/output variables.
  case hlsl::Semantic::Kind::Arbitrary: {
    return theBuilder.addStageIOVar(type, sc, name.str());
    // TODO: patch constant function in hull shader
  }
  // According to DXIL spec, the DispatchThreadID SV can only be used by CSIn.
  // According to Vulkan spec, the GlobalInvocationId can only be used in CSIn.
  case hlsl::Semantic::Kind::DispatchThreadID: {
    stageVar->setIsSpirvBuiltin();
    return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::GlobalInvocationId);
  }
  // According to DXIL spec, the GroupID SV can only be used by CSIn.
  // According to Vulkan spec, the WorkgroupId can only be used in CSIn.
  case hlsl::Semantic::Kind::GroupID: {
    stageVar->setIsSpirvBuiltin();
    return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::WorkgroupId);
  }
  // According to DXIL spec, the GroupThreadID SV can only be used by CSIn.
  // According to Vulkan spec, the LocalInvocationId can only be used in CSIn.
  case hlsl::Semantic::Kind::GroupThreadID: {
    stageVar->setIsSpirvBuiltin();
    return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::LocalInvocationId);
  }
  // According to DXIL spec, the GroupIndex SV can only be used by CSIn.
  // According to Vulkan spec, the LocalInvocationIndex can only be used in
  // CSIn.
  case hlsl::Semantic::Kind::GroupIndex: {
    stageVar->setIsSpirvBuiltin();
    return theBuilder.addStageBuiltinVar(type, sc,
                                         BuiltIn::LocalInvocationIndex);
  }
  // According to DXIL spec, the OutputControlID SV can only be used by HSIn.
  // According to Vulkan spec, the InvocationId BuiltIn can only be used in
  // HS/GS In.
  case hlsl::Semantic::Kind::OutputControlPointID: {
    stageVar->setIsSpirvBuiltin();
    return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::InvocationId);
  }
  // According to DXIL spec, the PrimitiveID SV can only be used by PCIn, HSIn,
  // DSIn, GSIn, GSOut, and PSIn.
  // According to Vulkan spec, the PrimitiveId BuiltIn can only be used in
  // HS/DS/PS In, GS In/Out.
  case hlsl::Semantic::Kind::PrimitiveID: {
    // PrimitiveId requires either Tessellation or Geometry capability.
    // Need to require one for PSIn.
    if (sigPointKind == hlsl::SigPoint::Kind::PSIn)
      theBuilder.requireCapability(spv::Capability::Geometry);

    // Translate to PrimitiveId BuiltIn for all valid SigPoints.
    stageVar->setIsSpirvBuiltin();
    return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::PrimitiveId);
  }
  // According to DXIL spec, the TessFactor SV can only be used by PCOut and
  // DSIn.
  // According to Vulkan spec, the TessLevelOuter BuiltIn can only be used in
  // PCOut and DSIn.
  case hlsl::Semantic::Kind::TessFactor: {
    stageVar->setIsSpirvBuiltin();
    return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::TessLevelOuter);
  }
  // According to DXIL spec, the InsideTessFactor SV can only be used by PCOut
  // and DSIn.
  // According to Vulkan spec, the TessLevelInner BuiltIn can only be used in
  // PCOut and DSIn.
  case hlsl::Semantic::Kind::InsideTessFactor: {
    stageVar->setIsSpirvBuiltin();
    return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::TessLevelInner);
  }
  // According to DXIL spec, the DomainLocation SV can only be used by DSIn.
  // According to Vulkan spec, the TessCoord BuiltIn can only be used in DSIn.
  case hlsl::Semantic::Kind::DomainLocation: {
    stageVar->setIsSpirvBuiltin();
    return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::TessCoord);
  }
  // According to DXIL spec, the GSInstanceID SV can only be used by GSIn.
  // According to Vulkan spec, the InvocationId BuiltIn can only be used in
  // HS/GS In.
  case hlsl::Semantic::Kind::GSInstanceID: {
    stageVar->setIsSpirvBuiltin();
    return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::InvocationId);
  }
  // According to DXIL spec, the SampleIndex SV can only be used by PSIn.
  // According to Vulkan spec, the SampleId BuiltIn can only be used in PSIn.
  case hlsl::Semantic::Kind::SampleIndex: {
    theBuilder.requireCapability(spv::Capability::SampleRateShading);

    stageVar->setIsSpirvBuiltin();
    return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::SampleId);
  }
  // According to DXIL spec, the StencilRef SV can only be used by PSOut.
  case hlsl::Semantic::Kind::StencilRef: {
    theBuilder.addExtension(Extension::EXT_shader_stencil_export,
                            stageVar->getSemanticStr(), srcLoc);
    theBuilder.requireCapability(spv::Capability::StencilExportEXT);

    stageVar->setIsSpirvBuiltin();
    return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::FragStencilRefEXT);
  }
  // According to DXIL spec, the ViewID SV can only be used by PSIn.
  case hlsl::Semantic::Kind::Barycentrics: {
    theBuilder.addExtension(Extension::AMD_shader_explicit_vertex_parameter,
                            stageVar->getSemanticStr(), srcLoc);
    stageVar->setIsSpirvBuiltin();

    // Selecting the correct builtin according to interpolation mode
    auto bi = BuiltIn::Max;
    if (decl->hasAttr<HLSLNoPerspectiveAttr>()) {
      if (decl->hasAttr<HLSLCentroidAttr>()) {
        bi = BuiltIn::BaryCoordNoPerspCentroidAMD;
      } else if (decl->hasAttr<HLSLSampleAttr>()) {
        bi = BuiltIn::BaryCoordNoPerspSampleAMD;
      } else {
        bi = BuiltIn::BaryCoordNoPerspAMD;
      }
    } else {
      if (decl->hasAttr<HLSLCentroidAttr>()) {
        bi = BuiltIn::BaryCoordSmoothCentroidAMD;
      } else if (decl->hasAttr<HLSLSampleAttr>()) {
        bi = BuiltIn::BaryCoordSmoothSampleAMD;
      } else {
        bi = BuiltIn::BaryCoordSmoothAMD;
      }
    }

    return theBuilder.addStageBuiltinVar(type, sc, bi);
  }
  // According to DXIL spec, the RenderTargetArrayIndex SV can only be used by
  // VSIn, VSOut, HSCPIn, HSCPOut, DSIn, DSOut, GSVIn, GSOut, PSIn.
  // According to Vulkan spec, the Layer BuiltIn can only be used in GSOut and
  // PSIn.
  case hlsl::Semantic::Kind::RenderTargetArrayIndex: {
    switch (sigPointKind) {
    case hlsl::SigPoint::Kind::VSIn:
    case hlsl::SigPoint::Kind::HSCPIn:
    case hlsl::SigPoint::Kind::HSCPOut:
    case hlsl::SigPoint::Kind::PCOut:
    case hlsl::SigPoint::Kind::DSIn:
    case hlsl::SigPoint::Kind::DSCPIn:
    case hlsl::SigPoint::Kind::GSVIn:
      return theBuilder.addStageIOVar(type, sc, name.str());
    case hlsl::SigPoint::Kind::VSOut:
    case hlsl::SigPoint::Kind::DSOut:
      theBuilder.addExtension(Extension::EXT_shader_viewport_index_layer,
                              "SV_RenderTargetArrayIndex", srcLoc);
      theBuilder.requireCapability(
          spv::Capability::ShaderViewportIndexLayerEXT);

      stageVar->setIsSpirvBuiltin();
      return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::Layer);
    case hlsl::SigPoint::Kind::GSOut:
    case hlsl::SigPoint::Kind::PSIn:
      theBuilder.requireCapability(spv::Capability::Geometry);

      stageVar->setIsSpirvBuiltin();
      return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::Layer);
    default:
      llvm_unreachable("invalid usage of SV_RenderTargetArrayIndex sneaked in");
    }
  }
  // According to DXIL spec, the ViewportArrayIndex SV can only be used by
  // VSIn, VSOut, HSCPIn, HSCPOut, DSIn, DSOut, GSVIn, GSOut, PSIn.
  // According to Vulkan spec, the ViewportIndex BuiltIn can only be used in
  // GSOut and PSIn.
  case hlsl::Semantic::Kind::ViewPortArrayIndex: {
    switch (sigPointKind) {
    case hlsl::SigPoint::Kind::VSIn:
    case hlsl::SigPoint::Kind::HSCPIn:
    case hlsl::SigPoint::Kind::HSCPOut:
    case hlsl::SigPoint::Kind::PCOut:
    case hlsl::SigPoint::Kind::DSIn:
    case hlsl::SigPoint::Kind::DSCPIn:
    case hlsl::SigPoint::Kind::GSVIn:
      return theBuilder.addStageIOVar(type, sc, name.str());
    case hlsl::SigPoint::Kind::VSOut:
    case hlsl::SigPoint::Kind::DSOut:
      theBuilder.addExtension(Extension::EXT_shader_viewport_index_layer,
                              "SV_ViewPortArrayIndex", srcLoc);
      theBuilder.requireCapability(
          spv::Capability::ShaderViewportIndexLayerEXT);

      stageVar->setIsSpirvBuiltin();
      return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::ViewportIndex);
    case hlsl::SigPoint::Kind::GSOut:
    case hlsl::SigPoint::Kind::PSIn:
      theBuilder.requireCapability(spv::Capability::MultiViewport);

      stageVar->setIsSpirvBuiltin();
      return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::ViewportIndex);
    default:
      llvm_unreachable("invalid usage of SV_ViewportArrayIndex sneaked in");
    }
  }
  // According to DXIL spec, the Coverage SV can only be used by PSIn and PSOut.
  // According to Vulkan spec, the SampleMask BuiltIn can only be used in
  // PSIn and PSOut.
  case hlsl::Semantic::Kind::Coverage: {
    stageVar->setIsSpirvBuiltin();
    return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::SampleMask);
  }
  // According to DXIL spec, the ViewID SV can only be used by VSIn, PCIn,
  // HSIn, DSIn, GSIn, PSIn.
  // According to Vulkan spec, the ViewIndex BuiltIn can only be used in
  // VS/HS/DS/GS/PS input.
  case hlsl::Semantic::Kind::ViewID: {
    theBuilder.addExtension(Extension::KHR_multiview,
                            stageVar->getSemanticStr(), srcLoc);
    theBuilder.requireCapability(spv::Capability::MultiView);

    stageVar->setIsSpirvBuiltin();
    return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::ViewIndex);
  }
    // According to DXIL spec, the InnerCoverage SV can only be used as PSIn.
    // According to Vulkan spec, the FullyCoveredEXT BuiltIn can only be used as
    // PSIn.
  case hlsl::Semantic::Kind::InnerCoverage: {
    theBuilder.addExtension(Extension::EXT_fragment_fully_covered,
                            stageVar->getSemanticStr(), srcLoc);
    theBuilder.requireCapability(spv::Capability::FragmentFullyCoveredEXT);

    stageVar->setIsSpirvBuiltin();
    return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::FullyCoveredEXT);
  }
  default:
    emitError("semantic %0 unimplemented", srcLoc)
        << stageVar->getSemanticStr();
    break;
  }

  return 0;
}

bool DeclResultIdMapper::validateVKAttributes(const NamedDecl *decl) {
  bool success = true;
  if (const auto *idxAttr = decl->getAttr<VKIndexAttr>()) {
    if (!shaderModel.IsPS()) {
      emitError("vk::index only allowed in pixel shader",
                idxAttr->getLocation());
      success = false;
    }

    const auto *locAttr = decl->getAttr<VKLocationAttr>();

    if (!locAttr) {
      emitError("vk::index should be used together with vk::location for "
                "dual-source blending",
                idxAttr->getLocation());
      success = false;
    } else {
      const auto locNumber = locAttr->getNumber();
      if (locNumber != 0) {
        emitError("dual-source blending should use vk::location 0",
                  locAttr->getLocation());
        success = false;
      }
    }

    const auto idxNumber = idxAttr->getNumber();
    if (idxNumber != 0 && idxNumber != 1) {
      emitError("dual-source blending only accepts 0 or 1 as vk::index",
                idxAttr->getLocation());
      success = false;
    }
  }

  return success;
}

bool DeclResultIdMapper::validateVKBuiltins(const NamedDecl *decl,
                                            const hlsl::SigPoint *sigPoint) {
  bool success = true;

  if (const auto *builtinAttr = decl->getAttr<VKBuiltInAttr>()) {
    // The front end parsing only allows vk::builtin to be attached to a
    // function/parameter/variable; all of them are DeclaratorDecls.
    const auto declType = getTypeOrFnRetType(cast<DeclaratorDecl>(decl));
    const auto loc = builtinAttr->getLocation();

    if (decl->hasAttr<VKLocationAttr>()) {
      emitError("cannot use vk::builtin and vk::location together", loc);
      success = false;
    }

    const llvm::StringRef builtin = builtinAttr->getBuiltIn();

    if (builtin == "HelperInvocation") {
      if (!declType->isBooleanType()) {
        emitError("HelperInvocation builtin must be of boolean type", loc);
        success = false;
      }

      if (sigPoint->GetKind() != hlsl::SigPoint::Kind::PSIn) {
        emitError(
            "HelperInvocation builtin can only be used as pixel shader input",
            loc);
        success = false;
      }
    } else if (builtin == "PointSize") {
      if (!declType->isFloatingType()) {
        emitError("PointSize builtin must be of float type", loc);
        success = false;
      }

      switch (sigPoint->GetKind()) {
      case hlsl::SigPoint::Kind::VSOut:
      case hlsl::SigPoint::Kind::HSCPIn:
      case hlsl::SigPoint::Kind::HSCPOut:
      case hlsl::SigPoint::Kind::DSCPIn:
      case hlsl::SigPoint::Kind::DSOut:
      case hlsl::SigPoint::Kind::GSVIn:
      case hlsl::SigPoint::Kind::GSOut:
      case hlsl::SigPoint::Kind::PSIn:
        break;
      default:
        emitError("PointSize builtin cannot be used as %0", loc)
            << sigPoint->GetName();
        success = false;
      }
    } else if (builtin == "BaseVertex" || builtin == "BaseInstance" ||
               builtin == "DrawIndex") {
      if (!declType->isSpecificBuiltinType(BuiltinType::Kind::Int) &&
          !declType->isSpecificBuiltinType(BuiltinType::Kind::UInt)) {
        emitError("%0 builtin must be of 32-bit scalar integer type", loc)
            << builtin;
        success = false;
      }

      if (sigPoint->GetKind() != hlsl::SigPoint::Kind::VSIn) {
        emitError("%0 builtin can only be used in vertex shader input", loc)
            << builtin;
        success = false;
      }
    } else if (builtin == "DeviceIndex") {
      if (getStorageClassForSigPoint(sigPoint) != spv::StorageClass::Input) {
        emitError("%0 builtin can only be used as shader input", loc)
            << builtin;
        success = false;
      }
      if (!declType->isSpecificBuiltinType(BuiltinType::Kind::Int) &&
          !declType->isSpecificBuiltinType(BuiltinType::Kind::UInt)) {
        emitError("%0 builtin must be of 32-bit scalar integer type", loc)
            << builtin;
        success = false;
      }
    }
  }

  return success;
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
      llvm_unreachable("Found invalid SigPoint kind for semantic");
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
      llvm_unreachable("Found invalid SigPoint kind for semantic");
    }
    break;
  }
  default:
    llvm_unreachable("Found invalid SigPoint kind for semantic");
  }
  return sc;
}

uint32_t DeclResultIdMapper::getTypeAndCreateCounterForPotentialAliasVar(
    const DeclaratorDecl *decl, bool *shouldBeAlias, SpirvEvalInfo *info) {
  if (const auto *varDecl = dyn_cast<VarDecl>(decl)) {
    // This method is only intended to be used to create SPIR-V variables in the
    // Function or Private storage class.
    assert(!varDecl->isExternallyVisible() || varDecl->isStaticDataMember());
  }

  const QualType type = getTypeOrFnRetType(decl);
  // Whether we should generate this decl as an alias variable.
  bool genAlias = false;

  if (const auto *buffer = dyn_cast<HLSLBufferDecl>(decl->getDeclContext())) {
    // For ConstantBuffer and TextureBuffer
    if (buffer->isConstantBufferView())
      genAlias = true;
  } else if (TypeTranslator::isOrContainsAKindOfStructuredOrByteBuffer(type)) {
    genAlias = true;
  }

  if (shouldBeAlias)
    *shouldBeAlias = genAlias;

  if (genAlias) {
    needsLegalization = true;

    createCounterVarForDecl(decl);

    if (info)
      info->setContainsAliasComponent(true);
  }

  return typeTranslator.translateType(type);
}

} // end namespace spirv
} // end namespace clang
