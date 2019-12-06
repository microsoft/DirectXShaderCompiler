//===-- SpirvBuilder.h - SPIR-V Builder -----------------------*- C++ -*---===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_CLANG_SPIRV_SPIRVBUILDER_H
#define LLVM_CLANG_SPIRV_SPIRVBUILDER_H

#include "clang/SPIRV/SpirvBasicBlock.h"
#include "clang/SPIRV/SpirvContext.h"
#include "clang/SPIRV/SpirvFunction.h"
#include "clang/SPIRV/SpirvInstruction.h"
#include "clang/SPIRV/SpirvModule.h"

namespace clang {
namespace spirv {

/// The SPIR-V in-memory representation builder class.
///
/// This class exports API for constructing SPIR-V in-memory representation
/// interactively. Under the hood, it allocates SPIR-V entity objects from
/// SpirvContext and wires them up into a connected structured representation.
///
/// At any time, there can only exist at most one function under building;
/// but there can exist multiple basic blocks under construction.
///
/// Call `getModule()` to get the SPIR-V words after finishing building the
/// module.
class SpirvBuilder {
  friend class CapabilityVisitor;

public:
  SpirvBuilder(ASTContext &ac, SpirvContext &c, const SpirvCodeGenOptions &);
  ~SpirvBuilder() = default;

  // Forbid copy construction and assignment
  SpirvBuilder(const SpirvBuilder &) = delete;
  SpirvBuilder &operator=(const SpirvBuilder &) = delete;

  // Forbid move construction and assignment
  SpirvBuilder(SpirvBuilder &&) = delete;
  SpirvBuilder &operator=(SpirvBuilder &&) = delete;

  /// Returns the SPIR-V module being built.
  SpirvModule *getModule() { return module; }

  // === Function and Basic Block ===

  /// \brief Begins building a SPIR-V function by allocating a SpirvFunction
  /// object. Returns the pointer for the function on success. Returns nullptr
  /// on failure.
  ///
  /// At any time, there can only exist at most one function under building.
  SpirvFunction *beginFunction(QualType returnType,
                               llvm::ArrayRef<QualType> paramTypes,
                               SourceLocation, llvm::StringRef name = "",
                               bool isPrecise = false,
                               SpirvFunction *func = nullptr);

  /// \brief Creates and registers a function parameter of the given pointer
  /// type in the current function and returns its pointer.
  SpirvFunctionParameter *addFnParam(QualType ptrType, bool isPrecise,
                                     SourceLocation, llvm::StringRef name = "");

  /// \brief Creates a local variable of the given type in the current
  /// function and returns it.
  ///
  /// The corresponding pointer type of the given type will be constructed in
  /// this method for the variable itself.
  SpirvVariable *addFnVar(QualType valueType, SourceLocation,
                          llvm::StringRef name = "", bool isPrecise = false,
                          SpirvInstruction *init = nullptr);

  /// \brief Ends building of the current function. All basic blocks constructed
  /// from the beginning or after ending the previous function will be collected
  /// into this function.
  void endFunction();

  /// \brief Creates a SPIR-V basic block. On success, returns the <label-id>
  /// for the basic block. On failure, returns zero.
  SpirvBasicBlock *createBasicBlock(llvm::StringRef name = "");

  /// \brief Adds the basic block with the given label as a successor to the
  /// current basic block.
  void addSuccessor(SpirvBasicBlock *successorBB);

  /// \brief Sets the merge target to the given basic block.
  /// The caller must make sure the current basic block contains an
  /// OpSelectionMerge or OpLoopMerge instruction.
  void setMergeTarget(SpirvBasicBlock *mergeLabel);

  /// \brief Sets the continue target to the given basic block.
  /// The caller must make sure the current basic block contains an
  /// OpLoopMerge instruction.
  void setContinueTarget(SpirvBasicBlock *continueLabel);

  /// \brief Returns true if the current basic block inserting into is
  /// terminated.
  inline bool isCurrentBasicBlockTerminated() const {
    return insertPoint && insertPoint->hasTerminator();
  }

  /// \brief Sets insertion point to the given basic block.
  inline void setInsertPoint(SpirvBasicBlock *bb) { insertPoint = bb; }

  // === Instruction at the current Insertion Point ===

