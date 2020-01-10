//===--------- DxilNoops.cpp - Dxil Noop Instructions ---------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

namespace llvm {
  class CallInst;
}

namespace hlsl {
  bool IsDxilCopy(llvm::CallInst *CI);
}


