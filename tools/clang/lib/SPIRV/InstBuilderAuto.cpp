//===-- InstBuilder.cpp - SPIR-V instruction builder ------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// AUTOMATICALLY GENERATED from the SPIR-V JSON grammar:
//   spirv.core.grammar.json.
// DO NOT MODIFY!

#include "clang/SPIRV/InstBuilder.h"

namespace clang {
namespace spirv {

static_assert(spv::Version == 0x00010000 && spv::Revision == 11,
              "Needs to regenerate outdated InstBuilder");

namespace {
inline bool bitEnumContains(spv::ImageOperandsMask bits,
                            spv::ImageOperandsMask bit) {
  return (uint32_t(bits) & uint32_t(bit)) != 0;
}
inline bool bitEnumContains(spv::MemoryAccessMask bits,
                            spv::MemoryAccessMask bit) {
  return (uint32_t(bits) & uint32_t(bit)) != 0;
}
} // namespace

InstBuilder::InstBuilder(WordConsumer consumer)
    : TheConsumer(consumer), TheStatus(Status::Success) {}

void InstBuilder::setConsumer(WordConsumer consumer) { TheConsumer = consumer; }
const WordConsumer &InstBuilder::getConsumer() const { return TheConsumer; }

InstBuilder::Status InstBuilder::x() {
  if (TheConsumer == nullptr)
    return Status::NullConsumer;

  if (TheStatus != Status::Success)
    return TheStatus;

  if (!Expectation.empty()) {
    switch (Expectation.front()) {
    case OperandKind::BuiltIn:
      return Status::ExpectBuiltIn;
    case OperandKind::FPFastMathMode:
      return Status::ExpectFPFastMathMode;
    case OperandKind::FPRoundingMode:
      return Status::ExpectFPRoundingMode;
    case OperandKind::FunctionParameterAttribute:
      return Status::ExpectFunctionParameterAttribute;
    case OperandKind::IdRef:
      return Status::ExpectIdRef;
    case OperandKind::LinkageType:
      return Status::ExpectLinkageType;
    case OperandKind::LiteralInteger:
      return Status::ExpectLiteralInteger;
    case OperandKind::LiteralString:
      return Status::ExpectLiteralString;
    }
  }

  if (!TheInst.empty())
    TheInst.front() |= uint32_t(TheInst.size()) << 16;
  TheConsumer(std::move(TheInst));
  TheInst.clear();

  return TheStatus;
}

void InstBuilder::clear() {
  TheInst.clear();
  Expectation.clear();
  TheStatus = Status::Success;
}

InstBuilder &InstBuilder::opNop() {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(1);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpNop));

  return *this;
}

InstBuilder &InstBuilder::opUndef(uint32_t result_type, uint32_t result_id) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(3);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpUndef));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);

  return *this;
}

InstBuilder &InstBuilder::opSourceContinued(std::string continued_source) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(2);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpSourceContinued));
  encodeString(continued_source);

  return *this;
}

InstBuilder &InstBuilder::opSource(spv::SourceLanguage source_language,
                                   uint32_t version,
                                   llvm::Optional<uint32_t> file,
                                   llvm::Optional<std::string> source) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpSource));
  TheInst.emplace_back(static_cast<uint32_t>(source_language));
  TheInst.emplace_back(version);
  if (file.hasValue()) {
    const auto &val = file.getValue();
    TheInst.emplace_back(val);
  }
  if (source.hasValue()) {
    const auto &val = source.getValue();
    encodeString(val);
  }

  return *this;
}

InstBuilder &InstBuilder::opSourceExtension(std::string extension) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(2);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpSourceExtension));
  encodeString(extension);

  return *this;
}

InstBuilder &InstBuilder::opName(uint32_t target, std::string name) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(3);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpName));
  TheInst.emplace_back(target);
  encodeString(name);

  return *this;
}

InstBuilder &InstBuilder::opMemberName(uint32_t type, uint32_t member,
                                       std::string name) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpMemberName));
  TheInst.emplace_back(type);
  TheInst.emplace_back(member);
  encodeString(name);

  return *this;
}

InstBuilder &InstBuilder::opString(uint32_t result_id, std::string string) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(3);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpString));
  TheInst.emplace_back(result_id);
  encodeString(string);

  return *this;
}

InstBuilder &InstBuilder::opLine(uint32_t file, uint32_t line,
                                 uint32_t column) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpLine));
  TheInst.emplace_back(file);
  TheInst.emplace_back(line);
  TheInst.emplace_back(column);

  return *this;
}

InstBuilder &InstBuilder::opExtension(std::string name) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(2);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpExtension));
  encodeString(name);

  return *this;
}

InstBuilder &InstBuilder::opExtInstImport(uint32_t result_id,
                                          std::string name) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(3);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpExtInstImport));
  TheInst.emplace_back(result_id);
  encodeString(name);

  return *this;
}

InstBuilder &
InstBuilder::opExtInst(uint32_t result_type, uint32_t result_id, uint32_t set,
                       uint32_t instruction,
                       llvm::ArrayRef<uint32_t> operand_1_operand_2_) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(6);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpExtInst));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(set);
  TheInst.emplace_back(instruction);
  TheInst.insert(TheInst.end(), operand_1_operand_2_.begin(),
                 operand_1_operand_2_.end());

  return *this;
}

InstBuilder &InstBuilder::opMemoryModel(spv::AddressingModel addressing_model,
                                        spv::MemoryModel memory_model) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(3);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpMemoryModel));
  TheInst.emplace_back(static_cast<uint32_t>(addressing_model));
  TheInst.emplace_back(static_cast<uint32_t>(memory_model));

  return *this;
}

InstBuilder &InstBuilder::opEntryPoint(spv::ExecutionModel execution_model,
                                       uint32_t entry_point, std::string name,
                                       llvm::ArrayRef<uint32_t> interface) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpEntryPoint));
  TheInst.emplace_back(static_cast<uint32_t>(execution_model));
  TheInst.emplace_back(entry_point);
  encodeString(name);
  TheInst.insert(TheInst.end(), interface.begin(), interface.end());

  return *this;
}

InstBuilder &InstBuilder::opExecutionMode(uint32_t entry_point,
                                          spv::ExecutionMode mode) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(3);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpExecutionMode));
  TheInst.emplace_back(entry_point);
  encodeExecutionMode(mode);

  return *this;
}

InstBuilder &InstBuilder::opCapability(spv::Capability capability) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(2);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpCapability));
  TheInst.emplace_back(static_cast<uint32_t>(capability));

  return *this;
}

InstBuilder &InstBuilder::opTypeVoid(uint32_t result_id) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(2);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpTypeVoid));
  TheInst.emplace_back(result_id);

  return *this;
}

InstBuilder &InstBuilder::opTypeBool(uint32_t result_id) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(2);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpTypeBool));
  TheInst.emplace_back(result_id);

  return *this;
}

InstBuilder &InstBuilder::opTypeInt(uint32_t result_id, uint32_t width,
                                    uint32_t signedness) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpTypeInt));
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(width);
  TheInst.emplace_back(signedness);

  return *this;
}

InstBuilder &InstBuilder::opTypeFloat(uint32_t result_id, uint32_t width) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(3);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpTypeFloat));
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(width);

  return *this;
}

InstBuilder &InstBuilder::opTypeVector(uint32_t result_id,
                                       uint32_t component_type,
                                       uint32_t component_count) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpTypeVector));
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(component_type);
  TheInst.emplace_back(component_count);

  return *this;
}

InstBuilder &InstBuilder::opTypeMatrix(uint32_t result_id, uint32_t column_type,
                                       uint32_t column_count) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpTypeMatrix));
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(column_type);
  TheInst.emplace_back(column_count);

  return *this;
}

InstBuilder &InstBuilder::opTypeImage(
    uint32_t result_id, uint32_t sampled_type, spv::Dim dim, uint32_t depth,
    uint32_t arrayed, uint32_t ms, uint32_t sampled,
    spv::ImageFormat image_format,
    llvm::Optional<spv::AccessQualifier> access_qualifier) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(10);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpTypeImage));
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(sampled_type);
  TheInst.emplace_back(static_cast<uint32_t>(dim));
  TheInst.emplace_back(depth);
  TheInst.emplace_back(arrayed);
  TheInst.emplace_back(ms);
  TheInst.emplace_back(sampled);
  TheInst.emplace_back(static_cast<uint32_t>(image_format));
  if (access_qualifier.hasValue()) {
    const auto &val = access_qualifier.getValue();
    TheInst.emplace_back(static_cast<uint32_t>(val));
  }

  return *this;
}

InstBuilder &InstBuilder::opTypeSampler(uint32_t result_id) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(2);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpTypeSampler));
  TheInst.emplace_back(result_id);

  return *this;
}

InstBuilder &InstBuilder::opTypeSampledImage(uint32_t result_id,
                                             uint32_t image_type) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(3);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpTypeSampledImage));
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(image_type);

  return *this;
}

InstBuilder &InstBuilder::opTypeArray(uint32_t result_id, uint32_t element_type,
                                      uint32_t length) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpTypeArray));
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(element_type);
  TheInst.emplace_back(length);

  return *this;
}

InstBuilder &InstBuilder::opTypeRuntimeArray(uint32_t result_id,
                                             uint32_t element_type) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(3);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpTypeRuntimeArray));
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(element_type);

  return *this;
}

InstBuilder &InstBuilder::opTypeStruct(
    uint32_t result_id, llvm::ArrayRef<uint32_t> member_0_type_member_1_type_) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(3);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpTypeStruct));
  TheInst.emplace_back(result_id);
  TheInst.insert(TheInst.end(), member_0_type_member_1_type_.begin(),
                 member_0_type_member_1_type_.end());

  return *this;
}

InstBuilder &
InstBuilder::opTypeOpaque(uint32_t result_id,
                          std::string the_name_of_the_opaque_type) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(3);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpTypeOpaque));
  TheInst.emplace_back(result_id);
  encodeString(the_name_of_the_opaque_type);

  return *this;
}

InstBuilder &InstBuilder::opTypePointer(uint32_t result_id,
                                        spv::StorageClass storage_class,
                                        uint32_t type) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpTypePointer));
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(static_cast<uint32_t>(storage_class));
  TheInst.emplace_back(type);

  return *this;
}

InstBuilder &InstBuilder::opTypeFunction(
    uint32_t result_id, uint32_t return_type,
    llvm::ArrayRef<uint32_t> parameter_0_type_parameter_1_type_) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpTypeFunction));
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(return_type);
  TheInst.insert(TheInst.end(), parameter_0_type_parameter_1_type_.begin(),
                 parameter_0_type_parameter_1_type_.end());

  return *this;
}

InstBuilder &InstBuilder::opTypeEvent(uint32_t result_id) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(2);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpTypeEvent));
  TheInst.emplace_back(result_id);

  return *this;
}

InstBuilder &InstBuilder::opTypeDeviceEvent(uint32_t result_id) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(2);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpTypeDeviceEvent));
  TheInst.emplace_back(result_id);

  return *this;
}

InstBuilder &InstBuilder::opTypeReserveId(uint32_t result_id) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(2);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpTypeReserveId));
  TheInst.emplace_back(result_id);

  return *this;
}

InstBuilder &InstBuilder::opTypeQueue(uint32_t result_id) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(2);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpTypeQueue));
  TheInst.emplace_back(result_id);

  return *this;
}

InstBuilder &InstBuilder::opTypePipe(uint32_t result_id,
                                     spv::AccessQualifier qualifier) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(3);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpTypePipe));
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(static_cast<uint32_t>(qualifier));

  return *this;
}

InstBuilder &
InstBuilder::opTypeForwardPointer(uint32_t pointer_type,
                                  spv::StorageClass storage_class) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(3);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpTypeForwardPointer));
  TheInst.emplace_back(pointer_type);
  TheInst.emplace_back(static_cast<uint32_t>(storage_class));

  return *this;
}

InstBuilder &InstBuilder::opConstantTrue(uint32_t result_type,
                                         uint32_t result_id) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(3);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpConstantTrue));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);

  return *this;
}

InstBuilder &InstBuilder::opConstantFalse(uint32_t result_type,
                                          uint32_t result_id) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(3);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpConstantFalse));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);

  return *this;
}

InstBuilder &
InstBuilder::opConstantComposite(uint32_t result_type, uint32_t result_id,
                                 llvm::ArrayRef<uint32_t> constituents) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpConstantComposite));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.insert(TheInst.end(), constituents.begin(), constituents.end());

  return *this;
}

InstBuilder &InstBuilder::opConstantSampler(
    uint32_t result_type, uint32_t result_id,
    spv::SamplerAddressingMode sampler_addressing_mode, uint32_t param,
    spv::SamplerFilterMode sampler_filter_mode) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(6);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpConstantSampler));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(static_cast<uint32_t>(sampler_addressing_mode));
  TheInst.emplace_back(param);
  TheInst.emplace_back(static_cast<uint32_t>(sampler_filter_mode));

  return *this;
}

