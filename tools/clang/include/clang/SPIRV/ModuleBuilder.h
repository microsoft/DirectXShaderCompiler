//===-- ModuleBuilder.h - SPIR-V builder ----------------------*- C++ -*---===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_CLANG_SPIRV_MODULEBUILDER_H
#define LLVM_CLANG_SPIRV_MODULEBUILDER_H

#include <memory>
#include <string>
#include <vector>

#include "clang/AST/Type.h"
#include "clang/SPIRV/FeatureManager.h"
#include "clang/SPIRV/InstBuilder.h"
#include "clang/SPIRV/SPIRVContext.h"
#include "clang/SPIRV/Structure.h"
#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/StringRef.h"

namespace clang {
namespace spirv {

/// \brief SPIR-V module builder.
///
/// This class exports API for constructing SPIR-V binary interactively.
/// At any time, there can only exist at most one function under building;
/// but there can exist multiple basic blocks under construction.
///
/// Call `takeModule()` to get the SPIR-V words after finishing building the
/// module.
class ModuleBuilder {
public:
  /// \brief Constructs a ModuleBuilder with the given SPIR-V context.
  ModuleBuilder(SPIRVContext *, FeatureManager *features, bool enableReflect);

  /// \brief Returns the associated SPIRVContext.
  inline SPIRVContext *getSPIRVContext();

  /// \brief Takes the SPIR-V module under building. This will consume the
  /// module under construction.
  std::vector<uint32_t> takeModule();

  // === Function and Basic Block ===

  /// \brief Begins building a SPIR-V function. Returns the <result-id> for the
  /// function on success. Returns zero on failure.
  ///
  /// If the resultId supplied is not zero, the created function will use it;
  /// otherwise, an unused <result-id> will be assgined.
  /// At any time, there can only exist at most one function under building.
  uint32_t beginFunction(uint32_t funcType, uint32_t returnType,
                         llvm::StringRef name = "", uint32_t resultId = 0);

  /// \brief Registers a function parameter of the given pointer type in the
  /// current function and returns its <result-id>.
  uint32_t addFnParam(uint32_t ptrType, llvm::StringRef name = "");

  /// \brief Creates a local variable of the given type in the current
  /// function and returns its <result-id>.
  ///
  /// The corresponding pointer type of the given type will be constructed in
  /// this method for the variable itself.
  uint32_t addFnVar(uint32_t valueType, llvm::StringRef name = "",
                    llvm::Optional<uint32_t> init = llvm::None);

  /// \brief Ends building of the current function. Returns true of success,
  /// false on failure. All basic blocks constructed from the beginning or
  /// after ending the previous function will be collected into this function.
  bool endFunction();

  /// \brief Creates a SPIR-V basic block. On success, returns the <label-id>
  /// for the basic block. On failure, returns zero.
  uint32_t createBasicBlock(llvm::StringRef name = "");

  /// \brief Adds the basic block with the given label as a successor to the
  /// current basic block.
  void addSuccessor(uint32_t successorLabel);

  /// \brief Sets the merge target to the basic block with the given <label-id>.
  /// The caller must make sure the current basic block contains an
  /// OpSelectionMerge or OpLoopMerge instruction.
  void setMergeTarget(uint32_t mergeLabel);

  /// \brief Sets the continue target to the basic block with the given
  /// <label-id>. The caller must make sure the current basic block contains an
  /// OpLoopMerge instruction.
  void setContinueTarget(uint32_t continueLabel);

  /// \brief Returns true if the current basic block inserting into is
  /// terminated.
  inline bool isCurrentBasicBlockTerminated() const;

  /// \brief Sets insertion point to the basic block with the given <label-id>.
  void setInsertPoint(uint32_t labelId);

  // === Instruction at the current Insertion Point ===

  /// \brief Creates a composite construct instruction with the given
  /// <result-type> and constituents and returns the <result-id> for the
  /// composite.
  uint32_t createCompositeConstruct(uint32_t resultType,
                                    llvm::ArrayRef<uint32_t> constituents);

