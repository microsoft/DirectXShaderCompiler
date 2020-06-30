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
#include "clang/SPIRV/AstTypeProbe.h"
#include "clang/SPIRV/SpirvBuilder.h"
#include "clang/SPIRV/SpirvInstruction.h"

namespace {
/// Rounds the given value up to the given power of 2.
inline uint32_t roundToPow2(uint32_t val, uint32_t pow2) {
  assert(pow2 != 0);
  return (val + pow2 - 1) & ~(pow2 - 1);
}
} // anonymous namespace

namespace clang {
namespace spirv {

SpirvInstruction *
RawBufferHandler::bitCastToNumericalOrBool(SpirvInstruction *instr,
                                           QualType fromType, QualType toType,
                                           SourceLocation loc) {
  if (isSameType(astContext, fromType, toType))
    return instr;

  if (toType->isBooleanType() || fromType->isBooleanType())
    return theEmitter.castToType(instr, fromType, toType, loc);

  // Perform a bitcast
  return spvBuilder.createUnaryOp(spv::Op::OpBitcast, toType, instr, loc);
}

SpirvInstruction *RawBufferHandler::load16BitsAtBitOffset0(
    SpirvInstruction *buffer, SpirvInstruction *&index,
    QualType target16BitType, uint32_t &bitOffset) {
  assert(bitOffset == 0);
  const auto loc = buffer->getSourceLocation();
  SpirvInstruction *result = nullptr;
  auto *constUint0 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 0));
  // The underlying element type of the ByteAddressBuffer is uint. So we
  // need to load 32-bits at the very least.
  auto *loadPtr = spvBuilder.createAccessChain(astContext.UnsignedIntTy, buffer,
                                               {constUint0, index}, loc);
  result = spvBuilder.createLoad(astContext.UnsignedIntTy, loadPtr, loc);
  // Only need to mask the lowest 16 bits of the loaded 32-bit uint.
  // OpUConvert can perform truncation in this case.
  result = spvBuilder.createUnaryOp(spv::Op::OpUConvert,
                                    astContext.UnsignedShortTy, result, loc);
  result = bitCastToNumericalOrBool(result, astContext.UnsignedShortTy,
                                    target16BitType, loc);
  result->setRValue();

  // Now that a 16-bit load at bit-offset 0 has been performed, the next load
  // should be done at *the same base index* at bit-offset 16.
  bitOffset = (bitOffset + 16) % 32;
  return result;
}

SpirvInstruction *RawBufferHandler::load32BitsAtBitOffset0(
    SpirvInstruction *buffer, SpirvInstruction *&index,
    QualType target32BitType, uint32_t &bitOffset) {
  assert(bitOffset == 0);
  const auto loc = buffer->getSourceLocation();
  SpirvInstruction *result = nullptr;
  // Only need to perform one 32-bit uint load.
  auto *constUint0 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 0));
  auto *constUint1 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 1));
  auto *loadPtr = spvBuilder.createAccessChain(astContext.UnsignedIntTy, buffer,
                                               {constUint0, index}, loc);
  result = spvBuilder.createLoad(astContext.UnsignedIntTy, loadPtr, loc);
  result = bitCastToNumericalOrBool(result, astContext.UnsignedIntTy,
                                    target32BitType, loc);
  result->setRValue();
  // Now that a 32-bit load at bit-offset 0 has been performed, the next load
  // should be done at *the next base index* at bit-offset 0.
  bitOffset = (bitOffset + 32) % 32;
  index = spvBuilder.createBinaryOp(spv::Op::OpIAdd, astContext.UnsignedIntTy,
                                    index, constUint1, loc);

  return result;
}