  /// \brief Creates a composite construct instruction with the given
  /// <result-type> and constituents and returns the pointer of the
  /// composite instruction.
  SpirvCompositeConstruct *
  createCompositeConstruct(QualType resultType,
                           llvm::ArrayRef<SpirvInstruction *> constituents,
                           SourceLocation loc);

  /// \brief Creates a composite extract instruction. The given composite is
  /// indexed using the given literal indexes to obtain the resulting element.
  /// Returns the instruction pointer for the extracted element.
  SpirvCompositeExtract *
  createCompositeExtract(QualType resultType, SpirvInstruction *composite,
                         llvm::ArrayRef<uint32_t> indexes, SourceLocation loc);

  /// \brief Creates a composite insert instruction. The given object will
  /// replace the component in the composite at the given indices. Returns the
  /// instruction pointer for the new composite.
  SpirvCompositeInsert *createCompositeInsert(QualType resultType,
                                              SpirvInstruction *composite,
                                              llvm::ArrayRef<uint32_t> indices,
                                              SpirvInstruction *object,
                                              SourceLocation loc);

  /// \brief Creates a vector shuffle instruction of selecting from the two
  /// vectors using selectors and returns the instruction pointer of the result
  /// vector.
  SpirvVectorShuffle *createVectorShuffle(QualType resultType,
                                          SpirvInstruction *vector1,
                                          SpirvInstruction *vector2,
                                          llvm::ArrayRef<uint32_t> selectors,
                                          SourceLocation loc);

  /// \brief Creates a load instruction loading the value of the given
  /// <result-type> from the given pointer. Returns the instruction pointer for
  /// the loaded value.
  SpirvLoad *createLoad(QualType resultType, SpirvInstruction *pointer,
                        SourceLocation loc);
  SpirvLoad *createLoad(const SpirvType *resultType, SpirvInstruction *pointer,
                        SourceLocation loc);

  /// \brief Creates a store instruction storing the given value into the given
  /// address.
  void createStore(SpirvInstruction *address, SpirvInstruction *value,
                   SourceLocation loc);

  /// \brief Creates a function call instruction and returns the instruction
  /// pointer for the return value.
  SpirvFunctionCall *
  createFunctionCall(QualType returnType, SpirvFunction *func,
                     llvm::ArrayRef<SpirvInstruction *> params,
                     SourceLocation loc);

  /// \brief Creates an access chain instruction to retrieve the element from
  /// the given base by walking through the given indexes. Returns the
  /// instruction pointer for the pointer to the element.
  /// Note: The given 'resultType' should be the underlying value type, not the
  /// pointer type. The type lowering pass automatically adds pointerness and
  /// proper storage class (based on the access base) to the result type.
  SpirvAccessChain *
  createAccessChain(QualType resultType, SpirvInstruction *base,
                    llvm::ArrayRef<SpirvInstruction *> indexes,
                    SourceLocation loc);

  /// \brief Creates a unary operation with the given SPIR-V opcode. Returns
  /// the instruction pointer for the result.
  SpirvUnaryOp *createUnaryOp(spv::Op op, QualType resultType,
                              SpirvInstruction *operand, SourceLocation loc);

  /// \brief Creates a binary operation with the given SPIR-V opcode. Returns
  /// the instruction pointer for the result.
  SpirvBinaryOp *createBinaryOp(spv::Op op, QualType resultType,
                                SpirvInstruction *lhs, SpirvInstruction *rhs,
                                SourceLocation loc);

  SpirvSpecConstantBinaryOp *createSpecConstantBinaryOp(spv::Op op,
                                                        QualType resultType,
                                                        SpirvInstruction *lhs,
                                                        SpirvInstruction *rhs,
                                                        SourceLocation loc);

  /// \brief Creates an operation with the given OpGroupNonUniform* SPIR-V
  /// opcode.
  SpirvNonUniformElect *createGroupNonUniformElect(spv::Op op,
                                                   QualType resultType,
                                                   spv::Scope execScope,
                                                   SourceLocation);
  SpirvNonUniformUnaryOp *createGroupNonUniformUnaryOp(
      SourceLocation, spv::Op op, QualType resultType, spv::Scope execScope,
      SpirvInstruction *operand,
      llvm::Optional<spv::GroupOperation> groupOp = llvm::None);
  SpirvNonUniformBinaryOp *createGroupNonUniformBinaryOp(
      spv::Op op, QualType resultType, spv::Scope execScope,
      SpirvInstruction *operand1, SpirvInstruction *operand2, SourceLocation);

