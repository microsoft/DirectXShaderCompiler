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
  if (const auto *arrType = dyn_cast<ArrayType>(type))
    return isOrContains16BitType(arrType->getElementType());
  if (const auto *pointerType = dyn_cast<SpirvPointerType>(type))
    return isOrContains16BitType(pointerType->getPointeeType());
  if (const auto *raType = dyn_cast<RuntimeArrayType>(type))
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

bool SpirvType::isMatrixOrArrayOfMatrix(const SpirvType *type) {
  if (isa<MatrixType>(type))
    return true;
  if (const auto *arrayType = dyn_cast<ArrayType>(type))
    return isMatrixOrArrayOfMatrix(arrayType->getElementType());

  return false;
}

MatrixType::MatrixType(const VectorType *vecType, uint32_t vecCount)
    : SpirvType(TK_Matrix), vectorType(vecType), vectorCount(vecCount) {}

bool MatrixType::operator==(const MatrixType &that) const {
  return vectorType == that.vectorType && vectorCount == that.vectorCount;
}

ImageType::ImageType(const NumericalType *type, spv::Dim dim, WithDepth depth,
                     bool arrayed, bool ms, WithSampler sampled,
                     spv::ImageFormat format)
    : SpirvType(TK_Image, getImageName(dim, arrayed)), sampledType(type),
      dimension(dim), imageDepth(depth), isArrayed(arrayed), isMultiSampled(ms),
      isSampled(sampled), imageFormat(format) {}

std::string ImageType::getImageName(spv::Dim dim, bool arrayed) {
  const char *dimStr = "";
  switch (dim) {
  case spv::Dim::Dim1D:
    dimStr = "1d.";
    break;
  case spv::Dim::Dim2D:
    dimStr = "2d.";
    break;
  case spv::Dim::Dim3D:
    dimStr = "3d.";
    break;
  case spv::Dim::Cube:
    dimStr = "cube.";
    break;
  case spv::Dim::Rect:
    dimStr = "rect.";
    break;
  case spv::Dim::Buffer:
    dimStr = "buffer.";
    break;
  case spv::Dim::SubpassData:
    dimStr = "subpass.";
    break;
  default:
    break;
  }
  std::string name =
      std::string("type.") + dimStr + "image" + (arrayed ? ".array" : "");
  return name;
}

bool ImageType::operator==(const ImageType &that) const {
  return sampledType == that.sampledType && dimension == that.dimension &&
         isArrayed == that.isArrayed && isMultiSampled == that.isMultiSampled &&
         isSampled == that.isSampled && imageFormat == that.imageFormat;
}

bool ArrayType::operator==(const ArrayType &that) const {
  return elementType == that.elementType && elementCount == that.elementCount &&
         rowMajorElem.hasValue() == that.rowMajorElem.hasValue() &&
         (!rowMajorElem.hasValue() ||
          rowMajorElem.getValue() == that.rowMajorElem.getValue());
}

StructType::StructType(llvm::ArrayRef<StructType::FieldInfo> fieldsVec,
                       llvm::StringRef name, bool isReadOnly,
                       StructInterfaceType iface)
    : SpirvType(TK_Struct, name), fields(fieldsVec.begin(), fieldsVec.end()),
      readOnly(isReadOnly), interfaceType(iface) {}

bool StructType::FieldInfo::
operator==(const StructType::FieldInfo &that) const {
  return type == that.type && vkOffsetAttr == that.vkOffsetAttr &&
         packOffsetAttr == that.packOffsetAttr &&
         isRowMajor.hasValue() == that.isRowMajor.hasValue() &&
         (!isRowMajor.hasValue() ||
          isRowMajor.getValue() == that.isRowMajor.getValue());
}

bool StructType::operator==(const StructType &that) const {
  return fields == that.fields && getName() == that.getName() &&
         readOnly == that.readOnly && interfaceType == that.interfaceType;
}

HybridStructType::HybridStructType(
    llvm::ArrayRef<HybridStructType::FieldInfo> fieldsVec, llvm::StringRef name,
    bool isReadOnly, StructInterfaceType iface)
    : HybridType(TK_HybridStruct, name),
      fields(fieldsVec.begin(), fieldsVec.end()), readOnly(isReadOnly),
      interfaceType(iface) {}

bool HybridStructType::FieldInfo::
operator==(const HybridStructType::FieldInfo &that) const {
  return astType == that.astType && vkOffsetAttr == that.vkOffsetAttr &&
         packOffsetAttr == that.packOffsetAttr;
}

bool HybridStructType::operator==(const HybridStructType &that) const {
  return fields == that.fields && getName() == that.getName() &&
         readOnly == that.readOnly && interfaceType == that.interfaceType;
}

} // namespace spirv
} // namespace clang
