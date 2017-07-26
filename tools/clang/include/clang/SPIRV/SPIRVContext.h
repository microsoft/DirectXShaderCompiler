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

#include "clang/Frontend/FrontendAction.h"
#include "clang/SPIRV/Constant.h"
#include "clang/SPIRV/Decoration.h"
#include "clang/SPIRV/Type.h"

namespace clang {
namespace spirv {

struct TypeHash {
  std::size_t operator()(const Type &t) const {
    // TODO: We could improve this hash function if necessary.
    return std::hash<uint32_t>{}(static_cast<uint32_t>(t.getOpcode()));
  }
};
struct DecorationHash {
  std::size_t operator()(const Decoration &d) const {
    // TODO: We could probably improve this hash function if needed.
    return std::hash<uint32_t>{}(static_cast<uint32_t>(d.getValue()));
  }
};
struct ConstantHash {
  std::size_t operator()(const Constant &c) const {
    // TODO: We could improve this hash function if necessary.
    return std::hash<uint32_t>{}(static_cast<uint32_t>(c.getTypeId()));
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

  /// \brief Returns the <result-id> that defines the given Type. If the type
  /// has not been defined, it will define and store its instruction.
  uint32_t getResultIdForType(const Type *);

  /// \brief Returns the <result-id> that defines the given Constant. If the
  /// constant has not been defined, it will define and return its result-id.
  uint32_t getResultIdForConstant(const Constant *);

  /// \brief Registers the existence of the given type in the current context,
  /// and returns the unique Type pointer.
  const Type *registerType(const Type &);

  /// \brief Registers the existence of the given constant in the current
  /// context, and returns the unique pointer to it.
  const Constant *registerConstant(const Constant &);

  /// \brief Registers the existence of the given decoration in the current
  /// context, and returns the unique Decoration pointer.
  const Decoration *registerDecoration(const Decoration &);

private:
  using TypeSet = std::unordered_set<Type, TypeHash>;
  using ConstantSet = std::unordered_set<Constant, ConstantHash>;
  using DecorationSet = std::unordered_set<Decoration, DecorationHash>;

  uint32_t nextId;

  /// \brief All the unique Decorations defined in the current context.
  DecorationSet existingDecorations;

  /// \brief All the unique types defined in the current context.
  TypeSet existingTypes;

  /// \brief All constants defined in the current context.
  /// These can be boolean, integer, float, or composite constants.
  ConstantSet existingConstants;

  /// \brief Maps a given type to the <result-id> that is defined for
  /// that type. If a Type* does not exist in the map, the type
  /// is not yet defined and is not associated with a <result-id>.
  std::unordered_map<const Type *, uint32_t> typeResultIdMap;

  /// \brief Maps a given constant to the <result-id> that is defined for
  /// that constant. If a Constant* does not exist in the map, the constant
  /// is not yet defined and is not associated with a <result-id>.
  std::unordered_map<const Constant *, uint32_t> constantResultIdMap;
};

SPIRVContext::SPIRVContext() : nextId(1) {}
uint32_t SPIRVContext::getNextId() const { return nextId; }
uint32_t SPIRVContext::takeNextId() { return nextId++; }

} // end namespace spirv
} // end namespace clang

#endif
