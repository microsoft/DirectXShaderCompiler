//===--- Decoration.cpp - SPIR-V Decoration implementation-----------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/SPIRV/Decoration.h"
#include "clang/SPIRV/SPIRVContext.h"
#include "clang/SPIRV/String.h"
#include "llvm/llvm_assert/assert.h"

namespace clang {
namespace spirv {

const Decoration *Decoration::getUniqueDecoration(SPIRVContext &context,
                                                  const Decoration &d) {
  return context.registerDecoration(d);
}
const Decoration *Decoration::getRelaxedPrecision(SPIRVContext &context) {
  Decoration d = Decoration(spv::Decoration::RelaxedPrecision);
  return getUniqueDecoration(context, d);
}
const Decoration *Decoration::getSpecId(SPIRVContext &context, uint32_t id) {
  Decoration d = Decoration(spv::Decoration::SpecId, {id});
  return getUniqueDecoration(context, d);
}
const Decoration *Decoration::getBlock(SPIRVContext &context) {
  Decoration d = Decoration(spv::Decoration::Block);
  return getUniqueDecoration(context, d);
}

const Decoration *Decoration::getBufferBlock(SPIRVContext &context) {
  Decoration d = Decoration(spv::Decoration::BufferBlock);
  return getUniqueDecoration(context, d);
}
const Decoration *Decoration::getRowMajor(SPIRVContext &context,
                                          uint32_t member_idx) {
  Decoration d = Decoration(spv::Decoration::RowMajor);
  d.setMemberIndex(member_idx);
  return getUniqueDecoration(context, d);
}
const Decoration *Decoration::getColMajor(SPIRVContext &context,
                                          uint32_t member_idx) {
  Decoration d = Decoration(spv::Decoration::ColMajor);
  d.setMemberIndex(member_idx);
  return getUniqueDecoration(context, d);
}
const Decoration *Decoration::getArrayStride(SPIRVContext &context,
                                             uint32_t stride) {
  Decoration d = Decoration(spv::Decoration::ArrayStride, {stride});
  return getUniqueDecoration(context, d);
}
const Decoration *Decoration::getMatrixStride(SPIRVContext &context,
                                              uint32_t stride,
                                              uint32_t member_idx) {
  Decoration d = Decoration(spv::Decoration::MatrixStride, {stride});
  d.setMemberIndex(member_idx);
  return getUniqueDecoration(context, d);
}
const Decoration *Decoration::getGLSLShared(SPIRVContext &context) {
  Decoration d = Decoration(spv::Decoration::GLSLShared);
  return getUniqueDecoration(context, d);
}
const Decoration *Decoration::getGLSLPacked(SPIRVContext &context) {
  Decoration d = Decoration(spv::Decoration::GLSLPacked);
  return getUniqueDecoration(context, d);
}
const Decoration *Decoration::getCPacked(SPIRVContext &context) {
  Decoration d = Decoration(spv::Decoration::CPacked);
  return getUniqueDecoration(context, d);
}
const Decoration *Decoration::getBuiltIn(SPIRVContext &context,
                                         spv::BuiltIn builtin,
                                         llvm::Optional<uint32_t> member_idx) {
  Decoration d =
      Decoration(spv::Decoration::BuiltIn, {static_cast<uint32_t>(builtin)});
  d.setMemberIndex(member_idx);
  return getUniqueDecoration(context, d);
}
const Decoration *
Decoration::getNoPerspective(SPIRVContext &context,
                             llvm::Optional<uint32_t> member_idx) {
  Decoration d = Decoration(spv::Decoration::NoPerspective);
  d.setMemberIndex(member_idx);
  return getUniqueDecoration(context, d);
}
const Decoration *Decoration::getFlat(SPIRVContext &context,
                                      llvm::Optional<uint32_t> member_idx) {
  Decoration d = Decoration(spv::Decoration::Flat);
  d.setMemberIndex(member_idx);
  return getUniqueDecoration(context, d);
}
const Decoration *Decoration::getPatch(SPIRVContext &context,
                                       llvm::Optional<uint32_t> member_idx) {
  Decoration d = Decoration(spv::Decoration::Patch);
  d.setMemberIndex(member_idx);
  return getUniqueDecoration(context, d);
}
const Decoration *Decoration::getCentroid(SPIRVContext &context,
                                          llvm::Optional<uint32_t> member_idx) {
  Decoration d = Decoration(spv::Decoration::Centroid);
  d.setMemberIndex(member_idx);
  return getUniqueDecoration(context, d);
}
const Decoration *Decoration::getSample(SPIRVContext &context,
                                        llvm::Optional<uint32_t> member_idx) {
  Decoration d = Decoration(spv::Decoration::Sample);
  d.setMemberIndex(member_idx);
  return getUniqueDecoration(context, d);
}
const Decoration *Decoration::getInvariant(SPIRVContext &context) {
  Decoration d = Decoration(spv::Decoration::Invariant);
  return getUniqueDecoration(context, d);
}
const Decoration *Decoration::getRestrict(SPIRVContext &context) {
  Decoration d = Decoration(spv::Decoration::Restrict);
  return getUniqueDecoration(context, d);
}
const Decoration *Decoration::getAliased(SPIRVContext &context) {
  Decoration d = Decoration(spv::Decoration::Aliased);
  return getUniqueDecoration(context, d);
}
const Decoration *Decoration::getVolatile(SPIRVContext &context,
                                          llvm::Optional<uint32_t> member_idx) {
  Decoration d = Decoration(spv::Decoration::Volatile);
  d.setMemberIndex(member_idx);
  return getUniqueDecoration(context, d);
}
const Decoration *Decoration::getConstant(SPIRVContext &context) {
  Decoration d = Decoration(spv::Decoration::Constant);
  return getUniqueDecoration(context, d);
}
const Decoration *Decoration::getCoherent(SPIRVContext &context,
                                          llvm::Optional<uint32_t> member_idx) {
  Decoration d = Decoration(spv::Decoration::Coherent);
  d.setMemberIndex(member_idx);
  return getUniqueDecoration(context, d);
}
const Decoration *
Decoration::getNonWritable(SPIRVContext &context,
                           llvm::Optional<uint32_t> member_idx) {
  Decoration d = Decoration(spv::Decoration::NonWritable);
  d.setMemberIndex(member_idx);
  return getUniqueDecoration(context, d);
}
const Decoration *
Decoration::getNonReadable(SPIRVContext &context,
                           llvm::Optional<uint32_t> member_idx) {
  Decoration d = Decoration(spv::Decoration::NonReadable);
  d.setMemberIndex(member_idx);
  return getUniqueDecoration(context, d);
}
const Decoration *Decoration::getUniform(SPIRVContext &context,
                                         llvm::Optional<uint32_t> member_idx) {
  Decoration d = Decoration(spv::Decoration::Uniform);
  d.setMemberIndex(member_idx);
  return getUniqueDecoration(context, d);
}
const Decoration *Decoration::getSaturatedConversion(SPIRVContext &context) {
  Decoration d = Decoration(spv::Decoration::SaturatedConversion);
  return getUniqueDecoration(context, d);
}
const Decoration *Decoration::getStream(SPIRVContext &context,
                                        uint32_t stream_number,
                                        llvm::Optional<uint32_t> member_idx) {
  Decoration d = Decoration(spv::Decoration::Stream, {stream_number});
  d.setMemberIndex(member_idx);
  return getUniqueDecoration(context, d);
}
const Decoration *Decoration::getLocation(SPIRVContext &context,
                                          uint32_t location,
                                          llvm::Optional<uint32_t> member_idx) {
  Decoration d = Decoration(spv::Decoration::Location, {location});
  d.setMemberIndex(member_idx);
  return getUniqueDecoration(context, d);
}
const Decoration *
Decoration::getComponent(SPIRVContext &context, uint32_t component,
                         llvm::Optional<uint32_t> member_idx) {
  Decoration d = Decoration(spv::Decoration::Component, {component});
  d.setMemberIndex(member_idx);
  return getUniqueDecoration(context, d);
}
const Decoration *Decoration::getIndex(SPIRVContext &context, uint32_t index) {
  Decoration d = Decoration(spv::Decoration::Index, {index});
  return getUniqueDecoration(context, d);
}
const Decoration *Decoration::getBinding(SPIRVContext &context,
                                         uint32_t binding_point) {
  Decoration d = Decoration(spv::Decoration::Binding, {binding_point});
  return getUniqueDecoration(context, d);
}
const Decoration *Decoration::getDescriptorSet(SPIRVContext &context,
                                               uint32_t set) {
  Decoration d = Decoration(spv::Decoration::DescriptorSet, {set});
  return getUniqueDecoration(context, d);
}
const Decoration *Decoration::getOffset(SPIRVContext &context,
                                        uint32_t byte_offset,
                                        uint32_t member_idx) {
  Decoration d = Decoration(spv::Decoration::Offset, {byte_offset});
  d.setMemberIndex(member_idx);
  return getUniqueDecoration(context, d);
}
const Decoration *
Decoration::getXfbBuffer(SPIRVContext &context, uint32_t xfb_buf,
                         llvm::Optional<uint32_t> member_idx) {
  Decoration d = Decoration(spv::Decoration::XfbBuffer, {xfb_buf});
  d.setMemberIndex(member_idx);
  return getUniqueDecoration(context, d);
}
const Decoration *
Decoration::getXfbStride(SPIRVContext &context, uint32_t xfb_stride,
                         llvm::Optional<uint32_t> member_idx) {
  Decoration d = Decoration(spv::Decoration::XfbStride, {xfb_stride});
  d.setMemberIndex(member_idx);
  return getUniqueDecoration(context, d);
}
const Decoration *
Decoration::getFuncParamAttr(SPIRVContext &context,
                             spv::FunctionParameterAttribute attr) {
  Decoration d =
      Decoration(spv::Decoration::FuncParamAttr, {static_cast<uint32_t>(attr)});
  return getUniqueDecoration(context, d);
}
const Decoration *Decoration::getFPRoundingMode(SPIRVContext &context,
                                                spv::FPRoundingMode mode) {
  Decoration d = Decoration(spv::Decoration::FPRoundingMode,
                            {static_cast<uint32_t>(mode)});
  return getUniqueDecoration(context, d);
}
const Decoration *Decoration::getFPFastMathMode(SPIRVContext &context,
                                                spv::FPFastMathModeShift mode) {
  Decoration d = Decoration(spv::Decoration::FPFastMathMode,
                            {static_cast<uint32_t>(mode)});
  return getUniqueDecoration(context, d);
}
const Decoration *
Decoration::getLinkageAttributes(SPIRVContext &context, std::string name,
                                 spv::LinkageType linkage_type) {
  llvm::SmallVector<uint32_t, 2> args;
  const auto &vec = string::encodeSPIRVString(name);
  args.insert(args.end(), vec.begin(), vec.end());
  args.push_back(static_cast<uint32_t>(linkage_type));
  Decoration d = Decoration(spv::Decoration::LinkageAttributes, args);
  return getUniqueDecoration(context, d);
}
const Decoration *Decoration::getNoContraction(SPIRVContext &context) {
  Decoration d = Decoration(spv::Decoration::NoContraction);
  return getUniqueDecoration(context, d);
}
const Decoration *Decoration::getInputAttachmentIndex(SPIRVContext &context,
                                                      uint32_t index) {
  Decoration d = Decoration(spv::Decoration::InputAttachmentIndex, {index});
  return getUniqueDecoration(context, d);
}
const Decoration *Decoration::getAlignment(SPIRVContext &context,
                                           uint32_t alignment) {
  Decoration d = Decoration(spv::Decoration::Alignment, {alignment});
  return getUniqueDecoration(context, d);
}
const Decoration *Decoration::getOverrideCoverageNV(SPIRVContext &context) {
  Decoration d = Decoration(spv::Decoration::OverrideCoverageNV);
  return getUniqueDecoration(context, d);
}
const Decoration *Decoration::getPassthroughNV(SPIRVContext &context) {
  Decoration d = Decoration(spv::Decoration::PassthroughNV);
  return getUniqueDecoration(context, d);
}
const Decoration *Decoration::getViewportRelativeNV(SPIRVContext &context) {
  Decoration d = Decoration(spv::Decoration::ViewportRelativeNV);
  return getUniqueDecoration(context, d);
}
const Decoration *
Decoration::getSecondaryViewportRelativeNV(SPIRVContext &context,
                                           uint32_t offset) {
  Decoration d = Decoration(spv::Decoration::SecondaryViewportRelativeNV);
  return getUniqueDecoration(context, d);
}
const Decoration *Decoration::getNonUniformEXT(SPIRVContext &context) {
  Decoration d = Decoration(spv::Decoration::NonUniformEXT);
  return getUniqueDecoration(context, d);
}

const Decoration *Decoration::getHlslCounterBufferGOOGLE(SPIRVContext &context,
                                                         uint32_t id) {
  Decoration d = Decoration(spv::Decoration::HlslCounterBufferGOOGLE, {id});
  return getUniqueDecoration(context, d);
}

const Decoration *
Decoration::getHlslSemanticGOOGLE(SPIRVContext &context,
                                  llvm::StringRef semantic,
                                  llvm ::Optional<uint32_t> member_idx) {
  Decoration d = Decoration(spv::Decoration::HlslSemanticGOOGLE,
                            string::encodeSPIRVString(semantic));
  d.setMemberIndex(member_idx);
  return getUniqueDecoration(context, d);
}

std::vector<uint32_t> Decoration::withTargetId(uint32_t targetId) const {
  std::vector<uint32_t> words;

  // TODO: we are essentially duplicate the work InstBuilder is responsible for.
  // Should figure out a way to unify them.
  words.reserve(3 + args.size() + (memberIndex.hasValue() ? 1 : 0));
  words.push_back(static_cast<uint32_t>(getDecorateOpcode(id, memberIndex)));
  words.push_back(targetId);
  if (memberIndex.hasValue())
    words.push_back(*memberIndex);
  words.push_back(static_cast<uint32_t>(id));
  words.insert(words.end(), args.begin(), args.end());
  words.front() |= static_cast<uint32_t>(words.size()) << 16;

  return words;
}

spv::Op
Decoration::getDecorateOpcode(spv::Decoration decoration,
                              const llvm::Optional<uint32_t> &memberIndex) {
  if (decoration == spv::Decoration::HlslCounterBufferGOOGLE)
    return spv::Op::OpDecorateId;

  if (decoration == spv::Decoration::HlslSemanticGOOGLE)
    return memberIndex.hasValue() ? spv::Op::OpMemberDecorateStringGOOGLE
                                  : spv::Op::OpDecorateStringGOOGLE;

  return memberIndex.hasValue() ? spv::Op::OpMemberDecorate
                                : spv::Op::OpDecorate;
}

} // end namespace spirv
} // end namespace clang