InstBuilder &InstBuilder::opConstantNull(uint32_t result_type,
                                         uint32_t result_id) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(3);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpConstantNull));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);

  return *this;
}

InstBuilder &InstBuilder::opSpecConstantTrue(uint32_t result_type,
                                             uint32_t result_id) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(3);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpSpecConstantTrue));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);

  return *this;
}

InstBuilder &InstBuilder::opSpecConstantFalse(uint32_t result_type,
                                              uint32_t result_id) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(3);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpSpecConstantFalse));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);

  return *this;
}

InstBuilder &
InstBuilder::opSpecConstantComposite(uint32_t result_type, uint32_t result_id,
                                     llvm::ArrayRef<uint32_t> constituents) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpSpecConstantComposite));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.insert(TheInst.end(), constituents.begin(), constituents.end());

  return *this;
}

InstBuilder &InstBuilder::opSpecConstantOp(uint32_t result_type,
                                           uint32_t result_id, spv::Op opcode) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpSpecConstantOp));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(static_cast<uint32_t>(opcode));

  return *this;
}

InstBuilder &InstBuilder::opFunction(uint32_t result_type, uint32_t result_id,
                                     spv::FunctionControlMask function_control,
                                     uint32_t function_type) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpFunction));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(static_cast<uint32_t>(function_control));
  TheInst.emplace_back(function_type);

  return *this;
}

InstBuilder &InstBuilder::opFunctionParameter(uint32_t result_type,
                                              uint32_t result_id) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(3);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpFunctionParameter));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);

  return *this;
}

InstBuilder &InstBuilder::opFunctionEnd() {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(1);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpFunctionEnd));

  return *this;
}

InstBuilder &
InstBuilder::opFunctionCall(uint32_t result_type, uint32_t result_id,
                            uint32_t function,
                            llvm::ArrayRef<uint32_t> argument_0_argument_1_) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpFunctionCall));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(function);
  TheInst.insert(TheInst.end(), argument_0_argument_1_.begin(),
                 argument_0_argument_1_.end());

  return *this;
}

InstBuilder &InstBuilder::opVariable(uint32_t result_type, uint32_t result_id,
                                     spv::StorageClass storage_class,
                                     llvm::Optional<uint32_t> initializer) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpVariable));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(static_cast<uint32_t>(storage_class));
  if (initializer.hasValue()) {
    const auto &val = initializer.getValue();
    TheInst.emplace_back(val);
  }

  return *this;
}

InstBuilder &InstBuilder::opImageTexelPointer(uint32_t result_type,
                                              uint32_t result_id,
                                              uint32_t image,
                                              uint32_t coordinate,
                                              uint32_t sample) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(6);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpImageTexelPointer));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(image);
  TheInst.emplace_back(coordinate);
  TheInst.emplace_back(sample);

  return *this;
}

InstBuilder &
InstBuilder::opLoad(uint32_t result_type, uint32_t result_id, uint32_t pointer,
                    llvm::Optional<spv::MemoryAccessMask> memory_access) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpLoad));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(pointer);
  if (memory_access.hasValue()) {
    const auto &val = memory_access.getValue();
    encodeMemoryAccess(val);
  }

  return *this;
}

InstBuilder &
InstBuilder::opStore(uint32_t pointer, uint32_t object,
                     llvm::Optional<spv::MemoryAccessMask> memory_access) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpStore));
  TheInst.emplace_back(pointer);
  TheInst.emplace_back(object);
  if (memory_access.hasValue()) {
    const auto &val = memory_access.getValue();
    encodeMemoryAccess(val);
  }

  return *this;
}

InstBuilder &
InstBuilder::opCopyMemory(uint32_t target, uint32_t source,
                          llvm::Optional<spv::MemoryAccessMask> memory_access) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpCopyMemory));
  TheInst.emplace_back(target);
  TheInst.emplace_back(source);
  if (memory_access.hasValue()) {
    const auto &val = memory_access.getValue();
    encodeMemoryAccess(val);
  }

  return *this;
}

InstBuilder &InstBuilder::opCopyMemorySized(
    uint32_t target, uint32_t source, uint32_t size,
    llvm::Optional<spv::MemoryAccessMask> memory_access) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpCopyMemorySized));
  TheInst.emplace_back(target);
  TheInst.emplace_back(source);
  TheInst.emplace_back(size);
  if (memory_access.hasValue()) {
    const auto &val = memory_access.getValue();
    encodeMemoryAccess(val);
  }

  return *this;
}

InstBuilder &InstBuilder::opAccessChain(uint32_t result_type,
                                        uint32_t result_id, uint32_t base,
                                        llvm::ArrayRef<uint32_t> indexes) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpAccessChain));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(base);
  TheInst.insert(TheInst.end(), indexes.begin(), indexes.end());

  return *this;
}

InstBuilder &
InstBuilder::opInBoundsAccessChain(uint32_t result_type, uint32_t result_id,
                                   uint32_t base,
                                   llvm::ArrayRef<uint32_t> indexes) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpInBoundsAccessChain));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(base);
  TheInst.insert(TheInst.end(), indexes.begin(), indexes.end());

  return *this;
}

InstBuilder &InstBuilder::opPtrAccessChain(uint32_t result_type,
                                           uint32_t result_id, uint32_t base,
                                           uint32_t element,
                                           llvm::ArrayRef<uint32_t> indexes) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(6);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpPtrAccessChain));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(base);
  TheInst.emplace_back(element);
  TheInst.insert(TheInst.end(), indexes.begin(), indexes.end());

  return *this;
}

InstBuilder &InstBuilder::opArrayLength(uint32_t result_type,
                                        uint32_t result_id, uint32_t structure,
                                        uint32_t array_member) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpArrayLength));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(structure);
  TheInst.emplace_back(array_member);

  return *this;
}

InstBuilder &InstBuilder::opGenericPtrMemSemantics(uint32_t result_type,
                                                   uint32_t result_id,
                                                   uint32_t pointer) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(
      static_cast<uint32_t>(spv::Op::OpGenericPtrMemSemantics));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(pointer);

  return *this;
}

InstBuilder &
InstBuilder::opInBoundsPtrAccessChain(uint32_t result_type, uint32_t result_id,
                                      uint32_t base, uint32_t element,
                                      llvm::ArrayRef<uint32_t> indexes) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(6);
  TheInst.emplace_back(
      static_cast<uint32_t>(spv::Op::OpInBoundsPtrAccessChain));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(base);
  TheInst.emplace_back(element);
  TheInst.insert(TheInst.end(), indexes.begin(), indexes.end());

  return *this;
}

InstBuilder &InstBuilder::opDecorate(uint32_t target,
                                     spv::Decoration decoration) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(3);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpDecorate));
  TheInst.emplace_back(target);
  encodeDecoration(decoration);

  return *this;
}

InstBuilder &InstBuilder::opMemberDecorate(uint32_t structure_type,
                                           uint32_t member,
                                           spv::Decoration decoration) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpMemberDecorate));
  TheInst.emplace_back(structure_type);
  TheInst.emplace_back(member);
  encodeDecoration(decoration);

  return *this;
}

InstBuilder &InstBuilder::opDecorationGroup(uint32_t result_id) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(2);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpDecorationGroup));
  TheInst.emplace_back(result_id);

  return *this;
}

InstBuilder &InstBuilder::opGroupDecorate(uint32_t decoration_group,
                                          llvm::ArrayRef<uint32_t> targets) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(3);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpGroupDecorate));
  TheInst.emplace_back(decoration_group);
  TheInst.insert(TheInst.end(), targets.begin(), targets.end());

  return *this;
}

InstBuilder &InstBuilder::opGroupMemberDecorate(
    uint32_t decoration_group,
    llvm::ArrayRef<std::pair<uint32_t, uint32_t>> targets) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(3);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpGroupMemberDecorate));
  TheInst.emplace_back(decoration_group);
  for (const auto &param : targets) {
    TheInst.emplace_back(param.first);
    TheInst.emplace_back(param.second);
    ;
  }

  return *this;
}

InstBuilder &InstBuilder::opVectorExtractDynamic(uint32_t result_type,
                                                 uint32_t result_id,
                                                 uint32_t vector,
                                                 uint32_t index) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpVectorExtractDynamic));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(vector);
  TheInst.emplace_back(index);

  return *this;
}

InstBuilder &InstBuilder::opVectorInsertDynamic(uint32_t result_type,
                                                uint32_t result_id,
                                                uint32_t vector,
                                                uint32_t component,
                                                uint32_t index) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(6);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpVectorInsertDynamic));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(vector);
  TheInst.emplace_back(component);
  TheInst.emplace_back(index);

  return *this;
}

InstBuilder &InstBuilder::opVectorShuffle(uint32_t result_type,
                                          uint32_t result_id, uint32_t vector_1,
                                          uint32_t vector_2,
                                          llvm::ArrayRef<uint32_t> components) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(6);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpVectorShuffle));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(vector_1);
  TheInst.emplace_back(vector_2);
  for (const auto &param : components) {
    TheInst.emplace_back(param);
    ;
  }

  return *this;
}

InstBuilder &
InstBuilder::opCompositeConstruct(uint32_t result_type, uint32_t result_id,
                                  llvm::ArrayRef<uint32_t> constituents) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpCompositeConstruct));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.insert(TheInst.end(), constituents.begin(), constituents.end());

  return *this;
}

InstBuilder &InstBuilder::opCompositeExtract(uint32_t result_type,
                                             uint32_t result_id,
                                             uint32_t composite,
                                             llvm::ArrayRef<uint32_t> indexes) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpCompositeExtract));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(composite);
  for (const auto &param : indexes) {
    TheInst.emplace_back(param);
    ;
  }

  return *this;
}

InstBuilder &InstBuilder::opCompositeInsert(uint32_t result_type,
                                            uint32_t result_id, uint32_t object,
                                            uint32_t composite,
                                            llvm::ArrayRef<uint32_t> indexes) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(6);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpCompositeInsert));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(object);
  TheInst.emplace_back(composite);
  for (const auto &param : indexes) {
    TheInst.emplace_back(param);
    ;
  }

  return *this;
}

InstBuilder &InstBuilder::opCopyObject(uint32_t result_type, uint32_t result_id,
                                       uint32_t operand) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpCopyObject));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand);

  return *this;
}

InstBuilder &InstBuilder::opTranspose(uint32_t result_type, uint32_t result_id,
                                      uint32_t matrix) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpTranspose));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(matrix);

  return *this;
}

InstBuilder &InstBuilder::opSampledImage(uint32_t result_type,
                                         uint32_t result_id, uint32_t image,
                                         uint32_t sampler) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpSampledImage));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(image);
  TheInst.emplace_back(sampler);

  return *this;
}

InstBuilder &InstBuilder::opImageSampleImplicitLod(
    uint32_t result_type, uint32_t result_id, uint32_t sampled_image,
    uint32_t coordinate,
    llvm::Optional<spv::ImageOperandsMask> image_operands) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(6);
  TheInst.emplace_back(
      static_cast<uint32_t>(spv::Op::OpImageSampleImplicitLod));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(sampled_image);
  TheInst.emplace_back(coordinate);
  if (image_operands.hasValue()) {
    const auto &val = image_operands.getValue();
    encodeImageOperands(val);
  }

  return *this;
}

InstBuilder &InstBuilder::opImageSampleExplicitLod(
    uint32_t result_type, uint32_t result_id, uint32_t sampled_image,
    uint32_t coordinate, spv::ImageOperandsMask image_operands) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(6);
  TheInst.emplace_back(
      static_cast<uint32_t>(spv::Op::OpImageSampleExplicitLod));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(sampled_image);
  TheInst.emplace_back(coordinate);
  encodeImageOperands(image_operands);

  return *this;
}

InstBuilder &InstBuilder::opImageSampleDrefImplicitLod(
    uint32_t result_type, uint32_t result_id, uint32_t sampled_image,
    uint32_t coordinate, uint32_t dref,
    llvm::Optional<spv::ImageOperandsMask> image_operands) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(7);
  TheInst.emplace_back(
      static_cast<uint32_t>(spv::Op::OpImageSampleDrefImplicitLod));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(sampled_image);
  TheInst.emplace_back(coordinate);
  TheInst.emplace_back(dref);
  if (image_operands.hasValue()) {
    const auto &val = image_operands.getValue();
    encodeImageOperands(val);
  }

  return *this;
}

InstBuilder &InstBuilder::opImageSampleDrefExplicitLod(
    uint32_t result_type, uint32_t result_id, uint32_t sampled_image,
    uint32_t coordinate, uint32_t dref, spv::ImageOperandsMask image_operands) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(7);
  TheInst.emplace_back(
      static_cast<uint32_t>(spv::Op::OpImageSampleDrefExplicitLod));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(sampled_image);
  TheInst.emplace_back(coordinate);
  TheInst.emplace_back(dref);
  encodeImageOperands(image_operands);

  return *this;
}

