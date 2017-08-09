//===--- DeclResultIdMapper.cpp - DeclResultIdMapper impl --------*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "DeclResultIdMapper.h"

#include "dxc/HLSL/DxilConstants.h"
#include "dxc/HLSL/DxilTypeSystem.h"
#include "clang/AST/HlslTypes.h"

namespace clang {
namespace spirv {

bool DeclResultIdMapper::createStageVarFromFnReturn(
    const FunctionDecl *funcDecl) {
  // SemanticDecl for the return value is attached to the FunctionDecl.
  return createStageVariables(funcDecl, true);
}

bool DeclResultIdMapper::createStageVarFromFnParam(
    const ParmVarDecl *paramDecl) {
  return createStageVariables(paramDecl, false);
}

void DeclResultIdMapper::registerDeclResultId(const NamedDecl *symbol,
                                              uint32_t resultId) {
  normalDecls[symbol] = resultId;
}

bool DeclResultIdMapper::isStageVariable(uint32_t varId) const {
  for (const auto &var : stageVars)
    if (var.getSpirvId() == varId)
      return true;
  return false;
}

uint32_t DeclResultIdMapper::getDeclResultId(const NamedDecl *decl) const {
  if (const uint32_t id = getNormalDeclResultId(decl))
    return id;
  if (const uint32_t id = getRemappedDeclResultId(decl))
    return id;

  assert(false && "found unregistered decl");
  return 0;
}

uint32_t DeclResultIdMapper::getOrRegisterDeclResultId(const NamedDecl *decl) {
  if (const uint32_t id = getNormalDeclResultId(decl))
    return id;
  if (const uint32_t id = getRemappedDeclResultId(decl))
    return id;

  const uint32_t id = theBuilder.getSPIRVContext()->takeNextId();
  registerDeclResultId(decl, id);

  return id;
}

uint32_t
DeclResultIdMapper::getRemappedDeclResultId(const NamedDecl *decl) const {
  auto it = remappedDecls.find(decl);
  if (it != remappedDecls.end())
    return it->second;
  return 0;
}

uint32_t
DeclResultIdMapper::getNormalDeclResultId(const NamedDecl *decl) const {
  auto it = normalDecls.find(decl);
  if (it != normalDecls.end())
    return it->second;
  return 0;
}

std::vector<uint32_t> DeclResultIdMapper::collectStageVariables() const {
  std::vector<uint32_t> vars;

  for (const auto &var : stageVars)
    vars.push_back(var.getSpirvId());

  return vars;
}

void DeclResultIdMapper::finalizeStageIOLocations() {
  uint32_t nextInputLocation = 0;
  uint32_t nextOutputLocation = 0;

  // TODO: sort the variables according to some criteria first, e.g.,
  // alphabetical order of semantic names.
  for (auto &var : stageVars) {
    if (!var.isSpirvBuitin()) {
      if (var.getSigPoint()->IsInput()) {
        theBuilder.decorateLocation(var.getSpirvId(), nextInputLocation++);
      } else if (var.getSigPoint()->IsOutput()) {
        theBuilder.decorateLocation(var.getSpirvId(), nextOutputLocation++);
      }
    }
  }
}

QualType
DeclResultIdMapper::getFnParamOrRetType(const DeclaratorDecl *decl) const {
  if (const auto *funcDecl = dyn_cast<FunctionDecl>(decl)) {
    return funcDecl->getReturnType();
  }
  return decl->getType();
}

bool DeclResultIdMapper::createStageVariables(const DeclaratorDecl *decl,
                                              bool forRet) {
  QualType type = getFnParamOrRetType(decl);

  if (type->isVoidType()) {
    // No stage variables will be created for void type.
    return true;
  }

  const llvm::StringRef semanticStr = getStageVarSemantic(decl);
  if (!semanticStr.empty()) {
    // Found semantic attached directly to this Decl. This means we need to
    // map this decl to a single stage variable.
    const uint32_t typeId = typeTranslator.translateType(type);

    // TODO: fix this when supporting parameter in/out qualifiers
    const hlsl::DxilParamInputQual qual =
        forRet ? hlsl::DxilParamInputQual::Out : hlsl::DxilParamInputQual::In;

    // TODO: use the correct isPC value when supporting patch constant function
    // in hull shader
    const hlsl::SigPoint *sigPoint =
        hlsl::SigPoint::GetSigPoint(hlsl::SigPointFromInputQual(
            qual, shaderModel.GetKind(), /*isPC*/ false));

    const auto *semantic = hlsl::Semantic::GetByName(semanticStr);

    // Error out when the given semantic is invalid in this shader model
    if (hlsl::SigPoint::GetInterpretation(
            semantic->GetKind(), sigPoint->GetKind(), shaderModel.GetMajor(),
            shaderModel.GetMinor()) ==
        hlsl::DXIL::SemanticInterpretationKind::NA) {
      emitError("invalid semantic %0 for shader module %1")
          << semanticStr << shaderModel.GetName();
      return false;
    }

    StageVar stageVar(sigPoint, semantic, typeId);
    const uint32_t varId = createSpirvStageVar(&stageVar);
    stageVar.setSpirvId(varId);

    stageVars.push_back(stageVar);
    remappedDecls[decl] = varId;
  } else {
    // If the decl itself doesn't have semantic, it should be a struct having
    // all its fields with semantics.
    assert(type->isStructureType() &&
           "found non-struct decls without semantics");

    const auto *structDecl = cast<RecordType>(type.getTypePtr())->getDecl();

    // Recursively handle all the fields.
    for (const auto *field : structDecl->fields()) {
      if (!createStageVariables(field, forRet))
        return false;
    }
  }

  return true;
}

uint32_t DeclResultIdMapper::createSpirvStageVar(StageVar *stageVar) {
  const auto semanticKind = stageVar->getSemantic()->GetKind();
  const auto sigPointKind = stageVar->getSigPoint()->GetKind();
  const uint32_t type = stageVar->getSpirvTypeId();

  // The following translation assumes that semantic validity in the current
  // shader model is already checked, so it only covers valid SigPoints for
  // each semantic.

  switch (semanticKind) {
  // According to DXIL spec, the Position SV can be used by all SigPoints
  // other than PCIn, HSIn, GSIn, PSOut, CSIn.
  // According to Vulkan spec, the Position decoration can only be used
  // by PSOut, HS/DS/GS In/Out.
  case hlsl::Semantic::Kind::Position: {
    switch (sigPointKind) {
    case hlsl::SigPoint::Kind::VSIn:
      return theBuilder.addStageIOVariable(type, spv::StorageClass::Input);
    case hlsl::SigPoint::Kind::VSOut:
      stageVar->setIsSpirvBuiltin();
      return theBuilder.addStageBuiltinVariable(type, spv::BuiltIn::Position);
    case hlsl::SigPoint::Kind::PSIn:
      stageVar->setIsSpirvBuiltin();
      return theBuilder.addStageBuiltinVariable(type, spv::BuiltIn::FragCoord);
    default:
      emitError("semantic Position for SigPoint %0 unimplemented yet")
          << stageVar->getSigPoint()->GetName();
      break;
    }
  }
  // According to DXIL spec, the VertexID SV can only be used by VSIn.
  case hlsl::Semantic::Kind::VertexID:
    stageVar->setIsSpirvBuiltin();
    return theBuilder.addStageBuiltinVariable(type, spv::BuiltIn::VertexIndex);
  // According to DXIL spec, the InstanceID SV can  be used by VSIn, VSOut,
  // HSCPIn, HSCPOut, DSCPIn, DSOut, GSVIn, GSOut, PSIn.
  // According to Vulkan spec, the InstanceIndex can only be used by VSIn.
  case hlsl::Semantic::Kind::InstanceID: {
    switch (sigPointKind) {
    case hlsl::SigPoint::Kind::VSIn:
      stageVar->setIsSpirvBuiltin();
      return theBuilder.addStageBuiltinVariable(type,
                                                spv::BuiltIn::InstanceIndex);
    case hlsl::SigPoint::Kind::VSOut:
      return theBuilder.addStageIOVariable(type, spv::StorageClass::Output);
    case hlsl::SigPoint::Kind::PSIn:
      return theBuilder.addStageIOVariable(type, spv::StorageClass::Input);
    default:
      emitError("semantic InstanceID for SigPoint %0 unimplemented yet")
          << stageVar->getSigPoint()->GetName();
      break;
    }
  }
  // According to DXIL spec, the Depth SV can only be used by PSOut.
  case hlsl::Semantic::Kind::Depth:
    stageVar->setIsSpirvBuiltin();
    return theBuilder.addStageBuiltinVariable(type, spv::BuiltIn::FragDepth);
  // According to DXIL spec, the Target SV can only be used by PSOut.
  // There is no corresponding builtin decoration in SPIR-V. So generate normal
  // Vulkan stage input/output variables.
  case hlsl::Semantic::Kind::Target:
  // An arbitrary semantic is defined by users. Generate normal Vulkan stage
  // input/output variables.
  case hlsl::Semantic::Kind::Arbitrary: {
    if (stageVar->getSigPoint()->IsInput())
      return theBuilder.addStageIOVariable(type, spv::StorageClass::Input);
    if (stageVar->getSigPoint()->IsOutput())
      return theBuilder.addStageIOVariable(type, spv::StorageClass::Output);
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