//===- DxilRemoveUnstructuredLoopExits.h - Make unrolled loops structured ---===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

namespace llvm {
  class Loop;
  class LoopInfo;
  class DominatorTree;
}

namespace hlsl {
  bool RemoveUnstructuredLoopExits(llvm::Loop *L, llvm::LoopInfo *LI, llvm::DominatorTree *DT);
}

