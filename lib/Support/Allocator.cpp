//===--- Allocator.cpp - Simple memory allocation abstraction -------------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// Allocator.cpp                                                             //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file implements the BumpPtrAllocator interface.                      //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/Support/Allocator.h"
#include "llvm/Support/raw_ostream.h"

namespace llvm {

namespace detail {

void printBumpPtrAllocatorStats(unsigned NumSlabs, size_t BytesAllocated,
                                size_t TotalMemory) {
  errs() << "\nNumber of memory regions: " << NumSlabs << '\n'
         << "Bytes used: " << BytesAllocated << '\n'
         << "Bytes allocated: " << TotalMemory << '\n'
         << "Bytes wasted: " << (TotalMemory - BytesAllocated)
         << " (includes alignment, etc)\n";
}

} // End namespace detail.

void PrintRecyclerStats(size_t Size,
                        size_t Align,
                        size_t FreeListSize) {
  errs() << "Recycler element size: " << Size << '\n'
         << "Recycler element alignment: " << Align << '\n'
         << "Number of elements free for recycling: " << FreeListSize << '\n';
}

}