InstBuilder &InstBuilder::opImageSampleProjImplicitLod(
    uint32_t result_type, uint32_t result_id, uint32_t sampled_image,
    uint32_t coordinate,
    llvm::Optional<spv::ImageOperandsMask> image_operands) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(6);
  TheInst.emplace_back(
      static_cast<uint32_t>(spv::Op::OpImageSampleProjImplicitLod));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(sampled_image);
  TheInst.emplace_back(coordinate);
  if (image_operands.hasValue()) {
    const auto &val = image_operands.getValue();
    encodeImageOperands(val);
  }

  return *this;
}

InstBuilder &InstBuilder::opImageSampleProjExplicitLod(
    uint32_t result_type, uint32_t result_id, uint32_t sampled_image,
    uint32_t coordinate, spv::ImageOperandsMask image_operands) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(6);
  TheInst.emplace_back(
      static_cast<uint32_t>(spv::Op::OpImageSampleProjExplicitLod));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(sampled_image);
  TheInst.emplace_back(coordinate);
  encodeImageOperands(image_operands);

  return *this;
}

InstBuilder &InstBuilder::opImageSampleProjDrefImplicitLod(
    uint32_t result_type, uint32_t result_id, uint32_t sampled_image,
    uint32_t coordinate, uint32_t dref,
    llvm::Optional<spv::ImageOperandsMask> image_operands) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(7);
  TheInst.emplace_back(
      static_cast<uint32_t>(spv::Op::OpImageSampleProjDrefImplicitLod));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(sampled_image);
  TheInst.emplace_back(coordinate);
  TheInst.emplace_back(dref);
  if (image_operands.hasValue()) {
    const auto &val = image_operands.getValue();
    encodeImageOperands(val);
  }

  return *this;
}

InstBuilder &InstBuilder::opImageSampleProjDrefExplicitLod(
    uint32_t result_type, uint32_t result_id, uint32_t sampled_image,
    uint32_t coordinate, uint32_t dref, spv::ImageOperandsMask image_operands) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(7);
  TheInst.emplace_back(
      static_cast<uint32_t>(spv::Op::OpImageSampleProjDrefExplicitLod));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(sampled_image);
  TheInst.emplace_back(coordinate);
  TheInst.emplace_back(dref);
  encodeImageOperands(image_operands);

  return *this;
}

InstBuilder &InstBuilder::opImageFetch(
    uint32_t result_type, uint32_t result_id, uint32_t image,
    uint32_t coordinate,
    llvm::Optional<spv::ImageOperandsMask> image_operands) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(6);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpImageFetch));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(image);
  TheInst.emplace_back(coordinate);
  if (image_operands.hasValue()) {
    const auto &val = image_operands.getValue();
    encodeImageOperands(val);
  }

  return *this;
}

InstBuilder &InstBuilder::opImageGather(
    uint32_t result_type, uint32_t result_id, uint32_t sampled_image,
    uint32_t coordinate, uint32_t component,
    llvm::Optional<spv::ImageOperandsMask> image_operands) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(7);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpImageGather));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(sampled_image);
  TheInst.emplace_back(coordinate);
  TheInst.emplace_back(component);
  if (image_operands.hasValue()) {
    const auto &val = image_operands.getValue();
    encodeImageOperands(val);
  }

  return *this;
}

InstBuilder &InstBuilder::opImageDrefGather(
    uint32_t result_type, uint32_t result_id, uint32_t sampled_image,
    uint32_t coordinate, uint32_t dref,
    llvm::Optional<spv::ImageOperandsMask> image_operands) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(7);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpImageDrefGather));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(sampled_image);
  TheInst.emplace_back(coordinate);
  TheInst.emplace_back(dref);
  if (image_operands.hasValue()) {
    const auto &val = image_operands.getValue();
    encodeImageOperands(val);
  }

  return *this;
}

InstBuilder &InstBuilder::opImageRead(
    uint32_t result_type, uint32_t result_id, uint32_t image,
    uint32_t coordinate,
    llvm::Optional<spv::ImageOperandsMask> image_operands) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(6);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpImageRead));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(image);
  TheInst.emplace_back(coordinate);
  if (image_operands.hasValue()) {
    const auto &val = image_operands.getValue();
    encodeImageOperands(val);
  }

  return *this;
}

InstBuilder &InstBuilder::opImageWrite(
    uint32_t image, uint32_t coordinate, uint32_t texel,
    llvm::Optional<spv::ImageOperandsMask> image_operands) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpImageWrite));
  TheInst.emplace_back(image);
  TheInst.emplace_back(coordinate);
  TheInst.emplace_back(texel);
  if (image_operands.hasValue()) {
    const auto &val = image_operands.getValue();
    encodeImageOperands(val);
  }

  return *this;
}

InstBuilder &InstBuilder::opImage(uint32_t result_type, uint32_t result_id,
                                  uint32_t sampled_image) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpImage));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(sampled_image);

  return *this;
}

InstBuilder &InstBuilder::opImageQueryFormat(uint32_t result_type,
                                             uint32_t result_id,
                                             uint32_t image) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpImageQueryFormat));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(image);

  return *this;
}

InstBuilder &InstBuilder::opImageQueryOrder(uint32_t result_type,
                                            uint32_t result_id,
                                            uint32_t image) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpImageQueryOrder));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(image);

  return *this;
}

InstBuilder &InstBuilder::opImageQuerySizeLod(uint32_t result_type,
                                              uint32_t result_id,
                                              uint32_t image,
                                              uint32_t level_of_detail) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpImageQuerySizeLod));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(image);
  TheInst.emplace_back(level_of_detail);

  return *this;
}

InstBuilder &InstBuilder::opImageQuerySize(uint32_t result_type,
                                           uint32_t result_id, uint32_t image) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpImageQuerySize));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(image);

  return *this;
}

InstBuilder &InstBuilder::opImageQueryLod(uint32_t result_type,
                                          uint32_t result_id,
                                          uint32_t sampled_image,
                                          uint32_t coordinate) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpImageQueryLod));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(sampled_image);
  TheInst.emplace_back(coordinate);

  return *this;
}

InstBuilder &InstBuilder::opImageQueryLevels(uint32_t result_type,
                                             uint32_t result_id,
                                             uint32_t image) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpImageQueryLevels));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(image);

  return *this;
}

InstBuilder &InstBuilder::opImageQuerySamples(uint32_t result_type,
                                              uint32_t result_id,
                                              uint32_t image) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpImageQuerySamples));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(image);

  return *this;
}

InstBuilder &InstBuilder::opConvertFToU(uint32_t result_type,
                                        uint32_t result_id,
                                        uint32_t float_value) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpConvertFToU));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(float_value);

  return *this;
}

InstBuilder &InstBuilder::opConvertFToS(uint32_t result_type,
                                        uint32_t result_id,
                                        uint32_t float_value) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpConvertFToS));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(float_value);

  return *this;
}

InstBuilder &InstBuilder::opConvertSToF(uint32_t result_type,
                                        uint32_t result_id,
                                        uint32_t signed_value) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpConvertSToF));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(signed_value);

  return *this;
}

InstBuilder &InstBuilder::opConvertUToF(uint32_t result_type,
                                        uint32_t result_id,
                                        uint32_t unsigned_value) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpConvertUToF));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(unsigned_value);

  return *this;
}

InstBuilder &InstBuilder::opUConvert(uint32_t result_type, uint32_t result_id,
                                     uint32_t unsigned_value) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpUConvert));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(unsigned_value);

  return *this;
}

InstBuilder &InstBuilder::opSConvert(uint32_t result_type, uint32_t result_id,
                                     uint32_t signed_value) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpSConvert));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(signed_value);

  return *this;
}

InstBuilder &InstBuilder::opFConvert(uint32_t result_type, uint32_t result_id,
                                     uint32_t float_value) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpFConvert));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(float_value);

  return *this;
}

InstBuilder &InstBuilder::opQuantizeToF16(uint32_t result_type,
                                          uint32_t result_id, uint32_t value) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpQuantizeToF16));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(value);

  return *this;
}

InstBuilder &InstBuilder::opConvertPtrToU(uint32_t result_type,
                                          uint32_t result_id,
                                          uint32_t pointer) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpConvertPtrToU));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(pointer);

  return *this;
}

InstBuilder &InstBuilder::opSatConvertSToU(uint32_t result_type,
                                           uint32_t result_id,
                                           uint32_t signed_value) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpSatConvertSToU));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(signed_value);

  return *this;
}

InstBuilder &InstBuilder::opSatConvertUToS(uint32_t result_type,
                                           uint32_t result_id,
                                           uint32_t unsigned_value) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpSatConvertUToS));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(unsigned_value);

  return *this;
}

InstBuilder &InstBuilder::opConvertUToPtr(uint32_t result_type,
                                          uint32_t result_id,
                                          uint32_t integer_value) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpConvertUToPtr));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(integer_value);

  return *this;
}

InstBuilder &InstBuilder::opPtrCastToGeneric(uint32_t result_type,
                                             uint32_t result_id,
                                             uint32_t pointer) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpPtrCastToGeneric));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(pointer);

  return *this;
}

InstBuilder &InstBuilder::opGenericCastToPtr(uint32_t result_type,
                                             uint32_t result_id,
                                             uint32_t pointer) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpGenericCastToPtr));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(pointer);

  return *this;
}

InstBuilder &
InstBuilder::opGenericCastToPtrExplicit(uint32_t result_type,
                                        uint32_t result_id, uint32_t pointer,
                                        spv::StorageClass storage) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(
      static_cast<uint32_t>(spv::Op::OpGenericCastToPtrExplicit));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(pointer);
  TheInst.emplace_back(static_cast<uint32_t>(storage));

  return *this;
}

InstBuilder &InstBuilder::opBitcast(uint32_t result_type, uint32_t result_id,
                                    uint32_t operand) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpBitcast));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand);

  return *this;
}

InstBuilder &InstBuilder::opSNegate(uint32_t result_type, uint32_t result_id,
                                    uint32_t operand) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpSNegate));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand);

  return *this;
}

InstBuilder &InstBuilder::opFNegate(uint32_t result_type, uint32_t result_id,
                                    uint32_t operand) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpFNegate));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand);

  return *this;
}

InstBuilder &InstBuilder::opIAdd(uint32_t result_type, uint32_t result_id,
                                 uint32_t operand_1, uint32_t operand_2) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpIAdd));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand_1);
  TheInst.emplace_back(operand_2);

  return *this;
}

InstBuilder &InstBuilder::opFAdd(uint32_t result_type, uint32_t result_id,
                                 uint32_t operand_1, uint32_t operand_2) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpFAdd));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand_1);
  TheInst.emplace_back(operand_2);

  return *this;
}

InstBuilder &InstBuilder::opISub(uint32_t result_type, uint32_t result_id,
                                 uint32_t operand_1, uint32_t operand_2) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpISub));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand_1);
  TheInst.emplace_back(operand_2);

  return *this;
}

InstBuilder &InstBuilder::opFSub(uint32_t result_type, uint32_t result_id,
                                 uint32_t operand_1, uint32_t operand_2) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpFSub));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand_1);
  TheInst.emplace_back(operand_2);

  return *this;
}

InstBuilder &InstBuilder::opIMul(uint32_t result_type, uint32_t result_id,
                                 uint32_t operand_1, uint32_t operand_2) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpIMul));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand_1);
  TheInst.emplace_back(operand_2);

  return *this;
}

InstBuilder &InstBuilder::opFMul(uint32_t result_type, uint32_t result_id,
                                 uint32_t operand_1, uint32_t operand_2) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpFMul));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand_1);
  TheInst.emplace_back(operand_2);

  return *this;
}

InstBuilder &InstBuilder::opUDiv(uint32_t result_type, uint32_t result_id,
                                 uint32_t operand_1, uint32_t operand_2) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpUDiv));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand_1);
  TheInst.emplace_back(operand_2);

  return *this;
}

InstBuilder &InstBuilder::opSDiv(uint32_t result_type, uint32_t result_id,
                                 uint32_t operand_1, uint32_t operand_2) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpSDiv));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand_1);
  TheInst.emplace_back(operand_2);

  return *this;
}

InstBuilder &InstBuilder::opFDiv(uint32_t result_type, uint32_t result_id,
                                 uint32_t operand_1, uint32_t operand_2) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpFDiv));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand_1);
  TheInst.emplace_back(operand_2);

  return *this;
}

InstBuilder &InstBuilder::opUMod(uint32_t result_type, uint32_t result_id,
                                 uint32_t operand_1, uint32_t operand_2) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpUMod));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand_1);
  TheInst.emplace_back(operand_2);

  return *this;
}

