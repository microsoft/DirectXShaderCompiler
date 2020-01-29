//===--------- DxilValueCache.cpp - Dxil Constant Value Cache ------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/Pass.h"
#include "llvm/Transforms/Utils/ValueMapper.h"

namespace llvm {

class Module;
class DominatorTree;

struct DxilValueCache : public ImmutablePass {
  static char ID;

  // Special Weak Value to Weak Value map.
  struct WeakValueMap {
    struct ValueVH : public CallbackVH {
      ValueVH(Value *V) : CallbackVH(V) {}
      void allUsesReplacedWith(Value *) override { setValPtr(nullptr); }
    };
    struct ValueEntry {
      WeakVH Value;
      ValueVH Self;
      ValueEntry() : Value(nullptr), Self(nullptr) {}
      inline void Set(llvm::Value *Key, llvm::Value *V) { Self = Key; Value = V; }
      inline bool IsStale() const { return Self == nullptr; }
    };
    ValueMap<const Value *, ValueEntry> Map;
    Value *Get(Value *V);
    void Set(Value *Key, Value *V);
    bool Seen(Value *v);
    void SetSentinel(Value *V);
    void ResetUnknowns();
    void dump() const;
  private:
    Value *GetSentinel(LLVMContext &Ctx);
    std::unique_ptr<Value> Sentinel;
  };

private:

  WeakValueMap ValueMap;

  void MarkAlwaysReachable(BasicBlock *BB);
  void MarkUnreachable(BasicBlock *BB);
  bool IsAlwaysReachable_(BasicBlock *BB);
  bool IsUnreachable_(BasicBlock *BB);
  bool MayBranchTo(BasicBlock *A, BasicBlock *B);
  Value *TryGetCachedValue(Value *V);
  Value *ProcessValue(Value *V, DominatorTree *DT);

  Value *ProcessAndSimplify_PHI(Instruction *I, DominatorTree *DT);
  Value *ProcessAndSimpilfy_Br(Instruction *I, DominatorTree *DT);
  Value *ProcessAndSimpilfy_Load(Instruction *LI, DominatorTree *DT);
  Value *SimplifyAndCacheResult(Instruction *I, DominatorTree *DT);

public:

  const char *getPassName() const override;
  DxilValueCache();
  void getAnalysisUsage(AnalysisUsage &) const;

  void dump() const;
  Value *GetValue(Value *V, DominatorTree *DT=nullptr);
  void ResetUnknowns() { ValueMap.ResetUnknowns(); }
  bool IsAlwaysReachable(BasicBlock *BB, DominatorTree *DT=nullptr);
  bool IsUnreachable(BasicBlock *BB, DominatorTree *DT=nullptr);
};

void initializeDxilValueCachePass(class llvm::PassRegistry &);
Pass *createDxilValueCachePass();

}


