//===---- RawBufferMethods.cpp ---- Raw Buffer Methods ----------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//===----------------------------------------------------------------------===//

#include "RawBufferMethods.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/CharUnits.h"
#include "clang/AST/RecordLayout.h"
#include "clang/SPIRV/AstTypeProbe.h"
#include "clang/SPIRV/SpirvBuilder.h"
#include "clang/SPIRV/SpirvInstruction.h"

namespace clang {
namespace spirv {

SpirvInstruction *
RawBufferHandler::bitCastToNumericalOrBool(SpirvInstruction *instr,
                                           QualType fromType, QualType toType,
                                           SourceLocation loc) {
  if (isSameType(astContext, fromType, toType))
    return instr;

  if (toType->isBooleanType())
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

SpirvInstruction *RawBufferHandler::load32BitsAtBitOffset16(
    SpirvInstruction *buffer, SpirvInstruction *&index,
    QualType target32BitType, uint32_t &bitOffset) {
  assert(bitOffset == 16);
  const auto loc = buffer->getSourceLocation();
  SpirvInstruction *result = nullptr;
  SpirvInstruction *ptr = nullptr;
  auto *constUint0 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 0));
  auto *constUint1 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 1));
  auto *constUint16 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 16));

  // The underlying element type of the ByteAddressBuffer is uint. Since the
  // bitOffset is not zero, we need to perform two load operations.

  // Load the first 32-bit uint. Only its 16 MSBs matter.
  // The 16 MSBs of the loaded value becomes the 16 LSBs of the result.
  ptr = spvBuilder.createAccessChain(astContext.UnsignedIntTy, buffer,
                                     {constUint0, index}, loc);
  SpirvInstruction *lsb =
      spvBuilder.createLoad(astContext.UnsignedIntTy, ptr, loc);

  // Right shift by 16 bits leaves the upper 16 bits as 0.
  lsb = spvBuilder.createBinaryOp(spv::Op::OpShiftRightLogical,
                                  astContext.UnsignedIntTy, lsb, constUint16,
                                  loc);

  // Increment the base index
  index = spvBuilder.createBinaryOp(spv::Op::OpIAdd, astContext.UnsignedIntTy,
                                    index, constUint1, loc);

  // Load the second 32-bit uint. Only its 16 LSBs matter.
  // The 16 LSBs of the loaded value becomes the 16 MSBs of the result.
  ptr = spvBuilder.createAccessChain(astContext.UnsignedIntTy, buffer,
                                     {constUint0, index}, loc);
  SpirvInstruction *msb =
      spvBuilder.createLoad(astContext.UnsignedIntTy, ptr, loc);

  // Left shift by 16 bits leaves the lower 16 bits as 0.
  msb = spvBuilder.createBinaryOp(spv::Op::OpShiftLeftLogical,
                                  astContext.UnsignedIntTy, msb, constUint16,
                                  loc);

  // Bitwise Or the MSBs and LSBs to get the resulting 32-bit value.
  result = spvBuilder.createBinaryOp(spv::Op::OpBitwiseOr,
                                     astContext.UnsignedIntTy, lsb, msb, loc);

  result = bitCastToNumericalOrBool(result, astContext.UnsignedIntTy,
                                    target32BitType, loc);
  result->setRValue();

  // Now that a 32-bit load at bit-offset 16 has been performed, the next load
  // should be done at *the next base index* at bit-offset 16.
  // The base index has already been incremented.
  bitOffset = (bitOffset + 32) % 32;

  return result;
}

