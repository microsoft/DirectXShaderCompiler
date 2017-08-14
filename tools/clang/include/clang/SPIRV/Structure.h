//===--- Structure.h - SPIR-V representation structures ------*- C++ -*---===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines several classes for representing SPIR-V basic blocks,
// functions, and modules. They are not intended to be general representations
// that can be used for various purposes; instead, they are just special
// crafted to be provide structured representation of SPIR-V modules in the
// ModuleBuilder.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_SPIRV_STRUCTURE_H
#define LLVM_CLANG_SPIRV_STRUCTURE_H

#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "spirv/1.0/spirv.hpp11"
#include "clang/SPIRV/Constant.h"
#include "clang/SPIRV/InstBuilder.h"
#include "clang/SPIRV/Type.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/StringRef.h"

namespace clang {
namespace spirv {

// === Instruction definition ===

/// \brief The class representing a SPIR-V instruction.
class Instruction {
public:
  /// Constructs an instruction from the given underlying SPIR-V binary words.
  inline Instruction(std::vector<uint32_t> &&);

  // Copy constructor/assignment
  Instruction(const Instruction &) = default;
  Instruction &operator=(const Instruction &) = default;

  // Move constructor/assignment
  Instruction(Instruction &&) = default;
  Instruction &operator=(Instruction &&) = default;

  /// Returns true if this instruction is empty, which contains no underlying
  /// SPIR-V binary words.
  inline bool isEmpty() const;

  /// Returns the opcode for this instruction. Returns spv::Op::Max if this
  /// instruction is empty.
  spv::Op getOpcode() const;

  /// Returns the underlying SPIR-v binary words for this instruction.
  /// This instruction will be in an empty state after this call.
  inline std::vector<uint32_t> take();

  /// Returns true if this instruction is a termination instruction.
  ///
  /// See "2.2.4. Control Flow" in the SPIR-V spec for the defintion of
  /// termination instructions.
  bool isTerminator() const;

private:
  // TODO: do some statistics and switch to use SmallVector here if helps.
  std::vector<uint32_t> words; ///< Underlying SPIR-V words
};

// === Basic block definition ===

/// \brief The class representing a SPIR-V basic block.
class BasicBlock {
public:
  /// \brief Constructs a basic block with the given <label-id>.
  inline explicit BasicBlock(uint32_t labelId, llvm::StringRef debugName = "");

  // Disable copy constructor/assignment
  BasicBlock(const BasicBlock &) = delete;
  BasicBlock &operator=(const BasicBlock &) = delete;

  // Move constructor/assignment
  BasicBlock(BasicBlock &&that);
  BasicBlock &operator=(BasicBlock &&that);

  /// \brief Returns true if this basic block is empty, which has no <label-id>
  /// assigned and no instructions.
  inline bool isEmpty() const;
  /// \brief Clears everything in this basic block and turns it into an
  /// empty basic block.
  inline void clear();

  /// \brief Serializes this basic block and feeds it to the comsumer in the
  /// given InstBuilder. After this call, this basic block will be in an empty
  /// state.
  void take(InstBuilder *builder);

  /// \brief Appends an instruction to this basic block.
  inline void appendInstruction(Instruction &&);

  /// \brief Preprends an instruction to this basic block.
  inline void prependInstruction(Instruction &&);

  /// \brief Adds the given basic block as a successsor to this basic block.
  inline void addSuccessor(BasicBlock *);

  /// \brief Gets all successor basic blocks.
  inline const llvm::SmallVector<BasicBlock *, 2> &getSuccessors() const;

  /// \brief Sets the merge target to the given basic block.
  /// The caller must make sure this basic block contains an OpSelectionMerge or
  /// OpLoopMerge instruction.
  inline void setMergeTarget(BasicBlock *);

  /// \brief Returns the merge target if this basic block contains an
  /// OpSelectionMerge or OpLoopMerge instruction. Returns nullptr otherwise.
  inline BasicBlock *getMergeTarget() const;

  /// \brief Sets the continue target to the given basic block.
  /// The caller must make sure this basic block contains an OpLoopMerge
  /// instruction.
  inline void setContinueTarget(BasicBlock *);

  /// \brief Returns the continue target if this basic block contains an
  /// OpLoopMerge instruction. Returns nullptr otherwise.
  inline BasicBlock *getContinueTarget() const;

  /// \brief Returns the label id of this basic block.
  inline uint32_t getLabelId() const;