  /// \brief Creates an atomic instruction with the given parameters and returns
  /// its pointer.
  SpirvAtomic *createAtomicOp(spv::Op opcode, QualType resultType,
                              SpirvInstruction *orignalValuePtr,
                              spv::Scope scope,
                              spv::MemorySemanticsMask memorySemantics,
                              SpirvInstruction *valueToOp, SourceLocation);
  SpirvAtomic *createAtomicCompareExchange(
      QualType resultType, SpirvInstruction *orignalValuePtr, spv::Scope scope,
      spv::MemorySemanticsMask equalMemorySemantics,
      spv::MemorySemanticsMask unequalMemorySemantics,
      SpirvInstruction *valueToOp, SpirvInstruction *comparator,
      SourceLocation);

  /// \brief Creates an OpSampledImage SPIR-V instruction with proper
  /// decorations for the given parameters.
  SpirvSampledImage *createSampledImage(QualType, SpirvInstruction *image,
                                        SpirvInstruction *sampler,
                                        SourceLocation);

  /// \brief Creates an OpImageTexelPointer SPIR-V instruction with the given
  /// parameters.
  SpirvImageTexelPointer *createImageTexelPointer(QualType resultType,
                                                  SpirvInstruction *image,
                                                  SpirvInstruction *coordinate,
                                                  SpirvInstruction *sample,
                                                  SourceLocation);

  /// \brief Creates SPIR-V instructions for sampling the given image.
  ///
  /// If compareVal is given a non-zero value, *Dref* variants of OpImageSample*
  /// will be generated.
  ///
  /// If lod or grad is given a non-zero value, *ExplicitLod variants of
  /// OpImageSample* will be generated; otherwise, *ImplicitLod variant will
  /// be generated.
  ///
  /// If bias, lod, grad, or minLod is given a non-zero value, an additional
  /// image operands, Bias, Lod, Grad, or MinLod will be attached to the current
  /// instruction, respectively. Panics if both lod and minLod are non-zero.
  ///
  /// If residencyCodeId is not zero, the sparse version of the instructions
  /// will be used, and the SPIR-V instruction for storing the resulting
  /// residency code will also be emitted.
  ///
  /// If isNonUniform is true, the sampled image will be decorated with
  /// NonUniformEXT.
  SpirvInstruction *
  createImageSample(QualType texelType, QualType imageType,
                    SpirvInstruction *image, SpirvInstruction *sampler,
                    SpirvInstruction *coordinate, SpirvInstruction *compareVal,
                    SpirvInstruction *bias, SpirvInstruction *lod,
                    std::pair<SpirvInstruction *, SpirvInstruction *> grad,
                    SpirvInstruction *constOffset, SpirvInstruction *varOffset,
                    SpirvInstruction *constOffsets, SpirvInstruction *sample,
                    SpirvInstruction *minLod, SpirvInstruction *residencyCodeId,
                    SourceLocation loc);

  /// \brief Creates SPIR-V instructions for reading a texel from an image. If
  /// doImageFetch is true, OpImageFetch is used. OpImageRead is used otherwise.
  /// OpImageFetch should be used for sampled images. OpImageRead should be used
  /// for images without a sampler.
  ///
  /// If residencyCodeId is not zero, the sparse version of the instructions
  /// will be used, and the SPIR-V instruction for storing the resulting
  /// residency code will also be emitted.
  SpirvInstruction *createImageFetchOrRead(
      bool doImageFetch, QualType texelType, QualType imageType,
      SpirvInstruction *image, SpirvInstruction *coordinate,
      SpirvInstruction *lod, SpirvInstruction *constOffset,
      SpirvInstruction *varOffset, SpirvInstruction *constOffsets,
      SpirvInstruction *sample, SpirvInstruction *residencyCode,
      SourceLocation loc);

  /// \brief Creates SPIR-V instructions for writing to the given image.
  void createImageWrite(QualType imageType, SpirvInstruction *image,
                        SpirvInstruction *coord, SpirvInstruction *texel,
                        SourceLocation loc);

