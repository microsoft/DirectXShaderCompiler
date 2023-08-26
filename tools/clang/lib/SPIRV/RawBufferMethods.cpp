//===---- RawBufferMethods.cpp ---- Raw Buffer Methods ----------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//===----------------------------------------------------------------------===//

#include "RawBufferMethods.h"
#include "AlignmentSizeCalculator.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/CharUnits.h"
#include "clang/AST/RecordLayout.h"
#include "clang/AST/Type.h"
#include "clang/SPIRV/AstTypeProbe.h"
#include "clang/SPIRV/SpirvBuilder.h"
#include "clang/SPIRV/SpirvInstruction.h"
#include <cstdint>

namespace {
/// Rounds the given value up to the given power of 2.
inline uint32_t roundToPow2(uint32_t val, uint32_t pow2) {
  assert(pow2 != 0);
  return (val + pow2 - 1) & ~(pow2 - 1);
}
} // anonymous namespace

namespace clang {
namespace spirv {

SpirvInstruction *RawBufferHandler::bitCastToNumericalOrBool(
    SpirvInstruction *instr, QualType fromType, QualType toType,
    SourceLocation loc, SourceRange range) {
  if (isSameType(astContext, fromType, toType))
    return instr;

  if (toType->isBooleanType() || fromType->isBooleanType())
    return theEmitter.castToType(instr, fromType, toType, loc, range);

  // Perform a bitcast
  return spvBuilder.createUnaryOp(spv::Op::OpBitcast, toType, instr, loc,
                                  range);
}

SpirvInstruction *RawBufferHandler::load16Bits(SpirvInstruction *buffer,
                                               BufferAddress &address,
                                               QualType target16BitType,
                                               SourceRange range) {
  const auto loc = buffer->getSourceLocation();
  SpirvInstruction *result = nullptr;
  auto *constUint0 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 0));
  auto *constUint3 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 3));
  auto *constUint4 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 4));

  auto *index = address.getWord(loc, range);

  // Take the remainder and multiply by 8 to get the bit offset within the word.
  auto *bitOffset =
      spvBuilder.createBinaryOp(spv::Op::OpUMod, astContext.UnsignedIntTy,
                                address.getByte(), constUint4, loc, range);
  bitOffset = spvBuilder.createBinaryOp(spv::Op::OpShiftLeftLogical,
                                        astContext.UnsignedIntTy, bitOffset,
                                        constUint3, loc, range);

  // The underlying element type of the ByteAddressBuffer is uint. So we
  // need to load 32-bits at the very least.
  auto *ptr = spvBuilder.createAccessChain(astContext.UnsignedIntTy, buffer,
                                           {constUint0, index}, loc, range);
  result = spvBuilder.createLoad(astContext.UnsignedIntTy, ptr, loc, range);
  result = spvBuilder.createBinaryOp(spv::Op::OpShiftRightLogical,
                                     astContext.UnsignedIntTy, result,
                                     bitOffset, loc, range);
  result = spvBuilder.createUnaryOp(
      spv::Op::OpUConvert, astContext.UnsignedShortTy, result, loc, range);
  result = bitCastToNumericalOrBool(result, astContext.UnsignedShortTy,
                                    target16BitType, loc, range);
  result->setRValue();

  address.incrementByte(2, loc, range);
  return result;
}

SpirvInstruction *RawBufferHandler::load32Bits(SpirvInstruction *buffer,
                                               BufferAddress &address,
                                               QualType target32BitType,
                                               SourceRange range) {
  const auto loc = buffer->getSourceLocation();
  SpirvInstruction *result = nullptr;
  // Only need to perform one 32-bit uint load.
  auto *constUint0 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 0));

  auto *index = address.getWord(loc, range);

  auto *loadPtr = spvBuilder.createAccessChain(astContext.UnsignedIntTy, buffer,
                                               {constUint0, index}, loc, range);
  result = spvBuilder.createLoad(astContext.UnsignedIntTy, loadPtr, loc, range);
  result = bitCastToNumericalOrBool(result, astContext.UnsignedIntTy,
                                    target32BitType, loc, range);
  result->setRValue();

  address.incrementWord(loc, range);

  return result;
}