InstBuilder &InstBuilder::opSRem(uint32_t result_type, uint32_t result_id,
                                 uint32_t operand_1, uint32_t operand_2) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpSRem));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand_1);
  TheInst.emplace_back(operand_2);

  return *this;
}

InstBuilder &InstBuilder::opSMod(uint32_t result_type, uint32_t result_id,
                                 uint32_t operand_1, uint32_t operand_2) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpSMod));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand_1);
  TheInst.emplace_back(operand_2);

  return *this;
}

InstBuilder &InstBuilder::opFRem(uint32_t result_type, uint32_t result_id,
                                 uint32_t operand_1, uint32_t operand_2) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpFRem));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand_1);
  TheInst.emplace_back(operand_2);

  return *this;
}

InstBuilder &InstBuilder::opFMod(uint32_t result_type, uint32_t result_id,
                                 uint32_t operand_1, uint32_t operand_2) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpFMod));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand_1);
  TheInst.emplace_back(operand_2);

  return *this;
}

InstBuilder &InstBuilder::opVectorTimesScalar(uint32_t result_type,
                                              uint32_t result_id,
                                              uint32_t vector,
                                              uint32_t scalar) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpVectorTimesScalar));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(vector);
  TheInst.emplace_back(scalar);

  return *this;
}

InstBuilder &InstBuilder::opMatrixTimesScalar(uint32_t result_type,
                                              uint32_t result_id,
                                              uint32_t matrix,
                                              uint32_t scalar) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpMatrixTimesScalar));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(matrix);
  TheInst.emplace_back(scalar);

  return *this;
}

InstBuilder &InstBuilder::opVectorTimesMatrix(uint32_t result_type,
                                              uint32_t result_id,
                                              uint32_t vector,
                                              uint32_t matrix) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpVectorTimesMatrix));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(vector);
  TheInst.emplace_back(matrix);

  return *this;
}

InstBuilder &InstBuilder::opMatrixTimesVector(uint32_t result_type,
                                              uint32_t result_id,
                                              uint32_t matrix,
                                              uint32_t vector) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpMatrixTimesVector));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(matrix);
  TheInst.emplace_back(vector);

  return *this;
}

InstBuilder &InstBuilder::opMatrixTimesMatrix(uint32_t result_type,
                                              uint32_t result_id,
                                              uint32_t left_matrix,
                                              uint32_t right_matrix) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpMatrixTimesMatrix));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(left_matrix);
  TheInst.emplace_back(right_matrix);

  return *this;
}

InstBuilder &InstBuilder::opOuterProduct(uint32_t result_type,
                                         uint32_t result_id, uint32_t vector_1,
                                         uint32_t vector_2) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpOuterProduct));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(vector_1);
  TheInst.emplace_back(vector_2);

  return *this;
}

InstBuilder &InstBuilder::opDot(uint32_t result_type, uint32_t result_id,
                                uint32_t vector_1, uint32_t vector_2) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpDot));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(vector_1);
  TheInst.emplace_back(vector_2);

  return *this;
}

InstBuilder &InstBuilder::opIAddCarry(uint32_t result_type, uint32_t result_id,
                                      uint32_t operand_1, uint32_t operand_2) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpIAddCarry));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand_1);
  TheInst.emplace_back(operand_2);

  return *this;
}

InstBuilder &InstBuilder::opISubBorrow(uint32_t result_type, uint32_t result_id,
                                       uint32_t operand_1, uint32_t operand_2) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpISubBorrow));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand_1);
  TheInst.emplace_back(operand_2);

  return *this;
}

InstBuilder &InstBuilder::opUMulExtended(uint32_t result_type,
                                         uint32_t result_id, uint32_t operand_1,
                                         uint32_t operand_2) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpUMulExtended));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand_1);
  TheInst.emplace_back(operand_2);

  return *this;
}

InstBuilder &InstBuilder::opSMulExtended(uint32_t result_type,
                                         uint32_t result_id, uint32_t operand_1,
                                         uint32_t operand_2) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpSMulExtended));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand_1);
  TheInst.emplace_back(operand_2);

  return *this;
}

InstBuilder &InstBuilder::opAny(uint32_t result_type, uint32_t result_id,
                                uint32_t vector) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpAny));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(vector);

  return *this;
}

InstBuilder &InstBuilder::opAll(uint32_t result_type, uint32_t result_id,
                                uint32_t vector) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpAll));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(vector);

  return *this;
}

InstBuilder &InstBuilder::opIsNan(uint32_t result_type, uint32_t result_id,
                                  uint32_t x) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpIsNan));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(x);

  return *this;
}

InstBuilder &InstBuilder::opIsInf(uint32_t result_type, uint32_t result_id,
                                  uint32_t x) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpIsInf));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(x);

  return *this;
}

InstBuilder &InstBuilder::opIsFinite(uint32_t result_type, uint32_t result_id,
                                     uint32_t x) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpIsFinite));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(x);

  return *this;
}

InstBuilder &InstBuilder::opIsNormal(uint32_t result_type, uint32_t result_id,
                                     uint32_t x) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpIsNormal));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(x);

  return *this;
}

InstBuilder &InstBuilder::opSignBitSet(uint32_t result_type, uint32_t result_id,
                                       uint32_t x) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpSignBitSet));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(x);

  return *this;
}

InstBuilder &InstBuilder::opLessOrGreater(uint32_t result_type,
                                          uint32_t result_id, uint32_t x,
                                          uint32_t y) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpLessOrGreater));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(x);
  TheInst.emplace_back(y);

  return *this;
}

InstBuilder &InstBuilder::opOrdered(uint32_t result_type, uint32_t result_id,
                                    uint32_t x, uint32_t y) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpOrdered));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(x);
  TheInst.emplace_back(y);

  return *this;
}

InstBuilder &InstBuilder::opUnordered(uint32_t result_type, uint32_t result_id,
                                      uint32_t x, uint32_t y) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpUnordered));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(x);
  TheInst.emplace_back(y);

  return *this;
}

InstBuilder &InstBuilder::opLogicalEqual(uint32_t result_type,
                                         uint32_t result_id, uint32_t operand_1,
                                         uint32_t operand_2) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpLogicalEqual));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand_1);
  TheInst.emplace_back(operand_2);

  return *this;
}

InstBuilder &InstBuilder::opLogicalNotEqual(uint32_t result_type,
                                            uint32_t result_id,
                                            uint32_t operand_1,
                                            uint32_t operand_2) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpLogicalNotEqual));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand_1);
  TheInst.emplace_back(operand_2);

  return *this;
}

InstBuilder &InstBuilder::opLogicalOr(uint32_t result_type, uint32_t result_id,
                                      uint32_t operand_1, uint32_t operand_2) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpLogicalOr));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand_1);
  TheInst.emplace_back(operand_2);

  return *this;
}

InstBuilder &InstBuilder::opLogicalAnd(uint32_t result_type, uint32_t result_id,
                                       uint32_t operand_1, uint32_t operand_2) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpLogicalAnd));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand_1);
  TheInst.emplace_back(operand_2);

  return *this;
}

InstBuilder &InstBuilder::opLogicalNot(uint32_t result_type, uint32_t result_id,
                                       uint32_t operand) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpLogicalNot));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand);

  return *this;
}

InstBuilder &InstBuilder::opSelect(uint32_t result_type, uint32_t result_id,
                                   uint32_t condition, uint32_t object_1,
                                   uint32_t object_2) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(6);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpSelect));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(condition);
  TheInst.emplace_back(object_1);
  TheInst.emplace_back(object_2);

  return *this;
}

InstBuilder &InstBuilder::opIEqual(uint32_t result_type, uint32_t result_id,
                                   uint32_t operand_1, uint32_t operand_2) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpIEqual));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand_1);
  TheInst.emplace_back(operand_2);

  return *this;
}

InstBuilder &InstBuilder::opINotEqual(uint32_t result_type, uint32_t result_id,
                                      uint32_t operand_1, uint32_t operand_2) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpINotEqual));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand_1);
  TheInst.emplace_back(operand_2);

  return *this;
}

InstBuilder &InstBuilder::opUGreaterThan(uint32_t result_type,
                                         uint32_t result_id, uint32_t operand_1,
                                         uint32_t operand_2) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpUGreaterThan));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand_1);
  TheInst.emplace_back(operand_2);

  return *this;
}

InstBuilder &InstBuilder::opSGreaterThan(uint32_t result_type,
                                         uint32_t result_id, uint32_t operand_1,
                                         uint32_t operand_2) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpSGreaterThan));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand_1);
  TheInst.emplace_back(operand_2);

  return *this;
}

InstBuilder &InstBuilder::opUGreaterThanEqual(uint32_t result_type,
                                              uint32_t result_id,
                                              uint32_t operand_1,
                                              uint32_t operand_2) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpUGreaterThanEqual));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand_1);
  TheInst.emplace_back(operand_2);

  return *this;
}

InstBuilder &InstBuilder::opSGreaterThanEqual(uint32_t result_type,
                                              uint32_t result_id,
                                              uint32_t operand_1,
                                              uint32_t operand_2) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpSGreaterThanEqual));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand_1);
  TheInst.emplace_back(operand_2);

  return *this;
}

InstBuilder &InstBuilder::opULessThan(uint32_t result_type, uint32_t result_id,
                                      uint32_t operand_1, uint32_t operand_2) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpULessThan));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand_1);
  TheInst.emplace_back(operand_2);

  return *this;
}

InstBuilder &InstBuilder::opSLessThan(uint32_t result_type, uint32_t result_id,
                                      uint32_t operand_1, uint32_t operand_2) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpSLessThan));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand_1);
  TheInst.emplace_back(operand_2);

  return *this;
}

InstBuilder &InstBuilder::opULessThanEqual(uint32_t result_type,
                                           uint32_t result_id,
                                           uint32_t operand_1,
                                           uint32_t operand_2) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpULessThanEqual));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand_1);
  TheInst.emplace_back(operand_2);

  return *this;
}

InstBuilder &InstBuilder::opSLessThanEqual(uint32_t result_type,
                                           uint32_t result_id,
                                           uint32_t operand_1,
                                           uint32_t operand_2) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpSLessThanEqual));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand_1);
  TheInst.emplace_back(operand_2);

  return *this;
}

InstBuilder &InstBuilder::opFOrdEqual(uint32_t result_type, uint32_t result_id,
                                      uint32_t operand_1, uint32_t operand_2) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpFOrdEqual));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand_1);
  TheInst.emplace_back(operand_2);

  return *this;
}

InstBuilder &InstBuilder::opFUnordEqual(uint32_t result_type,
                                        uint32_t result_id, uint32_t operand_1,
                                        uint32_t operand_2) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpFUnordEqual));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand_1);
  TheInst.emplace_back(operand_2);

  return *this;
}

InstBuilder &InstBuilder::opFOrdNotEqual(uint32_t result_type,
                                         uint32_t result_id, uint32_t operand_1,
                                         uint32_t operand_2) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpFOrdNotEqual));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand_1);
  TheInst.emplace_back(operand_2);

  return *this;
}

InstBuilder &InstBuilder::opFUnordNotEqual(uint32_t result_type,
                                           uint32_t result_id,
                                           uint32_t operand_1,
                                           uint32_t operand_2) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpFUnordNotEqual));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand_1);
  TheInst.emplace_back(operand_2);

  return *this;
}

InstBuilder &InstBuilder::opFOrdLessThan(uint32_t result_type,
                                         uint32_t result_id, uint32_t operand_1,
                                         uint32_t operand_2) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpFOrdLessThan));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand_1);
  TheInst.emplace_back(operand_2);

  return *this;
}

InstBuilder &InstBuilder::opFUnordLessThan(uint32_t result_type,
                                           uint32_t result_id,
                                           uint32_t operand_1,
                                           uint32_t operand_2) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpFUnordLessThan));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand_1);
  TheInst.emplace_back(operand_2);

  return *this;
}

InstBuilder &InstBuilder::opFOrdGreaterThan(uint32_t result_type,
                                            uint32_t result_id,
                                            uint32_t operand_1,
                                            uint32_t operand_2) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpFOrdGreaterThan));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand_1);
  TheInst.emplace_back(operand_2);

  return *this;
}

InstBuilder &InstBuilder::opFUnordGreaterThan(uint32_t result_type,
                                              uint32_t result_id,
                                              uint32_t operand_1,
                                              uint32_t operand_2) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpFUnordGreaterThan));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand_1);
  TheInst.emplace_back(operand_2);

  return *this;
}

InstBuilder &InstBuilder::opFOrdLessThanEqual(uint32_t result_type,
                                              uint32_t result_id,
                                              uint32_t operand_1,
                                              uint32_t operand_2) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpFOrdLessThanEqual));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand_1);
  TheInst.emplace_back(operand_2);

  return *this;
}

InstBuilder &InstBuilder::opFUnordLessThanEqual(uint32_t result_type,
                                                uint32_t result_id,
                                                uint32_t operand_1,
                                                uint32_t operand_2) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpFUnordLessThanEqual));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand_1);
  TheInst.emplace_back(operand_2);

  return *this;
}

