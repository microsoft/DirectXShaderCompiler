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

#include "GlPerVertex.h"
#include "SpirvEvalInfo.h"
#include "TypeTranslator.h"

namespace clang {
namespace spirv {

/// \brief The class containing HLSL and SPIR-V information about a Vulkan stage
/// (builtin/input/output) variable.
class StageVar {
public:
  inline StageVar(const hlsl::SigPoint *sig, llvm::StringRef semaStr,
                  const hlsl::Semantic *sema, llvm::StringRef semaName,
                  uint32_t semaIndex, const VKBuiltInAttr *builtin,
                  uint32_t type)
      : sigPoint(sig), semanticStr(semaStr), semantic(sema),
        semanticName(semaName), semanticIndex(semaIndex), builtinAttr(builtin),
        typeId(type), valueId(0), isBuiltin(false),
        storageClass(spv::StorageClass::Max), location(nullptr) {
    isBuiltin = builtinAttr != nullptr;
  }

  const hlsl::SigPoint *getSigPoint() const { return sigPoint; }
  const hlsl::Semantic *getSemantic() const { return semantic; }

  uint32_t getSpirvTypeId() const { return typeId; }

  uint32_t getSpirvId() const { return valueId; }
  void setSpirvId(uint32_t id) { valueId = id; }

  const VKBuiltInAttr *getBuiltInAttr() const { return builtinAttr; }

  std::string getSemanticStr() const;
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
  /// Original HLSL semantic string (without index) in the source code.
  llvm::StringRef semanticName;
  /// HLSL semantic index.
  uint32_t semanticIndex;
  /// SPIR-V BuiltIn attribute.
  const VKBuiltInAttr *builtinAttr;
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
/// single stage variable if it is of non-struct type. If it is of struct
/// type, the fields with attached semantics will need to be translated into
/// stage variables per Vulkan's requirements.
class DeclResultIdMapper {
public:
  inline DeclResultIdMapper(const hlsl::ShaderModel &stage, ASTContext &context,
                            ModuleBuilder &builder,
                            const EmitSPIRVOptions &spirvOptions);

  /// \brief Creates the stage output variables by parsing the semantics
  /// attached to the given function's parameter or return value and returns
  /// true on success. SPIR-V instructions will also be generated to update the
  /// contents of the output variables by extracting sub-values from the given
  /// storedValue. forPCF should be set to true for handling decls in patch
  /// constant function.
  ///
  /// Note that the control point stage output variable of HS should be created
  /// by the other overload.
  bool createStageOutputVar(const DeclaratorDecl *decl, uint32_t storedValue,
                            bool forPCF);
  /// \brief Overload for handling HS control point stage ouput variable.
  bool createStageOutputVar(const DeclaratorDecl *decl, uint32_t arraySize,
                            uint32_t invocationId, uint32_t storedValue);

  /// \brief Creates the stage input variables by parsing the semantics attached
  /// to the given function's parameter and returns true on success. SPIR-V
  /// instructions will also be generated to load the contents from the input
  /// variables and composite them into one and write to *loadedValue. forPCF
  /// should be set to true for handling decls in patch constant function.
  bool createStageInputVar(const ParmVarDecl *paramDecl, uint32_t *loadedValue,
                           bool forPCF);

  /// \brief Creates a function-scope paramter in the current function and
  /// returns its <result-id>.
  uint32_t createFnParam(const ParmVarDecl *param);

  /// \brief Creates a function-scope variable in the current function and
  /// returns its <result-id>.
  uint32_t createFnVar(const VarDecl *var, llvm::Optional<uint32_t> init);

  /// \brief Creates a file-scope variable and returns its <result-id>.
  uint32_t createFileVar(const VarDecl *var, llvm::Optional<uint32_t> init);

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

  /// \brief Creates a PushConstant block from the given decl.
  uint32_t createPushConstant(const VarDecl *decl);

  /// \brief Sets the <result-id> of the entry function.
  void setEntryFunctionId(uint32_t id) { entryFunctionId = id; }

private:
  /// The struct containing SPIR-V information of a AST Decl.
  struct DeclSpirvInfo {
    /// Default constructor to satisfy DenseMap
    DeclSpirvInfo() : info(0), indexInCTBuffer(-1) {}

    DeclSpirvInfo(const SpirvEvalInfo &info_, int index = -1)
        : info(info_), indexInCTBuffer(index) {}

    /// Implicit conversion to SpirvEvalInfo.
    operator SpirvEvalInfo() const { return info; }

    SpirvEvalInfo info;
    /// Value >= 0 means that this decl is a VarDecl inside a cbuffer/tbuffer
    /// and this is the index; value < 0 means this is just a standalone decl.
    int indexInCTBuffer;
  };

