
#include "llvm/Pass.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/Transforms/Utils/UnrollLoop.h"
#include "llvm/Support/Debug.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/PredIteratorCache.h"
#include "llvm/Analysis/InstructionSimplify.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/SSAUpdater.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/ADT/SetVector.h"

#include "dxc/DXIL/DxilUtil.h"
#include "dxc/Support/Global.h"
#include "dxc/DXIL/DxilOperations.h"
#include "dxc/DXIL/DxilModule.h"
#include "dxc/HLSL/HLModule.h"
#include "dxc/DXIL/DxilInstructions.h"

#include <set>
#include <unordered_map>

using namespace llvm;
using namespace hlsl;

namespace {

typedef std::unordered_map<Value*, Value*> ValueToValueMap;
typedef llvm::SetVector<Value*> ValueSetVector;
typedef llvm::SmallVector<Value*, 4> IndexVector;
typedef std::unordered_map<Value*, IndexVector> ValueToIdxMap;

// Errors:
class ResourceUseErrors
{
  bool m_bErrorsReported;
public:
  ResourceUseErrors() : m_bErrorsReported(false) {}

  enum ErrorCode {
    // Collision between use of one resource GV and another.
    // All uses must be guaranteed to resolve to only one GV.
    // Additionally, when writing resource to alloca, all uses
    // of that alloca are considered resolving to a single GV.
    GVConflicts,

    // static global resources are disallowed for libraries at this time.
    // for non-library targets, they should have been eliminated already.
    StaticGVUsed,

    // user function calls with resource params or return type are
    // are currently disallowed for libraries.
    UserCallsWithResources,

    // When searching up from store pointer looking for alloca,
    // we encountered an unexpted value type
    UnexpectedValuesFromStorePointer,

    // When remapping values to be replaced, we add them to RemappedValues
    // so we don't use dead values stored in other sets/maps.  Circular
    // remaps that should not happen are aadded to RemappingCyclesDetected.
    RemappingCyclesDetected,

    // Without SUPPORT_SELECT_ON_ALLOCA, phi/select on alloca based
    // pointer is disallowed, since this scenario is still untested.
    // This error also covers any other unknown alloca pointer uses.
    // Supported:
    // alloca (-> gep)? -> load -> ...
    // alloca (-> gep)? -> store.
    // Unsupported without SUPPORT_SELECT_ON_ALLOCA:
    // alloca (-> gep)? -> phi/select -> ...
    AllocaUserDisallowed,

#ifdef SUPPORT_SELECT_ON_ALLOCA
    // Conflict in select/phi between GV pointer and alloca pointer.  This
    // algorithm can't handle this case.
    AllocaSelectConflict,
#endif

    ErrorCodeCount
  };

  const StringRef ErrorText[ErrorCodeCount] = {
    "local resource not guaranteed to map to unique global resource.",
    "static global resource use is disallowed for library functions.",
    "exported library functions cannot have resource parameters or return value.",
    "internal error: unexpected instruction type when looking for alloca from store.",
    "internal error: cycles detected in value remapping.",
    "phi/select disallowed on pointers to local resources."
#ifdef SUPPORT_SELECT_ON_ALLOCA
    ,"unable to resolve merge of global and local resource pointers."
#endif
  };

  ValueSetVector ErrorSets[ErrorCodeCount];

  // Ulitimately, the goal of ErrorUsers is to mark all create handles
  // so we don't try to report errors on them again later.
  std::unordered_set<Value*> ErrorUsers;  // users of error values
  bool AddErrorUsers(Value* V) {
    auto it = ErrorUsers.insert(V);
    if (!it.second)
      return false;   // already there
    if (isa<GEPOperator>(V) ||
        isa<LoadInst>(V) ||
        isa<PHINode>(V) ||
        isa<SelectInst>(V) ||
        isa<AllocaInst>(V)) {
      for (auto U : V->users()) {
        AddErrorUsers(U);
      }
    } else if(isa<StoreInst>(V)) {
      AddErrorUsers(cast<StoreInst>(V)->getPointerOperand());
    }
    // create handle will be marked, but users not followed
    return true;
  }
  void ReportError(ErrorCode ec, Value* V) {
    DXASSERT_NOMSG(ec < ErrorCodeCount);
    if (!ErrorSets[ec].insert(V))
      return;   // Error already reported
    AddErrorUsers(V);
    m_bErrorsReported = true;
    if (Instruction *I = dyn_cast<Instruction>(V)) {
      dxilutil::EmitErrorOnInstruction(I, ErrorText[ec]);
    } else {
      StringRef Name = V->getName();
      std::string escName;
      if (isa<Function>(V)) {
        llvm::raw_string_ostream os(escName);
        dxilutil::PrintEscapedString(Name, os);
        os.flush();
        Name = escName;
      }
      Twine msg = Twine(ErrorText[ec]) + " Value: " + Name;
      V->getContext().emitError(msg);
    }
  }

  bool ErrorsReported() {
    return m_bErrorsReported;
  }
};

unsigned CountArrayDimensions(Type* Ty,
    // Optionally collect dimensions
    SmallVector<unsigned, 4> *dims = nullptr) {
  if (Ty->isPointerTy())
    Ty = Ty->getPointerElementType();
  unsigned dim = 0;
  if (dims)
    dims->clear();
  while (Ty->isArrayTy()) {
    if (dims)
      dims->push_back(Ty->getArrayNumElements());
    dim++;
    Ty = Ty->getArrayElementType();
  }
  return dim;
}

// Helper class for legalizing resource use
// Convert select/phi on resources to select/phi on index to GEP on GV.
// Convert resource alloca to index alloca.
// Assumes createHandleForLib has no select/phi
class LegalizeResourceUseHelper {
  // Change:
  //  gep1 = GEP gRes, i1
  //  res1 = load gep1
  //  gep2 = GEP gRes, i2
  //  gep3 = GEP gRes, i3
  //  gep4 = phi gep2, gep3           <-- handle select/phi on GEP
  //  res4 = load gep4
  //  res5 = phi res1, res4
  //  res6 = load GEP gRes, 23        <-- handle constant GepExpression
  //  res = select cnd2, res5, res6
  //  handle = createHandleForLib(res)
  // To:
  //  i4 = phi i2, i3
  //  i5 = phi i1, i4
  //  i6 = select cnd, i5, 23
  //  gep = GEP gRes, i6
  //  res = load gep
  //  handle = createHandleForLib(res)

  // Also handles alloca
  //  resArray = alloca [2 x Resource]
  //  gep1 = GEP gRes, i1
  //  res1 = load gep1
  //  gep2 = GEP gRes, i2
  //  gep3 = GEP gRes, i3
  //  phi4 = phi gep2, gep3
  //  res4 = load phi4
  //  gep5 = GEP resArray, 0
  //  gep6 = GEP resArray, 1
  //  store gep5, res1
  //  store gep6, res4
  //  gep7 = GEP resArray, i7   <-- dynamically index array
  //  res = load gep7
  //  handle = createHandleForLib(res)
  // Desired result:
  //  idxArray = alloca [2 x i32]
  //  phi4 = phi i2, i3
  //  gep5 = GEP idxArray, 0
  //  gep6 = GEP idxArray, 1
  //  store gep5, i1
  //  store gep6, phi4
  //  gep7 = GEP idxArray, i7
  //  gep8 = GEP gRes, gep7
  //  res = load gep8
  //  handle = createHandleForLib(res)

  // Also handles multi-dim resource index and multi-dim resource array allocas

  // Basic algorithm:
  // - recursively mark each GV user with GV (ValueToResourceGV)
  //  - verify only one GV used for any given value
  // - handle allocas by searching up from store for alloca
  //  - then recursively mark alloca users
  // - ResToIdxReplacement keeps track of vector of indices that
  //   will be used to replace a given resource value or pointer
  // - Next, create selects/phis for indices corresponding to
  //   selects/phis on resource pointers or values.
  //  - leave incoming index values undef for now
  // - Create index allocas to replace resource allocas
  // - Create GEPs on index allocas to replace GEPs on resource allocas
  // - Create index loads on index allocas to replace loads on resource alloca GEP
  // - Fill in replacements for GEPs on resource GVs
  //  - copy replacement index vectors to corresponding loads
  // - Create index stores to replace resource stores to alloca/GEPs
  // - Update selects/phis incoming index values
  // - SimplifyMerges: replace index phis/selects on same value with that value
  //  - RemappedValues[phi/select] set to replacement value
  //  - use LookupValue from now on when reading from ResToIdxReplacement
  // - Update handles by replacing load/GEP chains that go through select/phi
  //   with direct GV GEP + load, with select/phi on GEP indices instead.

public:
  ResourceUseErrors m_Errors;

  ValueToValueMap ValueToResourceGV;
  ValueToIdxMap ResToIdxReplacement;
  // Value sets we can use to iterate
  ValueSetVector Selects, GEPs, Stores, Handles;
  ValueSetVector Allocas, AllocaGEPs, AllocaLoads;
#ifdef SUPPORT_SELECT_ON_ALLOCA
  ValueSetVector AllocaSelects;
#endif

  std::unordered_set<Value *> NonUniformSet;

  // New index selects created by pass, so we can try simplifying later
  ValueSetVector NewSelects;