  /// \brief Creates SPIR-V instructions for gathering the given image.
  ///
  /// If compareVal is given a non-null value, OpImageDrefGather or
  /// OpImageSparseDrefGather will be generated; otherwise, OpImageGather or
  /// OpImageSparseGather will be generated.
  /// If residencyCode is not null, the sparse version of the instructions
  /// will be used, and the SPIR-V instruction for storing the resulting
  /// residency code will also be emitted.
  /// If isNonUniform is true, the sampled image will be decorated with
  /// NonUniformEXT.
  SpirvInstruction *
  createImageGather(QualType texelType, QualType imageType,
                    SpirvInstruction *image, SpirvInstruction *sampler,
                    SpirvInstruction *coordinate, SpirvInstruction *component,
                    SpirvInstruction *compareVal, SpirvInstruction *constOffset,
                    SpirvInstruction *varOffset, SpirvInstruction *constOffsets,
                    SpirvInstruction *sample, SpirvInstruction *residencyCode,
                    SourceLocation);

  /// \brief Creates an OpImageSparseTexelsResident SPIR-V instruction for the
  /// given Resident Code and returns the instruction pointer.
  SpirvImageSparseTexelsResident *
  createImageSparseTexelsResident(SpirvInstruction *resident_code,
                                  SourceLocation);

  /// \brief Creates an image query instruction.
  /// The given 'lod' is used as the Lod argument in the case of
  /// OpImageQuerySizeLod, and it is used as the 'coordinate' parameter in the
  /// case of OpImageQueryLod.
  SpirvImageQuery *createImageQuery(spv::Op opcode, QualType resultType,
                                    SourceLocation loc, SpirvInstruction *image,
                                    SpirvInstruction *lod = nullptr);

  /// \brief Creates a select operation with the given values for true and false
  /// cases and returns the instruction pointer.
  SpirvSelect *createSelect(QualType resultType, SpirvInstruction *condition,
                            SpirvInstruction *trueValue,
                            SpirvInstruction *falseValue, SourceLocation);

  /// \brief Creates a switch statement for the given selector, default, and
  /// branches. Results in OpSelectionMerge followed by OpSwitch.
  void
  createSwitch(SpirvBasicBlock *mergeLabel, SpirvInstruction *selector,
               SpirvBasicBlock *defaultLabel,
               llvm::ArrayRef<std::pair<uint32_t, SpirvBasicBlock *>> target,
               SourceLocation);

  /// \brief Creates a fragment-shader discard via by emitting OpKill.
  void createKill(SourceLocation);

  /// \brief Creates an unconditional branch to the given target label.
  /// If mergeBB and continueBB are non-null, it creates an OpLoopMerge
  /// instruction followed by an unconditional branch to the given target label.
  void createBranch(
      SpirvBasicBlock *targetLabel, SourceLocation loc,
      SpirvBasicBlock *mergeBB = nullptr, SpirvBasicBlock *continueBB = nullptr,
      spv::LoopControlMask loopControl = spv::LoopControlMask::MaskNone);

  /// \brief Creates a conditional branch. An OpSelectionMerge instruction
  /// will be created if mergeLabel is not null and continueLabel is null.
  /// An OpLoopMerge instruction will also be created if both continueLabel
  /// and mergeLabel are not null. For other cases, mergeLabel and continueLabel
  /// will be ignored. If selection control mask and/or loop control mask are
  /// provided, they will be applied to the corresponding SPIR-V instruction.
  /// Otherwise, MaskNone will be used.
  void createConditionalBranch(
      SpirvInstruction *condition, SpirvBasicBlock *trueLabel,
      SpirvBasicBlock *falseLabel, SourceLocation loc,
      SpirvBasicBlock *mergeLabel = nullptr,
      SpirvBasicBlock *continueLabel = nullptr,
      spv::SelectionControlMask selectionControl =
          spv::SelectionControlMask::MaskNone,
      spv::LoopControlMask loopControl = spv::LoopControlMask::MaskNone);

  /// \brief Creates a return instruction.
  void createReturn(SourceLocation);
  /// \brief Creates a return value instruction.
  void createReturnValue(SpirvInstruction *value, SourceLocation);

