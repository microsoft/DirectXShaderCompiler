//===-- SPIRVContext.h - Context holding SPIR-V codegen data ----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_CLANG_SPIRV_SPIRVCONTEXT_H
#define LLVM_CLANG_SPIRV_SPIRVCONTEXT_H

#include <array>
#include <unordered_map>

#include "clang/Frontend/FrontendAction.h"
#include "clang/SPIRV/SpirvInstruction.h"
#include "clang/SPIRV/SpirvType.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseMapInfo.h"
#include "llvm/Support/Allocator.h"

namespace clang {
namespace spirv {

// Provides DenseMapInfo for spv::StorageClass so that we can use
// spv::StorageClass as key to DenseMap.
//
// Mostly from DenseMapInfo<unsigned> in DenseMapInfo.h.
struct StorageClassDenseMapInfo {
  static inline spv::StorageClass getEmptyKey() {
    return spv::StorageClass::Max;
  }
  static inline spv::StorageClass getTombstoneKey() {
    return spv::StorageClass::Max;
  }
  static unsigned getHashValue(const spv::StorageClass &Val) {
    return static_cast<unsigned>(Val) * 37U;
  }
  static bool isEqual(const spv::StorageClass &LHS,
                      const spv::StorageClass &RHS) {
    return LHS == RHS;
  }
};

// Provides DenseMapInfo for QualType so that we can use it key to DenseMap.
//
// Mostly from DenseMapInfo<unsigned> in DenseMapInfo.h.
struct QualTypeDenseMapInfo {
  static inline QualType getEmptyKey() { return {}; }
  static inline QualType getTombstoneKey() { return {}; }
  static unsigned getHashValue(const QualType &Val) {
    return llvm::hash_combine(Val.getTypePtr(), Val.getCVRQualifiers());
  }
  static bool isEqual(const QualType &LHS, const QualType &RHS) {
    return LHS == RHS;
  }
};

// Provides DenseMapInfo for ArrayType so we can create a DenseSet of array
// types.
struct ArrayTypeMapInfo {
  static inline ArrayType *getEmptyKey() { return nullptr; }
  static inline ArrayType *getTombstoneKey() { return nullptr; }
  static unsigned getHashValue(const ArrayType *Val) {
    return llvm::hash_combine(Val->getElementType(), Val->getElementCount(),
                              Val->getStride().hasValue());
  }
  static bool isEqual(const ArrayType *LHS, const ArrayType *RHS) {
    // Either both are null, or both should have the same underlying type.
    return (LHS == RHS) || (LHS && RHS && *LHS == *RHS);
  }
};

// Provides DenseMapInfo for RuntimeArrayType so we can create a DenseSet of
// runtime array types.
struct RuntimeArrayTypeMapInfo {
  static inline RuntimeArrayType *getEmptyKey() { return nullptr; }
  static inline RuntimeArrayType *getTombstoneKey() { return nullptr; }
  static unsigned getHashValue(const RuntimeArrayType *Val) {
    return llvm::hash_combine(Val->getElementType(),
                              Val->getStride().hasValue());
  }
  static bool isEqual(const RuntimeArrayType *LHS,
                      const RuntimeArrayType *RHS) {
    // Either both are null, or both should have the same underlying type.
    return (LHS == RHS) || (LHS && RHS && *LHS == *RHS);
  }
};

// Provides DenseMapInfo for ImageType so we can create a DenseSet of
// image types.
struct ImageTypeMapInfo {
  static inline ImageType *getEmptyKey() { return nullptr; }
  static inline ImageType *getTombstoneKey() { return nullptr; }
  static unsigned getHashValue(const ImageType *Val) {
    return llvm::hash_combine(Val->getSampledType(), Val->isArrayedImage(),
                              Val->isMSImage(),
                              static_cast<uint32_t>(Val->getDimension()),
                              static_cast<uint32_t>(Val->withSampler()),
                              static_cast<uint32_t>(Val->getImageFormat()));
  }
  static bool isEqual(const ImageType *LHS, const ImageType *RHS) {
    // Either both are null, or both should have the same underlying type.
    return (LHS == RHS) || (LHS && RHS && *LHS == *RHS);
  }
};

// Provides DenseMapInfo for FunctionType so we can create a DenseSet of
// function types.
struct FunctionTypeMapInfo {
  static inline FunctionType *getEmptyKey() { return nullptr; }
  static inline FunctionType *getTombstoneKey() { return nullptr; }
  static unsigned getHashValue(const FunctionType *Val) {
    // Hashing based on return type and number of function parameters.
    return llvm::hash_combine(Val->getReturnType(),
                              Val->getParamTypes().size());
  }
  static bool isEqual(const FunctionType *LHS, const FunctionType *RHS) {
    // Either both are null, or both should have the same underlying type.
    return (LHS == RHS) || (LHS && RHS && *LHS == *RHS);
  }
};

/// The class owning various SPIR-V entities allocated in memory during CodeGen.
///
/// All entities should be allocated from an object of this class using
/// placement new. This way other components of the CodeGen do not need to worry
/// about lifetime of those SPIR-V entities. They will be deleted when such a
/// context is deleted. Therefore, this context should outlive the usages of the
/// the SPIR-V entities allocated in memory.
class SpirvContext {
  friend class SpirvBuilder;
  friend class EmitTypeHandler;

public:
  SpirvContext();
  ~SpirvContext() = default;

  // Forbid copy construction and assignment
  SpirvContext(const SpirvContext &) = delete;
  SpirvContext &operator=(const SpirvContext &) = delete;