  // Values that have been replaced with other values need remapping
  ValueToValueMap RemappedValues;

  // Things to clean up if no users:
  std::unordered_set<Instruction*> CleanupInsts;

  GlobalVariable *LookupResourceGV(Value *V) {
    auto itGV = ValueToResourceGV.find(V);
    if (itGV == ValueToResourceGV.end())
      return nullptr;
    return cast<GlobalVariable>(itGV->second);
  }

  // Follow RemappedValues, return input if not remapped
  Value *LookupValue(Value *V) {
    auto it = RemappedValues.find(V);
    SmallPtrSet<Value*, 4> visited;
    while (it != RemappedValues.end()) {
      // Cycles should not happen, but are bad if they do.
      if (visited.count(it->second)) {
        DXASSERT(false, "otherwise, circular remapping");
        m_Errors.ReportError(ResourceUseErrors::RemappingCyclesDetected, V);
        break;
      }
      V = it->second;
      it = RemappedValues.find(V);
      if (it != RemappedValues.end())
        visited.insert(V);
    }
    return V;
  }

  bool AreLoadUsersTrivial(LoadInst *LI) {
    for (auto U : LI->users()) {
      if (CallInst *CI = dyn_cast<CallInst>(U)) {
        Function *F = CI->getCalledFunction();
        DxilModule &DM = F->getParent()->GetDxilModule();
        hlsl::OP *hlslOP = DM.GetOP();
        if (hlslOP->IsDxilOpFunc(F)) {
          hlsl::OP::OpCodeClass opClass;
          if (hlslOP->GetOpCodeClass(F, opClass) &&
            opClass == DXIL::OpCodeClass::CreateHandleForLib) {
            continue;
          }
        }
      }
      return false;
    }
    return true;
  }

  // This is used to quickly skip the common case where no work is needed
  bool AreGEPUsersTrivial(GEPOperator *GEP) {
    if (GlobalVariable *GV = LookupResourceGV(GEP)) {
      if (GEP->getPointerOperand() != LookupResourceGV(GEP))
        return false;
    }
    for (auto U : GEP->users()) {
      if (LoadInst *LI = dyn_cast<LoadInst>(U)) {
        if (AreLoadUsersTrivial(LI))
          continue;
      }
      return false;
    }
    return true;
  }

  // AssignResourceGVFromStore is used on pointer being stored to.
  // Follow GEP/Phi/Select up to Alloca, then CollectResourceGVUsers on Alloca
  void AssignResourceGVFromStore(GlobalVariable *GV, Value *V,
                                 SmallPtrSet<Value*, 4> &visited,
                                 bool bNonUniform) {
    // Prevent cycles as we search up
    if (visited.count(V) != 0)
      return;
    // Verify and skip if already processed
    auto it = ValueToResourceGV.find(V);
    if (it != ValueToResourceGV.end()) {
      if (it->second != GV) {
        m_Errors.ReportError(ResourceUseErrors::GVConflicts, V);
      }
      return;
    }
    if (AllocaInst *AI = dyn_cast<AllocaInst>(V)) {
      CollectResourceGVUsers(GV, AI, /*bAlloca*/true, bNonUniform);
      return;
    } else if (GEPOperator *GEP = dyn_cast<GEPOperator>(V)) {
      // follow the pointer up
      AssignResourceGVFromStore(GV, GEP->getPointerOperand(), visited, bNonUniform);
      return;
    } else if (PHINode *Phi = dyn_cast<PHINode>(V)) {
#ifdef SUPPORT_SELECT_ON_ALLOCA
      // follow all incoming values
      for (auto it : Phi->operand_values())
        AssignResourceGVFromStore(GV, it, visited, bNonUniform);
#else
      m_Errors.ReportError(ResourceUseErrors::AllocaUserDisallowed, V);
#endif
      return;
    } else if (SelectInst *Sel = dyn_cast<SelectInst>(V)) {
#ifdef SUPPORT_SELECT_ON_ALLOCA
      // follow all incoming values
      AssignResourceGVFromStore(GV, Sel->getTrueValue(), visited, bNonUniform);
      AssignResourceGVFromStore(GV, Sel->getFalseValue(), visited, bNonUniform);
#else
      m_Errors.ReportError(ResourceUseErrors::AllocaUserDisallowed, V);
#endif
      return;
    } else if (isa<GlobalVariable>(V) &&
               cast<GlobalVariable>(V)->getLinkage() ==
                    GlobalVariable::LinkageTypes::InternalLinkage) {
      // this is writing to global static, which is disallowed at this point.
      m_Errors.ReportError(ResourceUseErrors::StaticGVUsed, V);
      return;
    } else {
      // Most likely storing to output parameter
      m_Errors.ReportError(ResourceUseErrors::UserCallsWithResources, V);
      return;
    }
    return;
  }

  // Recursively mark values with GV, following users.
  // Starting value V should be GV itself.
  // Returns true if value/uses reference no other GV in map.
  void CollectResourceGVUsers(GlobalVariable *GV, Value *V, bool bAlloca = false, bool bNonUniform = false) {
    // Recursively tag value V and its users as using GV.
    auto it = ValueToResourceGV.find(V);
    if (it != ValueToResourceGV.end()) {
      if (it->second != GV) {
        m_Errors.ReportError(ResourceUseErrors::GVConflicts, V);
#ifdef SUPPORT_SELECT_ON_ALLOCA
      } else {
        // if select/phi, make sure bAlloca is consistent
        if (isa<PHINode>(V) || isa<SelectInst>(V))
          if ((bAlloca && AllocaSelects.count(V) == 0) ||
              (!bAlloca && Selects.count(V) == 0))
            m_Errors.ReportError(ResourceUseErrors::AllocaSelectConflict, V);
#endif
      }
      return;
    }
    ValueToResourceGV[V] = GV;
    if (GV == V) {
      // Just add and recurse users
      // make sure bAlloca is clear for users
      bAlloca = false;
    } else if (GEPOperator *GEP = dyn_cast<GEPOperator>(V)) {
      if (bAlloca)
        AllocaGEPs.insert(GEP);
      else if (!AreGEPUsersTrivial(GEP))
        GEPs.insert(GEP);
      else
        return; // Optimization: skip trivial GV->GEP->load->createHandle
      if (GetElementPtrInst *GEPInst = dyn_cast<GetElementPtrInst>(GEP)) {
        if (DxilMDHelper::IsMarkedNonUniform(GEPInst))
          bNonUniform = true;
      }
    } else if (LoadInst *LI = dyn_cast<LoadInst>(V)) {
      if (bAlloca)
        AllocaLoads.insert(LI);
      // clear bAlloca for users
      bAlloca = false;
      if (bNonUniform)
        NonUniformSet.insert(LI);
    } else if (StoreInst *SI = dyn_cast<StoreInst>(V)) {
      Stores.insert(SI);
      if (!bAlloca) {
        // Find and mark allocas this store could be storing to
        SmallPtrSet<Value*, 4> visited;
        AssignResourceGVFromStore(GV, SI->getPointerOperand(), visited, bNonUniform);
      }
      return;
    } else if (PHINode *Phi = dyn_cast<PHINode>(V)) {
      if (bAlloca) {
#ifdef SUPPORT_SELECT_ON_ALLOCA
        AllocaSelects.insert(Phi);
#else
        m_Errors.ReportError(ResourceUseErrors::AllocaUserDisallowed, V);
#endif
      } else {
        Selects.insert(Phi);
      }
    } else if (SelectInst *Sel = dyn_cast<SelectInst>(V)) {
      if (bAlloca) {
#ifdef SUPPORT_SELECT_ON_ALLOCA
        AllocaSelects.insert(Sel);
#else
        m_Errors.ReportError(ResourceUseErrors::AllocaUserDisallowed, V);
#endif
      } else {
        Selects.insert(Sel);
      }
    } else if (AllocaInst *AI = dyn_cast<AllocaInst>(V)) {
      Allocas.insert(AI);
      // set bAlloca for users
      bAlloca = true;
    } else if (Constant *C = dyn_cast<Constant>(V)) {
      // skip @llvm.used entry
      return;
    } else if (bAlloca) {
      m_Errors.ReportError(ResourceUseErrors::AllocaUserDisallowed, V);
    } else {
      // Must be createHandleForLib or user function call.
      CallInst *CI = cast<CallInst>(V);
      Function *F = CI->getCalledFunction();
      hlsl::OP *hlslOP = nullptr;
      if (GV->getParent()->HasDxilModule()) {
        DxilModule &DM = GV->getParent()->GetDxilModule();
        hlslOP = DM.GetOP();
      }
      else {
        HLModule &HM = GV->getParent()->GetHLModule();
        hlslOP = HM.GetOP();
      }
      if (hlslOP->IsDxilOpFunc(F)) {
        hlsl::OP::OpCodeClass opClass;
        if (hlslOP->GetOpCodeClass(F, opClass) &&
            opClass == DXIL::OpCodeClass::CreateHandleForLib) {
          Handles.insert(CI);
          if (bNonUniform)
            NonUniformSet.insert(CI);
          return;
        }
      }
      // This could be user call with resource param, which is disallowed for lib_6_3
      m_Errors.ReportError(ResourceUseErrors::UserCallsWithResources, V);
      return;
    }

    // Recurse users
    for (auto U : V->users())
      CollectResourceGVUsers(GV, U, bAlloca, bNonUniform);
    return;
  }