SpirvInstruction *RawBufferHandler::load64BitsAtBitOffset0(
    SpirvInstruction *buffer, SpirvInstruction *&index,
    QualType target64BitType, uint32_t &bitOffset) {
  assert(bitOffset == 0);
  const auto loc = buffer->getSourceLocation();
  SpirvInstruction *result = nullptr;
  SpirvInstruction *ptr = nullptr;
  auto *constUint0 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 0));
  auto *constUint1 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 1));
  auto *constUint32 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 32));

  // Need to perform two 32-bit uint loads and construct a 64-bit value.

  // Load the first 32-bit uint (word0).
  ptr = spvBuilder.createAccessChain(astContext.UnsignedIntTy, buffer,
                                     {constUint0, index}, loc);
  SpirvInstruction *word0 =
      spvBuilder.createLoad(astContext.UnsignedIntTy, ptr, loc);
  // Increment the base index
  index = spvBuilder.createBinaryOp(spv::Op::OpIAdd, astContext.UnsignedIntTy,
                                    index, constUint1, loc);
  // Load the second 32-bit uint (word1).
  ptr = spvBuilder.createAccessChain(astContext.UnsignedIntTy, buffer,
                                     {constUint0, index}, loc);
  SpirvInstruction *word1 =
      spvBuilder.createLoad(astContext.UnsignedIntTy, ptr, loc);

  // Convert both word0 and word1 to 64-bit uints.
  word0 = spvBuilder.createUnaryOp(spv::Op::OpUConvert,
                                   astContext.UnsignedLongLongTy, word0, loc);
  word1 = spvBuilder.createUnaryOp(spv::Op::OpUConvert,
                                   astContext.UnsignedLongLongTy, word1, loc);

  // Shift word1 to the left by 32 bits.
  word1 = spvBuilder.createBinaryOp(spv::Op::OpShiftLeftLogical,
                                    astContext.UnsignedLongLongTy, word1,
                                    constUint32, loc);

  // BitwiseOr word0 and word1.
  result = spvBuilder.createBinaryOp(
      spv::Op::OpBitwiseOr, astContext.UnsignedLongLongTy, word0, word1, loc);
  result = bitCastToNumericalOrBool(result, astContext.UnsignedLongLongTy,
                                    target64BitType, loc);
  result->setRValue();
  // Now that a 64-bit load at bit-offset 0 has been performed, the next load
  // should be done at *the base index + 2* at bit-offset 0. The index has
  // already been incremented once. Need to increment it once more.
  bitOffset = (bitOffset + 64) % 32;
  index = spvBuilder.createBinaryOp(spv::Op::OpIAdd, astContext.UnsignedIntTy,
                                    index, constUint1, loc);

  return result;
}

SpirvInstruction *RawBufferHandler::load16BitsAtBitOffset16(
    SpirvInstruction *buffer, SpirvInstruction *&index,
    QualType target16BitType, uint32_t &bitOffset) {
  assert(bitOffset == 16);
  const auto loc = buffer->getSourceLocation();
  SpirvInstruction *result = nullptr;
  auto *constUint0 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 0));
  auto *constUint1 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 1));
  auto *constUint16 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 16));

  // The underlying element type of the ByteAddressBuffer is uint. So we
  // need to load 32-bits at the very least.
  auto *ptr = spvBuilder.createAccessChain(astContext.UnsignedIntTy, buffer,
                                           {constUint0, index}, loc);
  result = spvBuilder.createLoad(astContext.UnsignedIntTy, ptr, loc);
  result = spvBuilder.createBinaryOp(spv::Op::OpShiftRightLogical,
                                     astContext.UnsignedIntTy, result,
                                     constUint16, loc);
  result = spvBuilder.createUnaryOp(spv::Op::OpUConvert,
                                    astContext.UnsignedShortTy, result, loc);
  result = bitCastToNumericalOrBool(result, astContext.UnsignedShortTy,
                                    target16BitType, loc);
  result->setRValue();

  // Now that a 16-bit load at bit-offset 16 has been performed, the next load
  // should be done at *the next base index* at bit-offset 0.
  bitOffset = (bitOffset + 16) % 32;
  index = spvBuilder.createBinaryOp(spv::Op::OpIAdd, astContext.UnsignedIntTy,
                                    index, constUint1, loc);
  return result;
}

