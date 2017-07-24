//===-- Constant.h - SPIR-V Constant ----------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_CLANG_SPIRV_CONSTANT_H
#define LLVM_CLANG_SPIRV_CONSTANT_H

#include <set>
#include <unordered_set>
#include <vector>

#include "spirv/1.0/spirv.hpp11"
#include "clang/SPIRV/Decoration.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/Optional.h"

namespace clang {
namespace spirv {

class SPIRVContext;

/// \brief SPIR-V Constant
///
/// This class defines a unique SPIR-V constant.
/// A SPIR-V constant includes its <opcode> defined by the SPIR-V Spec.
/// It also incldues any arguments (32-bit words) needed to initialize that
/// constant. It also includes a set of decorations that are applied to
/// that constant.
///
/// The class includes static getXXX(...) functions for getting pointers of any
/// needed constant. A unique constant has a unique pointer (e.g. calling
/// 'getTrue' function will always return the same pointer for the given
/// context).
class Constant {
public:
  using DecorationSet = std::set<const Decoration *>;

  spv::Op getOpcode() const { return opcode; }
  uint32_t getTypeId() const { return typeId; }
  const std::vector<uint32_t> &getArgs() const { return args; }
  const DecorationSet &getDecorations() const { return decorations; }
  bool hasDecoration(const Decoration *) const;

  // OpConstantTrue and OpConstantFalse are boolean.
  // OpSpecConstantTrue and OpSpecConstantFalse are boolean.
  bool isBoolean() const;

  // OpConstant and OpSpecConstant are only allowed to take integers and floats.
  bool isNumerical() const;

  // OpConstantComposite and OpSpecConstantComposite.
  bool isComposite() const;

  // Get constants.
  static const Constant *getTrue(SPIRVContext &ctx, uint32_t type_id,
                                 DecorationSet dec = {});
  static const Constant *getFalse(SPIRVContext &ctx, uint32_t type_id,
                                  DecorationSet dec = {});
  static const Constant *getInt32(SPIRVContext &ctx, uint32_t type_id,
                                  int32_t value, DecorationSet dec = {});
  static const Constant *getUint32(SPIRVContext &ctx, uint32_t type_id,
                                   uint32_t value, DecorationSet dec = {});
  static const Constant *getFloat32(SPIRVContext &ctx, uint32_t type_id,
                                    float value, DecorationSet dec = {});

  // TODO: 64-bit float and integer constant implementation

  static const Constant *getComposite(SPIRVContext &ctx, uint32_t type_id,
                                      llvm::ArrayRef<uint32_t> constituents,
                                      DecorationSet dec = {});
  static const Constant *getSampler(SPIRVContext &ctx, uint32_t type_id,
                                    spv::SamplerAddressingMode, uint32_t param,
                                    spv::SamplerFilterMode,
                                    DecorationSet dec = {});
  static const Constant *getNull(SPIRVContext &ctx, uint32_t type_id,
                                 DecorationSet dec = {});

  // Get specialization constants.
  static const Constant *getSpecTrue(SPIRVContext &ctx, uint32_t type_id,
                                     DecorationSet dec = {});
  static const Constant *getSpecFalse(SPIRVContext &ctx, uint32_t type_id,
                                      DecorationSet dec = {});
  static const Constant *getSpecInt32(SPIRVContext &ctx, uint32_t type_id,
                                      int32_t value, DecorationSet dec = {});
  static const Constant *getSpecUint32(SPIRVContext &ctx, uint32_t type_id,
                                       uint32_t value, DecorationSet dec = {});
  static const Constant *getSpecFloat32(SPIRVContext &ctx, uint32_t type_id,
                                        float value, DecorationSet dec = {});
  static const Constant *getSpecComposite(SPIRVContext &ctx, uint32_t type_id,
                                          llvm::ArrayRef<uint32_t> constituents,
                                          DecorationSet dec = {});

  bool operator==(const Constant &other) const {
    return opcode == other.opcode && args == other.args &&
           decorations == other.decorations;
  }

  // \brief Construct the SPIR-V words for this constant with the given
  // <result-id>.
  std::vector<uint32_t> withResultId(uint32_t resultId) const;

private:
  /// \brief Private constructor.
  Constant(spv::Op, uint32_t type, llvm::ArrayRef<uint32_t> arg = {},
           std::set<const Decoration *> dec = {});

  /// \brief Returns the unique constant pointer within the given context.
  static const Constant *getUniqueConstant(SPIRVContext &, const Constant &);

private:
  spv::Op opcode;             ///< OpCode of the constant
  uint32_t typeId;            ///< <result-id> of the type of the constant
  std::vector<uint32_t> args; ///< Arguments needed to define the constant
  DecorationSet decorations;  ///< Decorations applied to the constant
};

} // end namespace spirv
} // end namespace clang

#endif