  /// \brief Returns the SPIR-V information for the given decl.
  /// Returns nullptr if no such decl was previously registered.
  const DeclSpirvInfo *getDeclSpirvInfo(const ValueDecl *decl) const;

public:
  /// \brief Returns the information for the given decl. If the decl is not
  /// registered previously, return an invalid SpirvEvalInfo.
  ///
  /// This method will emit a fatal error if checkRegistered is true and the
  /// decl is not registered.
  SpirvEvalInfo getDeclResultId(const ValueDecl *decl,
                                bool checkRegistered = true);

  /// \brief Returns the <result-id> for the given function if already
  /// registered; otherwise, treats the given function as a normal decl and
  /// returns a newly assigned <result-id> for it.
  uint32_t getOrRegisterFnResultId(const FunctionDecl *fn);

  /// \brief Returns the associated counter's <result-id> for the given
  /// {RW|Append|Consume}StructuredBuffer variable.
  uint32_t getOrCreateCounterId(const ValueDecl *decl);

  /// \brief Returns the <type-id> for the given cbuffer, tbuffer,
  /// ConstantBuffer, TextureBuffer, or push constant block.
  ///
  /// Note: we need this method because constant/texture buffers and push
  /// constant blocks are all represented as normal struct types upon which
  /// they are parameterized. That is different from structured buffers,
  /// for which we can tell they are not normal structs by investigating
  /// the name. But for constant/texture buffers and push constant blocks,
  /// we need to have the additional Block/BufferBlock decoration to keep
  /// type consistent. Normal translation path for structs via TypeTranslator
  /// won't attach Block/BufferBlock decoration.
  uint32_t getCTBufferPushConstantTypeId(const DeclContext *decl);

  /// \brief Returns all defined stage (builtin/input/ouput) variables in this
  /// mapper.
  std::vector<uint32_t> collectStageVars() const;

  /// \brief Writes out the contents in the function parameter for the GS
  /// stream output to the corresponding stage output variables in a recursive
  /// manner. Returns true on success, false if errors occur.
  ///
  /// decl is the Decl with semantic string attached and will be used to find
  /// the stage output variable to write to, value is the <result-id> for the
  /// SPIR-V variable to read data from.
  ///
  /// This method is specially for writing back per-vertex data at the time of
  /// OpEmitVertex in GS.
  bool writeBackOutputStream(const ValueDecl *decl, uint32_t value);

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
  /// \brief Wrapper method to create a fatal error message and report it
  /// in the diagnostic engine associated with this consumer.
  template <unsigned N>
  DiagnosticBuilder emitFatalError(const char (&message)[N],
                                   SourceLocation loc) {
    const auto diagId =
        diags.getCustomDiagID(clang::DiagnosticsEngine::Fatal, message);
    return diags.Report(loc, diagId);
  }

  /// \brief Wrapper method to create an error message and report it
  /// in the diagnostic engine associated with this consumer.
  template <unsigned N>
  DiagnosticBuilder emitError(const char (&message)[N], SourceLocation loc) {
    const auto diagId =
        diags.getCustomDiagID(clang::DiagnosticsEngine::Error, message);
    return diags.Report(loc, diagId);
  }

  /// \brief Wrapper method to create a warning message and report it
  /// in the diagnostic engine associated with this consumer.
  template <unsigned N>
  DiagnosticBuilder emitWarning(const char (&message)[N], SourceLocation loc) {
    const auto diagId =
        diags.getCustomDiagID(clang::DiagnosticsEngine::Warning, message);
    return diags.Report(loc, diagId);
  }