InstBuilder &InstBuilder::opFOrdGreaterThanEqual(uint32_t result_type,
                                                 uint32_t result_id,
                                                 uint32_t operand_1,
                                                 uint32_t operand_2) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpFOrdGreaterThanEqual));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand_1);
  TheInst.emplace_back(operand_2);

  return *this;
}

InstBuilder &InstBuilder::opFUnordGreaterThanEqual(uint32_t result_type,
                                                   uint32_t result_id,
                                                   uint32_t operand_1,
                                                   uint32_t operand_2) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(
      static_cast<uint32_t>(spv::Op::OpFUnordGreaterThanEqual));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand_1);
  TheInst.emplace_back(operand_2);

  return *this;
}

InstBuilder &InstBuilder::opShiftRightLogical(uint32_t result_type,
                                              uint32_t result_id, uint32_t base,
                                              uint32_t shift) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpShiftRightLogical));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(base);
  TheInst.emplace_back(shift);

  return *this;
}

InstBuilder &InstBuilder::opShiftRightArithmetic(uint32_t result_type,
                                                 uint32_t result_id,
                                                 uint32_t base,
                                                 uint32_t shift) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpShiftRightArithmetic));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(base);
  TheInst.emplace_back(shift);

  return *this;
}

InstBuilder &InstBuilder::opShiftLeftLogical(uint32_t result_type,
                                             uint32_t result_id, uint32_t base,
                                             uint32_t shift) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpShiftLeftLogical));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(base);
  TheInst.emplace_back(shift);

  return *this;
}

InstBuilder &InstBuilder::opBitwiseOr(uint32_t result_type, uint32_t result_id,
                                      uint32_t operand_1, uint32_t operand_2) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpBitwiseOr));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand_1);
  TheInst.emplace_back(operand_2);

  return *this;
}

InstBuilder &InstBuilder::opBitwiseXor(uint32_t result_type, uint32_t result_id,
                                       uint32_t operand_1, uint32_t operand_2) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpBitwiseXor));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand_1);
  TheInst.emplace_back(operand_2);

  return *this;
}

InstBuilder &InstBuilder::opBitwiseAnd(uint32_t result_type, uint32_t result_id,
                                       uint32_t operand_1, uint32_t operand_2) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpBitwiseAnd));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand_1);
  TheInst.emplace_back(operand_2);

  return *this;
}

InstBuilder &InstBuilder::opNot(uint32_t result_type, uint32_t result_id,
                                uint32_t operand) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpNot));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(operand);

  return *this;
}

InstBuilder &InstBuilder::opBitFieldInsert(uint32_t result_type,
                                           uint32_t result_id, uint32_t base,
                                           uint32_t insert, uint32_t offset,
                                           uint32_t count) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(7);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpBitFieldInsert));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(base);
  TheInst.emplace_back(insert);
  TheInst.emplace_back(offset);
  TheInst.emplace_back(count);

  return *this;
}

InstBuilder &InstBuilder::opBitFieldSExtract(uint32_t result_type,
                                             uint32_t result_id, uint32_t base,
                                             uint32_t offset, uint32_t count) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(6);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpBitFieldSExtract));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(base);
  TheInst.emplace_back(offset);
  TheInst.emplace_back(count);

  return *this;
}

InstBuilder &InstBuilder::opBitFieldUExtract(uint32_t result_type,
                                             uint32_t result_id, uint32_t base,
                                             uint32_t offset, uint32_t count) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(6);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpBitFieldUExtract));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(base);
  TheInst.emplace_back(offset);
  TheInst.emplace_back(count);

  return *this;
}

InstBuilder &InstBuilder::opBitReverse(uint32_t result_type, uint32_t result_id,
                                       uint32_t base) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpBitReverse));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(base);

  return *this;
}

InstBuilder &InstBuilder::opBitCount(uint32_t result_type, uint32_t result_id,
                                     uint32_t base) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpBitCount));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(base);

  return *this;
}

InstBuilder &InstBuilder::opDPdx(uint32_t result_type, uint32_t result_id,
                                 uint32_t p) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpDPdx));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(p);

  return *this;
}

InstBuilder &InstBuilder::opDPdy(uint32_t result_type, uint32_t result_id,
                                 uint32_t p) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpDPdy));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(p);

  return *this;
}

InstBuilder &InstBuilder::opFwidth(uint32_t result_type, uint32_t result_id,
                                   uint32_t p) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpFwidth));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(p);

  return *this;
}

InstBuilder &InstBuilder::opDPdxFine(uint32_t result_type, uint32_t result_id,
                                     uint32_t p) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpDPdxFine));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(p);

  return *this;
}

InstBuilder &InstBuilder::opDPdyFine(uint32_t result_type, uint32_t result_id,
                                     uint32_t p) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpDPdyFine));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(p);

  return *this;
}

InstBuilder &InstBuilder::opFwidthFine(uint32_t result_type, uint32_t result_id,
                                       uint32_t p) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpFwidthFine));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(p);

  return *this;
}

InstBuilder &InstBuilder::opDPdxCoarse(uint32_t result_type, uint32_t result_id,
                                       uint32_t p) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpDPdxCoarse));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(p);

  return *this;
}

InstBuilder &InstBuilder::opDPdyCoarse(uint32_t result_type, uint32_t result_id,
                                       uint32_t p) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpDPdyCoarse));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(p);

  return *this;
}

InstBuilder &InstBuilder::opFwidthCoarse(uint32_t result_type,
                                         uint32_t result_id, uint32_t p) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpFwidthCoarse));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(p);

  return *this;
}

InstBuilder &InstBuilder::opEmitVertex() {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(1);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpEmitVertex));

  return *this;
}

InstBuilder &InstBuilder::opEndPrimitive() {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(1);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpEndPrimitive));

  return *this;
}

InstBuilder &InstBuilder::opEmitStreamVertex(uint32_t stream) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(2);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpEmitStreamVertex));
  TheInst.emplace_back(stream);

  return *this;
}

InstBuilder &InstBuilder::opEndStreamPrimitive(uint32_t stream) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(2);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpEndStreamPrimitive));
  TheInst.emplace_back(stream);

  return *this;
}

InstBuilder &InstBuilder::opControlBarrier(uint32_t execution, uint32_t memory,
                                           uint32_t semantics) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpControlBarrier));
  TheInst.emplace_back(execution);
  TheInst.emplace_back(memory);
  TheInst.emplace_back(semantics);

  return *this;
}

InstBuilder &InstBuilder::opMemoryBarrier(uint32_t memory, uint32_t semantics) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(3);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpMemoryBarrier));
  TheInst.emplace_back(memory);
  TheInst.emplace_back(semantics);

  return *this;
}

InstBuilder &InstBuilder::opAtomicLoad(uint32_t result_type, uint32_t result_id,
                                       uint32_t pointer, uint32_t scope,
                                       uint32_t semantics) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(6);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpAtomicLoad));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(pointer);
  TheInst.emplace_back(scope);
  TheInst.emplace_back(semantics);

  return *this;
}

InstBuilder &InstBuilder::opAtomicStore(uint32_t pointer, uint32_t scope,
                                        uint32_t semantics, uint32_t value) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpAtomicStore));
  TheInst.emplace_back(pointer);
  TheInst.emplace_back(scope);
  TheInst.emplace_back(semantics);
  TheInst.emplace_back(value);

  return *this;
}

InstBuilder &InstBuilder::opAtomicExchange(uint32_t result_type,
                                           uint32_t result_id, uint32_t pointer,
                                           uint32_t scope, uint32_t semantics,
                                           uint32_t value) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(7);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpAtomicExchange));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(pointer);
  TheInst.emplace_back(scope);
  TheInst.emplace_back(semantics);
  TheInst.emplace_back(value);

  return *this;
}

InstBuilder &InstBuilder::opAtomicCompareExchange(
    uint32_t result_type, uint32_t result_id, uint32_t pointer, uint32_t scope,
    uint32_t equal, uint32_t unequal, uint32_t value, uint32_t comparator) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(9);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpAtomicCompareExchange));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(pointer);
  TheInst.emplace_back(scope);
  TheInst.emplace_back(equal);
  TheInst.emplace_back(unequal);
  TheInst.emplace_back(value);
  TheInst.emplace_back(comparator);

  return *this;
}

InstBuilder &InstBuilder::opAtomicCompareExchangeWeak(
    uint32_t result_type, uint32_t result_id, uint32_t pointer, uint32_t scope,
    uint32_t equal, uint32_t unequal, uint32_t value, uint32_t comparator) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(9);
  TheInst.emplace_back(
      static_cast<uint32_t>(spv::Op::OpAtomicCompareExchangeWeak));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(pointer);
  TheInst.emplace_back(scope);
  TheInst.emplace_back(equal);
  TheInst.emplace_back(unequal);
  TheInst.emplace_back(value);
  TheInst.emplace_back(comparator);

  return *this;
}

InstBuilder &InstBuilder::opAtomicIIncrement(uint32_t result_type,
                                             uint32_t result_id,
                                             uint32_t pointer, uint32_t scope,
                                             uint32_t semantics) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(6);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpAtomicIIncrement));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(pointer);
  TheInst.emplace_back(scope);
  TheInst.emplace_back(semantics);

  return *this;
}

InstBuilder &InstBuilder::opAtomicIDecrement(uint32_t result_type,
                                             uint32_t result_id,
                                             uint32_t pointer, uint32_t scope,
                                             uint32_t semantics) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(6);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpAtomicIDecrement));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(pointer);
  TheInst.emplace_back(scope);
  TheInst.emplace_back(semantics);

  return *this;
}

InstBuilder &InstBuilder::opAtomicIAdd(uint32_t result_type, uint32_t result_id,
                                       uint32_t pointer, uint32_t scope,
                                       uint32_t semantics, uint32_t value) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(7);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpAtomicIAdd));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(pointer);
  TheInst.emplace_back(scope);
  TheInst.emplace_back(semantics);
  TheInst.emplace_back(value);

  return *this;
}

InstBuilder &InstBuilder::opAtomicISub(uint32_t result_type, uint32_t result_id,
                                       uint32_t pointer, uint32_t scope,
                                       uint32_t semantics, uint32_t value) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(7);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpAtomicISub));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(pointer);
  TheInst.emplace_back(scope);
  TheInst.emplace_back(semantics);
  TheInst.emplace_back(value);

  return *this;
}

InstBuilder &InstBuilder::opAtomicSMin(uint32_t result_type, uint32_t result_id,
                                       uint32_t pointer, uint32_t scope,
                                       uint32_t semantics, uint32_t value) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(7);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpAtomicSMin));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(pointer);
  TheInst.emplace_back(scope);
  TheInst.emplace_back(semantics);
  TheInst.emplace_back(value);

  return *this;
}

InstBuilder &InstBuilder::opAtomicUMin(uint32_t result_type, uint32_t result_id,
                                       uint32_t pointer, uint32_t scope,
                                       uint32_t semantics, uint32_t value) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(7);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpAtomicUMin));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(pointer);
  TheInst.emplace_back(scope);
  TheInst.emplace_back(semantics);
  TheInst.emplace_back(value);

  return *this;
}

InstBuilder &InstBuilder::opAtomicSMax(uint32_t result_type, uint32_t result_id,
                                       uint32_t pointer, uint32_t scope,
                                       uint32_t semantics, uint32_t value) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(7);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpAtomicSMax));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(pointer);
  TheInst.emplace_back(scope);
  TheInst.emplace_back(semantics);
  TheInst.emplace_back(value);

  return *this;
}

InstBuilder &InstBuilder::opAtomicUMax(uint32_t result_type, uint32_t result_id,
                                       uint32_t pointer, uint32_t scope,
                                       uint32_t semantics, uint32_t value) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(7);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpAtomicUMax));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(pointer);
  TheInst.emplace_back(scope);
  TheInst.emplace_back(semantics);
  TheInst.emplace_back(value);

  return *this;
}

InstBuilder &InstBuilder::opAtomicAnd(uint32_t result_type, uint32_t result_id,
                                      uint32_t pointer, uint32_t scope,
                                      uint32_t semantics, uint32_t value) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(7);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpAtomicAnd));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(pointer);
  TheInst.emplace_back(scope);
  TheInst.emplace_back(semantics);
  TheInst.emplace_back(value);

  return *this;
}

InstBuilder &InstBuilder::opAtomicOr(uint32_t result_type, uint32_t result_id,
                                     uint32_t pointer, uint32_t scope,
                                     uint32_t semantics, uint32_t value) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(7);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpAtomicOr));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(pointer);
  TheInst.emplace_back(scope);
  TheInst.emplace_back(semantics);
  TheInst.emplace_back(value);

  return *this;
}