  /// \brief Creates a composite extract instruction. The given composite is
  /// indexed using the given literal indexes to obtain the resulting element.
  /// Returns the <result-id> for the extracted element.
  uint32_t createCompositeExtract(uint32_t resultType, uint32_t composite,
                                  llvm::ArrayRef<uint32_t> indexes);

  /// \brief Creates a composite insert instruction. The given object will
  /// replace the component in the composite at the given indices. Returns the
  /// <result-id> for the new composite.
  uint32_t createCompositeInsert(uint32_t resultType, uint32_t composite,
                                 llvm::ArrayRef<uint32_t> indices,
                                 uint32_t object);

  /// \brief Creates a vector shuffle instruction of selecting from the two
  /// vectors using selectors and returns the <result-id> of the result vector.
  uint32_t createVectorShuffle(uint32_t resultType, uint32_t vector1,
                               uint32_t vector2,
                               llvm::ArrayRef<uint32_t> selectors);

  /// \brief Creates a load instruction loading the value of the given
  /// <result-type> from the given pointer. Returns the <result-id> for the
  /// loaded value.
  uint32_t createLoad(uint32_t resultType, uint32_t pointer);
  /// \brief Creates a store instruction storing the given value into the given
  /// address.
  void createStore(uint32_t address, uint32_t value);

  /// \brief Creates a function call instruction and returns the <result-id> for
  /// the return value.
  uint32_t createFunctionCall(uint32_t returnType, uint32_t functionId,
                              llvm::ArrayRef<uint32_t> params);

  /// \brief Creates an access chain instruction to retrieve the element from
  /// the given base by walking through the given indexes. Returns the
  /// <result-id> for the pointer to the element.
  uint32_t createAccessChain(uint32_t resultType, uint32_t base,
                             llvm::ArrayRef<uint32_t> indexes);

  /// \brief Creates a unary operation with the given SPIR-V opcode. Returns
  /// the <result-id> for the result.
  uint32_t createUnaryOp(spv::Op op, uint32_t resultType, uint32_t operand);

  /// \brief Creates a binary operation with the given SPIR-V opcode. Returns
  /// the <result-id> for the result.
  uint32_t createBinaryOp(spv::Op op, uint32_t resultType, uint32_t lhs,
                          uint32_t rhs);
  uint32_t createSpecConstantBinaryOp(spv::Op op, uint32_t resultType,
                                      uint32_t lhs, uint32_t rhs);

  /// \brief Creates an operation with the given OpGroupNonUniform* SPIR-V
  /// opcode. Returns the <result-id> for the result.
  uint32_t createGroupNonUniformOp(spv::Op op, uint32_t resultType,
                                   uint32_t execScope);
  uint32_t createGroupNonUniformUnaryOp(
      spv::Op op, uint32_t resultType, uint32_t execScope, uint32_t operand,
      llvm::Optional<spv::GroupOperation> groupOp = llvm::None);
  uint32_t createGroupNonUniformBinaryOp(spv::Op op, uint32_t resultType,
                                         uint32_t execScope, uint32_t operand1,
                                         uint32_t operand2);

  /// \brief Creates an atomic instruction with the given parameters.
  /// Returns the <result-id> for the result.
  uint32_t createAtomicOp(spv::Op opcode, uint32_t resultType,
                          uint32_t orignalValuePtr, uint32_t scopeId,
                          uint32_t memorySemanticsId, uint32_t valueToOp);
  uint32_t createAtomicCompareExchange(uint32_t resultType,
                                       uint32_t orignalValuePtr,
                                       uint32_t scopeId,
                                       uint32_t equalMemorySemanticsId,
                                       uint32_t unequalMemorySemanticsId,
                                       uint32_t valueToOp, uint32_t comparator);

