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
/// \brief Returns the stage variable's semantic for the given Decl.
llvm::StringRef getStageVarSemantic(const NamedDecl *decl) {
  for (auto *annotation : decl->getUnusualAnnotations()) {
    if (auto *semantic = dyn_cast<hlsl::SemanticDecl>(annotation)) {
      return semantic->SemanticName;
    }
  }
  return {};
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
} // anonymous namespace

bool DeclResultIdMapper::createStageOutputVar(const DeclaratorDecl *decl,
                                              uint32_t storedValue) {
  return createStageVars(decl, &storedValue, false, "out.var");
}

bool DeclResultIdMapper::createStageInputVar(const ParmVarDecl *paramDecl,
                                             uint32_t *loadedValue) {
  return createStageVars(paramDecl, loadedValue, true, "in.var");
}

const DeclResultIdMapper::DeclSpirvInfo *
DeclResultIdMapper::getDeclSpirvInfo(const NamedDecl *decl) const {
  auto it = astDecls.find(decl);
  if (it != astDecls.end())
    return &it->second;

  return nullptr;
}

uint32_t DeclResultIdMapper::getDeclResultId(const NamedDecl *decl) {
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
          /*decorateLayout*/ true);
      return theBuilder.createAccessChain(
          theBuilder.getPointerType(varType, info->storageClass),
          info->resultId, {theBuilder.getConstantInt32(info->indexInCTBuffer)});
    } else {
      return info->resultId;
    }

  assert(false && "found unregistered decl");
  return 0;
}

uint32_t DeclResultIdMapper::createFnParam(uint32_t paramType,
                                           const ParmVarDecl *param) {
  const uint32_t id = theBuilder.addFnParam(paramType, param->getName());
  astDecls[param] = {id, spv::StorageClass::Function, -1};

  return id;
}

uint32_t DeclResultIdMapper::createFnVar(uint32_t varType, const VarDecl *var,
                                         llvm::Optional<uint32_t> init) {
  const uint32_t id = theBuilder.addFnVar(varType, var->getName(), init);
  astDecls[var] = {id, spv::StorageClass::Function, -1};

  return id;
}

uint32_t DeclResultIdMapper::createFileVar(uint32_t varType, const VarDecl *var,
                                           llvm::Optional<uint32_t> init) {
  const uint32_t id = theBuilder.addModuleVar(
      varType, spv::StorageClass::Private, var->getName(), init);
  astDecls[var] = {id, spv::StorageClass::Private, -1};

  return id;
}

uint32_t DeclResultIdMapper::createExternVar(uint32_t varType,
                                             const VarDecl *var) {
  auto storageClass = spv::StorageClass::UniformConstant;

  // TODO: Figure out other cases where the storage class should be Uniform.
  if (auto *t = var->getType()->getAs<RecordType>()) {
    const llvm::StringRef typeName = t->getDecl()->getName();
    if (typeName == "ByteAddressBuffer" || typeName == "RWByteAddressBuffer")
      storageClass = spv::StorageClass::Uniform;
  }

  const uint32_t id = theBuilder.addModuleVar(varType, storageClass,
                                              var->getName(), llvm::None);
  astDecls[var] = {id, storageClass, -1};
  resourceVars.emplace_back(id, getResourceBinding(var),
                            var->getAttr<VKBindingAttr>());

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
    // Implicit generated struct declarations should be ignored.
    if (isa<CXXRecordDecl>(subDecl) && subDecl->isImplicit())
      continue;

    // The field can only be FieldDecl (for normal structs) or VarDecl (for
    // HLSLBufferDecls).
    assert(isa<VarDecl>(subDecl) || isa<FieldDecl>(subDecl));
    const auto *declDecl = cast<DeclaratorDecl>(subDecl);
    // All fields are qualified with const. It will affect the debug name.
    // We don't need it here.
    auto varType = declDecl->getType();
    varType.removeLocalConst();

    fieldTypes.push_back(typeTranslator.translateType(
        varType, true, declDecl->hasAttr<HLSLRowMajorAttr>()));
    fieldNames.push_back(declDecl->getName());
  }

  // Get the type for the whole buffer
  auto decorations = typeTranslator.getLayoutDecorations(decl);
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
    astDecls[varDecl] = {bufferVar, spv::StorageClass::Uniform, index++};
  }
  resourceVars.emplace_back(bufferVar, getResourceBinding(decl),
                            decl->getAttr<VKBindingAttr>());

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
  astDecls[decl] = {bufferVar, spv::StorageClass::Uniform, -1};
  resourceVars.emplace_back(bufferVar, getResourceBinding(context),
                            decl->getAttr<VKBindingAttr>());

  return bufferVar;
}