SpirvInstruction *RawBufferHandler::processTemplatedLoadFromBuffer(
    SpirvInstruction *buffer, SpirvInstruction *&index,
    const QualType targetType, uint32_t &bitOffset) {
  const auto loc = buffer->getSourceLocation();
  SpirvInstruction *result = nullptr;

  // TODO: If 8-bit types are to be supported in the future, we should also
  // add code to support bitOffset 8 and 24.
  assert(bitOffset == 0 || bitOffset == 16);

  // Scalar types
  if (isScalarType(targetType)) {
    auto loadWidth = getElementSpirvBitwidth(
        astContext, targetType, theEmitter.getSpirvOptions().enable16BitTypes);
    switch (bitOffset) {
    case 0: {
      switch (loadWidth) {
      case 16:
        return load16BitsAtBitOffset0(buffer, index, targetType, bitOffset);
        break;
      case 32:
        return load32BitsAtBitOffset0(buffer, index, targetType, bitOffset);
        break;
      case 64:
        return load64BitsAtBitOffset0(buffer, index, targetType, bitOffset);
        break;
      default:
        theEmitter.emitError(
            "templated load of ByteAddressBuffer is only implemented for "
            "16, 32, and 64-bit types",
            loc);
        return nullptr;
      }
      break;
    }
    case 16: {
      switch (loadWidth) {
      case 16:
        return load16BitsAtBitOffset16(buffer, index, targetType, bitOffset);
        break;
      case 32:
      case 64:
        theEmitter.emitError(
            "templated buffer load should not result in loading "
            "32-bit or 64-bit values at bit offset 16",
            loc);
        return nullptr;
      default:
        theEmitter.emitError(
            "templated load of ByteAddressBuffer is only implemented for "
            "16, 32, and 64-bit types",
            loc);
        return nullptr;
      }
      break;
    }
    default:
      theEmitter.emitError(
          "templated load of ByteAddressBuffer is only implemented for "
          "16, 32, and 64-bit types",
          loc);
      return nullptr;
    }
  }

  // Vector types
  {
    QualType elemType = {};
    uint32_t elemCount = 0;
    if (isVectorType(targetType, &elemType, &elemCount)) {
      llvm::SmallVector<SpirvInstruction *, 4> loadedElems;
      for (uint32_t i = 0; i < elemCount; ++i) {
        loadedElems.push_back(
            processTemplatedLoadFromBuffer(buffer, index, elemType, bitOffset));
      }
      result =
          spvBuilder.createCompositeConstruct(targetType, loadedElems, loc);
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
            processTemplatedLoadFromBuffer(buffer, index, elemType, bitOffset));
      }
      result =
          spvBuilder.createCompositeConstruct(targetType, loadedElems, loc);
      result->setRValue();
      return result;
    }
  }

  // Matrix types
  {
    QualType elemType = {};
    uint32_t numRows = 0, numCols = 0;
    if (isMxNMatrix(targetType, &elemType, &numRows, &numCols)) {
      llvm::SmallVector<SpirvInstruction *, 4> loadedElems;
      llvm::SmallVector<SpirvInstruction *, 4> loadedRows;
      for (uint32_t i = 0; i < numRows; ++i) {
        for (uint32_t j = 0; j < numCols; ++j) {
          // TODO: This is currently doing a row_major matrix load. We must
          // investigate whether we also need to implement it for column_major.
          loadedElems.push_back(processTemplatedLoadFromBuffer(
              buffer, index, elemType, bitOffset));
        }
        const auto rowType = astContext.getExtVectorType(elemType, numCols);
        loadedRows.push_back(
            spvBuilder.createCompositeConstruct(rowType, loadedElems, loc));
        loadedElems.clear();
      }
      result = spvBuilder.createCompositeConstruct(targetType, loadedRows, loc);
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
    SpirvInstruction *originalIndex = index;
    uint32_t originalBitOffset = bitOffset;
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
      const auto wordOffset =
          ((originalBitOffset / 8) + fieldOffsetInBytes) / 4;
      bitOffset = (((originalBitOffset / 8) + fieldOffsetInBytes) % 4) * 8;

      if (wordOffset != 0) {
        // Divide the fieldOffset by 4 to figure out how much to increment the
        // index into the buffer (increment occurs by 32-bit words since the
        // underlying type is an array of uints).
        // The remainder by four tells us the *byte offset* (then multiply by 8
        // to get bit offset).
        index = spvBuilder.createBinaryOp(
            spv::Op::OpIAdd, astContext.UnsignedIntTy, originalIndex,
            spvBuilder.getConstantInt(astContext.UnsignedIntTy,
                                      llvm::APInt(32, wordOffset)),
            loc);
      }
      loadedElems.push_back(processTemplatedLoadFromBuffer(
          buffer, index, field->getType(), bitOffset));

      fieldOffsetInBytes += fieldSize;
    }

    // After we're done with loading the entire struct, we need to update the
    // index and bitOffset (in case we are loading an array of structs).
    //
    // Example: struct alignment = 8. struct size = 34 bytes
    // (34 / 8) = 4 full words
    // (34 % 8) = 2 > 0, therefore need to move to the next aligned address
    // So the starting byte offset after loading the entire struct is:
    // 8 * (4 + 1) = 40
    assert(structAlignment != 0);
    uint32_t newByteOffset = roundToPow2(structSize, structAlignment);
    uint32_t newWordOffset = ((originalBitOffset / 8) + newByteOffset) / 4;
    bitOffset = 8 * (((originalBitOffset / 8) + newByteOffset) % 4);
    index = spvBuilder.createBinaryOp(
        spv::Op::OpIAdd, astContext.UnsignedIntTy, originalIndex,
        spvBuilder.getConstantInt(astContext.UnsignedIntTy,
                                  llvm::APInt(32, newWordOffset)),
        loc);

    result = spvBuilder.createCompositeConstruct(targetType, loadedElems, loc);
    result->setRValue();
    return result;
  }

  llvm_unreachable("templated buffer load unimplemented for type");
}