  /// \brief Wrapper method to create a note message and report it
  /// in the diagnostic engine associated with this consumer.
  template <unsigned N>
  DiagnosticBuilder emitNote(const char (&message)[N], SourceLocation loc) {
    const auto diagId =
        diags.getCustomDiagID(clang::DiagnosticsEngine::Note, message);
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

  /// \brief An enum class for representing what the DeclContext is used for
  enum class ContextUsageKind {
    CBuffer,
    TBuffer,
    PushConstant,
  };

  /// Creates a variable of struct type with explicit layout decorations.
  /// The sub-Decls in the given DeclContext will be treated as the struct
  /// fields. The struct type will be named as typeName, and the variable
  /// will be named as varName.
  ///
  /// This method should only be used for cbuffers/ContantBuffers, tbuffers/
  /// TextureBuffers, and PushConstants. usageKind must be set properly
  /// depending on the usage kind.
  ///
  /// Panics if the DeclContext is neither HLSLBufferDecl or RecordDecl.
  uint32_t createVarOfExplicitLayoutStruct(const DeclContext *decl,
                                           ContextUsageKind usageKind,
                                           llvm::StringRef typeName,
                                           llvm::StringRef varName);

  /// A struct containing information about a particular HLSL semantic.
  struct SemanticInfo {
    llvm::StringRef str;            ///< The original semantic string
    const hlsl::Semantic *semantic; ///< The unique semantic object
    llvm::StringRef name;           ///< The semantic string without index
    uint32_t index;                 ///< The semantic index
    SourceLocation loc;             ///< Source code location

    bool isValid() const { return semantic != nullptr; }
  };

  /// Returns the given decl's HLSL semantic information.
  static SemanticInfo getStageVarSemantic(const ValueDecl *decl);

  /// Creates all the stage variables mapped from semantics on the given decl.
  /// Returns true on sucess.
  ///
  /// If decl is of struct type, this means flattening it and create stand-
  /// alone variables for each field. If arraySize is not zero, the created
  /// stage variables will have an additional arrayness over its original type.
  /// This is for supporting HS/DS/GS, which takes in primitives containing
  /// multiple vertices. asType should be the type we are treating decl as;
  /// For HS/DS/GS, the outermost arrayness should be discarded and use
  /// arraySize instead.
  ///
  /// Also performs reading the stage variables and compose a temporary value
  /// of the given type and writing into *value, if asInput is true. Otherwise,
  /// Decomposes the *value according to type and writes back into the stage
  /// output variables, unless noWriteBack is set to true. noWriteBack is used
  /// by GS since in GS we manually control write back using .Append() method.
  ///
  /// invocationId is only used for HS to indicate the index of the output
  /// array element to write to.
  ///
  /// Assumes the decl has semantic attached to itself or to its fields.
  /// If inheritSemantic is valid, it will override all semantics attached to
  /// the children of this decl, and the children of this decl will be using
  /// the semantic in inheritSemantic, with index increasing sequentially.
  bool createStageVars(const hlsl::SigPoint *sigPoint,
                       const DeclaratorDecl *decl, bool asInput, QualType type,
                       uint32_t arraySize, const llvm::StringRef namePrefix,
                       llvm::Optional<uint32_t> invocationId, uint32_t *value,
                       bool noWriteBack, SemanticInfo *inheritSemantic);

  /// Creates the SPIR-V variable instruction for the given StageVar and returns
  /// the <result-id>. Also sets whether the StageVar is a SPIR-V builtin and
  /// its storage class accordingly. name will be used as the debug name when
  /// creating a stage input/output variable.
  uint32_t createSpirvStageVar(StageVar *, const DeclaratorDecl *decl,
                               const llvm::StringRef name, SourceLocation);

  /// Returns true if all vk::builtin usages are valid.
  bool validateVKBuiltins(const DeclaratorDecl *decl,
                          const hlsl::SigPoint *sigPoint);

  /// Creates the associated counter variable for RW/Append/Consume
  /// structured buffer.
  uint32_t createCounterVar(const ValueDecl *decl);

  /// Decorates varId of the given asType with proper interpolation modes
  /// considering the attributes on the given decl.
  void decoratePSInterpolationMode(const DeclaratorDecl *decl, QualType asType,
                                   uint32_t varId);

  /// Returns the proper SPIR-V storage class (Input or Output) for the given
  /// SigPoint.
  spv::StorageClass getStorageClassForSigPoint(const hlsl::SigPoint *);

  /// Returns true if the given SPIR-V stage variable has Input storage class.
  inline bool isInputStorageClass(const StageVar &v);

private:
  const hlsl::ShaderModel &shaderModel;
  ModuleBuilder &theBuilder;
  const EmitSPIRVOptions &spirvOptions;
  ASTContext &astContext;
  DiagnosticsEngine &diags;

  TypeTranslator typeTranslator;

  uint32_t entryFunctionId;

  /// Mapping of all Clang AST decls to their <result-id>s.
  llvm::DenseMap<const ValueDecl *, DeclSpirvInfo> astDecls;
  /// Vector of all defined stage variables.
  llvm::SmallVector<StageVar, 8> stageVars;
  /// Mapping from Clang AST decls to the corresponding stage variables'
  /// <result-id>s.
  /// This field is only used by GS for manually emitting vertices, when
  /// we need to query the <result-id> of the output stage variables
  /// involved in writing back. For other cases, stage variable reading
  /// and writing is done at the time of creating that stage variable,
  /// so that we don't need to query them again for reading and writing.
  llvm::DenseMap<const ValueDecl *, uint32_t> stageVarIds;
  /// Vector of all defined resource variables.
  llvm::SmallVector<ResourceVar, 8> resourceVars;
  /// Mapping from {RW|Append|Consume}StructuredBuffers to their
  /// counter variables
  llvm::DenseMap<const ValueDecl *, uint32_t> counterVars;

  /// Mapping from cbuffer/tbuffer/ConstantBuffer/TextureBufer/push-constant
  /// to the <type-id>
  llvm::DenseMap<const DeclContext *, uint32_t> ctBufferPCTypeIds;

public:
  /// The gl_PerVertex structs for both input and output
  GlPerVertex glPerVertex;
};

DeclResultIdMapper::DeclResultIdMapper(const hlsl::ShaderModel &model,
                                       ASTContext &context,
                                       ModuleBuilder &builder,
                                       const EmitSPIRVOptions &options)
    : shaderModel(model), theBuilder(builder), spirvOptions(options),
      astContext(context), diags(context.getDiagnostics()),
      typeTranslator(context, builder, diags), entryFunctionId(0),
      glPerVertex(model, context, builder, typeTranslator) {}

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