uint32_t DeclResultIdMapper::getOrRegisterFnResultId(const FunctionDecl *fn) {
  if (const auto *info = getDeclSpirvInfo(fn))
    return info->resultId;

  const uint32_t id = theBuilder.getSPIRVContext()->takeNextId();
  astDecls[fn] = {id, spv::StorageClass::Function, -1};

  return id;
}

namespace {
/// A class for resolving the storage class of a given Decl or Expr.
class StorageClassResolver : public RecursiveASTVisitor<StorageClassResolver> {
public:
  explicit StorageClassResolver(const DeclResultIdMapper &mapper)
      : declIdMapper(mapper), storageClass(spv::StorageClass::Max) {}

  // For querying the storage class of a remapped decl

  // Semantics may be attached to FunctionDecl, ParmVarDecl, and FieldDecl.
  // We create stage variables for them and we may need to query the storage
  // classes of these stage variables.
  bool VisitFunctionDecl(FunctionDecl *decl) { return processDecl(decl); }
  bool VisitFieldDecl(FieldDecl *decl) { return processDecl(decl); }
  bool VisitParmVarDecl(ParmVarDecl *decl) { return processDecl(decl); }

  // For querying the storage class of a normal decl

  // Normal decls should be referred in expressions.
  bool VisitDeclRefExpr(DeclRefExpr *expr) {
    return processDecl(expr->getDecl());
  }

  bool processDecl(NamedDecl *decl) {
    const auto *info = declIdMapper.getDeclSpirvInfo(decl);
    assert(info);
    if (storageClass == spv::StorageClass::Max) {
      storageClass = info->storageClass;
      return true;
    }

    // Two decls with different storage classes are referenced in this
    // expression. We should not visit such expression using this class.
    assert(storageClass == info->storageClass);
    return false;
  }

  spv::StorageClass get() const { return storageClass; }

private:
  const DeclResultIdMapper &declIdMapper;
  spv::StorageClass storageClass;
};
} // namespace

spv::StorageClass
DeclResultIdMapper::resolveStorageClass(const Expr *expr) const {
  auto resolver = StorageClassResolver(*this);
  resolver.TraverseStmt(const_cast<Expr *>(expr));
  return resolver.get();
}

spv::StorageClass
DeclResultIdMapper::resolveStorageClass(const Decl *decl) const {
  auto resolver = StorageClassResolver(*this);
  resolver.TraverseDecl(const_cast<Decl *>(decl));
  return resolver.get();
}

