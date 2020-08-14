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

#include "dxc/DXIL/DxilSemantic.h"
#include "dxc/DXIL/DxilSigPoint.h"
#include "dxc/Support/SPIRVOptions.h"
#include "spirv/unified1/spirv.hpp11"
#include "clang/AST/Attr.h"
#include "clang/SPIRV/FeatureManager.h"
#include "clang/SPIRV/SpirvBuilder.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/SmallVector.h"

#include "GlPerVertex.h"

namespace clang {
namespace spirv {

class SpirvEmitter;

/// A struct containing information about a particular HLSL semantic.
struct SemanticInfo {
  llvm::StringRef str;            ///< The original semantic string
  const hlsl::Semantic *semantic; ///< The unique semantic object
  llvm::StringRef name;           ///< The semantic string without index
  uint32_t index;                 ///< The semantic index
  SourceLocation loc;             ///< Source code location

  bool isValid() const { return semantic != nullptr; }

  inline hlsl::Semantic::Kind getKind() const;
  /// \brief Returns true if this semantic is a SV_Target.
  inline bool isTarget() const;
};

/// \brief The class containing HLSL and SPIR-V information about a Vulkan stage
/// (builtin/input/output) variable.
class StageVar {
public:
  inline StageVar(const hlsl::SigPoint *sig, SemanticInfo semaInfo,
                  const VKBuiltInAttr *builtin, QualType astType,
                  uint32_t locCount)
      : sigPoint(sig), semanticInfo(std::move(semaInfo)), builtinAttr(builtin),
        type(astType), value(nullptr), isBuiltin(false),
        storageClass(spv::StorageClass::Max), location(nullptr),
        locationCount(locCount) {
    isBuiltin = builtinAttr != nullptr;
  }

  const hlsl::SigPoint *getSigPoint() const { return sigPoint; }
  const SemanticInfo &getSemanticInfo() const { return semanticInfo; }
  std::string getSemanticStr() const;

  QualType getAstType() const { return type; }

  SpirvVariable *getSpirvInstr() const { return value; }
  void setSpirvInstr(SpirvVariable *spvInstr) { value = spvInstr; }

  const VKBuiltInAttr *getBuiltInAttr() const { return builtinAttr; }

  bool isSpirvBuitin() const { return isBuiltin; }
  void setIsSpirvBuiltin() { isBuiltin = true; }

  spv::StorageClass getStorageClass() const { return storageClass; }
  void setStorageClass(spv::StorageClass sc) { storageClass = sc; }

  const VKLocationAttr *getLocationAttr() const { return location; }
  void setLocationAttr(const VKLocationAttr *loc) { location = loc; }

  const VKIndexAttr *getIndexAttr() const { return indexAttr; }
  void setIndexAttr(const VKIndexAttr *idx) { indexAttr = idx; }

  uint32_t getLocationCount() const { return locationCount; }

private:
  /// HLSL SigPoint. It uniquely identifies each set of parameters that may be
  /// input or output for each entry point.
  const hlsl::SigPoint *sigPoint;
  /// Information about HLSL semantic string.
  SemanticInfo semanticInfo;
  /// SPIR-V BuiltIn attribute.
  const VKBuiltInAttr *builtinAttr;
  /// The AST QualType.
  QualType type;
  /// SPIR-V instruction.
  SpirvVariable *value;
  /// Indicates whether this stage variable should be a SPIR-V builtin.
  bool isBuiltin;
  /// SPIR-V storage class this stage variable belongs to.
  spv::StorageClass storageClass;
  /// Location assignment if input/output variable.
  const VKLocationAttr *location;
  /// Index assignment if PS output variable
  const VKIndexAttr *indexAttr;
  /// How many locations this stage variable takes.
  uint32_t locationCount;
};

class ResourceVar {
public:
  ResourceVar(SpirvVariable *var, const Decl *decl, SourceLocation loc,
              const hlsl::RegisterAssignment *r, const VKBindingAttr *b,
              const VKCounterBindingAttr *cb, bool counter = false,
              bool globalsBuffer = false)
      : variable(var), srcLoc(loc), reg(r), binding(b), counterBinding(cb),
        isCounterVar(counter), isGlobalsCBuffer(globalsBuffer) {}

