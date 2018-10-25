//===-- SpirvType.h - SPIR-V Type -----------------------------*- C++ -*---===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_CLANG_SPIRV_SPIRVTYPE_H
#define LLVM_CLANG_SPIRV_SPIRVTYPE_H

#include <string>
#include <utility>
#include <vector>

#include "spirv/unified1/spirv.hpp11"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Casting.h"

namespace clang {
namespace spirv {

class SpirvType {
public:
  enum Kind {
    TK_Void,
    TK_Bool,
    TK_Integer,
    TK_Float,
    TK_Vector,
    TK_Matrix,
    TK_Image,
    TK_Sampler,
    TK_SampledImage,
    TK_Array,
    TK_RuntimeArray,
    TK_Struct,
    TK_Pointer,
    TK_Function,
  };

  virtual ~SpirvType() = default;

  Kind getKind() const { return kind; }

protected:
  SpirvType(Kind k) : kind(k) {}

private:
  const Kind kind;
};

class VoidType : public SpirvType {
public:
  VoidType() : SpirvType(TK_Void) {}

  static bool classof(const SpirvType *t) { return t->getKind() == TK_Void; }
};

class ScalarType : public SpirvType {
public:
  static bool classof(const SpirvType *t);

protected:
  ScalarType(Kind k) : SpirvType(k) {}
};

class NumericalType : public ScalarType {
public:
  static bool classof(const SpirvType *t) {
    return t->getKind() == TK_Integer || t->getKind() == TK_Float;
  }

protected:
  NumericalType(Kind k) : ScalarType(k) {}
};

class BoolType : public ScalarType {
public:
  BoolType() : ScalarType(TK_Bool) {}

  static bool classof(const SpirvType *t) { return t->getKind() == TK_Bool; }
};

class IntegerType : public NumericalType {
public:
  IntegerType(uint32_t numBits, bool sign)
      : NumericalType(TK_Integer), bitwidth(numBits), isSigned(sign) {}

  static bool classof(const SpirvType *t) { return t->getKind() == TK_Integer; }

  uint32_t getBitwidth() const { return bitwidth; }
  bool isSignedInt() const { return isSigned; }

private:
  uint32_t bitwidth;
  bool isSigned;
};

class FloatType : public NumericalType {
public:
  FloatType(uint32_t numBits) : NumericalType(TK_Float), bitwidth(numBits) {}

  static bool classof(const SpirvType *t) { return t->getKind() == TK_Float; }

  uint32_t getBitwidth() const { return bitwidth; }

private:
  uint32_t bitwidth;
};

class VectorType : public SpirvType {
public:
  VectorType(const ScalarType *elemType, uint32_t elemCount)
      : SpirvType(TK_Vector), elementType(elemType), elementCount(elemCount) {}

  static bool classof(const SpirvType *t) { return t->getKind() == TK_Vector; }

  const SpirvType *getElementType() const {
    return llvm::cast<SpirvType>(elementType);
  }
  uint32_t getElementCount() const { return elementCount; }

private:
  const ScalarType *elementType;
  uint32_t elementCount;
};

class MatrixType : public SpirvType {
public:
  MatrixType(const VectorType *vecType, uint32_t vecCount, bool rowMajor);

  static bool classof(const SpirvType *t) { return t->getKind() == TK_Matrix; }

  bool operator==(const MatrixType &that) const;

  const SpirvType *getVecType() const {
    return llvm::cast<SpirvType>(vectorType);
  }
  uint32_t getVecCount() const { return vectorCount; }
  bool isRowMajorMat() const { return isRowMajor; }

private:
  const VectorType *vectorType;
  uint32_t vectorCount;
  // It's debatable whether we should put majorness as a field in the type
  // itself. Majorness only matters at the time of emitting SPIR-V words since
  // we need the layout decoration then. However, if we don't put it here,
  // we will need to rediscover the majorness information from QualType at
  // the time of emitting SPIR-V words.
  bool isRowMajor;
};

class ImageType : public SpirvType {
public:
  enum class WithSampler : uint32_t {
    Unknown = 0,
    Yes = 1,
    No = 2,
  };
  enum class WithDepth : uint32_t {
    No = 0,
    Yes = 1,
    Unknown = 2,
  };

  ImageType(const NumericalType *sampledType, spv::Dim, WithDepth depth,
            bool isArrayed, bool isMultiSampled, WithSampler sampled,
            spv::ImageFormat);

  static bool classof(const SpirvType *t) { return t->getKind() == TK_Image; }

  bool operator==(const ImageType &that) const;

