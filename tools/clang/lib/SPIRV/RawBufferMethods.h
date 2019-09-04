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
  /// grained load of bitwidths smaller than 32-bits.
  ///
  /// Example:
  /// targetType = uint16_t, address=0, offset=0
  ///                 --> Load the first 16-bit uint starting at address 0.
  /// targetType = uint16_t, address=0, offset=16
  ///                 --> Load the second 16-bit uint starting at address 0.
  SpirvInstruction *processTemplatedLoadFromBuffer(SpirvInstruction *buffer,
                                                   SpirvInstruction *&index,
                                                   const QualType targetType,
                                                   uint32_t &bitOffset);

  /// \brief Performs RWByteAddressBuffer.Store<T>(address, value).
  /// RWByteAddressBuffers are represented in SPIR-V as structs with only one
  /// member which is a runtime array of uints. This method works by decomposing
  /// the given |value| to reach numeric/bool types. Then performs necessary
  /// casts to uints and stores them in the underlying runtime array.
  /// The |bitOffset| parameter can be used for finer-grained bit-offset
  /// control.
  ///
  /// Example:
  /// targetType = uint16_t, address=0, offset=0
  ///                 --> Store to the first 16-bit uint starting at address 0.
  /// targetType = uint16_t, address=0, offset=16
  ///                 --> Store to the second 16-bit uint starting at address 0.
  void processTemplatedStoreToBuffer(SpirvInstruction *value,
                                     SpirvInstruction *buffer,
                                     SpirvInstruction *&index,
                                     const QualType valueType,
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
  void store16BitsAtBitOffset0(SpirvInstruction *value,
                               SpirvInstruction *buffer,
                               SpirvInstruction *&index,
                               const QualType valueType);

  void store32BitsAtBitOffset0(SpirvInstruction *value,
                               SpirvInstruction *buffer,
                               SpirvInstruction *&index,
                               const QualType valueType);

  void store64BitsAtBitOffset0(SpirvInstruction *value,
                               SpirvInstruction *buffer,
                               SpirvInstruction *&index,
                               const QualType valueType);

  void store16BitsAtBitOffset16(SpirvInstruction *value,
                                SpirvInstruction *buffer,
                                SpirvInstruction *&index,
                                const QualType valueType);

  void storeArrayOfScalars(std::deque<SpirvInstruction *> values,
                           SpirvInstruction *buffer, SpirvInstruction *&index,
                           const QualType valueType, uint32_t &bitOffset,
                           SourceLocation);

  /// \brief Serializes the given values into their components until a scalar or
  /// a struct has been reached. Returns the most basic type it reaches.
  QualType serializeToScalarsOrStruct(std::deque<SpirvInstruction *> *values,
                                      QualType valueType, SourceLocation);

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