  SpirvVariable *getSpirvInstr() const { return variable; }
  SourceLocation getSourceLocation() const { return srcLoc; }
  const hlsl::RegisterAssignment *getRegister() const { return reg; }
  const VKBindingAttr *getBinding() const { return binding; }
  bool isCounter() const { return isCounterVar; }
  bool isGlobalsBuffer() const { return isGlobalsCBuffer; }
  const VKCounterBindingAttr *getCounterBinding() const {
    return counterBinding;
  }

private:
  SpirvVariable *variable;                    ///< The variable
  SourceLocation srcLoc;                      ///< Source location
  const hlsl::RegisterAssignment *reg;        ///< HLSL register assignment
  const VKBindingAttr *binding;               ///< Vulkan binding assignment
  const VKCounterBindingAttr *counterBinding; ///< Vulkan counter binding
  bool isCounterVar;                          ///< Couter variable or not
  bool isGlobalsCBuffer;                      ///< $Globals cbuffer or not
};

/// A (instruction-pointer, is-alias-or-not) pair for counter variables
class CounterIdAliasPair {
public:
  /// Default constructor to satisfy llvm::DenseMap
  CounterIdAliasPair() : counterVar(nullptr), isAlias(false) {}
  CounterIdAliasPair(SpirvVariable *var, bool alias)
      : counterVar(var), isAlias(alias) {}

  /// Returns the pointer to the counter variable. Dereferences first if this is
  /// an alias to a counter variable.
  SpirvInstruction *get(SpirvBuilder &builder, SpirvContext &spvContext) const;

  /// Stores the counter variable's pointer in srcPair to the curent counter
  /// variable. The current counter variable must be an alias.
  inline void assign(const CounterIdAliasPair &srcPair, SpirvBuilder &,
                     SpirvContext &) const;

private:
  SpirvVariable *counterVar;
  /// Note: legalization specific code
  bool isAlias;
};

/// A class for holding all the counter variables associated with a struct's
/// fields
///
/// A alias local RW/Append/Consume structured buffer will need an associated
/// counter variable generated. There are four forms such an alias buffer can
/// be:
///
/// 1 (AssocCounter#1). A stand-alone variable,
/// 2 (AssocCounter#2). A struct field,
/// 3 (AssocCounter#3). A struct containing alias fields,
/// 4 (AssocCounter#4). A nested struct containing alias fields.
///
/// We consider the first two cases as *final* alias entities; The last two
/// cases are called as *intermediate* alias entities, since we can still
/// decompose them and get final alias entities.
///
/// We need to create an associated counter variable no matter which form the
/// alias buffer is in, which means we need to recursively visit all fields of a
/// struct to discover if it's not AssocCounter#1. That means a hierarchy.
///
/// The purpose of this class is to provide such hierarchy in a *flattened* way.
/// Each field's associated counter is represented with an index vector and the
/// counter's <result-id>. For example, for the following structs,
///
/// struct S {
///       RWStructuredBuffer s1;
///   AppendStructuredBuffer s2;
/// };
///
/// struct T {
///   S t1;
///   S t2;
/// };
///
/// An instance of T will have four associated counters for
///   field: indices, <result-id>
///   t1.s1: [0, 0], <id-1>
///   t1.s2: [0, 1], <id-2>
///   t2.s1: [1, 0], <id-3>
///   t2.s2: [1, 1], <id-4>
class CounterVarFields {
public:
  CounterVarFields() = default;

  /// Registers a field's associated counter.
  void append(const llvm::SmallVector<uint32_t, 4> &indices,
              SpirvVariable *counter) {
    fields.emplace_back(indices, counter);
  }

  /// Returns the counter associated with the field at the given indices if it
  /// has. Returns nullptr otherwise.
  const CounterIdAliasPair *
  get(const llvm::SmallVectorImpl<uint32_t> &indices) const;

