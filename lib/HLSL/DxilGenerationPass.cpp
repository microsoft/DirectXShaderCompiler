///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilGenerationPass.cpp                                                    //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// DxilGenerationPass implementation.                                        //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/HLSL/DxilGenerationPass.h"
#include "dxc/HLSL/DxilOperations.h"
#include "dxc/HLSL/DxilModule.h"
#include "dxc/HLSL/HLModule.h"
#include "dxc/HLSL/HLOperations.h"
#include "dxc/HLSL/DxilInstructions.h"
#include "dxc/HLSL/HLMatrixLowerHelper.h"
#include "dxc/HlslIntrinsicOp.h"
#include "dxc/Support/Global.h"
#include "dxc/HLSL/DxilTypeSystem.h"
#include "dxc/HLSL/HLOperationLower.h"
#include "HLSignatureLower.h"
#include "dxc/HLSL/DxilUtil.h"
#include "dxc/Support/exception.h"
#include "DxilEntryProps.h"

#include "llvm/IR/GetElementPtrTypeIterator.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/PassManager.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Utils/SSAUpdater.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Transforms/Utils/PromoteMemToReg.h"
#include <memory>
#include <unordered_set>
#include <iterator>

using namespace llvm;
using namespace hlsl;

// TODO: use hlsl namespace for the most of this file.

namespace {

// Collect unused phi of resources and remove them.
class ResourceRemover : public LoadAndStorePromoter {
  AllocaInst *AI;
  mutable std::unordered_set<PHINode *> unusedPhis;

public:
  ResourceRemover(ArrayRef<Instruction *> Insts, SSAUpdater &S)
      : LoadAndStorePromoter(Insts, S), AI(nullptr) {}

  void run(AllocaInst *AI, const SmallVectorImpl<Instruction *> &Insts) {
    // Remember which alloca we're promoting (for isInstInList).
    this->AI = AI;
    LoadAndStorePromoter::run(Insts);
    for (PHINode *P : unusedPhis) {
      P->eraseFromParent();
    }
  }
  bool
  isInstInList(Instruction *I,
               const SmallVectorImpl<Instruction *> &Insts) const override {
    if (LoadInst *LI = dyn_cast<LoadInst>(I))
      return LI->getOperand(0) == AI;
    return cast<StoreInst>(I)->getPointerOperand() == AI;
  }

  void replaceLoadWithValue(LoadInst *LI, Value *V) const override {
    if (PHINode *PHI = dyn_cast<PHINode>(V)) {
      if (PHI->user_empty())
        unusedPhis.insert(PHI);
    }
    LI->replaceAllUsesWith(UndefValue::get(LI->getType()));
  }
};

void SimplifyGlobalSymbol(GlobalVariable *GV) {
  Type *Ty = GV->getType()->getElementType();
  if (!Ty->isArrayTy()) {
    // Make sure only 1 load of GV in each function.
    std::unordered_map<Function *, Instruction *> handleMapOnFunction;
    for (User *U : GV->users()) {
      if (LoadInst *LI = dyn_cast<LoadInst>(U)) {
        Function *F = LI->getParent()->getParent();
        auto it = handleMapOnFunction.find(F);
        if (it == handleMapOnFunction.end()) {
          handleMapOnFunction[F] = LI;
        } else {
          LI->replaceAllUsesWith(it->second);
        }
      }
    }
    for (auto it : handleMapOnFunction) {
      Function *F = it.first;
      Instruction *I = it.second;
      IRBuilder<> Builder(dxilutil::FirstNonAllocaInsertionPt(F));
      Value *headLI = Builder.CreateLoad(GV);
      I->replaceAllUsesWith(headLI);
    }
  }
}

void InitResourceBase(const DxilResourceBase *pSource,
                      DxilResourceBase *pDest) {
  DXASSERT_NOMSG(pSource->GetClass() == pDest->GetClass());
  pDest->SetKind(pSource->GetKind());
  pDest->SetID(pSource->GetID());
  pDest->SetSpaceID(pSource->GetSpaceID());
  pDest->SetLowerBound(pSource->GetLowerBound());
  pDest->SetRangeSize(pSource->GetRangeSize());
  pDest->SetGlobalSymbol(pSource->GetGlobalSymbol());
  pDest->SetGlobalName(pSource->GetGlobalName());
  pDest->SetHandle(pSource->GetHandle());

  if (GlobalVariable *GV = dyn_cast<GlobalVariable>(pSource->GetGlobalSymbol()))
    SimplifyGlobalSymbol(GV);
}

void InitResource(const DxilResource *pSource, DxilResource *pDest) {
  pDest->SetCompType(pSource->GetCompType());
  pDest->SetSampleCount(pSource->GetSampleCount());
  pDest->SetElementStride(pSource->GetElementStride());
  pDest->SetGloballyCoherent(pSource->IsGloballyCoherent());
  pDest->SetHasCounter(pSource->HasCounter());
  pDest->SetRW(pSource->IsRW());
  pDest->SetROV(pSource->IsROV());
  InitResourceBase(pSource, pDest);
}

void InitDxilModuleFromHLModule(HLModule &H, DxilModule &M, bool HasDebugInfo) {

  // Subsystems.
  unsigned ValMajor, ValMinor;
  H.GetValidatorVersion(ValMajor, ValMinor);
  M.SetValidatorVersion(ValMajor, ValMinor);
  M.SetShaderModel(H.GetShaderModel(), H.GetHLOptions().bUseMinPrecision);

  // Entry function.
  if (!M.GetShaderModel()->IsLib()) {
    Function *EntryFn = H.GetEntryFunction();
    M.SetEntryFunction(EntryFn);
    M.SetEntryFunctionName(H.GetEntryFunctionName());
  }

  std::vector<GlobalVariable* > &LLVMUsed = M.GetLLVMUsed();

  // Resources
  for (auto && C : H.GetCBuffers()) {
    auto b = llvm::make_unique<DxilCBuffer>();
    InitResourceBase(C.get(), b.get());
    b->SetSize(C->GetSize());
    LLVMUsed.emplace_back(cast<GlobalVariable>(b->GetGlobalSymbol()));
    M.AddCBuffer(std::move(b));
  }
  for (auto && C : H.GetUAVs()) {
    auto b = llvm::make_unique<DxilResource>();
    InitResource(C.get(), b.get());
    LLVMUsed.emplace_back(cast<GlobalVariable>(b->GetGlobalSymbol()));
    M.AddUAV(std::move(b));
  }
  for (auto && C : H.GetSRVs()) {
    auto b = llvm::make_unique<DxilResource>();
    InitResource(C.get(), b.get());
    LLVMUsed.emplace_back(cast<GlobalVariable>(b->GetGlobalSymbol()));
    M.AddSRV(std::move(b));
  }
  for (auto && C : H.GetSamplers()) {
    auto b = llvm::make_unique<DxilSampler>();
    InitResourceBase(C.get(), b.get());
    b->SetSamplerKind(C->GetSamplerKind());
    LLVMUsed.emplace_back(cast<GlobalVariable>(b->GetGlobalSymbol()));
    M.AddSampler(std::move(b));
  }

  // Signatures.
  M.ResetRootSignature(H.ReleaseRootSignature());

  // Shader properties.
  //bool m_bDisableOptimizations;
  M.SetDisableOptimization(H.GetHLOptions().bDisableOptimizations);
  //bool m_bDisableMathRefactoring;
  //bool m_bEnableDoublePrecision;
  //bool m_bEnableDoubleExtensions;
  //M.CollectShaderFlags();

  //bool m_bForceEarlyDepthStencil;
  //bool m_bEnableRawAndStructuredBuffers;
  //bool m_bEnableMSAD;
  //M.m_ShaderFlags.SetAllResourcesBound(H.GetHLOptions().bAllResourcesBound);

  // DXIL type system.
  M.ResetTypeSystem(H.ReleaseTypeSystem());
  // Dxil OP.
  M.ResetOP(H.ReleaseOP());
  // Keep llvm used.
  M.EmitLLVMUsed();

  M.SetAllResourcesBound(H.GetHLOptions().bAllResourcesBound);

  M.SetAutoBindingSpace(H.GetAutoBindingSpace());

  // Update Validator Version
  M.UpgradeToMinValidatorVersion();
}

class DxilGenerationPass : public ModulePass {
  HLModule *m_pHLModule;
  bool m_HasDbgInfo;
  HLSLExtensionsCodegenHelper *m_extensionsCodegenHelper;

public:
  static char ID; // Pass identification, replacement for typeid
  explicit DxilGenerationPass(bool NoOpt = false)
      : ModulePass(ID), m_pHLModule(nullptr), m_extensionsCodegenHelper(nullptr), NotOptimized(NoOpt) {}

  const char *getPassName() const override { return "DXIL Generator"; }

  void SetExtensionsHelper(HLSLExtensionsCodegenHelper *helper) {
    m_extensionsCodegenHelper = helper;
  }

