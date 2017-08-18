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
  explicit ModuleBuilder(SPIRVContext *);

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

  /// \brief Creates SPIR-V instructions for sampling the given image.
  ///
  /// If lod or grad is given a non-zero value, *ExplicitLod variants of
  /// OpImageSample* will be generated; otherwise, *ImplicitLod variant will
  /// be generated.
  ///
  /// If bias, lod, or grad is given a non-zero value, an additional image
  /// operands, Bias, Lod, or Grad, will be attached to the current instruction,
  /// respectively.
  uint32_t createImageSample(uint32_t texelType, uint32_t imageType,
                             uint32_t image, uint32_t sampler,
                             uint32_t coordinate, uint32_t bias, uint32_t lod,
                             std::pair<uint32_t, uint32_t> grad,
                             uint32_t offset);

  /// \brief Creates SPIR-V instructions for fetching the given image.
  uint32_t createImageFetch(uint32_t texelType, uint32_t image,
                            uint32_t coordinate, uint32_t lod, uint32_t offset);

  /// \brief Creates SPIR-V instructions for sampling the given image.
  uint32_t createImageGather(uint32_t texelType, uint32_t imageType,
                             uint32_t image, uint32_t sampler,
                             uint32_t coordinate, uint32_t component,
                             uint32_t offset);

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

  // === SPIR-V Module Structure ===

  inline void requireCapability(spv::Capability);

  inline void setAddressingModel(spv::AddressingModel);
  inline void setMemoryModel(spv::MemoryModel);

  /// \brief Adds an entry point for the module under construction. We only
  /// support a single entry point per module for now.
  inline void addEntryPoint(spv::ExecutionModel em, uint32_t targetId,
                            std::string targetName,
                            llvm::ArrayRef<uint32_t> interfaces);

  /// \brief Adds an execution mode to the module under construction.
  void addExecutionMode(uint32_t entryPointId, spv::ExecutionMode em,
                        llvm::ArrayRef<uint32_t> params);

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

  /// \brief Decorates the given target <result-id> with the given decoration
  /// (without additional parameters).
  void decorate(uint32_t targetId, spv::Decoration);

  // === Type ===

  uint32_t getVoidType();
  uint32_t getBoolType();
  uint32_t getInt32Type();
  uint32_t getUint32Type();
  uint32_t getFloat32Type();
  uint32_t getVecType(uint32_t elemType, uint32_t elemCount);
  uint32_t getMatType(uint32_t colType, uint32_t colCount);
  uint32_t getPointerType(uint32_t pointeeType, spv::StorageClass);
  uint32_t getStructType(llvm::ArrayRef<uint32_t> fieldTypes,
                         llvm::StringRef structName = "",
                         llvm::ArrayRef<llvm::StringRef> fieldNames = {});
  uint32_t getArrayType(uint32_t elemType, uint32_t count);
  uint32_t getFunctionType(uint32_t returnType,
                           llvm::ArrayRef<uint32_t> paramTypes);
  uint32_t getImageType(uint32_t sampledType, spv::Dim, bool isArray);
  uint32_t getSamplerType();
  uint32_t getSampledImageType(uint32_t imageType);

  // === Constant ===
  uint32_t getConstantBool(bool value);
  uint32_t getConstantInt32(int32_t value);
  uint32_t getConstantUint32(uint32_t value);
  uint32_t getConstantFloat32(float value);
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

  SPIRVContext &theContext; ///< The SPIR-V context.
  SPIRVModule theModule;    ///< The module under building.

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
  theModule.addCapability(cap);
}

void ModuleBuilder::addEntryPoint(spv::ExecutionModel em, uint32_t targetId,
                                  std::string targetName,
                                  llvm::ArrayRef<uint32_t> interfaces) {
  theModule.addEntryPoint(em, targetId, std::move(targetName), interfaces);
}

} // end namespace spirv
} // end namespace clang

#endif