  // Forbid move construction and assignment
  SpirvContext(SpirvContext &&) = delete;
  SpirvContext &operator=(SpirvContext &&) = delete;

  /// Allocates memory of the given size and alignment.
  void *allocate(size_t size, unsigned align) const {
    return allocator.Allocate(size, align);
  }

  /// Deallocates the memory pointed by the given pointer.
  void deallocate(void *ptr) const {}

  // === Types ===

  const VoidType *getVoidType() const { return voidType; }

  const BoolType *getBoolType() const { return boolType; }
  const IntegerType *getSIntType(uint32_t bitwidth);
  const IntegerType *getUIntType(uint32_t bitwidth);
  const FloatType *getFloatType(uint32_t bitwidth);

  const VectorType *getVectorType(const SpirvType *elemType, uint32_t count);
  // Note: In the case of non-floating-point matrices, this method returns an
  // array of vectors.
  const SpirvType *getMatrixType(const SpirvType *vecType, uint32_t vecCount);

  const ImageType *getImageType(const SpirvType *, spv::Dim,
                                ImageType::WithDepth, bool arrayed, bool ms,
                                ImageType::WithSampler sampled,
                                spv::ImageFormat);
  const SamplerType *getSamplerType() const { return samplerType; }
  const SampledImageType *getSampledImageType(const ImageType *image);
  const HybridSampledImageType *getSampledImageType(QualType image);

  const ArrayType *getArrayType(const SpirvType *elemType, uint32_t elemCount,
                                llvm::Optional<uint32_t> arrayStride);
  const RuntimeArrayType *
  getRuntimeArrayType(const SpirvType *elemType,
                      llvm::Optional<uint32_t> arrayStride);

  const StructType *getStructType(
      llvm::ArrayRef<StructType::FieldInfo> fields, llvm::StringRef name,
      bool isReadOnly = false,
      StructInterfaceType interfaceType = StructInterfaceType::InternalStorage);

  const HybridStructType *getHybridStructType(
      llvm::ArrayRef<HybridStructType::FieldInfo> fields, llvm::StringRef name,
      bool isReadOnly = false,
      StructInterfaceType interfaceType = StructInterfaceType::InternalStorage);

  const SpirvPointerType *getPointerType(const SpirvType *pointee,
                                         spv::StorageClass);
  const HybridPointerType *getPointerType(QualType pointee, spv::StorageClass);

  FunctionType *getFunctionType(const SpirvType *ret,
                                llvm::ArrayRef<const SpirvType *> param);
  HybridFunctionType *getFunctionType(QualType ret,
                                      llvm::ArrayRef<const SpirvType *> param);

  const StructType *getByteAddressBufferType(bool isWritable);
  const StructType *getACSBufferCounterType();

private:
  /// \brief The allocator used to create SPIR-V entity objects.
  ///
  /// SPIR-V entity objects are never destructed; rather, all memory associated
  /// with the SPIR-V entity objects will be released when the SpirvContext
  /// itself is destroyed.
  ///
  /// This field must appear the first since it will be used to allocate object
  /// for the other fields.
  mutable llvm::BumpPtrAllocator allocator;

  // Unique types

  const VoidType *voidType;
  const BoolType *boolType;

  // The type at index i is for bitwidth 2^i. So max bitwidth supported
  // is 2^6 = 64. Index 0/1/2/3 is not used right now.
  std::array<const IntegerType *, 7> sintTypes;
  std::array<const IntegerType *, 7> uintTypes;
  std::array<const FloatType *, 7> floatTypes;

  using VectorTypeArray = std::array<const VectorType *, 5>;
  using MatrixTypeVector = std::vector<const MatrixType *>;
  using SCToPtrTyMap =
      llvm::DenseMap<spv::StorageClass, const SpirvPointerType *,
                     StorageClassDenseMapInfo>;

  // Vector/matrix types for each possible element count.
  // Type at index is for vector of index components. Index 0/1 is unused.

  llvm::DenseMap<const ScalarType *, VectorTypeArray> vecTypes;
  llvm::DenseMap<const VectorType *, MatrixTypeVector> matTypes;
  llvm::DenseSet<const ImageType *, ImageTypeMapInfo> imageTypes;
  const SamplerType *samplerType;
  llvm::DenseMap<const ImageType *, const SampledImageType *> sampledImageTypes;
  llvm::DenseSet<const ArrayType *, ArrayTypeMapInfo> arrayTypes;
  llvm::DenseSet<const RuntimeArrayType *, RuntimeArrayTypeMapInfo>
      runtimeArrayTypes;
  llvm::SmallVector<const StructType *, 8> structTypes;
  llvm::DenseMap<const SpirvType *, SCToPtrTyMap> pointerTypes;
  llvm::DenseSet<FunctionType *, FunctionTypeMapInfo> functionTypes;
};

} // end namespace spirv
} // end namespace clang

// operator new and delete aren't allowed inside namespaces.

/// Placement new for using the SpirvContext's allocator.
inline void *operator new(size_t bytes, const clang::spirv::SpirvContext &c,
                          size_t align = 8) {
  return c.allocate(bytes, align);
}

inline void *operator new(size_t bytes, const clang::spirv::SpirvContext *c,
                          size_t align = 8) {
  return c->allocate(bytes, align);
}

/// Placement delete companion to the new above.
inline void operator delete(void *ptr, const clang::spirv::SpirvContext &c,
                            size_t) {
  c.deallocate(ptr);
}

inline void operator delete(void *ptr, const clang::spirv::SpirvContext *c,
                            size_t) {
  c->deallocate(ptr);
}

#endif