  // Remove conflicting values from sets before
  // transforming the remainder.
  void RemoveConflictingValue(Value* V) {
    bool bRemoved = false;
    if (isa<GEPOperator>(V)) {
      bRemoved = GEPs.remove(V) || AllocaGEPs.remove(V);
    } else if (isa<LoadInst>(V)) {
      bRemoved = AllocaLoads.remove(V);
    } else if (isa<StoreInst>(V)) {
      bRemoved = Stores.remove(V);
    } else if (isa<PHINode>(V) || isa<SelectInst>(V)) {
      bRemoved = Selects.remove(V);
#ifdef SUPPORT_SELECT_ON_ALLOCA
      bRemoved |= AllocaSelects.remove(V);
#endif
    } else if (isa<AllocaInst>(V)) {
      bRemoved = Allocas.remove(V);
    } else if (isa<CallInst>(V)) {
      bRemoved = Handles.remove(V);
      return; // don't recurse
    }
    if (bRemoved) {
      // Recurse users
      for (auto U : V->users())
        RemoveConflictingValue(U);
    }
  }
  void RemoveConflicts() {
    for (auto V : m_Errors.ErrorSets[ResourceUseErrors::GVConflicts]) {
      RemoveConflictingValue(V);
      ValueToResourceGV.erase(V);
    }
  }

