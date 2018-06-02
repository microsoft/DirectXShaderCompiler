///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilLinker.cpp                                                           //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/HLSL/DxilLinker.h"
#include "dxc/HLSL/DxilCBuffer.h"
#include "dxc/HLSL/DxilFunctionProps.h"
#include "dxc/HLSL/DxilModule.h"
#include "dxc/HLSL/DxilOperations.h"
#include "dxc/HLSL/DxilResource.h"
#include "dxc/HLSL/DxilSampler.h"
#include "dxc/Support/Global.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/ADT/StringMap.h"
#include <memory>
#include <vector>

#include "dxc/HLSL/DxilContainer.h"
#include "llvm/IR/DiagnosticPrinter.h"
#include "llvm/IR/LLVMContext.h"

#include "dxc/HLSL/DxilGenerationPass.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/Scalar.h"

using namespace llvm;
using namespace hlsl;

namespace {

void CollectUsedFunctions(Constant *C,
                          std::unordered_set<Function *> &funcSet) {
  for (User *U : C->users()) {
    if (Instruction *I = dyn_cast<Instruction>(U)) {
      funcSet.insert(I->getParent()->getParent());
    } else {
      Constant *CU = cast<Constant>(U);
      CollectUsedFunctions(CU, funcSet);
    }
  }
}

template <class T>
void AddResourceMap(
    const std::vector<std::unique_ptr<T>> &resTab, DXIL::ResourceClass resClass,
    std::unordered_map<const llvm::Constant *, DxilResourceBase *> &resMap,
    DxilModule &DM) {
  for (auto &Res : resTab) {
    const DxilModule::ResourceLinkInfo &linkInfo =
        DM.GetResourceLinkInfo(resClass, Res->GetID());
    resMap[linkInfo.ResRangeID] = Res.get();
  }
}

void CloneFunction(Function *F, Function *NewF, ValueToValueMapTy &vmap) {
  SmallVector<ReturnInst *, 2> Returns;
  // Map params.
  auto paramIt = NewF->arg_begin();
  for (Argument &param : F->args()) {
    vmap[&param] = (paramIt++);
  }

  llvm::CloneFunctionInto(NewF, F, vmap, /*ModuleLevelChanges*/ true, Returns);

  // Remove params from vmap.
  for (Argument &param : F->args()) {
    vmap.erase(&param);
  }
}

} // namespace

namespace {

struct DxilFunctionLinkInfo {
  DxilFunctionLinkInfo(llvm::Function *F);
  llvm::Function *func;
  std::unordered_set<llvm::Function *> usedFunctions;
  std::unordered_set<llvm::GlobalVariable *> usedGVs;
  std::unordered_set<DxilResourceBase *> usedResources;
};

// Library to link.
class DxilLib {

public:
  DxilLib(std::unique_ptr<llvm::Module> pModule);
  virtual ~DxilLib() {}
  bool HasFunction(std::string &name);
  llvm::StringMap<std::unique_ptr<DxilFunctionLinkInfo>> &GetFunctionTable() {
    return m_functionNameMap;
  }
  bool IsInitFunc(llvm::Function *F);
  bool IsResourceGlobal(const llvm::Constant *GV);
  DxilResourceBase *GetResource(const llvm::Constant *GV);

  DxilModule &GetDxilModule() { return m_DM; }
  void LazyLoadFunction(Function *F);
  void BuildGlobalUsage();
  void CollectUsedInitFunctions(StringSet<> &addedFunctionSet,
                                SmallVector<StringRef, 4> &workList);

private:
  std::unique_ptr<llvm::Module> m_pModule;
  DxilModule &m_DM;
  // Map from name to Link info for extern functions.
  llvm::StringMap<std::unique_ptr<DxilFunctionLinkInfo>> m_functionNameMap;
  // Map from resource link global to resource.
  std::unordered_map<const llvm::Constant *, DxilResourceBase *> m_resourceMap;
  // Set of initialize functions for global variable.
  std::unordered_set<llvm::Function *> m_initFuncSet;
};

struct DxilLinkJob;

class DxilLinkerImpl : public hlsl::DxilLinker {
public:
  DxilLinkerImpl(LLVMContext &Ctx, unsigned valMajor, unsigned valMinor) : DxilLinker(Ctx, valMajor, valMinor) {}
  virtual ~DxilLinkerImpl() {}
  bool HasLibNameRegistered(StringRef name) override;
  bool RegisterLib(StringRef name, std::unique_ptr<llvm::Module> pModule,
                   std::unique_ptr<llvm::Module> pDebugModule) override;
  bool AttachLib(StringRef name) override;
  bool DetachLib(StringRef name) override;
  void DetachAll() override;