SpirvInstruction *RawBufferHandler::load64Bits(SpirvInstruction *buffer,
                                               BufferAddress &address,
                                               QualType target64BitType,
                                               SourceRange range) {
  const auto loc = buffer->getSourceLocation();
  SpirvInstruction *result = nullptr;
  SpirvInstruction *ptr = nullptr;
  auto *constUint0 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 0));
  auto *constUint32 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 32));

  auto *index = address.getWord(loc, range);

  // Need to perform two 32-bit uint loads and construct a 64-bit value.

  // Load the first 32-bit uint (word0).
  ptr = spvBuilder.createAccessChain(astContext.UnsignedIntTy, buffer,
                                     {constUint0, index}, loc, range);
  SpirvInstruction *word0 =
      spvBuilder.createLoad(astContext.UnsignedIntTy, ptr, loc, range);
  // Increment the base index
  address.incrementWord(loc, range);
  index = address.getWord(loc, range);
  // Load the second 32-bit uint (word1).
  ptr = spvBuilder.createAccessChain(astContext.UnsignedIntTy, buffer,
                                     {constUint0, index}, loc, range);
  SpirvInstruction *word1 =
      spvBuilder.createLoad(astContext.UnsignedIntTy, ptr, loc, range);

  // Convert both word0 and word1 to 64-bit uints.
  word0 = spvBuilder.createUnaryOp(
      spv::Op::OpUConvert, astContext.UnsignedLongLongTy, word0, loc, range);
  word1 = spvBuilder.createUnaryOp(
      spv::Op::OpUConvert, astContext.UnsignedLongLongTy, word1, loc, range);

  // Shift word1 to the left by 32 bits.
  word1 = spvBuilder.createBinaryOp(spv::Op::OpShiftLeftLogical,
                                    astContext.UnsignedLongLongTy, word1,
                                    constUint32, loc, range);

  // BitwiseOr word0 and word1.
  result = spvBuilder.createBinaryOp(spv::Op::OpBitwiseOr,
                                     astContext.UnsignedLongLongTy, word0,
                                     word1, loc, range);
  result = bitCastToNumericalOrBool(result, astContext.UnsignedLongLongTy,
                                    target64BitType, loc, range);
  result->setRValue();

  address.incrementWord(loc, range);

  return result;
}

