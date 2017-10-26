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

#include <tuple>
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

#include "SpirvEvalInfo.h"
#include "TypeTranslator.h"

namespace clang {
namespace spirv {

/// \brief The class containing HLSL and SPIR-V information about a Vulkan stage
/// (builtin/input/output) variable.
class StageVar {
public:
  inline StageVar(const hlsl::SigPoint *sig, llvm::StringRef semaStr,
                  const hlsl::Semantic *sema, uint32_t semaIndex, uint32_t type)
      : sigPoint(sig), semanticStr(semaStr), semantic(sema),
        semanticIndex(semaIndex), typeId(type), valueId(0), isBuiltin(false),
        storageClass(spv::StorageClass::Max), location(nullptr) {}

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

class ResourceVar {
public:
  /// The category of this resource.
  ///
  /// We only care about Vulkan image and sampler types here, since they can be
  /// bundled together as a combined image sampler which takes the same binding
  /// number. The compiler should allow this case.
  ///
  /// Numbers are assigned to make bit check easiser.
  enum class Category : uint32_t {
    Image = 1,
    Sampler = 2,
    Other = 3,
  };

  ResourceVar(uint32_t id, Category cat, const hlsl::RegisterAssignment *r,
              const VKBindingAttr *b, const VKCounterBindingAttr *cb,
              bool counter = false)
      : varId(id), category(cat), reg(r), binding(b), counterBinding(cb),
        isCounterVar(counter) {}

  uint32_t getSpirvId() const { return varId; }
  Category getCategory() const { return category; }
  const hlsl::RegisterAssignment *getRegister() const { return reg; }
  const VKBindingAttr *getBinding() const { return binding; }
  bool isCounter() const { return isCounterVar; }
  const auto *getCounterBinding() const { return counterBinding; }

private:
  uint32_t varId;                             ///< <result-id>
  Category category;                          ///< Resource category
  const hlsl::RegisterAssignment *reg;        ///< HLSL register assignment
  const VKBindingAttr *binding;               ///< Vulkan binding assignment
  const VKCounterBindingAttr *counterBinding; ///< Vulkan counter binding
  bool isCounterVar;                          ///< Couter variable or not
};

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
  bool createStageOutputVar(const DeclaratorDecl *decl, uint32_t storedValue,
                            bool isPatchConstant);

  /// \brief Creates the stage input variables by parsing the semantics attached
  /// to the given function's parameter and returns true on success. SPIR-V
  /// instructions will also be generated to load the contents from the input
  /// variables and composite them into one and write to *loadedValue.
  bool createStageInputVar(const ParmVarDecl *paramDecl, uint32_t *loadedValue,
                           bool isPatchConstant);

  /// \brief Creates an input/output stage variable which does not have any
  /// semantics (such as InputPatch/OutputPatch in Hull shaders). This method
  /// does not create a Load/Store from/to the created stage variable and leaves
  /// it to the caller to do so as they see fit, because it is possible that the
  /// stage variable may have to be accessed differently (using OpAccessChain
  /// for example).
  uint32_t createStageVarWithoutSemantics(bool isInput, uint32_t typeId,
                                          const llvm::StringRef name,
                                          const clang::VKLocationAttr *loc);

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
  uint32_t createExternVar(const VarDecl *var);

  /// \brief Creates a cbuffer/tbuffer from the given decl.
  ///
  /// In the AST, cbuffer/tbuffer is represented as a HLSLBufferDecl, which is
  /// a DeclContext, and all fields in the buffer are represented as VarDecls.
  /// We cannot do the normal translation path, which will translate a field
  /// into a standalone variable. We need to create a single SPIR-V variable
  /// for the whole buffer. When we refer to the field VarDecl later, we need
  /// to do an extra OpAccessChain to get its pointer from the SPIR-V variable
  /// standing for the whole buffer.
  uint32_t createCTBuffer(const HLSLBufferDecl *decl);

  /// \brief Creates a cbuffer/tbuffer from the given decl.
  ///
  /// In the AST, a variable whose type is ConstantBuffer/TextureBuffer is
  /// represented as a VarDecl whose DeclContext is a HLSLBufferDecl. These
  /// VarDecl's type is labelled as the struct upon which ConstantBuffer/
  /// TextureBuffer is parameterized. For a such VarDecl, we need to create
  /// a corresponding SPIR-V variable for it. Later referencing of such a
  /// VarDecl does not need an extra OpAccessChain.
  uint32_t createCTBuffer(const VarDecl *decl);