  std::unique_ptr<llvm::Module> Link(StringRef entry,
                                     StringRef profile) override;

private:
  bool AttachLib(DxilLib *lib);
  bool DetachLib(DxilLib *lib);
  bool AddFunctions(SmallVector<StringRef, 4> &workList,
                    DenseSet<DxilLib *> &libSet, StringSet<> &addedFunctionSet,
                    DxilLinkJob &linkJob, bool bLazyLoadDone);
  // Attached libs to link.
  std::unordered_set<DxilLib *> m_attachedLibs;
  // Owner of all DxilLib.
  StringMap<std::unique_ptr<DxilLib>> m_LibMap;
  llvm::StringMap<std::pair<DxilFunctionLinkInfo *, DxilLib *>>
      m_functionNameMap;
};

} // namespace

//------------------------------------------------------------------------------
//
// DxilFunctionLinkInfo methods.
//
DxilFunctionLinkInfo::DxilFunctionLinkInfo(Function *F) : func(F) {
  DXASSERT_NOMSG(F);
}

//------------------------------------------------------------------------------
//
// DxilLib methods.
//

DxilLib::DxilLib(std::unique_ptr<llvm::Module> pModule)
    : m_pModule(std::move(pModule)), m_DM(m_pModule->GetOrCreateDxilModule()) {
  Module &M = *m_pModule;
  const std::string &MID = M.getModuleIdentifier();

  // Collect function defines.
  for (Function &F : M.functions()) {
    if (F.isDeclaration())
      continue;
    if (F.getLinkage() == GlobalValue::LinkageTypes::InternalLinkage) {
      // Add prefix to internal function.
      F.setName(MID + F.getName());
    }
    m_functionNameMap[F.getName()] =
        llvm::make_unique<DxilFunctionLinkInfo>(&F);
  }

  // Update internal global name.
  for (GlobalVariable &GV : M.globals()) {
    if (GV.getLinkage() == GlobalValue::LinkageTypes::InternalLinkage) {
      // Add prefix to internal global.
      GV.setName(MID + GV.getName());
    }
  }
}

void DxilLib::LazyLoadFunction(Function *F) {
  DXASSERT(m_functionNameMap.count(F->getName()), "else invalid Function");
  DxilFunctionLinkInfo *linkInfo = m_functionNameMap[F->getName()].get();
  std::error_code EC = F->materialize();
  DXASSERT_LOCALVAR(EC, !EC, "else fail to materialize");

  // Build used functions for F.
  for (auto &BB : F->getBasicBlockList()) {
    for (auto &I : BB.getInstList()) {
      if (CallInst *CI = dyn_cast<CallInst>(&I)) {
        linkInfo->usedFunctions.insert(CI->getCalledFunction());
      }
    }
  }

  if (m_DM.HasDxilFunctionProps(F)) {
    DxilFunctionProps &props = m_DM.GetDxilFunctionProps(F);
    if (props.IsHS()) {
      // Add patch constant function to usedFunctions of entry.
      Function *patchConstantFunc = props.ShaderProps.HS.patchConstantFunc;
      linkInfo->usedFunctions.insert(patchConstantFunc);
    }
  }
  // Used globals will be build before link.
}

