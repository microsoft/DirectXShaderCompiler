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

#include "dxc/HLSL/DxilConstants.h"
#include "dxc/HLSL/DxilTypeSystem.h"
#include "clang/AST/Expr.h"
#include "clang/AST/HlslTypes.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "llvm/ADT/SmallBitVector.h"
#include "llvm/ADT/StringSet.h"

namespace clang {
namespace spirv {

bool DeclResultIdMapper::createStageOutputVar(const DeclaratorDecl *decl,
                                              uint32_t storedValue) {
  return createStageVars(decl, &storedValue, false, "out.var");
}

bool DeclResultIdMapper::createStageInputVar(const ParmVarDecl *paramDecl,
                                             uint32_t *loadedValue) {
  return createStageVars(paramDecl, loadedValue, true, "in.var");
}

void DeclResultIdMapper::registerDeclResultId(const NamedDecl *symbol,
                                              uint32_t resultId) {
  auto sc = spv::StorageClass::Function;
  if (const auto *varDecl = dyn_cast<VarDecl>(symbol)) {
    if (varDecl->isExternallyVisible()) {
      // TODO: Global variables are by default constant. But the default
      // behavior can be changed via command line option. So Uniform may
      // not be the correct storage class.
      sc = spv::StorageClass::Uniform;
    } else if (!varDecl->hasLocalStorage()) {
      // File scope variables
      sc = spv::StorageClass::Private;
    }
  }
  astDecls[symbol] = {resultId, sc};
}

const DeclResultIdMapper::DeclSpirvInfo *
DeclResultIdMapper::getDeclSpirvInfo(const NamedDecl *decl) const {
  auto it = astDecls.find(decl);
  if (it != astDecls.end())
    return &it->second;

  return nullptr;
}

uint32_t DeclResultIdMapper::getDeclResultId(const NamedDecl *decl) const {
  if (const auto *info = getDeclSpirvInfo(decl))
    return info->resultId;

  assert(false && "found unregistered decl");
  return 0;
}

uint32_t DeclResultIdMapper::getOrRegisterDeclResultId(const NamedDecl *decl) {
  if (const auto *info = getDeclSpirvInfo(decl))
    return info->resultId;

  const uint32_t id = theBuilder.getSPIRVContext()->takeNextId();
  registerDeclResultId(decl, id);

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
  // Typically we won't have that many stage input or output variables.
  // Using 64 should be fine here.
  // TODO: Emit errors if we need more than 64.
  LocationSet() : usedLocs(64, false), nextLoc(0) {}

  /// Uses the given location.
  void useLoc(uint32_t loc) { usedLocs.set(loc); }

  /// Uses the next available location.
  uint32_t useNextLoc() {
    while (usedLocs[nextLoc])
      nextLoc++;
    usedLocs.set(nextLoc);
    return nextLoc++;
  }

private:
  llvm::SmallBitVector usedLocs; ///< All previously used locations
  uint32_t nextLoc;              ///< Next available location
};
} // namespace

void DeclResultIdMapper::finalizeStageIOLocations() {
  { // Check semantic duplication
    llvm::StringSet<> seenInputSemantics;
    llvm::StringSet<> seenOutputSemantics;
    bool success = true;

    for (const auto &var : stageVars) {
      auto s = var.getSemanticStr();
      if (var.getSigPoint()->IsInput()) {
        if (seenInputSemantics.count(s)) {
          emitError("input semantic '%0' used more than once") << s;
          success = false;
        }
        seenInputSemantics.insert(s);
      } else {
        if (seenOutputSemantics.count(s)) {
          emitError("output semantic '%0' used more than once") << s;
          success = false;
        }
        seenOutputSemantics.insert(s);
      }
    }

    if (!success)
      return;
  }

  std::vector<const StageVar *> inputVars;
  std::vector<const StageVar *> outputVars;

  LocationSet inputLocs;
  LocationSet outputLocs;

  for (const auto &var : stageVars)
    if (!var.isSpirvBuitin()) {
      // Only SV_Target, SV_Depth, SV_DepthLessEqual, SV_DepthGreaterEqual,
      // SV_StencilRef, SV_Coverage are allowed in the pixel shader.
      // Arbitrary semantics are disallowed in pixel shader.
      if (var.getSemantic()->GetKind() == hlsl::Semantic::Kind::Target) {
        theBuilder.decorateLocation(var.getSpirvId(), var.getSemanticIndex());
        outputLocs.useLoc(var.getSemanticIndex());
      } else if (var.getSigPoint()->IsInput()) {
        inputVars.push_back(&var);
      } else if (var.getSigPoint()->IsOutput()) {
        outputVars.push_back(&var);
      }
    }

  // Sort stage input/output variables alphabetically
  const auto comp = [](const StageVar *a, const StageVar *b) {
    return a->getSemanticStr() < b->getSemanticStr();
  };

  std::sort(inputVars.begin(), inputVars.end(), comp);
  std::sort(outputVars.begin(), outputVars.end(), comp);

  for (const auto *var : inputVars)
    theBuilder.decorateLocation(var->getSpirvId(), inputLocs.useNextLoc());
  for (const auto *var : outputVars)
    theBuilder.decorateLocation(var->getSpirvId(), outputLocs.useNextLoc());
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
    stageVar.setSpirvId(varId);

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
  // According to Vulkan spec, the Position decoration can only be used
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
  // According to Vulkan spec, the InstanceIndex can only be used by VSIn.
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
  // According to DXIL spec, the Depth SV can only be used by PSOut.
  case hlsl::Semantic::Kind::Depth:
    stageVar->setIsSpirvBuiltin();
    return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::FragDepth);
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
  default:
    emitError("semantic %0 unimplemented yet")
        << stageVar->getSemantic()->GetName();
    break;
  }

  return 0;
}

llvm::StringRef DeclResultIdMapper::getStageVarSemantic(const NamedDecl *decl) {
  for (auto *annotation : decl->getUnusualAnnotations()) {
    if (auto *semantic = dyn_cast<hlsl::SemanticDecl>(annotation)) {
      return semantic->SemanticName;
    }
  }
  return {};
}

} // end namespace spirv
} // end namespace clang