  /// \brief Sets the <result-id> of the entry function.
  void setEntryFunctionId(uint32_t id) { entryFunctionId = id; }

public:
  /// The struct containing SPIR-V information of a AST Decl.
  struct DeclSpirvInfo {
    DeclSpirvInfo(uint32_t result = 0,
                  spv::StorageClass sc = spv::StorageClass::Function,
                  LayoutRule lr = LayoutRule::Void, int indexInCTB = -1)
        : resultId(result), storageClass(sc), layoutRule(lr),
          indexInCTBuffer(indexInCTB) {}

    /// Implicit conversion to SpirvEvalInfo.
    operator SpirvEvalInfo() const {
      return SpirvEvalInfo(resultId, storageClass, layoutRule);
    }

    uint32_t resultId;
    spv::StorageClass storageClass;
    /// Layout rule for this decl.
    LayoutRule layoutRule;
    /// Value >= 0 means that this decl is a VarDecl inside a cbuffer/tbuffer
    /// and this is the index; value < 0 means this is just a standalone decl.
    int indexInCTBuffer;
  };

  /// \brief Returns the SPIR-V information for the given decl.
  /// Returns nullptr if no such decl was previously registered.
  const DeclSpirvInfo *getDeclSpirvInfo(const NamedDecl *decl) const;

  /// \brief Returns the information for the given decl.
  ///
  /// This method will panic if the given decl is not registered.
  SpirvEvalInfo getDeclResultId(const NamedDecl *decl);

  /// \brief Returns the <result-id> for the given function if already
  /// registered; otherwise, treats the given function as a normal decl and
  /// returns a newly assigned <result-id> for it.
  uint32_t getOrRegisterFnResultId(const FunctionDecl *fn);

  /// \brief Returns the associated counter's <result-id> for the given
  /// {RW|Append|Consume}StructuredBuffer variable.
  uint32_t getOrCreateCounterId(const ValueDecl *decl);

  /// \brief Returns all defined stage (builtin/input/ouput) variables in this
  /// mapper.
  std::vector<uint32_t> collectStageVars() const;

  /// \brief Decorates all stage input and output variables with proper
  /// location and returns true on success.
  ///
  /// This method will write the location assignment into the module under
  /// construction.
  inline bool decorateStageIOLocations();

  /// \brief Decorates all resource variables with proper set and binding
  /// numbers and returns true on success.
  ///
  /// This method will write the set and binding number assignment into the
  /// module under construction.
  bool decorateResourceBindings();

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

  /// Creates a variable of struct type with explicit layout decorations.
  /// The sub-Decls in the given DeclContext will be treated as the struct
  /// fields. The struct type will be named as typeName, and the variable
  /// will be named as varName.
  ///
  /// Panics if the DeclContext is neither HLSLBufferDecl or RecordDecl.
  uint32_t createVarOfExplicitLayoutStruct(const DeclContext *decl,
                                           llvm::StringRef typeName,
                                           llvm::StringRef varName);

  /// Creates all the stage variables mapped from semantics on the given decl
  /// and returns true on success.
  ///
  /// Assumes the decl has semantic attached to itself or to its fields.
  bool createStageVars(const DeclaratorDecl *decl, uint32_t *value,
                       bool asInput, const llvm::Twine &namePrefix,
                       bool isPatchConstant, bool isOutputStream = false);

  /// Creates the SPIR-V variable instruction for the given StageVar and returns
  /// the <result-id>. Also sets whether the StageVar is a SPIR-V builtin and
  /// its storage class accordingly. name will be used as the debug name when
  /// creating a stage input/output variable.
  uint32_t createSpirvStageVar(StageVar *, const llvm::Twine &name);

  /// Creates the associated counter variable for RW/Append/Consume
  /// structured buffer.
  uint32_t createCounterVar(const ValueDecl *decl);

  /// Returns the proper SPIR-V storage class (Input or Output) for the given
  /// SigPoint.
  spv::StorageClass getStorageClassForSigPoint(const hlsl::SigPoint *);

  /// Returns true if the given SPIR-V stage variable has Input storage class.
  inline bool isInputStorageClass(const StageVar &v);

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
  /// Vector of all defined resource variables.
  llvm::SmallVector<ResourceVar, 8> resourceVars;
  /// Mapping from {Append|Consume}StructuredBuffers to their counter variables
  llvm::DenseMap<const NamedDecl *, uint32_t> counterVars;
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

bool DeclResultIdMapper::isInputStorageClass(const StageVar &v) {
  return getStorageClassForSigPoint(v.getSigPoint()) ==
         spv::StorageClass::Input;
}

} // end namespace spirv
} // end namespace clang

#endif