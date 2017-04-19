//===-- SPIRVContext.h - Context holding SPIR-V codegen data ----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_CLANG_SPIRV_SPIRVCONTEXT_H
#define LLVM_CLANG_SPIRV_SPIRVCONTEXT_H

#include <unordered_map>
#include <unordered_set>

#include "clang/SPIRV/Decoration.h"

namespace clang {
namespace spirv {

struct DecorationHash {
  std::size_t operator()(const Decoration &d) const {
    // TODO: We could probably improve this hash function if needed.
    return std::hash<uint32_t>{}(static_cast<uint32_t>(d.getValue()));
  }
};

/// \brief A class for holding various data needed in SPIR-V codegen.
/// It should outlive all SPIR-V codegen components that requires/allocates
/// data.
class SPIRVContext {
public:
  /// \brief Constructs a default SPIR-V context.
  inline SPIRVContext();

  // Disable copy/move constructors/assignments.
  SPIRVContext(const SPIRVContext &) = delete;
  SPIRVContext(SPIRVContext &&) = delete;
  SPIRVContext &operator=(const SPIRVContext &) = delete;
  SPIRVContext &operator=(SPIRVContext &&) = delete;

  /// \brief Returns the next unused <result-id>.
  inline uint32_t getNextId() const;
  /// \brief Consumes the next unused <result-id>.
  inline uint32_t takeNextId();

  /// \brief Registers the existence of the given decoration in the current
  /// context, and returns the unique Decoration pointer.
  const Decoration *registerDecoration(const Decoration &);

private:
  using DecorationSet = std::unordered_set<Decoration, DecorationHash>;

  uint32_t nextId;

  /// \brief All the unique Decorations defined in the current context.
  DecorationSet existingDecorations;
};

SPIRVContext::SPIRVContext() : nextId(1) {}
uint32_t SPIRVContext::getNextId() const { return nextId; }
uint32_t SPIRVContext::takeNextId() { return nextId++; }

} // end namespace spirv
} // end namespace clang

#endif
