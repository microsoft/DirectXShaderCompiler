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

#include <vector>

#include "dxc/HLSL/DxilSemantic.h"
#include "dxc/HLSL/DxilShaderModel.h"
#include "dxc/HLSL/DxilSigPoint.h"
#include "spirv/1.0/spirv.hpp11"
#include "clang/AST/Attr.h"
#include "clang/SPIRV/EmitSPIRVOptions.h"
#include "clang/SPIRV/ModuleBuilder.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Twine.h"

#include "TypeTranslator.h"

namespace clang {
namespace spirv {

/// \brief The class containing HLSL and SPIR-V information about a Vulkan stage
/// (builtin/input/output) variable.
class StageVar {
public:
  inline StageVar(const hlsl::SigPoint *sig, llvm::StringRef semaStr,
                  const hlsl::Semantic *sema, uint32_t semaIndex,
                  uint32_t type);

  const hlsl::SigPoint *getSigPoint() const { return sigPoint; }
  const hlsl::Semantic *getSemantic() const { return semantic; }

  uint32_t getSpirvTypeId() const { return typeId; }

  uint32_t getSpirvId() const { return valueId; }
  void setSpirvId(uint32_t id) { valueId = id; }

  llvm::StringRef getSemanticStr() const { return semanticStr; }
  uint32_t getSemanticIndex() const { return semanticIndex; }

  bool isSpirvBuitin() const { return isBuiltin; }
  void setIsSpirvBuiltin() { isBuiltin = true; }

  spv::StorageClass getStorageClass() const { return storageClass; }
  void setStorageClass(spv::StorageClass sc) { storageClass = sc; }

  const VKLocationAttr *getLocationAttr() const { return location; }
  void setLocationAttr(const VKLocationAttr *loc) { location = loc; }

private:
  /// HLSL SigPoint. It uniquely identifies each set of parameters that may be
  /// input or output for each entry point.
  const hlsl::SigPoint *sigPoint;
  /// Original HLSL semantic string in the source code.
  llvm::StringRef semanticStr;
  /// HLSL semantic.
  const hlsl::Semantic *semantic;
  /// HLSL semantic index.
  uint32_t semanticIndex;
  /// SPIR-V <type-id>.
  uint32_t typeId;
  /// SPIR-V <result-id>.
  uint32_t valueId;
  /// Indicates whether this stage variable should be a SPIR-V builtin.
  bool isBuiltin;
  /// SPIR-V storage class this stage variable belongs to.
  spv::StorageClass storageClass;
  /// Location assignment if input/output variable.
  const VKLocationAttr *location;
};

StageVar::StageVar(const hlsl::SigPoint *sig, llvm::StringRef semaStr,
                   const hlsl::Semantic *sema, uint32_t semaIndex,
                   uint32_t type)
    : sigPoint(sig), semanticStr(semaStr), semantic(sema),
      semanticIndex(semaIndex), typeId(type), valueId(0), isBuiltin(false),
      storageClass(spv::StorageClass::Max), location(nullptr) {}

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
class DeclResultIdMapper {
public:
  inline DeclResultIdMapper(const hlsl::ShaderModel &stage, ASTContext &context,
                            ModuleBuilder &builder, DiagnosticsEngine &diag,
                            const EmitSPIRVOptions &spirvOptions);

  /// \brief Creates the stage output variables by parsing the semantics
  /// attached to the given function's parameter or return value and returns
  /// true on success. SPIR-V instructions will also be generated to update the
  /// contents of the output variables by extracting sub-values from the given
  /// storedValue.
  bool createStageOutputVar(const DeclaratorDecl *decl, uint32_t storedValue);

  /// \brief Creates the stage input variables by parsing the semantics attached
  /// to the given function's parameter and returns true on success. SPIR-V
  /// instructions will also be generated to load the contents from the input
  /// variables and composite them into one and write to *loadedValue.
  bool createStageInputVar(const ParmVarDecl *paramDecl, uint32_t *loadedValue);

  /// \brief Creates a function-scope paramter in the current function and
  /// returns its <result-id>.
  uint32_t createFnParam(uint32_t paramType, const ParmVarDecl *param);

  /// \brief Creates a function-scope variable in the current function and
  /// returns its <result-id>.
  uint32_t createFnVar(uint32_t varType, const VarDecl *variable,
                       llvm::Optional<uint32_t> init);

  /// \brief Creates a file-scope variable and returns its <result-id>.
  uint32_t createFileVar(uint32_t varType, const VarDecl *variable,
                         llvm::Optional<uint32_t> init);

  /// \brief Creates an external-visible variable and returns its <result-id>.
  uint32_t createExternVar(uint32_t varType, const VarDecl *var);