  void CreateSelects() {
    if (Selects.empty()
#ifdef SUPPORT_SELECT_ON_ALLOCA
        && AllocaSelects.empty()
#endif
        )
      return;
    LLVMContext &Ctx =
#ifdef SUPPORT_SELECT_ON_ALLOCA
      Selects.empty() ? AllocaSelects[0]->getContext() :
#endif
      Selects[0]->getContext();
    Type *i32Ty = IntegerType::getInt32Ty(Ctx);
#ifdef SUPPORT_SELECT_ON_ALLOCA
    for (auto &SelectSet : {Selects, AllocaSelects}) {
      bool bAlloca = !(&SelectSet == &Selects);
#else
    for (auto &SelectSet : { Selects }) {
#endif
      for (auto pValue : SelectSet) {
        Type *SelectTy = i32Ty;
#ifdef SUPPORT_SELECT_ON_ALLOCA
        // For alloca case, type needs to match dimensionality of incoming value
        if (bAlloca) {
          // TODO: Not sure if this case will actually work
          //      (or whether it can even be generated from HLSL)
          Type *Ty = pValue->getType();
          SmallVector<unsigned, 4> dims;
          unsigned dim = CountArrayDimensions(Ty, &dims);
          for (unsigned i = 0; i < dim; i++)
            SelectTy = ArrayType::get(SelectTy, (uint64_t)dims[dim - i - 1]);
          if (Ty->isPointerTy())
            SelectTy = PointerType::get(SelectTy, 0);
        }
#endif
        Value *UndefValue = UndefValue::get(SelectTy);
        if (PHINode *Phi = dyn_cast<PHINode>(pValue)) {
          GlobalVariable *GV = LookupResourceGV(Phi);
          if (!GV)
            continue; // skip value removed due to conflict
          IRBuilder<> PhiBuilder(Phi);
          unsigned gvDim = CountArrayDimensions(GV->getType());
          IndexVector &idxVector = ResToIdxReplacement[Phi];
          idxVector.resize(gvDim, nullptr);
          unsigned numIncoming = Phi->getNumIncomingValues();
          for (unsigned i = 0; i < gvDim; i++) {
            PHINode *newPhi = PhiBuilder.CreatePHI(SelectTy, numIncoming);
            NewSelects.insert(newPhi);
            idxVector[i] = newPhi;
            for (unsigned j = 0; j < numIncoming; j++) {
              // Set incoming values to undef until next pass
              newPhi->addIncoming(UndefValue, Phi->getIncomingBlock(j));
            }
          }
        } else if (SelectInst *Sel = dyn_cast<SelectInst>(pValue)) {
          GlobalVariable *GV = LookupResourceGV(Sel);
          if (!GV)
            continue; // skip value removed due to conflict
          IRBuilder<> Builder(Sel);
          unsigned gvDim = CountArrayDimensions(GV->getType());
          IndexVector &idxVector = ResToIdxReplacement[Sel];
          idxVector.resize(gvDim, nullptr);
          for (unsigned i = 0; i < gvDim; i++) {
            Value *newSel = Builder.CreateSelect(Sel->getCondition(), UndefValue, UndefValue);
            NewSelects.insert(newSel);
            idxVector[i] = newSel;
          }
        } else {
          DXASSERT(false, "otherwise, non-select/phi in Selects set");
        }
      }
    }
  }

  // Create index allocas to replace resource allocas
  void CreateIndexAllocas() {
    if (Allocas.empty())
      return;
    Type *i32Ty = IntegerType::getInt32Ty(Allocas[0]->getContext());
    for (auto pValue : Allocas) {
      AllocaInst *pAlloca = cast<AllocaInst>(pValue);
      GlobalVariable *GV = LookupResourceGV(pAlloca);
      if (!GV)
        continue; // skip value removed due to conflict
      IRBuilder<> AllocaBuilder(pAlloca);
      unsigned gvDim = CountArrayDimensions(GV->getType());
      SmallVector<unsigned, 4> dimVector;
      unsigned allocaTyDim = CountArrayDimensions(pAlloca->getType(), &dimVector);
      Type *pIndexType = i32Ty;
      for (unsigned i = 0; i < allocaTyDim; i++) {
        pIndexType = ArrayType::get(pIndexType, dimVector[allocaTyDim - i - 1]);
      }
      Value *arraySize = pAlloca->getArraySize();
      IndexVector &idxVector = ResToIdxReplacement[pAlloca];
      idxVector.resize(gvDim, nullptr);
      for (unsigned i = 0; i < gvDim; i++) {
        AllocaInst *pAlloca = AllocaBuilder.CreateAlloca(pIndexType, arraySize);
        pAlloca->setAlignment(4);
        idxVector[i] = pAlloca;
      }
    }
  }

  // Add corresponding GEPs for index allocas
  IndexVector &ReplaceAllocaGEP(GetElementPtrInst *GEP) {
    IndexVector &idxVector = ResToIdxReplacement[GEP];
    if (!idxVector.empty())
      return idxVector;

    Value *Ptr = GEP->getPointerOperand();

    // Recurse for partial GEPs
    IndexVector &ptrIndices = isa<GetElementPtrInst>(Ptr) ?
      ReplaceAllocaGEP(cast<GetElementPtrInst>(Ptr)) : ResToIdxReplacement[Ptr];

    IRBuilder<> Builder(GEP);
    SmallVector<Value*, 4> gepIndices;
    for (auto it = GEP->idx_begin(), idxEnd = GEP->idx_end(); it != idxEnd; it++)
      gepIndices.push_back(*it);
    idxVector.resize(ptrIndices.size(), nullptr);
    for (unsigned i = 0; i < ptrIndices.size(); i++) {
      idxVector[i] = Builder.CreateInBoundsGEP(ptrIndices[i], gepIndices);
    }
    return idxVector;
  }

  void ReplaceAllocaGEPs() {
    for (auto V : AllocaGEPs) {
      ReplaceAllocaGEP(cast<GetElementPtrInst>(V));
    }
  }

  void ReplaceAllocaLoads() {
    for (auto V : AllocaLoads) {
      LoadInst *LI = cast<LoadInst>(V);
      Value *Ptr = LI->getPointerOperand();
      IRBuilder<> Builder(LI);
      IndexVector &idxVector = ResToIdxReplacement[V];
      IndexVector &ptrIndices = ResToIdxReplacement[Ptr];
      idxVector.resize(ptrIndices.size(), nullptr);
      for (unsigned i = 0; i < ptrIndices.size(); i++) {
        idxVector[i] = Builder.CreateLoad(ptrIndices[i]);
      }
    }
  }

  // Add GEP to ResToIdxReplacement with indices from incoming + GEP
  IndexVector &ReplaceGVGEPs(GEPOperator *GEP) {
    IndexVector &idxVector = ResToIdxReplacement[GEP];
    // Skip if already done
    // (we recurse into partial GEP and iterate all GEPs)
    if (!idxVector.empty())
      return idxVector;

    Type *i32Ty = IntegerType::getInt32Ty(GEP->getContext());
    Constant *Zero = Constant::getIntegerValue(i32Ty, APInt(32, 0));

    Value *Ptr = GEP->getPointerOperand();

    unsigned idx = 0;
    if (GlobalVariable *GV = dyn_cast<GlobalVariable>(Ptr)) {
      unsigned gvDim = CountArrayDimensions(GV->getType());
      idxVector.resize(gvDim, Zero);
    } else if (isa<GEPOperator>(Ptr) || isa<PHINode>(Ptr) || isa<SelectInst>(Ptr)) {
      // Recurse for partial GEPs
      IndexVector &ptrIndices = isa<GEPOperator>(Ptr) ?
        ReplaceGVGEPs(cast<GEPOperator>(Ptr)) : ResToIdxReplacement[Ptr];
      unsigned ptrDim = CountArrayDimensions(Ptr->getType());
      unsigned gvDim = ptrIndices.size();
      DXASSERT(ptrDim <= gvDim, "otherwise incoming pointer has more dimensions than associated GV");
      unsigned gepStart = gvDim - ptrDim;
      // Copy indices and add ours
      idxVector.resize(ptrIndices.size(), Zero);
      for (; idx < gepStart; idx++)
        idxVector[idx] = ptrIndices[idx];
    }
    if (GEP->hasIndices()) {
      auto itIdx = GEP->idx_begin();
      ++itIdx;  // Always skip leading zero (we don't support GV+n pointer arith)
      while (itIdx != GEP->idx_end())
        idxVector[idx++] = *itIdx++;
    }
    return idxVector;
  }

  // Add GEPs to ResToIdxReplacement and update loads
  void ReplaceGVGEPs() {
    if (GEPs.empty())
      return;
    for (auto V : GEPs) {
      GEPOperator *GEP = cast<GEPOperator>(V);
      IndexVector &gepVector = ReplaceGVGEPs(GEP);
      for (auto U : GEP->users()) {
        if (LoadInst *LI = dyn_cast<LoadInst>(U)) {
          // Just copy incoming indices
          ResToIdxReplacement[LI] = gepVector;
        }
      }
    }
  }

  // Create new index stores for incoming indices
  void ReplaceStores() {
    // generate stores of incoming indices to corresponding index pointers
    if (Stores.empty())
      return;
    for (auto V : Stores) {
      StoreInst *SI = cast<StoreInst>(V);
      IRBuilder<> Builder(SI);
      IndexVector &idxVector = ResToIdxReplacement[SI];
      Value *Ptr = SI->getPointerOperand();
      Value *Val = SI->getValueOperand();
      IndexVector &ptrIndices = ResToIdxReplacement[Ptr];
      IndexVector &valIndices = ResToIdxReplacement[Val];
      DXASSERT_NOMSG(ptrIndices.size() == valIndices.size());
      idxVector.resize(ptrIndices.size(), nullptr);
      for (unsigned i = 0; i < idxVector.size(); i++) {
        idxVector[i] = Builder.CreateStore(valIndices[i], ptrIndices[i]);
      }
    }
  }

  // For each Phi/Select: update matching incoming values for new phis
  void UpdateSelects() {
    for (auto V : Selects) {
      // update incoming index values corresponding to incoming resource values
      IndexVector &idxVector = ResToIdxReplacement[V];
      Instruction *I = cast<Instruction>(V);
      unsigned numOperands = I->getNumOperands();
      unsigned startOp = isa<PHINode>(V) ? 0 : 1;
      for (unsigned iOp = startOp; iOp < numOperands; iOp++) {
        IndexVector &incomingIndices = ResToIdxReplacement[I->getOperand(iOp)];
        DXASSERT_NOMSG(idxVector.size() == incomingIndices.size());
        for (unsigned i = 0; i < idxVector.size(); i++) {
          // must be instruction (phi/select)
          Instruction *indexI = cast<Instruction>(idxVector[i]);
          indexI->setOperand(iOp, incomingIndices[i]);
        }

        // Now clear incoming operand (adding to cleanup) to break cycles
        if (Instruction *OpI = dyn_cast<Instruction>(I->getOperand(iOp)))
          CleanupInsts.insert(OpI);
        I->setOperand(iOp, UndefValue::get(I->getType()));
      }
    }
  }

  // ReplaceHandles
  //  - iterate handles
  //    - insert GEP using new indices associated with resource value
  //    - load resource from new GEP
  //    - replace resource use in createHandleForLib with new load
  // Assumes: no users of handle are phi/select or store
  void ReplaceHandles() {
    if (Handles.empty())
      return;
    Type *i32Ty = IntegerType::getInt32Ty(Handles[0]->getContext());
    Constant *Zero = Constant::getIntegerValue(i32Ty, APInt(32, 0));
    for (auto V : Handles) {
      CallInst *CI = cast<CallInst>(V);
      DxilInst_CreateHandleForLib createHandle(CI);
      Value *res = createHandle.get_Resource();
      // Skip extra work if nothing between load and create handle
      if (LoadInst *LI = dyn_cast<LoadInst>(res)) {
        Value *Ptr = LI->getPointerOperand();
        if (GEPOperator *GEP = dyn_cast<GEPOperator>(Ptr))
          Ptr = GEP->getPointerOperand();
        if (isa<GlobalVariable>(Ptr))
          continue;
      }
      GlobalVariable *GV = LookupResourceGV(res);
      if (!GV)
        continue; // skip value removed due to conflict
      IRBuilder<> Builder(CI);
      IndexVector &idxVector = ResToIdxReplacement[res];
      DXASSERT(idxVector.size() == CountArrayDimensions(GV->getType()), "replacements empty or invalid");
      SmallVector<Value*, 4> gepIndices;
      gepIndices.push_back(Zero);
      for (auto idxVal : idxVector)
        gepIndices.push_back(LookupValue(idxVal));
      Value *GEP = Builder.CreateInBoundsGEP(GV, gepIndices);
      // Mark new GEP instruction non-uniform if necessary
      if (NonUniformSet.count(res) != 0 || NonUniformSet.count(CI) != 0)
        if (GetElementPtrInst *GEPInst = dyn_cast<GetElementPtrInst>(GEP))
          DxilMDHelper::MarkNonUniform(GEPInst);
      LoadInst *LI = Builder.CreateLoad(GEP);
      createHandle.set_Resource(LI);
      if (Instruction *resI = dyn_cast<Instruction>(res))
        CleanupInsts.insert(resI);
    }
  }

  // Delete unused CleanupInsts, restarting when changed
  // Return true if something was deleted
  bool CleanupUnusedValues() {
    //  - delete unused CleanupInsts, restarting when changed
    bool bAnyChanges = false;
    bool bChanged = false;
    do {
      bChanged = false;
      for (auto it = CleanupInsts.begin(); it != CleanupInsts.end();) {
        Instruction *I = *(it++);
        if (I->user_empty()) {
          // Add instructions operands CleanupInsts
          for (unsigned iOp = 0; iOp < I->getNumOperands(); iOp++) {
            if (Instruction *opI = dyn_cast<Instruction>(I->getOperand(iOp)))
              CleanupInsts.insert(opI);
          }
          I->eraseFromParent();
          CleanupInsts.erase(I);
          bChanged = true;
        }
      }
      if (bChanged)
        bAnyChanges = true;
    } while (bChanged);
    return bAnyChanges;
  }

  void SimplifyMerges() {
    // Loop if changed
    bool bChanged = false;
    do {
      bChanged = false;
      for (auto V : NewSelects) {
        if (LookupValue(V) != V)
          continue;
        Instruction *I = cast<Instruction>(V);
        unsigned startOp = isa<PHINode>(I) ? 0 : 1;
        Value *newV = dxilutil::MergeSelectOnSameValue(
          cast<Instruction>(V), startOp, I->getNumOperands());
        if (newV) {
          RemappedValues[V] = newV;
          bChanged = true;
        }
      }
    } while (bChanged);
  }

  void CleanupDeadInsts() {
    // Assuming everything was successful:
    // delete stores to allocas to remove cycles
    for (auto V : Stores) {
      StoreInst *SI = cast<StoreInst>(V);
      if (Instruction *I = dyn_cast<Instruction>(SI->getValueOperand()))
        CleanupInsts.insert(I);
      if (Instruction *I = dyn_cast<Instruction>(SI->getPointerOperand()))
        CleanupInsts.insert(I);
      SI->eraseFromParent();
    }
    CleanupUnusedValues();
  }

  void VerifyComplete(DxilModule &DM) {
    // Check that all handles now resolve to a global variable, otherwise,
    // they are likely loading from resource function parameter, which
    // is disallowed.
    hlsl::OP *hlslOP = DM.GetOP();
    for (Function &F : DM.GetModule()->functions()) {
      if (hlslOP->IsDxilOpFunc(&F)) {
        hlsl::OP::OpCodeClass opClass;
        if (hlslOP->GetOpCodeClass(&F, opClass) &&
          opClass == DXIL::OpCodeClass::CreateHandleForLib) {
          for (auto U : F.users()) {
            CallInst *CI = cast<CallInst>(U);
            if (m_Errors.ErrorUsers.count(CI))
              continue;   // Error already reported
            DxilInst_CreateHandleForLib createHandle(CI);
            Value *res = createHandle.get_Resource();
            LoadInst *LI = dyn_cast<LoadInst>(res);
            if (LI) {
              Value *Ptr = LI->getPointerOperand();
              if (GEPOperator *GEP = dyn_cast<GEPOperator>(Ptr))
                Ptr = GEP->getPointerOperand();
              if (isa<GlobalVariable>(Ptr))
                continue;
            }
            // handle wasn't processed
            // Right now, the most likely cause is user call with resources, but
            // this should be updated if there are other reasons for this to happen.
            m_Errors.ReportError(ResourceUseErrors::UserCallsWithResources, U);
          }
        }
      }
    }
  }

  // Fix resource global variable properties to external constant
  bool SetExternalConstant(GlobalVariable *GV) {
    if (GV->hasInitializer() || !GV->isConstant() ||
        GV->getLinkage() != GlobalVariable::LinkageTypes::ExternalLinkage) {
      GV->setInitializer(nullptr);
      GV->setConstant(true);
      GV->setLinkage(GlobalVariable::LinkageTypes::ExternalLinkage);
      return true;
    }
    return false;
  }

  bool CollectResources(HLModule &HM) {
    bool bChanged = false;
    for (const auto &res : HM.GetCBuffers()) {
      if (GlobalVariable *GV = dyn_cast<GlobalVariable>(res->GetGlobalSymbol())) {
        bChanged |= SetExternalConstant(GV);
        CollectResourceGVUsers(GV, GV);
      }
    }
    for (const auto &res : HM.GetSRVs()) {
      if (GlobalVariable *GV = dyn_cast<GlobalVariable>(res->GetGlobalSymbol())) {
        bChanged |= SetExternalConstant(GV);
        CollectResourceGVUsers(GV, GV);
      }
    }
    for (const auto &res : HM.GetUAVs()) {
      if (GlobalVariable *GV = dyn_cast<GlobalVariable>(res->GetGlobalSymbol())) {
        bChanged |= SetExternalConstant(GV);
        CollectResourceGVUsers(GV, GV);
      }
    }
    for (const auto &res : HM.GetSamplers()) {
      if (GlobalVariable *GV = dyn_cast<GlobalVariable>(res->GetGlobalSymbol())) {
        bChanged |= SetExternalConstant(GV);
        CollectResourceGVUsers(GV, GV);
      }
    }
    return bChanged;
  }

  bool CollectResources(DxilModule &DM) {
    bool bChanged = false;
    for (const auto &res : DM.GetCBuffers()) {
      if (GlobalVariable *GV = dyn_cast<GlobalVariable>(res->GetGlobalSymbol())) {
        bChanged |= SetExternalConstant(GV);
        CollectResourceGVUsers(GV, GV);
      }
    }
    for (const auto &res : DM.GetSRVs()) {
      if (GlobalVariable *GV = dyn_cast<GlobalVariable>(res->GetGlobalSymbol())) {
        bChanged |= SetExternalConstant(GV);
        CollectResourceGVUsers(GV, GV);
      }
    }
    for (const auto &res : DM.GetUAVs()) {
      if (GlobalVariable *GV = dyn_cast<GlobalVariable>(res->GetGlobalSymbol())) {
        bChanged |= SetExternalConstant(GV);
        CollectResourceGVUsers(GV, GV);
      }
    }
    for (const auto &res : DM.GetSamplers()) {
      if (GlobalVariable *GV = dyn_cast<GlobalVariable>(res->GetGlobalSymbol())) {
        bChanged |= SetExternalConstant(GV);
        CollectResourceGVUsers(GV, GV);
      }
    }
    return bChanged;
  }

  void DoTransform() {
    RemoveConflicts();
    CreateSelects();
    CreateIndexAllocas();
    ReplaceAllocaGEPs();
    ReplaceAllocaLoads();
    ReplaceGVGEPs();
    ReplaceStores();
    UpdateSelects();
    SimplifyMerges();
    ReplaceHandles();
    if (!m_Errors.ErrorsReported())
      CleanupDeadInsts();
  }

  bool ErrorsReported() {
    return m_Errors.ErrorsReported();
  }

  bool runOnModule(llvm::Module &M) {
    DxilModule &DM = M.GetOrCreateDxilModule();

    bool bChanged = CollectResources(DM);

    // If no selects or allocas are involved, there isn't anything to do
    if (Selects.empty() && Allocas.empty())
      return bChanged;

    DoTransform();
    VerifyComplete(DM);

    return true;
  }
};



template<typename T>
static std::string DumpValue(T *V) {
  std::string Val;
  raw_string_ostream OS(Val);
  OS << *V;
  OS.flush();
  return Val;
}

// Replace this with the stock llvm one.
static void Unroll_RemapInstruction(Instruction *I,
                                    ValueToValueMapTy &VMap) {
  for (unsigned op = 0, E = I->getNumOperands(); op != E; ++op) {
    Value *Op = I->getOperand(op);
    ValueToValueMapTy::iterator It = VMap.find(Op);
    if (It != VMap.end())
      I->setOperand(op, It->second);
  }

  if (PHINode *PN = dyn_cast<PHINode>(I)) {
    for (unsigned i = 0, e = PN->getNumIncomingValues(); i != e; ++i) {
      ValueToValueMapTy::iterator It = VMap.find(PN->getIncomingBlock(i));
      if (It != VMap.end())
        PN->setIncomingBlock(i, cast<BasicBlock>(It->second));
    }
  }
}


class DxilLoopUnroll : public LoopPass {
public:
  static char ID;
  LoopInfo *LI = nullptr;
  DxilLoopUnroll() : LoopPass(ID) {}
  bool runOnLoop(Loop *L, LPPassManager &LPM) override;
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    //AU.addRequired<AssumptionCacheTracker>();
    AU.addRequired<LoopInfoWrapperPass>();
    AU.addPreserved<LoopInfoWrapperPass>();
    AU.addRequiredID(LoopSimplifyID);