void DxilLib::BuildGlobalUsage() {
  Module &M = *m_pModule;

  // Collect init functions for static globals.
  if (GlobalVariable *Ctors = M.getGlobalVariable("llvm.global_ctors")) {
    if (ConstantArray *CA = dyn_cast<ConstantArray>(Ctors->getInitializer())) {
      for (User::op_iterator i = CA->op_begin(), e = CA->op_end(); i != e;
           ++i) {
        if (isa<ConstantAggregateZero>(*i))
          continue;
        ConstantStruct *CS = cast<ConstantStruct>(*i);
        if (isa<ConstantPointerNull>(CS->getOperand(1)))
          continue;

        // Must have a function or null ptr.
        if (!isa<Function>(CS->getOperand(1)))
          continue;
        Function *Ctor = cast<Function>(CS->getOperand(1));
        assert(Ctor->getReturnType()->isVoidTy() && Ctor->arg_size() == 0 &&
               "function type must be void (void)");
        // Add Ctor.
        m_initFuncSet.insert(Ctor);
        LazyLoadFunction(Ctor);
      }
    }
  }

  // Build used globals.
  for (GlobalVariable &GV : M.globals()) {
    std::unordered_set<Function *> funcSet;
    CollectUsedFunctions(&GV, funcSet);
    for (Function *F : funcSet) {
      DXASSERT(m_functionNameMap.count(F->getName()), "must exist in table");
      DxilFunctionLinkInfo *linkInfo = m_functionNameMap[F->getName()].get();
      linkInfo->usedGVs.insert(&GV);
    }
  }

  // Build resource map.
  AddResourceMap(m_DM.GetUAVs(), DXIL::ResourceClass::UAV, m_resourceMap, m_DM);
  AddResourceMap(m_DM.GetSRVs(), DXIL::ResourceClass::SRV, m_resourceMap, m_DM);
  AddResourceMap(m_DM.GetCBuffers(), DXIL::ResourceClass::CBuffer,
                 m_resourceMap, m_DM);
  AddResourceMap(m_DM.GetSamplers(), DXIL::ResourceClass::Sampler,
                 m_resourceMap, m_DM);
}

void DxilLib::CollectUsedInitFunctions(StringSet<> &addedFunctionSet,
                                       SmallVector<StringRef, 4> &workList) {
  // Add init functions to used functions.
  for (Function *Ctor : m_initFuncSet) {
    DXASSERT(m_functionNameMap.count(Ctor->getName()),
             "must exist in internal table");
    DxilFunctionLinkInfo *linkInfo = m_functionNameMap[Ctor->getName()].get();
    // If function other than Ctor used GV of Ctor.
    // Add Ctor to usedFunctions for it.
    for (GlobalVariable *GV : linkInfo->usedGVs) {
      std::unordered_set<Function *> funcSet;
      CollectUsedFunctions(GV, funcSet);
      bool bAdded = false;
      for (Function *F : funcSet) {
        if (F == Ctor)
          continue;
        // If F is added for link, add init func to workList.
        if (addedFunctionSet.count(F->getName())) {
          workList.emplace_back(Ctor->getName());
          bAdded = true;
          break;
        }
      }
      if (bAdded)
        break;
    }
  }
}

bool DxilLib::HasFunction(std::string &name) {
  return m_functionNameMap.count(name);
}

bool DxilLib::IsInitFunc(llvm::Function *F) { return m_initFuncSet.count(F); }
bool DxilLib::IsResourceGlobal(const llvm::Constant *GV) {
  return m_resourceMap.count(GV);
}
DxilResourceBase *DxilLib::GetResource(const llvm::Constant *GV) {
  if (IsResourceGlobal(GV))
    return m_resourceMap[GV];
  else
    return nullptr;
}


namespace {
// Create module from link defines.
struct DxilLinkJob {
  DxilLinkJob(LLVMContext &Ctx, unsigned valMajor, unsigned valMinor) : m_ctx(Ctx), m_valMajor(valMajor), m_valMinor(valMinor) {}
  std::unique_ptr<llvm::Module>
  Link(std::pair<DxilFunctionLinkInfo *, DxilLib *> &entryLinkPair,
       StringRef profile);
  void RunPreparePass(llvm::Module &M);
  void AddFunction(std::pair<DxilFunctionLinkInfo *, DxilLib *> &linkPair);
  void AddFunction(llvm::Function *F);

private:
  bool AddResource(DxilResourceBase *res, llvm::GlobalVariable *GV);
  void AddResourceToDM(DxilModule &DM);
  std::unordered_map<DxilFunctionLinkInfo *, DxilLib *> m_functionDefs;
  llvm::StringMap<llvm::Function *> m_dxilFunctions;
  // New created functions.
  llvm::StringMap<llvm::Function *> m_newFunctions;
  // New created globals.
  llvm::StringMap<llvm::GlobalVariable *> m_newGlobals;
  // Map for resource.
  llvm::StringMap<std::pair<DxilResourceBase *, llvm::GlobalVariable *>>
      m_resourceMap;
  LLVMContext &m_ctx;
  unsigned m_valMajor, m_valMinor;
};
} // namespace