  bool runOnModule(Module &M) override {
    m_pHLModule = &M.GetOrCreateHLModule();
    const ShaderModel *SM = m_pHLModule->GetShaderModel();

    // Load up debug information, to cross-reference values and the instructions
    // used to load them.
    m_HasDbgInfo = getDebugMetadataVersionFromModule(M) != 0;

    // EntrySig for shader functions.
    DxilEntryPropsMap EntryPropsMap;

    if (!SM->IsLib()) {
      Function *EntryFn = m_pHLModule->GetEntryFunction();
      if (!m_pHLModule->HasDxilFunctionProps(EntryFn)) {
        M.getContext().emitError("Entry function don't have property.");
        return false;
      }
      DxilFunctionProps &props = m_pHLModule->GetDxilFunctionProps(EntryFn);
      std::unique_ptr<DxilEntryProps> pProps =
          llvm::make_unique<DxilEntryProps>(
              props, m_pHLModule->GetHLOptions().bUseMinPrecision);
      HLSignatureLower sigLower(m_pHLModule->GetEntryFunction(), *m_pHLModule,
                                pProps->sig);
      sigLower.Run();
      EntryPropsMap[EntryFn] = std::move(pProps);
    } else {
      for (auto It = M.begin(); It != M.end();) {
        Function &F = *(It++);
        // Lower signature for each graphics or compute entry function.
        if (m_pHLModule->HasDxilFunctionProps(&F)) {
          DxilFunctionProps &props = m_pHLModule->GetDxilFunctionProps(&F);
          std::unique_ptr<DxilEntryProps> pProps =
              llvm::make_unique<DxilEntryProps>(
                  props, m_pHLModule->GetHLOptions().bUseMinPrecision);
          if (m_pHLModule->IsGraphicsShader(&F) ||
              m_pHLModule->IsComputeShader(&F)) {
            HLSignatureLower sigLower(&F, *m_pHLModule, pProps->sig);
            // TODO: BUG: This will lower patch constant function sigs twice if
            // used by two hull shaders!
            sigLower.Run();
          }
          EntryPropsMap[&F] = std::move(pProps);
        }
      }
    }

    std::unordered_set<LoadInst *> UpdateCounterSet;
    std::unordered_set<Value *> NonUniformSet;

    GenerateDxilOperations(M, UpdateCounterSet, NonUniformSet);

    GenerateDxilCBufferHandles(NonUniformSet);
    MarkUpdateCounter(UpdateCounterSet);
    LowerHLCreateHandle();
    MarkNonUniform(NonUniformSet);

    // LowerHLCreateHandle() should have translated HLCreateHandle to CreateHandleForLib.
    // Clean up HLCreateHandle functions.
    for (auto It = M.begin(); It != M.end();) {
      Function &F = *(It++);
      if (!F.isDeclaration()) {
        if (hlsl::GetHLOpcodeGroupByName(&F) ==
            HLOpcodeGroup::HLCreateHandle) {
          if (F.user_empty()) {
            F.eraseFromParent();
          } else {
            M.getContext().emitError("Fail to lower createHandle.");
          }
        }
      }
    }
    // Translate precise on allocas into function call to keep the information after mem2reg.
    // The function calls will be removed after propagate precise attribute.
    TranslatePreciseAttribute();

    // High-level metadata should now be turned into low-level metadata.
    const bool SkipInit = true;
    hlsl::DxilModule &DxilMod = M.GetOrCreateDxilModule(SkipInit);
    auto pProps = &EntryPropsMap.begin()->second->props;
    InitDxilModuleFromHLModule(*m_pHLModule, DxilMod, m_HasDbgInfo);
    DxilMod.ResetEntryPropsMap(std::move(EntryPropsMap));
    if (!SM->IsLib()) {
      DxilMod.SetShaderProperties(pProps);
    }

    HLModule::ClearHLMetadata(M);
    M.ResetHLModule();

    // We now have a DXIL representation - record this.
    SetPauseResumePasses(M, "hlsl-dxilemit", "hlsl-dxilload");

    (void)NotOptimized; // Dummy out unused member to silence warnings

    return true;
  }

private:
  void MarkUpdateCounter(std::unordered_set<LoadInst *> &UpdateCounterSet);
  // Generate DXIL cbuffer handles.
  void
  GenerateDxilCBufferHandles(std::unordered_set<Value *> &NonUniformSet);

  // change built-in funtion into DXIL operations
  void GenerateDxilOperations(Module &M,
                              std::unordered_set<LoadInst *> &UpdateCounterSet,
                              std::unordered_set<Value *> &NonUniformSet);
  void LowerHLCreateHandle();
  void MarkNonUniform(std::unordered_set<Value *> &NonUniformSet);

  // Translate precise attribute into HL function call.
  void TranslatePreciseAttribute();

  // Input module is not optimized.
  bool NotOptimized;
};
}

namespace {
void TranslateHLCreateHandle(Function *F, hlsl::OP &hlslOP) {
  Value *opArg = hlslOP.GetU32Const(
      (unsigned)DXIL::OpCode::CreateHandleForLib);

  for (auto U = F->user_begin(); U != F->user_end();) {
    Value *user = *(U++);
    if (!isa<Instruction>(user))
      continue;
    // must be call inst
    CallInst *CI = cast<CallInst>(user);
    Value *res = CI->getArgOperand(HLOperandIndex::kUnaryOpSrc0Idx);
    Value *newHandle = nullptr;
    IRBuilder<> Builder(CI);
    // Res could be ld/phi/select. Will be removed in
    // DxilLowerCreateHandleForLib.
    Function *createHandle = hlslOP.GetOpFunc(
        DXIL::OpCode::CreateHandleForLib, res->getType());
    newHandle = Builder.CreateCall(createHandle, {opArg, res});

    CI->replaceAllUsesWith(newHandle);
    if (res->user_empty()) {
      if (Instruction *I = dyn_cast<Instruction>(res))
        I->eraseFromParent();
    }

    CI->eraseFromParent();
  }
}
} // namespace

void DxilGenerationPass::LowerHLCreateHandle() {
  Module *M = m_pHLModule->GetModule();
  hlsl::OP &hlslOP = *m_pHLModule->GetOP();
  // generate dxil operation
  for (iplist<Function>::iterator F : M->getFunctionList()) {
    if (F->user_empty())
      continue;
    if (!F->isDeclaration()) {
      hlsl::HLOpcodeGroup group = hlsl::GetHLOpcodeGroup(F);
      if (group == HLOpcodeGroup::HLCreateHandle) {
        // Will lower in later pass.
        TranslateHLCreateHandle(F, hlslOP);
      }
    }
  }
}

void DxilGenerationPass::MarkNonUniform(
    std::unordered_set<Value *> &NonUniformSet) {
  for (Value *V : NonUniformSet) {
    for (User *U : V->users()) {
      if (GetElementPtrInst *I = dyn_cast<GetElementPtrInst>(U)) {
        DxilMDHelper::MarkNonUniform(I);
      }
    }
  }
}

static void
MarkUavUpdateCounter(Value* LoadOrGEP,
                     DxilResource &res,
                     std::unordered_set<LoadInst *> &UpdateCounterSet) {
  if (LoadInst *ldInst = dyn_cast<LoadInst>(LoadOrGEP)) {
    if (UpdateCounterSet.count(ldInst)) {
      DXASSERT_NOMSG(res.GetClass() == DXIL::ResourceClass::UAV);
      res.SetHasCounter(true);
    }
  } else {
    DXASSERT(dyn_cast<GEPOperator>(LoadOrGEP) != nullptr,
             "else AddOpcodeParamForIntrinsic in CodeGen did not patch uses "
             "to only have ld/st refer to temp object");
    GEPOperator *GEP = cast<GEPOperator>(LoadOrGEP);
    for (auto GEPU : GEP->users()) {
      MarkUavUpdateCounter(GEPU, res, UpdateCounterSet);
    }
  }
}
static void
MarkUavUpdateCounter(DxilResource &res,
                     std::unordered_set<LoadInst *> &UpdateCounterSet) {
  Value *GV = res.GetGlobalSymbol();
  for (auto U = GV->user_begin(), E = GV->user_end(); U != E;) {
    User *user = *(U++);
    // Skip unused user.
    if (user->user_empty())
      continue;
    MarkUavUpdateCounter(user, res, UpdateCounterSet);
  }
}

void DxilGenerationPass::MarkUpdateCounter(
    std::unordered_set<LoadInst *> &UpdateCounterSet) {
  for (size_t i = 0; i < m_pHLModule->GetUAVs().size(); i++) {
    HLResource &UAV = m_pHLModule->GetUAV(i);
    MarkUavUpdateCounter(UAV, UpdateCounterSet);
  }
}

