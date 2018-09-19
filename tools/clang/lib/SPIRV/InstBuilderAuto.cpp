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

static_assert(spv::Version == 0x00010300 && spv::Revision == 6,
              "Needs to regenerate outdated InstBuilder");

namespace {
inline bool bitEnumContains(spv::ImageOperandsMask bits,
                            spv::ImageOperandsMask bit) {
  return (uint32_t(bits) & uint32_t(bit)) != 0;
}
inline bool bitEnumContains(spv::LoopControlMask bits,
                            spv::LoopControlMask bit) {
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

InstBuilder &InstBuilder::opSourceContinued(llvm::StringRef continued_source) {
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
                                   llvm::Optional<llvm::StringRef> source) {
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
  encodeLoopControl(loop_control);

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

InstBuilder &InstBuilder::opModuleProcessed(std::string process) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(2);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpModuleProcessed));
  encodeString(process);

  return *this;
}

InstBuilder &InstBuilder::opExecutionModeId(uint32_t entry_point,
                                            spv::ExecutionMode mode) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(3);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpExecutionModeId));
  TheInst.emplace_back(entry_point);
  encodeExecutionMode(mode);

  return *this;
}

InstBuilder &InstBuilder::opDecorateId(uint32_t target,
                                       spv::Decoration decoration) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(3);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpDecorateId));
  TheInst.emplace_back(target);
  encodeDecoration(decoration);

  return *this;
}

InstBuilder &InstBuilder::opDecorateStringGOOGLE(uint32_t target,
                                                 spv::Decoration decoration) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(3);
  TheInst.emplace_back(static_cast<uint32_t>(spv::Op::OpDecorateStringGOOGLE));
  TheInst.emplace_back(target);
  encodeDecoration(decoration);

  return *this;
}

InstBuilder &
InstBuilder::opMemberDecorateStringGOOGLE(uint32_t struct_type, uint32_t member,
                                          spv::Decoration decoration) {
  if (!TheInst.empty()) {
    TheStatus = Status::NestedInst;
    return *this;
  }

  TheInst.reserve(4);
  TheInst.emplace_back(
      static_cast<uint32_t>(spv::Op::OpMemberDecorateStringGOOGLE));
  TheInst.emplace_back(struct_type);
  TheInst.emplace_back(member);
  encodeDecoration(decoration);

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

void InstBuilder::encodeLoopControl(spv::LoopControlMask value) {
  if (bitEnumContains(value, spv::LoopControlMask::DependencyLength)) {
    Expectation.emplace_back(OperandKind::LiteralInteger);
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
  case spv::ExecutionMode::SubgroupSize: {
    Expectation.emplace_back(OperandKind::LiteralInteger);
  } break;
  case spv::ExecutionMode::SubgroupsPerWorkgroup: {
    Expectation.emplace_back(OperandKind::LiteralInteger);
  } break;
  case spv::ExecutionMode::SubgroupsPerWorkgroupId: {
    Expectation.emplace_back(OperandKind::IdRef);
  } break;
  case spv::ExecutionMode::LocalSizeId: {
    Expectation.emplace_back(OperandKind::IdRef);
    Expectation.emplace_back(OperandKind::IdRef);
    Expectation.emplace_back(OperandKind::IdRef);
  } break;
  case spv::ExecutionMode::LocalSizeHintId: {
    Expectation.emplace_back(OperandKind::IdRef);
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
  case spv::Decoration::MaxByteOffset: {
    Expectation.emplace_back(OperandKind::LiteralInteger);
  } break;
  case spv::Decoration::AlignmentId: {
    Expectation.emplace_back(OperandKind::IdRef);
  } break;
  case spv::Decoration::MaxByteOffsetId: {
    Expectation.emplace_back(OperandKind::IdRef);
  } break;
  case spv::Decoration::SecondaryViewportRelativeNV: {
    Expectation.emplace_back(OperandKind::LiteralInteger);
  } break;
  case spv::Decoration::HlslCounterBufferGOOGLE: {
    Expectation.emplace_back(OperandKind::IdRef);
  } break;
  case spv::Decoration::HlslSemanticGOOGLE: {
    Expectation.emplace_back(OperandKind::LiteralString);
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