namespace {
const char kUndefFunction[] = "Cannot find definition of function ";
const char kRedefineFunction[] = "Definition already exists for function ";
const char kRedefineGlobal[] = "Definition already exists for global variable ";
const char kInvalidProfile[] = " is invalid profile to link";
const char kShaderKindMismatch[] =
    "Profile mismatch between entry function and target profile:";
const char kNoEntryProps[] =
    "Cannot find function property for entry function ";
const char kRedefineResource[] =
    "Resource already exists as ";
const char kInvalidValidatorVersion[] = "Validator version does not support target profile ";
} // namespace
//------------------------------------------------------------------------------
//
// DxilLinkJob methods.
//

namespace {
// Helper function to check type match.
bool IsMatchedType(Type *Ty0, Type *Ty);

StringRef RemoveNameSuffix(StringRef Name) {
  size_t DotPos = Name.rfind('.');
  if (DotPos != StringRef::npos && Name.back() != '.' &&
      isdigit(static_cast<unsigned char>(Name[DotPos + 1])))
    Name = Name.substr(0, DotPos);
  return Name;
}

bool IsMatchedStructType(StructType *ST0, StructType *ST) {
  StringRef Name0 = RemoveNameSuffix(ST0->getName());
  StringRef Name = RemoveNameSuffix(ST->getName());

  if (Name0 != Name)
    return false;

  if (ST0->getNumElements() != ST->getNumElements())
    return false;

  if (ST0->isLayoutIdentical(ST))
    return true;

  for (unsigned i = 0; i < ST->getNumElements(); i++) {
    Type *Ty = ST->getElementType(i);
    Type *Ty0 = ST0->getElementType(i);
    if (!IsMatchedType(Ty, Ty0))
      return false;
  }
  return true;
}

bool IsMatchedArrayType(ArrayType *AT0, ArrayType *AT) {
  if (AT0->getNumElements() != AT->getNumElements())
    return false;
  return IsMatchedType(AT0->getElementType(), AT->getElementType());
}

bool IsMatchedType(Type *Ty0, Type *Ty) {
  if (Ty0->isStructTy() && Ty->isStructTy()) {
    StructType *ST0 = cast<StructType>(Ty0);
    StructType *ST = cast<StructType>(Ty);
    return IsMatchedStructType(ST0, ST);
  }

  if (Ty0->isArrayTy() && Ty->isArrayTy()) {
    ArrayType *AT0 = cast<ArrayType>(Ty0);
    ArrayType *AT = cast<ArrayType>(Ty);
    return IsMatchedArrayType(AT0, AT);
  }

  if (Ty0->isPointerTy() && Ty->isPointerTy()) {
    if (Ty0->getPointerAddressSpace() != Ty->getPointerAddressSpace())
      return false;

    return IsMatchedType(Ty0->getPointerElementType(),
                         Ty->getPointerElementType());
  }

  return Ty0 == Ty;
}
} // namespace

bool DxilLinkJob::AddResource(DxilResourceBase *res, llvm::GlobalVariable *GV) {
  if (m_resourceMap.count(res->GetGlobalName())) {
    DxilResourceBase *res0 = m_resourceMap[res->GetGlobalName()].first;
    Type *Ty0 = res0->GetGlobalSymbol()->getType()->getPointerElementType();
    Type *Ty = res->GetGlobalSymbol()->getType()->getPointerElementType();
    // Make sure res0 match res.
    bool bMatch = IsMatchedType(Ty0, Ty);
    if (!bMatch) {
      // Report error.
      m_ctx.emitError(Twine(kRedefineResource) + res->GetResClassName() + " for " +
                      res->GetGlobalName());
      return false;
    }
  } else {
    m_resourceMap[res->GetGlobalName()] = std::make_pair(res, GV);
  }
  return true;
}