InstBuilder &InstBuilder::opAtomicXor(uint32_t result_type, uint32_t result_id,
                                      uint32_t pointer, uint32_t scope,
                                      uint32_t semantics, uint32_t value) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(7);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpAtomicXor));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(pointer);
  TheInst.emplace_back(scope);
  TheInst.emplace_back(semantics);
  TheInst.emplace_back(value);

  return *this;
}

InstBuilder &InstBuilder::opPhi(
    uint32_t result_type, uint32_t result_id,
    llvm::ArrayRef<std::pair<uint32_t, uint32_t>> variable_parent_) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpPhi));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  for (const auto &param : variable_parent_) {
    TheInst.emplace_back(param.first);
    TheInst.emplace_back(param.second);
    ;
  }

  return *this;
}

InstBuilder &InstBuilder::opLoopMerge(uint32_t merge_block,
                                      uint32_t continue_target,
                                      spv::LoopControlMask loop_control) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpLoopMerge));
  TheInst.emplace_back(merge_block);
  TheInst.emplace_back(continue_target);
  TheInst.emplace_back(static_cast<uint32_t>(loop_control));

  return *this;
}

InstBuilder &
InstBuilder::opSelectionMerge(uint32_t merge_block,
                              spv::SelectionControlMask selection_control) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(3);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpSelectionMerge));
  TheInst.emplace_back(merge_block);
  TheInst.emplace_back(static_cast<uint32_t>(selection_control));

  return *this;
}

InstBuilder &InstBuilder::opLabel(uint32_t result_id) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(2);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpLabel));
  TheInst.emplace_back(result_id);

  return *this;
}

InstBuilder &InstBuilder::opBranch(uint32_t target_label) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(2);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpBranch));
  TheInst.emplace_back(target_label);

  return *this;
}

InstBuilder &
InstBuilder::opBranchConditional(uint32_t condition, uint32_t true_label,
                                 uint32_t false_label,
                                 llvm::ArrayRef<uint32_t> branch_weights) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpBranchConditional));
  TheInst.emplace_back(condition);
  TheInst.emplace_back(true_label);
  TheInst.emplace_back(false_label);
  for (const auto &param : branch_weights) {
    TheInst.emplace_back(param);
    ;
  }

  return *this;
}

InstBuilder &
InstBuilder::opSwitch(uint32_t selector, uint32_t default_target,
                      llvm::ArrayRef<std::pair<uint32_t, uint32_t>> target) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpSwitch));
  TheInst.emplace_back(selector);
  TheInst.emplace_back(default_target);
  for (const auto &param : target) {
    TheInst.emplace_back(param.first);
    TheInst.emplace_back(param.second);
    ;
  }

  return *this;
}

InstBuilder &InstBuilder::opKill() {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(1);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpKill));

  return *this;
}

InstBuilder &InstBuilder::opReturn() {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(1);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpReturn));

  return *this;
}

InstBuilder &InstBuilder::opReturnValue(uint32_t value) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(2);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpReturnValue));
  TheInst.emplace_back(value);

  return *this;
}

InstBuilder &InstBuilder::opUnreachable() {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(1);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpUnreachable));

  return *this;
}

InstBuilder &InstBuilder::opLifetimeStart(uint32_t pointer, uint32_t size) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(3);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpLifetimeStart));
  TheInst.emplace_back(pointer);
  TheInst.emplace_back(size);

  return *this;
}

InstBuilder &InstBuilder::opLifetimeStop(uint32_t pointer, uint32_t size) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(3);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpLifetimeStop));
  TheInst.emplace_back(pointer);
  TheInst.emplace_back(size);

  return *this;
}

InstBuilder &
InstBuilder::opGroupAsyncCopy(uint32_t result_type, uint32_t result_id,
                              uint32_t execution, uint32_t destination,
                              uint32_t source, uint32_t num_elements,
                              uint32_t stride, uint32_t event) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(9);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpGroupAsyncCopy));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(execution);
  TheInst.emplace_back(destination);
  TheInst.emplace_back(source);
  TheInst.emplace_back(num_elements);
  TheInst.emplace_back(stride);
  TheInst.emplace_back(event);

  return *this;
}

InstBuilder &InstBuilder::opGroupWaitEvents(uint32_t execution,
                                            uint32_t num_events,
                                            uint32_t events_list) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpGroupWaitEvents));
  TheInst.emplace_back(execution);
  TheInst.emplace_back(num_events);
  TheInst.emplace_back(events_list);

  return *this;
}

InstBuilder &InstBuilder::opGroupAll(uint32_t result_type, uint32_t result_id,
                                     uint32_t execution, uint32_t predicate) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpGroupAll));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(execution);
  TheInst.emplace_back(predicate);

  return *this;
}

InstBuilder &InstBuilder::opGroupAny(uint32_t result_type, uint32_t result_id,
                                     uint32_t execution, uint32_t predicate) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpGroupAny));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(execution);
  TheInst.emplace_back(predicate);

  return *this;
}

InstBuilder &InstBuilder::opGroupBroadcast(uint32_t result_type,
                                           uint32_t result_id,
                                           uint32_t execution, uint32_t value,
                                           uint32_t local_id) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(6);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpGroupBroadcast));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(execution);
  TheInst.emplace_back(value);
  TheInst.emplace_back(local_id);

  return *this;
}

InstBuilder &InstBuilder::opGroupIAdd(uint32_t result_type, uint32_t result_id,
                                      uint32_t execution,
                                      spv::GroupOperation operation,
                                      uint32_t x) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(6);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpGroupIAdd));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(execution);
  TheInst.emplace_back(static_cast<uint32_t>(operation));
  TheInst.emplace_back(x);

  return *this;
}

InstBuilder &InstBuilder::opGroupFAdd(uint32_t result_type, uint32_t result_id,
                                      uint32_t execution,
                                      spv::GroupOperation operation,
                                      uint32_t x) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(6);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpGroupFAdd));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(execution);
  TheInst.emplace_back(static_cast<uint32_t>(operation));
  TheInst.emplace_back(x);

  return *this;
}

InstBuilder &InstBuilder::opGroupFMin(uint32_t result_type, uint32_t result_id,
                                      uint32_t execution,
                                      spv::GroupOperation operation,
                                      uint32_t x) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(6);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpGroupFMin));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(execution);
  TheInst.emplace_back(static_cast<uint32_t>(operation));
  TheInst.emplace_back(x);

  return *this;
}

InstBuilder &InstBuilder::opGroupUMin(uint32_t result_type, uint32_t result_id,
                                      uint32_t execution,
                                      spv::GroupOperation operation,
                                      uint32_t x) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(6);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpGroupUMin));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(execution);
  TheInst.emplace_back(static_cast<uint32_t>(operation));
  TheInst.emplace_back(x);

  return *this;
}

InstBuilder &InstBuilder::opGroupSMin(uint32_t result_type, uint32_t result_id,
                                      uint32_t execution,
                                      spv::GroupOperation operation,
                                      uint32_t x) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(6);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpGroupSMin));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(execution);
  TheInst.emplace_back(static_cast<uint32_t>(operation));
  TheInst.emplace_back(x);

  return *this;
}

InstBuilder &InstBuilder::opGroupFMax(uint32_t result_type, uint32_t result_id,
                                      uint32_t execution,
                                      spv::GroupOperation operation,
                                      uint32_t x) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(6);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpGroupFMax));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(execution);
  TheInst.emplace_back(static_cast<uint32_t>(operation));
  TheInst.emplace_back(x);

  return *this;
}

InstBuilder &InstBuilder::opGroupUMax(uint32_t result_type, uint32_t result_id,
                                      uint32_t execution,
                                      spv::GroupOperation operation,
                                      uint32_t x) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(6);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpGroupUMax));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(execution);
  TheInst.emplace_back(static_cast<uint32_t>(operation));
  TheInst.emplace_back(x);

  return *this;
}

InstBuilder &InstBuilder::opGroupSMax(uint32_t result_type, uint32_t result_id,
                                      uint32_t execution,
                                      spv::GroupOperation operation,
                                      uint32_t x) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(6);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpGroupSMax));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(execution);
  TheInst.emplace_back(static_cast<uint32_t>(operation));
  TheInst.emplace_back(x);

  return *this;
}

InstBuilder &InstBuilder::opReadPipe(uint32_t result_type, uint32_t result_id,
                                     uint32_t pipe, uint32_t pointer,
                                     uint32_t packet_size,
                                     uint32_t packet_alignment) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(7);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpReadPipe));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(pipe);
  TheInst.emplace_back(pointer);
  TheInst.emplace_back(packet_size);
  TheInst.emplace_back(packet_alignment);

  return *this;
}

InstBuilder &InstBuilder::opWritePipe(uint32_t result_type, uint32_t result_id,
                                      uint32_t pipe, uint32_t pointer,
                                      uint32_t packet_size,
                                      uint32_t packet_alignment) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(7);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpWritePipe));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(pipe);
  TheInst.emplace_back(pointer);
  TheInst.emplace_back(packet_size);
  TheInst.emplace_back(packet_alignment);

  return *this;
}

InstBuilder &InstBuilder::opReservedReadPipe(uint32_t result_type,
                                             uint32_t result_id, uint32_t pipe,
                                             uint32_t reserve_id,
                                             uint32_t index, uint32_t pointer,
                                             uint32_t packet_size,
                                             uint32_t packet_alignment) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(9);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpReservedReadPipe));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(pipe);
  TheInst.emplace_back(reserve_id);
  TheInst.emplace_back(index);
  TheInst.emplace_back(pointer);
  TheInst.emplace_back(packet_size);
  TheInst.emplace_back(packet_alignment);

  return *this;
}

InstBuilder &InstBuilder::opReservedWritePipe(uint32_t result_type,
                                              uint32_t result_id, uint32_t pipe,
                                              uint32_t reserve_id,
                                              uint32_t index, uint32_t pointer,
                                              uint32_t packet_size,
                                              uint32_t packet_alignment) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(9);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpReservedWritePipe));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(pipe);
  TheInst.emplace_back(reserve_id);
  TheInst.emplace_back(index);
  TheInst.emplace_back(pointer);
  TheInst.emplace_back(packet_size);
  TheInst.emplace_back(packet_alignment);

  return *this;
}

InstBuilder &InstBuilder::opReserveReadPipePackets(
    uint32_t result_type, uint32_t result_id, uint32_t pipe,
    uint32_t num_packets, uint32_t packet_size, uint32_t packet_alignment) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(7);
  TheInst.emplace_back(
      static_cast<uint32_t>(spv::Op::OpReserveReadPipePackets));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(pipe);
  TheInst.emplace_back(num_packets);
  TheInst.emplace_back(packet_size);
  TheInst.emplace_back(packet_alignment);

  return *this;
}

InstBuilder &InstBuilder::opReserveWritePipePackets(
    uint32_t result_type, uint32_t result_id, uint32_t pipe,
    uint32_t num_packets, uint32_t packet_size, uint32_t packet_alignment) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(7);
  TheInst.emplace_back(
      static_cast<uint32_t>(spv::Op::OpReserveWritePipePackets));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(pipe);
  TheInst.emplace_back(num_packets);
  TheInst.emplace_back(packet_size);
  TheInst.emplace_back(packet_alignment);

  return *this;
}

InstBuilder &InstBuilder::opCommitReadPipe(uint32_t pipe, uint32_t reserve_id,
                                           uint32_t packet_size,
                                           uint32_t packet_alignment) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpCommitReadPipe));
  TheInst.emplace_back(pipe);
  TheInst.emplace_back(reserve_id);
  TheInst.emplace_back(packet_size);
  TheInst.emplace_back(packet_alignment);

  return *this;
}

InstBuilder &InstBuilder::opCommitWritePipe(uint32_t pipe, uint32_t reserve_id,
                                            uint32_t packet_size,
                                            uint32_t packet_alignment) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpCommitWritePipe));
  TheInst.emplace_back(pipe);
  TheInst.emplace_back(reserve_id);
  TheInst.emplace_back(packet_size);
  TheInst.emplace_back(packet_alignment);

  return *this;
}

InstBuilder &InstBuilder::opIsValidReserveId(uint32_t result_type,
                                             uint32_t result_id,
                                             uint32_t reserve_id) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpIsValidReserveId));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(reserve_id);

  return *this;
}

InstBuilder &InstBuilder::opGetNumPipePackets(uint32_t result_type,
                                              uint32_t result_id, uint32_t pipe,
                                              uint32_t packet_size,
                                              uint32_t packet_alignment) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(6);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpGetNumPipePackets));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(pipe);
  TheInst.emplace_back(packet_size);
  TheInst.emplace_back(packet_alignment);

  return *this;
}

InstBuilder &InstBuilder::opGetMaxPipePackets(uint32_t result_type,
                                              uint32_t result_id, uint32_t pipe,
                                              uint32_t packet_size,
                                              uint32_t packet_alignment) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(6);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpGetMaxPipePackets));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(pipe);
  TheInst.emplace_back(packet_size);
  TheInst.emplace_back(packet_alignment);

  return *this;
}

