//===- LEB128.cpp - LEB128 utility functions implementation -----*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// LEB128.cpp                                                                //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file implements some utility functions for encoding SLEB128 and      //
// ULEB128 values.                                                           //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/Support/LEB128.h"

namespace llvm {

/// Utility function to get the size of the ULEB128-encoded value.
unsigned getULEB128Size(uint64_t Value) {
  unsigned Size = 0;
  do {
    Value >>= 7;
    Size += sizeof(int8_t);
  } while (Value);
  return Size;
}

/// Utility function to get the size of the SLEB128-encoded value.
unsigned getSLEB128Size(int64_t Value) {
  unsigned Size = 0;
  int Sign = Value >> (8 * sizeof(Value) - 1);
  bool IsMore;

  do {
    unsigned Byte = Value & 0x7f;
    Value >>= 7;
    IsMore = Value != Sign || ((Byte ^ Sign) & 0x40) != 0;
    Size += sizeof(int8_t);
  } while (IsMore);
  return Size;
}

}  // namespace llvm