void DxilLinkJob::AddResourceToDM(DxilModule &DM) {
  for (auto &it : m_resourceMap) {
    DxilResourceBase *res = it.second.first;
    GlobalVariable *GV = it.second.second;
    unsigned ID = 0;
    DxilResourceBase *basePtr = nullptr;
    switch (res->GetClass()) {
    case DXIL::ResourceClass::UAV: {
      std::unique_ptr<DxilResource> pUAV = llvm::make_unique<DxilResource>();
      DxilResource *ptr = pUAV.get();
      // Copy the content.
      *ptr = *(static_cast<DxilResource *>(res));
      ID = DM.AddUAV(std::move(pUAV));
      basePtr = &DM.GetUAV(ID);
    } break;
    case DXIL::ResourceClass::SRV: {
      std::unique_ptr<DxilResource> pSRV = llvm::make_unique<DxilResource>();
      DxilResource *ptr = pSRV.get();
      // Copy the content.
      *ptr = *(static_cast<DxilResource *>(res));
      ID = DM.AddSRV(std::move(pSRV));
      basePtr = &DM.GetSRV(ID);
    } break;
    case DXIL::ResourceClass::CBuffer: {
      std::unique_ptr<DxilCBuffer> pCBuf = llvm::make_unique<DxilCBuffer>();
      DxilCBuffer *ptr = pCBuf.get();
      // Copy the content.
      *ptr = *(static_cast<DxilCBuffer *>(res));
      ID = DM.AddCBuffer(std::move(pCBuf));
      basePtr = &DM.GetCBuffer(ID);
    } break;
    case DXIL::ResourceClass::Sampler: {
      std::unique_ptr<DxilSampler> pSampler = llvm::make_unique<DxilSampler>();
      DxilSampler *ptr = pSampler.get();
      // Copy the content.
      *ptr = *(static_cast<DxilSampler *>(res));
      ID = DM.AddSampler(std::move(pSampler));
      basePtr = &DM.GetSampler(ID);
    }
    default:
      DXASSERT(res->GetClass() == DXIL::ResourceClass::Sampler,
               "else invalid resource");
      break;
    }
    // Update ID.
    basePtr->SetID(ID);
    Constant *rangeID = ConstantInt::get(GV->getType()->getElementType(), ID);
    for (User *U : GV->users()) {
      LoadInst *LI = cast<LoadInst>(U);
      LI->replaceAllUsesWith(rangeID);
    }
  }
}

