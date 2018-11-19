//===-- SpirvType.cpp - SPIR-V Type Hierarchy -------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//===----------------------------------------------------------------------===//
//
//  This file implements the in-memory representation of SPIR-V types.
//
//===----------------------------------------------------------------------===//

#include "clang/SPIRV/SpirvType.h"

namespace clang {
namespace spirv {

bool ScalarType::classof(const SpirvType *t) {
  switch (t->getKind()) {
  case TK_Bool:
  case TK_Integer:
  case TK_Float:
    return true;
  default:
    break;
  }
  return false;
}

bool SpirvType::isTexture(const SpirvType *type) {
  if (const auto *imageType = dyn_cast<ImageType>(type)) {
    const auto dim = imageType->getDimension();
    const auto withSampler = imageType->withSampler();
    return (withSampler == ImageType::WithSampler::Yes) &&
           (dim == spv::Dim::Dim1D || dim == spv::Dim::Dim2D ||
            dim == spv::Dim::Dim3D || dim == spv::Dim::Cube);
  }
  return false;
}

bool SpirvType::isRWTexture(const SpirvType *type) {
  if (const auto *imageType = dyn_cast<ImageType>(type)) {
    const auto dim = imageType->getDimension();
    const auto withSampler = imageType->withSampler();
    return (withSampler == ImageType::WithSampler::No) &&
           (dim == spv::Dim::Dim1D || dim == spv::Dim::Dim2D ||
            dim == spv::Dim::Dim3D);
  }
  return false;
}

bool SpirvType::isSampler(const SpirvType *type) {
  return isa<SamplerType>(type);
}

bool SpirvType::isBuffer(const SpirvType *type) {
  if (const auto *imageType = dyn_cast<ImageType>(type)) {
    const auto dim = imageType->getDimension();
    const auto withSampler = imageType->withSampler();
    return imageType->getDimension() == spv::Dim::Buffer &&
           imageType->withSampler() == ImageType::WithSampler::Yes;
  }
  return false;
}

bool SpirvType::isRWBuffer(const SpirvType *type) {
  if (const auto *imageType = dyn_cast<ImageType>(type)) {
    return imageType->getDimension() == spv::Dim::Buffer &&
           imageType->withSampler() == ImageType::WithSampler::No;
  }
  return false;
}

bool SpirvType::isSubpassInput(const SpirvType *type) {
  if (const auto *imageType = dyn_cast<ImageType>(type)) {
    return imageType->getDimension() == spv::Dim::SubpassData &&
           imageType->isMSImage() == false;
  }
  return false;
}

bool SpirvType::isSubpassInputMS(const SpirvType *type) {
  if (const auto *imageType = dyn_cast<ImageType>(type)) {
    return imageType->getDimension() == spv::Dim::SubpassData &&
           imageType->isMSImage() == true;
  }
  return false;
}

bool SpirvType::isResourceType(const SpirvType *type) {
  if (isa<ImageType>(type) || isa<SamplerType>(type))
    return true;

  if (const auto *structType = dyn_cast<StructType>(type))
    return structType->getInterfaceType() !=
           StructInterfaceType::InternalStorage;

  if (const auto *pointerType = dyn_cast<SpirvPointerType>(type))
    return isResourceType(pointerType->getPointeeType());

  return false;
}

bool SpirvType::isOrContains16BitType(const SpirvType *type) {
  if (const auto *numericType = dyn_cast<NumericalType>(type))
    if (numericType->getBitwidth() == 16)
      return true;

  if (const auto *vecType = dyn_cast<VectorType>(type))
    return isOrContains16BitType(vecType->getElementType());
  if (const auto *matType = dyn_cast<MatrixType>(type))
    return isOrContains16BitType(matType->getElementType());
  if (const auto *arrType = dyn_cast<MatrixType>(type))
    return isOrContains16BitType(arrType->getElementType());
  if (const auto *pointerType = dyn_cast<SpirvPointerType>(type))
    return isOrContains16BitType(pointerType->getPointeeType());
  if (const auto *raType = dyn_cast<MatrixType>(type))
    return isOrContains16BitType(raType->getElementType());
  if (const auto *imgType = dyn_cast<ImageType>(type))
    return isOrContains16BitType(imgType->getSampledType());
  if (const auto *sampledImageType = dyn_cast<SampledImageType>(type))
    return isOrContains16BitType(sampledImageType->getImageType());
  if (const auto *structType = dyn_cast<StructType>(type))
    for (auto &field : structType->getFields())
      if (isOrContains16BitType(field.type))
        return true;

  return false;
}

MatrixType::MatrixType(const VectorType *vecType, uint32_t vecCount,
                       bool rowMajor)
    : SpirvType(TK_Matrix), vectorType(vecType), vectorCount(vecCount),
      isRowMajor(rowMajor) {}

bool MatrixType::operator==(const MatrixType &that) const {
  return vectorType == that.vectorType && vectorCount == that.vectorCount &&
         isRowMajor == that.isRowMajor;
}

ImageType::ImageType(const NumericalType *type, spv::Dim dim, WithDepth depth,
                     bool arrayed, bool ms, WithSampler sampled,
                     spv::ImageFormat format)
    : SpirvType(TK_Image), sampledType(type), dimension(dim), imageDepth(depth),
      isArrayed(arrayed), isMultiSampled(ms), isSampled(sampled),
      imageFormat(format) {}

bool ImageType::operator==(const ImageType &that) const {
  return sampledType == that.sampledType && dimension == that.dimension &&
         isArrayed == that.isArrayed && isMultiSampled == that.isMultiSampled &&
         isSampled == that.isSampled && imageFormat == that.imageFormat;
}

StructType::StructType(llvm::ArrayRef<StructType::FieldInfo> fieldsVec,
                       llvm::StringRef name, bool isReadOnly,
                       StructInterfaceType iface)
    : SpirvType(TK_Struct), fields(fieldsVec.begin(), fieldsVec.end()),
      structName(name), readOnly(isReadOnly), interfaceType(iface) {}

bool StructType::FieldInfo::
operator==(const StructType::FieldInfo &that) const {
  return type == that.type && name == that.name &&
         vkOffsetAttr == that.vkOffsetAttr &&
         packOffsetAttr == that.packOffsetAttr;
}

bool StructType::operator==(const StructType &that) const {
  return fields == that.fields && structName == that.structName &&
         readOnly == that.readOnly;
}

HybridStructType::HybridStructType(
    llvm::ArrayRef<HybridStructType::FieldInfo> fieldsVec, llvm::StringRef name,
    bool isReadOnly, StructInterfaceType iface)
    : HybridType(TK_HybridStruct), fields(fieldsVec.begin(), fieldsVec.end()),
      structName(name), readOnly(isReadOnly), interfaceType(iface) {}

bool HybridStructType::FieldInfo::
operator==(const HybridStructType::FieldInfo &that) const {
  return astType == that.astType && name == that.name &&
         vkOffsetAttr == that.vkOffsetAttr &&
         packOffsetAttr == that.packOffsetAttr;
}

bool HybridStructType::operator==(const HybridStructType &that) const {
  return fields == that.fields && structName == that.structName &&
         readOnly == that.readOnly;
}

} // namespace spirv
} // namespace clang
