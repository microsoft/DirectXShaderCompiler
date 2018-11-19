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
#include "clang/AST/Attr.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"

namespace clang {
namespace spirv {

enum class StructInterfaceType : uint32_t {
  InternalStorage = 0,
  StorageBuffer = 1,
  UniformBuffer = 2,
};

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
    // Order matters: all the following are hybrid types
    TK_HybridStruct,
    TK_HybridPointer,
    TK_HybridSampledImage,
    TK_HybridFunction,
  };

  virtual ~SpirvType() = default;

  Kind getKind() const { return kind; }

  static bool isTexture(const SpirvType *);
  static bool isRWTexture(const SpirvType *);
  static bool isSampler(const SpirvType *);
  static bool isBuffer(const SpirvType *);
  static bool isRWBuffer(const SpirvType *);
  static bool isSubpassInput(const SpirvType *);
  static bool isSubpassInputMS(const SpirvType *);
  static bool isResourceType(const SpirvType *);
  static bool isOrContains16BitType(const SpirvType *);

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

  uint32_t getBitwidth() const { return bitwidth; }

protected:
  NumericalType(Kind k, uint32_t width) : ScalarType(k), bitwidth(width) {}

  uint32_t bitwidth;
};

class BoolType : public ScalarType {
public:
  BoolType() : ScalarType(TK_Bool) {}

  static bool classof(const SpirvType *t) { return t->getKind() == TK_Bool; }
};

class IntegerType : public NumericalType {
public:
  IntegerType(uint32_t numBits, bool sign)
      : NumericalType(TK_Integer, numBits), isSigned(sign) {}

  static bool classof(const SpirvType *t) { return t->getKind() == TK_Integer; }

  bool isSignedInt() const { return isSigned; }

private:
  bool isSigned;
};

class FloatType : public NumericalType {
public:
  FloatType(uint32_t numBits) : NumericalType(TK_Float, numBits) {}

  static bool classof(const SpirvType *t) { return t->getKind() == TK_Float; }
};

class VectorType : public SpirvType {
public:
  VectorType(const ScalarType *elemType, uint32_t elemCount)
      : SpirvType(TK_Vector), elementType(elemType), elementCount(elemCount) {}

  static bool classof(const SpirvType *t) { return t->getKind() == TK_Vector; }

  const SpirvType *getElementType() const { return elementType; }
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

  const SpirvType *getVecType() const { return vectorType; }
  const SpirvType *getElementType() const {
    return vectorType->getElementType();
  }
  uint32_t getVecCount() const { return vectorCount; }
  bool isRowMajorMat() const { return isRowMajor; }

  uint32_t numCols() const { return vectorCount; }
  uint32_t numRows() const { return vectorType->getElementCount(); }

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

  const SpirvType *getSampledType() const { return sampledType; }
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

  const SpirvType *getImageType() const { return imageType; }

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
      : SpirvType(TK_RuntimeArray), elementType(elemType) {}

  static bool classof(const SpirvType *t) {
    return t->getKind() == TK_RuntimeArray;
  }

  const SpirvType *getElementType() const { return elementType; }

private:
  const SpirvType *elementType;
};

class StructType : public SpirvType {
public:
  struct FieldInfo {
  public:
    FieldInfo(const SpirvType *type_, llvm::StringRef name_ = "",
              clang::VKOffsetAttr *offset = nullptr,
              hlsl::ConstantPacking *packOffset = nullptr)
        : type(type_), name(name_), vkOffsetAttr(offset),
          packOffsetAttr(packOffset) {}

    bool operator==(const FieldInfo &that) const;

    // The field's type.
    const SpirvType *type;
    // The field's name.
    std::string name;
    // vk::offset attributes associated with this field.
    clang::VKOffsetAttr *vkOffsetAttr;
    // :packoffset() annotations associated with this field.
    hlsl::ConstantPacking *packOffsetAttr;
  };

  StructType(
      llvm::ArrayRef<FieldInfo> fields, llvm::StringRef name, bool isReadOnly,
      StructInterfaceType interfaceType = StructInterfaceType::InternalStorage);

  static bool classof(const SpirvType *t) { return t->getKind() == TK_Struct; }

  llvm::ArrayRef<FieldInfo> getFields() const { return fields; }
  bool isReadOnly() const { return readOnly; }
  std::string getStructName() const { return structName; }
  StructInterfaceType getInterfaceType() const { return interfaceType; }

  bool operator==(const StructType &that) const;

private:
  // Reflection is heavily used in graphics pipelines. Reflection relies on
  // struct names and field names. That basically means we cannot ignore these
  // names when considering unification. Otherwise, reflection will be confused.

  llvm::SmallVector<FieldInfo, 8> fields;
  std::string structName;
  bool readOnly;
  // Indicates the interface type of this structure. If this structure is a
  // storage buffer shader-interface, it will be decorated with 'BufferBlock'.
  // If this structure is a uniform buffer shader-interface, it will be
  // decorated with 'Block'.
  StructInterfaceType interfaceType;
};

class SpirvPointerType : public SpirvType {
public:
  SpirvPointerType(const SpirvType *pointee, spv::StorageClass sc)
      : SpirvType(TK_Pointer), pointeeType(pointee), storageClass(sc) {}