  /// \brief Returns the debug name of this basic block.
  inline llvm::StringRef getDebugName() const;

  /// \brief Returns true if this basic block is terminated.
  bool isTerminated() const;

private:
  uint32_t labelId; ///< The label id for this basic block. Zero means invalid.
  std::string debugName;
  std::deque<Instruction> instructions;

  llvm::SmallVector<BasicBlock *, 2> successors;
  BasicBlock *mergeTarget;
  BasicBlock *continueTarget;
};

// === Function definition ===

/// \brief The class representing a SPIR-V function.
class Function {
public:
  /// \brief Constructs a SPIR-V function with the given parameters.
  inline Function(uint32_t resultType, uint32_t resultId,
                  spv::FunctionControlMask control, uint32_t functionType);

  // Disable copy constructor/assignment
  Function(const Function &) = delete;
  Function &operator=(const Function &) = delete;

  // Move constructor/assignment
  Function(Function &&that);
  Function &operator=(Function &&that);

  /// \brief Returns true if this function is empty.
  inline bool isEmpty() const;

  /// \brief Clears all paramters and basic blocks and turns this function into
  /// an empty function.
  void clear();

  /// \brief Serializes this function and feeds it to the comsumer in the given
  /// InstBuilder. After this call, this function will be in an empty state.
  void take(InstBuilder *builder);

  /// \brief Adds a parameter to this function.
  inline void addParameter(uint32_t paramResultType, uint32_t paramResultId);

  /// \brief Adds a local variable to this function.
  void addVariable(uint32_t varResultType, uint32_t varResultId,
                   llvm::Optional<uint32_t> init);

  /// \brief Adds a basic block to this function.
  inline void addBasicBlock(std::unique_ptr<BasicBlock> block);

  /// \brief Adds the reachable basic blocks of this function to the given
  /// vector.
  void getReachableBasicBlocks(std::vector<BasicBlock *> *) const;

private:
  uint32_t resultType;
  uint32_t resultId;
  spv::FunctionControlMask funcControl;
  uint32_t funcType;

  /// Parameter <result-type> and <result-id> pairs.
  std::vector<std::pair<uint32_t, uint32_t>> parameters;
  /// Local variables.
  std::vector<Instruction> variables;
  std::vector<std::unique_ptr<BasicBlock>> blocks;
};

// === Module components defintion ====

/// \brief The struct representing a SPIR-V module header.
struct Header {
  /// \brief Default constructs a SPIR-V module header with id bound 0.
  Header();

  /// \brief Feeds the consumer with all the SPIR-V words for this header.
  void collect(const WordConsumer &consumer);

  const uint32_t magicNumber;
  const uint32_t version;
  const uint32_t generator;
  uint32_t bound;
  const uint32_t reserved;
};

/// \brief The struct representing an entry point.
struct EntryPoint {
  inline EntryPoint(spv::ExecutionModel, uint32_t id, std::string name,
                    const std::vector<uint32_t> &interface);

  const spv::ExecutionModel executionModel;
  const uint32_t targetId;
  const std::string targetName;
  const std::vector<uint32_t> interfaces;
};

/// \brief The struct representing a debug name.
struct DebugName {
  inline DebugName(uint32_t id, std::string targetName,
                   llvm::Optional<uint32_t> index = llvm::None);

  const uint32_t targetId;
  const std::string name;
  const llvm::Optional<uint32_t> memberIndex;
};

/// \brief The struct representing a deocoration and its target <result-id>.
struct DecorationIdPair {
  inline DecorationIdPair(const Decoration &decor, uint32_t id);

  const Decoration &decoration;
  const uint32_t targetId;
};

/// \brief The struct representing a type and its <result-id>.
struct TypeIdPair {
  inline TypeIdPair(const Type &ty, uint32_t id);

  const Type &type;
  const uint32_t resultId;
};

// === Module defintion ====

/// \brief The class representing a SPIR-V module.
class SPIRVModule {
public:
  /// \brief Default constructs an empty SPIR-V module.
  inline SPIRVModule();

  // Disable copy constructor/assignment
  SPIRVModule(const SPIRVModule &) = delete;
  SPIRVModule &operator=(const SPIRVModule &) = delete;

  // Move constructor/assignment
  SPIRVModule(SPIRVModule &&that) = default;
  SPIRVModule &operator=(SPIRVModule &&that) = default;