void DxilGenerationPass::GenerateDxilCBufferHandles(
    std::unordered_set<Value *> &NonUniformSet) {
  // For CBuffer, handle are mapped to HLCreateHandle.
  OP *hlslOP = m_pHLModule->GetOP();
  Value *opArg = hlslOP->GetU32Const((unsigned)OP::OpCode::CreateHandleForLib);
  LLVMContext &Ctx = hlslOP->GetCtx();
  Value *zeroIdx = hlslOP->GetU32Const(0);

  for (size_t i = 0; i < m_pHLModule->GetCBuffers().size(); i++) {
    DxilCBuffer &CB = m_pHLModule->GetCBuffer(i);
    GlobalVariable *GV = cast<GlobalVariable>(CB.GetGlobalSymbol());
    // Remove GEP created in HLObjectOperationLowerHelper::UniformCbPtr.
    GV->removeDeadConstantUsers();
    std::string handleName = std::string(GV->getName());

    DIVariable *DIV = nullptr;
    DILocation *DL = nullptr;
    if (m_HasDbgInfo) {
      DebugInfoFinder &Finder = m_pHLModule->GetOrCreateDebugInfoFinder();
      DIV = HLModule::FindGlobalVariableDebugInfo(GV, Finder);
      if (DIV)
        // TODO: how to get col?
        DL = DILocation::get(Ctx, DIV->getLine(), 1,
                             DIV->getScope());
    }

    if (CB.GetRangeSize() == 1) {
      Function *createHandle =
          hlslOP->GetOpFunc(OP::OpCode::CreateHandleForLib,
                            GV->getType()->getElementType());
      for (auto U = GV->user_begin(); U != GV->user_end(); ) {
        // Must HLCreateHandle.
        CallInst *CI = cast<CallInst>(*(U++));
        // Put createHandle to entry block.
        IRBuilder<> Builder(dxilutil::FirstNonAllocaInsertionPt(CI));
        Value *V = Builder.CreateLoad(GV);
        CallInst *handle = Builder.CreateCall(createHandle, {opArg, V}, handleName);
        if (m_HasDbgInfo) {
          // TODO: add debug info.
          //handle->setDebugLoc(DL);
          (void)(DL);
        }
        CI->replaceAllUsesWith(handle);
        CI->eraseFromParent();
      }
    } else {
      PointerType *Ty = GV->getType();
      Type *EltTy = Ty->getElementType()->getArrayElementType()->getPointerTo(
          Ty->getAddressSpace());
      Function *createHandle = hlslOP->GetOpFunc(
          OP::OpCode::CreateHandleForLib, EltTy->getPointerElementType());

      for (auto U = GV->user_begin(); U != GV->user_end();) {
        // Must HLCreateHandle.
        CallInst *CI = cast<CallInst>(*(U++));
        IRBuilder<> Builder(CI);
        Value *CBIndex = CI->getArgOperand(HLOperandIndex::kCreateHandleIndexOpIdx);
        if (isa<ConstantInt>(CBIndex)) {
          // Put createHandle to entry block for const index.
          Builder.SetInsertPoint(dxilutil::FirstNonAllocaInsertionPt(CI));
        }
        // Add GEP for cbv array use.
        Value *GEP = Builder.CreateGEP(GV, {zeroIdx, CBIndex});
        /*
        if (!NonUniformSet.count(CBIndex))
          args[DXIL::OperandIndex::kCreateHandleIsUniformOpIdx] =
              hlslOP->GetI1Const(0);
        else
          args[DXIL::OperandIndex::kCreateHandleIsUniformOpIdx] =
              hlslOP->GetI1Const(1);*/

        Value *V = Builder.CreateLoad(GEP);
        CallInst *handle = Builder.CreateCall(createHandle, {opArg, V}, handleName);
        CI->replaceAllUsesWith(handle);
        CI->eraseFromParent();
      }
    }
  } 
}

void DxilGenerationPass::GenerateDxilOperations(
    Module &M, std::unordered_set<LoadInst *> &UpdateCounterSet,
    std::unordered_set<Value *> &NonUniformSet) {
  // remove all functions except entry function
  Function *entry = m_pHLModule->GetEntryFunction();
  const ShaderModel *pSM = m_pHLModule->GetShaderModel();
  Function *patchConstantFunc = nullptr;
  if (pSM->IsHS()) {
    DxilFunctionProps &funcProps = m_pHLModule->GetDxilFunctionProps(entry);
    patchConstantFunc = funcProps.ShaderProps.HS.patchConstantFunc;
  }

  if (!pSM->IsLib()) {
    for (auto F = M.begin(); F != M.end();) {
      Function *func = F++;

      if (func->isDeclaration())
        continue;
      if (func == entry)
        continue;
      if (func == patchConstantFunc)
        continue;
      if (func->user_empty())
        func->eraseFromParent();
    }
  }

  TranslateBuiltinOperations(*m_pHLModule, m_extensionsCodegenHelper,
                             UpdateCounterSet, NonUniformSet);

  // Remove unused HL Operation functions.
  std::vector<Function *> deadList;
  for (iplist<Function>::iterator F : M.getFunctionList()) {
    hlsl::HLOpcodeGroup group = hlsl::GetHLOpcodeGroupByName(F);
    if (group != HLOpcodeGroup::NotHL || F->isIntrinsic())
      if (F->user_empty())
        deadList.emplace_back(F);
  }

  for (Function *F : deadList)
    F->eraseFromParent();
}

static void TranslatePreciseAttributeOnFunction(Function &F, Module &M) {
  BasicBlock &BB = F.getEntryBlock(); // Get the entry node for the function

  // Find allocas that has precise attribute, by looking at all instructions in
  // the entry node
  for (BasicBlock::iterator I = BB.begin(), E = BB.end(); I != E;) {
    Instruction *Inst = (I++);
    if (AllocaInst *AI = dyn_cast<AllocaInst>(Inst)) {
      if (HLModule::HasPreciseAttributeWithMetadata(AI)) {
        HLModule::MarkPreciseAttributeOnPtrWithFunctionCall(AI, M);
      }
    } else {
      DXASSERT(!HLModule::HasPreciseAttributeWithMetadata(Inst), "Only alloca can has precise metadata.");
    }
  }

  FastMathFlags FMF;
  FMF.setUnsafeAlgebra();
  // Set fast math for all FPMathOperators.
  // Already set FastMath in options. But that only enable things like fadd.
  // Every inst which type is float can be cast to FPMathOperator.
  for (Function::iterator BBI = F.begin(), BBE = F.end(); BBI != BBE; ++BBI) {
    BasicBlock *BB = BBI;
    for (BasicBlock::iterator I = BB->begin(), E = BB->end(); I != E; ++I) {
      if (dyn_cast<FPMathOperator>(I)) {
        // Set precise fast math on those instructions that support it.
        if (DxilModule::PreservesFastMathFlags(I))
          I->copyFastMathFlags(FMF);
      }
    }
  }
}

void DxilGenerationPass::TranslatePreciseAttribute() {  
  bool bIEEEStrict = m_pHLModule->GetHLOptions().bIEEEStrict;
  // If IEEE strict, everying is precise, don't need to mark it.
  if (bIEEEStrict)
    return;

  Module &M = *m_pHLModule->GetModule();
  // TODO: If not inline every function, for function has call site with precise
  // argument and call site without precise argument, need to clone the function
  // to propagate the precise for the precise call site.
  // This should be done at CGMSHLSLRuntime::FinishCodeGen.
  if (m_pHLModule->GetShaderModel()->IsLib()) {
    // TODO: If all functions have been inlined, and unreferenced functions removed,
    //        it should make sense to run on all funciton bodies,
    //        even when not processing a library.
    for (Function &F : M.functions()) {
      if (!F.isDeclaration())
        TranslatePreciseAttributeOnFunction(F, M);
    }
  } else {
    Function *EntryFn = m_pHLModule->GetEntryFunction();
    TranslatePreciseAttributeOnFunction(*EntryFn, M);
    if (m_pHLModule->GetShaderModel()->IsHS()) {
      DxilFunctionProps &EntryQual = m_pHLModule->GetDxilFunctionProps(EntryFn);
      Function *patchConstantFunc = EntryQual.ShaderProps.HS.patchConstantFunc;
      TranslatePreciseAttributeOnFunction(*patchConstantFunc, M);
    }
  }
}

char DxilGenerationPass::ID = 0;

ModulePass *llvm::createDxilGenerationPass(bool NotOptimized, hlsl::HLSLExtensionsCodegenHelper *extensionsHelper) {
  DxilGenerationPass *dxilPass = new DxilGenerationPass(NotOptimized);
  dxilPass->SetExtensionsHelper(extensionsHelper);
  return dxilPass;
}

INITIALIZE_PASS(DxilGenerationPass, "dxilgen", "HLSL DXIL Generation", false, false)


///////////////////////////////////////////////////////////////////////////////

namespace {
class HLEmitMetadata : public ModulePass {
public:
  static char ID; // Pass identification, replacement for typeid
  explicit HLEmitMetadata() : ModulePass(ID) {}

