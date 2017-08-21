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

namespace clang {
namespace spirv {

Constant::Constant(spv::Op op, uint32_t type, llvm::ArrayRef<uint32_t> arg,
                   DecorationSet decs)
    : opcode(op), typeId(type), args(arg) {
  decorations = llvm::SetVector<const Decoration *>(decs.begin(), decs.end());
}

const Constant *Constant::getUniqueConstant(SPIRVContext &context,
                                            const Constant &c) {
  return context.registerConstant(c);
}

const Constant *Constant::getTrue(SPIRVContext &ctx, uint32_t type_id,
                                  DecorationSet dec) {
  Constant c = Constant(spv::Op::OpConstantTrue, type_id, {}, dec);
  return getUniqueConstant(ctx, c);
}

const Constant *Constant::getFalse(SPIRVContext &ctx, uint32_t type_id,
                                   DecorationSet dec) {
  Constant c = Constant(spv::Op::OpConstantFalse, type_id, {}, dec);
  return getUniqueConstant(ctx, c);
}

const Constant *Constant::getFloat32(SPIRVContext &ctx, uint32_t type_id,
                                     float value, DecorationSet dec) {
  Constant c = Constant(spv::Op::OpConstant, type_id,
                        {cast::BitwiseCast<uint32_t, float>(value)}, dec);
  return getUniqueConstant(ctx, c);
}

const Constant *Constant::getUint32(SPIRVContext &ctx, uint32_t type_id,
                                    uint32_t value, DecorationSet dec) {
  Constant c = Constant(spv::Op::OpConstant, type_id, {value}, dec);
  return getUniqueConstant(ctx, c);
}

const Constant *Constant::getInt32(SPIRVContext &ctx, uint32_t type_id,
                                   int32_t value, DecorationSet dec) {
  Constant c = Constant(spv::Op::OpConstant, type_id,
                        {cast::BitwiseCast<uint32_t, int32_t>(value)}, dec);
  return getUniqueConstant(ctx, c);
}

const Constant *Constant::getComposite(SPIRVContext &ctx, uint32_t type_id,
                                       llvm::ArrayRef<uint32_t> constituents,
                                       DecorationSet dec) {
  Constant c =
      Constant(spv::Op::OpConstantComposite, type_id, constituents, dec);
  return getUniqueConstant(ctx, c);
}

const Constant *Constant::getSampler(SPIRVContext &ctx, uint32_t type_id,
                                     spv::SamplerAddressingMode sam,
                                     uint32_t param, spv::SamplerFilterMode sfm,
                                     DecorationSet dec) {
  Constant c = Constant(
      spv::Op::OpConstantSampler, type_id,
      {static_cast<uint32_t>(sam), param, static_cast<uint32_t>(sfm)}, dec);
  return getUniqueConstant(ctx, c);
}

const Constant *Constant::getNull(SPIRVContext &ctx, uint32_t type_id,
                                  DecorationSet dec) {
  Constant c = Constant(spv::Op::OpConstantNull, type_id, {}, dec);
  return getUniqueConstant(ctx, c);
}

const Constant *Constant::getSpecTrue(SPIRVContext &ctx, uint32_t type_id,
                                      DecorationSet dec) {
  Constant c = Constant(spv::Op::OpSpecConstantTrue, type_id, {}, dec);
  return getUniqueConstant(ctx, c);
}

const Constant *Constant::getSpecFalse(SPIRVContext &ctx, uint32_t type_id,
                                       DecorationSet dec) {
  Constant c = Constant(spv::Op::OpSpecConstantFalse, type_id, {}, dec);
  return getUniqueConstant(ctx, c);
}

const Constant *Constant::getSpecFloat32(SPIRVContext &ctx, uint32_t type_id,
                                         float value, DecorationSet dec) {
  Constant c = Constant(spv::Op::OpSpecConstant, type_id,
                        {cast::BitwiseCast<uint32_t, float>(value)}, dec);
  return getUniqueConstant(ctx, c);
}

const Constant *Constant::getSpecUint32(SPIRVContext &ctx, uint32_t type_id,
                                        uint32_t value, DecorationSet dec) {
  Constant c = Constant(spv::Op::OpSpecConstant, type_id, {value}, dec);
  return getUniqueConstant(ctx, c);
}

const Constant *Constant::getSpecInt32(SPIRVContext &ctx, uint32_t type_id,
                                       int32_t value, DecorationSet dec) {
  Constant c = Constant(spv::Op::OpSpecConstant, type_id,
                        {cast::BitwiseCast<uint32_t, int32_t>(value)}, dec);
  return getUniqueConstant(ctx, c);
}

const Constant *
Constant::getSpecComposite(SPIRVContext &ctx, uint32_t type_id,
                           llvm::ArrayRef<uint32_t> constituents,
                           DecorationSet dec) {
  Constant c =
      Constant(spv::Op::OpSpecConstantComposite, type_id, constituents, dec);
  return getUniqueConstant(ctx, c);
}

bool Constant::hasDecoration(const Decoration *d) const {
  return decorations.count(d);
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