  /// \brief Creates an OpImageTexelPointer SPIR-V instruction with the given
  /// parameters.
  uint32_t createImageTexelPointer(uint32_t resultType, uint32_t imageId,
                                   uint32_t coordinate, uint32_t sample);

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
  uint32_t createImageSample(uint32_t texelType, uint32_t imageType,
                             uint32_t image, uint32_t sampler,
                             bool isNonUniform, uint32_t coordinate,
                             uint32_t compareVal, uint32_t bias, uint32_t lod,
                             std::pair<uint32_t, uint32_t> grad,
                             uint32_t constOffset, uint32_t varOffset,
                             uint32_t constOffsets, uint32_t sample,
                             uint32_t minLod, uint32_t residencyCodeId);

  /// \brief Creates SPIR-V instructions for reading a texel from an image. If
  /// doImageFetch is true, OpImageFetch is used. OpImageRead is used otherwise.
  /// OpImageFetch should be used for sampled images. OpImageRead should be used
  /// for images without a sampler.
  ///
  /// If residencyCodeId is not zero, the sparse version of the instructions
  /// will be used, and the SPIR-V instruction for storing the resulting
  /// residency code will also be emitted.
  uint32_t createImageFetchOrRead(bool doImageFetch, uint32_t texelType,
                                  QualType imageType, uint32_t image,
                                  uint32_t coordinate, uint32_t lod,
                                  uint32_t constOffset, uint32_t varOffset,
                                  uint32_t constOffsets, uint32_t sample,
                                  uint32_t residencyCodeId);

  /// \brief Creates SPIR-V instructions for writing to the given image.
  void createImageWrite(QualType imageType, uint32_t imageId, uint32_t coordId,
                        uint32_t texelId);

  /// \brief Creates SPIR-V instructions for gathering the given image.
  ///
  /// If compareVal is given a non-zero value, OpImageDrefGather or
  /// OpImageSparseDrefGather will be generated; otherwise, OpImageGather or
  /// OpImageSparseGather will be generated.
  /// If residencyCodeId is not zero, the sparse version of the instructions
  /// will be used, and the SPIR-V instruction for storing the resulting
  /// residency code will also be emitted.
  /// If isNonUniform is true, the sampled image will be decorated with
  /// NonUniformEXT.
  uint32_t createImageGather(uint32_t texelType, uint32_t imageType,
                             uint32_t image, uint32_t sampler,
                             bool isNonUniform, uint32_t coordinate,
                             uint32_t component, uint32_t compareVal,
                             uint32_t constOffset, uint32_t varOffset,
                             uint32_t constOffsets, uint32_t sample,
                             uint32_t residencyCodeId);

  /// \brief Creates an OpImageSparseTexelsResident SPIR-V instruction for the
  /// given Resident Code and returns the <result-id> of the instruction.
  uint32_t createImageSparseTexelsResident(uint32_t resident_code);

  /// \brief Creates a select operation with the given values for true and false
  /// cases and returns the <result-id> for the result.
  uint32_t createSelect(uint32_t resultType, uint32_t condition,
                        uint32_t trueValue, uint32_t falseValue);

  /// \brief Creates a switch statement for the given selector, default, and
  /// branches. Results in OpSelectionMerge followed by OpSwitch.
  void createSwitch(uint32_t mergeLabel, uint32_t selector,
                    uint32_t defaultLabel,
                    llvm::ArrayRef<std::pair<uint32_t, uint32_t>> target);

  /// \brief Creates a fragment-shader discard via by emitting OpKill.
  void createKill();

  /// \brief Creates an unconditional branch to the given target label.
  /// If mergeBB and continueBB are non-zero, it creates an OpLoopMerge
  /// instruction followed by an unconditional branch to the given target label.
  void createBranch(
      uint32_t targetLabel, uint32_t mergeBB = 0, uint32_t continueBB = 0,
      spv::LoopControlMask loopControl = spv::LoopControlMask::MaskNone);

  /// \brief Creates a conditional branch. An OpSelectionMerge instruction
  /// will be created if mergeLabel is not 0 and continueLabel is 0.
  /// An OpLoopMerge instruction will also be created if both continueLabel
  /// and mergeLabel are not 0. For other cases, mergeLabel and continueLabel
  /// will be ignored. If selection control mask and/or loop control mask are
  /// provided, they will be applied to the corresponding SPIR-V instruction.
  /// Otherwise, MaskNone will be used.
  void createConditionalBranch(
      uint32_t condition, uint32_t trueLabel, uint32_t falseLabel,
      uint32_t mergeLabel = 0, uint32_t continueLabel = 0,
      spv::SelectionControlMask selectionControl =
          spv::SelectionControlMask::MaskNone,
      spv::LoopControlMask loopControl = spv::LoopControlMask::MaskNone);

