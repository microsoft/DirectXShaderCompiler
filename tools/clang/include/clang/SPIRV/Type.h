//===-- Type.h - SPIR-V Type ------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_CLANG_SPIRV_TYPE_H
#define LLVM_CLANG_SPIRV_TYPE_H

#include <set>
#include <unordered_set>
#include <vector>

#include "spirv/1.0/spirv.hpp11"
#include "clang/SPIRV/Decoration.h"
#include "llvm/ADT/Optional.h"

namespace clang {
namespace spirv {

class SPIRVContext;

/// \brief SPIR-V Type
///
/// This class defines a unique SPIR-V Type.
/// A SPIR-V Type includes its <opcode> defined by the SPIR-V Spec.
/// It also incldues any arguments (32-bit words) needed to define the
/// type. It also includes a set of decorations that are applied to that type.
///
/// The class includes static getXXX(...) functions for getting pointers of any
/// needed type. A unique type has a unique pointer (e.g. calling
/// 'getBoolean' function will always return the same pointer for the given
/// context).
class Type {
public:
  using DecorationSet = std::set<const Decoration *>;

  spv::Op getOpcode() const { return opcode; }
  const std::vector<uint32_t> &getArgs() const { return args; }
  const std::set<const Decoration *> &getDecorations() const {
    return decorations;
  }
  bool hasDecoration(const Decoration *) const;

  bool isBooleanType() const;
  bool isIntegerType() const;
  bool isFloatType() const;
  bool isNumericalType() const;
  bool isScalarType() const;
  bool isVectorType() const;
  bool isMatrixType() const;
  bool isArrayType() const;
  bool isStructureType() const;
  bool isAggregateType() const;
  bool isCompositeType() const;
  bool isImageType() const;

  // Scalar types do not take any decorations.
  static const Type *getVoid(SPIRVContext &ctx);
  static const Type *getBool(SPIRVContext &ctx);
  static const Type *getInt8(SPIRVContext &ctx);
  static const Type *getUint8(SPIRVContext &ctx);
  static const Type *getInt16(SPIRVContext &ctx);
  static const Type *getUint16(SPIRVContext &ctx);
  static const Type *getInt32(SPIRVContext &ctx);
  static const Type *getUint32(SPIRVContext &ctx);
  static const Type *getInt64(SPIRVContext &ctx);
  static const Type *getUint64(SPIRVContext &ctx);
  static const Type *getFloat16(SPIRVContext &ctx);
  static const Type *getFloat32(SPIRVContext &ctx);
  static const Type *getFloat64(SPIRVContext &ctx);
  static const Type *getVec2(SPIRVContext &ctx, uint32_t component_type);
  static const Type *getVec3(SPIRVContext &ctx, uint32_t component_type);
  static const Type *getVec4(SPIRVContext &ctx, uint32_t component_type);
  static const Type *getMatrix(SPIRVContext &ctx, uint32_t column_type_id,
                               uint32_t column_count);

  static const Type *
  getImage(SPIRVContext &ctx, uint32_t sampled_type, spv::Dim dim,
           uint32_t depth, uint32_t arrayed, uint32_t ms, uint32_t sampled,
           spv::ImageFormat image_format,
           llvm::Optional<spv::AccessQualifier> access_qualifier = llvm::None,
           DecorationSet decs = {});
  static const Type *getSampler(SPIRVContext &ctx, DecorationSet decs = {});
  static const Type *getSampledImage(SPIRVContext &ctx, uint32_t imag_type_id,
                                     DecorationSet decs = {});
  static const Type *getArray(SPIRVContext &ctx, uint32_t component_type_id,
                              uint32_t len_id, DecorationSet decs = {});
  static const Type *getRuntimeArray(SPIRVContext &ctx,
                                     uint32_t component_type_id,
                                     DecorationSet decs = {});
  static const Type *getStruct(SPIRVContext &ctx,
                               std::initializer_list<uint32_t> members,
                               DecorationSet d = {});
  static const Type *getOpaque(SPIRVContext &ctx, std::string name,
                               DecorationSet decs = {});
  static const Type *getPointer(SPIRVContext &ctx,
                                spv::StorageClass storage_class, uint32_t type,
                                DecorationSet decs = {});
  static const Type *getFunction(SPIRVContext &ctx, uint32_t return_type,
                                 std::initializer_list<uint32_t> params,
                                 DecorationSet decs = {});
  static const Type *getEvent(SPIRVContext &ctx, DecorationSet decs = {});
  static const Type *getDeviceEvent(SPIRVContext &ctx, DecorationSet decs = {});
  static const Type *getReserveId(SPIRVContext &ctx, DecorationSet decs = {});
  static const Type *getQueue(SPIRVContext &ctx, DecorationSet decs = {});
  static const Type *getPipe(SPIRVContext &ctx, spv::AccessQualifier qualifier,
                             DecorationSet decs = {});
  static const Type *getForwardPointer(SPIRVContext &ctx, uint32_t pointer_type,
                                       spv::StorageClass storage_class,
                                       DecorationSet decs = {});
  bool operator==(const Type &other) const {
    return opcode == other.opcode && args == other.args &&
           decorations == other.decorations;
  }

private:
  /// \brief Private constructor.
  Type(spv::Op op, std::vector<uint32_t> arg = {},
       std::set<const Decoration *> dec = {});

  /// \brief Returns the unique Type pointer within the given context.
  static const Type *getUniqueType(SPIRVContext &, const Type &);

private:
  spv::Op opcode;             ///< OpCode of the Type defined in SPIR-V Spec
  std::vector<uint32_t> args; ///< Arguments needed to define the type
  std::set<const Decoration *> decorations; ///< decorations applied to the type
};

} // end namespace spirv
} // end namespace clang

#endif