SpirvInstruction *RawBufferHandler::processTemplatedLoadFromBuffer(
    SpirvInstruction *buffer, BufferAddress &address, const QualType targetType,
    SourceRange range) {
  const auto loc = buffer->getSourceLocation();
  SpirvInstruction *result = nullptr;

  // Scalar types
  if (isScalarType(targetType)) {
    SpirvInstruction *scalarResult = nullptr;
    auto loadWidth = getElementSpirvBitwidth(
        astContext, targetType, theEmitter.getSpirvOptions().enable16BitTypes);
    switch (loadWidth) {
    case 16:
      scalarResult = load16Bits(buffer, address, targetType, range);
      break;
    case 32:
      scalarResult = load32Bits(buffer, address, targetType, range);
      break;
    case 64:
      scalarResult = load64Bits(buffer, address, targetType, range);
      break;
    default:
      theEmitter.emitError(
          "templated load of ByteAddressBuffer is only implemented for "
          "16, 32, and 64-bit types",
          loc);
      return nullptr;
    }
    assert(scalarResult != nullptr);
    // We set the layout rule for scalars. Other types are built up from the
    // scalars, and should inherit this layout rule or default to Void.
    scalarResult->setLayoutRule(SpirvLayoutRule::Void);
    return scalarResult;
  }

  // Vector types
  {
    QualType elemType = {};
    uint32_t elemCount = 0;
    if (isVectorType(targetType, &elemType, &elemCount)) {
      llvm::SmallVector<SpirvInstruction *, 4> loadedElems;
      for (uint32_t i = 0; i < elemCount; ++i) {
        loadedElems.push_back(
            processTemplatedLoadFromBuffer(buffer, address, elemType, range));
      }
      result = spvBuilder.createCompositeConstruct(targetType, loadedElems, loc,
                                                   range);
      result->setRValue();
      return result;
    }
  }

  // Array types
  {
    QualType elemType = {};
    uint32_t elemCount = 0;
    if (const auto *arrType = astContext.getAsConstantArrayType(targetType)) {
      elemCount = static_cast<uint32_t>(arrType->getSize().getZExtValue());
      elemType = arrType->getElementType();
      llvm::SmallVector<SpirvInstruction *, 4> loadedElems;
      for (uint32_t i = 0; i < elemCount; ++i) {
        loadedElems.push_back(
            processTemplatedLoadFromBuffer(buffer, address, elemType, range));
      }
      result = spvBuilder.createCompositeConstruct(targetType, loadedElems, loc,
                                                   range);
      result->setRValue();
      return result;
    }
  }

  // Matrix types
  {
    QualType elemType = {};
    uint32_t numRows = 0;
    uint32_t numCols = 0;
    if (isMxNMatrix(targetType, &elemType, &numRows, &numCols)) {
      // In DX, the default matrix orientation in ByteAddressBuffer is column
      // major. If HLSL/DXIL support the `column_major` and `row_major`
      // attributes in the future, we will have to check for them here and
      // override the behavior.
      //
      // The assume buffer matrix order is controlled by the
      // `-fspv-use-legacy-buffer-matrix-order` flag:
      //   (a) false --> assume the matrix is stored column major
      //   (b) true  --> assume the matrix is stored row major
      //
      // We provide (b) for compatibility with legacy shaders that depend on
      // the previous, incorrect, raw buffer matrix order assumed by the SPIR-V
      // codegen.
      const bool isBufferColumnMajor =
          !theEmitter.getSpirvOptions().useLegacyBufferMatrixOrder;
      const uint32_t numElements = numRows * numCols;
      llvm::SmallVector<SpirvInstruction *, 16> loadedElems(numElements);
      for (uint32_t i = 0; i != numElements; ++i)
        loadedElems[i] =
            processTemplatedLoadFromBuffer(buffer, address, elemType, range);

      llvm::SmallVector<SpirvInstruction *, 4> loadedRows;
      for (uint32_t i = 0; i < numRows; ++i) {
        llvm::SmallVector<SpirvInstruction *, 4> loadedColumn;
        for (uint32_t j = 0; j < numCols; ++j) {
          const uint32_t elementIndex =
              isBufferColumnMajor ? (j * numRows + i) : (i * numCols + j);
          loadedColumn.push_back(loadedElems[elementIndex]);
        }
        const auto rowType = astContext.getExtVectorType(elemType, numCols);
        loadedRows.push_back(spvBuilder.createCompositeConstruct(
            rowType, loadedColumn, loc, range));
      }

      result = spvBuilder.createCompositeConstruct(targetType, loadedRows, loc,
                                                   range);
      result->setRValue();
      return result;
    }
  }

  // Struct types
  // The "natural" layout for structure types dictates that structs are
  // aligned like their field with the largest alignment.
  // As a result, there might exist some padding after some struct members.
  if (const auto *structType = targetType->getAs<RecordType>()) {
    const auto *decl = structType->getDecl();
    llvm::SmallVector<SpirvInstruction *, 4> loadedElems;
    uint32_t fieldOffsetInBytes = 0;
    uint32_t structAlignment = 0, structSize = 0, stride = 0;
    std::tie(structAlignment, structSize) =
        AlignmentSizeCalculator(astContext, theEmitter.getSpirvOptions())
            .getAlignmentAndSize(targetType,
                                 theEmitter.getSpirvOptions().sBufferLayoutRule,
                                 llvm::None, &stride);
    for (const auto *field : decl->fields()) {
      AlignmentSizeCalculator alignmentCalc(astContext,
                                            theEmitter.getSpirvOptions());
      uint32_t fieldSize = 0, fieldAlignment = 0;
      std::tie(fieldAlignment, fieldSize) = alignmentCalc.getAlignmentAndSize(
          field->getType(), theEmitter.getSpirvOptions().sBufferLayoutRule,
          /*isRowMajor*/ llvm::None, &stride);
      fieldOffsetInBytes = roundToPow2(fieldOffsetInBytes, fieldAlignment);
      auto *byteOffset = address.getByte();
      if (fieldOffsetInBytes != 0) {
        byteOffset = spvBuilder.createBinaryOp(
            spv::Op::OpIAdd, astContext.UnsignedIntTy, byteOffset,
            spvBuilder.getConstantInt(astContext.UnsignedIntTy,
                                      llvm::APInt(32, fieldOffsetInBytes)),
            loc, range);
      }

      loadedElems.push_back(processTemplatedLoadFromBuffer(
          buffer, byteOffset, field->getType(), range));

      fieldOffsetInBytes += fieldSize;
    }

    // After we're done with loading the entire struct, we need to update the
    // byteAddress (in case we are loading an array of structs).
    //
    // struct size = 34 bytes (34 / 8) = 4 full words (34 % 8) = 2 > 0,
    // therefore need to move to the next aligned address So the starting byte
    // offset after loading the entire struct is: 8 * (4 + 1) = 40
    assert(structAlignment != 0);
    SpirvInstruction *structWidth = spvBuilder.getConstantInt(
        astContext.UnsignedIntTy,
        llvm::APInt(32, roundToPow2(structSize, structAlignment)));
    address.incrementByte(structWidth, loc, range);

    result = spvBuilder.createCompositeConstruct(targetType, loadedElems, loc,
                                                 range);
    result->setRValue();
    return result;
  }

  llvm_unreachable("templated buffer load unimplemented for type");
}

