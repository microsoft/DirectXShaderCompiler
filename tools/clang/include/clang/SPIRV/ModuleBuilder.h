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

#include "clang/SPIRV/InstBuilder.h"
#include "clang/SPIRV/SPIRVContext.h"
#include "clang/SPIRV/Structure.h"
#include "llvm/ADT/Optional.h"

namespace clang {
namespace spirv {

/// \brief SPIR-V module builder.
///
/// This class exports API for constructing SPIR-V binary interactively.
/// Call beginModule() to start building a SPIR-V module and endModule()
/// to complete building a module. Call takeModule() to get the final
/// SPIR-V binary.
class ModuleBuilder {
public:
  enum class Status {
    Success,
    ErrNestedModule,       ///< Tried to create module inside module
    ErrNestedFunction,     ///< Tried to create function inside function
    ErrNestedBasicBlock,   ///< Tried to crate basic block inside basic block
    ErrDetachedBasicBlock, ///< Tried to create basic block out of function
    ErrNoActiveFunction,   ///< Tried to finish building non existing function
    ErrActiveBasicBlock,   ///< Tried to finish building function when there are
                           ///< active basic block
    ErrNoActiveBasicBlock, ///< Tried to insert instructions without active
                           ///< basic block
  };

  /// \brief Constructs a ModuleBuilder with the given SPIR-V context.
  explicit ModuleBuilder(SPIRVContext *);

  /// \brief Begins building a SPIR-V module.
  Status beginModule();
  /// \brief Ends building the current module.
  Status endModule();
  /// \brief Begins building a SPIR-V function.
  Status beginFunction(uint32_t funcType, uint32_t returnType);
  /// \brief Ends building the current function.
  Status endFunction();
  /// \brief Begins building a SPIR-V basic block.
  Status beginBasicBlock();
  /// \brief Ends building the current SPIR-V basic block with OpReturn.
  Status endBasicBlockWithReturn();

  /// \brief Takes the SPIR-V module under building.
  std::vector<uint32_t> takeModule();

private:
  /// \brief Ends building the current basic block.
  Status endBasicBlock();

  SPIRVContext &theContext;                 ///< The SPIR-V context.
  SPIRVModule theModule;                    ///< The module under building.
  llvm::Optional<Function> theFunction;     ///< The function under building.
  llvm::Optional<BasicBlock> theBasicBlock; ///< The basic block under building.
  std::vector<uint32_t> constructSite;      ///< InstBuilder construction site.
  InstBuilder instBuilder;
};

} // end namespace spirv
} // end namespace clang

#endif