  /// Assigns to all the fields' associated counter from the srcFields.
  /// Returns true if there are no errors during the assignment.
  ///
  /// This first overload is for assigning a struct as whole: we need to update
  /// all the associated counters in the target struct. This second overload is
  /// for assigning a potentially nested struct.
  bool assign(const CounterVarFields &srcFields, SpirvBuilder &,
              SpirvContext &) const;
  bool assign(const CounterVarFields &srcFields,
              const llvm::SmallVector<uint32_t, 4> &dstPrefix,
              const llvm::SmallVector<uint32_t, 4> &srcPrefix, SpirvBuilder &,
              SpirvContext &) const;

private:
  struct IndexCounterPair {
    IndexCounterPair(const llvm::SmallVector<uint32_t, 4> &idx,
                     SpirvVariable *counter)
        : indices(idx), counterVar(counter, true) {}

    llvm::SmallVector<uint32_t, 4> indices; ///< Index vector
    CounterIdAliasPair counterVar;          ///< Counter variable information
  };

  llvm::SmallVector<IndexCounterPair, 4> fields;
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
  inline DeclResultIdMapper(ASTContext &context, SpirvContext &spirvContext,
                            SpirvBuilder &spirvBuilder, SpirvEmitter &emitter,
                            FeatureManager &features,
                            const SpirvCodeGenOptions &spirvOptions);

  /// \brief Returns the SPIR-V builtin variable.
  SpirvVariable *getBuiltinVar(spv::BuiltIn builtIn, QualType type,
                               SourceLocation);

  /// \brief Creates the stage output variables by parsing the semantics
  /// attached to the given function's parameter or return value and returns
  /// true on success. SPIR-V instructions will also be generated to update the
  /// contents of the output variables by extracting sub-values from the given
  /// storedValue. forPCF should be set to true for handling decls in patch
  /// constant function.
  ///
  /// Note that the control point stage output variable of HS should be created
  /// by the other overload.
  bool createStageOutputVar(const DeclaratorDecl *decl,
                            SpirvInstruction *storedValue, bool forPCF);
  /// \brief Overload for handling HS control point stage ouput variable.
  bool createStageOutputVar(const DeclaratorDecl *decl, uint32_t arraySize,
                            SpirvInstruction *invocationId,
                            SpirvInstruction *storedValue);

  /// \brief Creates the stage input variables by parsing the semantics attached
  /// to the given function's parameter and returns true on success. SPIR-V
  /// instructions will also be generated to load the contents from the input
  /// variables and composite them into one and write to *loadedValue. forPCF
  /// should be set to true for handling decls in patch constant function.
  bool createStageInputVar(const ParmVarDecl *paramDecl,
                           SpirvInstruction **loadedValue, bool forPCF);

  /// \brief Creates stage variables for raytracing.
  SpirvVariable *createRayTracingNVStageVar(spv::StorageClass sc,
                                            const VarDecl *decl);

  /// \brief Creates the taskNV stage variables for payload struct variable
  /// and returns true on success. SPIR-V instructions will also be generated
  /// to load/store the contents from/to *value. payloadMemOffset is incremented
  /// based on payload struct member size, alignment and offset, and SPIR-V
  /// decorations PerTaskNV and Offset are assigned to each member.
  bool createPayloadStageVars(const hlsl::SigPoint *sigPoint,
                              spv::StorageClass sc, const NamedDecl *decl,
                              bool asInput, QualType type,
                              const llvm::StringRef namePrefix,
                              SpirvInstruction **value,
                              uint32_t payloadMemOffset = 0);

  /// \brief Creates a function-scope paramter in the current function and
  /// returns its instruction. dbgArgNumber is used to specify the argument
  /// number of param among function parameters, which will be used for the
  /// debug information. Note that dbgArgNumber for the first function
  /// parameter must have "1", not "0", which is what Clang generates for
  /// LLVM debug metadata.
  SpirvFunctionParameter *createFnParam(const ParmVarDecl *param,
                                        uint32_t dbgArgNumber = 0);

  /// \brief Creates the counter variable associated with the given param.
  /// This is meant to be used for forward-declared functions and this objects
  /// of methods.
  ///
  /// Note: legalization specific code
  inline void createFnParamCounterVar(const VarDecl *param);

  /// \brief Creates a function-scope variable in the current function and
  /// returns its instruction.
  SpirvVariable *createFnVar(const VarDecl *var,
                             llvm::Optional<SpirvInstruction *> init);

