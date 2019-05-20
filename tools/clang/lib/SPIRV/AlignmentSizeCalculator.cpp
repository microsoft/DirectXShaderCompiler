//===--- AlignmentSizeCalculator.cpp -- Alignemnt And Size Calc --*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "AlignmentSizeCalculator.h"
#include "clang/AST/Attr.h"
#include "clang/SPIRV/AstTypeProbe.h"

namespace {

/// The alignment for 4-component float vectors.
constexpr uint32_t kStd140Vec4Alignment = 16u;

/// Rounds the given value up to the given power of 2.
inline uint32_t roundToPow2(uint32_t val, uint32_t pow2) {
  assert(pow2 != 0);
  return (val + pow2 - 1) & ~(pow2 - 1);
}

/// Returns true if the given vector type (of the given size) crosses the
/// 4-component vector boundary if placed at the given offset.
bool improperStraddle(clang::QualType type, int size, int offset) {
  assert(clang::spirv::isVectorType(type));
  return size <= 16 ? offset / 16 != (offset + size - 1) / 16
                    : offset % 16 != 0;
}

} // end anonymous namespace

namespace clang {
namespace spirv {

void AlignmentSizeCalculator::alignUsingHLSLRelaxedLayout(
    QualType fieldType, uint32_t fieldSize, uint32_t fieldAlignment,
    uint32_t *currentOffset) {
  QualType vecElemType = {};
  const bool fieldIsVecType = isVectorType(fieldType, &vecElemType);

  // Adjust according to HLSL relaxed layout rules.
  // Aligning vectors as their element types so that we can pack a float
  // and a float3 tightly together.
  if (fieldIsVecType) {
    uint32_t scalarAlignment = 0;
    std::tie(scalarAlignment, std::ignore) = getAlignmentAndSize(
        vecElemType, SpirvLayoutRule::Void, /*isRowMajor*/ llvm::None, nullptr);
    if (scalarAlignment <= 4)
      fieldAlignment = scalarAlignment;
  }

  *currentOffset = roundToPow2(*currentOffset, fieldAlignment);

  // Adjust according to HLSL relaxed layout rules.
  // Bump to 4-component vector alignment if there is a bad straddle
  if (fieldIsVecType &&
      improperStraddle(fieldType, fieldSize, *currentOffset)) {
    fieldAlignment = kStd140Vec4Alignment;
    *currentOffset = roundToPow2(*currentOffset, fieldAlignment);
  }
}

std::pair<uint32_t, uint32_t> AlignmentSizeCalculator::getAlignmentAndSize(
    QualType type, SpirvLayoutRule rule, llvm::Optional<bool> isRowMajor,
    uint32_t *stride) {
  // std140 layout rules:

  // 1. If the member is a scalar consuming N basic machine units, the base
  //    alignment is N.
  //
  // 2. If the member is a two- or four-component vector with components
  //    consuming N basic machine units, the base alignment is 2N or 4N,
  //    respectively.
  //
  // 3. If the member is a three-component vector with components consuming N
  //    basic machine units, the base alignment is 4N.
  //
  // 4. If the member is an array of scalars or vectors, the base alignment and
  //    array stride are set to match the base alignment of a single array
  //    element, according to rules (1), (2), and (3), and rounded up to the
  //    base alignment of a vec4. The array may have padding at the end; the
  //    base offset of the member following the array is rounded up to the next
  //    multiple of the base alignment.
  //
  // 5. If the member is a column-major matrix with C columns and R rows, the
  //    matrix is stored identically to an array of C column vectors with R
  //    components each, according to rule (4).
  //
  // 6. If the member is an array of S column-major matrices with C columns and
  //    R rows, the matrix is stored identically to a row of S X C column
  //    vectors with R components each, according to rule (4).
  //
  // 7. If the member is a row-major matrix with C columns and R rows, the
  //    matrix is stored identically to an array of R row vectors with C
  //    components each, according to rule (4).
  //
  // 8. If the member is an array of S row-major matrices with C columns and R
  //    rows, the matrix is stored identically to a row of S X R row vectors
  //    with C components each, according to rule (4).
  //
  // 9. If the member is a structure, the base alignment of the structure is N,
  //    where N is the largest base alignment value of any of its members, and
  //    rounded up to the base alignment of a vec4. The individual members of
  //    this substructure are then assigned offsets by applying this set of
  //    rules recursively, where the base offset of the first member of the
  //    sub-structure is equal to the aligned offset of the structure. The
  //    structure may have padding at the end; the base offset of the member
  //    following the sub-structure is rounded up to the next multiple of the
  //    base alignment of the structure.
  //
  // 10. If the member is an array of S structures, the S elements of the array
  //     are laid out in order, according to rule (9).
  //
  // This method supports multiple layout rules, all of them modifying the
  // std140 rules listed above:
  //
  // std430:
  // - Array base alignment and stride does not need to be rounded up to a
  //   multiple of 16.
  // - Struct base alignment does not need to be rounded up to a multiple of 16.
  //
  // Relaxed std140/std430:
  // - Vector base alignment is set as its element type's base alignment.
  //
  // FxcCTBuffer:
  // - Vector base alignment is set as its element type's base alignment.
  // - Arrays/structs do not need to have padding at the end; arrays/structs do
  //   not affect the base offset of the member following them.
  //
  // FxcSBuffer:
  // - Vector/matrix/array base alignment is set as its element type's base
  //   alignment.
  // - Arrays/structs do not need to have padding at the end; arrays/structs do
  //   not affect the base offset of the member following them.
  // - Struct base alignment does not need to be rounded up to a multiple of 16.

  const auto desugaredType = desugarType(type, &isRowMajor);
  if (desugaredType != type) {
    auto result = getAlignmentAndSize(desugaredType, rule, isRowMajor, stride);
    return result;
  }

  { // Rule 1
    QualType ty = {};
    if (isScalarType(type, &ty))
      if (const auto *builtinType = ty->getAs<BuiltinType>())
        switch (builtinType->getKind()) {
        case BuiltinType::Bool:
        case BuiltinType::Int:
        case BuiltinType::UInt:
        case BuiltinType::Float:
          return {4, 4};
        case BuiltinType::Double:
        case BuiltinType::LongLong:
        case BuiltinType::ULongLong:
          return {8, 8};
        case BuiltinType::Min12Int:
        case BuiltinType::Min16Int:
        case BuiltinType::Min16UInt:
        case BuiltinType::Min16Float:
        case BuiltinType::Min10Float: {
          if (spvOptions.enable16BitTypes)
            return {2, 2};
          else
            return {4, 4};
        }
        // the 'Half' enum always represents 16-bit floats.
        // int16_t and uint16_t map to Short and UShort.
        case BuiltinType::Short:
        case BuiltinType::UShort:
        case BuiltinType::Half:
          return {2, 2};
        // 'HalfFloat' always represents 32-bit floats.
        case BuiltinType::HalfFloat:
          return {4, 4};
        default:
          emitError("alignment and size calculation for type %0 unimplemented")
              << type;
          return {0, 0};
        }
  }

  { // Rule 2 and 3
    QualType elemType = {};
    uint32_t elemCount = {};
    if (isVectorType(type, &elemType, &elemCount)) {
      uint32_t alignment = 0, size = 0;
      std::tie(alignment, size) =
          getAlignmentAndSize(elemType, rule, isRowMajor, stride);
      // Use element alignment for fxc rules and VK_EXT_scalar_block_layout
      if (rule != SpirvLayoutRule::FxcCTBuffer &&
          rule != SpirvLayoutRule::FxcSBuffer &&
          rule != SpirvLayoutRule::Scalar)
        alignment = (elemCount == 3 ? 4 : elemCount) * size;

      return {alignment, elemCount * size};
    }
  }

  { // Rule 5 and 7
    QualType elemType = {};
    uint32_t rowCount = 0, colCount = 0;
    if (isMxNMatrix(type, &elemType, &rowCount, &colCount)) {
      uint32_t alignment = 0, size = 0;
      std::tie(alignment, size) =
          getAlignmentAndSize(elemType, rule, isRowMajor, stride);

      // Matrices are treated as arrays of vectors:
      // The base alignment and array stride are set to match the base alignment
      // of a single array element, according to rules 1, 2, and 3, and rounded
      // up to the base alignment of a vec4.
      bool rowMajor = isRowMajor.hasValue()
                          ? isRowMajor.getValue()
                          : isRowMajorMatrix(spvOptions, type);

      const uint32_t vecStorageSize = rowMajor ? rowCount : colCount;

      if (rule == SpirvLayoutRule::FxcSBuffer ||
          rule == SpirvLayoutRule::Scalar) {
        *stride = vecStorageSize * size;
        // Use element alignment for fxc structured buffers and
        // VK_EXT_scalar_block_layout
        return {alignment, rowCount * colCount * size};
      }

      alignment *= (vecStorageSize == 3 ? 4 : vecStorageSize);
      if (rule == SpirvLayoutRule::GLSLStd140 ||
          rule == SpirvLayoutRule::RelaxedGLSLStd140 ||
          rule == SpirvLayoutRule::FxcCTBuffer) {
        alignment = roundToPow2(alignment, kStd140Vec4Alignment);
      }
      *stride = alignment;
      size = (rowMajor ? colCount : rowCount) * alignment;

      return {alignment, size};
    }
  }

  // Rule 9
  if (const auto *structType = type->getAs<RecordType>()) {
    // Special case for handling empty structs, whose size is 0 and has no
    // requirement over alignment (thus 1).
    if (structType->getDecl()->field_empty())
      return {1, 0};

    uint32_t maxAlignment = 0;
    uint32_t structSize = 0;

    for (const auto *field : structType->getDecl()->fields()) {
      uint32_t memberAlignment = 0, memberSize = 0;
      std::tie(memberAlignment, memberSize) =
          getAlignmentAndSize(field->getType(), rule, isRowMajor, stride);

      if (rule == SpirvLayoutRule::RelaxedGLSLStd140 ||
          rule == SpirvLayoutRule::RelaxedGLSLStd430 ||
          rule == SpirvLayoutRule::FxcCTBuffer) {
        alignUsingHLSLRelaxedLayout(field->getType(), memberSize,
                                    memberAlignment, &structSize);
      } else {
        structSize = roundToPow2(structSize, memberAlignment);
      }

      // Reset the current offset to the one specified in the source code
      // if exists. It's debatable whether we should do sanity check here.
      // If the developers want manually control the layout, we leave
      // everything to them.
      if (const auto *offsetAttr = field->getAttr<VKOffsetAttr>()) {
        structSize = offsetAttr->getOffset();
      }

      // The base alignment of the structure is N, where N is the largest
      // base alignment value of any of its members...
      maxAlignment = std::max(maxAlignment, memberAlignment);
      structSize += memberSize;
    }

    if (rule == SpirvLayoutRule::Scalar) {
      // A structure has a scalar alignment equal to the largest scalar
      // alignment of any of its members in VK_EXT_scalar_block_layout.
      return {maxAlignment, structSize};
    }

    if (rule == SpirvLayoutRule::GLSLStd140 ||
        rule == SpirvLayoutRule::RelaxedGLSLStd140 ||
        rule == SpirvLayoutRule::FxcCTBuffer) {
      // ... and rounded up to the base alignment of a vec4.
      maxAlignment = roundToPow2(maxAlignment, kStd140Vec4Alignment);
    }

    if (rule != SpirvLayoutRule::FxcCTBuffer &&
        rule != SpirvLayoutRule::FxcSBuffer) {
      // The base offset of the member following the sub-structure is rounded up
      // to the next multiple of the base alignment of the structure.
      structSize = roundToPow2(structSize, maxAlignment);
    }
    return {maxAlignment, structSize};
  }

  // Rule 4, 6, 8, and 10
  if (const auto *arrayType = astContext.getAsConstantArrayType(type)) {
    const auto elemCount = arrayType->getSize().getZExtValue();
    uint32_t alignment = 0, size = 0;
    std::tie(alignment, size) = getAlignmentAndSize(arrayType->getElementType(),
                                                    rule, isRowMajor, stride);

    if (rule == SpirvLayoutRule::FxcSBuffer ||
        rule == SpirvLayoutRule::Scalar) {
      *stride = size;
      // Use element alignment for fxc structured buffers and
      // VK_EXT_scalar_block_layout
      return {alignment, size * elemCount};
    }

    if (rule == SpirvLayoutRule::GLSLStd140 ||
        rule == SpirvLayoutRule::RelaxedGLSLStd140 ||
        rule == SpirvLayoutRule::FxcCTBuffer) {
      // The base alignment and array stride are set to match the base alignment
      // of a single array element, according to rules 1, 2, and 3, and rounded
      // up to the base alignment of a vec4.
      alignment = roundToPow2(alignment, kStd140Vec4Alignment);
    }
    if (rule == SpirvLayoutRule::FxcCTBuffer) {
      // In fxc cbuffer/tbuffer packing rules, arrays does not affect the data
      // packing after it. But we still need to make sure paddings are inserted
      // internally if necessary.
      *stride = roundToPow2(size, alignment);
      size += *stride * (elemCount - 1);
    } else {
      // Need to round size up considering stride for scalar types
      size = roundToPow2(size, alignment);
      *stride = size; // Use size instead of alignment here for Rule 10
      size *= elemCount;
      // The base offset of the member following the array is rounded up to the
      // next multiple of the base alignment.
      size = roundToPow2(size, alignment);
    }

    return {alignment, size};
  }

  emitError("alignment and size calculation for type %0 unimplemented") << type;
  return {0, 0};
}

} // namespace spirv
} // namespace clang