SpirvInstruction *RawBufferHandler::processTemplatedLoadFromBuffer(
    SpirvInstruction *buffer, SpirvInstruction *byteAddress,
    const QualType targetType, SourceRange range) {
  BufferAddress address(byteAddress, theEmitter);

  return processTemplatedLoadFromBuffer(buffer, address, targetType, range);
}

void RawBufferHandler::store16Bits(SpirvInstruction *value,
                                   SpirvInstruction *buffer,
                                   BufferAddress &address,
                                   const QualType valueType,
                                   SourceRange range) {
  const auto loc = buffer->getSourceLocation();
  SpirvInstruction *result = nullptr;
  auto *constUint0 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 0));
  auto *constUint3 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 3));
  auto *constUint4 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 4));
  auto *constUint16 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 16));
  auto *constUintFFFF = spvBuilder.getConstantInt(astContext.UnsignedIntTy,
                                                  llvm::APInt(32, 0xffff));

  auto *index = address.getWord(loc, range);

  // Take the remainder and multiply by 8 to get the bit offset within the word.
  auto *bitOffset =
      spvBuilder.createBinaryOp(spv::Op::OpUMod, astContext.UnsignedIntTy,
                                address.getByte(), constUint4, loc, range);
  bitOffset = spvBuilder.createBinaryOp(spv::Op::OpShiftLeftLogical,
                                        astContext.UnsignedIntTy, bitOffset,
                                        constUint3, loc, range);

  // The underlying element type of the ByteAddressBuffer is uint. So we
  // need to store a 32-bit value.
  auto *ptr = spvBuilder.createAccessChain(astContext.UnsignedIntTy, buffer,
                                           {constUint0, index}, loc, range);

  result = bitCastToNumericalOrBool(value, valueType,
                                    astContext.UnsignedShortTy, loc, range);
  result = spvBuilder.createUnaryOp(
      spv::Op::OpUConvert, astContext.UnsignedIntTy, result, loc, range);
  result = spvBuilder.createBinaryOp(spv::Op::OpShiftLeftLogical,
                                     astContext.UnsignedIntTy, result,
                                     bitOffset, loc, range);

  auto *maskOffset =
      spvBuilder.createBinaryOp(spv::Op::OpISub, astContext.UnsignedIntTy,
                                constUint16, bitOffset, loc, range);

  auto *mask = spvBuilder.createBinaryOp(spv::Op::OpShiftLeftLogical,
                                         astContext.UnsignedIntTy,
                                         constUintFFFF, maskOffset, loc, range);

  // Load and mask the other value in the word.
  auto *masked = spvBuilder.createBinaryOp(
      spv::Op::OpBitwiseAnd, astContext.UnsignedIntTy,
      spvBuilder.createLoad(astContext.UnsignedIntTy, ptr, loc), mask, loc,
      range);

  result =
      spvBuilder.createBinaryOp(spv::Op::OpBitwiseOr, astContext.UnsignedIntTy,
                                masked, result, loc, range);
  spvBuilder.createStore(ptr, result, loc, range);
  address.incrementByte(2, loc, range);
}