InstBuilder &InstBuilder::opGroupReserveReadPipePackets(
    uint32_t result_type, uint32_t result_id, uint32_t execution, uint32_t pipe,
    uint32_t num_packets, uint32_t packet_size, uint32_t packet_alignment) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(8);
  TheInst.emplace_back(
      static_cast<uint32_t>(spv::Op::OpGroupReserveReadPipePackets));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(execution);
  TheInst.emplace_back(pipe);
  TheInst.emplace_back(num_packets);
  TheInst.emplace_back(packet_size);
  TheInst.emplace_back(packet_alignment);

  return *this;
}

InstBuilder &InstBuilder::opGroupReserveWritePipePackets(
    uint32_t result_type, uint32_t result_id, uint32_t execution, uint32_t pipe,
    uint32_t num_packets, uint32_t packet_size, uint32_t packet_alignment) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(8);
  TheInst.emplace_back(
      static_cast<uint32_t>(spv::Op::OpGroupReserveWritePipePackets));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(execution);
  TheInst.emplace_back(pipe);
  TheInst.emplace_back(num_packets);
  TheInst.emplace_back(packet_size);
  TheInst.emplace_back(packet_alignment);

  return *this;
}

InstBuilder &InstBuilder::opGroupCommitReadPipe(uint32_t execution,
                                                uint32_t pipe,
                                                uint32_t reserve_id,
                                                uint32_t packet_size,
                                                uint32_t packet_alignment) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(6);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpGroupCommitReadPipe));
  TheInst.emplace_back(execution);
  TheInst.emplace_back(pipe);
  TheInst.emplace_back(reserve_id);
  TheInst.emplace_back(packet_size);
  TheInst.emplace_back(packet_alignment);

  return *this;
}

InstBuilder &InstBuilder::opGroupCommitWritePipe(uint32_t execution,
                                                 uint32_t pipe,
                                                 uint32_t reserve_id,
                                                 uint32_t packet_size,
                                                 uint32_t packet_alignment) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(6);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpGroupCommitWritePipe));
  TheInst.emplace_back(execution);
  TheInst.emplace_back(pipe);
  TheInst.emplace_back(reserve_id);
  TheInst.emplace_back(packet_size);
  TheInst.emplace_back(packet_alignment);

  return *this;
}

InstBuilder &InstBuilder::opEnqueueMarker(uint32_t result_type,
                                          uint32_t result_id, uint32_t queue,
                                          uint32_t num_events,
                                          uint32_t wait_events,
                                          uint32_t ret_event) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(7);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpEnqueueMarker));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(queue);
  TheInst.emplace_back(num_events);
  TheInst.emplace_back(wait_events);
  TheInst.emplace_back(ret_event);

  return *this;
}

InstBuilder &InstBuilder::opEnqueueKernel(
    uint32_t result_type, uint32_t result_id, uint32_t queue, uint32_t flags,
    uint32_t nd_range, uint32_t num_events, uint32_t wait_events,
    uint32_t ret_event, uint32_t invoke, uint32_t param, uint32_t param_size,
    uint32_t param_align, llvm::ArrayRef<uint32_t> local_size) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(14);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpEnqueueKernel));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(queue);
  TheInst.emplace_back(flags);
  TheInst.emplace_back(nd_range);
  TheInst.emplace_back(num_events);
  TheInst.emplace_back(wait_events);
  TheInst.emplace_back(ret_event);
  TheInst.emplace_back(invoke);
  TheInst.emplace_back(param);
  TheInst.emplace_back(param_size);
  TheInst.emplace_back(param_align);
  TheInst.insert(TheInst.end(), local_size.begin(), local_size.end());

  return *this;
}

InstBuilder &InstBuilder::opGetKernelNDrangeSubGroupCount(
    uint32_t result_type, uint32_t result_id, uint32_t nd_range,
    uint32_t invoke, uint32_t param, uint32_t param_size,
    uint32_t param_align) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(8);
  TheInst.emplace_back(
      static_cast<uint32_t>(spv::Op::OpGetKernelNDrangeSubGroupCount));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(nd_range);
  TheInst.emplace_back(invoke);
  TheInst.emplace_back(param);
  TheInst.emplace_back(param_size);
  TheInst.emplace_back(param_align);

  return *this;
}

InstBuilder &InstBuilder::opGetKernelNDrangeMaxSubGroupSize(
    uint32_t result_type, uint32_t result_id, uint32_t nd_range,
    uint32_t invoke, uint32_t param, uint32_t param_size,
    uint32_t param_align) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(8);
  TheInst.emplace_back(
      static_cast<uint32_t>(spv::Op::OpGetKernelNDrangeMaxSubGroupSize));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(nd_range);
  TheInst.emplace_back(invoke);
  TheInst.emplace_back(param);
  TheInst.emplace_back(param_size);
  TheInst.emplace_back(param_align);

  return *this;
}

InstBuilder &InstBuilder::opGetKernelWorkGroupSize(
    uint32_t result_type, uint32_t result_id, uint32_t invoke, uint32_t param,
    uint32_t param_size, uint32_t param_align) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(7);
  TheInst.emplace_back(
      static_cast<uint32_t>(spv::Op::OpGetKernelWorkGroupSize));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(invoke);
  TheInst.emplace_back(param);
  TheInst.emplace_back(param_size);
  TheInst.emplace_back(param_align);

  return *this;
}

InstBuilder &InstBuilder::opGetKernelPreferredWorkGroupSizeMultiple(
    uint32_t result_type, uint32_t result_id, uint32_t invoke, uint32_t param,
    uint32_t param_size, uint32_t param_align) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(7);
  TheInst.emplace_back(static_cast<uint32_t>(
      spv::Op::OpGetKernelPreferredWorkGroupSizeMultiple));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(invoke);
  TheInst.emplace_back(param);
  TheInst.emplace_back(param_size);
  TheInst.emplace_back(param_align);

  return *this;
}

InstBuilder &InstBuilder::opRetainEvent(uint32_t event) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(2);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpRetainEvent));
  TheInst.emplace_back(event);

  return *this;
}

InstBuilder &InstBuilder::opReleaseEvent(uint32_t event) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(2);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpReleaseEvent));
  TheInst.emplace_back(event);

  return *this;
}

InstBuilder &InstBuilder::opCreateUserEvent(uint32_t result_type,
                                            uint32_t result_id) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(3);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpCreateUserEvent));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);

  return *this;
}

InstBuilder &InstBuilder::opIsValidEvent(uint32_t result_type,
                                         uint32_t result_id, uint32_t event) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpIsValidEvent));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(event);

  return *this;
}

InstBuilder &InstBuilder::opSetUserEventStatus(uint32_t event,
                                               uint32_t status) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(3);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpSetUserEventStatus));
  TheInst.emplace_back(event);
  TheInst.emplace_back(status);

  return *this;
}

InstBuilder &InstBuilder::opCaptureEventProfilingInfo(uint32_t event,
                                                      uint32_t profiling_info,
                                                      uint32_t value) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(
      static_cast<uint32_t>(spv::Op::OpCaptureEventProfilingInfo));
  TheInst.emplace_back(event);
  TheInst.emplace_back(profiling_info);
  TheInst.emplace_back(value);

  return *this;
}

InstBuilder &InstBuilder::opGetDefaultQueue(uint32_t result_type,
                                            uint32_t result_id) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(3);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpGetDefaultQueue));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);

  return *this;
}

InstBuilder &InstBuilder::opBuildNDRange(uint32_t result_type,
                                         uint32_t result_id,
                                         uint32_t global_work_size,
                                         uint32_t local_work_size,
                                         uint32_t global_work_offset) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(6);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpBuildNDRange));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(global_work_size);
  TheInst.emplace_back(local_work_size);
  TheInst.emplace_back(global_work_offset);

  return *this;
}

InstBuilder &InstBuilder::opImageSparseSampleImplicitLod(
    uint32_t result_type, uint32_t result_id, uint32_t sampled_image,
    uint32_t coordinate,
    llvm::Optional<spv::ImageOperandsMask> image_operands) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(6);
  TheInst.emplace_back(
      static_cast<uint32_t>(spv::Op::OpImageSparseSampleImplicitLod));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(sampled_image);
  TheInst.emplace_back(coordinate);
  if (image_operands.hasValue()) {
    const auto &val = image_operands.getValue();
    encodeImageOperands(val);
  }

  return *this;
}

InstBuilder &InstBuilder::opImageSparseSampleExplicitLod(
    uint32_t result_type, uint32_t result_id, uint32_t sampled_image,
    uint32_t coordinate, spv::ImageOperandsMask image_operands) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(6);
  TheInst.emplace_back(
      static_cast<uint32_t>(spv::Op::OpImageSparseSampleExplicitLod));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(sampled_image);
  TheInst.emplace_back(coordinate);
  encodeImageOperands(image_operands);

  return *this;
}

InstBuilder &InstBuilder::opImageSparseSampleDrefImplicitLod(
    uint32_t result_type, uint32_t result_id, uint32_t sampled_image,
    uint32_t coordinate, uint32_t dref,
    llvm::Optional<spv::ImageOperandsMask> image_operands) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(7);
  TheInst.emplace_back(
      static_cast<uint32_t>(spv::Op::OpImageSparseSampleDrefImplicitLod));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(sampled_image);
  TheInst.emplace_back(coordinate);
  TheInst.emplace_back(dref);
  if (image_operands.hasValue()) {
    const auto &val = image_operands.getValue();
    encodeImageOperands(val);
  }

  return *this;
}

InstBuilder &InstBuilder::opImageSparseSampleDrefExplicitLod(
    uint32_t result_type, uint32_t result_id, uint32_t sampled_image,
    uint32_t coordinate, uint32_t dref, spv::ImageOperandsMask image_operands) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(7);
  TheInst.emplace_back(
      static_cast<uint32_t>(spv::Op::OpImageSparseSampleDrefExplicitLod));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(sampled_image);
  TheInst.emplace_back(coordinate);
  TheInst.emplace_back(dref);
  encodeImageOperands(image_operands);

  return *this;
}

InstBuilder &InstBuilder::opImageSparseSampleProjImplicitLod(
    uint32_t result_type, uint32_t result_id, uint32_t sampled_image,
    uint32_t coordinate,
    llvm::Optional<spv::ImageOperandsMask> image_operands) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(6);
  TheInst.emplace_back(
      static_cast<uint32_t>(spv::Op::OpImageSparseSampleProjImplicitLod));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(sampled_image);
  TheInst.emplace_back(coordinate);
  if (image_operands.hasValue()) {
    const auto &val = image_operands.getValue();
    encodeImageOperands(val);
  }

  return *this;
}

InstBuilder &InstBuilder::opImageSparseSampleProjExplicitLod(
    uint32_t result_type, uint32_t result_id, uint32_t sampled_image,
    uint32_t coordinate, spv::ImageOperandsMask image_operands) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(6);
  TheInst.emplace_back(
      static_cast<uint32_t>(spv::Op::OpImageSparseSampleProjExplicitLod));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(sampled_image);
  TheInst.emplace_back(coordinate);
  encodeImageOperands(image_operands);

  return *this;
}

InstBuilder &InstBuilder::opImageSparseSampleProjDrefImplicitLod(
    uint32_t result_type, uint32_t result_id, uint32_t sampled_image,
    uint32_t coordinate, uint32_t dref,
    llvm::Optional<spv::ImageOperandsMask> image_operands) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(7);
  TheInst.emplace_back(
      static_cast<uint32_t>(spv::Op::OpImageSparseSampleProjDrefImplicitLod));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(sampled_image);
  TheInst.emplace_back(coordinate);
  TheInst.emplace_back(dref);
  if (image_operands.hasValue()) {
    const auto &val = image_operands.getValue();
    encodeImageOperands(val);
  }

  return *this;
}

InstBuilder &InstBuilder::opImageSparseSampleProjDrefExplicitLod(
    uint32_t result_type, uint32_t result_id, uint32_t sampled_image,
    uint32_t coordinate, uint32_t dref, spv::ImageOperandsMask image_operands) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(7);
  TheInst.emplace_back(
      static_cast<uint32_t>(spv::Op::OpImageSparseSampleProjDrefExplicitLod));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(sampled_image);
  TheInst.emplace_back(coordinate);
  TheInst.emplace_back(dref);
  encodeImageOperands(image_operands);

  return *this;
}

InstBuilder &InstBuilder::opImageSparseFetch(
    uint32_t result_type, uint32_t result_id, uint32_t image,
    uint32_t coordinate,
    llvm::Optional<spv::ImageOperandsMask> image_operands) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(6);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpImageSparseFetch));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(image);
  TheInst.emplace_back(coordinate);
  if (image_operands.hasValue()) {
    const auto &val = image_operands.getValue();
    encodeImageOperands(val);
  }

  return *this;
}