  /// \brief Creates a file-scope variable and returns its instruction.
  SpirvVariable *createFileVar(const VarDecl *var,
                               llvm::Optional<SpirvInstruction *> init);

  /// \brief Creates an external-visible variable and returns its instruction.
  SpirvVariable *createExternVar(const VarDecl *var);

  /// \brief Returns an OpString instruction that represents the given VarDecl.
  /// VarDecl must be a variable of string type.
  ///
  /// This function inspects the VarDecl for an initialization expression. If
  /// initialization expression is not found, it will emit an error because the
  /// variable cannot be deduced to an OpString literal, and string variables do
  /// not exist in SPIR-V.
  ///
  /// Note: HLSL has the 'string' type which can be used for rare purposes such
  /// as printf (SPIR-V's DebugPrintf). SPIR-V does not have a 'char' or
  /// 'string' type, and therefore any variable of such type is never created.
  /// The string literal is evaluated when needed and an OpString is generated
  /// for it.
  SpirvInstruction *createOrUpdateStringVar(const VarDecl *);

  /// \brief Creates an Enum constant.
  void createEnumConstant(const EnumConstantDecl *decl);

  /// \brief Creates a cbuffer/tbuffer from the given decl.
  ///
  /// In the AST, cbuffer/tbuffer is represented as a HLSLBufferDecl, which is
  /// a DeclContext, and all fields in the buffer are represented as VarDecls.
  /// We cannot do the normal translation path, which will translate a field
  /// into a standalone variable. We need to create a single SPIR-V variable
  /// for the whole buffer. When we refer to the field VarDecl later, we need
  /// to do an extra OpAccessChain to get its pointer from the SPIR-V variable
  /// standing for the whole buffer.
  SpirvVariable *createCTBuffer(const HLSLBufferDecl *decl);

  /// \brief Creates a cbuffer/tbuffer from the given decl.
  ///
  /// In the AST, a variable whose type is ConstantBuffer/TextureBuffer is
  /// represented as a VarDecl whose DeclContext is a HLSLBufferDecl. These
  /// VarDecl's type is labelled as the struct upon which ConstantBuffer/
  /// TextureBuffer is parameterized. For a such VarDecl, we need to create
  /// a corresponding SPIR-V variable for it. Later referencing of such a
  /// VarDecl does not need an extra OpAccessChain.
  SpirvVariable *createCTBuffer(const VarDecl *decl);

  /// \brief Creates a PushConstant block from the given decl.
  SpirvVariable *createPushConstant(const VarDecl *decl);

  /// \brief Creates the $Globals cbuffer.
  void createGlobalsCBuffer(const VarDecl *var);

  /// \brief Returns the suitable type for the given decl, considering the
  /// given decl could possibly be created as an alias variable. If true, a
  /// pointer-to-the-value type will be returned, otherwise, just return the
  /// normal value type. For an alias variable having a associated counter, the
  /// counter variable will also be emitted.
  ///
  /// If the type is for an alias variable, writes true to *shouldBeAlias and
  /// writes storage class, layout rule, and valTypeId to *info.
  ///
  /// Note: legalization specific code
  QualType
  getTypeAndCreateCounterForPotentialAliasVar(const DeclaratorDecl *var,
                                              bool *shouldBeAlias = nullptr);

  /// \brief Sets the entry function.
  void setEntryFunction(SpirvFunction *fn) { entryFunction = fn; }

  /// Raytracing specific functions
  /// \brief Handle specific implicit declarations present only in raytracing
  /// stages.
  void createRayTracingNVImplicitVar(const VarDecl *varDecl);

  /// \brief Creates a ShaderRecordBufferNV block from the given decl.
  SpirvVariable *createShaderRecordBufferNV(const VarDecl *decl);
  SpirvVariable *createShaderRecordBufferNV(const HLSLBufferDecl *decl);

private:
  /// The struct containing SPIR-V information of a AST Decl.
  struct DeclSpirvInfo {
    /// Default constructor to satisfy DenseMap
    DeclSpirvInfo() : instr(nullptr), indexInCTBuffer(-1) {}

    DeclSpirvInfo(SpirvInstruction *instr_, int index = -1)
        : instr(instr_), indexInCTBuffer(index) {}

