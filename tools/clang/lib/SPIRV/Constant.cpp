//===--- Constant.cpp - SPIR-V Constant implementation --------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/SPIRV/Constant.h"
#include "clang/SPIRV/BitwiseCast.h"
#include "clang/SPIRV/SPIRVContext.h"

namespace {
uint32_t zeroExtendTo32Bits(uint16_t value) {
  // TODO: The ordering of the 2 words depends on the endian-ness of the host
  // machine. Assuming Little Endian at the moment.
  struct two16Bits {
    uint16_t low;
    uint16_t high;
  };

  two16Bits result = {value, 0};
  return clang::spirv::cast::BitwiseCast<uint32_t, two16Bits>(result);
}

uint32_t signExtendTo32Bits(int16_t value) {
  // TODO: The ordering of the 2 words depends on the endian-ness of the host
  // machine. Assuming Little Endian at the moment.
  struct two16Bits {
    int16_t low;
    uint16_t high;
  };

  two16Bits result = {value, 0};

  // Sign bit is 1
  if (value >> 15) {
    result.high = 0xffff;
  }
  return clang::spirv::cast::BitwiseCast<uint32_t, two16Bits>(result);
}
} // namespace

namespace clang {
namespace spirv {

Constant::Constant(spv::Op op, uint32_t type, llvm::ArrayRef<uint32_t> arg)
    : opcode(op), typeId(type), args(arg.begin(), arg.end()) {}

const Constant *Constant::getUniqueConstant(SPIRVContext &context,
                                            const Constant &c) {
  return context.registerConstant(c);
}

const Constant *Constant::getTrue(SPIRVContext &ctx, uint32_t type_id) {
  Constant c = Constant(spv::Op::OpConstantTrue, type_id, {});
  return getUniqueConstant(ctx, c);
}

const Constant *Constant::getFalse(SPIRVContext &ctx, uint32_t type_id) {
  Constant c = Constant(spv::Op::OpConstantFalse, type_id, {});
  return getUniqueConstant(ctx, c);
}

const Constant *Constant::getFloat16(SPIRVContext &ctx, uint32_t type_id,
                                     int16_t value) {
  // According to the SPIR-V Spec:
  // When the type's bit width is less than 32-bits, the literal's value appears
  // in the low-order bits of the word, and the high-order bits must be 0 for a
  // floating-point type.
  Constant c =
      Constant(spv::Op::OpConstant, type_id, {zeroExtendTo32Bits(value)});
  return getUniqueConstant(ctx, c);
}

const Constant *Constant::getFloat32(SPIRVContext &ctx, uint32_t type_id,
                                     float value) {
  Constant c = Constant(spv::Op::OpConstant, type_id,
                        {cast::BitwiseCast<uint32_t, float>(value)});
  return getUniqueConstant(ctx, c);
}

const Constant *Constant::getFloat64(SPIRVContext &ctx, uint32_t type_id,
                                     double value) {
  // TODO: The ordering of the 2 words depends on the endian-ness of the host
  // machine.
  struct wideFloat {
    uint32_t word0;
    uint32_t word1;
  };
  wideFloat words = cast::BitwiseCast<wideFloat, double>(value);
  Constant c =
      Constant(spv::Op::OpConstant, type_id, {words.word0, words.word1});
  return getUniqueConstant(ctx, c);
}

const Constant *Constant::getUint16(SPIRVContext &ctx, uint32_t type_id,
                                    uint16_t value) {
  // According to the SPIR-V Spec:
  // When the type's bit width is less than 32-bits, the literal's value appears
  // in the low-order bits of the word, and the high-order bits must be 0 for an
  // integer type with Signedness of 0.
  Constant c =
      Constant(spv::Op::OpConstant, type_id, {zeroExtendTo32Bits(value)});
  return getUniqueConstant(ctx, c);
}

const Constant *Constant::getUint32(SPIRVContext &ctx, uint32_t type_id,
                                    uint32_t value) {
  Constant c = Constant(spv::Op::OpConstant, type_id, {value});
  return getUniqueConstant(ctx, c);
}

const Constant *Constant::getUint64(SPIRVContext &ctx, uint32_t type_id,
                                    uint64_t value) {
  struct wideInt {
    uint32_t word0;
    uint32_t word1;
  };
  wideInt words = cast::BitwiseCast<wideInt, uint64_t>(value);
  Constant c =
      Constant(spv::Op::OpConstant, type_id, {words.word0, words.word1});
  return getUniqueConstant(ctx, c);
}

const Constant *Constant::getInt16(SPIRVContext &ctx, uint32_t type_id,
                                   int16_t value) {
  // According to the SPIR-V Spec:
  // When the type's bit width is less than 32-bits, the literal's value appears
  // in the low-order bits of the word, and the high-order bits must be
  // sign-extended for integers with Signedness of 1.
  Constant c =
      Constant(spv::Op::OpConstant, type_id, {signExtendTo32Bits(value)});
  return getUniqueConstant(ctx, c);
}

const Constant *Constant::getInt32(SPIRVContext &ctx, uint32_t type_id,
                                   int32_t value) {
  Constant c = Constant(spv::Op::OpConstant, type_id,
                        {cast::BitwiseCast<uint32_t, int32_t>(value)});
  return getUniqueConstant(ctx, c);
}

const Constant *Constant::getInt64(SPIRVContext &ctx, uint32_t type_id,
                                   int64_t value) {
  struct wideInt {
    uint32_t word0;
    uint32_t word1;
  };
  wideInt words = cast::BitwiseCast<wideInt, int64_t>(value);
  Constant c =
      Constant(spv::Op::OpConstant, type_id, {words.word0, words.word1});
  return getUniqueConstant(ctx, c);
}

const Constant *Constant::getComposite(SPIRVContext &ctx, uint32_t type_id,
                                       llvm::ArrayRef<uint32_t> constituents) {
  Constant c = Constant(spv::Op::OpConstantComposite, type_id, constituents);
  return getUniqueConstant(ctx, c);
}

const Constant *Constant::getSampler(SPIRVContext &ctx, uint32_t type_id,
                                     spv::SamplerAddressingMode sam,
                                     uint32_t param,
                                     spv::SamplerFilterMode sfm) {
  Constant c =
      Constant(spv::Op::OpConstantSampler, type_id,
               {static_cast<uint32_t>(sam), param, static_cast<uint32_t>(sfm)});
  return getUniqueConstant(ctx, c);
}

const Constant *Constant::getNull(SPIRVContext &ctx, uint32_t type_id) {
  Constant c = Constant(spv::Op::OpConstantNull, type_id, {});
  return getUniqueConstant(ctx, c);
}

bool Constant::isBoolean() const {
  return (opcode == spv::Op::OpConstantTrue ||
          opcode == spv::Op::OpConstantFalse ||
          opcode == spv::Op::OpSpecConstantTrue ||
          opcode == spv::Op::OpSpecConstantFalse);
}

bool Constant::isNumerical() const {
  return (opcode == spv::Op::OpConstant || opcode == spv::Op::OpSpecConstant);
}

bool Constant::isComposite() const {
  return (opcode == spv::Op::OpConstantComposite ||
          opcode == spv::Op::OpSpecConstantComposite);
}

std::vector<uint32_t> Constant::withResultId(uint32_t resultId) const {
  std::vector<uint32_t> words;

  // TODO: we are essentially duplicate the work InstBuilder is responsible for.
  // Should figure out a way to unify them.
  words.reserve(3 + args.size());
  words.push_back(static_cast<uint32_t>(opcode));
  words.push_back(typeId);
  words.push_back(resultId);
  words.insert(words.end(), args.begin(), args.end());
  words.front() |= static_cast<uint32_t>(words.size()) << 16;

  return words;
}

} // end namespace spirv
} // end namespace clang
