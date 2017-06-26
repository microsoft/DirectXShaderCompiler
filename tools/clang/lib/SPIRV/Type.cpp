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

Type::Type(spv::Op op, std::vector<uint32_t> arg,
           std::set<const Decoration *> decs)
    : opcode(op), args(arg), decorations(decs) {}

const Type *Type::getUniqueType(SPIRVContext &context, const Type &t) {
  return context.registerType(t);
}
const Type *Type::getVoid(SPIRVContext &context) {
  Type t = Type(spv::Op::OpTypeVoid);
  return getUniqueType(context, t);
}
const Type *Type::getBool(SPIRVContext &context) {
  Type t = Type(spv::Op::OpTypeBool);
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
const Type *Type::getVector(SPIRVContext &context, uint32_t component_type,
                            uint32_t vec_size) {
  Type t = Type(spv::Op::OpTypeVector, {component_type, vec_size});
  return getUniqueType(context, t);
}
const Type *Type::getVec2(SPIRVContext &context, uint32_t component_type) {
  return getVector(context, component_type, 2u);
}
const Type *Type::getVec3(SPIRVContext &context, uint32_t component_type) {
  return getVector(context, component_type, 3u);
}
const Type *Type::getVec4(SPIRVContext &context, uint32_t component_type) {
  return getVector(context, component_type, 4u);
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
               llvm::Optional<spv::AccessQualifier> access_qualifier) {
  std::vector<uint32_t> args = {
      sampled_type, uint32_t(dim),         depth, arrayed, ms,
      sampled,      uint32_t(image_format)};
  if (access_qualifier.hasValue()) {
    args.push_back(static_cast<uint32_t>(access_qualifier.getValue()));
  }
  Type t = Type(spv::Op::OpTypeImage, args);
  return getUniqueType(context, t);
}
const Type *Type::getSampler(SPIRVContext &context) {
  Type t = Type(spv::Op::OpTypeSampler);
  return getUniqueType(context, t);
}
const Type *Type::getSampledImage(SPIRVContext &context,
                                  uint32_t image_type_id) {
  Type t = Type(spv::Op::OpTypeSampledImage, {image_type_id});
  return getUniqueType(context, t);
}
const Type *Type::getArray(SPIRVContext &context, uint32_t component_type_id,
                           uint32_t len_id) {
  Type t = Type(spv::Op::OpTypeArray, {component_type_id, len_id});
  return getUniqueType(context, t);
}
const Type *Type::getRuntimeArray(SPIRVContext &context,
                                  uint32_t component_type_id) {
  Type t = Type(spv::Op::OpTypeRuntimeArray, {component_type_id});
  return getUniqueType(context, t);
}
const Type *Type::getStruct(SPIRVContext &context,
                            std::initializer_list<uint32_t> members) {
  Type t = Type(spv::Op::OpTypeStruct, std::vector<uint32_t>(members));
  return getUniqueType(context, t);
}
const Type *Type::getOpaque(SPIRVContext &context, std::string name) {
  Type t = Type(spv::Op::OpTypeOpaque, string::encodeSPIRVString(name));
  return getUniqueType(context, t);
}
const Type *Type::getTyePointer(SPIRVContext &context,
                                spv::StorageClass storage_class,
                                uint32_t type) {
  Type t = Type(spv::Op::OpTypePointer,
                {static_cast<uint32_t>(storage_class), type});
  return getUniqueType(context, t);
}
const Type *Type::getFunction(SPIRVContext &context, uint32_t return_type,
                              std::initializer_list<uint32_t> params) {
  std::vector<uint32_t> args = {return_type};
  args.insert(args.end(), params.begin(), params.end());
  Type t = Type(spv::Op::OpTypeFunction, args);
  return getUniqueType(context, t);
}
const Type *Type::getEvent(SPIRVContext &context) {
  Type t = Type(spv::Op::OpTypeEvent);
  return getUniqueType(context, t);
}
const Type *Type::getDeviceEvent(SPIRVContext &context) {
  Type t = Type(spv::Op::OpTypeDeviceEvent);
  return getUniqueType(context, t);
}
const Type *Type::getQueue(SPIRVContext &context) {
  Type t = Type(spv::Op::OpTypeQueue);
  return getUniqueType(context, t);
}
const Type *Type::getPipe(SPIRVContext &context,
                          spv::AccessQualifier qualifier) {
  Type t = Type(spv::Op::OpTypePipe, {static_cast<uint32_t>(qualifier)});
  return getUniqueType(context, t);
}
const Type *Type::getForwardPointer(SPIRVContext &context,
                                    uint32_t pointer_type,
                                    spv::StorageClass storage_class) {
  Type t = Type(spv::Op::OpTypeForwardPointer,
                {pointer_type, static_cast<uint32_t>(storage_class)});
  return getUniqueType(context, t);
}
const Type *Type::getType(SPIRVContext &context, spv::Op op,
                          std::vector<uint32_t> arg,
                          std::set<const Decoration *> dec) {
  Type t = Type(op, arg, dec);
  return getUniqueType(context, t);
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

} // end namespace spirv
} // end namespace clang
