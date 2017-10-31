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

/// The class for representing special gl_PerVertex builtin interface block.
/// The Position, PointSize, ClipDistance, and CullDistance builtin should
/// be handled by this class, except for Position builtin used in GS output
/// and PS input.
///
/// Although the Vulkan spec does not require this directly, it seems the only
/// way to avoid violating the spec is to group the Position, ClipDistance, and
/// CullDistance builtins together into a struct. That's also how GLSL handles
/// these builtins. In GLSL, this struct is called gl_PerVertex.
///
/// This struct should appear as the entry point parameters but it should not
/// have location assignment. We can have two such block at most: one for input,
/// one for output.
///
/// Reading/writing of the ClipDistance/CullDistance builtin is not as
/// straightforward as other builtins. This is because in HLSL, we can have
/// multiple entities annotated with SV_ClipDistance/SV_CullDistance and they
/// can be float or vector of float type. For example,
///
///   float2 var1 : SV_ClipDistance2,
///   float  var2 : SV_ClipDistance0,
///   float3 var3 : SV_ClipDistance1,
///
/// But in Vulkan, ClipDistance/CullDistance is required to be a float array.
/// So we need to combine these variables into one single float array. The way
/// we do it is to sort all entities according to the SV_ClipDistance/
/// SV_CullDistance index, and concatenate them tightly. So for the above,
/// var2 will take the first two floats in the array, var3 will take the next
/// three, and var1 will take the next two. In total, we have an size-6 float
/// array for ClipDistance builtin.
class GlPerVertex {
public:
  GlPerVertex(const hlsl::ShaderModel &sm, ASTContext &context,
              ModuleBuilder &builder, TypeTranslator &translator);

  /// Records an declaration of SV_ClipDistance/SV_CullDistance so later
  /// we can caculate the ClipDistance/CullDistance array layout.
  bool recordClipCullDistanceDecl(const DeclaratorDecl *decl, bool asInput);

  /// Calculates the layout for ClipDistance/CullDistance arrays.
  void calculateClipCullDistanceArraySize();

  /// Emits SPIR-V code for the input and/or ouput gl_PerVertex builtin
  /// interface blocks. If inputArrayLength is not zero, the input gl_PerVertex
  /// will have an additional arrayness of the given size. Similarly for
  /// outputArrayLength.
  ///
  /// Note that this method should be called after recordClipCullDistanceDecl()
  /// and calculateClipCullDistanceArraySize().
  void generateVars(uint32_t inputArrayLength, uint32_t outputArrayLength);

  /// Returns the <result-id>s for stage input variables.
  llvm::SmallVector<uint32_t, 4> getStageInVars() const;
  /// Returns the <result-id>s for stage output variables.
  llvm::SmallVector<uint32_t, 4> getStageOutVars() const;

  /// Tries to access the builtin translated from the given HLSL semantic of
  /// the given index. If sigPoint dicates this is input, builtins will be read
  /// to compose a new temporary value of the correct type and writes to *value.
  /// Otherwise, the *value will be decomposed and writes to the builtins.
  /// Emits SPIR-V instructions and returns true if we are accessing builtins
  /// belonging to gl_PerVertex. Does nothing and returns true if we are
  /// accessing builtins not in gl_PerVertex. Returns false if errors occurs.
  bool tryToAccess(hlsl::Semantic::Kind, uint32_t semanticIndex,
                   llvm::Optional<uint32_t> invocationId, uint32_t *value,
                   hlsl::SigPoint::Kind sigPoint);

private:
  template <unsigned N>
  DiagnosticBuilder emitError(const char (&message)[N],
                              SourceLocation loc = {}) {
    const auto diagId = astContext.getDiagnostics().getCustomDiagID(
        clang::DiagnosticsEngine::Error, message);
    return astContext.getDiagnostics().Report(loc, diagId);
  }

  /// Creates a gl_PerVertex interface block variable. If arraySize is not zero,
  /// The created variable will be an array of gl_PerVertex of the given size.
  /// Otherwise, it will just be a plain struct.
  uint32_t createBlockVar(bool asInput, uint32_t arraySize);
  /// Creates a stand-alone Position builtin variable.
  uint32_t createPositionVar(bool asInput);
  /// Creates a stand-alone ClipDistance builtin variable.
  uint32_t createClipDistanceVar(bool asInput, uint32_t arraySize);
  /// Creates a stand-alone CullDistance builtin variable.
  uint32_t createCullDistanceVar(bool asInput, uint32_t arraySize);

