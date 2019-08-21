//===------ RawBufferMethods.h ---- Raw Buffer Methods ----------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//===----------------------------------------------------------------------===//
#ifndef LLVM_CLANG_SPIRV_RAWBUFFERMETHODS_H
#define LLVM_CLANG_SPIRV_RAWBUFFERMETHODS_H

class ASTContext;
class SpirvBuilder;
class SpirvInstruction;

#include "SpirvEmitter.h"

namespace clang {
namespace spirv {

class RawBufferHandler {
public:
  RawBufferHandler(SpirvEmitter &emitter)
      : theEmitter(emitter), astContext(emitter.getASTContext()),
        spvBuilder(emitter.getSpirvBuilder()) {}

  /// \brief Performs (RW)ByteAddressBuffer.Load<T>(address).
  /// (RW)ByteAddressBuffers are represented as structs with only one member
  /// which is a runtime array in SPIR-V. This method works by loading one or
  /// more uints, and performing necessary casts and composite constructions
  /// to build the 'targetType'. The 'offset' parameter can be used for finer
  /// grained load of bitwidths smaller than 32-bits. Example: targetType =
  /// uint16_t, address=0, offset=0
  ///                 --> Load the first 16-bit uint starting at address 0.
  /// targetType = uint16_t, address=0, offset=16
  ///                 --> Load the second 16-bit uint starting at address 0.
  SpirvInstruction *processTemplatedLoadFromBuffer(SpirvInstruction *buffer,
                                                   SpirvInstruction *&index,
                                                   const QualType targetType,
                                                   uint32_t &bitOffset);

private:
  SpirvInstruction *load16BitsAtBitOffset0(SpirvInstruction *buffer,
                                           SpirvInstruction *&index,
                                           QualType target16BitType,
                                           uint32_t &bitOffset);

  SpirvInstruction *load32BitsAtBitOffset0(SpirvInstruction *buffer,
                                           SpirvInstruction *&index,
                                           QualType target32BitType,
                                           uint32_t &bitOffset);

  SpirvInstruction *load64BitsAtBitOffset0(SpirvInstruction *buffer,
                                           SpirvInstruction *&index,
                                           QualType target64BitType,
                                           uint32_t &bitOffset);

  SpirvInstruction *load16BitsAtBitOffset16(SpirvInstruction *buffer,
                                            SpirvInstruction *&index,
                                            QualType target16BitType,
                                            uint32_t &bitOffset);

private:
  /// \brief Performs an OpBitCast from |fromType| to |toType| on the given
  /// instruction.
  ///
  /// If the |toType| is a boolean type, it performs a regular type cast.
  ///
  /// If the |fromType| and |toType| are the same, does not thing and returns
  /// the given instruction
  SpirvInstruction *bitCastToNumericalOrBool(SpirvInstruction *instr,
                                             QualType fromType, QualType toType,
                                             SourceLocation loc);

private:
  SpirvEmitter &theEmitter;
  ASTContext &astContext;
  SpirvBuilder &spvBuilder;
};

} // namespace spirv
} // namespace clang

#endif // LLVM_CLANG_SPIRV_RAWBUFFERMETHODS_H