std::vector<uint32_t> DeclResultIdMapper::collectStageVars() const {
  std::vector<uint32_t> vars;

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
  BindingSet() : nextBinding(0) {}

  /// Uses the given set and binding number.
  void useBinding(uint32_t binding, uint32_t set) {
    bindings[set].insert(binding);
  }

  /// Uses the next avaiable binding number in set 0.
  uint32_t useNextBinding() {
    auto &set0bindings = bindings[0];
    while (set0bindings.count(nextBinding))
      nextBinding++;
    set0bindings.insert(nextBinding);
    return nextBinding++;
  }

  /// Returns true if the given set and binding number is already used.
  bool isBindingUsed(uint32_t binding, uint32_t set) {
    return bindings[set].count(binding);
  }

private:
  std::unordered_map<uint32_t, llvm::SmallSet<uint32_t, 8>> bindings;
  uint32_t nextBinding; ///< Next available binding number in set 0
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
  const auto locAssigned = [forInput](const StageVar &v) {
    if (forInput ? v.getSigPoint()->IsInput() : v.getSigPoint()->IsOutput())
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
      if ((forInput ? !var.getSigPoint()->IsInput()
                    : !var.getSigPoint()->IsOutput()))
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
    if ((forInput ? !var.getSigPoint()->IsInput()
                  : !var.getSigPoint()->IsOutput()))
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
      if (var.getSemantic()->GetKind() == hlsl::Semantic::Kind::Target) {
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

bool DeclResultIdMapper::decorateResourceBindings() {
  // Returns true if the given ResourceVar has explicit descriptor set and
  // binding specified.
  const auto bindingAssigned = [](const ResourceVar &var) {
    return var.getBinding() != nullptr;
  };

  BindingSet bindingSet;
  bool noError = true;

  if (std::all_of(resourceVars.begin(), resourceVars.end(), bindingAssigned)) {
    for (const auto &var : resourceVars) {
      const auto attrLoc = var.getBinding()->getLocation();
      const auto set = var.getBinding()->getSet();
      const auto binding = var.getBinding()->getBinding();

      if (bindingSet.isBindingUsed(binding, set)) {
        emitError("resource binding #%0 in descriptor set #%1 already assigned",
                  attrLoc)
            << binding << set;
        noError = false;
      }

      theBuilder.decorateDSetBinding(var.getSpirvId(),
                                     var.getBinding()->getSet(),
                                     var.getBinding()->getBinding());

      bindingSet.useBinding(binding, set);
    }
    return noError;
  }

  if (std::any_of(resourceVars.begin(), resourceVars.end(), bindingAssigned)) {
    // We have checked that not all of the stage variables have explicit
    // set and binding assignment.
    emitError("partial explicit resource binding assignment via "
              "[[vk::binding(X[, Y])]] unsupported");
    return false;
  }

  for (const auto &var : resourceVars) {
    // TODO: we can have duplicated set and binding number because of there are
    // multiple resource types in the following. E.g., :register(s0) and
    // :register(t0) will both map to set #0 and binding #0.
    uint32_t set = 0, binding = 0;
    if (const auto *reg = var.getRegister()) {
      set = reg->RegisterSpace;
      binding = reg->RegisterNumber;
      bindingSet.useBinding(binding, set);
    } else {
      binding = bindingSet.useNextBinding();
    }

    theBuilder.decorateDSetBinding(var.getSpirvId(), set, binding);
  }

  return true;
}

QualType
DeclResultIdMapper::getFnParamOrRetType(const DeclaratorDecl *decl) const {
  if (const auto *funcDecl = dyn_cast<FunctionDecl>(decl)) {
    return funcDecl->getReturnType();
  }
  return decl->getType();
}

bool DeclResultIdMapper::createStageVars(const DeclaratorDecl *decl,
                                         uint32_t *value, bool asInput,
                                         const llvm::Twine &namePrefix) {
  QualType type = getFnParamOrRetType(decl);
  if (type->isVoidType()) {
    // No stage variables will be created for void type.
    return true;
  }
  const uint32_t typeId = typeTranslator.translateType(type);

  const llvm::StringRef semanticStr = getStageVarSemantic(decl);
  if (!semanticStr.empty()) {
    // Found semantic attached directly to this Decl. This means we need to
    // map this decl to a single stage variable.

    const hlsl::DxilParamInputQual qual =
        asInput ? hlsl::DxilParamInputQual::In : hlsl::DxilParamInputQual::Out;

    // TODO: use the correct isPC value when supporting patch constant function
    // in hull shader
    const hlsl::SigPoint *sigPoint =
        hlsl::SigPoint::GetSigPoint(hlsl::SigPointFromInputQual(
            qual, shaderModel.GetKind(), /*isPC*/ false));

    llvm::StringRef semanticName;
    uint32_t semanticIndex = 0;
    hlsl::Semantic::DecomposeNameAndIndex(semanticStr, &semanticName,
                                          &semanticIndex);

    const auto *semantic = hlsl::Semantic::GetByName(semanticName);

    // Error out when the given semantic is invalid in this shader model
    if (hlsl::SigPoint::GetInterpretation(
            semantic->GetKind(), sigPoint->GetKind(), shaderModel.GetMajor(),
            shaderModel.GetMinor()) ==
        hlsl::DXIL::SemanticInterpretationKind::NA) {
      emitError("invalid semantic %0 for shader module %1")
          << semanticStr << shaderModel.GetName();
      return false;
    }

    StageVar stageVar(sigPoint, semanticStr, semantic, semanticIndex, typeId);
    llvm::Twine name = namePrefix + "." + semanticStr;
    const uint32_t varId = createSpirvStageVar(&stageVar, name);
    if (varId == 0)
      return false;

    // Decorate with interpolation modes for pixel shader input variables
    if (shaderModel.IsPS() && sigPoint->IsInput()) {
      const QualType elemType = typeTranslator.getElementType(type);

      if (elemType->isBooleanType() || elemType->isIntegerType()) {
        // TODO: Probably we can call hlsl::ValidateSignatureElement() for the
        // following check.
        if (decl->getAttr<HLSLLinearAttr>() ||
            decl->getAttr<HLSLCentroidAttr>() ||
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

    stageVar.setSpirvId(varId);
    stageVar.setLocationAttr(decl->getAttr<VKLocationAttr>());

    stageVars.push_back(stageVar);

    if (asInput) {
      *value = theBuilder.createLoad(typeId, varId);
    } else {
      theBuilder.createStore(varId, *value);
    }
  } else {
    // If the decl itself doesn't have semantic, it should be a struct having
    // all its fields with semantics.
    assert(type->isStructureType() &&
           "found non-struct decls without semantics");

    const auto *structDecl = cast<RecordType>(type.getTypePtr())->getDecl();

    if (asInput) {
      // If this decl translates into multiple stage input variables, we need to
      // load their values into a composite.
      llvm::SmallVector<uint32_t, 4> subValues;
      for (const auto *field : structDecl->fields()) {
        uint32_t subValue = 0;
        if (!createStageVars(field, &subValue, true, namePrefix))
          return false;
        subValues.push_back(subValue);
      }
      *value = theBuilder.createCompositeConstruct(typeId, subValues);
    } else {
      // If this decl translates into multiple stage output variables, we need
      // to store the value components into them.
      for (const auto *field : structDecl->fields()) {
        const uint32_t fieldType =
            typeTranslator.translateType(field->getType());
        uint32_t subValue = theBuilder.createCompositeExtract(
            fieldType, *value, {field->getFieldIndex()});
        if (!createStageVars(field, &subValue, false, namePrefix))
          return false;
      }
    }
  }

  return true;
}

uint32_t DeclResultIdMapper::createSpirvStageVar(StageVar *stageVar,
                                                 const llvm::Twine &name) {
  using spv::BuiltIn;

  const auto semanticKind = stageVar->getSemantic()->GetKind();
  const auto sigPointKind = stageVar->getSigPoint()->GetKind();
  const uint32_t type = stageVar->getSpirvTypeId();

  // The following translation assumes that semantic validity in the current
  // shader model is already checked, so it only covers valid SigPoints for
  // each semantic.

  // TODO: case for patch constant
  const auto sc = stageVar->getSigPoint()->IsInput()
                      ? spv::StorageClass::Input
                      : spv::StorageClass::Output;
  stageVar->setStorageClass(sc);

  switch (semanticKind) {
  // According to DXIL spec, the Position SV can be used by all SigPoints
  // other than PCIn, HSIn, GSIn, PSOut, CSIn.
  // According to Vulkan spec, the Position BuiltIn can only be used
  // by PSOut, HS/DS/GS In/Out.
  case hlsl::Semantic::Kind::Position: {
    switch (sigPointKind) {
    case hlsl::SigPoint::Kind::VSIn:
      return theBuilder.addStageIOVar(type, sc, name.str());
    case hlsl::SigPoint::Kind::VSOut:
      stageVar->setIsSpirvBuiltin();
      return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::Position);
    case hlsl::SigPoint::Kind::PSIn:
      stageVar->setIsSpirvBuiltin();
      return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::FragCoord);
    default:
      emitError("semantic Position for SigPoint %0 unimplemented yet")
          << stageVar->getSigPoint()->GetName();
      break;
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
          << stageVar->getSigPoint()->GetName();
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
  // According to DXIL spec, the IsFrontFace SV can only be used by GSOut and
  // PSIn.
  // According to Vulkan spec, the FrontFacing BuitIn can only be used in PSIn.
  case hlsl::Semantic::Kind::IsFrontFace: {
    switch (sigPointKind) {
    case hlsl::SigPoint::Kind::PSIn:
      stageVar->setIsSpirvBuiltin();
      return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::FrontFacing);
    default:
      emitError("semantic IsFrontFace for SigPoint %0 unimplemented yet")
          << stageVar->getSigPoint()->GetName();
      break;
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
  case hlsl::Semantic::Kind::DispatchThreadID: {
    // DispatchThreadID semantic is only valid for compute shaders, and it is
    // always an input.
    stageVar->setIsSpirvBuiltin();
    return theBuilder.addStageBuiltinVar(type, spv::StorageClass::Input,
                                         BuiltIn::GlobalInvocationId);
  }
  default:
    emitError("semantic %0 unimplemented yet")
        << stageVar->getSemantic()->GetName();
    break;
  }

  return 0;
}

} // end namespace spirv
} // end namespace clang