  const char *getPassName() const override { return "HLSL High-Level Metadata Emit"; }

  bool runOnModule(Module &M) override {
    if (M.HasHLModule()) {
      HLModule::ClearHLMetadata(M);
      M.GetHLModule().EmitHLMetadata();
      return true;
    }

    return false;
  }
};
}

char HLEmitMetadata::ID = 0;

ModulePass *llvm::createHLEmitMetadataPass() {
  return new HLEmitMetadata();
}

INITIALIZE_PASS(HLEmitMetadata, "hlsl-hlemit", "HLSL High-Level Metadata Emit", false, false)

///////////////////////////////////////////////////////////////////////////////

namespace {
class HLEnsureMetadata : public ModulePass {
public:
  static char ID; // Pass identification, replacement for typeid
  explicit HLEnsureMetadata() : ModulePass(ID) {}

  const char *getPassName() const override { return "HLSL High-Level Metadata Ensure"; }

  bool runOnModule(Module &M) override {
    if (!M.HasHLModule()) {
      M.GetOrCreateHLModule();
      return true;
    }

    return false;
  }
};
}

char HLEnsureMetadata::ID = 0;

ModulePass *llvm::createHLEnsureMetadataPass() {
  return new HLEnsureMetadata();
}

INITIALIZE_PASS(HLEnsureMetadata, "hlsl-hlensure", "HLSL High-Level Metadata Ensure", false, false)

///////////////////////////////////////////////////////////////////////////////
// Precise propagate.

namespace {
class DxilPrecisePropagatePass : public ModulePass {
public:
  static char ID; // Pass identification, replacement for typeid
  explicit DxilPrecisePropagatePass() : ModulePass(ID) {}

  const char *getPassName() const override { return "DXIL Precise Propagate"; }

  bool runOnModule(Module &M) override {
    DxilModule &dxilModule = M.GetOrCreateDxilModule();
    DxilTypeSystem &typeSys = dxilModule.GetTypeSystem();
    std::unordered_set<Instruction*> processedSet;
    std::vector<Function*> deadList;
    for (Function &F : M.functions()) {
      if (HLModule::HasPreciseAttribute(&F)) {
        PropagatePreciseOnFunctionUser(F, typeSys, processedSet);
        deadList.emplace_back(&F);
      }
    }
    for (Function *F : deadList)
      F->eraseFromParent();
    return true;
  }

private:
  void PropagatePreciseOnFunctionUser(
      Function &F, DxilTypeSystem &typeSys,
      std::unordered_set<Instruction *> &processedSet);
};

char DxilPrecisePropagatePass::ID = 0;

}

static void PropagatePreciseAttribute(Instruction *I, DxilTypeSystem &typeSys,
    std::unordered_set<Instruction *> &processedSet);

static void PropagatePreciseAttributeOnOperand(
    Value *V, DxilTypeSystem &typeSys, LLVMContext &Context,
    std::unordered_set<Instruction *> &processedSet) {
  Instruction *I = dyn_cast<Instruction>(V);
  // Skip none inst.
  if (!I)
    return;

  FPMathOperator *FPMath = dyn_cast<FPMathOperator>(I);
  // Skip none FPMath
  if (!FPMath)
    return;

  // Skip inst already marked.
  if (processedSet.count(I) > 0)
    return;
  // TODO: skip precise on integer type, sample instruction...
  processedSet.insert(I);
  // Set precise fast math on those instructions that support it.
  if (DxilModule::PreservesFastMathFlags(I))
    DxilModule::SetPreciseFastMathFlags(I);

  // Fast math not work on call, use metadata.
  if (CallInst *CI = dyn_cast<CallInst>(I))
    HLModule::MarkPreciseAttributeWithMetadata(CI);
  PropagatePreciseAttribute(I, typeSys, processedSet);
}

static void PropagatePreciseAttributeOnPointer(
    Value *Ptr, DxilTypeSystem &typeSys, LLVMContext &Context,
    std::unordered_set<Instruction *> &processedSet) {
  // Find all store and propagate on the val operand of store.
  // For CallInst, if Ptr is used as out parameter, mark it.
  for (User *U : Ptr->users()) {
    Instruction *user = cast<Instruction>(U);
    if (StoreInst *stInst = dyn_cast<StoreInst>(user)) {
      Value *val = stInst->getValueOperand();
      PropagatePreciseAttributeOnOperand(val, typeSys, Context, processedSet);
    } else if (CallInst *CI = dyn_cast<CallInst>(user)) {
      bool bReadOnly = true;

      Function *F = CI->getCalledFunction();
      const DxilFunctionAnnotation *funcAnnotation =
          typeSys.GetFunctionAnnotation(F);
      for (unsigned i = 0; i < CI->getNumArgOperands(); ++i) {
        if (Ptr != CI->getArgOperand(i))
          continue;

        const DxilParameterAnnotation &paramAnnotation =
            funcAnnotation->GetParameterAnnotation(i);
        // OutputPatch and OutputStream will be checked after scalar repl.
        // Here only check out/inout
        if (paramAnnotation.GetParamInputQual() == DxilParamInputQual::Out ||
            paramAnnotation.GetParamInputQual() == DxilParamInputQual::Inout) {
          bReadOnly = false;
          break;
        }
      }

      if (!bReadOnly)
        PropagatePreciseAttributeOnOperand(CI, typeSys, Context, processedSet);
    }
  }
}

static void
PropagatePreciseAttribute(Instruction *I, DxilTypeSystem &typeSys,
                          std::unordered_set<Instruction *> &processedSet) {
  LLVMContext &Context = I->getContext();
  if (AllocaInst *AI = dyn_cast<AllocaInst>(I)) {
    PropagatePreciseAttributeOnPointer(AI, typeSys, Context, processedSet);
  } else if (dyn_cast<CallInst>(I)) {
    // Propagate every argument.
    // TODO: only propagate precise argument.
    for (Value *src : I->operands())
      PropagatePreciseAttributeOnOperand(src, typeSys, Context, processedSet);
  } else if (dyn_cast<FPMathOperator>(I)) {
    // TODO: only propagate precise argument.
    for (Value *src : I->operands())
      PropagatePreciseAttributeOnOperand(src, typeSys, Context, processedSet);
  } else if (LoadInst *ldInst = dyn_cast<LoadInst>(I)) {
    Value *Ptr = ldInst->getPointerOperand();
    PropagatePreciseAttributeOnPointer(Ptr, typeSys, Context, processedSet);
  } else if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(I))
    PropagatePreciseAttributeOnPointer(GEP, typeSys, Context, processedSet);
  // TODO: support more case which need
}

void DxilPrecisePropagatePass::PropagatePreciseOnFunctionUser(
    Function &F, DxilTypeSystem &typeSys,
    std::unordered_set<Instruction *> &processedSet) {
  LLVMContext &Context = F.getContext();
  for (auto U = F.user_begin(), E = F.user_end(); U != E;) {
    CallInst *CI = cast<CallInst>(*(U++));
    Value *V = CI->getArgOperand(0);
    PropagatePreciseAttributeOnOperand(V, typeSys, Context, processedSet);
    CI->eraseFromParent();
  }
}

ModulePass *llvm::createDxilPrecisePropagatePass() {
  return new DxilPrecisePropagatePass();
}

INITIALIZE_PASS(DxilPrecisePropagatePass, "hlsl-dxil-precise", "DXIL precise attribute propagate", false, false)

///////////////////////////////////////////////////////////////////////////////

namespace {
class HLDeadFunctionElimination : public ModulePass {
public:
  static char ID; // Pass identification, replacement for typeid
  explicit HLDeadFunctionElimination () : ModulePass(ID) {}

  const char *getPassName() const override { return "Remove all unused function except entry from HLModule"; }

  bool runOnModule(Module &M) override {
    if (M.HasHLModule()) {
      HLModule &HLM = M.GetHLModule();

      bool IsLib = HLM.GetShaderModel()->IsLib();
      // Remove unused functions except entry and patch constant func.
      // For library profile, only remove unused external functions.
      Function *EntryFunc = HLM.GetEntryFunction();
      Function *PatchConstantFunc = HLM.GetPatchConstantFunction();

      return dxilutil::RemoveUnusedFunctions(M, EntryFunc, PatchConstantFunc,
                                             IsLib);
    }

    return false;
  }
};
}

char HLDeadFunctionElimination::ID = 0;

ModulePass *llvm::createHLDeadFunctionEliminationPass() {
  return new HLDeadFunctionElimination();
}

INITIALIZE_PASS(HLDeadFunctionElimination, "hl-dfe", "Remove all unused function except entry from HLModule", false, false)


///////////////////////////////////////////////////////////////////////////////
// Legalize resource use.
// Map local or static global resource to global resource.
// Require inline for static global resource.