std::unique_ptr<Module>
DxilLinkJob::Link(std::pair<DxilFunctionLinkInfo *, DxilLib *> &entryLinkPair,
                  StringRef profile) {

  Function *entryFunc = entryLinkPair.first->func;
  DxilModule &entryDM = entryLinkPair.second->GetDxilModule();
  if (!entryDM.HasDxilFunctionProps(entryFunc)) {
    // Cannot get function props.
    m_ctx.emitError(Twine(kNoEntryProps) + entryFunc->getName());
    return nullptr;
  }

  DxilFunctionProps props = entryDM.GetDxilFunctionProps(entryFunc);
  if (props.shaderKind == DXIL::ShaderKind::Library ||
      props.shaderKind == DXIL::ShaderKind::Invalid) {
    m_ctx.emitError(profile + Twine(kInvalidProfile));
    // Invalid profile.
    return nullptr;
  }

  const ShaderModel *pSM = ShaderModel::GetByName(profile.data());
  if (pSM->GetKind() != props.shaderKind) {
    // Shader kind mismatch.
    m_ctx.emitError(Twine(kShaderKindMismatch) + profile + " and " +
                    ShaderModel::GetKindName(props.shaderKind));
    return nullptr;
  }

  // Create new module.
  std::unique_ptr<Module> pM =
      llvm::make_unique<Module>(entryFunc->getName(), entryDM.GetCtx());
  // Set target.
  pM->setTargetTriple(entryDM.GetModule()->getTargetTriple());
  // Add dxil operation functions before create DxilModule.
  for (auto &it : m_dxilFunctions) {
    Function *F = it.second;
    Function *NewF = Function::Create(F->getFunctionType(), F->getLinkage(),
                                      F->getName(), pM.get());
    NewF->setAttributes(F->getAttributes());
    m_newFunctions[NewF->getName()] = NewF;
  }

  // Create DxilModule.
  const bool bSkipInit = true;
  DxilModule &DM = pM->GetOrCreateDxilModule(bSkipInit);
  DM.SetShaderModel(pSM);

  // Set Validator version, verifying that it supports the requested profile
  unsigned minValMajor, minValMinor;
  DM.GetMinValidatorVersion(minValMajor, minValMinor);
  if (minValMajor > m_valMajor || (minValMajor == m_valMajor && minValMinor > m_valMinor)) {
    m_ctx.emitError(Twine(kInvalidValidatorVersion) + profile);
    return nullptr;
  }
  DM.SetValidatorVersion(m_valMajor, m_valMinor);

  // Add type sys
  DxilTypeSystem &typeSys = DM.GetTypeSystem();

  ValueToValueMapTy vmap;

  std::unordered_set<Function *> initFuncSet;
  // Add function
  for (auto &it : m_functionDefs) {
    DxilFunctionLinkInfo *linkInfo = it.first;
    DxilLib *pLib = it.second;
    DxilModule &tmpDM = pLib->GetDxilModule();
    DxilTypeSystem &tmpTypeSys = tmpDM.GetTypeSystem();

    Function *F = linkInfo->func;
    Function *NewF = Function::Create(F->getFunctionType(), F->getLinkage(),
                                      F->getName(), pM.get());
    NewF->setAttributes(F->getAttributes());

    if (!NewF->hasFnAttribute(llvm::Attribute::NoInline))
      NewF->addFnAttr(llvm::Attribute::AlwaysInline);

    if (tmpTypeSys.GetFunctionAnnotation(F)) {
      // Clone funcAnnotation to typeSys.
      typeSys.CopyFunctionAnnotation(NewF, F, tmpTypeSys);
    }

    // Add to function map.
    m_newFunctions[NewF->getName()] = NewF;
    if (pLib->IsInitFunc(F))
      initFuncSet.insert(NewF);

    vmap[F] = NewF;
  }

  // Set Entry
  Function *NewEntryFunc = m_newFunctions[entryFunc->getName()];
  DM.SetEntryFunction(NewEntryFunc);
  DM.SetEntryFunctionName(entryFunc->getName());
  if (entryDM.HasDxilEntrySignature(entryFunc)) {
    // Add signature.
    DxilEntrySignature &entrySig = entryDM.GetDxilEntrySignature(entryFunc);
    std::unique_ptr<DxilEntrySignature> newSig =
        llvm::make_unique<DxilEntrySignature>(entrySig);
    DM.ResetEntrySignature(newSig.release());
  }

  if (NewEntryFunc->hasFnAttribute(llvm::Attribute::AlwaysInline))
    NewEntryFunc->removeFnAttr(llvm::Attribute::AlwaysInline);
  if (props.IsHS()) {
    Function *patchConstantFunc = props.ShaderProps.HS.patchConstantFunc;
    Function *newPatchConstantFunc =
        m_newFunctions[patchConstantFunc->getName()];
    props.ShaderProps.HS.patchConstantFunc = newPatchConstantFunc;

    if (newPatchConstantFunc->hasFnAttribute(llvm::Attribute::AlwaysInline))
      newPatchConstantFunc->removeFnAttr(llvm::Attribute::AlwaysInline);
  }
  // Set EntryProps
  DM.SetShaderProperties(&props);

  // Debug info.

  // Add global
  bool bSuccess = true;
  for (auto &it : m_functionDefs) {
    DxilFunctionLinkInfo *linkInfo = it.first;
    DxilLib *pLib = it.second;

    for (GlobalVariable *GV : linkInfo->usedGVs) {
      // Skip added globals.
      if (m_newGlobals.count(GV->getName())) {
        if (vmap.find(GV) == vmap.end()) {
          if (DxilResourceBase *res = pLib->GetResource(GV)) {
            // For resource of same name, if class and type match, just map to
            // same NewGV.
            GlobalVariable *NewGV = m_newGlobals[GV->getName()];
            if (AddResource(res, NewGV)) {
              vmap[GV] = NewGV;
            } else {
              bSuccess = false;
            }
            continue;
          }

          // Redefine of global.
          m_ctx.emitError(Twine(kRedefineGlobal) + GV->getName());
          bSuccess = false;
        }
        continue;
      }
      Constant *Initializer = nullptr;
      if (GV->hasInitializer())
        Initializer = GV->getInitializer();

      GlobalVariable *NewGV = new GlobalVariable(
          *pM, GV->getType()->getElementType(), GV->isConstant(),
          GV->getLinkage(), Initializer, GV->getName(),
          /*InsertBefore*/ nullptr, GV->getThreadLocalMode(),
          GV->getType()->getAddressSpace(), GV->isExternallyInitialized());

      m_newGlobals[GV->getName()] = NewGV;

      vmap[GV] = NewGV;

      if (DxilResourceBase *res = pLib->GetResource(GV)) {
        bSuccess &= AddResource(res, NewGV);
      }
    }
  }

  if (!bSuccess)
    return nullptr;

  // Clone functions.
  for (auto &it : m_functionDefs) {
    DxilFunctionLinkInfo *linkInfo = it.first;

    Function *F = linkInfo->func;
    Function *NewF = m_newFunctions[F->getName()];

    // Add dxil functions to vmap.
    for (Function *UsedF : linkInfo->usedFunctions) {
      if (!vmap.count(UsedF)) {
        // Extern function need match by name
        DXASSERT(m_newFunctions.count(UsedF->getName()),
                 "Must have new function.");
        vmap[UsedF] = m_newFunctions[UsedF->getName()];
      }
    }

    CloneFunction(F, NewF, vmap);
  }

  // Call global constrctor.
  IRBuilder<> Builder(
      DM.GetEntryFunction()->getEntryBlock().getFirstInsertionPt());
  for (auto &it : m_functionDefs) {
    DxilFunctionLinkInfo *linkInfo = it.first;
    DxilLib *pLib = it.second;

    Function *F = linkInfo->func;
    if (pLib->IsInitFunc(F)) {
      Function *NewF = m_newFunctions[F->getName()];
      Builder.CreateCall(NewF);
    }
  }

  // Refresh intrinsic cache.
  DM.GetOP()->RefreshCache();

  // Add resource to DM.
  // This should be after functions cloned.
  AddResourceToDM(DM);

  RunPreparePass(*pM);

  return pM;
}

