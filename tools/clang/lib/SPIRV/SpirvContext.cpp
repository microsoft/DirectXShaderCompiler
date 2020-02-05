//===--- SpirvContext.cpp - SPIR-V SpirvContext implementation-------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include <algorithm>
#include <tuple>

#include "clang/SPIRV/SpirvContext.h"

namespace clang {
namespace spirv {

SpirvContext::SpirvContext()
    : allocator(), voidType(nullptr), boolType(nullptr), sintTypes({}),
      uintTypes({}), floatTypes({}), samplerType(nullptr),
      curShaderModelKind(ShaderModelKind::Invalid), majorVersion(0),
      minorVersion(0) {
  voidType = new (this) VoidType;
  boolType = new (this) BoolType;
  samplerType = new (this) SamplerType;
  accelerationStructureTypeNV = new (this) AccelerationStructureTypeNV;
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

const SpirvType *SpirvContext::getMatrixType(const SpirvType *elemType,
                                             uint32_t count) {
  // We are certain this should be a vector type. Otherwise, cast causes an
  // assertion failure.
  const VectorType *vecType = cast<VectorType>(elemType);
  assert(count == 2 || count == 3 || count == 4);

  // In the case of non-floating-point matrices, we represent them as array of
  // vectors.
  if (!isa<FloatType>(vecType->getElementType())) {
    return getArrayType(elemType, count, llvm::None);
  }

  auto foundVec = matTypes.find(vecType);

  if (foundVec != matTypes.end()) {
    const auto &matVector = foundVec->second;
    // Create a temporary object for finding in the vector.
    MatrixType type(vecType, count);

    for (const auto *cachedType : matVector)
      if (type == *cachedType)
        return cachedType;
  }

  const auto *ptr = new (this) MatrixType(vecType, count);

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

  // Create a temporary object for finding in the set.
  ImageType type(elemType, dim, depth, arrayed, ms, sampled, format);
  auto found = imageTypes.find(&type);
  if (found != imageTypes.end())
    return *found;

  auto inserted = imageTypes.insert(
      new (this) ImageType(elemType, dim, depth, arrayed, ms, sampled, format));

  return *(inserted.first);
}

const SampledImageType *
SpirvContext::getSampledImageType(const ImageType *image) {
  auto found = sampledImageTypes.find(image);

  if (found != sampledImageTypes.end())
    return found->second;

  return sampledImageTypes[image] = new (this) SampledImageType(image);
}

const HybridSampledImageType *
SpirvContext::getSampledImageType(QualType image) {
  return new (this) HybridSampledImageType(image);
}

const ArrayType *
SpirvContext::getArrayType(const SpirvType *elemType, uint32_t elemCount,
                           llvm::Optional<uint32_t> arrayStride) {
  ArrayType type(elemType, elemCount, arrayStride);

  auto found = arrayTypes.find(&type);
  if (found != arrayTypes.end())
    return *found;

  auto inserted =
      arrayTypes.insert(new (this) ArrayType(elemType, elemCount, arrayStride));
  // The return value is an (iterator, bool) pair. The boolean indicates whether
  // it was actually added as a new type.
  return *(inserted.first);
}

const RuntimeArrayType *
SpirvContext::getRuntimeArrayType(const SpirvType *elemType,
                                  llvm::Optional<uint32_t> arrayStride) {
  RuntimeArrayType type(elemType, arrayStride);
  auto found = runtimeArrayTypes.find(&type);
  if (found != runtimeArrayTypes.end())
    return *found;

  auto inserted = runtimeArrayTypes.insert(
      new (this) RuntimeArrayType(elemType, arrayStride));
  return *(inserted.first);
}

const StructType *
SpirvContext::getStructType(llvm::ArrayRef<StructType::FieldInfo> fields,
                            llvm::StringRef name, bool isReadOnly,
                            StructInterfaceType interfaceType) {
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
    bool isReadOnly, StructInterfaceType interfaceType) {
  return new (this) HybridStructType(fields, name, isReadOnly, interfaceType);
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

const HybridPointerType *SpirvContext::getPointerType(QualType pointee,
                                                      spv::StorageClass sc) {
  return new (this) HybridPointerType(pointee, sc);
}

FunctionType *
SpirvContext::getFunctionType(const SpirvType *ret,
                              llvm::ArrayRef<const SpirvType *> param) {
  // Create a temporary object for finding in the set.
  FunctionType type(ret, param);
  auto found = functionTypes.find(&type);
  if (found != functionTypes.end())
    return *found;

  auto inserted = functionTypes.insert(new (this) FunctionType(ret, param));
  return *inserted.first;
}

const StructType *SpirvContext::getByteAddressBufferType(bool isWritable) {
  // Create a uint RuntimeArray.
  const auto *raType =
      getRuntimeArrayType(getUIntType(32), /* ArrayStride */ 4);

  // Create a struct containing the runtime array as its only member.
  return getStructType(
      {StructType::FieldInfo(raType, /*name*/ "", /*offset*/ 0)},
      isWritable ? "type.RWByteAddressBuffer" : "type.ByteAddressBuffer",
      !isWritable, StructInterfaceType::StorageBuffer);
}

const StructType *SpirvContext::getACSBufferCounterType() {
  // Create int32.
  const auto *int32Type = getSIntType(32);

  // Create a struct containing the integer counter as its only member.
  const StructType *type =
      getStructType({StructType::FieldInfo(int32Type, "counter", /*offset*/ 0)},
                    "type.ACSBuffer.counter",
                    /*isReadOnly*/ false, StructInterfaceType::StorageBuffer);

  return type;
}

SpirvDebugInstruction *
SpirvContext::getDebugTypeBasic(const SpirvType *spirvType,
                                llvm::StringRef name, SpirvConstant *size,
                                uint32_t encoding) {
  // Reuse existing debug type if possible.
  if (debugTypes.find(spirvType) != debugTypes.end())
    return debugTypes[spirvType];

  auto *debugType = new (this) SpirvDebugTypeBasic(name, size, encoding);
  debugTypes[spirvType] = debugType;
  return debugType;
}

SpirvDebugInstruction *
SpirvContext::getDebugTypeArray(const SpirvType *spirvType,
                                SpirvDebugInstruction *elemType,
                                llvm::ArrayRef<uint32_t> elemCount) {
  // Reuse existing debug type if possible.
  if (debugTypes.find(spirvType) != debugTypes.end())
    return debugTypes[spirvType];

  auto *eTy = dyn_cast<SpirvDebugType>(elemType);
  assert(eTy && "Element type must be a SpirvDebugType.");
  auto *debugType = new (this) SpirvDebugTypeArray(eTy, elemCount);
  debugTypes[spirvType] = debugType;
  return debugType;
}

SpirvDebugInstruction *
SpirvContext::getDebugTypeVector(const SpirvType *spirvType,
                                 SpirvDebugInstruction *elemType,
                                 uint32_t elemCount) {
  // Reuse existing debug type if possible.
  if (debugTypes.find(spirvType) != debugTypes.end())
    return debugTypes[spirvType];

  auto *debugType = new (this) SpirvDebugTypeVector(elemType, elemCount);
  debugTypes[spirvType] = debugType;
  return debugType;
}
SpirvDebugInstruction* SpirvContext::getDebugTypeFunction(
  const SpirvType* spirvType, uint32_t flags, SpirvDebugInstruction* ret,
  llvm::ArrayRef<SpirvDebugInstruction*> params) {
  // Reuse existing debug type if possible.
  if (debugTypes.find(spirvType) != debugTypes.end())
    return debugTypes[spirvType];

  auto *debugType = new (this) SpirvDebugTypeFunction(flags, ret, params);
  debugTypes[spirvType] = debugType;
  return debugType;
}

} // end namespace spirv
} // end namespace clang