    AU.addRequired<DominatorTreeWrapperPass>();
    //AU.addPreservedID(LoopSimplifyID);
    //AU.addRequiredID(LCSSAID);
    //AU.addPreservedID(LCSSAID);
    //AU.addRequired<ScalarEvolution>();
    //AU.addPreserved<ScalarEvolution>();
    //AU.addRequired<TargetTransformInfoWrapperPass>();
    // FIXME: Loop unroll requires LCSSA. And LCSSA requires dom info.
    // If loop unroll does not preserve dom info then LCSSA pass on next
    // loop will receive invalid dom info.
    // For now, recreate dom info, if loop is unrolled.
    //AU.addPreserved<DominatorTreeWrapperPass>();
  }
};

char DxilLoopUnroll::ID;

static bool SimplifyPHIs(BasicBlock *BB) {
  bool Changed = false;
  SmallVector<Instruction *, 16> Removed;
  for (Instruction &I : *BB) {
    PHINode *PN = dyn_cast<PHINode>(&I);
    if (!PN)
      continue;

    if (PN->getNumIncomingValues() == 1) {
      Value *V = PN->getIncomingValue(0);
      PN->replaceAllUsesWith(V);
      Removed.push_back(PN);
      Changed = true;
    }
  }

  for (Instruction *I : Removed)
    I->eraseFromParent();

  return Changed;
}

static void FindAllDataDependency(Instruction *I, std::set<Instruction *> &Set, std::set<BasicBlock *> &Blocks) {
  for (User *U : I->users()) {
    if (PHINode *PN = dyn_cast<PHINode>(U)) {
      continue;
    }
    else if (Instruction *UserI = dyn_cast<Instruction>(U)) {
      if (!Set.count(UserI)) {
        Set.insert(UserI);
        Blocks.insert(UserI->getParent());
        FindAllDataDependency(UserI, Set, Blocks);
      }
    }
  }
}

struct ClonedIteration {
  SmallVector<BasicBlock *, 16> Body;
  BasicBlock *Latch = nullptr;
  BasicBlock *Header = nullptr;
  ValueToValueMapTy VarMap;
  std::set<BasicBlock *> Extended;

  ClonedIteration(const ClonedIteration &o) {
    Body = o.Body;
    Latch = o.Latch;
    for (ValueToValueMapTy::const_iterator It = o.VarMap.begin(), End = o.VarMap.end(); It != End; It++)
      VarMap[It->first] = It->second;
  }
  ClonedIteration() {}
};

static void ReplaceUsersIn(BasicBlock *BB, Value *Old, Value *New) {
  SmallVector<Use *, 16> Uses;
  for (Use &U : Old->uses()) {
    if (Instruction *I = dyn_cast<Instruction>(U.getUser())) {
      if (I->getParent() == BB) {
        Uses.push_back(&U);
      }
    }
  }
  
  for (Use *U : Uses) {
    U->set(New);
  }
}

static bool IsConstantI1(Value *V, bool *Val=nullptr) {
  if (ConstantInt *C = dyn_cast<ConstantInt>(V)) {
    if (V->getType() == Type::getInt1Ty(V->getContext())) {
      if (Val)
        *Val = (bool)C->getLimitedValue();
      return true;
    }
  }
  return false;
}

// Figure out what to do with this.
static bool SimplifyInstructionsInBlock_NoDelete(BasicBlock *BB,
                                       const TargetLibraryInfo *TLI) {
  bool MadeChange = false;

#ifndef NDEBUG
  // In debug builds, ensure that the terminator of the block is never replaced
  // or deleted by these simplifications. The idea of simplification is that it
  // cannot introduce new instructions, and there is no way to replace the
  // terminator of a block without introducing a new instruction.
  AssertingVH<Instruction> TerminatorVH(--BB->end());
#endif

  for (BasicBlock::iterator BI = BB->begin(), E = --BB->end(); BI != E; ) {
    assert(!BI->isTerminator());
    Instruction *Inst = BI++;

    WeakVH BIHandle(BI);
    if (recursivelySimplifyInstruction(Inst, TLI)) {
      MadeChange = true;
      if (BIHandle != BI)
        BI = BB->begin();
      continue;
    }

//    MadeChange |= RecursivelyDeleteTriviallyDeadInstructions(Inst, TLI);
    if (BIHandle != BI)
      BI = BB->begin();
  }
  return MadeChange;
}

static bool IsLoopInvariant(Value *V, Loop *L) {
  if (L->isLoopInvariant(V)) {
    return true;
  }
  if (PHINode *PN = dyn_cast<PHINode>(V)) {
    if (PN->getNumIncomingValues() == 0) {
      return IsLoopInvariant(PN->getIncomingValue(0), L);
    }
  }
  return false;
}

static bool HasUnrollElements(BasicBlock *BB, Loop *L) {
  for (Instruction &I : *BB) {
    if (LoadInst *Load = dyn_cast<LoadInst>(&I)) {
      Value *PtrV = Load->getPointerOperand();
      if (hlsl::dxilutil::IsHLSLObjectType(PtrV->getType()->getPointerElementType())) {
        if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(PtrV)) {
          if (GEP->hasAllConstantIndices())
            continue;
          for (auto It = GEP->idx_begin(); It != GEP->idx_end(); It++) {
            Value *Idx = *It;
            if (!IsLoopInvariant(Idx, L)) {
              return true;
            }
          }
        }
      }
    }
  }
  return false;
}