  const SpirvType *getSampledType() const {
    return llvm::cast<SpirvType>(sampledType);
  }
  spv::Dim getDimension() const { return dimension; }
  WithDepth getDepth() const { return imageDepth; }
  bool isArrayedImage() const { return isArrayed; }
  bool isMSImage() const { return isMultiSampled; }
  WithSampler withSampler() const { return isSampled; }
  spv::ImageFormat getImageFormat() const { return imageFormat; }

private:
  const NumericalType *sampledType;
  spv::Dim dimension;
  WithDepth imageDepth;
  bool isArrayed;
  bool isMultiSampled;
  WithSampler isSampled;
  spv::ImageFormat imageFormat;
};

class SamplerType : public SpirvType {
public:
  SamplerType() : SpirvType(TK_Sampler) {}

  static bool classof(const SpirvType *t) { return t->getKind() == TK_Sampler; }
};

class SampledImageType : public SpirvType {
public:
  SampledImageType(const ImageType *image)
      : SpirvType(TK_SampledImage), imageType(image) {}

  static bool classof(const SpirvType *t) {
    return t->getKind() == TK_SampledImage;
  }

  const ImageType *getImageType() const { return imageType; }

private:
  const ImageType *imageType;
};

class ArrayType : public SpirvType {
public:
  ArrayType(const SpirvType *elemType, uint32_t elemCount)
      : SpirvType(TK_Array), elementType(elemType), elementCount(elemCount) {}

  const SpirvType *getElementType() const { return elementType; }
  uint32_t getElementCount() const { return elementCount; }

  static bool classof(const SpirvType *t) { return t->getKind() == TK_Array; }

private:
  const SpirvType *elementType;
  uint32_t elementCount;
};

class RuntimeArrayType : public SpirvType {
public:
  RuntimeArrayType(const SpirvType *elemType)
      : SpirvType(TK_Array), elementType(elemType) {}

  static bool classof(const SpirvType *t) {
    return t->getKind() == TK_RuntimeArray;
  }

  const SpirvType *getElementType() const { return elementType; }

private:
  const SpirvType *elementType;
};

class StructType : public SpirvType {
public:
  StructType(llvm::ArrayRef<const SpirvType *> memberTypes,
             llvm::StringRef name, llvm::ArrayRef<llvm::StringRef> memberNames,
             bool isReadOnly);

  static bool classof(const SpirvType *t) { return t->getKind() == TK_Struct; }

  bool isReadOnly() const { return readOnly; }
  std::string getStructName() const { return structName; }
  llvm::ArrayRef<const SpirvType *> getFieldTypes() const { return fieldTypes; }
  llvm::ArrayRef<std::string> getFieldNames() const { return fieldNames; }

  bool operator==(const StructType &that) const;

private:
  // Reflection is heavily used in graphics pipelines. Reflection relies on
  // struct names and field names. That basically means we cannot ignore these
  // names when considering unification. Otherwise, reflection will be confused.

  std::string structName;
  llvm::SmallVector<const SpirvType *, 8> fieldTypes;
  llvm::SmallVector<std::string, 8> fieldNames;
  bool readOnly;
};

class SpirvPointerType : public SpirvType {
public:
  SpirvPointerType(const SpirvType *pointee, spv::StorageClass sc)
      : SpirvType(TK_Pointer), pointeeType(pointee), storageClass(sc) {}

  static bool classof(const SpirvType *t) { return t->getKind() == TK_Pointer; }

  const SpirvType *getPointeeType() const { return pointeeType; }
  spv::StorageClass getStorageClass() const { return storageClass; }

private:
  const SpirvType *pointeeType;
  spv::StorageClass storageClass;
};

class FunctionType : public SpirvType {
public:
  FunctionType(const SpirvType *ret, llvm::ArrayRef<const SpirvType *> param)
      : SpirvType(TK_Function), returnType(ret),
        paramTypes(param.begin(), param.end()) {}

  static bool classof(const SpirvType *t) {
    return t->getKind() == TK_Function;
  }

  bool operator==(const FunctionType &that) const {
    return returnType == that.returnType && paramTypes == that.paramTypes;
  }

  const SpirvType *getReturnType() const { return returnType; }
  llvm::ArrayRef<const SpirvType *> getParamTypes() const { return paramTypes; }

private:
  const SpirvType *returnType;
  llvm::SmallVector<const SpirvType *, 8> paramTypes;
};

} // end namespace spirv
} // end namespace clang

#endif // LLVM_CLANG_SPIRV_SPIRVTYPE_H