  /// \brief Creates a return instruction.
  void createReturn();
  /// \brief Creates a return value instruction.
  void createReturnValue(uint32_t value);

  /// \brief Creates an OpExtInst instruction with the given instruction set id,
  /// instruction number, and operands. Returns the <result-id> of the
  /// instruction.
  uint32_t createExtInst(uint32_t resultType, uint32_t setId, uint32_t instId,
                         llvm::ArrayRef<uint32_t> operands);

  /// \brief Creates an OpMemoryBarrier or OpControlBarrier instruction with the
  /// given flags. If execution scope id (exec) is non-zero, an OpControlBarrier
  /// is created; otherwise an OpMemoryBarrier is created.
  void createBarrier(uint32_t exec, uint32_t memory, uint32_t semantics);

  /// \brief Creates an OpBitFieldInsert SPIR-V instruction for the given
  /// arguments.
  uint32_t createBitFieldInsert(uint32_t resultType, uint32_t base,
                                uint32_t insert, uint32_t offset,
                                uint32_t count);

  /// \brief Creates an OpBitFieldUExtract or OpBitFieldSExtract SPIR-V
  /// instruction for the given arguments.
  uint32_t createBitFieldExtract(uint32_t resultType, uint32_t base,
                                 uint32_t offset, uint32_t count,
                                 bool isSigned);

  /// \brief Creates an OpEmitVertex instruction.
  void createEmitVertex();

  /// \brief Creates an OpEndPrimitive instruction.
  void createEndPrimitive();

  // === SPIR-V Module Structure ===

  inline void useSpirv1p3();

  inline void requireCapability(spv::Capability);

  inline void setAddressingModel(spv::AddressingModel);
  inline void setMemoryModel(spv::MemoryModel);

  /// \brief Adds an entry point for the module under construction. We only
  /// support a single entry point per module for now.
  inline void addEntryPoint(spv::ExecutionModel em, uint32_t targetId,
                            std::string targetName,
                            llvm::ArrayRef<uint32_t> interfaces);

  inline void setShaderModelVersion(uint32_t major, uint32_t minor);

  /// \brief Sets the source file name and the <result-id> for the OpString for
  /// the file name.
  inline void setSourceFileName(uint32_t id, std::string name);

  /// \brief Adds an execution mode to the module under construction.
  void addExecutionMode(uint32_t entryPointId, spv::ExecutionMode em,
                        llvm::ArrayRef<uint32_t> params);

  /// \brief Adds an extension to the module under construction for translating
  /// the given target at the given source location.
  void addExtension(Extension, llvm::StringRef target, SourceLocation);

  /// \brief If not added already, adds an OpExtInstImport (import of extended
  /// instruction set) of the GLSL instruction set. Returns the <result-id> for
  /// the imported GLSL instruction set.
  uint32_t getGLSLExtInstSet();

  /// \brief Adds a stage input/ouput variable whose value is of the given type.
  ///
  /// The corresponding pointer type of the given type will be constructed in
  /// this method for the variable itself.
  uint32_t addStageIOVar(uint32_t type, spv::StorageClass storageClass,
                         std::string name);

  /// \brief Adds a stage builtin variable whose value is of the given type.
  ///
  /// The corresponding pointer type of the given type will be constructed in
  /// this method for the variable itself.
  uint32_t addStageBuiltinVar(uint32_t type, spv::StorageClass storageClass,
                              spv::BuiltIn);

  /// \brief Adds a module variable. This variable should not have the Function
  /// storage class.
  ///
  /// The corresponding pointer type of the given type will be constructed in
  /// this method for the variable itself.
  uint32_t addModuleVar(uint32_t valueType, spv::StorageClass storageClass,
                        llvm::StringRef name = "",
                        llvm::Optional<uint32_t> init = llvm::None);