namespace {

static const StringRef kStaticResourceLibErrorMsg = "static global resource use is disallowed in library exports.";

class DxilPromoteStaticResources : public ModulePass {
public:
  static char ID; // Pass identification, replacement for typeid
  explicit DxilPromoteStaticResources()
      : ModulePass(ID) {}

  const char *getPassName() const override {
    return "DXIL Legalize Static Resource Use";
  }

  bool runOnModule(Module &M) override {
    // Promote static global variables.
    return PromoteStaticGlobalResources(M);
  }

private:
  bool PromoteStaticGlobalResources(Module &M);
};

char DxilPromoteStaticResources::ID = 0;

class DxilPromoteLocalResources : public FunctionPass {
  void getAnalysisUsage(AnalysisUsage &AU) const override;

public:
  static char ID; // Pass identification, replacement for typeid
  explicit DxilPromoteLocalResources()
      : FunctionPass(ID) {}

  const char *getPassName() const override {
    return "DXIL Legalize Resource Use";
  }

  bool runOnFunction(Function &F) override {
    // Promote local resource first.
    return PromoteLocalResource(F);
  }

private:
  bool PromoteLocalResource(Function &F);
};

char DxilPromoteLocalResources::ID = 0;

}

void DxilPromoteLocalResources::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<AssumptionCacheTracker>();
  AU.addRequired<DominatorTreeWrapperPass>();
  AU.setPreservesAll();
}

bool DxilPromoteLocalResources::PromoteLocalResource(Function &F) {
  bool bModified = false;
  std::vector<AllocaInst *> Allocas;
  DominatorTree *DT = &getAnalysis<DominatorTreeWrapperPass>().getDomTree();
  AssumptionCache &AC =
      getAnalysis<AssumptionCacheTracker>().getAssumptionCache(F);

  BasicBlock &BB = F.getEntryBlock();
  unsigned allocaSize = 0;
  while (1) {
    Allocas.clear();

    // Find allocas that are safe to promote, by looking at all instructions in
    // the entry node
    for (BasicBlock::iterator I = BB.begin(), E = --BB.end(); I != E; ++I)
      if (AllocaInst *AI = dyn_cast<AllocaInst>(I)) { // Is it an alloca?
        if (dxilutil::IsHLSLObjectType(dxilutil::GetArrayEltTy(AI->getAllocatedType()))) {
          if (isAllocaPromotable(AI))
            Allocas.push_back(AI);
        }
      }
    if (Allocas.empty())
      break;

    // No update.
    // Report error and break.
    if (allocaSize == Allocas.size()) {
      F.getContext().emitError(dxilutil::kResourceMapErrorMsg);
      break;
    }
    allocaSize = Allocas.size();

    PromoteMemToReg(Allocas, *DT, nullptr, &AC);
    bModified = true;
  }

  return bModified;
}

FunctionPass *llvm::createDxilPromoteLocalResources() {
  return new DxilPromoteLocalResources();
}

INITIALIZE_PASS_BEGIN(DxilPromoteLocalResources,
                      "hlsl-dxil-promote-local-resources",
                      "DXIL promote local resource use", false, true)
INITIALIZE_PASS_DEPENDENCY(AssumptionCacheTracker)
INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
INITIALIZE_PASS_END(DxilPromoteLocalResources,
                    "hlsl-dxil-promote-local-resources",
                    "DXIL promote local resource use", false, true)

bool DxilPromoteStaticResources::PromoteStaticGlobalResources(
    Module &M) {
  if (M.GetOrCreateHLModule().GetShaderModel()->IsLib()) {
    // Read/write to global static resource is disallowed for libraries:
    // Resource use needs to be resolved to a single real global resource,
    // but it may not be possible since any external function call may re-enter
    // at any other library export, which could modify the global static
    // between write and read.
    // While it could work for certain cases, describing the boundary at
    // the HLSL level is difficult, so at this point it's better to disallow.
    // example of what could work:
    //  After inlining, exported functions must have writes to static globals
    //  before reads, and must not have any external function calls between
    //  writes and subsequent reads, such that the static global may be
    //  optimized away for the exported function.
    for (auto &GV : M.globals()) {
      if (GV.getLinkage() == GlobalVariable::LinkageTypes::InternalLinkage &&
        dxilutil::IsHLSLObjectType(dxilutil::GetArrayEltTy(GV.getType()))) {
        if (!GV.user_empty()) {
          if (Instruction *I = dyn_cast<Instruction>(*GV.user_begin())) {
            dxilutil::EmitErrorOnInstruction(I, kStaticResourceLibErrorMsg);
            break;
          }
        }
      }
    }
    return false;
  }

  bool bModified = false;
  std::set<GlobalVariable *> staticResources;
  for (auto &GV : M.globals()) {
    if (GV.getLinkage() == GlobalVariable::LinkageTypes::InternalLinkage &&
        dxilutil::IsHLSLObjectType(dxilutil::GetArrayEltTy(GV.getType()))) {
      staticResources.insert(&GV);
    }
  }
  SSAUpdater SSA;
  SmallVector<Instruction *, 4> Insts;
  // Make sure every resource load has mapped to global variable.
  while (!staticResources.empty()) {
    bool bUpdated = false;
    for (auto it = staticResources.begin(); it != staticResources.end();) {
      GlobalVariable *GV = *(it++);
      // Build list of instructions to promote.
      for (User *U : GV->users()) {
        Instruction *I = cast<Instruction>(U);
        Insts.emplace_back(I);
      }

      LoadAndStorePromoter(Insts, SSA).run(Insts);
      if (GV->user_empty()) {
        bUpdated = true;
        staticResources.erase(GV);
      }

      Insts.clear();
    }
    if (!bUpdated) {
      M.getContext().emitError(dxilutil::kResourceMapErrorMsg);
      break;
    }
    bModified = true;
  }
  return bModified;
}

ModulePass *llvm::createDxilPromoteStaticResources() {
  return new DxilPromoteStaticResources();
}

INITIALIZE_PASS(DxilPromoteStaticResources,
                "hlsl-dxil-promote-static-resources",
                "DXIL promote static resource use", false, false)

///////////////////////////////////////////////////////////////////////////////
// Legalize EvalOperations.
// Make sure src of EvalOperations are from function parameter.
// This is needed in order to translate EvaluateAttribute operations that traces
// back to LoadInput operations during translation stage. Promoting load/store
// instructions beforehand will allow us to easily trace back to loadInput from
// function call.
namespace {

class DxilLegalizeEvalOperations : public ModulePass {
public:
  static char ID; // Pass identification, replacement for typeid
  explicit DxilLegalizeEvalOperations() : ModulePass(ID) {}

  const char *getPassName() const override {
    return "DXIL Legalize EvalOperations";
  }

  bool runOnModule(Module &M) override {
    for (Function &F : M.getFunctionList()) {
      hlsl::HLOpcodeGroup group = hlsl::GetHLOpcodeGroup(&F);
      if (group != HLOpcodeGroup::NotHL) {
        std::vector<CallInst *> EvalFunctionCalls;
        // Find all EvaluateAttribute calls
        for (User *U : F.users()) {
          if (CallInst *CI = dyn_cast<CallInst>(U)) {
            IntrinsicOp evalOp =
                static_cast<IntrinsicOp>(hlsl::GetHLOpcode(CI));
            if (evalOp == IntrinsicOp::IOP_EvaluateAttributeAtSample ||
                evalOp == IntrinsicOp::IOP_EvaluateAttributeCentroid ||
                evalOp == IntrinsicOp::IOP_EvaluateAttributeSnapped ||
                evalOp == IntrinsicOp::IOP_GetAttributeAtVertex) {
              EvalFunctionCalls.push_back(CI);
            }
          }
        }
        if (EvalFunctionCalls.empty()) {
          continue;
        }
        // Start from the call instruction, find all allocas that this call
        // uses.
        std::unordered_set<AllocaInst *> allocas;
        for (CallInst *CI : EvalFunctionCalls) {
          FindAllocasForEvalOperations(CI, allocas);
        }
        SSAUpdater SSA;
        SmallVector<Instruction *, 4> Insts;
        for (AllocaInst *AI : allocas) {
          for (User *user : AI->users()) {
            if (isa<LoadInst>(user) || isa<StoreInst>(user)) {
              Insts.emplace_back(cast<Instruction>(user));
            }
          }
          LoadAndStorePromoter(Insts, SSA).run(Insts);
          Insts.clear();
        }
      }
    }
    return true;
  }

private:
  void FindAllocasForEvalOperations(Value *val,
                                    std::unordered_set<AllocaInst *> &allocas);
};

char DxilLegalizeEvalOperations::ID = 0;

// Find allocas for EvaluateAttribute operations
void DxilLegalizeEvalOperations::FindAllocasForEvalOperations(
    Value *val, std::unordered_set<AllocaInst *> &allocas) {
  Value *CurVal = val;
  while (!isa<AllocaInst>(CurVal)) {
    if (CallInst *CI = dyn_cast<CallInst>(CurVal)) {
      CurVal = CI->getOperand(HLOperandIndex::kUnaryOpSrc0Idx);
    } else if (InsertElementInst *IE = dyn_cast<InsertElementInst>(CurVal)) {
      Value *arg0 =
          IE->getOperand(0); // Could be another insertelement or undef
      Value *arg1 = IE->getOperand(1);
      FindAllocasForEvalOperations(arg0, allocas);
      CurVal = arg1;
    } else if (ShuffleVectorInst *SV = dyn_cast<ShuffleVectorInst>(CurVal)) {
      Value *arg0 = SV->getOperand(0);
      Value *arg1 = SV->getOperand(1);
      FindAllocasForEvalOperations(
          arg0, allocas); // Shuffle vector could come from different allocas
      CurVal = arg1;
    } else if (ExtractElementInst *EE = dyn_cast<ExtractElementInst>(CurVal)) {
      CurVal = EE->getOperand(0);
    } else if (LoadInst *LI = dyn_cast<LoadInst>(CurVal)) {
      CurVal = LI->getOperand(0);
    } else {
      break;
    }
  }
  if (AllocaInst *AI = dyn_cast<AllocaInst>(CurVal)) {
    allocas.insert(AI);
  }
}
} // namespace