void RawBufferHandler::store16BitsAtBitOffset0(SpirvInstruction *value,
                                               SpirvInstruction *buffer,
                                               SpirvInstruction *&index,
                                               const QualType valueType) {
  const auto loc = buffer->getSourceLocation();
  SpirvInstruction *result = nullptr;
  auto *constUint0 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 0));
  // The underlying element type of the ByteAddressBuffer is uint. So we
  // need to store a 32-bit value.
  auto *ptr = spvBuilder.createAccessChain(astContext.UnsignedIntTy, buffer,
                                           {constUint0, index}, loc);
  result = bitCastToNumericalOrBool(value, valueType,
                                    astContext.UnsignedShortTy, loc);
  result = spvBuilder.createUnaryOp(spv::Op::OpUConvert,
                                    astContext.UnsignedIntTy, result, loc);
  spvBuilder.createStore(ptr, result, loc);
}

void RawBufferHandler::store16BitsAtBitOffset16(SpirvInstruction *value,
                                               SpirvInstruction *buffer,
                                               SpirvInstruction *&index,
                                               const QualType valueType) {
  const auto loc = buffer->getSourceLocation();
  SpirvInstruction *result = nullptr;
  auto *constUint0 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 0));
  auto *constUint1 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 1));
  auto *constUint16 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 16));
  // The underlying element type of the ByteAddressBuffer is uint. So we
  // need to store a 32-bit value.
  auto *ptr = spvBuilder.createAccessChain(astContext.UnsignedIntTy, buffer,
                                           {constUint0, index}, loc);
  result = bitCastToNumericalOrBool(value, valueType,
                                    astContext.UnsignedShortTy, loc);
  result = spvBuilder.createUnaryOp(spv::Op::OpUConvert,
                                    astContext.UnsignedIntTy, result, loc);
  result = spvBuilder.createBinaryOp(spv::Op::OpShiftLeftLogical,
                                     astContext.UnsignedIntTy, result,
                                     constUint16, loc);
  result = spvBuilder.createBinaryOp(
      spv::Op::OpBitwiseOr, astContext.UnsignedIntTy,
      spvBuilder.createLoad(astContext.UnsignedIntTy, ptr, loc), result, loc);
  spvBuilder.createStore(ptr, result, loc);
  index = spvBuilder.createBinaryOp(spv::Op::OpIAdd, astContext.UnsignedIntTy,
                                    index, constUint1, loc);
}