bool IsMarkedFullUnroll(Loop *L) {
  if (MDNode *LoopID = L->getLoopID())
    return GetUnrollMetadata(LoopID, "llvm.loop.unroll.full");
  return false;
}

static bool HeuristicallyDetermineUnrollNecessary(Loop *L) {
  for (BasicBlock *BB : L->getBlocks()) {
    if (HasUnrollElements(BB, L))
      return true;
  }
  SmallVector<BasicBlock *, 16> ExitBlocks;
  L->getExitBlocks(ExitBlocks);
  for (BasicBlock *BB : ExitBlocks)
    if (HasUnrollElements(BB, L))
      return true;
  return false;
}

static bool HasSuccessorsInLoop(BasicBlock *BB, Loop *L) {
  bool PartOfOuterLoop = false;
  for (BasicBlock *Succ : successors(BB)) {
    if (L->contains(Succ)) {
      return true;
    }
  }
  return false;
}

static void DetachFromSuccessors(BasicBlock *BB) {
  SmallVector<BasicBlock *, 16> Successors(succ_begin(BB), succ_end(BB));
  for (BasicBlock *Succ : Successors) {
    Succ->removePredecessor(BB);
  }
}

/// Return true if the specified block is in the list.
static bool isExitBlock(BasicBlock *BB,
                        const SmallVectorImpl<BasicBlock *> &ExitBlocks) {
  for (unsigned i = 0, e = ExitBlocks.size(); i != e; ++i)
    if (ExitBlocks[i] == BB)
      return true;
  return false;
}

static bool processInstruction(std::set<BasicBlock *> &Body, Loop &L, Instruction &Inst, DominatorTree &DT, // HLSL Change
                               const SmallVectorImpl<BasicBlock *> &ExitBlocks,
                               PredIteratorCache &PredCache, LoopInfo *LI) {

  SmallVector<Use *, 16> UsesToRewrite;

  BasicBlock *InstBB = Inst.getParent();

  for (Use &U : Inst.uses()) {
    Instruction *User = cast<Instruction>(U.getUser());
    BasicBlock *UserBB = User->getParent();
    if (PHINode *PN = dyn_cast<PHINode>(User))
      UserBB = PN->getIncomingBlock(U);

    if (InstBB != UserBB && /*!L.contains(UserBB)*/!Body.count(UserBB)) // HLSL Change
      UsesToRewrite.push_back(&U);
  }

  // If there are no uses outside the loop, exit with no change.
  if (UsesToRewrite.empty())
    return false;
#if 0
  ++NumLCSSA; // We are applying the transformation
#endif
  // Invoke instructions are special in that their result value is not available
  // along their unwind edge. The code below tests to see whether DomBB
  // dominates
  // the value, so adjust DomBB to the normal destination block, which is
  // effectively where the value is first usable.
  BasicBlock *DomBB = Inst.getParent();
  if (InvokeInst *Inv = dyn_cast<InvokeInst>(&Inst))
    DomBB = Inv->getNormalDest();

  DomTreeNode *DomNode = DT.getNode(DomBB);

  SmallVector<PHINode *, 16> AddedPHIs;
  SmallVector<PHINode *, 8> PostProcessPHIs;

  SSAUpdater SSAUpdate;
  SSAUpdate.Initialize(Inst.getType(), Inst.getName());

  // Insert the LCSSA phi's into all of the exit blocks dominated by the
  // value, and add them to the Phi's map.
  for (SmallVectorImpl<BasicBlock *>::const_iterator BBI = ExitBlocks.begin(),
                                                     BBE = ExitBlocks.end();
       BBI != BBE; ++BBI) {
    BasicBlock *ExitBB = *BBI;
    if (!DT.dominates(DomNode, DT.getNode(ExitBB)))
      continue;

    // If we already inserted something for this BB, don't reprocess it.
    if (SSAUpdate.HasValueForBlock(ExitBB))
      continue;

    PHINode *PN = PHINode::Create(Inst.getType(), PredCache.size(ExitBB),
                                  Inst.getName() + ".lcssa", ExitBB->begin());

    // Add inputs from inside the loop for this PHI.
    for (BasicBlock *Pred : PredCache.get(ExitBB)) {
      PN->addIncoming(&Inst, Pred);

      // If the exit block has a predecessor not within the loop, arrange for
      // the incoming value use corresponding to that predecessor to be
      // rewritten in terms of a different LCSSA PHI.
      if (/*!L.contains(Pred)*/ !Body.count(Pred)) // HLSL Change
        UsesToRewrite.push_back(
            &PN->getOperandUse(PN->getOperandNumForIncomingValue(
                 PN->getNumIncomingValues() - 1)));
    }

    AddedPHIs.push_back(PN);

    // Remember that this phi makes the value alive in this block.
    SSAUpdate.AddAvailableValue(ExitBB, PN);

    // LoopSimplify might fail to simplify some loops (e.g. when indirect
    // branches are involved). In such situations, it might happen that an exit
    // for Loop L1 is the header of a disjoint Loop L2. Thus, when we create
    // PHIs in such an exit block, we are also inserting PHIs into L2's header.
    // This could break LCSSA form for L2 because these inserted PHIs can also
    // have uses outside of L2. Remember all PHIs in such situation as to
    // revisit than later on. FIXME: Remove this if indirectbr support into
    // LoopSimplify gets improved.
    if (auto *OtherLoop = LI->getLoopFor(ExitBB))
      if (!L.contains(OtherLoop))
        PostProcessPHIs.push_back(PN);
  }

  // Rewrite all uses outside the loop in terms of the new PHIs we just
  // inserted.
  for (unsigned i = 0, e = UsesToRewrite.size(); i != e; ++i) {
    // If this use is in an exit block, rewrite to use the newly inserted PHI.
    // This is required for correctness because SSAUpdate doesn't handle uses in
    // the same block.  It assumes the PHI we inserted is at the end of the
    // block.
    Instruction *User = cast<Instruction>(UsesToRewrite[i]->getUser());
    BasicBlock *UserBB = User->getParent();
    if (PHINode *PN = dyn_cast<PHINode>(User))
      UserBB = PN->getIncomingBlock(*UsesToRewrite[i]);

    if (isa<PHINode>(UserBB->begin()) && isExitBlock(UserBB, ExitBlocks)) {
      // Tell the VHs that the uses changed. This updates SCEV's caches.
      if (UsesToRewrite[i]->get()->hasValueHandle())
        ValueHandleBase::ValueIsRAUWd(*UsesToRewrite[i], UserBB->begin());
      UsesToRewrite[i]->set(UserBB->begin());
      continue;
    }

    // Otherwise, do full PHI insertion.
    SSAUpdate.RewriteUse(*UsesToRewrite[i]);
  }

  // Post process PHI instructions that were inserted into another disjoint loop
  // and update their exits properly.
  for (auto *I : PostProcessPHIs) {
    if (I->use_empty())
      continue;

    BasicBlock *PHIBB = I->getParent();
    Loop *OtherLoop = LI->getLoopFor(PHIBB);
    SmallVector<BasicBlock *, 8> EBs;
    OtherLoop->getExitBlocks(EBs);
    if (EBs.empty())
      continue;

    // Recurse and re-process each PHI instruction. FIXME: we should really
    // convert this entire thing to a worklist approach where we process a
    // vector of instructions...
    processInstruction(Body, *OtherLoop, *I, DT, EBs, PredCache, LI);
  }

  // Remove PHI nodes that did not have any uses rewritten.
  for (unsigned i = 0, e = AddedPHIs.size(); i != e; ++i) {
    if (AddedPHIs[i]->use_empty())
      AddedPHIs[i]->eraseFromParent();
  }

  return true;

}

static bool blockDominatesAnExit(BasicBlock *BB,
                     DominatorTree &DT,
                     const SmallVectorImpl<BasicBlock *> &ExitBlocks) {
  DomTreeNode *DomNode = DT.getNode(BB);
  for (BasicBlock *Exit : ExitBlocks)
    if (DT.dominates(DomNode, DT.getNode(Exit)))
      return true;
  return false;
};