    /// Implicit conversion to SpirvInstruction*.
    operator SpirvInstruction *() const { return instr; }

    SpirvInstruction *instr;
    /// Value >= 0 means that this decl is a VarDecl inside a cbuffer/tbuffer
    /// and this is the index; value < 0 means this is just a standalone decl.
    int indexInCTBuffer;
  };

  /// \brief Returns the SPIR-V information for the given decl.
  /// Returns nullptr if no such decl was previously registered.
  const DeclSpirvInfo *getDeclSpirvInfo(const ValueDecl *decl) const;

public:
  /// \brief Returns the information for the given decl.
  ///
  /// This method will panic if the given decl is not registered.
  SpirvInstruction *getDeclEvalInfo(const ValueDecl *decl, SourceLocation loc);

  /// \brief Returns the instruction pointer for the given function if already
  /// registered; otherwise, treats the given function as a normal decl and
  /// returns a newly created instruction for it.
  SpirvFunction *getOrRegisterFn(const FunctionDecl *fn);

  /// Registers that the given decl should be translated into the given spec
  /// constant.
  void registerSpecConstant(const VarDecl *decl,
                            SpirvInstruction *specConstant);

  /// \brief Returns the associated counter's (instr-ptr, is-alias-or-not)
  /// pair for the given {RW|Append|Consume}StructuredBuffer variable.
  /// If indices is not nullptr, walks trhough the fields of the decl, expected
  /// to be of struct type, using the indices to find the field. Returns nullptr
  /// if the given decl has no associated counter variable created.
  const CounterIdAliasPair *getCounterIdAliasPair(
      const DeclaratorDecl *decl,
      const llvm::SmallVector<uint32_t, 4> *indices = nullptr);

  /// \brief Returns all the associated counters for the given decl. The decl is
  /// expected to be a struct containing alias RW/Append/Consume structured
  /// buffers. Returns nullptr if it does not.
  const CounterVarFields *getCounterVarFields(const DeclaratorDecl *decl);

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
  const SpirvType *getCTBufferPushConstantType(const DeclContext *decl);

  /// \brief Returns all defined stage (builtin/input/ouput) variables in this
  /// mapper.
  std::vector<SpirvVariable *> collectStageVars() const;

  /// \brief Writes out the contents in the function parameter for the GS
  /// stream output to the corresponding stage output variables in a recursive
  /// manner. Returns true on success, false if errors occur.
  ///
  /// decl is the Decl with semantic string attached and will be used to find
  /// the stage output variable to write to, value is the  SPIR-V variable to
  /// read data from.
  ///
  /// This method is specially for writing back per-vertex data at the time of
  /// OpEmitVertex in GS.
  bool writeBackOutputStream(const NamedDecl *decl, QualType type,
                             SpirvInstruction *value);

  /// \brief Negates to get the additive inverse of SV_Position.y if requested.
  SpirvInstruction *invertYIfRequested(SpirvInstruction *position,
                                       SourceLocation loc);

  /// \brief Reciprocates to get the multiplicative inverse of SV_Position.w
  /// if requested.
  SpirvInstruction *invertWIfRequested(SpirvInstruction *position,
                                       SourceLocation loc);

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

  /// \brief Returns whether the SPIR-V module requires SPIR-V legalization
  /// passes run to make it legal.
  bool requiresLegalization() const { return needsLegalization; }

  /// \brief Returns whether the SPIR-V module requires an optimization pass to
  /// flatten array/structure of resources.
  bool requiresFlatteningCompositeResources() const {
    return needsFlatteningCompositeResources;
  }

  /// \brief Returns the given decl's HLSL semantic information.
  static SemanticInfo getStageVarSemantic(const NamedDecl *decl);