  /// \brief Creates an OpExtInst instruction with the given instruction set,
  /// instruction number, and operands. Returns the resulting instruction
  /// pointer.
  SpirvInstruction *createExtInst(QualType resultType, SpirvExtInstImport *set,
                                  GLSLstd450 instId,
                                  llvm::ArrayRef<SpirvInstruction *> operands,
                                  SourceLocation);
  SpirvInstruction *createExtInst(const SpirvType *resultType,
                                  SpirvExtInstImport *set, GLSLstd450 instId,
                                  llvm::ArrayRef<SpirvInstruction *> operands,
                                  SourceLocation);

  /// \brief Creates an OpMemoryBarrier or OpControlBarrier instruction with the
  /// given flags. If execution scope (exec) is provided, an OpControlBarrier
  /// is created; otherwise an OpMemoryBarrier is created.
  void createBarrier(spv::Scope memoryScope,
                     spv::MemorySemanticsMask memorySemantics,
                     llvm::Optional<spv::Scope> exec, SourceLocation);

  /// \brief Creates an OpBitFieldInsert SPIR-V instruction for the given
  /// arguments.
  SpirvBitFieldInsert *
  createBitFieldInsert(QualType resultType, SpirvInstruction *base,
                       SpirvInstruction *insert, SpirvInstruction *offset,
                       SpirvInstruction *count, SourceLocation);

  /// \brief Creates an OpBitFieldUExtract or OpBitFieldSExtract SPIR-V
  /// instruction for the given arguments.
  SpirvBitFieldExtract *createBitFieldExtract(QualType resultType,
                                              SpirvInstruction *base,
                                              SpirvInstruction *offset,
                                              SpirvInstruction *count,
                                              bool isSigned, SourceLocation);

  /// \brief Creates an OpEmitVertex instruction.
  void createEmitVertex(SourceLocation);

  /// \brief Creates an OpEndPrimitive instruction.
  void createEndPrimitive(SourceLocation);

  /// \brief Creates an OpArrayLength instruction.
  SpirvArrayLength *createArrayLength(QualType resultType, SourceLocation loc,
                                      SpirvInstruction *structure,
                                      uint32_t arrayMember);

  /// \brief Creates SPIR-V instructions for NV raytracing ops.
  SpirvInstruction *
  createRayTracingOpsNV(spv::Op opcode, QualType resultType,
                        llvm::ArrayRef<SpirvInstruction *> operands,
                        SourceLocation loc);

  // === SPIR-V Module Structure ===
  inline void setMemoryModel(spv::AddressingModel, spv::MemoryModel);

  /// \brief Adds an entry point for the module under construction. We only
  /// support a single entry point per module for now.
  inline void addEntryPoint(spv::ExecutionModel em, SpirvFunction *target,
                            std::string targetName,
                            llvm::ArrayRef<SpirvVariable *> interfaces);

  /// \brief Sets the shader model version, source file name, and source file
  /// content. Returns the SpirvString instruction of the file name.
  inline SpirvString *setDebugSource(uint32_t major, uint32_t minor,
                                     const std::vector<llvm::StringRef> &name,
                                     llvm::StringRef content);

  /// \brief Adds an execution mode to the module under construction.
  inline void addExecutionMode(SpirvFunction *entryPoint, spv::ExecutionMode em,
                               llvm::ArrayRef<uint32_t> params, SourceLocation);

  /// \brief Adds an OpModuleProcessed instruction to the module under
  /// construction.
  void addModuleProcessed(llvm::StringRef process);

  /// \brief If not added already, adds an OpExtInstImport (import of extended
  /// instruction set) of the GLSL instruction set. Returns the  the imported
  /// GLSL instruction set.
  SpirvExtInstImport *getGLSLExtInstSet();

  /// \brief Adds a stage input/ouput variable whose value is of the given type.
  ///
  /// Note: the corresponding pointer type of the given type will not be
  /// constructed in this method.
  SpirvVariable *addStageIOVar(QualType type, spv::StorageClass storageClass,
                               std::string name, bool isPrecise,
                               SourceLocation loc);