  static bool classof(const SpirvType *t) { return t->getKind() == TK_Pointer; }

  const SpirvType *getPointeeType() const { return pointeeType; }
  spv::StorageClass getStorageClass() const { return storageClass; }

  bool operator==(const SpirvPointerType &that) const {
    return pointeeType == that.pointeeType && storageClass == that.storageClass;
  }

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

  // void setReturnType(const SpirvType *t) { returnType = t; }
  const SpirvType *getReturnType() const { return returnType; }
  llvm::ArrayRef<const SpirvType *> getParamTypes() const { return paramTypes; }

private:
  const SpirvType *returnType;
  llvm::SmallVector<const SpirvType *, 8> paramTypes;
};

class HybridType : public SpirvType {
public:
  static bool classof(const SpirvType *t) {
    return t->getKind() >= TK_HybridStruct && t->getKind() <= TK_HybridFunction;
  }

protected:
  HybridType(Kind k) : SpirvType(k) {}
};

/// **NOTE**: This type is created in order to facilitate transition of old
/// infrastructure to the new infrastructure. Using this type should be avoided
/// as much as possible.
///
/// This type uses a mix of SpirvType and QualType for the structure fields.
class HybridStructType : public HybridType {
public:
  struct FieldInfo {
  public:
    FieldInfo(QualType astType_, llvm::StringRef name_ = "",
              clang::VKOffsetAttr *offset = nullptr,
              hlsl::ConstantPacking *packOffset = nullptr)
        : astType(astType_), name(name_), vkOffsetAttr(offset),
          packOffsetAttr(packOffset) {}

    bool operator==(const FieldInfo &that) const;

    // The field's type.
    QualType astType;
    // The field's name.
    std::string name;
    // vk::offset attributes associated with this field.
    clang::VKOffsetAttr *vkOffsetAttr;
    // :packoffset() annotations associated with this field.
    hlsl::ConstantPacking *packOffsetAttr;
  };

  HybridStructType(
      llvm::ArrayRef<FieldInfo> fields, llvm::StringRef name, bool isReadOnly,
      StructInterfaceType interfaceType = StructInterfaceType::InternalStorage);

  static bool classof(const SpirvType *t) {
    return t->getKind() == TK_HybridStruct;
  }

  llvm::ArrayRef<FieldInfo> getFields() const { return fields; }
  bool isReadOnly() const { return readOnly; }
  std::string getStructName() const { return structName; }
  StructInterfaceType getInterfaceType() const { return interfaceType; }

  bool operator==(const HybridStructType &that) const;

private:
  // Reflection is heavily used in graphics pipelines. Reflection relies on
  // struct names and field names. That basically means we cannot ignore these
  // names when considering unification. Otherwise, reflection will be confused.

  llvm::SmallVector<FieldInfo, 8> fields;
  std::string structName;
  bool readOnly;
  // Indicates the interface type of this structure. If this structure is a
  // storage buffer shader-interface, it will be decorated with 'BufferBlock'.
  // If this structure is a uniform buffer shader-interface, it will be
  // decorated with 'Block'.
  StructInterfaceType interfaceType;
};

class HybridPointerType : public HybridType {
public:
  HybridPointerType(QualType pointee, spv::StorageClass sc)
      : HybridType(TK_HybridPointer), pointeeType(pointee), storageClass(sc) {}

  static bool classof(const SpirvType *t) {
    return t->getKind() == TK_HybridPointer;
  }

  QualType getPointeeType() const { return pointeeType; }
  spv::StorageClass getStorageClass() const { return storageClass; }

  bool operator==(const HybridPointerType &that) const {
    return pointeeType == that.pointeeType && storageClass == that.storageClass;
  }

private:
  QualType pointeeType;
  spv::StorageClass storageClass;
};

class HybridSampledImageType : public HybridType {
public:
  HybridSampledImageType(QualType image)
      : HybridType(TK_HybridSampledImage), imageType(image) {}

  static bool classof(const SpirvType *t) {
    return t->getKind() == TK_HybridSampledImage;
  }

  QualType getImageType() const { return imageType; }

private:
  QualType imageType;
};

// This class can be extended to also accept QualType vector as param types.
class HybridFunctionType : public HybridType {
public:
  HybridFunctionType(QualType ret, llvm::ArrayRef<const SpirvType *> param)
      : HybridType(TK_HybridFunction), astReturnType(ret),
        paramTypes(param.begin(), param.end()) {}

  static bool classof(const SpirvType *t) {
    return t->getKind() == TK_HybridFunction;
  }

  bool operator==(const HybridFunctionType &that) const {
    return astReturnType == that.astReturnType &&
           returnType == that.returnType && paramTypes == that.paramTypes;
  }

  QualType getAstReturnType() const { return astReturnType; }
  void setReturnType(const SpirvType *t) { returnType = t; }
  const SpirvType *getReturnType() const { return returnType; }
  llvm::ArrayRef<const SpirvType *> getParamTypes() const { return paramTypes; }

private:
  QualType astReturnType;
  const SpirvType *returnType;
  llvm::SmallVector<const SpirvType *, 8> paramTypes;
};

} // end namespace spirv
} // end namespace clang

#endif // LLVM_CLANG_SPIRV_SPIRVTYPE_H