void RawBufferHandler::store32BitsAtBitOffset0(SpirvInstruction *value,
                                               SpirvInstruction *buffer,
                                               SpirvInstruction *&index,
                                               const QualType valueType) {
  const auto loc = buffer->getSourceLocation();
  auto *constUint0 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 0));
  auto *constUint1 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 1));
  // The underlying element type of the ByteAddressBuffer is uint. So we
  // need to store a 32-bit value.
  auto *ptr = spvBuilder.createAccessChain(astContext.UnsignedIntTy, buffer,
                                           {constUint0, index}, loc);
  value =
      bitCastToNumericalOrBool(value, valueType, astContext.UnsignedIntTy, loc);
  spvBuilder.createStore(ptr, value, loc);
  index = spvBuilder.createBinaryOp(spv::Op::OpIAdd, astContext.UnsignedIntTy,
                                    index, constUint1, loc);
}

void RawBufferHandler::store64BitsAtBitOffset0(SpirvInstruction *value,
                                               SpirvInstruction *buffer,
                                               SpirvInstruction *&index,
                                               const QualType valueType) {
  const auto loc = buffer->getSourceLocation();
  auto *constUint0 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 0));
  auto *constUint1 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 1));
  auto *constUint32 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 32));

  // The underlying element type of the ByteAddressBuffer is uint. So we
  // need to store two 32-bit values.
  auto *ptr = spvBuilder.createAccessChain(astContext.UnsignedIntTy, buffer,
                                           {constUint0, index}, loc);
  // First convert the 64-bit value to uint64_t. Then extract two 32-bit words
  // from it.
  value = bitCastToNumericalOrBool(value, valueType,
                                   astContext.UnsignedLongLongTy, loc);

  // Use OpUConvert to perform truncation (produces the least significant bits).
  SpirvInstruction *lsb = spvBuilder.createUnaryOp(
      spv::Op::OpUConvert, astContext.UnsignedIntTy, value, loc);

  // Shift uint64_t to the right by 32 bits and truncate to get the most
  // significant bits.
  SpirvInstruction *msb = spvBuilder.createUnaryOp(
      spv::Op::OpUConvert, astContext.UnsignedIntTy,
      spvBuilder.createBinaryOp(spv::Op::OpShiftRightLogical,
                                astContext.UnsignedLongLongTy, value,
                                constUint32, loc),
      loc);

  spvBuilder.createStore(ptr, lsb, loc);
  index = spvBuilder.createBinaryOp(spv::Op::OpIAdd, astContext.UnsignedIntTy,
                                    index, constUint1, loc);
  ptr = spvBuilder.createAccessChain(astContext.UnsignedIntTy, buffer,
                                     {constUint0, index}, loc);
  spvBuilder.createStore(ptr, msb, loc);
  index = spvBuilder.createBinaryOp(spv::Op::OpIAdd, astContext.UnsignedIntTy,
                                    index, constUint1, loc);
}

