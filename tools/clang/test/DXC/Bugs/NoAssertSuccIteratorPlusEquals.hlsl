// RUN: %dxc -T cs_6_6 %s | FileCheck %s

// This was tripping a false-positive assertion:
// dxc: include/llvm/IR/CFG.h:216: Self &llvm::SuccIterator<llvm::TerminatorInst *, llvm::BasicBlock>::operator+=(int) [Term_ = llvm::TerminatorInst *, BB_ = llvm::BasicBlock]: Assertion `index_is_valid(new_idx) && "Iterator index out of bound"' failed.

// CHECK: @main

[numthreads(1, 1, 1)]
void main() {
  int i = 0;
  while (true) {
    for (i = 0; i < 2; i++) {
      while (true) {
        int unused = 0;
        while (true) {
          if (i < 2) {
            return;
          } else {
            break;
          }
        }
        if (i < 2) { break; }
      }
    }
  }
}