void RawBufferHandler::store32Bits(SpirvInstruction *value,
                                   SpirvInstruction *buffer,
                                   BufferAddress &address,
                                   const QualType valueType,
                                   SourceRange range) {
  const auto loc = buffer->getSourceLocation();
  auto *constUint0 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 0));

  auto *index = address.getWord(loc, range);

  // The underlying element type of the ByteAddressBuffer is uint. So we
  // need to store a 32-bit value.
  auto *ptr = spvBuilder.createAccessChain(astContext.UnsignedIntTy, buffer,
                                           {constUint0, index}, loc, range);
  value = bitCastToNumericalOrBool(value, valueType, astContext.UnsignedIntTy,
                                   loc, range);
  spvBuilder.createStore(ptr, value, loc, range);
  address.incrementWord(loc, range);
}

void RawBufferHandler::store64Bits(SpirvInstruction *value,
                                   SpirvInstruction *buffer,
                                   BufferAddress &address,
                                   const QualType valueType,
                                   SourceRange range) {
  const auto loc = buffer->getSourceLocation();
  auto *constUint0 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 0));
  auto *constUint32 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 32));

  auto *index = address.getWord(loc, range);

  // The underlying element type of the ByteAddressBuffer is uint. So we
  // need to store two 32-bit values.
  auto *ptr = spvBuilder.createAccessChain(astContext.UnsignedIntTy, buffer,
                                           {constUint0, index}, loc, range);
  // First convert the 64-bit value to uint64_t. Then extract two 32-bit words
  // from it.
  value = bitCastToNumericalOrBool(value, valueType,
                                   astContext.UnsignedLongLongTy, loc, range);

  // Use OpUConvert to perform truncation (produces the least significant bits).
  SpirvInstruction *lsb = spvBuilder.createUnaryOp(
      spv::Op::OpUConvert, astContext.UnsignedIntTy, value, loc, range);

  // Shift uint64_t to the right by 32 bits and truncate to get the most
  // significant bits.
  SpirvInstruction *msb = spvBuilder.createUnaryOp(
      spv::Op::OpUConvert, astContext.UnsignedIntTy,
      spvBuilder.createBinaryOp(spv::Op::OpShiftRightLogical,
                                astContext.UnsignedLongLongTy, value,
                                constUint32, loc, range),
      loc, range);

  spvBuilder.createStore(ptr, lsb, loc, range);
  address.incrementWord(loc, range);
  index = address.getWord(loc, range);
  ptr = spvBuilder.createAccessChain(astContext.UnsignedIntTy, buffer,
                                     {constUint0, index}, loc, range);
  spvBuilder.createStore(ptr, msb, loc, range);
  address.incrementWord(loc, range);
}