void RawBufferHandler::storeArrayOfScalars(
    std::deque<SpirvInstruction *> values, SpirvInstruction *buffer,
    SpirvInstruction *&index, const QualType valueType, uint32_t &bitOffset,
    SourceLocation loc) {
  auto *constUint0 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 0));
  auto *constUint1 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 1));
  auto *constUint16 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 16));
  const auto storeWidth = getElementSpirvBitwidth(
      astContext, valueType, theEmitter.getSpirvOptions().enable16BitTypes);
  const uint32_t elemCount = values.size();

  if (storeWidth == 16u) {
    uint32_t elemIndex = 0;
    if (bitOffset == 16) {
      // First store the first element at offset 16 of the last memory index.
      store16BitsAtBitOffset16(values[0], buffer, index, valueType);
      bitOffset = 0;
      ++elemIndex;
    }
    // Do a custom store based on the number of elements.
    for (; elemIndex < elemCount; elemIndex = elemIndex + 2) {
      // The underlying element type of the ByteAddressBuffer is uint. So we
      // need to store a 32-bit value by combining two 16-bit values.
      SpirvInstruction *word = nullptr;
      word = bitCastToNumericalOrBool(values[elemIndex], valueType,
                                      astContext.UnsignedShortTy, loc);
      // Zero-extend to 32 bits.
      word = spvBuilder.createUnaryOp(spv::Op::OpUConvert,
                                      astContext.UnsignedIntTy, word, loc);
      if (elemIndex + 1 < elemCount) {
        SpirvInstruction *msb = nullptr;
        msb = bitCastToNumericalOrBool(values[elemIndex + 1], valueType,
                                       astContext.UnsignedShortTy, loc);
        msb = spvBuilder.createUnaryOp(spv::Op::OpUConvert,
                                       astContext.UnsignedIntTy, msb, loc);
        msb = spvBuilder.createBinaryOp(spv::Op::OpShiftLeftLogical,
                                        astContext.UnsignedIntTy, msb,
                                        constUint16, loc);
        word = spvBuilder.createBinaryOp(
            spv::Op::OpBitwiseOr, astContext.UnsignedIntTy, word, msb, loc);
        // We will store two 16-bit values.
        bitOffset = (bitOffset + 32) % 32;
      } else {
        // We will store one 16-bit value.
        bitOffset = (bitOffset + 16) % 32;
      }

      auto *ptr = spvBuilder.createAccessChain(astContext.UnsignedIntTy, buffer,
                                               {constUint0, index}, loc);
      spvBuilder.createStore(ptr, word, loc);
      index = spvBuilder.createBinaryOp(
          spv::Op::OpIAdd, astContext.UnsignedIntTy, index, constUint1, loc);
    }
  } else if (storeWidth == 32u || storeWidth == 64u) {
    assert(bitOffset == 0);
    for (uint32_t i = 0; i < elemCount; ++i)
      processTemplatedStoreToBuffer(values[i], buffer, index, valueType, bitOffset);
  }
}

QualType RawBufferHandler::serializeToScalarsOrStruct(
    std::deque<SpirvInstruction *> *values, QualType valueType,
    SourceLocation loc) {
  uint32_t size = values->size();

  // Vector type
  {
    QualType elemType = {};
    uint32_t elemCount = 0;
    if (isVectorType(valueType, &elemType, &elemCount)) {
      for (uint32_t i = 0; i < size; ++i) {
        for (uint32_t j = 0; j < elemCount; ++j) {
          values->push_back(spvBuilder.createCompositeExtract(
              elemType, values->front(), {j}, loc));
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
      for (uint32_t i = 0; i < size; ++i) {
        for (uint32_t j = 0; j < numRows; ++j) {
          for (uint32_t k = 0; k < numCols; ++k) {
            // TODO: This is currently doing a row_major matrix store. We must
            // investigate whether we also need to implement it for
            // column_major.
            values->push_back(spvBuilder.createCompositeExtract(
                elemType, values->front(), {j, k}, loc));
          }
        }
        values->pop_front();
      }
      return serializeToScalarsOrStruct(values, elemType, loc);
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
              arrElemType, values->front(), {j}, loc));
        }
        values->pop_front();
      }
      return serializeToScalarsOrStruct(values, arrElemType, loc);
    }
  }

  if (isScalarType(valueType))
    return valueType;

  if (const auto *structType = valueType->getAs<RecordType>())
    return valueType;

  llvm_unreachable("unhandled type when serializing an array");
}