  /// \brief Returns SPIR-V instruction for given stage var decl.
  SpirvInstruction *getStageVarInstruction(const DeclaratorDecl *decl) {
    auto *value = stageVarInstructions.lookup(decl);
    assert(value);
    return value;
  }

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
    Globals,
    ShaderRecordBufferNV,
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
  /// If arraySize is 0, the variable will be created as a struct ; if arraySize
  /// is > 0, the variable will be created as an array; if arraySize is -1, the
  /// variable will be created as a runtime array.
  ///
  /// Panics if the DeclContext is neither HLSLBufferDecl or RecordDecl.
  SpirvVariable *createStructOrStructArrayVarOfExplicitLayout(
      const DeclContext *decl, int arraySize, ContextUsageKind usageKind,
      llvm::StringRef typeName, llvm::StringRef varName);

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
  bool createStageVars(const hlsl::SigPoint *sigPoint, const NamedDecl *decl,
                       bool asInput, QualType asType, uint32_t arraySize,
                       const llvm::StringRef namePrefix,
                       llvm::Optional<SpirvInstruction *> invocationId,
                       SpirvInstruction **value, bool noWriteBack,
                       SemanticInfo *inheritSemantic);

  /// Creates the SPIR-V variable instruction for the given StageVar and returns
  /// the instruction. Also sets whether the StageVar is a SPIR-V builtin and
  /// its storage class accordingly. name will be used as the debug name when
  /// creating a stage input/output variable.
  SpirvVariable *createSpirvStageVar(StageVar *, const NamedDecl *decl,
                                     const llvm::StringRef name,
                                     SourceLocation);

  /// Returns true if all vk:: attributes usages are valid.
  bool validateVKAttributes(const NamedDecl *decl);

  /// Returns true if all vk::builtin usages are valid.
  bool validateVKBuiltins(const NamedDecl *decl,
                          const hlsl::SigPoint *sigPoint);

  /// Methods for creating counter variables associated with the given decl.

  /// Creates assoicated counter variables for all AssocCounter cases (see the
  /// comment of CounterVarFields).
  void createCounterVarForDecl(const DeclaratorDecl *decl);
  /// Creates the associated counter variable for final RW/Append/Consume
  /// structured buffer. Handles AssocCounter#1 and AssocCounter#2 (see the
  /// comment of CounterVarFields).
  ///
  /// declId is the SPIR-V instruction for the given decl. It should be non-zero
  /// for non-alias buffers.
  ///
  /// The counter variable will be created as an alias variable (of
  /// pointer-to-pointer type in Private storage class) if isAlias is true.
  ///
  /// Note: isAlias - legalization specific code
  void
  createCounterVar(const DeclaratorDecl *decl, SpirvInstruction *declInstr,
                   bool isAlias,
                   const llvm::SmallVector<uint32_t, 4> *indices = nullptr);
  /// Creates all assoicated counter variables by recursively visiting decl's
  /// fields. Handles AssocCounter#3 and AssocCounter#4 (see the comment of
  /// CounterVarFields).
  inline void createFieldCounterVars(const DeclaratorDecl *decl);
  void createFieldCounterVars(const DeclaratorDecl *rootDecl,
                              const DeclaratorDecl *decl,
                              llvm::SmallVector<uint32_t, 4> *indices);

  /// Decorates varInstr of the given asType with proper interpolation modes
  /// considering the attributes on the given decl.
  void decorateInterpolationMode(const NamedDecl *decl, QualType asType,
                                 SpirvVariable *varInstr);

  /// Returns the proper SPIR-V storage class (Input or Output) for the given
  /// SigPoint.
  spv::StorageClass getStorageClassForSigPoint(const hlsl::SigPoint *);

  /// Returns true if the given SPIR-V stage variable has Input storage class.
  inline bool isInputStorageClass(const StageVar &v);

  /// Creates DebugGlobalVariable and returns it if rich debug information
  /// generation is enabled. Otherwise, returns nullptr.
  SpirvDebugGlobalVariable *createDebugGlobalVariable(SpirvVariable *var,
                                                      const QualType &type,
                                                      const SourceLocation &loc,
                                                      const StringRef &name);