InstBuilder &InstBuilder::opImageSparseGather(
    uint32_t result_type, uint32_t result_id, uint32_t sampled_image,
    uint32_t coordinate, uint32_t component,
    llvm::Optional<spv::ImageOperandsMask> image_operands) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(7);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpImageSparseGather));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(sampled_image);
  TheInst.emplace_back(coordinate);
  TheInst.emplace_back(component);
  if (image_operands.hasValue()) {
    const auto &val = image_operands.getValue();
    encodeImageOperands(val);
  }

  return *this;
}

InstBuilder &InstBuilder::opImageSparseDrefGather(
    uint32_t result_type, uint32_t result_id, uint32_t sampled_image,
    uint32_t coordinate, uint32_t dref,
    llvm::Optional<spv::ImageOperandsMask> image_operands) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(7);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpImageSparseDrefGather));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(sampled_image);
  TheInst.emplace_back(coordinate);
  TheInst.emplace_back(dref);
  if (image_operands.hasValue()) {
    const auto &val = image_operands.getValue();
    encodeImageOperands(val);
  }

  return *this;
}

InstBuilder &InstBuilder::opImageSparseTexelsResident(uint32_t result_type,
                                                      uint32_t result_id,
                                                      uint32_t resident_code) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(
      static_cast<uint32_t>(spv::Op::OpImageSparseTexelsResident));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(resident_code);

  return *this;
}

InstBuilder &InstBuilder::opNoLine() {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(1);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpNoLine));

  return *this;
}

InstBuilder &InstBuilder::opAtomicFlagTestAndSet(uint32_t result_type,
                                                 uint32_t result_id,
                                                 uint32_t pointer,
                                                 uint32_t scope,
                                                 uint32_t semantics) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(6);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpAtomicFlagTestAndSet));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(pointer);
  TheInst.emplace_back(scope);
  TheInst.emplace_back(semantics);

  return *this;
}

InstBuilder &InstBuilder::opAtomicFlagClear(uint32_t pointer, uint32_t scope,
                                            uint32_t semantics) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpAtomicFlagClear));
  TheInst.emplace_back(pointer);
  TheInst.emplace_back(scope);
  TheInst.emplace_back(semantics);

  return *this;
}

InstBuilder &InstBuilder::opImageSparseRead(
    uint32_t result_type, uint32_t result_id, uint32_t image,
    uint32_t coordinate,
    llvm::Optional<spv::ImageOperandsMask> image_operands) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(6);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpImageSparseRead));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(image);
  TheInst.emplace_back(coordinate);
  if (image_operands.hasValue()) {
    const auto &val = image_operands.getValue();
    encodeImageOperands(val);
  }

  return *this;
}

InstBuilder &InstBuilder::opSubgroupBallotKHR(uint32_t result_type,
                                              uint32_t result_id,
                                              uint32_t predicate) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpSubgroupBallotKHR));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(predicate);

  return *this;
}

InstBuilder &InstBuilder::opSubgroupFirstInvocationKHR(uint32_t result_type,
                                                       uint32_t result_id,
                                                       uint32_t value) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(
      static_cast<uint32_t>(spv::Op::OpSubgroupFirstInvocationKHR));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(value);

  return *this;
}

InstBuilder &InstBuilder::opSubgroupAllKHR(uint32_t result_type,
                                           uint32_t result_id,
                                           uint32_t predicate) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpSubgroupAllKHR));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(predicate);

  return *this;
}

InstBuilder &InstBuilder::opSubgroupAnyKHR(uint32_t result_type,
                                           uint32_t result_id,
                                           uint32_t predicate) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpSubgroupAnyKHR));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(predicate);

  return *this;
}

InstBuilder &InstBuilder::opSubgroupAllEqualKHR(uint32_t result_type,
                                                uint32_t result_id,
                                                uint32_t predicate) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpSubgroupAllEqualKHR));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(predicate);

  return *this;
}

InstBuilder &InstBuilder::opSubgroupReadInvocationKHR(uint32_t result_type,
                                                      uint32_t result_id,
                                                      uint32_t value,
                                                      uint32_t index) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }
  if (result_type == 0) {
    TheStatus = Status::ZeroResultType;
    return *this;
  }
  if (result_id == 0) {
    TheStatus = Status::ZeroResultId;
    return *this;
  }

  TheInst.reserve(5);
  TheInst.emplace_back(
      static_cast<uint32_t>(spv::Op::OpSubgroupReadInvocationKHR));
  TheInst.emplace_back(result_type);
  TheInst.emplace_back(result_id);
  TheInst.emplace_back(value);
  TheInst.emplace_back(index);

  return *this;
}

void InstBuilder::encodeImageOperands(spv::ImageOperandsMask value) {
  if (bitEnumContains(value, spv::ImageOperandsMask::Bias)) {
    Expectation.emplace_back(OperandKind::IdRef);
  }
  if (bitEnumContains(value, spv::ImageOperandsMask::Lod)) {
    Expectation.emplace_back(OperandKind::IdRef);
  }
  if (bitEnumContains(value, spv::ImageOperandsMask::Grad)) {
    Expectation.emplace_back(OperandKind::IdRef);
    Expectation.emplace_back(OperandKind::IdRef);
  }
  if (bitEnumContains(value, spv::ImageOperandsMask::ConstOffset)) {
    Expectation.emplace_back(OperandKind::IdRef);
  }
  if (bitEnumContains(value, spv::ImageOperandsMask::Offset)) {
    Expectation.emplace_back(OperandKind::IdRef);
  }
  if (bitEnumContains(value, spv::ImageOperandsMask::ConstOffsets)) {
    Expectation.emplace_back(OperandKind::IdRef);
  }
  if (bitEnumContains(value, spv::ImageOperandsMask::Sample)) {
    Expectation.emplace_back(OperandKind::IdRef);
  }
  if (bitEnumContains(value, spv::ImageOperandsMask::MinLod)) {
    Expectation.emplace_back(OperandKind::IdRef);
  }
  TheInst.emplace_back(static_cast<uint32_t>(value));
}

void InstBuilder::encodeMemoryAccess(spv::MemoryAccessMask value) {
  if (bitEnumContains(value, spv::MemoryAccessMask::Aligned)) {
    Expectation.emplace_back(OperandKind::LiteralInteger);
  }
  TheInst.emplace_back(static_cast<uint32_t>(value));
}

void InstBuilder::encodeExecutionMode(spv::ExecutionMode value) {
  switch (value) {
  case spv::ExecutionMode::Invocations: {
    Expectation.emplace_back(OperandKind::LiteralInteger);
  } break;
  case spv::ExecutionMode::LocalSize: {
    Expectation.emplace_back(OperandKind::LiteralInteger);
    Expectation.emplace_back(OperandKind::LiteralInteger);
    Expectation.emplace_back(OperandKind::LiteralInteger);
  } break;
  case spv::ExecutionMode::LocalSizeHint: {
    Expectation.emplace_back(OperandKind::LiteralInteger);
    Expectation.emplace_back(OperandKind::LiteralInteger);
    Expectation.emplace_back(OperandKind::LiteralInteger);
  } break;
  case spv::ExecutionMode::OutputVertices: {
    Expectation.emplace_back(OperandKind::LiteralInteger);
  } break;
  case spv::ExecutionMode::VecTypeHint: {
    Expectation.emplace_back(OperandKind::LiteralInteger);
  } break;
  default:
    break;
  }

  TheInst.emplace_back(static_cast<uint32_t>(value));
}

void InstBuilder::encodeDecoration(spv::Decoration value) {
  switch (value) {
  case spv::Decoration::SpecId: {
    Expectation.emplace_back(OperandKind::LiteralInteger);
  } break;
  case spv::Decoration::ArrayStride: {
    Expectation.emplace_back(OperandKind::LiteralInteger);
  } break;
  case spv::Decoration::MatrixStride: {
    Expectation.emplace_back(OperandKind::LiteralInteger);
  } break;
  case spv::Decoration::BuiltIn: {
    Expectation.emplace_back(OperandKind::BuiltIn);
  } break;
  case spv::Decoration::Stream: {
    Expectation.emplace_back(OperandKind::LiteralInteger);
  } break;
  case spv::Decoration::Location: {
    Expectation.emplace_back(OperandKind::LiteralInteger);
  } break;
  case spv::Decoration::Component: {
    Expectation.emplace_back(OperandKind::LiteralInteger);
  } break;
  case spv::Decoration::Index: {
    Expectation.emplace_back(OperandKind::LiteralInteger);
  } break;
  case spv::Decoration::Binding: {
    Expectation.emplace_back(OperandKind::LiteralInteger);
  } break;
  case spv::Decoration::DescriptorSet: {
    Expectation.emplace_back(OperandKind::LiteralInteger);
  } break;
  case spv::Decoration::Offset: {
    Expectation.emplace_back(OperandKind::LiteralInteger);
  } break;
  case spv::Decoration::XfbBuffer: {
    Expectation.emplace_back(OperandKind::LiteralInteger);
  } break;
  case spv::Decoration::XfbStride: {
    Expectation.emplace_back(OperandKind::LiteralInteger);
  } break;
  case spv::Decoration::FuncParamAttr: {
    Expectation.emplace_back(OperandKind::FunctionParameterAttribute);
  } break;
  case spv::Decoration::FPRoundingMode: {
    Expectation.emplace_back(OperandKind::FPRoundingMode);
  } break;
  case spv::Decoration::FPFastMathMode: {
    Expectation.emplace_back(OperandKind::FPFastMathMode);
  } break;
  case spv::Decoration::LinkageAttributes: {
    Expectation.emplace_back(OperandKind::LiteralString);
    Expectation.emplace_back(OperandKind::LinkageType);
  } break;
  case spv::Decoration::InputAttachmentIndex: {
    Expectation.emplace_back(OperandKind::LiteralInteger);
  } break;
  case spv::Decoration::Alignment: {
    Expectation.emplace_back(OperandKind::LiteralInteger);
  } break;
  case spv::Decoration::SecondaryViewportRelativeNV: {
    Expectation.emplace_back(OperandKind::LiteralInteger);
  } break;
  default:
    break;
  }

  TheInst.emplace_back(static_cast<uint32_t>(value));
}

InstBuilder &InstBuilder::fPFastMathMode(spv::FPFastMathModeMask value) {
  if (Expectation.front() != OperandKind::FPFastMathMode) {
    TheStatus = Status::ExpectFPFastMathMode;
    return *this;
  }
  Expectation.pop_front();
  TheInst.emplace_back(static_cast<uint32_t>(value));
  return *this;
}

InstBuilder &InstBuilder::fPRoundingMode(spv::FPRoundingMode value) {
  if (Expectation.front() != OperandKind::FPRoundingMode) {
    TheStatus = Status::ExpectFPRoundingMode;
    return *this;
  }
  Expectation.pop_front();
  TheInst.emplace_back(static_cast<uint32_t>(value));
  return *this;
}

InstBuilder &InstBuilder::linkageType(spv::LinkageType value) {
  if (Expectation.front() != OperandKind::LinkageType) {
    TheStatus = Status::ExpectLinkageType;
    return *this;
  }
  Expectation.pop_front();
  TheInst.emplace_back(static_cast<uint32_t>(value));
  return *this;
}

InstBuilder &
InstBuilder::functionParameterAttribute(spv::FunctionParameterAttribute value) {
  if (Expectation.front() != OperandKind::FunctionParameterAttribute) {
    TheStatus = Status::ExpectFunctionParameterAttribute;
    return *this;
  }
  Expectation.pop_front();
  TheInst.emplace_back(static_cast<uint32_t>(value));
  return *this;
}

InstBuilder &InstBuilder::builtIn(spv::BuiltIn value) {
  if (Expectation.front() != OperandKind::BuiltIn) {
    TheStatus = Status::ExpectBuiltIn;
    return *this;
  }
  Expectation.pop_front();
  TheInst.emplace_back(static_cast<uint32_t>(value));
  return *this;
}

InstBuilder &InstBuilder::idRef(uint32_t value) {
  if (Expectation.front() != OperandKind::IdRef) {
    TheStatus = Status::ExpectIdRef;
    return *this;
  }
  Expectation.pop_front();
  TheInst.emplace_back(value);
  return *this;
}

InstBuilder &InstBuilder::literalInteger(uint32_t value) {
  if (Expectation.front() != OperandKind::LiteralInteger) {
    TheStatus = Status::ExpectLiteralInteger;
    return *this;
  }
  Expectation.pop_front();
  TheInst.emplace_back(value);
  return *this;
}

InstBuilder &InstBuilder::literalString(std::string value) {
  if (Expectation.front() != OperandKind::LiteralString) {
    TheStatus = Status::ExpectLiteralString;
    return *this;
  }
  Expectation.pop_front();
  encodeString(value);
  return *this;
}

} // end namespace spirv
} // end namespace clang