QualType RawBufferHandler::serializeToScalarsOrStruct(
    std::deque<SpirvInstruction *> *values, QualType valueType,
    SourceLocation loc, SourceRange range) {
  uint32_t size = values->size();

  // Vector type
  {
    QualType elemType = {};
    uint32_t elemCount = 0;
    if (isVectorType(valueType, &elemType, &elemCount)) {
      for (uint32_t i = 0; i < size; ++i) {
        for (uint32_t j = 0; j < elemCount; ++j) {
          values->push_back(spvBuilder.createCompositeExtract(
              elemType, values->front(), {j}, loc, range));
        }
        values->pop_front();
      }
      return elemType;
    }
  }

  // Matrix type
  {
    QualType elemType = {};
    uint32_t numRows = 0, numCols = 0;
    if (isMxNMatrix(valueType, &elemType, &numRows, &numCols)) {
      // Check if the destination buffer expects matrices in column major or row
      // major order. In the future, we may also need to consider the
      // `row_major` and `column_major` attribures. This is not handled by
      // HLSL/DXIL at the moment, so we ignore them too.
      const bool isBufferColumnMajor =
          !theEmitter.getSpirvOptions().useLegacyBufferMatrixOrder;
      for (uint32_t i = 0; i < size; ++i) {
        if (isBufferColumnMajor) {
          // Access the matrix in the column major order.
          for (uint32_t j = 0; j != numCols; ++j) {
            for (uint32_t k = 0; k != numRows; ++k) {
              values->push_back(spvBuilder.createCompositeExtract(
                  elemType, values->front(), {k, j}, loc, range));
            }
          }
        } else {
          // Access the matrix in the row major order.
          for (uint32_t j = 0; j != numRows; ++j) {
            for (uint32_t k = 0; k != numCols; ++k) {
              values->push_back(spvBuilder.createCompositeExtract(
                  elemType, values->front(), {j, k}, loc, range));
            }
          }
        }
        values->pop_front();
      }
      return serializeToScalarsOrStruct(values, elemType, loc, range);
    }
  }

  // Array type
  {
    if (const auto *arrType = astContext.getAsConstantArrayType(valueType)) {
      const uint32_t arrElemCount =
          static_cast<uint32_t>(arrType->getSize().getZExtValue());
      const QualType arrElemType = arrType->getElementType();
      for (uint32_t i = 0; i < size; ++i) {
        for (uint32_t j = 0; j < arrElemCount; ++j) {
          values->push_back(spvBuilder.createCompositeExtract(
              arrElemType, values->front(), {j}, loc, range));
        }
        values->pop_front();
      }
      return serializeToScalarsOrStruct(values, arrElemType, loc, range);
    }
  }

  if (isScalarType(valueType))
    return valueType;

  if (valueType->getAs<RecordType>())
    return valueType;

  llvm_unreachable("unhandled type when serializing an array");
}

void RawBufferHandler::processTemplatedStoreToBuffer(SpirvInstruction *value,
                                                     SpirvInstruction *buffer,
                                                     BufferAddress &address,
                                                     const QualType valueType,
                                                     SourceRange range) {
  const auto loc = buffer->getSourceLocation();

  // Scalar types
  if (isScalarType(valueType)) {
    auto storeWidth = getElementSpirvBitwidth(
        astContext, valueType, theEmitter.getSpirvOptions().enable16BitTypes);
    switch (storeWidth) {
    case 16:
      store16Bits(value, buffer, address, valueType, range);
      return;
    case 32:
      store32Bits(value, buffer, address, valueType, range);
      return;
    case 64:
      store64Bits(value, buffer, address, valueType, range);
      return;
    default:
      theEmitter.emitError(
          "templated load of ByteAddressBuffer is only implemented for "
          "16, 32, and 64-bit types",
          loc);
      return;
    }
  }

  // Vectors, Matrices, and Arrays can all be serialized and stored.
  if (isVectorType(valueType) || isMxNMatrix(valueType) ||
      isConstantArrayType(astContext, valueType)) {
    std::deque<SpirvInstruction *> elems;
    elems.push_back(value);
    auto serializedType =
        serializeToScalarsOrStruct(&elems, valueType, loc, range);
    if (isScalarType(serializedType) || serializedType->getAs<RecordType>()) {
      for (auto elem : elems)
        processTemplatedStoreToBuffer(elem, buffer, address, serializedType,
                                      range);
    }
    return;
  }

  // Struct types
  // The "natural" layout for structure types dictates that structs are
  // aligned like their field with the largest alignment.
  // As a result, there might exist some padding after some struct members.
  if (const auto *structType = valueType->getAs<RecordType>()) {
    const auto *decl = structType->getDecl();
    uint32_t fieldOffsetInBytes = 0;
    uint32_t structAlignment = 0, structSize = 0, stride = 0;
    std::tie(structAlignment, structSize) =
        AlignmentSizeCalculator(astContext, theEmitter.getSpirvOptions())
            .getAlignmentAndSize(valueType,
                                 theEmitter.getSpirvOptions().sBufferLayoutRule,
                                 llvm::None, &stride);
    uint32_t fieldIndex = 0;
    for (const auto *field : decl->fields()) {
      AlignmentSizeCalculator alignmentCalc(astContext,
                                            theEmitter.getSpirvOptions());
      uint32_t fieldSize = 0, fieldAlignment = 0;
      std::tie(fieldAlignment, fieldSize) = alignmentCalc.getAlignmentAndSize(
          field->getType(), theEmitter.getSpirvOptions().sBufferLayoutRule,
          /*isRowMajor*/ llvm::None, &stride);
      fieldOffsetInBytes = roundToPow2(fieldOffsetInBytes, fieldAlignment);
      auto *byteOffset = address.getByte();
      if (fieldOffsetInBytes != 0) {
        byteOffset = spvBuilder.createBinaryOp(
            spv::Op::OpIAdd, astContext.UnsignedIntTy, byteOffset,
            spvBuilder.getConstantInt(astContext.UnsignedIntTy,
                                      llvm::APInt(32, fieldOffsetInBytes)),
            loc, range);
      }

      processTemplatedStoreToBuffer(
          spvBuilder.createCompositeExtract(field->getType(), value,
                                            {fieldIndex}, loc, range),
          buffer, byteOffset, field->getType(), range);

      fieldOffsetInBytes += fieldSize;
      ++fieldIndex;
    }

    // After we're done with storing the entire struct, we need to update the
    // byteAddress (in case we are storing an array of structs).
    //
    // Example: struct alignment = 8. struct size = 34 bytes
    // (34 / 8) = 4 full words
    // (34 % 8) = 2 > 0, therefore need to move to the next aligned address
    // So the starting byte offset after loading the entire struct is:
    // 8 * (4 + 1) = 40
    assert(structAlignment != 0);
    auto *structWidth = spvBuilder.getConstantInt(
        astContext.UnsignedIntTy,
        llvm::APInt(32, roundToPow2(structSize, structAlignment)));
    address.incrementByte(structWidth, loc, range);

    return;
  }

  llvm_unreachable("templated buffer store unimplemented for type");
}

