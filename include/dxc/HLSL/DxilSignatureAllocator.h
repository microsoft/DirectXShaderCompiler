///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilSignatureAllocation.h                                                 //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Classes used for allocating signature elements.                           //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "dxc/HLSL/DxilSignature.h"

namespace hlsl {

class DxilSignatureAllocator {
public:
  // index flags
  static const uint8_t kIndexedUp = 1 << 0;     // Indexing continues upwards
  static const uint8_t kIndexedDown = 1 << 1;   // Indexing continues downwards
  static uint8_t GetIndexFlags(unsigned row, unsigned rows) {
    return ((row > 0) ? kIndexedUp : 0) | ((row < rows - 1) ? kIndexedDown : 0);
  }
  // element flags
  static const uint8_t kEFOccupied = 1 << 0;
  static const uint8_t kEFArbitrary = 1 << 1;
  static const uint8_t kEFSGV = 1 << 2;
  static const uint8_t kEFSV = 1 << 3;
  static const uint8_t kEFTessFactor = 1 << 4;
  static const uint8_t kEFConflictsWithIndexed = kEFSGV | kEFSV;
  static uint8_t GetElementFlags(const DxilSignatureElement *SE);

  // The following two functions enforce the rules of component ordering when packing different
  // kinds of elements into the same register.

  // given element flags, return element flags that conflict when placed to the left of the element
  static uint8_t GetConflictFlagsLeft(uint8_t flags);
  // given element flags, return element flags that conflict when placed to the right of the element
  static uint8_t GetConflictFlagsRight(uint8_t flags);

  enum ConflictType {
    kNoConflict = 0,
    kConflictsWithIndexed,
    kConflictsWithIndexedTessFactor,
    kConflictsWithInterpolationMode,
    kInsufficientFreeComponents,
    kOverlapElement,
    kIllegalComponentOrder,
    kConflictFit,
  };

  struct PackedRegister {
    // Flags:
    // - for occupied components, they signify element flags
    // - for unoccupied components, they signify conflict flags
    uint8_t Flags[4];
    DXIL::InterpolationMode Interp : 4;
    uint8_t IndexFlags : 2;
    uint8_t IndexingFixed : 1;

    PackedRegister();
    ConflictType DetectRowConflict(uint8_t flags, uint8_t indexFlags, DXIL::InterpolationMode interp, unsigned width);
    ConflictType DetectColConflict(uint8_t flags, unsigned col, unsigned width);
    void PlaceElement(uint8_t flags, uint8_t indexFlags, DXIL::InterpolationMode interp, unsigned col, unsigned width);
  };

  std::vector<PackedRegister> Registers;

  DxilSignatureAllocator(unsigned numRegisters);

  ConflictType DetectRowConflict(const DxilSignatureElement *SE, unsigned row);
  ConflictType DetectColConflict(const DxilSignatureElement *SE, unsigned row, unsigned col);
  void PlaceElement(const DxilSignatureElement *SE, unsigned row, unsigned col);

  unsigned PackNext(DxilSignatureElement* SE, unsigned startRow, unsigned numRows, unsigned startCol = 0);

  // Simple greedy in-order packer used by PackMain
  unsigned PackGreedy(std::vector<DxilSignatureElement*> elements, unsigned startRow, unsigned numRows, unsigned startCol = 0);

  // Main packing algorithm
  unsigned PackMain(std::vector<DxilSignatureElement*> elements, unsigned startRow, unsigned numRows);

  // Pack in a prefix-stable way.
  unsigned PackPrefixStable(std::vector<DxilSignatureElement*> elements, unsigned startRow, unsigned numRows);

};


} // namespace hlsl