  /// Emits SPIR-V instructions for reading the Position builtin.
  uint32_t readPosition() const;
  /// Emits SPIR-V instructions for reading the data starting from offset in
  /// the ClipDistance/CullDistance builtin. For clipCullIndex, 2 means
  /// ClipDistance; 3 means CullDistance. The data readed will be transformed
  /// into the given type asType.
  uint32_t readClipCullArrayAsType(uint32_t clipCullIndex, uint32_t offset,
                                   QualType asType) const;
  /// Emits SPIR-V instructions to read a field in gl_PerVertex.
  bool readField(hlsl::Semantic::Kind semanticKind, uint32_t semanticIndex,
                 uint32_t *value);

  /// Emits SPIR-V instructions for writing the Position builtin.
  void writePosition(llvm::Optional<uint32_t> invocationId,
                     uint32_t value) const;
  /// Emits SPIR-V instructions for writing data into the ClipDistance/
  /// CullDistance builtin starting from offset. For clipCullIndex, 2 means
  /// ClipDistance; 3 means CullDistance. The value to be written is fromValue,
  /// whose type is fromType. Necessary transformations will be generated
  /// to make sure type correctness.
  void writeClipCullArrayFromType(llvm::Optional<uint32_t> invocationId,
                                  uint32_t clipCullIndex, uint32_t offset,
                                  QualType fromType, uint32_t fromValue) const;
  /// Emits SPIR-V instructions to write a field in gl_PerVertex.
  bool writeField(hlsl::Semantic::Kind semanticKind, uint32_t semanticIndex,

                  llvm::Optional<uint32_t> invocationId, uint32_t *value);

  /// Internal implementation for recordClipCullDistanceDecl().
  bool doClipCullDistanceDecl(const DeclaratorDecl *decl, QualType type,
                              bool asInput);

private:
  using SemanticIndexToTypeMap = llvm::DenseMap<uint32_t, QualType>;
  using SemanticIndexToArrayOffsetMap = llvm::DenseMap<uint32_t, uint32_t>;

  const hlsl::ShaderModel &shaderModel;
  ASTContext &astContext;
  ModuleBuilder &theBuilder;
  TypeTranslator &typeTranslator;

  /// We can have Position, ClipDistance, and CullDistance either grouped (G)
  /// into the gl_PerVertex struct, or separated (S) as stand-alone variables.
  /// The following table shows for each shader stage, which one is used:
  ///
  /// ===== ===== ======
  /// Stage Input Output
  /// ===== ===== ======
  ///  VS     X     G
  ///  HS     G     G
  ///  DS     G     G
  ///  GS     G     S
  ///  PS     S     X
  /// ===== ===== ======
  ///
  /// Note that when we use separated variables, there is no extra arrayness.
  ///
  /// So depending on the shader stage, we may use one of the following set
  /// of variables to store <result-id>s of the variables:

  /// Indicates which set of variables are used.
  bool inIsGrouped, outIsGrouped;
  /// Input/output gl_PerVertex block variable if grouped.
  uint32_t inBlockVar, outBlockVar;
  /// Input/output ClipDistance/CullDistance variable if separated.
  uint32_t inClipVar, inCullVar;
  uint32_t outClipVar, outCullVar;

  /// The array size for the input/output gl_PerVertex block variabe.
  /// HS input and output, DS input, GS input has an additional level of
  /// arrayness. The array size is stored in this variable. Zero means
  /// the corresponding variable is a plain struct, not an array.
  uint32_t inArraySize, outArraySize;
  /// The array size of input/output ClipDistance/CullDistance float arrays.
  /// This is not the array size of the whole gl_PerVertex struct.
  uint32_t inClipArraySize, outClipArraySize;
  uint32_t inCullArraySize, outCullArraySize;