ModulePass *llvm::createDxilLegalizeEvalOperationsPass() {
  return new DxilLegalizeEvalOperations();
}

INITIALIZE_PASS(DxilLegalizeEvalOperations,
                "hlsl-dxil-legalize-eval-operations",
                "DXIL legalize eval operations", false, false)

///////////////////////////////////////////////////////////////////////////////
// Translate RawBufferLoad/RawBufferStore
// This pass is to make sure that we generate correct buffer load for DXIL
// For DXIL < 1.2, rawBufferLoad will be translated to BufferLoad instruction
// without mask.
// For DXIL >= 1.2, if min precision is enabled, currently generation pass is
// producing i16/f16 return type for min precisions. For rawBuffer, we will
// change this so that min precisions are returning its actual scalar type (i32/f32)
// and will be truncated to their corresponding types after loading / before storing.
namespace {

// Create { v0, v1 } from { v0.lo, v0.hi, v1.lo, v1.hi }
void Make64bitResultForLoad(Type *EltTy, ArrayRef<Value *> resultElts32,
                            unsigned size, MutableArrayRef<Value *> resultElts,
                            hlsl::OP *hlslOP, IRBuilder<> &Builder) {
  Type *i64Ty = Builder.getInt64Ty();
  Type *doubleTy = Builder.getDoubleTy();
  if (EltTy == doubleTy) {
    Function *makeDouble =
        hlslOP->GetOpFunc(DXIL::OpCode::MakeDouble, doubleTy);
    Value *makeDoubleOpArg =
        Builder.getInt32((unsigned)DXIL::OpCode::MakeDouble);
    for (unsigned i = 0; i < size; i++) {
      Value *lo = resultElts32[2 * i];
      Value *hi = resultElts32[2 * i + 1];
      Value *V = Builder.CreateCall(makeDouble, {makeDoubleOpArg, lo, hi});
      resultElts[i] = V;
    }
  } else {
    for (unsigned i = 0; i < size; i++) {
      Value *lo = resultElts32[2 * i];
      Value *hi = resultElts32[2 * i + 1];
      lo = Builder.CreateZExt(lo, i64Ty);
      hi = Builder.CreateZExt(hi, i64Ty);
      hi = Builder.CreateShl(hi, 32);
      resultElts[i] = Builder.CreateOr(lo, hi);
    }
  }
}

// Split { v0, v1 } to { v0.lo, v0.hi, v1.lo, v1.hi }
void Split64bitValForStore(Type *EltTy, ArrayRef<Value *> vals, unsigned size,
                           MutableArrayRef<Value *> vals32, hlsl::OP *hlslOP,
                           IRBuilder<> &Builder) {
  Type *i32Ty = Builder.getInt32Ty();
  Type *doubleTy = Builder.getDoubleTy();
  Value *undefI32 = UndefValue::get(i32Ty);

  if (EltTy == doubleTy) {
    Function *dToU = hlslOP->GetOpFunc(DXIL::OpCode::SplitDouble, doubleTy);
    Value *dToUOpArg = Builder.getInt32((unsigned)DXIL::OpCode::SplitDouble);
    for (unsigned i = 0; i < size; i++) {
      if (isa<UndefValue>(vals[i])) {
        vals32[2 * i] = undefI32;
        vals32[2 * i + 1] = undefI32;
      } else {
        Value *retVal = Builder.CreateCall(dToU, {dToUOpArg, vals[i]});
        Value *lo = Builder.CreateExtractValue(retVal, 0);
        Value *hi = Builder.CreateExtractValue(retVal, 1);
        vals32[2 * i] = lo;
        vals32[2 * i + 1] = hi;
      }
    }
  } else {
    for (unsigned i = 0; i < size; i++) {
      if (isa<UndefValue>(vals[i])) {
        vals32[2 * i] = undefI32;
        vals32[2 * i + 1] = undefI32;
      } else {
        Value *lo = Builder.CreateTrunc(vals[i], i32Ty);
        Value *hi = Builder.CreateLShr(vals[i], 32);
        hi = Builder.CreateTrunc(hi, i32Ty);
        vals32[2 * i] = lo;
        vals32[2 * i + 1] = hi;
      }
    }
  }
}

class DxilTranslateRawBuffer : public ModulePass {
public:
  static char ID;
  explicit DxilTranslateRawBuffer() : ModulePass(ID) {}
  bool runOnModule(Module &M) {
    unsigned major, minor;
    DxilModule &DM = M.GetDxilModule();
    DM.GetDxilVersion(major, minor);
    OP *hlslOP = DM.GetOP();
    // Split 64bit for shader model less than 6.3.
    if (major == 1 && minor <= 2) {
      for (auto F = M.functions().begin(); F != M.functions().end();) {
        Function *func = &*(F++);
        DXIL::OpCodeClass opClass;
        if (hlslOP->GetOpCodeClass(func, opClass)) {
          if (opClass == DXIL::OpCodeClass::RawBufferLoad) {
            Type *ETy =
                hlslOP->GetOverloadType(DXIL::OpCode::RawBufferLoad, func);

            bool is64 =
                ETy->isDoubleTy() || ETy == Type::getInt64Ty(ETy->getContext());
            if (is64) {
              ReplaceRawBufferLoad64Bit(func, ETy, M);
              func->eraseFromParent();
            }
          } else if (opClass == DXIL::OpCodeClass::RawBufferStore) {
            Type *ETy =
                hlslOP->GetOverloadType(DXIL::OpCode::RawBufferStore, func);

            bool is64 =
                ETy->isDoubleTy() || ETy == Type::getInt64Ty(ETy->getContext());
            if (is64) {
              ReplaceRawBufferStore64Bit(func, ETy, M);
              func->eraseFromParent();
            }
          }
        }
      }
    }
    if (major == 1 && minor < 2) {
      for (auto F = M.functions().begin(), E = M.functions().end(); F != E;) {
        Function *func = &*(F++);
        if (func->hasName()) {
          if (func->getName().startswith("dx.op.rawBufferLoad")) {
            ReplaceRawBufferLoad(func, M);
            func->eraseFromParent();
          } else if (func->getName().startswith("dx.op.rawBufferStore")) {
            ReplaceRawBufferStore(func, M);
            func->eraseFromParent();
          }
        }
      }
    } else if (M.GetDxilModule().GetUseMinPrecision()) {
      for (auto F = M.functions().begin(), E = M.functions().end(); F != E;) {
        Function *func = &*(F++);
        if (func->hasName()) {
          if (func->getName().startswith("dx.op.rawBufferLoad")) {
            ReplaceMinPrecisionRawBufferLoad(func, M);
          } else if (func->getName().startswith("dx.op.rawBufferStore")) {
            ReplaceMinPrecisionRawBufferStore(func, M);
          }
        }
      }
    }
    return true;
  }

private:
  // Replace RawBufferLoad/Store to BufferLoad/Store for DXIL < 1.2
  void ReplaceRawBufferLoad(Function *F, Module &M);
  void ReplaceRawBufferStore(Function *F, Module &M);
  void ReplaceRawBufferLoad64Bit(Function *F, Type *EltTy, Module &M);
  void ReplaceRawBufferStore64Bit(Function *F, Type *EltTy, Module &M);
  // Replace RawBufferLoad/Store of min-precision types to have its actual storage size
  void ReplaceMinPrecisionRawBufferLoad(Function *F, Module &M);
  void ReplaceMinPrecisionRawBufferStore(Function *F, Module &M);
  void ReplaceMinPrecisionRawBufferLoadByType(Function *F, Type *FromTy,
                                              Type *ToTy, OP *Op,
                                              const DataLayout &DL);
};
} // namespace