SpirvInstruction *RawBufferHandler::load64BitsAtBitOffset16(
    SpirvInstruction *buffer, SpirvInstruction *&index,
    QualType target64BitType, uint32_t &bitOffset) {
  assert(bitOffset == 16);
  const auto loc = buffer->getSourceLocation();
  SpirvInstruction *result = nullptr;
  SpirvInstruction *ptr = nullptr;
  auto *constUint0 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 0));
  auto *constUint1 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 1));
  auto *constUint16 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 16));
  auto *constUint48 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 48));

  // The underlying element type of the ByteAddressBuffer is uint. Since the
  // bitOffset is 16, we need to perform three load operations.
  // Use 16 bits from the first load, all the 32 bits from the second load, and
  // 16 bits from the third load.

  // Load the first 32-bit uint. Only its 16 MSBs matter.
  // Right shift by 16 bits leaves the upper 16 bits as 0.
  ptr = spvBuilder.createAccessChain(astContext.UnsignedIntTy, buffer,
                                     {constUint0, index}, loc);
  SpirvInstruction *first16 =
      spvBuilder.createLoad(astContext.UnsignedIntTy, ptr, loc);

  // Incremenet the index and load a 32-bit uint.
  index = spvBuilder.createBinaryOp(spv::Op::OpIAdd, astContext.UnsignedIntTy,
                                    index, constUint1, loc);
  ptr = spvBuilder.createAccessChain(astContext.UnsignedIntTy, buffer,
                                     {constUint0, index}, loc);
  SpirvInstruction *middle32 =
      spvBuilder.createLoad(astContext.UnsignedIntTy, ptr, loc);

  // Incremenet the index and load a 32-bit uint. Only its 16 LSBs matter.
  index = spvBuilder.createBinaryOp(spv::Op::OpIAdd, astContext.UnsignedIntTy,
                                    index, constUint1, loc);
  ptr = spvBuilder.createAccessChain(astContext.UnsignedIntTy, buffer,
                                     {constUint0, index}, loc);
  SpirvInstruction *last16 =
      spvBuilder.createLoad(astContext.UnsignedIntTy, ptr, loc);

  // Convert all parts to 64 bits
  first16 = spvBuilder.createUnaryOp(
      spv::Op::OpUConvert, astContext.UnsignedLongLongTy, first16, loc);
  middle32 = spvBuilder.createUnaryOp(
      spv::Op::OpUConvert, astContext.UnsignedLongLongTy, middle32, loc);
  last16 = spvBuilder.createUnaryOp(spv::Op::OpUConvert,
                                    astContext.UnsignedLongLongTy, last16, loc);

  // Perform: (first16 >> 16) | (middle32 << 16) | (last16 << 48)
  first16 = spvBuilder.createBinaryOp(spv::Op::OpShiftRightLogical,
                                      astContext.UnsignedLongLongTy, first16,
                                      constUint16, loc);
  middle32 = spvBuilder.createBinaryOp(spv::Op::OpShiftLeftLogical,
                                       astContext.UnsignedLongLongTy, middle32,
                                       constUint16, loc);
  last16 = spvBuilder.createBinaryOp(spv::Op::OpShiftLeftLogical,
                                     astContext.UnsignedLongLongTy, last16,
                                     constUint48, loc);

  result = spvBuilder.createBinaryOp(spv::Op::OpBitwiseOr,
                                     astContext.UnsignedLongLongTy, first16,
                                     middle32, loc);
  result = spvBuilder.createBinaryOp(
      spv::Op::OpBitwiseOr, astContext.UnsignedLongLongTy, result, last16, loc);

  result = bitCastToNumericalOrBool(result, astContext.UnsignedLongLongTy,
                                    target64BitType, loc);
  result->setRValue();

  // Now that a 64-bit load at bit-offset 16 has been performed, the next load
  // should be done at *the base index + 2* at bit-offset 16.
  // The base index has already been incremented twice.
  bitOffset = (bitOffset + 64) % 32;

  return result;
}

SpirvInstruction *RawBufferHandler::processTemplatedLoadFromBuffer(
    SpirvInstruction *buffer, SpirvInstruction *&index,
    const QualType targetType, uint32_t &bitOffset) {
  const auto loc = buffer->getSourceLocation();
  SpirvInstruction *result = nullptr;
  auto *constUint0 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 0));
  auto *constUint1 =
      spvBuilder.getConstantInt(astContext.UnsignedIntTy, llvm::APInt(32, 1));

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
        return load32BitsAtBitOffset16(buffer, index, targetType, bitOffset);
        break;
      case 64:
        return load64BitsAtBitOffset16(buffer, index, targetType, bitOffset);
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
    assert(bitOffset == 0);
    const auto &layout = astContext.getASTRecordLayout(decl);
    SpirvInstruction *originalIndex = index;
    uint32_t originalBitOffset = bitOffset;
    llvm::SmallVector<SpirvInstruction *, 4> loadedElems;
    uint32_t structAlignment = 0;
    uint32_t structSize = static_cast<uint32_t>(layout.getSize().getQuantity());
    uint32_t fieldIndex = 0;
    for (const auto *field : decl->fields()) {
      CharUnits alignment;
      std::tie(std::ignore, alignment) = astContext.getTypeInfoInChars(
          field->getType()->getUnqualifiedDesugaredType());
      structAlignment = std::max(
          structAlignment, static_cast<uint32_t>(alignment.getQuantity()));

      assert(fieldIndex < layout.getFieldCount());
      uint32_t fieldOffsetInBytes = static_cast<uint32_t>(
          astContext.toCharUnitsFromBits(layout.getFieldOffset(fieldIndex++))
              .getQuantity());
      if (fieldOffsetInBytes != 0) {
        // Divide the fieldOffset by 4 to figure out how much to increment the
        // index into the buffer (increment occurs by 32-bit words since the
        // underlying type is an array of uints).
        // The remainder by four tells us the *byte offset* (then multiply by 8
        // to get bit offset).
        auto wordOffset = fieldOffsetInBytes / 4;
        bitOffset = (fieldOffsetInBytes % 4) * 8;
        index = spvBuilder.createBinaryOp(
            spv::Op::OpIAdd, astContext.UnsignedIntTy, originalIndex,
            spvBuilder.getConstantInt(astContext.UnsignedIntTy,
                                      llvm::APInt(32, wordOffset)),
            loc);
      }
      loadedElems.push_back(processTemplatedLoadFromBuffer(
          buffer, index, field->getType(), bitOffset));
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
    uint32_t newByteOffset =
        structAlignment * ((structSize / structAlignment) +
                           (structSize % structAlignment > 0 ? 1 : 0));
    uint32_t newWordOffset = newByteOffset / 4;
    index = spvBuilder.createBinaryOp(
        spv::Op::OpIAdd, astContext.UnsignedIntTy, originalIndex,
        spvBuilder.getConstantInt(astContext.UnsignedIntTy,
                                  llvm::APInt(32, newWordOffset)),
        loc);

    // New bitOffset should be zero because after loading the struct, we will
    // be loading at the next aligned address.
    bitOffset = 0;
    result = spvBuilder.createCompositeConstruct(targetType, loadedElems, loc);
    result->setRValue();
    return result;
  }

  llvm_unreachable("templated buffer load unimplemented for type");
}

} // namespace spirv
} // namespace clang