  /// \brief Sets the <result-id> of the entry function.
  void setEntryFunctionId(uint32_t id) { entryFunctionId = id; }

public:
  /// The struct containing SPIR-V information of a AST Decl.
  struct DeclSpirvInfo {
    uint32_t resultId;
    spv::StorageClass storageClass;
  };

  /// \brief Returns the SPIR-V information for the given decl.
  /// Returns nullptr if no such decl was previously registered.
  const DeclSpirvInfo *getDeclSpirvInfo(const NamedDecl *decl) const;

  /// \brief Returns the <result-id> for the given decl.
  ///
  /// This method will panic if the given decl is not registered.
  uint32_t getDeclResultId(const NamedDecl *decl) const;

  /// \brief Returns the <result-id> for the given function if already
  /// registered; otherwise, treats the given function as a normal decl and
  /// returns a newly assigned <result-id> for it.
  uint32_t getOrRegisterFnResultId(const FunctionDecl *fn);

  /// Returns the storage class for the given expression. The expression is
  /// expected to be an lvalue. Otherwise this method may panic.
  spv::StorageClass resolveStorageClass(const Expr *expr) const;
  spv::StorageClass resolveStorageClass(const Decl *decl) const;

  /// \brief Returns all defined stage (builtin/input/ouput) variables in this
  /// mapper.
  std::vector<uint32_t> collectStageVars() const;

  /// \brief Decorates all stage input and output variables with proper
  /// location and returns true on success.
  ///
  /// This method will write the location assignment into the module under
  /// construction.
  inline bool decorateStageIOLocations();

private:
  /// \brief Wrapper method to create an error message and report it
  /// in the diagnostic engine associated with this consumer.
  template <unsigned N>
  DiagnosticBuilder emitError(const char (&message)[N],
                              SourceLocation loc = {}) {
    const auto diagId =
        diags.getCustomDiagID(clang::DiagnosticsEngine::Error, message);
    return diags.Report(loc, diagId);
  }

  /// \brief Checks whether some semantic is used more than once and returns
  /// true if no such cases. Returns false otherwise.
  bool checkSemanticDuplication(bool forInput);

  /// \brief Decorates all stage input (if forInput is true) or output (if
  /// forInput is false) variables with proper location and returns true on
  /// success.
  ///
  /// This method will write the location assignment into the module under
  /// construction.
  bool finalizeStageIOLocations(bool forInput);

  /// Returns the type of the given decl. If the given decl is a FunctionDecl,
  /// returns its result type.
  QualType getFnParamOrRetType(const DeclaratorDecl *decl) const;

  /// Creates all the stage variables mapped from semantics on the given decl
  /// and returns true on success.
  ///
  /// Assumes the decl has semantic attached to itself or to its fields.
  bool createStageVars(const DeclaratorDecl *decl, uint32_t *value,
                       bool asInput, const llvm::Twine &namePrefix);

  /// Creates the SPIR-V variable instruction for the given StageVar and returns
  /// the <result-id>. Also sets whether the StageVar is a SPIR-V builtin and
  /// its storage class accordingly. name will be used as the debug name when
  /// creating a stage input/output variable.
  uint32_t createSpirvStageVar(StageVar *, const llvm::Twine &name);

  /// \brief Returns the stage variable's semantic for the given Decl.
  static llvm::StringRef getStageVarSemantic(const NamedDecl *decl);

private:
  const hlsl::ShaderModel &shaderModel;
  ModuleBuilder &theBuilder;
  const EmitSPIRVOptions &spirvOptions;
  DiagnosticsEngine &diags;

  TypeTranslator typeTranslator;

  uint32_t entryFunctionId;

  /// Mapping of all Clang AST decls to their <result-id>s.
  llvm::DenseMap<const NamedDecl *, DeclSpirvInfo> astDecls;
  /// Vector of all defined stage variables.
  llvm::SmallVector<StageVar, 8> stageVars;
};

DeclResultIdMapper::DeclResultIdMapper(const hlsl::ShaderModel &model,
                                       ASTContext &context,
                                       ModuleBuilder &builder,
                                       DiagnosticsEngine &diag,
                                       const EmitSPIRVOptions &options)
    : shaderModel(model), theBuilder(builder), spirvOptions(options),
      diags(diag), typeTranslator(context, builder, diag), entryFunctionId(0) {}

bool DeclResultIdMapper::decorateStageIOLocations() {
  // Try both input and output even if input location assignment failed
  return finalizeStageIOLocations(true) & finalizeStageIOLocations(false);
}

} // end namespace spirv
} // end namespace clang

#endif