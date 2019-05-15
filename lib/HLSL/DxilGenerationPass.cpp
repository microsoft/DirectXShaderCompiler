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

#include "HLSignatureLower.h"
#include "dxc/DXIL/DxilEntryProps.h"
#include "dxc/DXIL/DxilModule.h"
#include "dxc/DXIL/DxilOperations.h"
#include "dxc/DXIL/DxilUtil.h"
#include "dxc/HLSL/DxilGenerationPass.h"
#include "dxc/HLSL/HLSLExtensionsCodegenHelper.h"
#include "dxc/HLSL/HLModule.h"
#include "dxc/HLSL/HLOperationLower.h"
#include "dxc/HLSL/HLOperations.h"
#include "dxc/Support/Global.h"
#include "llvm/Pass.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Casting.h"
#include "llvm/Transforms/Utils/SSAUpdater.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace llvm;
using namespace hlsl;

// TODO: use hlsl namespace for the most of this file.

namespace {

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
    if (GlobalVariable *GV = dyn_cast<GlobalVariable>(b->GetGlobalSymbol()))
      LLVMUsed.emplace_back(GV);
    M.AddCBuffer(std::move(b));
  }
  for (auto && C : H.GetUAVs()) {
    auto b = llvm::make_unique<DxilResource>();
    InitResource(C.get(), b.get());
    if (GlobalVariable *GV = dyn_cast<GlobalVariable>(b->GetGlobalSymbol()))
      LLVMUsed.emplace_back(GV);
    M.AddUAV(std::move(b));
  }
  for (auto && C : H.GetSRVs()) {
    auto b = llvm::make_unique<DxilResource>();
    InitResource(C.get(), b.get());
    if (GlobalVariable *GV = dyn_cast<GlobalVariable>(b->GetGlobalSymbol()))
      LLVMUsed.emplace_back(GV);
    M.AddSRV(std::move(b));
  }
  for (auto && C : H.GetSamplers()) {
    auto b = llvm::make_unique<DxilSampler>();
    InitResourceBase(C.get(), b.get());
    b->SetSamplerKind(C->GetSamplerKind());
    if (GlobalVariable *GV = dyn_cast<GlobalVariable>(b->GetGlobalSymbol()))
      LLVMUsed.emplace_back(GV);
    M.AddSampler(std::move(b));
  }

  // Signatures.
  M.ResetSerializedRootSignature(H.GetSerializedRootSignature());

  // Subobjects.
  M.ResetSubobjects(H.ReleaseSubobjects());

  // Shader properties.
  //bool m_bDisableOptimizations;
  M.SetDisableOptimization(H.GetHLOptions().bDisableOptimizations);
  M.SetLegacyResourceReservation(H.GetHLOptions().bLegacyResourceReservation);
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

    GenerateDxilOperations(M, UpdateCounterSet);

    GenerateDxilCBufferHandles();
    MarkUpdateCounter(UpdateCounterSet);
    LowerHLCreateHandle();

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
    DxilFunctionProps *pProps = nullptr;
    if (!SM->IsLib()) {
      pProps = &EntryPropsMap.begin()->second->props;
    }
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
  GenerateDxilCBufferHandles();

  // change built-in funtion into DXIL operations
  void GenerateDxilOperations(Module &M,
                              std::unordered_set<LoadInst *> &UpdateCounterSet);
  void LowerHLCreateHandle();

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
  Value *V = res.GetGlobalSymbol();
  for (auto U = V->user_begin(), E = V->user_end(); U != E;) {
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

void DxilGenerationPass::GenerateDxilCBufferHandles() {
  // For CBuffer, handle are mapped to HLCreateHandle.
  OP *hlslOP = m_pHLModule->GetOP();
  Value *opArg = hlslOP->GetU32Const((unsigned)OP::OpCode::CreateHandleForLib);
  LLVMContext &Ctx = hlslOP->GetCtx();
  Value *zeroIdx = hlslOP->GetU32Const(0);

  for (size_t i = 0; i < m_pHLModule->GetCBuffers().size(); i++) {
    DxilCBuffer &CB = m_pHLModule->GetCBuffer(i);
    GlobalVariable *GV = dyn_cast<GlobalVariable>(CB.GetGlobalSymbol());
    if (GV == nullptr)
      continue;

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
        Value *V = Builder.CreateLoad(GEP);
        CallInst *handle = Builder.CreateCall(createHandle, {opArg, V}, handleName);
        CI->replaceAllUsesWith(handle);
        CI->eraseFromParent();
      }
    }
  } 
}

void DxilGenerationPass::GenerateDxilOperations(
    Module &M, std::unordered_set<LoadInst *> &UpdateCounterSet) {
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
                             UpdateCounterSet);

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
