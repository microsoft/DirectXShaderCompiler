//===--- DeclResultIdMapper.h - AST Decl to SPIR-V <result-id> mapper ------==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_SPIRV_DECLRESULTIDMAPPER_H
#define LLVM_CLANG_LIB_SPIRV_DECLRESULTIDMAPPER_H

#include <string>
#include <vector>

#include "dxc/HLSL//DxilShaderModel.h"
#include "spirv/1.0/spirv.hpp11"
#include "clang/SPIRV/ModuleBuilder.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/SmallVector.h"

#include "TypeTranslator.h"

namespace clang {
namespace spirv {

/// \brief The class containing mappings from Clang frontend Decls to their
/// corresponding SPIR-V <result-id>s.
///
/// All symbols defined in the AST should be "defined" or registered in this
/// class and have their <result-id>s queried from this class. In the process
/// of defining a Decl, the SPIR-V module builder passed into the constructor
/// will be used to generate all SPIR-V instructions required.
///
/// This class acts as a middle layer to handle the mapping between HLSL
/// semantics and Vulkan stage (builtin/input/output) variables. Such mapping
/// is required because of the semantic differences between DirectX and
/// Vulkan and the essence of HLSL as the front-end language for DirectX.
/// A normal variable attached with some semantic will be translated into a
/// single stage variables if it is of non-struct type. If it is of struct
/// type, the fields with attached semantics will need to be translated into
/// stage variables per Vulkan's requirements.
///
/// In the following class, we call a Decl as *remapped* when it is translated
/// into a stage variable; otherwise, we call it as *normal*. Remapped decls
/// include:
/// * FunctionDecl if the return value is attached with a semantic
/// * ParmVarDecl if the parameter is attached with a semantic
/// * FieldDecl if the field is attached with a semantic.
class DeclResultIdMapper {
public:
  inline DeclResultIdMapper(const hlsl::ShaderModel &stage,
                            ModuleBuilder &builder, DiagnosticsEngine &diag);

  /// \brief Creates the stage variables by parsing the semantics attached to
  /// the given function's return value.
  void createStageVarFromFnReturn(const FunctionDecl *funcDecl);

  /// \brief Creates the stage variables by parsing the semantics attached to
  /// the given function's parameter.
  void createStageVarFromFnParam(const ParmVarDecl *paramDecl);

  /// \brief Registers a decl's <result-id> without generating any SPIR-V
  /// instruction. The given decl will be treated as normal decl.
  void registerDeclResultId(const NamedDecl *symbol, uint32_t resultId);

  /// \brief Returns true if the given <result-id> is for a stage variable.
  bool isStageVariable(uint32_t varId) const;

  /// \brief Returns the <result-id> for the given decl.
  ///
  /// This method will panic if the given decl is not registered.
  uint32_t getDeclResultId(const NamedDecl *decl) const;

  /// \brief Returns the <result-id> for the given decl if already registered;
  /// otherwise, treats the given decl as a normal decl and returns a newly
  /// assigned <result-id> for it.
  uint32_t getOrRegisterDeclResultId(const NamedDecl *decl);

  /// \brief Returns the <result-id> for the given remapped decl. Returns zero
  /// if it is not a registered remapped decl.
  uint32_t getRemappedDeclResultId(const NamedDecl *decl) const;

  /// \brief Returns the <result-id> for the given normal decl. Returns zero if
  /// it is not a registered normal decl.
  uint32_t getNormalDeclResultId(const NamedDecl *decl) const;

  /// \brief Returns all defined stage (builtin/input/ouput) variables in this
  /// mapper.
  std::vector<uint32_t> collectStageVariables() const;

  /// \brief Decorates all stage input and output variables with proper
  /// location.
  ///
  /// This method will writes the location assignment into the module under
  /// construction.
  void finalizeStageIOLocations();

private:
  /// \brief Stage variable kind.
  ///
  /// Stage variables include builtin, input, and output variables.
  /// They participate in interface matching in Vulkan pipelines.
  enum class StageVarKind {
    None,
    Arbitary,
    Position,
    Color,
    Target,
    // TODO: other possible kinds
  };

  using StageVarIdSemanticPair = std::pair<uint32_t, std::string>;

  /// Returns the type of the given decl. If the given decl is a FunctionDecl,
  /// returns its result type.
  QualType getFnParamOrRetType(const DeclaratorDecl *decl) const;

  /// Creates all the stage variables mapped from semantics on the given decl.
  ///
  /// Assumes the decl has semantic attached to itself or to its fields.
  void createStageVariables(const DeclaratorDecl *decl, bool actAsInput);

  /// \brief Returns the stage variable's kind for the given semantic.
  StageVarKind getStageVarKind(llvm::StringRef semantic) const;

  /// \brief Returns the stage variable's semantic for the given Decl.
  std::string getStageVarSemantic(const NamedDecl *decl) const;

private:
  const hlsl::ShaderModel &shaderModel;
  ModuleBuilder &theBuilder;
  TypeTranslator typeTranslator;

  /// Mapping of all remapped decls to their <result-id>s.
  llvm::DenseMap<const NamedDecl *, uint32_t> remappedDecls;
  /// Mapping of all normal decls to their <result-id>s.
  llvm::DenseMap<const NamedDecl *, uint32_t> normalDecls;
  /// <result-id>s of all defined stage variables.
  ///
  /// We need to keep a separate list here to avoid looping through the
  /// remappedDecls to find whether an <result-id> is for a stage variable.
  llvm::SmallSet<uint32_t, 16> stageVars;

  /// Stage input/oupt/builtin variables and their kinds.
  ///
  /// We need to keep a separate list here in order to sort them at the end
  /// of the module building.
  llvm::SmallVector<StageVarIdSemanticPair, 8> stageInputs;
  llvm::SmallVector<StageVarIdSemanticPair, 8> stageOutputs;
  llvm::SmallVector<StageVarIdSemanticPair, 8> stageBuiltins;
};

DeclResultIdMapper::DeclResultIdMapper(const hlsl::ShaderModel &model,
                                       ModuleBuilder &builder,
                                       DiagnosticsEngine &diag)
    : shaderModel(model), theBuilder(builder), typeTranslator(builder, diag) {}

} // end namespace spirv
} // end namespace clang

#endif