  /// \brief Decorates the given target <result-id> with the given location.
  void decorateLocation(uint32_t targetId, uint32_t location);

  /// \brief Decorates the given target <result-id> with the given index.
  void decorateIndex(uint32_t targetId, uint32_t index);

  /// \brief Decorates the given target <result-id> with the given descriptor
  /// set and binding number.
  void decorateDSetBinding(uint32_t targetId, uint32_t setNumber,
                           uint32_t bindingNumber);

  /// \brief Decorates the given target <result-id> with the given SpecId.
  void decorateSpecId(uint32_t targetId, uint32_t specId);

  /// \brief Decorates the given target <result-id> with the given input
  /// attchment index number.
  void decorateInputAttachmentIndex(uint32_t targetId, uint32_t indexNumber);

  /// \brief Decorates the given main buffer with the given counter buffer.
  void decorateCounterBufferId(uint32_t mainBufferId, uint32_t counterBufferId);

  /// \brief Decorates the given target <result-id> with the given HLSL semantic
  /// string.
  void decorateHlslSemantic(uint32_t targetId, llvm::StringRef semantic,
                            llvm::Optional<uint32_t> memberIdx = llvm::None);

  /// \brief Decorates the given target <result-id> with centroid
  void decorateCentroid(uint32_t targetId);

  /// \brief Decorates the given target <result-id> with flat
  void decorateFlat(uint32_t targetId);

  /// \brief Decorates the given target <result-id> with noperspective
  void decorateNoPerspective(uint32_t targetId);

  /// \brief Decorates the given target <result-id> with sample
  void decorateSample(uint32_t targetId);

  /// \brief Decorates the given target <result-id> with block
  void decorateBlock(uint32_t targetId);

  /// \brief Decorates the given target <result-id> with relaxedprecision
  void decorateRelaxedPrecision(uint32_t targetId);

  /// \brief Decorates the given target <result-id> with patch
  void decoratePatch(uint32_t targetId);

  /// \brief Decorates the given target <result-id> with nonuniformEXT
  void decorateNonUniformEXT(uint32_t targetId);

  // === Type ===

  uint32_t getVoidType();
  uint32_t getBoolType();
  uint32_t getInt16Type();
  uint32_t getInt32Type();
  uint32_t getInt64Type();
  uint32_t getUint16Type();
  uint32_t getUint32Type();
  uint32_t getUint64Type();
  uint32_t getFloat16Type();
  uint32_t getFloat32Type();
  uint32_t getFloat64Type();
  uint32_t getVecType(uint32_t elemType, uint32_t elemCount);
  uint32_t getMatType(QualType elemType, uint32_t colType, uint32_t colCount,
                      Type::DecorationSet decorations = {});
  uint32_t getPointerType(uint32_t pointeeType, spv::StorageClass);
  uint32_t getStructType(llvm::ArrayRef<uint32_t> fieldTypes,
                         llvm::StringRef structName = "",
                         llvm::ArrayRef<llvm::StringRef> fieldNames = {},
                         Type::DecorationSet decorations = {});
  uint32_t getArrayType(uint32_t elemType, uint32_t count,
                        Type::DecorationSet decorations = {});
  uint32_t getRuntimeArrayType(uint32_t elemType,
                               Type::DecorationSet decorations = {});
  uint32_t getFunctionType(uint32_t returnType,
                           llvm::ArrayRef<uint32_t> paramTypes);
  uint32_t getImageType(uint32_t sampledType, spv::Dim, uint32_t depth,
                        bool isArray, uint32_t ms = 0, uint32_t sampled = 1,
                        spv::ImageFormat format = spv::ImageFormat::Unknown);
  uint32_t getSamplerType();
  uint32_t getSampledImageType(uint32_t imageType);
  uint32_t getByteAddressBufferType(bool isRW);

  /// \brief Returns a struct type with 2 members. The first member is an
  /// unsigned integer type which can hold the 'Residency Code'. The second
  /// member will be of the given type.
  uint32_t getSparseResidencyStructType(uint32_t type);

