//===-- Decoration.h - SPIR-V Decoration ------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_CLANG_SPIRV_DECORATION_H
#define LLVM_CLANG_SPIRV_DECORATION_H

#include <vector>

#include "spirv/1.0/spirv.hpp11"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/SmallVector.h"

namespace clang {
namespace spirv {

class SPIRVContext;

/// \brief SPIR-V Decoration.
///
/// This class defines a unique SPIR-V Decoration.
/// A SPIR-V Decoration includes an identifier defined by the SPIR-V Spec.
/// It also incldues any arguments (32-bit words) needed to define the
/// decoration. If the decoration applies to a structure member, it also
/// includes the index of the member to which the decoration applies.
///
/// The class includes static getXXX(...) functions for getting pointers of any
/// needed decoration. A unique Decoration has a unique pointer (e.g. calling
/// 'getRelaxedPrecision' function will always return the same pointer for the
/// given context).
class Decoration {

public:
  spv::Decoration getValue() const { return id; }
  const llvm::SmallVector<uint32_t, 2> &getArgs() const { return args; }
  llvm::Optional<uint32_t> getMemberIndex() const { return memberIndex; }

  static const Decoration *getRelaxedPrecision(SPIRVContext &ctx);
  static const Decoration *getSpecId(SPIRVContext &ctx, uint32_t id);
  static const Decoration *getBlock(SPIRVContext &ctx);
  static const Decoration *getBufferBlock(SPIRVContext &ctx);
  static const Decoration *getRowMajor(SPIRVContext &ctx, uint32_t member_idx);
  static const Decoration *getColMajor(SPIRVContext &ctx, uint32_t member_idx);
  static const Decoration *getArrayStride(SPIRVContext &ctx, uint32_t stride);
  static const Decoration *getMatrixStride(SPIRVContext &ctx, uint32_t stride,
                                           uint32_t member_idx);
  static const Decoration *getGLSLShared(SPIRVContext &ctx);
  static const Decoration *getGLSLPacked(SPIRVContext &ctx);
  static const Decoration *getCPacked(SPIRVContext &ctx);
  static const Decoration *
  getBuiltIn(SPIRVContext &ctx, spv::BuiltIn builtin,
             llvm::Optional<uint32_t> member_idx = llvm::None);
  static const Decoration *
  getNoPerspective(SPIRVContext &ctx,
                   llvm::Optional<uint32_t> member_idx = llvm::None);
  static const Decoration *
  getFlat(SPIRVContext &ctx, llvm::Optional<uint32_t> member_idx = llvm::None);
  static const Decoration *
  getPatch(SPIRVContext &ctx, llvm::Optional<uint32_t> member_idx = llvm::None);
  static const Decoration *
  getCentroid(SPIRVContext &ctx,
              llvm::Optional<uint32_t> member_idx = llvm::None);

  static const Decoration *
  getSample(SPIRVContext &ctx,
            llvm::Optional<uint32_t> member_idx = llvm::None);
  static const Decoration *getInvariant(SPIRVContext &ctx);
  static const Decoration *getRestrict(SPIRVContext &ctx);
  static const Decoration *getAliased(SPIRVContext &ctx);
  static const Decoration *
  getVolatile(SPIRVContext &ctx,
              llvm::Optional<uint32_t> member_idx = llvm::None);

  static const Decoration *getConstant(SPIRVContext &ctx);
  static const Decoration *
  getCoherent(SPIRVContext &ctx,
              llvm::Optional<uint32_t> member_idx = llvm::None);

  static const Decoration *
  getNonWritable(SPIRVContext &ctx,
                 llvm::Optional<uint32_t> member_idx = llvm::None);
  static const Decoration *
  getNonReadable(SPIRVContext &ctx,
                 llvm::Optional<uint32_t> member_idx = llvm::None);
  static const Decoration *
  getUniform(SPIRVContext &ctx,
             llvm::Optional<uint32_t> member_idx = llvm::None);
  static const Decoration *getSaturatedConversion(SPIRVContext &ctx);
  static const Decoration *
  getStream(SPIRVContext &ctx, uint32_t stream_number,
            llvm::Optional<uint32_t> member_idx = llvm::None);
  static const Decoration *
  getLocation(SPIRVContext &ctx, uint32_t location,
              llvm::Optional<uint32_t> member_idx = llvm::None);
  static const Decoration *
  getComponent(SPIRVContext &ctx, uint32_t component,
               llvm::Optional<uint32_t> member_idx = llvm::None);
  static const Decoration *getIndex(SPIRVContext &ctx, uint32_t index);
  static const Decoration *getBinding(SPIRVContext &ctx,
                                      uint32_t binding_point);
  static const Decoration *getDescriptorSet(SPIRVContext &ctx, uint32_t set);
  static const Decoration *getOffset(SPIRVContext &ctx, uint32_t byte_offset,
                                     uint32_t member_idx);
  static const Decoration *
  getXfbBuffer(SPIRVContext &ctx, uint32_t xfb_buf,
               llvm::Optional<uint32_t> member_idx = llvm::None);
  static const Decoration *
  getXfbStride(SPIRVContext &ctx, uint32_t xfb_stride,
               llvm::Optional<uint32_t> member_idx = llvm::None);
  static const Decoration *
  getFuncParamAttr(SPIRVContext &ctx, spv::FunctionParameterAttribute attr);
  static const Decoration *getFPRoundingMode(SPIRVContext &ctx,
                                             spv::FPRoundingMode mode);
  static const Decoration *getFPFastMathMode(SPIRVContext &ctx,
                                             spv::FPFastMathModeShift mode);
  static const Decoration *getLinkageAttributes(SPIRVContext &ctx,
                                                std::string name,
                                                spv::LinkageType linkage_type);
  static const Decoration *getNoContraction(SPIRVContext &ctx);
  static const Decoration *getInputAttachmentIndex(SPIRVContext &ctx,
                                                   uint32_t index);
  static const Decoration *getAlignment(SPIRVContext &ctx, uint32_t alignment);
  static const Decoration *getOverrideCoverageNV(SPIRVContext &ctx);
  static const Decoration *getPassthroughNV(SPIRVContext &ctx);
  static const Decoration *getViewportRelativeNV(SPIRVContext &ctx);
  static const Decoration *getSecondaryViewportRelativeNV(SPIRVContext &ctx,
                                                          uint32_t offset);

  bool operator==(const Decoration &other) const {
    return id == other.id && args == other.args &&
           memberIndex.hasValue() == other.memberIndex.hasValue() &&
           (!memberIndex.hasValue() ||
            memberIndex.getValue() == other.memberIndex.getValue());
  }

private:
  /// \brief prevent public APIs from creating Decoration objects.
  Decoration(spv::Decoration dec_id, llvm::SmallVector<uint32_t, 2> arg = {},
             llvm::Optional<uint32_t> idx = llvm::None)
      : id(dec_id), args(arg), memberIndex(idx) {}

  /// \brief Sets the index of the structure member to which the decoration
  /// applies.
  void setMemberIndex(llvm::Optional<uint32_t> idx) { memberIndex = idx; }

  /// \brief Returns the unique decoration pointer within the given context.
  static const Decoration *getUniqueDecoration(SPIRVContext &ctx,
                                               const Decoration &d);

private:
  spv::Decoration id;                   ///< Defined by SPIR-V Spec
  llvm::SmallVector<uint32_t, 2> args;  ///< Decoration parameters
  llvm::Optional<uint32_t> memberIndex; ///< Struct member index (if applicable)
};

} // end namespace spirv
} // end namespace clang

#endif