  /// We need to record all SV_ClipDistance/SV_CullDistance decls' types
  /// since we need to generate the necessary conversion instructions when
  /// accessing the ClipDistance/CullDistance builtins.
  SemanticIndexToTypeMap inClipType, outClipType;
  SemanticIndexToTypeMap inCullType, outCullType;
  /// We also need to keep track of all SV_ClipDistance/SV_CullDistance decls'
  /// offsets in the float array.
  SemanticIndexToArrayOffsetMap inClipOffset, outClipOffset;
  SemanticIndexToArrayOffsetMap inCullOffset, outCullOffset;
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
                            ModuleBuilder &builder, DiagnosticsEngine &diag,
                            const EmitSPIRVOptions &spirvOptions);

  /// \brief Creates the stage output variables by parsing the semantics
  /// attached to the given function's parameter or return value and returns
  /// true on success. SPIR-V instructions will also be generated to update the
  /// contents of the output variables by extracting sub-values from the given
  /// storedValue.
  ///
  /// Note that the control point stage output variable of HS should be created
  /// by the other overload.
  bool createStageOutputVar(const DeclaratorDecl *decl, uint32_t storedValue,
                            bool isPatchConstant);
  /// \brief Overload for handling HS control point stage ouput variable.
  bool createStageOutputVar(const DeclaratorDecl *decl, uint32_t arraySize,
                            uint32_t invocationId, uint32_t storedValue);

  /// \brief Creates the stage input variables by parsing the semantics attached
  /// to the given function's parameter and returns true on success. SPIR-V
  /// instructions will also be generated to load the contents from the input
  /// variables and composite them into one and write to *loadedValue.
  bool createStageInputVar(const ParmVarDecl *paramDecl, uint32_t *loadedValue,
                           bool isPatchConstant);

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

private:
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

public:
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

  /// Creates a variable of struct type with explicit layout decorations.
  /// The sub-Decls in the given DeclContext will be treated as the struct
  /// fields. The struct type will be named as typeName, and the variable
  /// will be named as varName.
  ///
  /// Panics if the DeclContext is neither HLSLBufferDecl or RecordDecl.
  uint32_t createVarOfExplicitLayoutStruct(const DeclContext *decl,
                                           llvm::StringRef typeName,
                                           llvm::StringRef varName);

  /// Creates all the stage variables mapped from semantics on the given decl.
  /// Return true on sucess.
  ///
  /// If decl is of struct type, this means flattening it and create stand-
  /// alone variables for each field. If arraySize is not zero, the created
  /// stage variables will have an additional arrayness over its original type.
  /// This is for supporting HS/DS/GS, which takes in primitives containing
  /// multiple vertices. asType should be the type we are treating decl as;
  /// For HS/DS/GS, the outermost arrayness should be discarded and use
  /// arraySize instead.
  ///
  /// Also performs updating the stage variables (loading/storing from/to the
  /// given value) depending on asInput.
  ///
  /// invocationId is only used for HS to indicate the index of the output
  /// array element to write to.
  ///
  /// Assumes the decl has semantic attached to itself or to its fields.
  bool createStageVars(const DeclaratorDecl *decl,
                       const hlsl::SigPoint *sigPoint, bool asInput,
                       QualType type, uint32_t arraySize,
                       llvm::Optional<uint32_t> invocationId, uint32_t *value,
                       const llvm::Twine &namePrefix);

  /// Creates the SPIR-V variable instruction for the given StageVar and returns
  /// the <result-id>. Also sets whether the StageVar is a SPIR-V builtin and
  /// its storage class accordingly. name will be used as the debug name when
  /// creating a stage input/output variable.
  uint32_t createSpirvStageVar(StageVar *, const llvm::Twine &name);

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
  DiagnosticsEngine &diags;

  TypeTranslator typeTranslator;

  uint32_t entryFunctionId;

  /// Mapping of all Clang AST decls to their <result-id>s.
  llvm::DenseMap<const NamedDecl *, DeclSpirvInfo> astDecls;
  /// Vector of all defined stage variables.
  llvm::SmallVector<StageVar, 8> stageVars;
  /// Vector of all defined resource variables.
  llvm::SmallVector<ResourceVar, 8> resourceVars;
  /// Mapping from {RW|Append|Consume}StructuredBuffers to their
  /// counter variables
  llvm::DenseMap<const NamedDecl *, uint32_t> counterVars;

public:
  /// The gl_PerVertex structs for both input and output
  GlPerVertex glPerVertex;
};

DeclResultIdMapper::DeclResultIdMapper(const hlsl::ShaderModel &model,
                                       ASTContext &context,
                                       ModuleBuilder &builder,
                                       DiagnosticsEngine &diag,
                                       const EmitSPIRVOptions &options)
    : shaderModel(model), theBuilder(builder), spirvOptions(options),
      diags(diag), typeTranslator(context, builder, diag), entryFunctionId(0),
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