// We need to recreate the LCSSA form since our loop boundary is potentially different from
// the canonical one.
static bool CreateLCSSA(std::set<BasicBlock *> &Body, std::set<BasicBlock *> &ExitBlockSet, Loop *L, DominatorTree &DT, LoopInfo *LI) {
  /*
  std::set<BasicBlock *> ExitBlockSet;
  for (BasicBlock *BB : Body) {
    for (BasicBlock *Succ : successors(BB)) {
      if (!Body.count(Succ)) {
        ExitBlockSet.insert(Succ);
      }
    }
  }*/
  SmallVector<BasicBlock *, 4> ExitBlocks(ExitBlockSet.begin(), ExitBlockSet.end());

  PredIteratorCache PredCache;
  bool Changed = false;
  // Look at all the instructions in the loop, checking to see if they have uses
  // outside the loop.  If so, rewrite those uses.
  for (std::set<BasicBlock *>::iterator BBI = Body.begin(), BBE = Body.end();
       BBI != BBE; ++BBI) {
    BasicBlock *BB = *BBI;

    // For large loops, avoid use-scanning by using dominance information:  In
    // particular, if a block does not dominate any of the loop exits, then none
    // of the values defined in the block could be used outside the loop.
    if (!blockDominatesAnExit(BB, DT, ExitBlocks))
      continue;

    for (BasicBlock::iterator I = BB->begin(), E = BB->end(); I != E; ++I) {
      // Reject two common cases fast: instructions with no uses (like stores)
      // and instructions with one use that is in the same block as this.
      if (I->use_empty() ||
          (I->hasOneUse() && I->user_back()->getParent() == BB &&
           !isa<PHINode>(I->user_back())))
        continue;

      Instruction *Inst = &*I;
      Changed |= processInstruction(Body, *L, *I, DT, ExitBlocks, PredCache, LI);
    }
  }

  return Changed;
}

static bool IsInExitBlocks(Instruction *I, SmallVectorImpl<BasicBlock *> &Exits) {
  for (BasicBlock *BB : Exits)
    if (I->getParent() == BB)
      return true;
  return false;
}

static void FindAllocas(Value *V, std::set<AllocaInst *> &Insts) {
  if (AllocaInst *AI = dyn_cast<AllocaInst>(V)) {
    Insts.insert(AI);
  }
  else if (GEPOperator *GEP = dyn_cast<GEPOperator>(V)) {
    FindAllocas(GEP->getPointerOperand(), Insts);
  }
  else if (StoreInst *StoreI = dyn_cast<StoreInst>(V)) {
    FindAllocas(StoreI->getPointerOperand(), Insts);
  }
  else if (LoadInst *LoadI = dyn_cast<LoadInst>(V)) {
    FindAllocas(LoadI->getPointerOperand(), Insts);
  }
}

static Instruction *GetNonConstIdx(Value *V) {
  if (PHINode *PN = dyn_cast<PHINode>(V)) {
    if (PN->getNumIncomingValues() == 1) {
      return GetNonConstIdx(PN->getIncomingValue(0));
    }
    return PN;
  }
  else if (Instruction *I = dyn_cast<Instruction>(V)) {
    return I;
  }
  return nullptr;
}
#if 0
static void FindProblemUsers(Loop *L, std::set<BasicBlock *> &ProblemBlocks) {
  Module *M = L->getHeader()->getModule();

  SmallVector<BasicBlock *, 16> ExitBlocks;
  L->getExitBlocks(ExitBlocks);

  LegalizeResourceUseHelper Helper;
  Helper.CollectResources(M->GetHLModule());
  ValueSetVector &ProblemInsts = Helper.m_Errors.ErrorSets[ResourceUseErrors::GVConflicts];

  std::set<AllocaInst *> ProblemAllocas;

  bool ContainsInstsNeedToUnroll = false;
  for (Value *V : ProblemInsts) {
    FindAllocas(V, ProblemAllocas);
  }

  for (AllocaInst *AI : ProblemAllocas) {
    for (User *U : AI->users()) {
      if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(U)) {
        //if (!GEP->hasAllConstantIndices()) {
        if (L->contains(GEP->getParent()) || IsInExitBlocks(GEP, ExitBlocks)) {
          for (auto IdxIt = GEP->idx_begin(); IdxIt != GEP->idx_end(); IdxIt++) {
            Value *Idx = *IdxIt;
            if (Instruction *NonConstIdx = GetNonConstIdx(Idx)) {
              if (L->contains(NonConstIdx->getParent())) {
                ProblemBlocks.insert(GEP->getParent());
                break;
              }
            }
          }
        }
      }
    }
  }
}
#endif

bool IsProblemBlock(BasicBlock *BB, Loop *L) {
  //SimplifyPHIs(BB);
  for (Instruction &I : *BB) {
    if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(&I)) {
      for (auto IdxIt = GEP->idx_begin(); IdxIt != GEP->idx_end(); IdxIt++) {
        Value *Idx = *IdxIt;
        if (Instruction *NonConstIdx = GetNonConstIdx(Idx)) {
          if (L->contains(NonConstIdx->getParent())) {
            return true;
          }
        }
      }
    }
  }
  return false;
}
static void FindProblemUsers(Loop *L, std::set<BasicBlock *> &Blocks) {
  SmallVector<BasicBlock *, 16> ExitBlocks;
  L->getExitBlocks(ExitBlocks);

  for (BasicBlock *BB : L->getBlocks()) {
    if (IsProblemBlock(BB, L))
      Blocks.insert(BB);
  }
  for (BasicBlock *BB : ExitBlocks) {
    if (IsProblemBlock(BB, L))
      Blocks.insert(BB);
  }
}