void RawBufferHandler::processTemplatedStoreToBuffer(SpirvInstruction *value,
                                                     SpirvInstruction *buffer,
                                                     SpirvInstruction *&index,
                                                     const QualType valueType,
                                                     uint32_t &bitOffset) {
  assert(bitOffset == 0 || bitOffset == 16);
  const auto loc = buffer->getSourceLocation();

  // Scalar types
  if (isScalarType(valueType)) {
    auto storeWidth = getElementSpirvBitwidth(
        astContext, valueType, theEmitter.getSpirvOptions().enable16BitTypes);
    switch (bitOffset) {
    case 0: {
      switch (storeWidth) {
      case 16:
        store16BitsAtBitOffset0(value, buffer, index, valueType);
        return;
      case 32:
        store32BitsAtBitOffset0(value, buffer, index, valueType);
        return;
      case 64:
        store64BitsAtBitOffset0(value, buffer, index, valueType);
        return;
      default:
        theEmitter.emitError(
            "templated load of ByteAddressBuffer is only implemented for "
            "16, 32, and 64-bit types",
            loc);
        return;
      }
    }
    case 16: {
      // The only legal store at offset 16 is by a 16-bit value.
      assert(storeWidth == 16);
      store16BitsAtBitOffset16(value, buffer, index, valueType);
      return;
    }
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
    auto serializedType = serializeToScalarsOrStruct(&elems, valueType, loc);
    if (isScalarType(serializedType)) {
      storeArrayOfScalars(elems, buffer, index, serializedType, bitOffset, loc);
    } else if (const auto *structType = serializedType->getAs<RecordType>()) {
      for (auto elem : elems)
        processTemplatedStoreToBuffer(elem, buffer, index, serializedType,
                                      bitOffset);
    }
    return;
  }

  // Struct types
  // The "natural" layout for structure types dictates that structs are
  // aligned like their field with the largest alignment.
  // As a result, there might exist some padding after some struct members.
  if (const auto *structType = valueType->getAs<RecordType>()) {
    const auto *decl = structType->getDecl();
    SpirvInstruction *originalIndex = index;
    const auto originalBitOffset = bitOffset;
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
      const auto wordOffset =
          ((originalBitOffset / 8) + fieldOffsetInBytes) / 4;
      bitOffset = (((originalBitOffset / 8) + fieldOffsetInBytes) % 4) * 8;

      if (wordOffset != 0) {
        // Divide the fieldOffset by 4 to figure out how much to increment the
        // index into the buffer (increment occurs by 32-bit words since the
        // underlying type is an array of uints).
        // The remainder by four tells us the *byte offset* (then multiply by 8
        // to get bit offset).
        index = spvBuilder.createBinaryOp(
            spv::Op::OpIAdd, astContext.UnsignedIntTy, originalIndex,
            spvBuilder.getConstantInt(astContext.UnsignedIntTy,
                                      llvm::APInt(32, wordOffset)),
            loc);
      }

      processTemplatedStoreToBuffer(
          spvBuilder.createCompositeExtract(field->getType(), value,
                                            {fieldIndex}, loc),
          buffer, index, field->getType(), bitOffset);

      fieldOffsetInBytes += fieldSize;
      ++fieldIndex;
    }

    // After we're done with storing the entire struct, we need to update the
    // index (in case we are stroring an array of structs).
    //
    // Example: struct alignment = 8. struct size = 34 bytes
    // (34 / 8) = 4 full words
    // (34 % 8) = 2 > 0, therefore need to move to the next aligned address
    // So the starting byte offset after loading the entire struct is:
    // 8 * (4 + 1) = 40
    assert(structAlignment != 0);
    uint32_t newByteOffset = roundToPow2(structSize, structAlignment);
    uint32_t newWordOffset = ((originalBitOffset / 8) + newByteOffset) / 4;
    bitOffset = 8 * (((originalBitOffset / 8) + newByteOffset) % 4);
    index = spvBuilder.createBinaryOp(
        spv::Op::OpIAdd, astContext.UnsignedIntTy, originalIndex,
        spvBuilder.getConstantInt(astContext.UnsignedIntTy,
                                  llvm::APInt(32, newWordOffset)),
        loc);

    return;
  }

  llvm_unreachable("templated buffer store unimplemented for type");
}

} // namespace spirv
} // namespace clang
