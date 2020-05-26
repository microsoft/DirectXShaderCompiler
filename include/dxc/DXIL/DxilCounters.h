///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilCounters.h                                                            //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Counters for Dxil instructions types.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <stdint.h>

namespace llvm {
  class Module;
  class StringRef;
}

namespace hlsl {

struct DxilCounters {
  uint32_t InstructionCount = 0;
  uint32_t FloatInstructionCount = 0;
  uint32_t IntInstructionCount = 0;
  uint32_t UintInstructionCount = 0;
  uint32_t BranchCount = 0;
  uint32_t GlobalStaticArrayBytes = 0;
  uint32_t GlobalTGSMArrayBytes = 0;
  uint32_t LocalArrayBytes = 0;
  uint32_t GlobalStaticArrayAccesses = 0;
  uint32_t GlobalTGSMArrayAccesses = 0;
  uint32_t LocalArrayAccesses = 0;
  uint32_t TextureSampleCount = 0;
  uint32_t ResourceLoadCount = 0;
  uint32_t ResourceStoreCount = 0;
  uint32_t TextureCmpCount = 0;
  uint32_t TextureBiasCount = 0;
  uint32_t TextureGradCount = 0;
  uint32_t GSEmitCount = 0;
  uint32_t GSCutCount = 0;
  uint32_t AtomicCount = 0;
  uint32_t BarrierCount = 0;

  uint32_t AllArrayBytes() {
    return GlobalStaticArrayBytes
      + GlobalTGSMArrayBytes
      + LocalArrayBytes;
  }
  uint32_t AllArrayAccesses() {
    return GlobalStaticArrayAccesses
      + GlobalTGSMArrayAccesses
      + LocalArrayAccesses;
  }
};

void CountInstructions(llvm::Module &M, DxilCounters& counters);
uint32_t *LookupByName(llvm::StringRef name, DxilCounters& counters);

} // namespace hlsl