void RawBufferHandler::processTemplatedStoreToBuffer(
    SpirvInstruction *value, SpirvInstruction *buffer,
    SpirvInstruction *&byteAddress, const QualType valueType,
    SourceRange range) {
  BufferAddress address(byteAddress, theEmitter);

  processTemplatedStoreToBuffer(value, buffer, address, valueType, range);
}

SpirvInstruction *RawBufferHandler::BufferAddress::getByte() {
  return byteAddress;
}

SpirvInstruction *RawBufferHandler::BufferAddress::getWord(SourceLocation loc,
                                                           SourceRange range) {
  if (!wordIndexOutdated) {
    return wordIndex;
  }

  auto *constUint2 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 2));

  // Divide the byte index by 4 (shift right by 2) to get the index in the
  // word-sized buffer.
  wordIndex = spvBuilder.createBinaryOp(spv::Op::OpShiftRightLogical,
                                        astContext.UnsignedIntTy, byteAddress,
                                        constUint2, loc, range);
  wordIndexOutdated = false;

  return wordIndex;
}

void RawBufferHandler::BufferAddress::incrementByte(SpirvInstruction *width,
                                                    SourceLocation loc,
                                                    SourceRange range) {
  byteAddress =
      spvBuilder.createBinaryOp(spv::Op::OpIAdd, astContext.UnsignedIntTy,
                                byteAddress, width, loc, range);
  wordIndexOutdated = true;
}

void RawBufferHandler::BufferAddress::incrementByte(uint32_t width,
                                                    SourceLocation loc,
                                                    SourceRange range) {
  incrementByte(spvBuilder.getConstantInt(astContext.UnsignedIntTy,
                                          llvm::APInt(32, width)),
                loc, range);
}

void RawBufferHandler::BufferAddress::incrementWord(SourceLocation loc,
                                                    SourceRange range) {

  auto *constUint1 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 1));

  // Keep byte address up-to-date. If this is unneeded the optimizer will remove
  // it.
  incrementByte(4, loc, range);

  wordIndex =
      spvBuilder.createBinaryOp(spv::Op::OpIAdd, astContext.UnsignedIntTy,
                                wordIndex, constUint1, loc, range);

  wordIndexOutdated = false;
}

} // namespace spirv
} // namespace clang