void DxilLinkJob::AddFunction(
    std::pair<DxilFunctionLinkInfo *, DxilLib *> &linkPair) {
  m_functionDefs[linkPair.first] = linkPair.second;
}

void DxilLinkJob::AddFunction(llvm::Function *F) {
  m_dxilFunctions[F->getName()] = F;
}

void DxilLinkJob::RunPreparePass(Module &M) {
  legacy::PassManager PM;

  PM.add(createAlwaysInlinerPass(/*InsertLifeTime*/ false));
  PM.add(createDxilDeadFunctionEliminationPass());
  // mem2reg.
  PM.add(createPromoteMemoryToRegisterPass());
  // Remove unused functions.
  PM.add(createDeadCodeEliminationPass());
  PM.add(createGlobalDCEPass());

  PM.add(createSimplifyInstPass());
  PM.add(createCFGSimplificationPass());

  PM.add(createDxilCondenseResourcesPass());
  PM.add(createDxilFinalizeModulePass());
  PM.add(createComputeViewIdStatePass());
  PM.add(createDxilDeadFunctionEliminationPass());
  PM.add(createNoPausePassesPass());
  PM.add(createDxilEmitMetadataPass());

  PM.run(M);
}

//------------------------------------------------------------------------------
//
// DxilLinkerImpl methods.
//

bool DxilLinkerImpl::HasLibNameRegistered(StringRef name) {
  return m_LibMap.count(name);
}

bool DxilLinkerImpl::RegisterLib(StringRef name,
                                 std::unique_ptr<llvm::Module> pModule,
                                 std::unique_ptr<llvm::Module> pDebugModule) {
  if (m_LibMap.count(name))
    return false;

  std::unique_ptr<llvm::Module> pM =
      pDebugModule ? std::move(pDebugModule) : std::move(pModule);

  if (!pM)
    return false;

  pM->setModuleIdentifier(name);
  std::unique_ptr<DxilLib> pLib =
      llvm::make_unique<DxilLib>(std::move(pM));
  m_LibMap[name] = std::move(pLib);
  return true;
}

bool DxilLinkerImpl::AttachLib(StringRef name) {
  auto iter = m_LibMap.find(name);
  if (iter == m_LibMap.end()) {
    return false;
  }

  return AttachLib(iter->second.get());
}
bool DxilLinkerImpl::DetachLib(StringRef name) {
  auto iter = m_LibMap.find(name);
  if (iter == m_LibMap.end()) {
    return false;
  }
  return DetachLib(iter->second.get());
}

void DxilLinkerImpl::DetachAll() {
  m_functionNameMap.clear();
  m_attachedLibs.clear();
}