  /// Determines the register type for a resource that does not have an
  /// explicit register() declaration.  Returns true if it is able to
  /// determine the register type and will set |*registerTypeOut| to
  /// 'u', 's', 'b', or 't'. Assumes |registerTypeOut| to be non-nullptr.
  ///
  /// Uses the following mapping of HLSL types to register spaces:
  /// t - for shader resource views (SRV)
  ///    TEXTURE1D
  ///    TEXTURE1DARRAY
  ///    TEXTURE2D
  ///    TEXTURE2DARRAY
  ///    TEXTURE3D
  ///    TEXTURECUBE
  ///    TEXTURECUBEARRAY
  ///    TEXTURE2DMS
  ///    TEXTURE2DMSARRAY
  ///    STRUCTUREDBUFFER
  ///    BYTEADDRESSBUFFER
  ///    BUFFER
  ///    TBUFFER
  ///
  /// s - for samplers
  ///    SAMPLER
  ///    SAMPLER1D
  ///    SAMPLER2D
  ///    SAMPLER3D
  ///    SAMPLERCUBE
  ///    SAMPLERSTATE
  ///    SAMPLERCOMPARISONSTATE
  ///
  /// u - for unordered access views (UAV)
  ///    RWBYTEADDRESSBUFFER
  ///    RWSTRUCTUREDBUFFER
  ///    APPENDSTRUCTUREDBUFFER
  ///    CONSUMESTRUCTUREDBUFFER
  ///    RWBUFFER
  ///    RWTEXTURE1D
  ///    RWTEXTURE1DARRAY
  ///    RWTEXTURE2D
  ///    RWTEXTURE2DARRAY
  ///    RWTEXTURE3D
  ///
  /// b - for constant buffer views (CBV)
  ///    CBUFFER
  ///    CONSTANTBUFFER
  bool getImplicitRegisterType(const ResourceVar &var, char *registerTypeOut) const;

private:
  SpirvBuilder &spvBuilder;
  SpirvEmitter &theEmitter;
  const SpirvCodeGenOptions &spirvOptions;
  ASTContext &astContext;
  SpirvContext &spvContext;
  DiagnosticsEngine &diags;
  SpirvFunction *entryFunction;

  /// Mapping of all Clang AST decls to their instruction pointers.
  llvm::DenseMap<const ValueDecl *, DeclSpirvInfo> astDecls;
  llvm::DenseMap<const ValueDecl *, SpirvFunction *> astFunctionDecls;
  /// Vector of all defined stage variables.
  llvm::SmallVector<StageVar, 8> stageVars;
  /// Mapping from Clang AST decls to the corresponding stage variables.
  /// This field is only used by GS for manually emitting vertices, when
  /// we need to query the output stage variables involved in writing back. For
  /// other cases, stage variable reading and writing is done at the time of
  /// creating that stage variable, so that we don't need to query them again
  /// for reading and writing.
  llvm::DenseMap<const ValueDecl *, SpirvVariable *> stageVarInstructions;
  /// Vector of all defined resource variables.
  llvm::SmallVector<ResourceVar, 8> resourceVars;
  /// Mapping from {RW|Append|Consume}StructuredBuffers to their
  /// counter variables' (instr-ptr, is-alias-or-not) pairs
  ///
  /// conterVars holds entities of AssocCounter#1, fieldCounterVars holds
  /// entities of the rest.
  llvm::DenseMap<const DeclaratorDecl *, CounterIdAliasPair> counterVars;
  llvm::DenseMap<const DeclaratorDecl *, CounterVarFields> fieldCounterVars;

  /// Mapping from cbuffer/tbuffer/ConstantBuffer/TextureBufer/push-constant
  /// to the SPIR-V type.
  llvm::DenseMap<const DeclContext *, const SpirvType *> ctBufferPCTypes;

  /// The SPIR-V builtin variables accessed by WaveGetLaneCount(),
  /// WaveGetLaneIndex() and ray tracing builtins.
  ///
  /// These are the only few cases where SPIR-V builtin variables are accessed
  /// using HLSL intrinsic function calls. All other builtin variables are
  /// accessed using stage IO variables.
  llvm::DenseMap<uint32_t, SpirvVariable *> builtinToVarMap;