void DxilTranslateRawBuffer::ReplaceRawBufferLoad(Function *F,
                                                                Module &M) {
  OP *op = M.GetDxilModule().GetOP();
  Type *RTy = F->getReturnType();
  if (StructType *STy = dyn_cast<StructType>(RTy)) {
    Type *ETy = STy->getElementType(0);
    Function *newFunction = op->GetOpFunc(hlsl::DXIL::OpCode::BufferLoad, ETy);
    for (auto U = F->user_begin(), E = F->user_end(); U != E;) {
      User *user = *(U++);
      if (CallInst *CI = dyn_cast<CallInst>(user)) {
        IRBuilder<> Builder(CI);
        SmallVector<Value *, 4> args;
        args.emplace_back(op->GetI32Const((unsigned)DXIL::OpCode::BufferLoad));
        for (unsigned i = 1; i < 4; ++i) {
          args.emplace_back(CI->getArgOperand(i));
        }
        CallInst *newCall = Builder.CreateCall(newFunction, args);
        CI->replaceAllUsesWith(newCall);
        CI->eraseFromParent();
      } else {
        DXASSERT(false, "function can only be used with call instructions.");
      }
    }
  } else {
    DXASSERT(false, "RawBufferLoad should return struct type.");
  }
}

void DxilTranslateRawBuffer::ReplaceRawBufferLoad64Bit(Function *F, Type *EltTy, Module &M) {
  OP *hlslOP = M.GetDxilModule().GetOP();
  Function *bufLd = hlslOP->GetOpFunc(DXIL::OpCode::RawBufferLoad,
                                      Type::getInt32Ty(M.getContext()));
  for (auto U = F->user_begin(), E = F->user_end(); U != E;) {
    User *user = *(U++);
    if (CallInst *CI = dyn_cast<CallInst>(user)) {
      IRBuilder<> Builder(CI);
      SmallVector<Value *, 4> args(CI->arg_operands());

      Value *offset = CI->getArgOperand(
          DXIL::OperandIndex::kRawBufferLoadElementOffsetOpIdx);

      unsigned size = 0;
      bool bNeedStatus = false;
      for (User *U : CI->users()) {
        ExtractValueInst *Elt = cast<ExtractValueInst>(U);
        DXASSERT(Elt->getNumIndices() == 1, "else invalid use for resRet");
        unsigned idx = Elt->getIndices()[0];
        if (idx == 4) {
          bNeedStatus = true;
        } else {
          size = std::max(size, idx+1);
        }
      }
      unsigned maskHi = 0;
      unsigned maskLo = 0;
      switch (size) {
      case 1:
        maskLo = 3;
        break;
      case 2:
        maskLo = 0xf;
        break;
      case 3:
        maskLo = 0xf;
        maskHi = 3;
        break;
      case 4:
        maskLo = 0xf;
        maskHi = 0xf;
        break;
      }

      args[DXIL::OperandIndex::kRawBufferLoadMaskOpIdx] =
          Builder.getInt8(maskLo);
      Value *resultElts[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
      CallInst *newLd = Builder.CreateCall(bufLd, args);

      Value *resultElts32[8];
      unsigned eltBase = 0;
      for (unsigned i = 0; i < size; i++) {
        if (i == 2) {
          // Update offset 4 by 4 bytes.
          args[DXIL::OperandIndex::kRawBufferLoadElementOffsetOpIdx] =
              Builder.CreateAdd(offset, Builder.getInt32(4 * 4));
          args[DXIL::OperandIndex::kRawBufferLoadMaskOpIdx] =
              Builder.getInt8(maskHi);
          newLd = Builder.CreateCall(bufLd, args);
          eltBase = 4;
        }
        unsigned resBase = 2 * i;
        resultElts32[resBase] =
            Builder.CreateExtractValue(newLd, resBase - eltBase);
        resultElts32[resBase + 1] =
            Builder.CreateExtractValue(newLd, resBase + 1 - eltBase);
      }

      Make64bitResultForLoad(EltTy, resultElts32, size, resultElts, hlslOP, Builder);
      if (bNeedStatus) {
        resultElts[4] = Builder.CreateExtractValue(newLd, 4);
      }
      for (auto it = CI->user_begin(); it != CI->user_end(); ) {
        ExtractValueInst *Elt = cast<ExtractValueInst>(*(it++));
        DXASSERT(Elt->getNumIndices() == 1, "else invalid use for resRet");
        unsigned idx = Elt->getIndices()[0];
        if (!Elt->user_empty()) {
          Value *newElt = resultElts[idx];
          Elt->replaceAllUsesWith(newElt);
        }
        Elt->eraseFromParent();
      }

      CI->eraseFromParent();
    } else {
      DXASSERT(false, "function can only be used with call instructions.");
    }
  }
}

void DxilTranslateRawBuffer::ReplaceRawBufferStore(Function *F,
  Module &M) {
  OP *op = M.GetDxilModule().GetOP();
  DXASSERT(F->getReturnType()->isVoidTy(), "rawBufferStore should return a void type.");
  Type *ETy = F->getFunctionType()->getParamType(4); // value
  Function *newFunction = op->GetOpFunc(hlsl::DXIL::OpCode::BufferStore, ETy);
  for (auto U = F->user_begin(), E = F->user_end(); U != E;) {
    User *user = *(U++);
    if (CallInst *CI = dyn_cast<CallInst>(user)) {
      IRBuilder<> Builder(CI);
      SmallVector<Value *, 4> args;
      args.emplace_back(op->GetI32Const((unsigned)DXIL::OpCode::BufferStore));
      for (unsigned i = 1; i < 9; ++i) {
        args.emplace_back(CI->getArgOperand(i));
      }
      Builder.CreateCall(newFunction, args);
      CI->eraseFromParent();
    }
    else {
      DXASSERT(false, "function can only be used with call instructions.");
    }
  }
}

void DxilTranslateRawBuffer::ReplaceRawBufferStore64Bit(Function *F, Type *ETy,
                                                        Module &M) {
  OP *hlslOP = M.GetDxilModule().GetOP();
  Function *newFunction = hlslOP->GetOpFunc(hlsl::DXIL::OpCode::RawBufferStore,
                                            Type::getInt32Ty(M.getContext()));
  for (auto U = F->user_begin(), E = F->user_end(); U != E;) {
    User *user = *(U++);
    if (CallInst *CI = dyn_cast<CallInst>(user)) {
      IRBuilder<> Builder(CI);
      SmallVector<Value *, 4> args(CI->arg_operands());
      Value *vals[4] = {
          CI->getArgOperand(DXIL::OperandIndex::kRawBufferStoreVal0OpIdx),
          CI->getArgOperand(DXIL::OperandIndex::kRawBufferStoreVal1OpIdx),
          CI->getArgOperand(DXIL::OperandIndex::kRawBufferStoreVal2OpIdx),
          CI->getArgOperand(DXIL::OperandIndex::kRawBufferStoreVal3OpIdx)};
      ConstantInt *cMask = cast<ConstantInt>(
          CI->getArgOperand(DXIL::OperandIndex::kRawBufferStoreMaskOpIdx));
      Value *undefI32 = UndefValue::get(Builder.getInt32Ty());
      Value *vals32[8] = {undefI32, undefI32, undefI32, undefI32,
                          undefI32, undefI32, undefI32, undefI32};

      unsigned maskLo = 0;
      unsigned maskHi = 0;
      unsigned size = 0;
      unsigned mask = cMask->getLimitedValue();
      switch (mask) {
      case 1:
        maskLo = 3;
        size = 1;
        break;
      case 3:
        maskLo = 15;
        size = 2;
        break;
      case 7:
        maskLo = 15;
        maskHi = 3;
        size = 3;
        break;
      case 15:
        maskLo = 15;
        maskHi = 15;
        size = 4;
        break;
      default:
        DXASSERT(0, "invalid mask");
      }

      Split64bitValForStore(ETy, vals, size, vals32, hlslOP, Builder);
      args[DXIL::OperandIndex::kRawBufferStoreMaskOpIdx] =
          Builder.getInt8(maskLo);
      args[DXIL::OperandIndex::kRawBufferStoreVal0OpIdx] = vals32[0];
      args[DXIL::OperandIndex::kRawBufferStoreVal1OpIdx] = vals32[1];
      args[DXIL::OperandIndex::kRawBufferStoreVal2OpIdx] = vals32[2];
      args[DXIL::OperandIndex::kRawBufferStoreVal3OpIdx] = vals32[3];

      Builder.CreateCall(newFunction, args);

      if (maskHi) {
        Value *offset = args[DXIL::OperandIndex::kBufferStoreCoord1OpIdx];
        // Update offset 4 by 4 bytes.
        offset = Builder.CreateAdd(offset, Builder.getInt32(4 * 4));
        args[DXIL::OperandIndex::kRawBufferStoreElementOffsetOpIdx] = offset;
        args[DXIL::OperandIndex::kRawBufferStoreMaskOpIdx] =
            Builder.getInt8(maskHi);
        args[DXIL::OperandIndex::kRawBufferStoreVal0OpIdx] = vals32[4];
        args[DXIL::OperandIndex::kRawBufferStoreVal1OpIdx] = vals32[5];
        args[DXIL::OperandIndex::kRawBufferStoreVal2OpIdx] = vals32[6];
        args[DXIL::OperandIndex::kRawBufferStoreVal3OpIdx] = vals32[7];

        Builder.CreateCall(newFunction, args);
      }
      CI->eraseFromParent();
    } else {
      DXASSERT(false, "function can only be used with call instructions.");
    }
  }
}

void DxilTranslateRawBuffer::ReplaceMinPrecisionRawBufferLoad(Function *F,
                                                              Module &M) {
  OP *Op = M.GetDxilModule().GetOP();
  Type *RetTy = F->getReturnType();
  if (StructType *STy = dyn_cast<StructType>(RetTy)) {
    Type *EltTy = STy->getElementType(0);
    if (EltTy->isHalfTy()) {
      ReplaceMinPrecisionRawBufferLoadByType(F, Type::getHalfTy(M.getContext()),
                                             Type::getFloatTy(M.getContext()),
                                             Op, M.getDataLayout());
    } else if (EltTy == Type::getInt16Ty(M.getContext())) {
      ReplaceMinPrecisionRawBufferLoadByType(
          F, Type::getInt16Ty(M.getContext()), Type::getInt32Ty(M.getContext()),
          Op, M.getDataLayout());
    }
  } else {
    DXASSERT(false, "RawBufferLoad should return struct type.");
  }
}

void DxilTranslateRawBuffer::ReplaceMinPrecisionRawBufferStore(Function *F,
                                                              Module &M) {
  DXASSERT(F->getReturnType()->isVoidTy(), "rawBufferStore should return a void type.");
  Type *ETy = F->getFunctionType()->getParamType(4); // value
  Type *NewETy;
  if (ETy->isHalfTy()) {
    NewETy = Type::getFloatTy(M.getContext());
  }
  else if (ETy == Type::getInt16Ty(M.getContext())) {
    NewETy = Type::getInt32Ty(M.getContext());
  }
  else {
    return; // not a min precision type
  }
  Function *newFunction = M.GetDxilModule().GetOP()->GetOpFunc(
      DXIL::OpCode::RawBufferStore, NewETy);
  // for each function
  // add argument 4-7 to its upconverted values
  // replace function call
  for (auto FuncUser = F->user_begin(), FuncEnd = F->user_end(); FuncUser != FuncEnd;) {
    CallInst *CI = dyn_cast<CallInst>(*(FuncUser++));
    DXASSERT(CI, "function user must be a call instruction.");
    IRBuilder<> CIBuilder(CI);
    SmallVector<Value *, 9> Args;
    for (unsigned i = 0; i < 4; ++i) {
      Args.emplace_back(CI->getArgOperand(i));
    }
    // values to store should be converted to its higher precision types
    if (ETy->isHalfTy()) {
      for (unsigned i = 4; i < 8; ++i) {
        Value *NewV = CIBuilder.CreateFPExt(CI->getArgOperand(i),
                                            Type::getFloatTy(M.getContext()));
        Args.emplace_back(NewV);
      }
    }
    else if (ETy == Type::getInt16Ty(M.getContext())) {
      // This case only applies to typed buffer since Store operation of byte
      // address buffer for min precision is handled by implicit conversion on
      // intrinsic call. Since we are extending integer, we have to know if we
      // should sign ext or zero ext. We can do this by iterating checking the
      // size of the element at struct type and comp type at type annotation
      CallInst *handleCI = dyn_cast<CallInst>(CI->getArgOperand(1));
      DXASSERT(handleCI, "otherwise handle was not an argument to buffer store.");
      ConstantInt *resClass = dyn_cast<ConstantInt>(handleCI->getArgOperand(1));
      DXASSERT_LOCALVAR(resClass, resClass && resClass->getSExtValue() ==
                               (unsigned)DXIL::ResourceClass::UAV,
               "otherwise buffer store called on non uav kind.");
      ConstantInt *rangeID = dyn_cast<ConstantInt>(handleCI->getArgOperand(2)); // range id or idx?
      DXASSERT(rangeID, "wrong createHandle call.");
      DxilResource dxilRes = M.GetDxilModule().GetUAV(rangeID->getSExtValue());
      StructType *STy = dyn_cast<StructType>(dxilRes.GetRetType());
      DxilStructAnnotation *SAnnot = M.GetDxilModule().GetTypeSystem().GetStructAnnotation(STy);
      ConstantInt *offsetInt = dyn_cast<ConstantInt>(CI->getArgOperand(3));
      unsigned offset = offsetInt->getSExtValue();
      unsigned currentOffset = 0;
      for (DxilStructTypeIterator iter = begin(STy, SAnnot), ItEnd = end(STy, SAnnot); iter != ItEnd; ++iter) {
        std::pair<Type *, DxilFieldAnnotation*> pair = *iter;
        currentOffset += M.getDataLayout().getTypeAllocSize(pair.first);
        if (currentOffset > offset) {
          if (pair.second->GetCompType().IsUIntTy()) {
            for (unsigned i = 4; i < 8; ++i) {
              Value *NewV = CIBuilder.CreateZExt(CI->getArgOperand(i), Type::getInt32Ty(M.getContext()));
              Args.emplace_back(NewV);
            }
            break;
          }
          else if (pair.second->GetCompType().IsIntTy()) {
            for (unsigned i = 4; i < 8; ++i) {
              Value *NewV = CIBuilder.CreateSExt(CI->getArgOperand(i), Type::getInt32Ty(M.getContext()));
              Args.emplace_back(NewV);
            }
            break;
          }
          else {
            DXASSERT(false, "Invalid comp type");
          }
        }
      }
    }

    // mask
    Args.emplace_back(CI->getArgOperand(8));
    // alignment
    Args.emplace_back(M.GetDxilModule().GetOP()->GetI32Const(
        M.getDataLayout().getTypeAllocSize(NewETy)));
    CIBuilder.CreateCall(newFunction, Args);
    CI->eraseFromParent();
   }
}


void DxilTranslateRawBuffer::ReplaceMinPrecisionRawBufferLoadByType(
    Function *F, Type *FromTy, Type *ToTy, OP *Op, const DataLayout &DL) {
  Function *newFunction = Op->GetOpFunc(DXIL::OpCode::RawBufferLoad, ToTy);
  for (auto FUser = F->user_begin(), FEnd = F->user_end(); FUser != FEnd;) {
    User *UserCI = *(FUser++);
    if (CallInst *CI = dyn_cast<CallInst>(UserCI)) {
      IRBuilder<> CIBuilder(CI);
      SmallVector<Value *, 5> newFuncArgs;
      // opcode, handle, index, elementOffset, mask
      // Compiler is generating correct element offset even for min precision types
      // So no need to recalculate here
      for (unsigned i = 0; i < 5; ++i) {
        newFuncArgs.emplace_back(CI->getArgOperand(i));
      }
      // new alignment for new type
      newFuncArgs.emplace_back(Op->GetI32Const(DL.getTypeAllocSize(ToTy)));
      CallInst *newCI = CIBuilder.CreateCall(newFunction, newFuncArgs);
      for (auto CIUser = CI->user_begin(), CIEnd = CI->user_end();
           CIUser != CIEnd;) {
        User *UserEV = *(CIUser++);
        if (ExtractValueInst *EV = dyn_cast<ExtractValueInst>(UserEV)) {
          IRBuilder<> EVBuilder(EV);
          ArrayRef<unsigned> Indices = EV->getIndices();
          DXASSERT(Indices.size() == 1, "Otherwise we have wrong extract value.");
          Value *newEV = EVBuilder.CreateExtractValue(newCI, Indices);
          Value *newTruncV = nullptr;
          if (4 == Indices[0]) { // Don't truncate status
            newTruncV = newEV;
          }
          else if (FromTy->isHalfTy()) {
            newTruncV = EVBuilder.CreateFPTrunc(newEV, FromTy);
          } else if (FromTy->isIntegerTy()) {
            newTruncV = EVBuilder.CreateTrunc(newEV, FromTy);
          } else {
            DXASSERT(false, "unexpected type conversion");
          }
          EV->replaceAllUsesWith(newTruncV);
          EV->eraseFromParent();
        }
      }
      CI->eraseFromParent();
    }
  }
  F->eraseFromParent();
}

char DxilTranslateRawBuffer::ID = 0;
ModulePass *llvm::createDxilTranslateRawBuffer() {
  return new DxilTranslateRawBuffer();
}

INITIALIZE_PASS(DxilTranslateRawBuffer, "hlsl-translate-dxil-raw-buffer",
                "Translate raw buffer load", false, false)
