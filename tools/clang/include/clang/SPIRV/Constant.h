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

#include <vector>

#include "spirv/unified1/spirv.hpp11"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"

namespace clang {
namespace spirv {

class SPIRVContext;

/// \brief SPIR-V Constant
///
/// This class defines a unique SPIR-V constant.
/// A SPIR-V constant includes its <opcode> defined by the SPIR-V Spec.
/// It also incldues any arguments (32-bit words) needed to initialize that
/// constant.
///
/// The class includes static getXXX(...) functions for getting pointers of any
/// needed constant. A unique constant has a unique pointer (e.g. calling
/// 'getTrue' function will always return the same pointer for the given
/// context).
class Constant {
public:
  spv::Op getOpcode() const { return opcode; }
  uint32_t getTypeId() const { return typeId; }
  const llvm::SmallVector<uint32_t, 4> &getArgs() const { return args; }

  // OpConstantTrue and OpConstantFalse are boolean.
  // OpSpecConstantTrue and OpSpecConstantFalse are boolean.
  bool isBoolean() const;

  // OpConstant and OpSpecConstant are only allowed to take integers and floats.
  bool isNumerical() const;

  // OpConstantComposite and OpSpecConstantComposite.
  bool isComposite() const;

  // Get constants.
  static const Constant *getTrue(SPIRVContext &ctx, uint32_t type_id);
  static const Constant *getFalse(SPIRVContext &ctx, uint32_t type_id);
  static const Constant *getInt16(SPIRVContext &ctx, uint32_t type_id,
                                  int16_t value);
  static const Constant *getInt32(SPIRVContext &ctx, uint32_t type_id,
                                  int32_t value);
  static const Constant *getInt64(SPIRVContext &ctx, uint32_t type_id,
                                  int64_t value);
  static const Constant *getUint16(SPIRVContext &ctx, uint32_t type_id,
                                   uint16_t value);
  static const Constant *getUint32(SPIRVContext &ctx, uint32_t type_id,
                                   uint32_t value);
  static const Constant *getUint64(SPIRVContext &ctx, uint32_t type_id,
                                   uint64_t value);
  static const Constant *getFloat16(SPIRVContext &ctx, uint32_t type_id,
                                    int16_t value);
  static const Constant *getFloat32(SPIRVContext &ctx, uint32_t type_id,
                                    float value);
  static const Constant *getFloat64(SPIRVContext &ctx, uint32_t type_id,
                                    double value);

  // TODO: 64-bit float and integer constant implementation

  static const Constant *getComposite(SPIRVContext &ctx, uint32_t type_id,
                                      llvm::ArrayRef<uint32_t> constituents);
  static const Constant *getSampler(SPIRVContext &ctx, uint32_t type_id,
                                    spv::SamplerAddressingMode, uint32_t param,
                                    spv::SamplerFilterMode);
  static const Constant *getNull(SPIRVContext &ctx, uint32_t type_id);

  bool operator==(const Constant &other) const {
    return opcode == other.opcode && typeId == other.typeId &&
           args == other.args;
  }

  // \brief Construct the SPIR-V words for this constant with the given
  // <result-id>.
  std::vector<uint32_t> withResultId(uint32_t resultId) const;

private:
  /// \brief Private constructor.
  Constant(spv::Op, uint32_t type, llvm::ArrayRef<uint32_t> arg = {});

  /// \brief Returns the unique constant pointer within the given context.
  static const Constant *getUniqueConstant(SPIRVContext &, const Constant &);

private:
  spv::Op opcode;  ///< OpCode of the constant
  uint32_t typeId; ///< <result-id> of the type of the constant
  llvm::SmallVector<uint32_t, 4> args; ///< Arguments defining the constant
};

} // end namespace spirv
} // end namespace clang

#endif
