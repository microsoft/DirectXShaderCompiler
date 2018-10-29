//===--- SPIRVContext.cpp - SPIR-V SPIRVContext implementation-------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include <algorithm>
#include <tuple>

#include "clang/SPIRV/SPIRVContext.h"

namespace clang {
namespace spirv {

uint32_t SPIRVContext::getResultIdForType(const Type *t, bool *isRegistered) {
  assert(t != nullptr);
  uint32_t result_id = 0;

  auto iter = typeResultIdMap.find(t);
  if (iter == typeResultIdMap.end()) {
    // The Type has not been defined yet. Reserve an ID for it.
    result_id = takeNextId();
    typeResultIdMap[t] = result_id;
    if (isRegistered)
      *isRegistered = false;
  } else {
    result_id = iter->second;
    if (isRegistered)
      *isRegistered = true;
  }

  assert(result_id != 0);
  return result_id;
}

uint32_t SPIRVContext::getResultIdForConstant(const Constant *c) {
  assert(c != nullptr);
  uint32_t result_id = 0;

  auto iter = constantResultIdMap.find(c);
  if (iter == constantResultIdMap.end()) {
    // The constant has not been defined yet. Reserve an ID for it.
    result_id = takeNextId();
    constantResultIdMap[c] = result_id;
  } else {
    result_id = iter->second;
  }

  assert(result_id != 0);
  return result_id;
}

const Type *SPIRVContext::registerType(const Type &t) {
  // Insert function will only insert if it doesn't already exist in the set.
  TypeSet::iterator it;
  std::tie(it, std::ignore) = existingTypes.insert(t);
  return &*it;
}

const Constant *SPIRVContext::registerConstant(const Constant &c) {
  // Insert function will only insert if it doesn't already exist in the set.
  ConstantSet::iterator it;
  std::tie(it, std::ignore) = existingConstants.insert(c);
  return &*it;
}

const Decoration *SPIRVContext::registerDecoration(const Decoration &d) {
  // Insert function will only insert if it doesn't already exist in the set.
  DecorationSet::iterator it;
  std::tie(it, std::ignore) = existingDecorations.insert(d);
  return &*it;
}

SpirvContext::SpirvContext()
    : allocator(), voidType(nullptr), boolType(nullptr), sintTypes({}),
      uintTypes({}), floatTypes({}), samplerType(nullptr) {
  voidType = new (this) VoidType;
  boolType = new (this) BoolType;
  samplerType = new (this) SamplerType;
}

inline uint32_t log2ForBitwidth(uint32_t bitwidth) {
  assert(bitwidth >= 16 && bitwidth <= 64 && llvm::isPowerOf2_32(bitwidth));

  return llvm::Log2_32(bitwidth);
}

const IntegerType *SpirvContext::getSIntType(uint32_t bitwidth) {
  auto &type = sintTypes[log2ForBitwidth(bitwidth)];

  if (type == nullptr) {
    type = new (this) IntegerType(bitwidth, true);
  }
  return type;
}

const IntegerType *SpirvContext::getUIntType(uint32_t bitwidth) {
  auto &type = uintTypes[log2ForBitwidth(bitwidth)];

  if (type == nullptr) {
    type = new (this) IntegerType(bitwidth, false);
  }
  return type;
}

const FloatType *SpirvContext::getFloatType(uint32_t bitwidth) {
  auto &type = floatTypes[log2ForBitwidth(bitwidth)];

  if (type == nullptr) {
    type = new (this) FloatType(bitwidth);
  }
  return type;
}

const VectorType *SpirvContext::getVectorType(const SpirvType *elemType,
                                              uint32_t count) {
  // We are certain this should be a scalar type. Otherwise, cast causes an
  // assertion failure.
  const ScalarType *scalarType = cast<ScalarType>(elemType);
  assert(count == 2 || count == 3 || count == 4);

  auto found = vecTypes.find(scalarType);

  if (found != vecTypes.end()) {
    auto &type = found->second[count];
    if (type != nullptr)
      return type;
  } else {
    // Make sure to initialize since std::array is "an aggregate type with the
    // same semantics as a struct holding a C-style array T[N]".
    vecTypes[scalarType] = {};
  }

  return vecTypes[scalarType][count] = new (this) VectorType(scalarType, count);
}

const MatrixType *SpirvContext::getMatrixType(const SpirvType *elemType,
                                              uint32_t count, bool isRowMajor) {
  // We are certain this should be a vector type. Otherwise, cast causes an
  // assertion failure.
  const VectorType *vecType = cast<VectorType>(elemType);
  assert(count == 2 || count == 3 || count == 4);

  auto foundVec = matTypes.find(vecType);

  if (foundVec != matTypes.end()) {
    const auto &matVector = foundVec->second;
    // Create a temporary object for finding in the vector.
    MatrixType type(vecType, count, isRowMajor);

    for (const auto *cachedType : matVector)
      if (type == *cachedType)
        return cachedType;
  }

  const auto *ptr = new (this) MatrixType(vecType, count, isRowMajor);

  matTypes[vecType].push_back(ptr);

  return ptr;
}

const ImageType *SpirvContext::getImageType(const SpirvType *sampledType,
                                            spv::Dim dim,
                                            ImageType::WithDepth depth,
                                            bool arrayed, bool ms,
                                            ImageType::WithSampler sampled,
                                            spv::ImageFormat format) {
  // We are certain this should be a numerical type. Otherwise, cast causes an
  // assertion failure.
  const NumericalType *elemType = cast<NumericalType>(sampledType);

  // Create a temporary object for finding in the vector.
  ImageType type(elemType, dim, depth, arrayed, ms, sampled, format);

  auto found = std::find_if(
      imageTypes.begin(), imageTypes.end(),
      [&type](const ImageType *cachedType) { return type == *cachedType; });

  if (found != imageTypes.end())
    return *found;

  imageTypes.push_back(
      new (this) ImageType(elemType, dim, depth, arrayed, ms, sampled, format));

  return imageTypes.back();
}

const SampledImageType *
SpirvContext::getSampledImageType(const ImageType *image) {
  auto found = sampledImageTypes.find(image);

  if (found != sampledImageTypes.end())
    return found->second;

  return sampledImageTypes[image] = new (this) SampledImageType(image);
}

const ArrayType *SpirvContext::getArrayType(const SpirvType *elemType,
                                            uint32_t elemCount) {
  auto foundElemType = arrayTypes.find(elemType);

  if (foundElemType != arrayTypes.end()) {
    auto &elemTypeMap = foundElemType->second;
    auto foundCount = elemTypeMap.find(elemCount);

    if (foundCount != elemTypeMap.end())
      return foundCount->second;
  }

  return arrayTypes[elemType][elemCount] =
             new (this) ArrayType(elemType, elemCount);
}

const RuntimeArrayType *
SpirvContext::getRuntimeArrayType(const SpirvType *elemType) {
  auto found = runtimeArrayTypes.find(elemType);

  if (found != runtimeArrayTypes.end())
    return found->second;

  return runtimeArrayTypes[elemType] = new (this) RuntimeArrayType(elemType);
}

const StructType *
SpirvContext::getStructType(llvm::ArrayRef<StructType::FieldInfo> fields,
                            llvm::StringRef name, bool isReadOnly,
                            StructType::InterfaceType interfaceType) {
  // We are creating a temporary struct type here for querying whether the
  // same type was already created. It is a little bit costly, but we can
  // avoid allocating directly from the bump pointer allocator, from which
  // then we are unable to reclaim until the allocator itself is destroyed.

  StructType type(fields, name, isReadOnly, interfaceType);

  auto found = std::find_if(
      structTypes.begin(), structTypes.end(),
      [&type](const StructType *cachedType) { return type == *cachedType; });

  if (found != structTypes.end())
    return *found;

  structTypes.push_back(
      new (this) StructType(fields, name, isReadOnly, interfaceType));

  return structTypes.back();
}

const HybridStructType *SpirvContext::getHybridStructType(
    llvm::ArrayRef<HybridStructType::FieldInfo> fields, llvm::StringRef name,
    bool isReadOnly, HybridStructType::InterfaceType interfaceType) {
  // We are creating a temporary struct type here for querying whether the
  // same type was already created. It is a little bit costly, but we can
  // avoid allocating directly from the bump pointer allocator, from which
  // then we are unable to reclaim until the allocator itself is destroyed.

  HybridStructType type(fields, name, isReadOnly, interfaceType);

  auto found = std::find_if(
      hybridStructTypes.begin(), hybridStructTypes.end(),
      [&type](const HybridStructType *cachedType) { return type == *cachedType; });

  if (found != hybridStructTypes.end())
    return *found;

  hybridStructTypes.push_back(
      new (this) HybridStructType(fields, name, isReadOnly, interfaceType));

  return hybridStructTypes.back();
}

const SpirvPointerType *SpirvContext::getPointerType(const SpirvType *pointee,
                                                     spv::StorageClass sc) {
  auto foundPointee = pointerTypes.find(pointee);

  if (foundPointee != pointerTypes.end()) {
    auto &pointeeMap = foundPointee->second;
    auto foundSC = pointeeMap.find(sc);

    if (foundSC != pointeeMap.end())
      return foundSC->second;
  }

  return pointerTypes[pointee][sc] = new (this) SpirvPointerType(pointee, sc);
}

const FunctionType *
SpirvContext::getFunctionType(const SpirvType *ret,
                              llvm::ArrayRef<const SpirvType *> param) {
  // Create a temporary object for finding in the vector.
  FunctionType type(ret, param);

  auto found = std::find_if(
      functionTypes.begin(), functionTypes.end(),
      [&type](const FunctionType *cachedType) { return type == *cachedType; });

  if (found != functionTypes.end())
    return *found;

  functionTypes.push_back(new (this) FunctionType(ret, param));

  return functionTypes.back();
}

const StructType *SpirvContext::getByteAddressBufferType(bool isWritable) {
  // Create a uint RuntimeArray.
  const auto *raType = getRuntimeArrayType(getUIntType(32));

  // Create a struct containing the runtime array as its only member.
  return getStructType({StructType::FieldInfo(raType)},
                       isWritable ? "type.RWByteAddressBuffer"
                                  : "type.ByteAddressBuffer",
                       !isWritable);
}

const StructType *SpirvContext::getACSBufferCounterType() {
  // Create int32.
  const auto *int32Type = getSIntType(32);

  // Create a struct containing the integer counter as its only member.
  const StructType *type =
      getStructType({int32Type}, "type.ACSBuffer.counter", {"counter"});

  return type;
}

SpirvConstant *SpirvContext::getConstantUint32(uint32_t value) {
  const IntegerType *intType = getUIntType(32);
  SpirvConstantInteger tempConstant(intType, value);

  auto found =
      std::find_if(integerConstants.begin(), integerConstants.end(),
                   [&tempConstant](SpirvConstantInteger *cachedConstant) {
                     return tempConstant == *cachedConstant;
                   });

  if (found != integerConstants.end())
    return *found;

  // Couldn't find the constant. Create one.
  auto *intConst = new (this) SpirvConstantInteger(intType, value);
  integerConstants.push_back(intConst);
  return intConst;
}

SpirvConstant *SpirvContext::getConstantInt32(int32_t value) {
  const IntegerType *intType = getSIntType(32);
  SpirvConstantInteger tempConstant(intType, value);

  auto found =
      std::find_if(integerConstants.begin(), integerConstants.end(),
                   [&tempConstant](SpirvConstantInteger *cachedConstant) {
                     return tempConstant == *cachedConstant;
                   });

  if (found != integerConstants.end())
    return *found;

  // Couldn't find the constant. Create one.
  auto *intConst = new (this) SpirvConstantInteger(intType, value);
  integerConstants.push_back(intConst);
  return intConst;
}

} // end namespace spirv
} // end namespace clang
