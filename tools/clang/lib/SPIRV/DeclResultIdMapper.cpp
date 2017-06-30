//===--- DeclResultIdMapper.cpp - DeclResultIdMapper impl --------*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "DeclResultIdMapper.h"

#include "clang/AST/HlslTypes.h"
#include "llvm/ADT/StringSwitch.h"

namespace clang {
namespace spirv {

void DeclResultIdMapper::createStageVarFromFnReturn(
    const FunctionDecl *funcDecl) {
  // SemanticDecl for the return value is attached to the FunctionDecl.
  createStageVariables(funcDecl, false);
}

void DeclResultIdMapper::createStageVarFromFnParam(
    const ParmVarDecl *paramDecl) {
  // TODO: We cannot treat all parameters as stage inputs because of
  // out/input modifiers.
  createStageVariables(paramDecl, true);
}

void DeclResultIdMapper::registerDeclResultId(const NamedDecl *symbol,
                                              uint32_t resultId) {
  normalDecls[symbol] = resultId;
}

bool DeclResultIdMapper::isStageVariable(uint32_t varId) const {
  return stageVars.count(varId) != 0;
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
  std::vector<uint32_t> stageVars;

  for (const auto &builtin : stageBuiltins) {
    stageVars.push_back(builtin.first);
  }
  for (const auto &input : stageInputs) {
    stageVars.push_back(input.first);
  }
  for (const auto &output : stageOutputs) {
    stageVars.push_back(output.first);
  }

  return stageVars;
}

void DeclResultIdMapper::finalizeStageIOLocations() {
  uint32_t nextInputLocation = 0;
  uint32_t nextOutputLocation = 0;

  // TODO: sort the variables according to some criteria first, e.g.,
  // alphabetical order of semantic names.
  for (const auto &input : stageInputs) {
    theBuilder.decorateLocation(input.first, nextInputLocation++);
  }
  for (const auto &output : stageOutputs) {
    theBuilder.decorateLocation(output.first, nextOutputLocation++);
  }
}

QualType
DeclResultIdMapper::getFnParamOrRetType(const DeclaratorDecl *decl) const {
  if (const auto *funcDecl = dyn_cast<FunctionDecl>(decl)) {
    return funcDecl->getReturnType();
  }
  return decl->getType();
}

void DeclResultIdMapper::createStageVariables(const DeclaratorDecl *decl,
                                              bool actAsInput) {
  QualType type = getFnParamOrRetType(decl);

  if (type->isVoidType()) {
    // No stage variables will be created for void type.
    return;
  }

  const std::string semantic = getStageVarSemantic(decl);
  if (!semantic.empty()) {
    // Found semantic attached directly to this Decl. This means we need to
    // map this decl to a single stage variable.
    const uint32_t typeId = typeTranslator.translateType(type);
    const auto kind = getStageVarKind(semantic);

    if (actAsInput) {
      // Stage (builtin) input variable cases
      const uint32_t varId =
          theBuilder.addStageIOVariable(typeId, spv::StorageClass::Input);

      stageInputs.push_back(std::make_pair(varId, semantic));
      remappedDecls[decl] = varId;
      stageVars.insert(varId);
    } else {
      // Handle output builtin variables first
      if (shaderModel.IsVS() && kind == StageVarKind::Position) {
        const uint32_t varId =
            theBuilder.addStageBuiltinVariable(typeId, spv::BuiltIn::Position);

        stageBuiltins.push_back(std::make_pair(varId, semantic));
        remappedDecls[decl] = varId;
        stageVars.insert(varId);
      } else {
        // The rest are normal stage output variables
        const uint32_t varId =
            theBuilder.addStageIOVariable(typeId, spv::StorageClass::Output);

        stageOutputs.push_back(std::make_pair(varId, semantic));
        remappedDecls[decl] = varId;
        stageVars.insert(varId);
      }
    }
  } else {
    // If the decl itself doesn't have semantic, it should be a struct having
    // all its fields with semantics.
    assert(type->isStructureType() &&
           "found non-struct decls without semantics");

    const auto *structDecl = cast<RecordType>(type.getTypePtr())->getDecl();

    // Recursively handle all the fields.
    for (const auto *field : structDecl->fields()) {
      createStageVariables(field, actAsInput);
    }
  }
}

DeclResultIdMapper::StageVarKind
DeclResultIdMapper::getStageVarKind(llvm::StringRef semantic) const {
  return llvm::StringSwitch<StageVarKind>(semantic)
      .Case("", StageVarKind::None)
      .StartsWith("COLOR", StageVarKind::Color)
      .StartsWith("POSITION", StageVarKind::Position)
      .StartsWith("SV_POSITION", StageVarKind::Position)
      .StartsWith("SV_TARGET", StageVarKind::Target)
      .Default(StageVarKind::Arbitary);
}

std::string
DeclResultIdMapper::getStageVarSemantic(const NamedDecl *decl) const {
  for (auto *annotation : decl->getUnusualAnnotations()) {
    if (auto *semantic = dyn_cast<hlsl::SemanticDecl>(annotation)) {
      return semantic->SemanticName.upper();
    }
  }
  return "";
}

} // end namespace spirv
} // end namespace clang