bool DxilLoopUnroll::runOnLoop(Loop *L, LPPassManager &LPM) {

  Module *M = L->getHeader()->getModule();

  std::set<BasicBlock *> ProblemBlocks;
  FindProblemUsers(L, ProblemBlocks);

  if (!IsMarkedFullUnroll(L) && !ProblemBlocks.size())
    return false;

  if (!L->isSafeToClone())
    return false;

  LoopInfo *LI = &getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
  DominatorTree *DT = &getAnalysis<DominatorTreeWrapperPass>().getDomTree(); // TODO: Update the Dom tree
  Function *F = L->getBlocks()[0]->getParent();

  BasicBlock *Latch = L->getLoopLatch();
  BasicBlock *Header = L->getHeader();

  SmallVector<BasicBlock *, 16> ExitBlocks;
  L->getExitBlocks(ExitBlocks);
  SmallVector<BasicBlock *, 16> BlocksInLoop(L->getBlocks().begin(), L->getBlocks().end());
  BlocksInLoop.append(ExitBlocks.begin(), ExitBlocks.end());
  std::set<BasicBlock *> ExitBlockSet;

  // Quit if we don't have a single latch block
  if (!Latch)
    return false;

  // TODO: See if possible to do this without requiring loop rotation.
  // If the loop exit condition is not in the latch, then the loop is not rotated. Give up.
  if (!cast<BranchInst>(Latch->getTerminator())->isConditional())
    return false;

  // Simplify the PHI nodes that have single incoming value. The original LCSSA form
  // (if exists) does not necessarily work for our unroll because we may be unrolling
  // from a different boundary.
  for (BasicBlock *BB : BlocksInLoop)
    SimplifyPHIs(BB);

#if 0
  // Determine if we absolutely
  if (!HeuristicallyDetermineUnrollNecessary(L))
    return false;
#endif

  // Keep track of the PHI nodes at the header.
  SmallVector<PHINode *, 16> PHIs;
  for (auto it = Header->begin(); it != Header->end(); it++) {
    if (PHINode *PN = dyn_cast<PHINode>(it)) {
      if (PN->getNumIncomingValues() != 2)
        return false;
      PHIs.push_back(PN);
    }
    else {
      break;
    }
  }

  std::set<BasicBlock *> ToBeCloned;
  for (BasicBlock *BB : L->getBlocks())
    ToBeCloned.insert(BB);

  std::set<BasicBlock *> NewExits;
  std::set<BasicBlock *> FakeExits;
  for (BasicBlock *BB : ExitBlocks) {
    ExitBlockSet.insert(BB);
    bool CloneThisExitBlock = true;// !!ProblemBlocks.count(BB);

    if (CloneThisExitBlock) {
      ToBeCloned.insert(BB);

      BasicBlock *FakeExit = BasicBlock::Create(BB->getContext(), "loop.exit.new");
      F->getBasicBlockList().insert(BB, FakeExit);

      TerminatorInst *OldTerm = BB->getTerminator();
      OldTerm->removeFromParent();
      FakeExit->getInstList().push_back(OldTerm);

      BranchInst::Create(FakeExit, BB);
      for (BasicBlock *Succ : successors(FakeExit)) {
        for (Instruction &I : *Succ) {
          if (PHINode *PN = dyn_cast<PHINode>(&I)) {
            for (unsigned i = 0; i < PN->getNumIncomingValues(); i++) {
              if (PN->getIncomingBlock(i) == BB)
                PN->setIncomingBlock(i, FakeExit);
            }
          }
        }
      }

      NewExits.insert(FakeExit);
      FakeExits.insert(FakeExit);
    }
    else {
      NewExits.insert(BB);
    }
  }

  SmallVector<ClonedIteration, 16> Clones;
  SmallVector<BasicBlock *, 16> ClonedBlocks;
  bool Succeeded = false;

  // Reistablish LCSSA form to get ready for unrolling.
  CreateLCSSA(ToBeCloned, NewExits, L, *DT, LI);

  std::map<BasicBlock *, BasicBlock *> CloneMap;
  std::map<BasicBlock *, BasicBlock *> ReverseCloneMap;
  for (int i = 0; i < 128; i++) { // TODO: Num of iterations
    ClonedBlocks.clear();
    CloneMap.clear();
    ClonedIteration *PrevIteration = nullptr;
    if (Clones.size())
      PrevIteration = &Clones.back();

    Clones.resize(Clones.size() + 1);
    ClonedIteration &Cloned = Clones.back();

    // Helper function for cloning a block
    auto CloneBlock = [Header, &ExitBlockSet, &ToBeCloned, &Cloned, &CloneMap, &ReverseCloneMap, &ClonedBlocks, F](BasicBlock *BB) { // TODO: Cleanup
      BasicBlock *ClonedBB = CloneBasicBlock(BB, Cloned.VarMap);
      ClonedBlocks.push_back(ClonedBB);
      ReverseCloneMap[ClonedBB] = BB;
      CloneMap[BB] = ClonedBB;
      ClonedBB->insertInto(F, Header);
      Cloned.VarMap[BB] = ClonedBB;

      if (ExitBlockSet.count(BB))
        Cloned.Extended.insert(ClonedBB);

      return ClonedBB;
    };

    for (BasicBlock *BB : ToBeCloned) {
      BasicBlock *ClonedBB = CloneBlock(BB);
      Cloned.Body.push_back(ClonedBB);
      if (BB == Latch) {
        Cloned.Latch = ClonedBB;
      }
      if (BB == Header) {
        Cloned.Header = ClonedBB;
      }
    }

    for (BasicBlock *ClonedBB : ClonedBlocks) {
      BasicBlock *BB = ReverseCloneMap[ClonedBB];
      // If branching to outside of the loop, need to update the
      // phi nodes there to include incoming values.
      for (BasicBlock *Succ : successors(ClonedBB)) {
        if (ToBeCloned.count(Succ))
          continue;
        for (Instruction &I : *Succ) {
          PHINode *PN = dyn_cast<PHINode>(&I);
          if (!PN)
            break;
          Value *OldIncoming = PN->getIncomingValueForBlock(BB);
          Value *NewIncoming = OldIncoming;
          if (Cloned.VarMap.count(OldIncoming)) { // TODO: Query once
            NewIncoming = Cloned.VarMap[OldIncoming];
          }
          PN->addIncoming(NewIncoming, ClonedBB);
        }
      }
    }


    for (BasicBlock *BB : ClonedBlocks) {
      for (Instruction &I : *BB) {
        Unroll_RemapInstruction(&I, Cloned.VarMap);
      }
    }

    for (int i = 0; i < ClonedBlocks.size(); i++) {
      BasicBlock *ClonedBB = ClonedBlocks[i];
      TerminatorInst *TI = ClonedBB->getTerminator();
      if (BranchInst *BI = dyn_cast<BranchInst>(TI)) {
        for (unsigned j = 0, NumSucc = BI->getNumSuccessors(); j < NumSucc; j++) {
          BasicBlock *OldSucc = BI->getSuccessor(j);
          if (CloneMap.count(OldSucc)) { // TODO: Do one query
            BI->setSuccessor(j, CloneMap[OldSucc]);
          }
        }
      }
    }

    // If this is the first block
    if (!PrevIteration) {
      // Replace the phi nodes in the clone block with the values coming
      // from outside of the loop
      for (PHINode *PN : PHIs) {
        PHINode *ClonedPN = cast<PHINode>(Cloned.VarMap[PN]);
        Value *ReplacementVal = ClonedPN->getIncomingValue(0); // TODO: Actually find the right one, also make sure there's only a single one.
        ClonedPN->replaceAllUsesWith(ReplacementVal);
        ClonedPN->eraseFromParent();
        Cloned.VarMap[PN] = ReplacementVal;
      }
    }
    else {
      for (PHINode *PN : PHIs) {
        PHINode *ClonedPN = cast<PHINode>(Cloned.VarMap[PN]);
        Value *ReplacementVal = PrevIteration->VarMap[PN->getIncomingValue(1)]; // TODO: Actually find the right one, also make sure there's only a single one.
        ClonedPN->replaceAllUsesWith(ReplacementVal);
        ClonedPN->eraseFromParent();
        Cloned.VarMap[PN] = ReplacementVal;
      }

      // Make the latch of the previous iteration branch to the header
      // of this new iteration.
      if (BranchInst *BI = dyn_cast<BranchInst>(PrevIteration->Latch->getTerminator())) {
        for (unsigned i = 0; i < BI->getNumSuccessors(); i++) {
          if (BI->getSuccessor(i) == PrevIteration->Header) {
            BI->setSuccessor(i, Cloned.Header);
            break;
          }
        }
      }
    }

    for (int i = 0; i < ClonedBlocks.size(); i++) {
      BasicBlock *ClonedBB = ClonedBlocks[i];
      SimplifyInstructionsInBlock_NoDelete(ClonedBB, NULL);
    }

    if (BranchInst *BI = dyn_cast<BranchInst>(Cloned.Latch->getTerminator())) {
      bool Cond = false;
      if (!IsConstantI1(BI->getCondition(), &Cond)) {
        break;
      }
      if (!Cond && BI->getSuccessor(0) == Cloned.Header) {
        Succeeded = true;
        break;
      }
      else if (Cond && BI->getSuccessor(1) == Cloned.Header) {
        Succeeded = true;
        break;
      }
    }
  }

  if (Succeeded) {
    // Go through the predecessors of the old header and
    // make them branch to the new header.
    SmallVector<BasicBlock *, 8> Preds(pred_begin(Header), pred_end(Header));
    for (BasicBlock *PredBB : Preds) {
      if (L->contains(PredBB))
        continue;
      BranchInst *BI = cast<BranchInst>(PredBB->getTerminator());
      for (unsigned i = 0, NumSucc = BI->getNumSuccessors(); i < NumSucc; i++) {
        if (BI->getSuccessor(i) == Header) {
          BI->setSuccessor(i, Clones.front().Header);
        }
      }
    }

    Loop *OuterL = L->getParentLoop();
    // If there's an outer loop, insert the new blocks
    // into
    if (OuterL) {
      for (size_t i = 0; i < Clones.size(); i++) {
        auto &Iteration = Clones[i];
        for (BasicBlock *BB : Iteration.Body) {
          if (!Iteration.Extended.count(BB))
            //if (i < Clones.size()-1 || HasSuccessorsInLoop(BB, OuterL)) // FIXME: Fix this. It still has return blocks being added to the outer loop.
            OuterL->addBasicBlockToLoop(BB, *LI);
        }
      }

      for (BasicBlock *BB : FakeExits) {
        if (HasSuccessorsInLoop(BB, OuterL)) // FIXME: Fix this. It still has return blocks being added to the outer loop.
          OuterL->addBasicBlockToLoop(BB, *LI);
      }

      for (size_t i = 0; i < Clones.size(); i++) {
        auto &Iteration = Clones[i];
        for (BasicBlock *BB : Iteration.Extended) {
          if (HasSuccessorsInLoop(BB, OuterL)) // FIXME: Fix this. It still has return blocks being added to the outer loop.
            OuterL->addBasicBlockToLoop(BB, *LI);
        }
      }

      // Remove the original blocks that we've cloned.
      for (BasicBlock *BB : ToBeCloned) {
        if (OuterL->contains(BB))
          OuterL->removeBlockFromLoop(BB);
      }
      // TODO: Simplify here, since outer loop is now weird (multiple latches etc).
      //simplifyLoop(OuterL, nullptr, LI, nullptr);
    }

    // Remove flattened loop from queue.
    // If there's an outer loop, this will also take care
    // of removing blocks.
    LPM.deleteLoopFromQueue(L); // TODO: Figure out the impact of this.
    // TODO: Update dominator tree

    for (BasicBlock *BB : ToBeCloned)
      DetachFromSuccessors(BB);
    for (BasicBlock *BB : ToBeCloned)
      BB->dropAllReferences();
    for (BasicBlock *BB : ToBeCloned)
      BB->eraseFromParent();

    return true;
  }

  // If we were unsuccessful in unrolling the loop
  else {
    // Remove all the cloned blocks
    for (ClonedIteration &Iteration : Clones)
      for (BasicBlock *BB : Iteration.Body)
        DetachFromSuccessors(BB);
    for (ClonedIteration &Iteration : Clones)
      for (BasicBlock *BB : Iteration.Body)
        BB->dropAllReferences();
    for (ClonedIteration &Iteration : Clones)
      for (BasicBlock *BB : Iteration.Body)
        BB->eraseFromParent();
    return false;
  }
}

}

Pass *llvm::createDxilLoopUnrollPass() {
  return new DxilLoopUnroll();
}

INITIALIZE_PASS(DxilLoopUnroll, "dxil-loop-unroll", "Dxil Unroll loops", false, false)