  /// \brief Returns true if this module is empty.
  bool isEmpty() const;
  /// \brief Clears all instructions and functions and turns this module into
  /// an empty module.
  void clear();

  /// \brief Collects all the SPIR-V words in this module and consumes them
  /// using the consumer within the given InstBuilder. This method is
  /// destructive; the module will be consumed and cleared after calling it.
  void take(InstBuilder *builder);

  /// \brief Sets the id bound to the given bound.
  inline void setBound(uint32_t newBound);

  inline void addCapability(spv::Capability);
  inline void addExtension(std::string extension);
  inline void addExtInstSet(uint32_t setId, llvm::StringRef extInstSet);
  inline void setAddressingModel(spv::AddressingModel);
  inline void setMemoryModel(spv::MemoryModel);
  inline void addEntryPoint(spv::ExecutionModel, uint32_t targetId,
                            std::string targetName,
                            llvm::ArrayRef<uint32_t> intefaces);
  inline void addExecutionMode(Instruction &&);
  // TODO: source code debug information
  inline void addDebugName(uint32_t targetId, llvm::StringRef name,
                           llvm::Optional<uint32_t> memberIndex = llvm::None);
  inline void addDecoration(const Decoration &decoration, uint32_t targetId);
  inline void addType(const Type *type, uint32_t resultId);
  inline void addConstant(const Constant *constant, uint32_t resultId);
  inline void addVariable(Instruction &&);
  inline void addFunction(std::unique_ptr<Function>);

  /// \brief Returns the <result-id> of the given extended instruction set.
  /// Returns 0 if the given set has not been imported.
  inline uint32_t getExtInstSetId(llvm::StringRef setName);

private:
  /// \brief Collects all the Integer type definitions in this module and
  /// consumes them using the consumer within the given InstBuilder.
  /// After this method is called, all integer types are remove from the list of
  /// types in this object.
  void takeIntegerTypes(InstBuilder *builder);

  /// \brief Finds the constant on which the given array type depends.
  /// If found, (a) defines the constant by passing it to the consumer in the
  /// given InstBuilder. (b) Removes the constant from the list of constants
  /// in this object.
  void takeConstantForArrayType(const Type &arrType, InstBuilder *ib);

private:
  Header header; ///< SPIR-V module header.
  std::vector<spv::Capability> capabilities;
  std::vector<std::string> extensions;
  llvm::MapVector<const char *, uint32_t> extInstSets;
  // Addressing and memory model must exist for a valid SPIR-V module.
  // We make them optional here just to provide extra flexibility of
  // the representation.
  llvm::Optional<spv::AddressingModel> addressingModel;
  llvm::Optional<spv::MemoryModel> memoryModel;
  std::vector<EntryPoint> entryPoints;
  std::vector<Instruction> executionModes;
  // TODO: source code debug information
  std::vector<DebugName> debugNames;
  std::vector<DecorationIdPair> decorations;
  // Note that types and constants are interdependent; Types like arrays have
  // <result-id>s for constants in their definition, and constants all have
  // their corresponding types. We store types and constants separately, but
  // they should be handled together.
  llvm::MapVector<const Type *, uint32_t> types;
  llvm::MapVector<const Constant *, uint32_t> constants;