  // === Constant ===
  uint32_t getConstantBool(bool value, bool isSpecConst = false);
  uint32_t getConstantInt16(int16_t value);
  uint32_t getConstantInt32(int32_t value, bool isSpecConst = false);
  uint32_t getConstantInt64(int64_t value);
  uint32_t getConstantUint16(uint16_t value);
  uint32_t getConstantUint32(uint32_t value, bool isSpecConst = false);
  uint32_t getConstantUint64(uint64_t value);
  uint32_t getConstantFloat16(int16_t value);
  uint32_t getConstantFloat32(float value, bool isSpecConst = false);
  uint32_t getConstantFloat64(double value);
  uint32_t getConstantComposite(uint32_t typeId,
                                llvm::ArrayRef<uint32_t> constituents);
  uint32_t getConstantNull(uint32_t type);

private:
  /// \brief Map from basic blocks' <label-id> to their structured
  /// representation.
  ///
  /// We need MapVector here to remember the order of insertion. Order matters
  /// here since, for example, we'll know for sure the first basic block is the
  /// entry block.
  using OrderedBasicBlockMap =
      llvm::MapVector<uint32_t, std::unique_ptr<BasicBlock>>;

  /// \brief Returns the basic block with the given <label-id>.
  BasicBlock *getBasicBlock(uint32_t label);

  /// \brief Returns the composed ImageOperandsMask from non-zero parameters
  /// and pushes non-zero parameters to *orderedParams in the expected order.
  spv::ImageOperandsMask composeImageOperandsMask(
      uint32_t bias, uint32_t lod, const std::pair<uint32_t, uint32_t> &grad,
      uint32_t constOffset, uint32_t varOffset, uint32_t constOffsets,
      uint32_t sample, uint32_t minLod,
      llvm::SmallVectorImpl<uint32_t> *orderedParams);

  SPIRVContext &theContext;       ///< The SPIR-V context.
  FeatureManager *featureManager; ///< SPIR-V version/extension manager.
  const bool allowReflect;        ///< Whether allow reflect instructions.

  SPIRVModule theModule;                 ///< The module under building.
  std::unique_ptr<Function> theFunction; ///< The function under building.
  OrderedBasicBlockMap basicBlocks;      ///< The basic blocks under building.
  BasicBlock *insertPoint;               ///< The current insertion point.

  /// An InstBuilder associated with the current ModuleBuilder.
  /// It can be used to contruct instructions on the fly.
  /// The constructed instruction will appear in constructSite.
  InstBuilder instBuilder;
  std::vector<uint32_t> constructSite; ///< InstBuilder construction site.
  uint32_t glslExtSetId; ///< The <result-id> of GLSL extended instruction set.
};

SPIRVContext *ModuleBuilder::getSPIRVContext() { return &theContext; }

bool ModuleBuilder::isCurrentBasicBlockTerminated() const {
  return insertPoint && insertPoint->isTerminated();
}

void ModuleBuilder::setAddressingModel(spv::AddressingModel am) {
  theModule.setAddressingModel(am);
}

void ModuleBuilder::setMemoryModel(spv::MemoryModel mm) {
  theModule.setMemoryModel(mm);
}

void ModuleBuilder::requireCapability(spv::Capability cap) {
  if (cap != spv::Capability::Max)
    theModule.addCapability(cap);
}

void ModuleBuilder::addEntryPoint(spv::ExecutionModel em, uint32_t targetId,
                                  std::string targetName,
                                  llvm::ArrayRef<uint32_t> interfaces) {
  theModule.addEntryPoint(em, targetId, std::move(targetName), interfaces);
}

void ModuleBuilder::setShaderModelVersion(uint32_t major, uint32_t minor) {
  theModule.setShaderModelVersion(major * 100 + minor * 10);
}

void ModuleBuilder::setSourceFileName(uint32_t id, std::string name) {
  theModule.setSourceFileName(id, std::move(name));
}

} // end namespace spirv
} // end namespace clang

#endif
