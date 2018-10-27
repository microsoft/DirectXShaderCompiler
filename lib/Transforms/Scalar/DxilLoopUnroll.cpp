

#include "llvm/Pass.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Transforms/Scalar.h"

using namespace llvm;
//using namespace hlsl;

namespace {

class DxilLoopUnroll : public LoopPass {
public:
  static char ID;
  DxilLoopUnroll() : LoopPass(ID) {}
  bool runOnLoop(Loop *L, LPPassManager &LPM) override;
};

char DxilLoopUnroll::ID;

bool DxilLoopUnroll::runOnLoop(Loop *L, LPPassManager &LPM) {
  SmallVector<BasicBlock *, 16> ExitBlocks;
  L->getExitBlocks(ExitBlocks);
  size_t size = ExitBlocks.size();
  printf("%u\n", (unsigned)size);
  return false;
}

}

Pass *llvm::createDxilLoopUnrollPass() {
  return new DxilLoopUnroll();
}

INITIALIZE_PASS(DxilLoopUnroll, "dxil-loop-unroll", "Dxil Unroll loops", false, false)
