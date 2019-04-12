#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/CFG.h"
#include "llvm/Transforms/Scalar.h"
#include "dxc/DXIL/DxilModule.h"
#include "dxc/HLSL/HLModule.h"

#include <unordered_map>
#include <limits>

using namespace llvm;

namespace {

class DxilFixConstArrayInitializer : public ModulePass {
public:
  static char ID;
  DxilFixConstArrayInitializer() : ModulePass(ID) {
    initializeDxilFixConstArrayInitializerPass(*PassRegistry::getPassRegistry());
  }
  bool runOnModule(Module &M) override;
  const char *getPassName() const override { return "Dxil Fix Const Array Initializer"; }
};

char DxilFixConstArrayInitializer::ID;
}

static bool TryFixGlobalVariable(GlobalVariable &GV, BasicBlock *EntryBlock, const std::unordered_map<Instruction *, unsigned> &InstOrder) {
  // Only proceed if the variable has an undef initializer
  if (!GV.hasInitializer() || !isa<UndefValue>(GV.getInitializer()))
    return false;

  // Only handle cases when it's a scalar type or if it's an array of
  // scalars.
  Type *Ty = GV.getType()->getPointerElementType();
  if (Ty->isArrayTy() && Ty->getArrayElementType()->isArrayTy())
    return false;

  // Record the earliest load and latest store in the entry block.
  unsigned EarliestLoad = std::numeric_limits<unsigned>::max();
  unsigned LatestStore = 0;

  bool IsArray = Ty->isArrayTy();
  SmallVector<Constant *, 16> InitValue;
  SmallVector<unsigned, 16> LatestStores;

  Type *ElementTy = nullptr;
  if (IsArray) {
    InitValue.resize(Ty->getArrayNumElements());
    LatestStores.resize(Ty->getArrayNumElements());
    ElementTy = Ty->getArrayElementType();
  }
  else {
    InitValue.resize(1);
    LatestStores.resize(1);
    ElementTy = Ty;
  }

  SmallVector<StoreInst *, 8> StoresToRemove;

  for (User *U : GV.users()) {
    if (GEPOperator *GEP = dyn_cast<GEPOperator>(U)) {
      bool AllConstIndices = GEP->hasAllConstantIndices();
      unsigned NumIndices = GEP->getNumIndices();
      if (IsArray && NumIndices != 2)
        return false;
      if (!IsArray && NumIndices != 1)
        return false;

      for (User *GEPUser : GEP->users()) {
        if (StoreInst *Store = dyn_cast<StoreInst>(GEPUser)) {
          if (Store->getParent() == EntryBlock) {
            if (!isa<Constant>(Store->getValueOperand()))
              return false;

            if (!AllConstIndices)
              return false;

            unsigned StoreIndex = InstOrder.at(Store);
            if (StoreIndex > EarliestLoad)
              return false;

            LatestStore = std::max(LatestStore, StoreIndex);

            if (IsArray) {
              uint64_t Index = cast<ConstantInt>(GEP->getOperand(2))->getLimitedValue();
              if (LatestStores[Index] <= StoreIndex) {
                InitValue[Index] = cast<Constant>(Store->getValueOperand());
                LatestStores[Index] = StoreIndex;
              }
            }
            else {
              if (LatestStores[0] <= StoreIndex) {
                InitValue[0] = cast<Constant>(Store->getValueOperand());
                LatestStores[0] = StoreIndex;
              }
            }

            StoresToRemove.push_back(Store);
          }
        }
        else if (LoadInst *Load = dyn_cast<LoadInst>(GEPUser)) {
          if (Load->getParent() == EntryBlock)
            EarliestLoad = std::min(EarliestLoad, InstOrder.at(Load));

          if (EarliestLoad < LatestStore)
            return false;
        }
        // If we have some weird cases, like chained GEPS, or bitcasts, give up.
        else {
          return false;
        }
      }
    }
  }

  Constant *Initializer = nullptr;

  for (Constant *C : InitValue)
    if (!C)
      return false;

  if (IsArray) {
    Initializer = ConstantArray::get(cast<ArrayType>(Ty), InitValue);
  }
  else {
    Initializer = InitValue[0];
  }

  GV.setInitializer(Initializer);

  for (StoreInst *Store : StoresToRemove)
    Store->eraseFromParent();

  return true;
}

bool DxilFixConstArrayInitializer::runOnModule(Module &M) {
  BasicBlock *EntryBlock = nullptr;

  if (M.HasDxilModule()) {
    hlsl::DxilModule &DM = M.GetDxilModule();
    if (DM.GetEntryFunction()) {
      EntryBlock = &DM.GetEntryFunction()->getEntryBlock();
    }
  }
  else if (M.HasHLModule()) {
    hlsl::HLModule &HM = M.GetHLModule();
    if (HM.GetEntryFunction())
      EntryBlock = &HM.GetEntryFunction()->getEntryBlock();
  }

  if (!EntryBlock)
    return false;

  // If some block might branch to the entry for some reason (like if it's a loop header),
  // give up now. Have to make sure this block is not preceeded by anything.
  if (pred_begin(EntryBlock) != pred_end(EntryBlock))
    return false;

  // Find the instruction order for everything in the entry block.
  std::unordered_map<Instruction *, unsigned> InstOrder;
  for (Instruction &I : *EntryBlock) {
    InstOrder[&I] = InstOrder.size();
  }

  bool Changed = false;
  for (GlobalVariable &GV : M.globals()) {
    Changed = TryFixGlobalVariable(GV, EntryBlock, InstOrder);
  }

  return Changed;
}


Pass *llvm::createDxilFixConstArrayInitializerPass() {
  return new DxilFixConstArrayInitializer();
}

INITIALIZE_PASS(DxilFixConstArrayInitializer, "dxil-fix-array-init", "Dxil Fix Array Initializer", false, false)