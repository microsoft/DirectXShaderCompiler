//===--- Type.cpp - SPIR-V Type implementation-----------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/SPIRV/Type.h"
#include "clang/SPIRV/SPIRVContext.h"
#include "clang/SPIRV/String.h"

namespace clang {
namespace spirv {

Type::Type(spv::Op op, std::vector<uint32_t> arg, DecorationSet decs,
           llvm::StringRef n)
    : opcode(op), args(std::move(arg)), name(n.str()) {
  decorations = llvm::SetVector<const Decoration *>(decs.begin(), decs.end());
}

const Type *Type::getUniqueType(SPIRVContext &context, const Type &t) {
  return context.registerType(t);
}
const Type *Type::getVoid(SPIRVContext &context) {
  Type t = Type(spv::Op::OpTypeVoid, {});
  return getUniqueType(context, t);
}
const Type *Type::getBool(SPIRVContext &context) {
  Type t = Type(spv::Op::OpTypeBool, {});
  return getUniqueType(context, t);
}
const Type *Type::getInt8(SPIRVContext &context) {
  Type t = Type(spv::Op::OpTypeInt, {8, 1});
  return getUniqueType(context, t);
}
const Type *Type::getUint8(SPIRVContext &context) {
  Type t = Type(spv::Op::OpTypeInt, {8, 0});
  return getUniqueType(context, t);
}
const Type *Type::getInt16(SPIRVContext &context) {
  Type t = Type(spv::Op::OpTypeInt, {16, 1});
  return getUniqueType(context, t);
}
const Type *Type::getUint16(SPIRVContext &context) {
  Type t = Type(spv::Op::OpTypeInt, {16, 0});
  return getUniqueType(context, t);
}
const Type *Type::getInt32(SPIRVContext &context) {
  Type t = Type(spv::Op::OpTypeInt, {32, 1});
  return getUniqueType(context, t);
}
const Type *Type::getUint32(SPIRVContext &context) {
  Type t = Type(spv::Op::OpTypeInt, {32, 0});
  return getUniqueType(context, t);
}
const Type *Type::getInt64(SPIRVContext &context) {
  Type t = Type(spv::Op::OpTypeInt, {64, 1});
  return getUniqueType(context, t);
}
const Type *Type::getUint64(SPIRVContext &context) {
  Type t = Type(spv::Op::OpTypeInt, {64, 0});
  return getUniqueType(context, t);
}
const Type *Type::getFloat16(SPIRVContext &context) {
  Type t = Type(spv::Op::OpTypeFloat, {16});
  return getUniqueType(context, t);
}
const Type *Type::getFloat32(SPIRVContext &context) {
  Type t = Type(spv::Op::OpTypeFloat, {32});
  return getUniqueType(context, t);
}
const Type *Type::getFloat64(SPIRVContext &context) {
  Type t = Type(spv::Op::OpTypeFloat, {64});
  return getUniqueType(context, t);
}
const Type *Type::getVec2(SPIRVContext &context, uint32_t component_type) {
  Type t = Type(spv::Op::OpTypeVector, {component_type, 2u});
  return getUniqueType(context, t);
}
const Type *Type::getVec3(SPIRVContext &context, uint32_t component_type) {
  Type t = Type(spv::Op::OpTypeVector, {component_type, 3u});
  return getUniqueType(context, t);
}
const Type *Type::getVec4(SPIRVContext &context, uint32_t component_type) {
  Type t = Type(spv::Op::OpTypeVector, {component_type, 4u});
  return getUniqueType(context, t);
}
const Type *Type::getMatrix(SPIRVContext &context, uint32_t column_type_id,
                            uint32_t column_count) {
  Type t = Type(spv::Op::OpTypeMatrix, {column_type_id, column_count});
  return getUniqueType(context, t);
}
const Type *
Type::getImage(SPIRVContext &context, uint32_t sampled_type, spv::Dim dim,
               uint32_t depth, uint32_t arrayed, uint32_t ms, uint32_t sampled,
               spv::ImageFormat image_format,
               llvm::Optional<spv::AccessQualifier> access_qualifier,
               DecorationSet d) {
  std::vector<uint32_t> args = {
      sampled_type, uint32_t(dim),         depth, arrayed, ms,
      sampled,      uint32_t(image_format)};
  if (access_qualifier.hasValue()) {
    args.push_back(static_cast<uint32_t>(access_qualifier.getValue()));
  }
  Type t = Type(spv::Op::OpTypeImage, args, d);
  return getUniqueType(context, t);
}
const Type *Type::getSampler(SPIRVContext &context, DecorationSet d) {
  Type t = Type(spv::Op::OpTypeSampler, {}, d);
  return getUniqueType(context, t);
}
const Type *Type::getSampledImage(SPIRVContext &context, uint32_t image_type_id,
                                  DecorationSet d) {
  Type t = Type(spv::Op::OpTypeSampledImage, {image_type_id}, d);
  return getUniqueType(context, t);
}
const Type *Type::getArray(SPIRVContext &context, uint32_t component_type_id,
                           uint32_t len_id, DecorationSet d) {
  Type t = Type(spv::Op::OpTypeArray, {component_type_id, len_id}, d);
  return getUniqueType(context, t);
}
const Type *Type::getRuntimeArray(SPIRVContext &context,
                                  uint32_t component_type_id, DecorationSet d) {
  Type t = Type(spv::Op::OpTypeRuntimeArray, {component_type_id}, d);
  return getUniqueType(context, t);
}
const Type *Type::getStruct(SPIRVContext &context,
                            llvm::ArrayRef<uint32_t> members,
                            llvm::StringRef name, DecorationSet d) {
  Type t = Type(spv::Op::OpTypeStruct, std::vector<uint32_t>(members), d, name);
  return getUniqueType(context, t);
}
const Type *Type::getPointer(SPIRVContext &context,
                             spv::StorageClass storage_class, uint32_t type,
                             DecorationSet d) {
  Type t = Type(spv::Op::OpTypePointer,
                {static_cast<uint32_t>(storage_class), type}, d);
  return getUniqueType(context, t);
}
const Type *Type::getFunction(SPIRVContext &context, uint32_t return_type,
                              const std::vector<uint32_t> &params,
                              DecorationSet d) {
  std::vector<uint32_t> args = {return_type};
  args.insert(args.end(), params.begin(), params.end());
  Type t = Type(spv::Op::OpTypeFunction, args, d);
  return getUniqueType(context, t);
}

bool Type::operator==(const Type &other) const {
  if (opcode == other.opcode && args == other.args &&
      decorations.size() == other.decorations.size() && name == other.name) {
    // If two types have the same decorations, but in different order,
    // they are in fact the same type.
    for (const Decoration *dec : decorations) {
      if (other.decorations.count(dec) == 0)
        return false;
    }
    return true;
  }
  return false;
}

bool Type::isBooleanType() const { return opcode == spv::Op::OpTypeBool; }
bool Type::isIntegerType() const { return opcode == spv::Op::OpTypeInt; }
bool Type::isFloatType() const { return opcode == spv::Op::OpTypeFloat; }
bool Type::isNumericalType() const { return isFloatType() || isIntegerType(); }
bool Type::isScalarType() const { return isBooleanType() || isNumericalType(); }
bool Type::isVectorType() const { return opcode == spv::Op::OpTypeVector; }
bool Type::isMatrixType() const { return opcode == spv::Op::OpTypeMatrix; }
bool Type::isArrayType() const { return opcode == spv::Op::OpTypeArray; }
bool Type::isStructureType() const { return opcode == spv::Op::OpTypeStruct; }
bool Type::isAggregateType() const {
  return isStructureType() || isArrayType();
}
bool Type::isCompositeType() const {
  return isAggregateType() || isMatrixType() || isVectorType();
}
bool Type::isImageType() const { return opcode == spv::Op::OpTypeImage; }

bool Type::hasDecoration(const Decoration *d) const {
  return decorations.count(d);
}

std::vector<uint32_t> Type::withResultId(uint32_t resultId) const {
  std::vector<uint32_t> words;

  // TODO: we are essentially duplicate the work InstBuilder is responsible for.
  // Should figure out a way to unify them.
  words.reserve(2 + args.size());
  words.push_back(static_cast<uint32_t>(opcode));
  words.push_back(resultId);
  words.insert(words.end(), args.begin(), args.end());
  words.front() |= static_cast<uint32_t>(words.size()) << 16;

  return words;
}

} // end namespace spirv
} // end namespace clang