  std::vector<Instruction> variables;
  std::vector<std::unique_ptr<Function>> functions;
};

// === Instruction inline implementations ===

Instruction::Instruction(std::vector<uint32_t> &&data)
    : words(std::move(data)) {}

bool Instruction::isEmpty() const { return words.empty(); }

std::vector<uint32_t> Instruction::take() { return std::move(words); }

// === Basic block inline implementations ===

BasicBlock::BasicBlock(uint32_t id, llvm::StringRef name)
    : labelId(id), debugName(name), mergeTarget(nullptr),
      continueTarget(nullptr) {}

bool BasicBlock::isEmpty() const {
  return labelId == 0 && instructions.empty();
}

void BasicBlock::appendInstruction(Instruction &&inst) {
  instructions.push_back(std::move(inst));
}

void BasicBlock::prependInstruction(Instruction &&inst) {
  instructions.push_front(std::move(inst));
}

void BasicBlock::addSuccessor(BasicBlock *successor) {
  successors.push_back(successor);
}

const llvm::SmallVector<BasicBlock *, 2> &BasicBlock::getSuccessors() const {
  return successors;
}

void BasicBlock::setMergeTarget(BasicBlock *target) { mergeTarget = target; }

BasicBlock *BasicBlock::getMergeTarget() const { return mergeTarget; }

void BasicBlock::setContinueTarget(BasicBlock *target) {
  continueTarget = target;
}

BasicBlock *BasicBlock::getContinueTarget() const { return continueTarget; }

uint32_t BasicBlock::getLabelId() const { return labelId; }
llvm::StringRef BasicBlock::getDebugName() const { return debugName; }

// === Function inline implementations ===

Function::Function(uint32_t rType, uint32_t rId,
                   spv::FunctionControlMask control, uint32_t fType)
    : resultType(rType), resultId(rId), funcControl(control), funcType(fType) {}

bool Function::isEmpty() const {
  return resultType == 0 && resultId == 0 &&
         funcControl == spv::FunctionControlMask::MaskNone && funcType == 0 &&
         parameters.empty() && blocks.empty();
}

void Function::addParameter(uint32_t rType, uint32_t rId) {
  parameters.emplace_back(rType, rId);
}

void Function::addBasicBlock(std::unique_ptr<BasicBlock> block) {
  blocks.push_back(std::move(block));
}

// === Module components inline implementations ===

EntryPoint::EntryPoint(spv::ExecutionModel em, uint32_t id, std::string name,
                       const std::vector<uint32_t> &interface)
    : executionModel(em), targetId(id), targetName(std::move(name)),
      interfaces(interface) {}

DebugName::DebugName(uint32_t id, std::string targetName,
                     llvm::Optional<uint32_t> index)
    : targetId(id), name(std::move(targetName)), memberIndex(index) {}

DecorationIdPair::DecorationIdPair(const Decoration &decor, uint32_t id)
    : decoration(decor), targetId(id) {}

TypeIdPair::TypeIdPair(const Type &ty, uint32_t id) : type(ty), resultId(id) {}

// === Module inline implementations ===

SPIRVModule::SPIRVModule()
    : addressingModel(llvm::None), memoryModel(llvm::None) {}

void SPIRVModule::setBound(uint32_t newBound) { header.bound = newBound; }

void SPIRVModule::addCapability(spv::Capability cap) {
  capabilities.push_back(cap);
}

void SPIRVModule::addExtension(std::string ext) {
  extensions.push_back(std::move(ext));
}

uint32_t SPIRVModule::getExtInstSetId(llvm::StringRef setName) {
  const auto &iter = extInstSets.find(setName.data());
  if (iter != extInstSets.end())
    return iter->second;
  return 0;
}

void SPIRVModule::addExtInstSet(uint32_t setId, llvm::StringRef extInstSet) {
  extInstSets.insert(std::make_pair(extInstSet.data(), setId));
}

void SPIRVModule::setAddressingModel(spv::AddressingModel am) {
  addressingModel = llvm::Optional<spv::AddressingModel>(am);
}

void SPIRVModule::setMemoryModel(spv::MemoryModel mm) {
  memoryModel = llvm::Optional<spv::MemoryModel>(mm);
}

void SPIRVModule::addEntryPoint(spv::ExecutionModel em, uint32_t targetId,
                                std::string name,
                                llvm::ArrayRef<uint32_t> interfaces) {
  entryPoints.emplace_back(em, targetId, std::move(name), interfaces);
}

void SPIRVModule::addExecutionMode(Instruction &&execMode) {
  executionModes.push_back(std::move(execMode));
}

void SPIRVModule::addDebugName(uint32_t targetId, llvm::StringRef name,
                               llvm::Optional<uint32_t> memberIndex) {
  if (!name.empty()) {
    debugNames.emplace_back(targetId, name, memberIndex);
  }
}

void SPIRVModule::addDecoration(const Decoration &decoration,
                                uint32_t targetId) {
  decorations.emplace_back(decoration, targetId);
}

void SPIRVModule::addType(const Type *type, uint32_t resultId) {
  types.insert(std::make_pair(type, resultId));
}

void SPIRVModule::addConstant(const Constant *constant, uint32_t resultId) {
  constants.insert(std::make_pair(constant, resultId));
};

void SPIRVModule::addVariable(Instruction &&var) {
  variables.push_back(std::move(var));
}

void SPIRVModule::addFunction(std::unique_ptr<Function> f) {
  functions.push_back(std::move(f));
}

} // end namespace spirv
} // end namespace clang

#endif
