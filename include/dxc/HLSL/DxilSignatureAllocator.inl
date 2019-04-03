///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilSignatureAllocator.inl                                                //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

//#include "dxc/Support/Global.h"   // for DXASSERT
//#include "dxc/HLSL/DxilSignatureAllocator.h"

using std::vector;      // #include <vector>
using std::unique_ptr;  // #include <memory>
using std::sort;        // #include <algorithm>

namespace hlsl {

//------------------------------------------------------------------------------
//
// DxilSignatureAllocator methods.
//
uint8_t DxilSignatureAllocator::GetElementFlags(const PackElement *SE) {
  uint8_t flags = 0;
  DXIL::SemanticInterpretationKind interpretation = SE->GetInterpretation();
  switch (interpretation) {
    case DXIL::SemanticInterpretationKind::Arb:
      flags |= kEFArbitrary;
      break;
    case DXIL::SemanticInterpretationKind::SV:
      flags |= kEFSV;
      break;
    case DXIL::SemanticInterpretationKind::SGV:
      flags |= kEFSGV;
      break;
    case DXIL::SemanticInterpretationKind::TessFactor:
      flags |= kEFTessFactor;
      break;
    case DXIL::SemanticInterpretationKind::ClipCull:
      flags |= kEFClipCull;
      break;
    default:
      DXASSERT(false, "otherwise, unexpected interpretation for allocated element");
  }
  return flags;
}

// The following two functions enforce the rules of component ordering when packing different
// kinds of elements into the same register.

// given element flags, return element flags that conflict when placed to the left of the element
uint8_t DxilSignatureAllocator::GetConflictFlagsLeft(uint8_t flags) {
  uint8_t conflicts = 0;
  if (flags & kEFArbitrary)
    conflicts |= kEFSGV | kEFSV | kEFTessFactor | kEFClipCull;
  if (flags & kEFSV)
    conflicts |= kEFSGV;
  if (flags & kEFTessFactor)
    conflicts |= kEFSGV;
  if (flags & kEFClipCull)
    conflicts |= kEFSGV;
  return conflicts;
}

// given element flags, return element flags that conflict when placed to the right of the element
uint8_t DxilSignatureAllocator::GetConflictFlagsRight(uint8_t flags) {
  uint8_t conflicts = 0;
  if (flags & kEFSGV)
    conflicts |= kEFArbitrary | kEFSV | kEFTessFactor | kEFClipCull;
  if (flags & kEFSV)
    conflicts |= kEFArbitrary;
  if (flags & kEFTessFactor)
    conflicts |= kEFArbitrary;
  if (flags & kEFClipCull)
    conflicts |= kEFArbitrary;
  return conflicts;
}

DxilSignatureAllocator::PackedRegister::PackedRegister()
    : Interp(DXIL::InterpolationMode::Undefined), IndexFlags(0),
      IndexingFixed(0), DataWidth(DXIL::SignatureDataWidth::Undefined) {
  for (unsigned i = 0; i < 4; ++i)
    Flags[i] = 0;
}

DxilSignatureAllocator::ConflictType DxilSignatureAllocator::PackedRegister::DetectRowConflict(uint8_t flags, uint8_t indexFlags, DXIL::InterpolationMode interp, unsigned width, DXIL::SignatureDataWidth dataWidth) {
  // indexing already present, and element incompatible with indexing
  if (IndexFlags && (flags & kEFConflictsWithIndexed))
    return kConflictsWithIndexed;
  // indexing cannot be changed, and element indexing is incompatible when merged
  if (IndexingFixed && (indexFlags | IndexFlags) != IndexFlags)
    return kConflictsWithIndexed;
  if ((flags & kEFTessFactor) && (indexFlags | IndexFlags) != indexFlags)
    return kConflictsWithIndexedTessFactor;
  if (Interp != DXIL::InterpolationMode::Undefined && Interp != interp)
    return kConflictsWithInterpolationMode;
  if (DataWidth != DXIL::SignatureDataWidth::Undefined && DataWidth != dataWidth)
    return kConflictDataWidth;
  unsigned freeWidth = 0;
  for (unsigned i = 0; i < 4; ++i) {
    if ((Flags[i] & kEFOccupied) || (Flags[i] & flags))
      freeWidth = 0;
    else
      ++freeWidth;
    if (width <= freeWidth)
      break;
  }
  if (width > freeWidth)
    return kInsufficientFreeComponents;
  return kNoConflict;
}

DxilSignatureAllocator::ConflictType DxilSignatureAllocator::PackedRegister::DetectColConflict(uint8_t flags, unsigned col, unsigned width) {
  if (col + width > 4)
    return kConflictFit;
  flags |= kEFOccupied;
  for (unsigned i = col; i < col + width; ++i) {
    if (flags & Flags[i]) {
      if (Flags[i] & kEFOccupied)
        return kOverlapElement;
      else
        return kIllegalComponentOrder;
    }
  }
  return kNoConflict;
}

void DxilSignatureAllocator::PackedRegister::PlaceElement(
    uint8_t flags, uint8_t indexFlags, DXIL::InterpolationMode interp,
    unsigned col, unsigned width, DXIL::SignatureDataWidth dataWidth) {
  // Assume no conflicts (DetectRowConflict and DetectColConflict both return 0).
  Interp = interp;
  IndexFlags |= indexFlags;
  DataWidth = dataWidth;
  if ((flags & kEFConflictsWithIndexed) || (flags & kEFTessFactor)) {
    DXASSERT(indexFlags == IndexFlags, "otherwise, bug in DetectRowConflict checking index flags");
    IndexingFixed = 1;
  }
  uint8_t conflictLeft = GetConflictFlagsLeft(flags);
  uint8_t conflictRight = GetConflictFlagsRight(flags);
  for (unsigned i = 0; i < 4; ++i) {
    if ((Flags[i] & kEFOccupied) == 0) {
      if (i < col)
        Flags[i] |= conflictLeft;
      else if (i < col + width)
        Flags[i] = kEFOccupied | flags;
      else
        Flags[i] |= conflictRight;
    }
  }
}

DxilSignatureAllocator::DxilSignatureAllocator(unsigned numRegisters, bool useMinPrecision)
  : m_bIgnoreIndexing(false), m_bUseMinPrecision(useMinPrecision) {
  m_Registers.resize(numRegisters);
}

DxilSignatureAllocator::ConflictType DxilSignatureAllocator::DetectRowConflict(const PackElement *SE, unsigned row) {
  unsigned rows = SE->GetRows();
  if (rows + row > m_Registers.size())
    return kConflictFit;
  unsigned cols = SE->GetCols();
  DXIL::InterpolationMode interp = SE->GetInterpolationMode();
  uint8_t flags = GetElementFlags(SE);
  for (unsigned i = 0; i < rows; ++i) {
    uint8_t indexFlags = m_bIgnoreIndexing ? 0 : GetIndexFlags(i, rows);
    ConflictType conflict = m_Registers[row + i].DetectRowConflict(flags, indexFlags, interp, cols, SE->GetDataBitWidth());
    if (conflict)
      return conflict;
  }
  return kNoConflict;
}

DxilSignatureAllocator::ConflictType DxilSignatureAllocator::DetectColConflict(const PackElement *SE, unsigned row, unsigned col) {
  unsigned rows = SE->GetRows();
  unsigned cols = SE->GetCols();
  uint8_t flags = GetElementFlags(SE);
  for (unsigned i = 0; i < rows; ++i) {
    ConflictType conflict = m_Registers[row + i].DetectColConflict(flags, col, cols);
    if (conflict)
      return conflict;
  }
  return kNoConflict;
}

void DxilSignatureAllocator::PlaceElement(const PackElement *SE, unsigned row, unsigned col) {
  // Assume no conflicts (DetectRowConflict and DetectColConflict both return 0).
  unsigned rows = SE->GetRows();
  unsigned cols = SE->GetCols();
  DXIL::InterpolationMode interp = SE->GetInterpolationMode();
  uint8_t flags = GetElementFlags(SE);
  for (unsigned i = 0; i < rows; ++i) {
    uint8_t indexFlags = m_bIgnoreIndexing ? 0 : GetIndexFlags(i, rows);
    m_Registers[row + i].PlaceElement(flags, indexFlags, interp, col, cols, SE->GetDataBitWidth());
  }
}


namespace {

template <typename T>
int cmp(T a, T b) {
  if (a < b)
    return -1;
  if (b < a)
    return 1;
  return 0;
}
int CmpElements(const DxilSignatureAllocator::PackElement* left, const DxilSignatureAllocator::PackElement* right) {
  unsigned result;
  result = cmp((unsigned)left->GetInterpolationMode(), (unsigned)right->GetInterpolationMode());
  if (result) return result;
  result = -cmp(left->GetRows(), right->GetRows());
  if (result) return result;
  result = -cmp(left->GetCols(), right->GetCols());
  if (result) return result;
  result = cmp(left->GetID(), right->GetID());
  if (result) return result;
  return 0;
}

struct {
  bool operator()(const DxilSignatureAllocator::PackElement* left, const DxilSignatureAllocator::PackElement* right) {
    return CmpElements(left, right) < 0;
  }
} CmpElementsLess;

} // anonymous namespace

unsigned DxilSignatureAllocator::PackNext(PackElement* SE, unsigned startRow, unsigned numRows, unsigned startCol) {
  unsigned rowsUsed = startRow;

  unsigned rows = SE->GetRows();
  if (rows > numRows)
    return rowsUsed; // element will not fit

  unsigned cols = SE->GetCols();
  DXASSERT_NOMSG(startCol + cols <= 4);

  for (unsigned row = startRow; row <= (startRow + numRows - rows); ++row) {
    if (DetectRowConflict(SE, row))
      continue;
    for (unsigned col = startCol; col <= 4 - cols; ++col) {
      if (DetectColConflict(SE, row, col))
        continue;
      PlaceElement(SE, row, col);
      SE->SetLocation(row, col);
      return row + rows;
    }
  }

  return rowsUsed;
}

unsigned DxilSignatureAllocator::PackGreedy(std::vector<PackElement*> elements, unsigned startRow, unsigned numRows, unsigned startCol) {
  // Allocation failures should be caught by IsFullyAllocated()
  unsigned rowsUsed = startRow;

  for (auto &SE : elements) {
    rowsUsed = std::max(rowsUsed, PackNext(SE, startRow, numRows, startCol));
  }

  return rowsUsed;
}

unsigned DxilSignatureAllocator::PackOptimized(std::vector<PackElement*> elements, unsigned startRow, unsigned numRows) {
  unsigned rowsUsed = startRow;

  // Clip/Cull needs special handling due to limitations unique to these.
  //  Otherwise, packer could easily pack across too many registers in available gaps.
  // The rules are special/weird:
  //  - for interpolation mode, clip must be linear or linearCentroid, while cull may be anything
  //  - both have a maximum of 8 components shared between them
  //  - you can have a combined maximum of two registers declared with clip or cull SV's
  // other SV rules still apply:
  //  - no indexing allowed
  //  - cannot come before arbitrary values in same register
  // Strategy for dealing with these:
  //  - attempt to pack these into a two register allocator
  //    - if this fails, some constraint is blocking, or declaration order is preventing good packing
  //      for example: 2, 1, 2, 3 - total 8 components and packable, but if greedily packed, it will fail
  //      Packing largest to smallest would solve this.
  //  - track components used for each register and create temp elements for allocation tests

  // Packing overview
  //  - pack 4-component elements first
  //  - pack indexed tessfactors to the right
  //  - pack arbitrary elements
  //  - pack clip/cull
  //    - iterate rows and look for a viable location for each temp element
  //      When found, allocate original sub-elements associated with temp element.
  //  - next, pack system value elements
  //  - finally, pack SGV elements

  // ==========
  // Group elements
  std::vector<PackElement*>  clipcullElements,
                                      clipcullElementsByRow[2],
                                      vec4Elements,
                                      arbElements,
                                      svElements,
                                      sgvElements,
                                      indexedtessElements;

  for (auto &SE : elements) {
    // Clear any existing allocation
    if (SE->IsAllocated()) {
      SE->ClearLocation();
    }

    switch (SE->GetInterpretation()) {
      case DXIL::SemanticInterpretationKind::Arb:
        if (SE->GetCols() == 4)
          vec4Elements.push_back(SE);
        else
          arbElements.push_back(SE);
        break;
      case DXIL::SemanticInterpretationKind::ClipCull:
        clipcullElements.push_back(SE);
        break;
      case DXIL::SemanticInterpretationKind::SV:
        if (SE->GetCols() == 4)
          vec4Elements.push_back(SE);
        else
          svElements.push_back(SE);
        break;
      case DXIL::SemanticInterpretationKind::SGV:
        sgvElements.push_back(SE);
        break;
      case DXIL::SemanticInterpretationKind::TessFactor:
        if (SE->GetRows() > 1)
          indexedtessElements.push_back(SE);
        else
          svElements.push_back(SE);
        break;
      default:
        DXASSERT(false, "otherwise, unexpected interpretation for allocated element");
    }
  }

  // ==========
  // Allocate 4-component elements
  if (!vec4Elements.empty()) {
    std::sort(vec4Elements.begin(), vec4Elements.end(), CmpElementsLess);
    unsigned used = PackGreedy(vec4Elements, startRow, numRows);
    startRow += used;
    numRows -= used;
    if (rowsUsed < used)
      rowsUsed = used;
  }

  // ==========
  // Allocate indexed tessfactors in rightmost column
  if (!indexedtessElements.empty()) {
    std::sort(indexedtessElements.begin(), indexedtessElements.end(), CmpElementsLess);
    unsigned used = PackGreedy(indexedtessElements, startRow, numRows, 3);
    if (rowsUsed < used)
      rowsUsed = used;
  }

  // ==========
  // Allocate arbitrary
  if (!arbElements.empty()) {
    std::sort(arbElements.begin(), arbElements.end(), CmpElementsLess);
    unsigned used = PackGreedy(arbElements, startRow, numRows);
    if (rowsUsed < used)
      rowsUsed = used;
  }

  // ==========
  // Allocate system values
  if (!svElements.empty()) {
    std::sort(svElements.begin(), svElements.end(), CmpElementsLess);
    unsigned used = PackGreedy(svElements, startRow, numRows);
    if (rowsUsed < used)
      rowsUsed = used;
  }

  // ==========
  // Allocate clip/cull
  std::sort(clipcullElements.begin(), clipcullElements.end(), CmpElementsLess);
  unsigned numClipCullComponents = 0;
  unsigned clupCullMultiRowCols = 0;
  for (auto &SE : clipcullElements) {
    numClipCullComponents += SE->GetRows() * SE->GetCols();
    if (SE->GetRows() > 1) {
      clupCullMultiRowCols += SE->GetCols();
    }
  }
  if (0 == clupCullMultiRowCols) {
    // Preallocate clip/cull elements into two rows and allocate independently
    DxilSignatureAllocator clipcullAllocator(2, m_bUseMinPrecision);
    unsigned clipcullRegUsed = clipcullAllocator.PackGreedy(clipcullElements, 0, 2);
    unsigned clipcullComponentsByRow[2] = {0, 0};
    for (auto &SE : clipcullElements) {
      if (!SE->IsAllocated()) {
        continue;
      }
      unsigned row = SE->GetStartRow();
      DXASSERT_NOMSG(row < clipcullRegUsed);
      clipcullElementsByRow[row].push_back(SE);
      clipcullComponentsByRow[row] += SE->GetCols();
      // Deallocate element, to be allocated later:
      SE->ClearLocation();
    }

    // Allocate rows independently
    // Init temp elements, used to find compatible spaces for subsets:
    DummyElement clipcullTempElements[2];
    for (unsigned row = 0; row < clipcullRegUsed; ++row) {
      DXASSERT_NOMSG(!clipcullElementsByRow[row].empty());
      clipcullTempElements[row].kind = clipcullElementsByRow[row][0]->GetKind();
      clipcullTempElements[row].interpolation = clipcullElementsByRow[row][0]->GetInterpolationMode();
      clipcullTempElements[row].interpretation = clipcullElementsByRow[row][0]->GetInterpretation();
      clipcullTempElements[row].dataBitWidth = clipcullElementsByRow[row][0]->GetDataBitWidth();
      clipcullTempElements[row].rows = 1;
      clipcullTempElements[row].cols = clipcullComponentsByRow[row];
    }
    for (unsigned i = 0; i < clipcullRegUsed; ++i) {
      bool bAllocated = false;
      unsigned cols = clipcullComponentsByRow[i];
      for (unsigned row = startRow; row < startRow + numRows; ++row) {
        if (DetectRowConflict(&clipcullTempElements[i], row))
          continue;
        for (unsigned col = 0; col <= 4 - cols; ++col) {
          if (DetectColConflict(&clipcullTempElements[i], row, col))
            continue;
          for (auto &SE : clipcullElementsByRow[i]) {
            PlaceElement(SE, row, col);
            SE->SetLocation(row, col);
            col += SE->GetCols();
          }
          bAllocated = true;
          if (rowsUsed < row + 1)
            rowsUsed = row + 1;
          break;
        }
        if (bAllocated)
          break;
      }
    }
  } else if (numRows > 1) {
    // Multi-row clip/cull element found, test allocation at each pair of
    // rows.  If location found, allocate the elements.
    for (unsigned i = 0; i < numRows - 1; ++i) {
      unsigned row = startRow + i;
      // Use temp allocator with copy of rows to test locations
      DxilSignatureAllocator clipcullAllocator(2, m_bUseMinPrecision);
      clipcullAllocator.m_Registers[0] = m_Registers[row];
      clipcullAllocator.m_Registers[1] = m_Registers[row + 1];
      clipcullAllocator.PackGreedy(clipcullElements, 0, 2, 0);
      bool bFullyAllocated = true;
      for (auto &SE : clipcullElements) {
        bFullyAllocated &= SE->IsAllocated();
        if (!bFullyAllocated)
          break;
      }
      // Clear temp allocations
      for (auto &SE : clipcullElements)
        SE->ClearLocation();
      if (bFullyAllocated) {
        // Found a spot, do real allocation
        PackGreedy(clipcullElements, row, 2);
        bool bFullyAllocated = true;
        for (auto &SE : clipcullElements) {
          bFullyAllocated &= SE->IsAllocated();
          if (!bFullyAllocated)
            break;
        }
        DXASSERT(bFullyAllocated, "otherwise, clip/cull allocation failed when predicted to succeed.");
        break;
      }
    }
  }

  // ==========
  // Allocate system generated values
  if (!sgvElements.empty()) {
    std::sort(sgvElements.begin(), sgvElements.end(), CmpElementsLess);
    unsigned used = PackGreedy(sgvElements, startRow, numRows);
    if (rowsUsed < used)
      rowsUsed = used;
  }

  return rowsUsed;
}

unsigned DxilSignatureAllocator::PackPrefixStable(std::vector<PackElement*> elements, unsigned startRow, unsigned numRows) {
  unsigned rowsUsed = startRow;

  // Special handling for prefix-stable clip/cull arguments
  // - basically, do not pack with anything else to maximize chance to pack into two register limit
  // - this is complicated by multi-row clip/cull elements, which force allocation adjacency,
  //   but PrefixStable does not know in advance if this will be the case.
  unsigned clipcullRegUsed = 0;
  bool clipcullIndexed = false;
  DxilSignatureAllocator clipcullAllocator(2, m_bUseMinPrecision);
  DummyElement clipcullTempElements[2];

  for (auto &SE : elements) {
    // Clear any existing allocation
    if (SE->IsAllocated()) {
      SE->ClearLocation();
    }

    switch (SE->GetInterpretation()) {
      case DXIL::SemanticInterpretationKind::Arb:
      case DXIL::SemanticInterpretationKind::SGV:
        break;
      case DXIL::SemanticInterpretationKind::SV:
        break;
      case DXIL::SemanticInterpretationKind::ClipCull:
        {
          auto InitDummyElement = [](DummyElement &DE, PackElement *SE) {
            DE.kind = SE->GetKind();
            DE.interpolation = SE->GetInterpolationMode();
            DE.interpretation = SE->GetInterpretation();
            DE.dataBitWidth = SE->GetDataBitWidth();
            DE.rows = 1;
            DE.cols = 4;
          };
          // If an element is indexed, the two placeholder elements must be
          // packed concurrently, otherwise the indexing will work correctly.
          clipcullIndexed |= SE->GetRows() > 1;
          if (clipcullIndexed && clipcullRegUsed < 1) {
            // Just allocate one element with 2 rows
            //clipcullRegUsed = 2;
            auto &DE = clipcullTempElements[0];
            InitDummyElement(DE, SE);
            DE.rows = 2;
            rowsUsed = std::max(rowsUsed, PackNext(&DE, startRow, numRows));
            if (!DE.IsAllocated() ||
                0 == clipcullAllocator.PackNext(SE, 0, 2)) {  // allocate element
              // Failed to allocate 2-row dummy element or actual element.
              // Clear allocation.
              // This may repeat with additional elements.
              SE->ClearLocation();
            } else {
              // Init second dummy element to next row because it's used to
              // adjust element locations starting on that row.
              InitDummyElement(clipcullTempElements[1], SE);
              clipcullTempElements[1].row = DE.row + 1;
              clipcullRegUsed = 2;
            }
          } else if (clipcullIndexed && clipcullRegUsed < 2) {
            // Make sure additional element can be placed just after other
            // element, otherwise fail to allocate this element.
            auto &DE = clipcullTempElements[1];
            InitDummyElement(DE, SE);
            unsigned initialRow = clipcullTempElements[0].row;
            // check for conflict in next row
            ConflictType conflict = DetectColConflict(&DE, initialRow + 1, 0);
            if (conflict != kNoConflict ||
                0 == clipcullAllocator.PackNext(SE, 0, 2)) {  // allocate element
              // cannot reserve row just after first row, or failed alocation
              // Clear allocation.
              // This may repeat with additional elements.
              SE->ClearLocation();
            } else {
              DE.SetLocation(initialRow + 1, 0);
              PlaceElement(&DE, initialRow + 1, 0);
              clipcullRegUsed = 2;
              rowsUsed = std::max(rowsUsed, DE.GetStartRow() + DE.GetRows());
            }
          } else {
            unsigned used = clipcullAllocator.PackNext(SE, 0, 2); // allocate element
            if (used > clipcullRegUsed) {
              DXASSERT(!clipcullIndexed, "should have been handled elsewhere");
              // allocate placeholder element, reserving new row(s)
              auto &DE = clipcullTempElements[used - 1];
              InitDummyElement(DE, SE);
              rowsUsed = std::max(rowsUsed, PackNext(&DE, startRow, numRows));
              if (!DE.IsAllocated()) {
                // Failed to allocate new row - Clear allocation.
                // This may repeat with additional elements.
                SE->ClearLocation();
              } else {
                clipcullRegUsed = used;
              }
            }
          }
          if (SE->IsAllocated()) {
            // Actually place element in correct row:
            SE->SetLocation(clipcullTempElements[SE->GetStartRow()].GetStartRow(), SE->GetStartCol());
          }
          continue;
        }
        break;
      case DXIL::SemanticInterpretationKind::TessFactor:
        if (SE->GetRows() > 1) {
          // Maximize opportunity for packing while preserving prefix-stable property
          rowsUsed = std::max(rowsUsed, PackNext(SE, startRow, numRows, 3));
          continue;
        }
        break;
      default:
        DXASSERT(false, "otherwise, unexpected interpretation for allocated element");
    }
    rowsUsed = std::max(rowsUsed, PackNext(SE, startRow, numRows));
  }

  return rowsUsed;
}


} // namespace hlsl