  /// \brief Adds a stage builtin variable whose value is of the given type.
  ///
  /// Note: The corresponding pointer type of the given type will not be
  /// constructed in this method.
  SpirvVariable *addStageBuiltinVar(QualType type,
                                    spv::StorageClass storageClass,
                                    spv::BuiltIn, bool isPrecise,
                                    SourceLocation loc);

  /// \brief Adds a module variable. This variable should not have the Function
  /// storage class.
  ///
  /// Note: The corresponding pointer type of the given type will not be
  /// constructed in this method.
  SpirvVariable *
  addModuleVar(QualType valueType, spv::StorageClass storageClass,
               bool isPrecise, llvm::StringRef name = "",
               llvm::Optional<SpirvInstruction *> init = llvm::None,
               SourceLocation loc = {});
  SpirvVariable *
  addModuleVar(const SpirvType *valueType, spv::StorageClass storageClass,
               bool isPrecise, llvm::StringRef name = "",
               llvm::Optional<SpirvInstruction *> init = llvm::None,
               SourceLocation loc = {});

  /// \brief Decorates the given target with the given location.
  void decorateLocation(SpirvInstruction *target, uint32_t location);

  /// \brief Decorates the given target with the given index.
  void decorateIndex(SpirvInstruction *target, uint32_t index, SourceLocation);

  /// \brief Decorates the given target with the given descriptor set and
  /// binding number.
  void decorateDSetBinding(SpirvVariable *target, uint32_t setNumber,
                           uint32_t bindingNumber);

  /// \brief Decorates the given target with the given SpecId.
  void decorateSpecId(SpirvInstruction *target, uint32_t specId,
                      SourceLocation);

  /// \brief Decorates the given target with the given input attchment index
  /// number.
  void decorateInputAttachmentIndex(SpirvInstruction *target,
                                    uint32_t indexNumber, SourceLocation);

  /// \brief Decorates the given main buffer with the given counter buffer.
  void decorateCounterBuffer(SpirvInstruction *mainBuffer,
                             SpirvInstruction *counterBuffer, SourceLocation);

  /// \brief Decorates the given target with the given HLSL semantic string.
  void decorateHlslSemantic(SpirvInstruction *target, llvm::StringRef semantic,
                            llvm::Optional<uint32_t> memberIdx = llvm::None);

  /// \brief Decorates the given target with centroid
  void decorateCentroid(SpirvInstruction *target, SourceLocation);

  /// \brief Decorates the given target with flat
  void decorateFlat(SpirvInstruction *target, SourceLocation);

  /// \brief Decorates the given target with noperspective
  void decorateNoPerspective(SpirvInstruction *target, SourceLocation);

  /// \brief Decorates the given target with sample
  void decorateSample(SpirvInstruction *target, SourceLocation);

  /// \brief Decorates the given target with patch
  void decoratePatch(SpirvInstruction *target, SourceLocation);

  /// \brief Decorates the given target with NoContraction
  void decorateNoContraction(SpirvInstruction *target, SourceLocation);

  /// \brief Decorates the given target with PerPrimitiveNV
  void decoratePerPrimitiveNV(SpirvInstruction *target, SourceLocation);

  /// \brief Decorates the given target with PerTaskNV
  void decoratePerTaskNV(SpirvInstruction *target, uint32_t offset,
                         SourceLocation);

  /// --- Constants ---
  /// Each of these methods can acquire a unique constant from the SpirvContext,
  /// and add the context to the list of constants in the module.
  SpirvConstant *getConstantInt(QualType type, llvm::APInt value,
                                bool specConst = false);
  SpirvConstant *getConstantFloat(QualType type, llvm::APFloat value,
                                  bool specConst = false);
  SpirvConstant *getConstantBool(bool value, bool specConst = false);
  SpirvConstant *
  getConstantComposite(QualType compositeType,
                       llvm::ArrayRef<SpirvConstant *> constituents,
                       bool specConst = false);
  SpirvConstant *getConstantNull(QualType);

public:
  std::vector<uint32_t> takeModule();

protected:
  /// Only friend classes are allowed to add capability/extension to the module
  /// under construction.

  /// \brief Adds the given capability to the module under construction due to
  /// the feature used at the given source location.
  inline void requireCapability(spv::Capability, SourceLocation loc = {});