bool DxilLinkerImpl::AttachLib(DxilLib *lib) {
  if (!lib) {
    // Invalid arg.
    return false;
  }

  if (m_attachedLibs.count(lib))
    return false;

  StringMap<std::unique_ptr<DxilFunctionLinkInfo>> &funcTable =
      lib->GetFunctionTable();
  bool bSuccess = true;
  for (auto it = funcTable.begin(), e = funcTable.end(); it != e; it++) {
    StringRef name = it->getKey();
    if (m_functionNameMap.count(name)) {
      // Redefine of function.
      m_ctx.emitError(Twine(kRedefineFunction) + name);
      bSuccess = false;
      continue;
    }
    m_functionNameMap[name] = std::make_pair(it->second.get(), lib);
  }

  if (bSuccess) {
    m_attachedLibs.insert(lib);
  } else {
    for (auto it = funcTable.begin(), e = funcTable.end(); it != e; it++) {
      StringRef name = it->getKey();
      auto iter = m_functionNameMap.find(name);

      if (iter == m_functionNameMap.end())
        continue;

      // Remove functions of lib.
      if (m_functionNameMap[name].second == lib)
        m_functionNameMap.erase(name);
    }
  }

  return bSuccess;
}

bool DxilLinkerImpl::DetachLib(DxilLib *lib) {
  if (!lib) {
    // Invalid arg.
    return false;
  }

  if (!m_attachedLibs.count(lib))
    return false;

  m_attachedLibs.erase(lib);

  // Remove functions from lib.
  StringMap<std::unique_ptr<DxilFunctionLinkInfo>> &funcTable =
      lib->GetFunctionTable();
  for (auto it = funcTable.begin(), e = funcTable.end(); it != e; it++) {
    StringRef name = it->getKey();
    m_functionNameMap.erase(name);
  }
  return true;
}

bool DxilLinkerImpl::AddFunctions(SmallVector<StringRef, 4> &workList,
                                  DenseSet<DxilLib *> &libSet,
                                  StringSet<> &addedFunctionSet,
                                  DxilLinkJob &linkJob, bool bLazyLoadDone) {
  while (!workList.empty()) {
    StringRef name = workList.pop_back_val();
    // Ignore added function.
    if (addedFunctionSet.count(name))
      continue;
    if (!m_functionNameMap.count(name)) {
      // Cannot find function, report error.
      m_ctx.emitError(Twine(kUndefFunction) + name);
      return false;
    }

    std::pair<DxilFunctionLinkInfo *, DxilLib *> &linkPair =
        m_functionNameMap[name];
    linkJob.AddFunction(linkPair);

    DxilLib *pLib = linkPair.second;
    libSet.insert(pLib);
    if (!bLazyLoadDone) {
      Function *F = linkPair.first->func;
      pLib->LazyLoadFunction(F);
    }
    for (Function *F : linkPair.first->usedFunctions) {
      if (hlsl::OP::IsDxilOpFunc(F)) {
        // Add dxil operations directly.
        linkJob.AddFunction(F);
      } else {
        // Push function name to work list.
        workList.emplace_back(F->getName());
      }
    }

    addedFunctionSet.insert(name);
  }
  return true;
}

std::unique_ptr<llvm::Module> DxilLinkerImpl::Link(StringRef entry,
                                               StringRef profile) {
  StringSet<> addedFunctionSet;
  SmallVector<StringRef, 4> workList;
  workList.emplace_back(entry);

  DxilLinkJob linkJob(m_ctx, m_valMajor, m_valMinor);

  DenseSet<DxilLib *> libSet;
  if (!AddFunctions(workList, libSet, addedFunctionSet, linkJob,
                    /*bLazyLoadDone*/ false))
    return nullptr;

  // Save global users.
  for (auto &pLib : libSet) {
    pLib->BuildGlobalUsage();
  }

  // Save global ctor users.
  for (auto &pLib : libSet) {
    pLib->CollectUsedInitFunctions(addedFunctionSet, workList);
  }
  // Add init functions if used.
  // All init function already loaded in BuildGlobalUsage, so set bLazyLoad
  // false here.
  if (!AddFunctions(workList, libSet, addedFunctionSet, linkJob,
                    /*bLazyLoadDone*/ true))
    return nullptr;

  std::pair<DxilFunctionLinkInfo *, DxilLib *> &entryLinkPair =
      m_functionNameMap[entry];

  return linkJob.Link(entryLinkPair, profile);
}

namespace hlsl {

DxilLinker *DxilLinker::CreateLinker(LLVMContext &Ctx, unsigned valMajor, unsigned valMinor) {
  return new DxilLinkerImpl(Ctx, valMajor, valMinor);
}
} // namespace hlsl
