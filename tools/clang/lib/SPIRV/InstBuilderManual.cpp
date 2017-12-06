//===-- InstBuilder.cpp - SPIR-V instruction builder ------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/SPIRV/InstBuilder.h"
#include "clang/SPIRV/String.h"

#include "llvm/llvm_assert/assert.h"

namespace clang {
namespace spirv {

std::vector<uint32_t> InstBuilder::take() {
  std::vector<uint32_t> result;

  if (TheStatus == Status::Success && Expectation.empty() && !TheInst.empty()) {
    TheInst.front() |= uint32_t(TheInst.size()) << 16;
    result.swap(TheInst);
  }

  return result;
}

InstBuilder &InstBuilder::unaryOp(spv::Op op, uint32_t result_type,
                                  uint32_t result_id, uint32_t operand) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  // TODO: check op range

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(op));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand);

  return *this;
}

InstBuilder &InstBuilder::binaryOp(spv::Op op, uint32_t result_type,
                                   uint32_t result_id, uint32_t lhs,
                                   uint32_t rhs) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  // TODO: check op range

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(op));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(lhs);
  TheInst.emplace_back(rhs);

  return *this;
}

InstBuilder &InstBuilder::opConstant(uint32_t resultType, uint32_t resultId,
                                     uint32_t value) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpConstant));
  TheInst.emplace_back(resultType);
  TheInst.emplace_back(resultId);
  TheInst.emplace_back(value);

  return *this;
}

InstBuilder &InstBuilder::opImageSample(
    uint32_t result_type, uint32_t result_id, uint32_t sampled_image,
    uint32_t coordinate, uint32_t dref,
    llvm::Optional<spv::ImageOperandsMask> image_operands, bool is_explicit,
    bool is_sparse) {
  spv::Op op = spv::Op::Max;
  if (dref) {
    op = is_explicit ? (is_sparse ? spv::Op::OpImageSparseSampleDrefExplicitLod
                                  : spv::Op::OpImageSampleDrefExplicitLod)
                     : (is_sparse ? spv::Op::OpImageSparseSampleDrefImplicitLod
                                  : spv::Op::OpImageSampleDrefImplicitLod);
  } else {
    op = is_explicit ? (is_sparse ? spv::Op::OpImageSparseSampleExplicitLod
                                  : spv::Op::OpImageSampleExplicitLod)
                     : (is_sparse ? spv::Op::OpImageSparseSampleImplicitLod
                                  : spv::Op::OpImageSampleImplicitLod);
  }

  TheInst.emplace_back(static_cast<uint32_t>(op));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(sampled_image);
  TheInst.emplace_back(coordinate);
  if (dref)
    TheInst.emplace_back(dref);
  if (image_operands.hasValue()) {
    const auto &val = image_operands.getValue();
    encodeImageOperands(val);
  }
  return *this;
}

void InstBuilder::encodeString(std::string value) {
  const auto &words = string::encodeSPIRVString(value);
  TheInst.insert(TheInst.end(), words.begin(), words.end());
}

} // end namespace spirv
} // end namespace clang