  /// Whether the translated SPIR-V binary needs legalization.
  ///
  /// The following cases will require legalization:
  ///
  /// 1. Opaque types (textures, samplers) within structs
  /// 2. Structured buffer aliasing
  /// 3. Using SPIR-V instructions not allowed in the currect shader stage
  ///
  /// This covers the second case:
  ///
  /// When we have a kind of structured or byte buffer, meaning one of the
  /// following
  ///
  /// * StructuredBuffer
  /// * RWStructuredBuffer
  /// * AppendStructuredBuffer
  /// * ConsumeStructuredBuffer
  /// * ByteAddressStructuredBuffer
  /// * RWByteAddressStructuredBuffer
  ///
  /// and assigning to them (using operator=, passing in as function parameter,
  /// returning as function return), we need legalization.
  ///
  /// All variable definitions (including static/non-static local/global
  /// variables, function parameters/returns) will gain another level of
  /// pointerness, unless they will generate externally visible SPIR-V
  /// variables. So variables and parameters will be of pointer-to-pointer type,
  /// while function returns will be of pointer type. We adopt this mechanism to
  /// convey to the legalization passes that they are *alias* variables, and
  /// all accesses should happen to the aliased-to-variables. Loading such an
  /// alias variable will give the pointer to the aliased-to-variable, while
  /// storing into such an alias variable should write the pointer to the
  /// aliased-to-variable.
  ///
  /// Based on the above, CodeGen should take care of the following AST nodes:
  ///
  /// * Definition of alias variables: should add another level of pointers
  /// * Assigning non-alias variables to alias variables: should avoid the load
  ///   over the non-alias variables
  /// * Accessing alias variables: should load the pointer first and then
  ///   further compose access chains.
  ///
  /// Note that the associated counters bring about their own complication.
  /// We also need to apply the alias mechanism for them.
  ///
  /// If this is true, SPIRV-Tools legalization passes will be executed after
  /// the translation to legalize the generated SPIR-V binary.
  ///
  /// Note: legalization specific code
  bool needsLegalization;

  /// Whether the translated SPIR-V binary needs flattening of composite
  /// resources.
  ///
  /// If the source HLSL contains global structure of resources, we need to run
  /// an additional SPIR-V optimization pass to flatten such structures.
  bool needsFlatteningCompositeResources;

public:
  /// The gl_PerVertex structs for both input and output
  GlPerVertex glPerVertex;
};

hlsl::Semantic::Kind SemanticInfo::getKind() const {
  assert(semantic);
  return semantic->GetKind();
}
bool SemanticInfo::isTarget() const {
  return semantic && semantic->GetKind() == hlsl::Semantic::Kind::Target;
}

void CounterIdAliasPair::assign(const CounterIdAliasPair &srcPair,
                                SpirvBuilder &builder,
                                SpirvContext &context) const {
  assert(isAlias);
  builder.createStore(counterVar, srcPair.get(builder, context),
                      /* SourceLocation */ {});
}

DeclResultIdMapper::DeclResultIdMapper(ASTContext &context,
                                       SpirvContext &spirvContext,
                                       SpirvBuilder &spirvBuilder,
                                       SpirvEmitter &emitter,
                                       FeatureManager &features,
                                       const SpirvCodeGenOptions &options)
    : spvBuilder(spirvBuilder), theEmitter(emitter), spirvOptions(options),
      astContext(context), spvContext(spirvContext),
      diags(context.getDiagnostics()), entryFunction(nullptr),
      needsLegalization(false), needsFlatteningCompositeResources(false),
      glPerVertex(context, spirvContext, spirvBuilder) {}

bool DeclResultIdMapper::decorateStageIOLocations() {
  if (spvContext.isRay() || spvContext.isAS()) {
    // No location assignment for any raytracing stage variables or
    // amplification shader variables
    return true;
  }
  // Try both input and output even if input location assignment failed
  return finalizeStageIOLocations(true) & finalizeStageIOLocations(false);
}

bool DeclResultIdMapper::isInputStorageClass(const StageVar &v) {
  return getStorageClassForSigPoint(v.getSigPoint()) ==
         spv::StorageClass::Input;
}

void DeclResultIdMapper::createFnParamCounterVar(const VarDecl *param) {
  createCounterVarForDecl(param);
}

void DeclResultIdMapper::createFieldCounterVars(const DeclaratorDecl *decl) {
  llvm::SmallVector<uint32_t, 4> indices;
  createFieldCounterVars(decl, decl, &indices);
}

} // end namespace spirv
} // end namespace clang

#endif