  /// \brief Adds an extension to the module under construction for translating
  /// the given target at the given source location.
  inline void requireExtension(llvm::StringRef extension, SourceLocation);

private:
  /// \brief Returns the composed ImageOperandsMask from non-zero parameters
  /// and pushes non-zero parameters to *orderedParams in the expected order.
  spv::ImageOperandsMask composeImageOperandsMask(
      SpirvInstruction *bias, SpirvInstruction *lod,
      const std::pair<SpirvInstruction *, SpirvInstruction *> &grad,
      SpirvInstruction *constOffset, SpirvInstruction *varOffset,
      SpirvInstruction *constOffsets, SpirvInstruction *sample,
      SpirvInstruction *minLod);

private:
  ASTContext &astContext;
  SpirvContext &context; ///< From which we allocate various SPIR-V object

  SpirvModule *module;          ///< The current module being built
  SpirvFunction *function;      ///< The current function being built
  SpirvBasicBlock *insertPoint; ///< The current basic block being built

  /// \brief List of basic blocks being built.
  ///
  /// We need a vector here to remember the order of insertion. Order matters
  /// here since, for example, we'll know for sure the first basic block is
  /// the entry block.
  std::vector<SpirvBasicBlock *> basicBlocks;

  const SpirvCodeGenOptions &spirvOptions; ///< Command line options.

  /// A struct containing information regarding a builtin variable.
  struct BuiltInVarInfo {
    BuiltInVarInfo(spv::StorageClass s, spv::BuiltIn b, SpirvVariable *v)
        : sc(s), builtIn(b), variable(v) {}
    spv::StorageClass sc;
    spv::BuiltIn builtIn;
    SpirvVariable *variable;
  };
  /// Used as caches for all created builtin variables to avoid duplication.
  llvm::SmallVector<BuiltInVarInfo, 16> builtinVars;
};

void SpirvBuilder::requireCapability(spv::Capability cap, SourceLocation loc) {
  module->addCapability(new (context) SpirvCapability(loc, cap));
}

void SpirvBuilder::requireExtension(llvm::StringRef ext, SourceLocation loc) {
  module->addExtension(new (context) SpirvExtension(loc, ext));
}

void SpirvBuilder::setMemoryModel(spv::AddressingModel addrModel,
                                  spv::MemoryModel memModel) {
  module->setMemoryModel(new (context) SpirvMemoryModel(addrModel, memModel));
}

void SpirvBuilder::addEntryPoint(spv::ExecutionModel em, SpirvFunction *target,
                                 std::string targetName,
                                 llvm::ArrayRef<SpirvVariable *> interfaces) {
  module->addEntryPoint(new (context) SpirvEntryPoint(
      target->getSourceLocation(), em, target, targetName, interfaces));
}

SpirvString *
SpirvBuilder::setDebugSource(uint32_t major, uint32_t minor,
                             const std::vector<llvm::StringRef> &fileNames,
                             llvm::StringRef content) {
  uint32_t version = 100 * major + 10 * minor;
  SpirvSource *mainSource = nullptr;
  for (const auto &name : fileNames) {
    SpirvString *fileString =
        name.empty() ? nullptr
                     : new (context) SpirvString(/*SourceLocation*/ {}, name);
    SpirvSource *debugSource = new (context)
        SpirvSource(/*SourceLocation*/ {}, spv::SourceLanguage::HLSL, version,
                    fileString, content);
    module->addDebugSource(debugSource);
    if (!mainSource)
      mainSource = debugSource;
  }

  // If mainSource is nullptr, fileNames is empty and no input file is
  // specified. We must create a SpirvSource for OpSource HLSL <version>.
  if (!mainSource) {
    mainSource = new (context)
        SpirvSource(/*SourceLocation*/ {}, spv::SourceLanguage::HLSL, version,
                    nullptr, content);
    module->addDebugSource(mainSource);
  }
  return mainSource->getFile();
}

void SpirvBuilder::addExecutionMode(SpirvFunction *entryPoint,
                                    spv::ExecutionMode em,
                                    llvm::ArrayRef<uint32_t> params,
                                    SourceLocation loc) {
  module->addExecutionMode(
      new (context) SpirvExecutionMode(loc, entryPoint, em, params, false));
}

} // end namespace spirv
} // end namespace clang

#endif // LLVM_CLANG_SPIRV_SPIRVBUILDER_H
