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

#include "clang/SPIRV/InstBuilder.h"
#include "clang/SPIRV/SPIRVContext.h"
#include "clang/SPIRV/Structure.h"
#include "llvm/ADT/MapVector.h"

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

  /// \brief Takes the SPIR-V module under building. This will consume the
  /// module under construction.
  std::vector<uint32_t> takeModule();

  // === Function and Basic Block ===

  /// \brief Begins building a SPIR-V function. At any time, there can only
  /// exist at most one function under building. Returns the <result-id> for the
  /// function on success. Returns zero on failure.
  uint32_t beginFunction(uint32_t funcType, uint32_t returnType,
                         std::string name = "");

  /// \brief Registers a function parameter of the given type onto the current
  /// function under construction and returns its <result-id>.
  uint32_t addFnParameter(uint32_t type);

  /// \brief Ends building of the current function. Returns true of success,
  /// false on failure. All basic blocks constructed from the beginning or
  /// after ending the previous function will be collected into this function.
  bool endFunction();

  /// \brief Creates a SPIR-V basic block. On success, returns the <label-id>
  /// for the basic block. On failure, returns zero.
  uint32_t createBasicBlock();

  /// \brief Returns true if the current basic block inserting into is
  /// terminated.
  inline bool isCurrentBasicBlockTerminated() const;

  /// \brief Sets insertion point to the basic block with the given <label-id>.
  /// Returns true on success, false on failure.
  bool setInsertPoint(uint32_t labelId);

  // === Instruction at the current Insertion Point ===

  /// \brief Creates a load instruction loading the value of the given
  /// <result-type> from the given pointer. Returns the <result-id> for the
  /// loaded value.
  uint32_t createLoad(uint32_t resultType, uint32_t pointer);
  /// \brief Creates a store instruction storing the given value into the given
  /// address.
  void createStore(uint32_t address, uint32_t value);

  /// \brief Creates a return instruction.
  void createReturn();
  /// \brief Creates a return value instruction.
  void createReturnValue(uint32_t value);

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
  inline void addExecutionMode(uint32_t entryPointId, spv::ExecutionMode em,
                               const std::vector<uint32_t> &params);

  /// \brief Adds a stage input/ouput variable whose value is of the given type.
  ///
  /// The corresponding pointer type of the given type will be constructed in
  /// this method for the variable itself.
  uint32_t addStageIOVariable(uint32_t type, spv::StorageClass storageClass,
                              llvm::Optional<uint32_t> initializer);

  /// \brief Decorates the given target <result-id> with the given location.
  void decorateLocation(uint32_t targetId, uint32_t location);

  // === Type ===

  uint32_t getVoidType();
  uint32_t getFloatType();
  uint32_t getVec2Type(uint32_t elemType);
  uint32_t getVec3Type(uint32_t elemType);
  uint32_t getVec4Type(uint32_t elemType);
  uint32_t getPointerType(uint32_t pointeeType, spv::StorageClass);
  uint32_t getFunctionType(uint32_t returnType,
                           const std::vector<uint32_t> &paramTypes);

private:
  /// \brief Map from basic blocks' <label-id> to their structured
  /// representation.
  using OrderedBasicBlockMap =
      llvm::MapVector<uint32_t, std::unique_ptr<BasicBlock>>;

  SPIRVContext &theContext; ///< The SPIR-V context.
  SPIRVModule theModule;    ///< The module under building.

  std::unique_ptr<Function> theFunction; ///< The function under building.
  OrderedBasicBlockMap basicBlocks;      ///< The basic blocks under building.
  BasicBlock *insertPoint;               ///< The current insertion point.

  std::vector<uint32_t> constructSite; ///< InstBuilder construction site.
  InstBuilder instBuilder;
};

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
  theModule.addEntryPoint(em, targetId, targetName, interfaces);
}

void ModuleBuilder::addExecutionMode(uint32_t entryPointId,
                                     spv::ExecutionMode em,
                                     const std::vector<uint32_t> &params) {
  instBuilder.opExecutionMode(entryPointId, em);
  for (const auto &param : params) {
    instBuilder.literalInteger(param);
  }
  instBuilder.x();
  theModule.addExecutionMode(std::move(constructSite));
}

} // end namespace spirv
} // end namespace clang

#endif
