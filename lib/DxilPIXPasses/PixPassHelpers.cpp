///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// PixPassHelpers.cpp
// // Copyright (C) Microsoft Corporation. All rights reserved. // This file is
// distributed under the University of Illinois Open Source     // License. See
// LICENSE.TXT for details.                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/DXIL/DxilFunctionProps.h"
#include "dxc/DXIL/DxilInstructions.h"
#include "dxc/DXIL/DxilMetadataHelper.h"
#include "dxc/DXIL/DxilModule.h"
#include "dxc/DXIL/DxilOperations.h"
#include "dxc/DXIL/DxilResourceBinding.h"
#include "dxc/DXIL/DxilResourceProperties.h"
#include "dxc/DxilRootSignature/DxilRootSignature.h"
#include "dxc/HLSL/DxilPackSignatureElement.h"
#include "dxc/HLSL/DxilSpanAllocator.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include "PixPassHelpers.h"

#include "dxc/Support/Global.h"
#include "dxc/Support/WinIncludes.h"
#include "dxc/dxcapi.h"

#ifdef PIX_DEBUG_DUMP_HELPER
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include <iostream>
#endif

using namespace llvm;
using namespace hlsl;

namespace PIXPassHelpers {
static void FindRayQueryHandlesFromUse(Value *U,
                                       SmallPtrSetImpl<Value *> &Handles) {
  if (Handles.insert(U).second) {
    auto RayQueryHandleUses = U->uses();
    for (Use &Use : RayQueryHandleUses) {
      iterator_range<Value::user_iterator> Users = Use->users();
      for (User *User : Users) {
        if (isa<PHINode>(User) || isa<SelectInst>(User))
          FindRayQueryHandlesFromUse(User, Handles);
      }
    }
  }
}

void FindRayQueryHandlesForFunction(llvm::Function *F,
                                    SmallPtrSetImpl<Value *> &RayQueryHandles) {
  auto &blocks = F->getBasicBlockList();
  if (!blocks.empty()) {
    for (auto &block : blocks) {
      for (auto &instruction : block) {
        if (hlsl::OP::IsDxilOpFuncCallInst(
                &instruction, hlsl::OP::OpCode::AllocateRayQuery)) {
          FindRayQueryHandlesFromUse(&instruction, RayQueryHandles);
        }
      }
    }
  }
}

static bool IsDynamicResourceShaderModel(DxilModule &DM) {
  return DM.GetShaderModel()->IsSMAtLeast(6, 6);
}

static bool ShaderModelRequiresAnnotateHandle(DxilModule &DM) {
  return DM.GetShaderModel()->IsSMAtLeast(6, 6);
}

static char const *RawUAVType() { return "struct.RWByteAddressBuffer"; }
static char const *ShaderModelHandleTypeName(DxilModule &DM) {
  // Prior to sm6.6, lib handles were typed after the resource they denote.
  // In 6.6 and after, and in all non-lib shader models,
  // all handles are dx.types.Handle.
  if (!DM.GetShaderModel()->IsLib() || DM.GetShaderModel()->IsSM66Plus())
    return "dx.types.Handle";
  return RawUAVType();
}

llvm::CallInst *CreateHandleForResource(hlsl::DxilModule &DM,
                                        llvm::IRBuilder<> &Builder,
                                        hlsl::DxilResourceBase *resource,
                                        const char *name) {

  OP *HlslOP = DM.GetOP();
  LLVMContext &Ctx = DM.GetModule()->getContext();

  DXIL::ResourceClass resourceClass = resource->GetClass();

  auto const *shaderModel = DM.GetShaderModel();
  Type *resourceHandleType =
      DM.GetModule()->getTypeByName(ShaderModelHandleTypeName(DM));
  if (shaderModel->IsLib()) {
    llvm::Constant *object = resource->GetGlobalSymbol();
    Value *load = Builder.CreateLoad(object, resourceHandleType);
    llvm::cast<LoadInst>(load)->setAlignment(4);
    llvm::cast<LoadInst>(load)->setVolatile(false);
    Function *CreateHandleForLibOpFunc =
        HlslOP->GetOpFunc(DXIL::OpCode::CreateHandleForLib, load->getType());
    Constant *CreateHandleForLibOpcodeArg =
        HlslOP->GetU32Const((unsigned)DXIL::OpCode::CreateHandleForLib);
    auto *handle = Builder.CreateCall(CreateHandleForLibOpFunc,
                                      {CreateHandleForLibOpcodeArg, load});

    if (ShaderModelRequiresAnnotateHandle(DM)) {
      Function *annotHandleFn =
          HlslOP->GetOpFunc(DXIL::OpCode::AnnotateHandle, Type::getVoidTy(Ctx));
      Value *annotHandleArg =
          HlslOP->GetI32Const((unsigned)DXIL::OpCode::AnnotateHandle);
      DxilResourceProperties RP =
          resource_helper::loadPropsFromResourceBase(resource);
      Type *resPropertyTy = HlslOP->GetResourcePropertiesType();
      Value *propertiesV = resource_helper::getAsConstant(RP, resPropertyTy,
                                                          *DM.GetShaderModel());

      return Builder.CreateCall(annotHandleFn,
                                {annotHandleArg, handle, propertiesV});
    } else {
      return handle;
    }
  } else if (IsDynamicResourceShaderModel(DM)) {
    Function *CreateHandleFromBindingOpFunc = HlslOP->GetOpFunc(
        DXIL::OpCode::CreateHandleFromBinding, Type::getVoidTy(Ctx));
    Constant *CreateHandleFromBindingOpcodeArg =
        HlslOP->GetU32Const((unsigned)DXIL::OpCode::CreateHandleFromBinding);
    DxilResourceBinding binding =
        resource_helper::loadBindingFromResourceBase(resource);
    Value *bindingV = resource_helper::getAsConstant(
        binding, HlslOP->GetResourceBindingType(), *DM.GetShaderModel());

    Value *registerIndex = HlslOP->GetU32Const(0);

    Value *isUniformRes = HlslOP->GetI1Const(0);

    Value *createHandleFromBindingArgs[] = {CreateHandleFromBindingOpcodeArg,
                                            bindingV, registerIndex,
                                            isUniformRes};

    auto *handle = Builder.CreateCall(CreateHandleFromBindingOpFunc,
                                      createHandleFromBindingArgs, name);

    Function *annotHandleFn =
        HlslOP->GetOpFunc(DXIL::OpCode::AnnotateHandle, Type::getVoidTy(Ctx));
    Value *annotHandleArg =
        HlslOP->GetI32Const((unsigned)DXIL::OpCode::AnnotateHandle);
    DxilResourceProperties RP =
        resource_helper::loadPropsFromResourceBase(resource);
    Type *resPropertyTy = HlslOP->GetResourcePropertiesType();
    Value *propertiesV =
        resource_helper::getAsConstant(RP, resPropertyTy, *DM.GetShaderModel());

    return Builder.CreateCall(annotHandleFn,
                              {annotHandleArg, handle, propertiesV});
  } else {
    Function *CreateHandleOpFunc =
        HlslOP->GetOpFunc(DXIL::OpCode::CreateHandle, Type::getVoidTy(Ctx));
    Constant *CreateHandleOpcodeArg =
        HlslOP->GetU32Const((unsigned)DXIL::OpCode::CreateHandle);
    Constant *ClassArg = HlslOP->GetI8Const(
        static_cast<std::underlying_type<DxilResourceBase::Class>::type>(
            resourceClass));
    Constant *MetaDataArg = HlslOP->GetU32Const(resource->GetID());
    Constant *IndexArg = HlslOP->GetU32Const(0);
    Constant *FalseArg =
        HlslOP->GetI1Const(0); // non-uniform resource index: false
    return Builder.CreateCall(
        CreateHandleOpFunc,
        {CreateHandleOpcodeArg, ClassArg, MetaDataArg, IndexArg, FalseArg},
        name);
  }
}

static std::vector<uint8_t> SerializeRootSignatureToVector(
    DxilVersionedRootSignatureDesc const *rootSignature) {
  CComPtr<IDxcBlob> serializedRootSignature;
  CComPtr<IDxcBlobEncoding> errorBlob;
  constexpr bool allowReservedRegisterSpace = true;
  SerializeRootSignature(rootSignature, &serializedRootSignature, &errorBlob,
                         allowReservedRegisterSpace);
  std::vector<uint8_t> ret;
  if (serializedRootSignature == nullptr) {
    return ret;
  }
  auto const *serializedData = reinterpret_cast<const uint8_t *>(
      serializedRootSignature->GetBufferPointer());
  ret.assign(serializedData,
             serializedData + serializedRootSignature->GetBufferSize());

  return ret;
}

constexpr uint32_t toolsRegisterSpace = static_cast<uint32_t>(-2);

template <typename RootSigDesc, typename RootParameterDesc>
void ExtendRootSig(RootSigDesc &rootSigDesc, uint32_t toolsUAVRegister) {
  auto *existingParams = rootSigDesc.pParameters;
  for (uint32_t i = 0; i < rootSigDesc.NumParameters; ++i) {
    if (rootSigDesc.pParameters[i].ParameterType ==
        DxilRootParameterType::UAV) {
      if (rootSigDesc.pParameters[i].Descriptor.RegisterSpace ==
              toolsRegisterSpace &&
          rootSigDesc.pParameters[i].Descriptor.ShaderRegister ==
              toolsUAVRegister) {
        // Already added
        return;
      }
    }
  }
  auto *newParams = new RootParameterDesc[rootSigDesc.NumParameters + 1];
  if (existingParams != nullptr) {
    memcpy(newParams, existingParams,
           rootSigDesc.NumParameters * sizeof(RootParameterDesc));
    delete[] existingParams;
  }
  rootSigDesc.pParameters = newParams;
  rootSigDesc.pParameters[rootSigDesc.NumParameters].ParameterType =
      DxilRootParameterType::UAV;
  rootSigDesc.pParameters[rootSigDesc.NumParameters].Descriptor.RegisterSpace =
      toolsRegisterSpace;
  rootSigDesc.pParameters[rootSigDesc.NumParameters].Descriptor.ShaderRegister =
      toolsUAVRegister;
  rootSigDesc.pParameters[rootSigDesc.NumParameters].ShaderVisibility =
      DxilShaderVisibility::All;
  rootSigDesc.NumParameters++;
}

static std::vector<uint8_t>
AddUAVParamterToRootSignature(const void *Data, uint32_t Size,
                              uint32_t toolsUAVRegister) {
  DxilVersionedRootSignature rootSignature;
  DeserializeRootSignature(Data, Size, rootSignature.get_address_of());
  auto *rs = rootSignature.get_mutable();
  switch (rootSignature->Version) {
  case DxilRootSignatureVersion::Version_1_0:
    ExtendRootSig<DxilRootSignatureDesc, DxilRootParameter>(rs->Desc_1_0,
                                                            toolsUAVRegister);
    break;
  case DxilRootSignatureVersion::Version_1_1:
    ExtendRootSig<DxilRootSignatureDesc1, DxilRootParameter1>(rs->Desc_1_1,
                                                              toolsUAVRegister);
    rs->Desc_1_1.pParameters[rs->Desc_1_1.NumParameters - 1].Descriptor.Flags =
        hlsl::DxilRootDescriptorFlags::None;
    break;
  }
  return SerializeRootSignatureToVector(rs);
}

static void AddUAVToShaderAttributeRootSignature(DxilModule &DM,
                                                 uint32_t toolsUAVRegister) {
  auto rs = DM.GetSerializedRootSignature();
  if (!rs.empty()) {
    std::vector<uint8_t> asVector = AddUAVParamterToRootSignature(
        rs.data(), static_cast<uint32_t>(rs.size()), toolsUAVRegister);
    if (!asVector.empty()) {
      DM.ResetSerializedRootSignature(asVector);
    }
  }
}

static void AddUAVToDxilDefinedGlobalRootSignatures(DxilModule &DM,
                                                    uint32_t toolsUAVRegister) {
  struct ReplacementRootSignature {
    std::string Name;
    std::vector<uint8_t> Data;
  };

  std::vector<ReplacementRootSignature> replacementRootSignatures;
  auto *subObjects = DM.GetSubobjects();
  if (subObjects != nullptr) {
    for (auto const &subObject : subObjects->GetSubobjects()) {
      if (subObject.second->GetKind() ==
          DXIL::SubobjectKind::GlobalRootSignature) {
        const void *Data = nullptr;
        uint32_t Size = 0;
        constexpr bool notALocalRS = false;
        if (subObject.second->GetRootSignature(notALocalRS, Data, Size,
                                               nullptr)) {
          std::vector<uint8_t> extended =
              AddUAVParamterToRootSignature(Data, Size, toolsUAVRegister);
          if (!extended.empty()) {
            replacementRootSignatures.push_back(
                {subObject.first.str(), std::move(extended)});
          }
        }
      }
    }

    constexpr bool notALocalRS = false;
    for (auto const &replacementRootSignature : replacementRootSignatures) {
      subObjects->RemoveSubobject(replacementRootSignature.Name);
      subObjects->CreateRootSignature(
          replacementRootSignature.Name, notALocalRS,
          replacementRootSignature.Data.data(),
          static_cast<uint32_t>(replacementRootSignature.Data.size()));
    }
  }
}

// Set up a UAV with structure of a single int
hlsl::DxilResource *CreateGlobalUAVResource(hlsl::DxilModule &DM,
                                            unsigned int hlslBindIndex,
                                            const char *name) {
  LLVMContext &Ctx = DM.GetModule()->getContext();

  for (auto const &existingUAV : DM.GetUAVs()) {
    if (existingUAV->GetSpaceID() == toolsRegisterSpace &&
        existingUAV->GetLowerBound() == hlslBindIndex) {
      return existingUAV.get();
    }
  }

  const char *PIXStructTypeName = ShaderModelHandleTypeName(DM);
  llvm::StructType *UAVStructTy =
      DM.GetModule()->getTypeByName(PIXStructTypeName);

  if (UAVStructTy == nullptr) {
    SmallVector<llvm::Type *, 1> Elements{Type::getInt32Ty(Ctx)};
    UAVStructTy = llvm::StructType::create(Elements, PIXStructTypeName);
  }

  AddUAVToDxilDefinedGlobalRootSignatures(DM, hlslBindIndex);
  AddUAVToShaderAttributeRootSignature(DM, hlslBindIndex);

  unsigned int Id = static_cast<unsigned int>(DM.GetUAVs().size());
  std::unique_ptr<DxilResource> pUAV = llvm::make_unique<DxilResource>();
  pUAV->SetID(Id);

  auto const *shaderModel = DM.GetShaderModel();
  std::string PixUavName = "PIXUAV" + std::to_string(hlslBindIndex);
  if (shaderModel->IsLib()) {
    auto *Global =
        DM.GetModule()->getOrInsertGlobal(PixUavName.c_str(), UAVStructTy);
    GlobalVariable *NewGV = cast<GlobalVariable>(Global);
    NewGV->setConstant(true);
    NewGV->setLinkage(GlobalValue::ExternalLinkage);
    NewGV->setThreadLocal(false);
    NewGV->setAlignment(4);
    pUAV->SetGlobalSymbol(NewGV);
  } else {
    pUAV->SetGlobalSymbol(UndefValue::get(UAVStructTy->getPointerTo()));
  }
  pUAV->SetGlobalName(name);
  pUAV->SetRW(true); // sets UAV class
  pUAV->SetSpaceID(toolsRegisterSpace); // reserved-for-tools register space
  pUAV->SetSampleCount(0); // This is what compiler generates for a raw UAV
  pUAV->SetGloballyCoherent(false);
  pUAV->SetReorderCoherent(false);
  pUAV->SetHasCounter(false);
  pUAV->SetCompType(
      CompType::getInvalid()); // This is what compiler generates for a raw UAV
  pUAV->SetLowerBound(hlslBindIndex);
  pUAV->SetRangeSize(1);
  pUAV->SetElementStride(1);
  pUAV->SetKind(DXIL::ResourceKind::RawBuffer);
  auto HLSLType = DM.GetModule()->getTypeByName(RawUAVType());
  if (HLSLType == nullptr) {
    SmallVector<llvm::Type *, 1> Elements{Type::getInt32Ty(Ctx)};
    HLSLType = llvm::StructType::create(Elements, RawUAVType());
  }
  pUAV->SetHLSLType(HLSLType->getPointerTo());

  auto pAnnotation = DM.GetTypeSystem().GetStructAnnotation(UAVStructTy);
  if (pAnnotation == nullptr) {

    pAnnotation = DM.GetTypeSystem().AddStructAnnotation(UAVStructTy);
    pAnnotation->GetFieldAnnotation(0).SetCBufferOffset(0);
    pAnnotation->GetFieldAnnotation(0).SetCompType(
        hlsl::DXIL::ComponentType::I32);
    pAnnotation->GetFieldAnnotation(0).SetFieldName("count");
  }

  auto *ret = pUAV.get();
  DM.AddUAV(std::move(pUAV));
  DM.CollectShaderFlagsForModule();
  return ret;
}

void EraseIfUnused(hlsl::DxilModule &DM, llvm::Function *OpFunction) {
  if (OpFunction != nullptr && OpFunction->user_empty()) {
    DM.GetOP()->RemoveFunction(OpFunction);
    OpFunction->eraseFromParent();
  }
}

// A stale ViewID dependency table describes registers that do not match the
// module's current signature sizes. Clearing it removes both the module's
// cached copy and its IR metadata, so a downstream pass can recompute the
// table for the current signature.
void ClearViewIdState(hlsl::DxilModule &DM) {
  DM.GetSerializedViewIdState().clear();
  if (auto *ViewIdStateMD = DM.GetModule()->getNamedMetadata(
          hlsl::DxilMDHelper::kDxilViewIdStateMDName)) {
    DM.GetModule()->eraseNamedMetadata(ViewIdStateMD);
  }
}

// Set up a UAV with structure of a single int
llvm::CallInst *CreateUAVOnceForModule(hlsl::DxilModule &DM,
                                       llvm::IRBuilder<> &Builder,
                                       unsigned int hlslBindIndex,
                                       const char *name) {
  auto uav = CreateGlobalUAVResource(DM, hlslBindIndex, name);
  auto *handle = CreateHandleForResource(DM, Builder, uav, name);

  return handle;
}

llvm::Function *GetEntryFunction(hlsl::DxilModule &DM) {
  if (DM.GetEntryFunction() != nullptr) {
    return DM.GetEntryFunction();
  }
  return DM.GetPatchConstantFunction();
}

std::vector<llvm::Function *>
GetAllInstrumentableFunctions(hlsl::DxilModule &DM) {

  std::vector<llvm::Function *> ret;

  for (llvm::Function &F : DM.GetModule()->functions()) {
    if (F.isDeclaration() || F.isIntrinsic() || hlsl::OP::IsDxilOpFunc(&F))
      continue;
    if (F.getBasicBlockList().empty())
      continue;
    ret.push_back(&F);
  }

  return ret;
}

bool InlineNonEntryFunctions(
    hlsl::DxilModule &DM,
    llvm::SmallVectorImpl<llvm::Function *> *UninlinedFunctions) {
  if (UninlinedFunctions != nullptr) {
    UninlinedFunctions->clear();
  }

  if (DM.GetShaderModel()->IsLib()) {
    return false;
  }

  // The runtime invokes a hull shader patch-constant function directly, so it
  // is a second root of the call graph and stays alongside the entry point.
  llvm::Function *const entryFunction = DM.GetEntryFunction();
  llvm::Function *const patchConstantFunction = DM.GetPatchConstantFunction();

  // A module that names no entry point has no root, and the entry point has no
  // caller in the IR. Leave such a module alone rather than erase every
  // function in it.
  if (entryFunction == nullptr) {
    return false;
  }

  bool modified = false;

  // HLSL has no recursion, so the call graph is acyclic and inlining leaf-ward
  // terminates. A fixed-point loop also reaches a helper that loses its last
  // caller only once another helper is inlined away.
  bool inlinedACallThisRound = true;
  while (inlinedACallThisRound) {
    inlinedACallThisRound = false;

    for (llvm::Function *function : GetAllInstrumentableFunctions(DM)) {
      if (function == entryFunction || function == patchConstantFunction) {
        continue;
      }

      // llvm::InlineFunction is the mechanical inliner and ignores inlining
      // attributes. Clear the attribute so the module carries no claim that
      // contradicts its own shape.
      function->removeFnAttr(llvm::Attribute::NoInline);

      // Collect the call sites first, because inlining rewrites the use list.
      llvm::SmallVector<llvm::CallInst *, 8> callSites;
      for (llvm::User *user : function->users()) {
        if (auto *call = llvm::dyn_cast<llvm::CallInst>(user)) {
          if (call->getCalledFunction() == function) {
            callSites.push_back(call);
          }
        }
      }

      for (llvm::CallInst *callSite : callSites) {
        llvm::InlineFunctionInfo inlineFunctionInfo;
        if (llvm::InlineFunction(callSite, inlineFunctionInfo)) {
          inlinedACallThisRound = true;
          modified = true;
        }
      }

      // A body with no caller still gets numbered and advertised to PIX as
      // somewhere to step into. Erase it. DxilModule keeps an entry-property
      // map and a type-annotation map keyed on llvm::Function *, so tell it
      // first or both keep entries keyed on freed storage.
      if (function->use_empty()) {
        DM.RemoveFunction(function);
        function->eraseFromParent();
        modified = true;
      }
    }
  }

  if (UninlinedFunctions != nullptr) {
    // A function reached other than by a direct call, or one that
    // llvm::InlineFunction declines, is still here. PIX gets an instruction
    // range for it that no trace record arrives for, so report it.
    for (llvm::Function *function : GetAllInstrumentableFunctions(DM)) {
      if (function != entryFunction && function != patchConstantFunction) {
        UninlinedFunctions->push_back(function);
      }
    }
  }

  return modified;
}

hlsl::DXIL::ShaderKind GetFunctionShaderKind(hlsl::DxilModule &DM,
                                             llvm::Function *fn) {
  hlsl::DXIL::ShaderKind shaderKind = hlsl::DXIL::ShaderKind::Invalid;
  if (!DM.HasDxilFunctionProps(fn)) {
    auto ShaderModel = DM.GetShaderModel();
    shaderKind = ShaderModel->GetKind();
  } else {
    hlsl::DxilFunctionProps const &props = DM.GetDxilFunctionProps(fn);
    shaderKind = props.shaderKind;
  }
  return shaderKind;
}

ExpandedStruct ExpandStructType(LLVMContext &Ctx,
                                Type *OriginalPayloadStructType) {
  SmallVector<Type *, 16> Elements;
  for (unsigned int i = 0;
       i < OriginalPayloadStructType->getStructNumElements(); ++i) {
    Elements.push_back(OriginalPayloadStructType->getStructElementType(i));
  }
  Elements.push_back(Type::getInt32Ty(Ctx));
  Elements.push_back(Type::getInt32Ty(Ctx));
  Elements.push_back(Type::getInt32Ty(Ctx));
  ExpandedStruct ret;
  ret.ExpandedPayloadStructType =
      StructType::create(Ctx, Elements, "PIX_AS2MS_Expanded_Type");
  ret.ExpandedPayloadStructPtrType =
      ret.ExpandedPayloadStructType->getPointerTo();
  return ret;
}

void ReplaceAllUsesOfInstructionWithNewValueAndDeleteInstruction(
    Instruction *Instr, Value *newValue, Type *newType) {
  std::vector<Value *> users;
  for (auto u = Instr->user_begin(); u != Instr->user_end(); ++u) {
    users.push_back(*u);
  }

  for (auto user : users) {
    if (auto *instruction = llvm::cast<Instruction>(user)) {
      for (unsigned int i = 0; i < instruction->getNumOperands(); ++i) {
        auto *Operand = instruction->getOperand(i);
        if (Operand == Instr) {
          instruction->setOperand(i, newValue);
        }
      }
      if (llvm::isa<GetElementPtrInst>(instruction)) {
        auto *GEP = llvm::cast<GetElementPtrInst>(instruction);
        GEP->setSourceElementType(newType);
      } else if (hlsl::OP::IsDxilOpFuncCallInst(
                     instruction, hlsl::OP::OpCode::DispatchMesh)) {
        DxilModule &DM = instruction->getModule()->GetOrCreateDxilModule();
        OP *HlslOP = DM.GetOP();

        DxilInst_DispatchMesh DispatchMesh(instruction);
        IRBuilder<> B(instruction);
        SmallVector<Value *, 5> args;
        args.push_back(
            HlslOP->GetU32Const((unsigned)hlsl::OP::OpCode::DispatchMesh));
        args.push_back(DispatchMesh.get_threadGroupCountX());
        args.push_back(DispatchMesh.get_threadGroupCountY());
        args.push_back(DispatchMesh.get_threadGroupCountZ());
        args.push_back(newValue);

        B.CreateCall(HlslOP->GetOpFunc(DXIL::OpCode::DispatchMesh,
                                       newType->getPointerTo()),
                     args);

        instruction->removeFromParent();
        delete instruction;
      }
    }
  }

  Instr->removeFromParent();
  delete Instr;
}

// An authoritative row is mandatory: D3D12 matches signature elements
// between stages by register, so SV_Position on any other row fails
// pipeline creation with a linkage error. That row can already hold one of
// this shader's own elements, since pixel-shader-only system values
// (SV_IsFrontFace, SV_SampleIndex, SV_PrimitiveID without a geometry shader)
// pack after the interpolated attributes.
//
// Whatever occupies that row is safe to move: the upstream stage writes
// SV_Position there, so no upstream element shares that register, so
// nothing occupying it in this shader is linkage-bound.
//
// A hint carries no such guarantee and never displaces anything: SV_Position
// goes on a free row instead.
//
// Moving an element is metadata-only: dx.op.loadInput addresses elements by
// signature element ID and its row operand is relative to the element, so no
// instruction refers to the absolute row.
static std::vector<DxilSignatureElement *> FindElementsOccupyingSignatureRow(
    std::vector<std::unique_ptr<DxilSignatureElement>> const &Elements,
    unsigned int Row) {
  std::vector<DxilSignatureElement *> Occupants;
  for (auto const &Element : Elements) {
    if (!Element->IsAllocated())
      continue;
    unsigned int FirstRow = static_cast<unsigned int>(Element->GetStartRow());
    if (Row >= FirstRow && Row < FirstRow + Element->GetRows())
      Occupants.push_back(Element.get());
  }
  return Occupants;
}

// Mirrors how the validator checks a pre-allocated element against the
// allocator: kInsufficientFreeComponents from the row check only says the row
// is partly used, which is exactly what packing two scalars into one register
// looks like, so the column check is what decides.
static bool ElementFitsAtLocation(hlsl::DxilSignatureAllocator &Allocator,
                                  hlsl::DxilPackElement const &Element,
                                  unsigned int Row, unsigned int Column) {
  hlsl::DxilSignatureAllocator::ConflictType Conflict =
      Allocator.DetectRowConflict(&Element, Row);
  if (Conflict != hlsl::DxilSignatureAllocator::kNoConflict &&
      Conflict != hlsl::DxilSignatureAllocator::kInsufficientFreeComponents) {
    return false;
  }
  return Allocator.DetectColConflict(&Element, Row, Column) ==
         hlsl::DxilSignatureAllocator::kNoConflict;
}

// Gives Added_SV_Position a home -- TargetRow when the caller has one,
// otherwise wherever it fits -- and repacks whatever that displaces, using the
// same allocator the front end packs signatures with.
//
// A displaced element takes the first row that fits it, reusing gaps instead
// of appending past the end of the signature. DxilSignatureAllocator models
// rows, component columns, interpolation-mode and data-width compatibility,
// and the 32-register signature limit together, so no placement can exceed
// that limit.
//
// Returns false with every element left exactly where it was when the
// signature has no room, rather than emit an out-of-range register.
static bool PlaceSVPositionAndRepackDisplacedElements(
    hlsl::DxilSignature &Signature, DxilSignatureElement &Added_SV_Position,
    unsigned int TargetRow) {
  auto const &Elements = Signature.GetElements();
  bool const UseMinPrecision = Signature.UseMinPrecision();

  std::vector<DxilSignatureElement *> Displaced;
  if (TargetRow != kUnknownSVPositionRow)
    Displaced = FindElementsOccupyingSignatureRow(Elements, TargetRow);

  // The allocator takes raw pointers to these adapters and holds them across
  // calls, so both vectors are sized up front and never grow afterwards.
  std::vector<hlsl::DxilPackElement> Retained;
  Retained.reserve(Elements.size());
  std::vector<hlsl::DxilPackElement> ToRepack;
  ToRepack.reserve(Displaced.size());

  for (auto const &Element : Elements) {
    DxilSignatureElement *SignatureElement = Element.get();
    bool const Displacing = std::find(Displaced.begin(), Displaced.end(),
                                      SignatureElement) != Displaced.end();
    // Elements the packer never places -- SV_Coverage and similar, whose
    // interpretation is NotPacked -- use no register, and
    // DxilSignatureAllocator asserts if handed one. A well-formed signature
    // never marks such an element as allocated, so one occupying the target
    // row means the signature is malformed.
    if (!hlsl::DxilSignature::ShouldBeAllocated(
            SignatureElement->GetInterpretation()) ||
        !SignatureElement->IsAllocated()) {
      if (Displacing)
        return false;
      continue;
    }
    if (Displacing) {
      ToRepack.emplace_back(SignatureElement, UseMinPrecision);
    } else {
      Retained.emplace_back(SignatureElement, UseMinPrecision);
    }
  }

  hlsl::DxilSignatureAllocator Allocator(hlsl::DXIL::kMaxSignatureTotalVectors,
                                         UseMinPrecision);

  // Everything that is staying put keeps the register the front end gave it:
  // those elements are paired with the upstream stage by row, so repacking them
  // would break exactly the linkage this function exists to preserve.
  for (hlsl::DxilPackElement &Element : Retained) {
    unsigned int Row = Element.GetStartRow();
    unsigned int Column = Element.GetStartCol();
    if (!ElementFitsAtLocation(Allocator, Element, Row, Column)) {
      // The signature handed to this pass already overlaps itself, so there
      // is no consistent register layout to add to. Refuse rather than add
      // another element on top of it.
      return false;
    }
    Allocator.PlaceElement(&Element, Row, Column);
  }

  hlsl::DxilPackElement PositionElement(&Added_SV_Position, UseMinPrecision);
  if (TargetRow == kUnknownSVPositionRow) {
    if (Allocator.PackNext(&PositionElement, 0,
                           hlsl::DXIL::kMaxSignatureTotalVectors) == 0) {
      return false;
    }
  } else {
    // SV_Position is four components wide, so it always starts at column 0 and
    // owns the whole register once the occupants have been evicted.
    if (!ElementFitsAtLocation(Allocator, PositionElement, TargetRow, 0)) {
      return false;
    }
    Allocator.PlaceElement(&PositionElement, TargetRow, 0);
    PositionElement.SetLocation(TargetRow, 0);
  }

  // The target row must be reserved before displaced elements can be
  // repacked around it. Their old locations are saved so a partial repack
  // that runs out of registers can be undone.
  std::vector<std::pair<int, int>> OriginalLocations;
  OriginalLocations.reserve(ToRepack.size());
  for (hlsl::DxilPackElement &Element : ToRepack) {
    OriginalLocations.emplace_back(Element.Get()->GetStartRow(),
                                   Element.Get()->GetStartCol());
  }

  for (size_t Index = 0; Index < ToRepack.size(); ++Index) {
    ToRepack[Index].ClearLocation();
    if (Allocator.PackNext(&ToRepack[Index], 0,
                           hlsl::DXIL::kMaxSignatureTotalVectors) == 0) {
      for (size_t Undo = 0; Undo <= Index; ++Undo) {
        ToRepack[Undo].Get()->SetStartRow(OriginalLocations[Undo].first);
        ToRepack[Undo].Get()->SetStartCol(OriginalLocations[Undo].second);
      }
      return false;
    }
  }

  return true;
}

unsigned int FindOrAddSV_Position(hlsl::DxilModule &DM,
                                  unsigned UpStreamSVPosRow,
                                  SVPositionRowAuthority RowAuthority) {
  hlsl::DxilSignature &InputSignature = DM.GetInputSignature();
  auto &InputElements = InputSignature.GetElements();

  auto Existing_SV_Position =
      std::find_if(InputElements.begin(), InputElements.end(),
                   [](const std::unique_ptr<DxilSignatureElement> &Element) {
                     return Element->GetSemantic()->GetKind() ==
                            hlsl::DXIL::SemanticKind::Position;
                   });

  // SV_Position, if present, has to have full mask, so we needn't worry
  // about the shader having selected components that don't include x or y.
  if (Existing_SV_Position != InputElements.end())
    return Existing_SV_Position->get()->GetID();

  constexpr unsigned int RowCount = 1;
  constexpr unsigned int ColumnCount = 4;

  llvm::Function *EntryFunction = GetEntryFunction(DM);
  hlsl::DXIL::ShaderKind ShaderKind =
      EntryFunction != nullptr ? GetFunctionShaderKind(DM, EntryFunction)
                               : DM.GetShaderModel()->GetKind();

  // Evicting an occupant is sound only for a pixel shader's input signature:
  // the reasoning that the upstream stage writes SV_Position at this
  // register, and so nothing else, assumes one flat register space. A mesh
  // shader has two -- per-vertex and per-primitive, each numbered from zero
  // and packed by different rules -- so the row says nothing about what else
  // may be bound there. Mesh-to-pixel pipelines do not need the relocation
  // anyway, since that pairing is matched by semantic name, not register.
  //
  // This only rules out the shader being instrumented here. Whether the
  // upstream stage was a mesh shader is not visible from this module; the
  // caller that read the upstream signature decides that by declining to
  // claim the row is authoritative.
  unsigned int TargetRow = kUnknownSVPositionRow;
  if (UpStreamSVPosRow < hlsl::DXIL::kMaxSignatureTotalVectors) {
    bool const RowIsOccupied =
        !FindElementsOccupyingSignatureRow(InputElements, UpStreamSVPosRow)
             .empty();
    bool const MayDisplaceOccupants =
        RowAuthority == SVPositionRowAuthority::Authoritative &&
        ShaderKind == hlsl::DXIL::ShaderKind::Pixel;
    if (!RowIsOccupied || MayDisplaceOccupants)
      TargetRow = UpStreamSVPosRow;
  }

  auto Added_SV_Position =
      llvm::make_unique<DxilSignatureElement>(DXIL::SigPointKind::PSIn);
  // LinearNoperspective is the interpolation mode the front end gives a
  // pixel shader that declares SV_Position itself, so an instrumented
  // shader must match it: a driver honoring a different mode would hand the
  // instrumentation perspective-divided coordinates, and PIX would silently
  // attribute hits to the wrong pixel.
  Added_SV_Position->Initialize(
      "Position", hlsl::CompType::getF32(),
      hlsl::DXIL::InterpolationMode::LinearNoperspective, RowCount,
      ColumnCount);
  Added_SV_Position->AppendSemanticIndex(0);
  Added_SV_Position->SetKind(hlsl::DXIL::SemanticKind::Position);

  if (!PlaceSVPositionAndRepackDisplacedElements(
          InputSignature, *Added_SV_Position, TargetRow)) {
    // An authoritative row promises which register the upstream stage
    // writes SV_Position to. Placing it elsewhere would read pixel position
    // from a register nothing writes and misattribute PIX's results, so fail
    // instead and let the caller drop the feature for this draw.
    //
    // A hint carries no such promise, so the free-row fallback is still
    // usable. Emitting a register past the end of the signature is never an
    // option: that is invalid DXIL, and PIX does not validate what it
    // patches, so the module would reach the driver unchecked.
    bool const RowWasPromised =
        RowAuthority == SVPositionRowAuthority::Authoritative &&
        TargetRow != kUnknownSVPositionRow;
    if (RowWasPromised) {
      throw ::hlsl::Exception(
          E_FAIL, "PIX: the shader's input signature cannot accommodate the "
                  "SV_Position element at the register the upstream stage "
                  "writes it to.");
    }
    if (TargetRow == kUnknownSVPositionRow ||
        !PlaceSVPositionAndRepackDisplacedElements(
            InputSignature, *Added_SV_Position, kUnknownSVPositionRow)) {
      throw ::hlsl::Exception(
          E_FAIL, "PIX: the shader's input signature has no room for the "
                  "SV_Position element the instrumentation needs to read.");
    }
  }

  // Adding an input-signature element invalidates any ViewID dependency
  // table in the module: the table's size matches the previous element
  // count.
  ClearViewIdState(DM);

  // AppendElement sets the element's ID by default
  auto index = InputSignature.AppendElement(std::move(Added_SV_Position));
  return InputElements[index]->GetID();
}

void ForEachDynamicallyIndexedResource(
    hlsl::DxilModule &DM,
    const std::function<bool(bool, Instruction *, Value *)> &Visitor) {
  OP *HlslOP = DM.GetOP();
  LLVMContext &Ctx = DM.GetModule()->getContext();

  for (llvm::Function &F : DM.GetModule()->functions()) {
    if (F.isDeclaration() && !F.use_empty() && OP::IsDxilOpFunc(&F)) {
      if (F.hasName()) {
        if (F.getName().find("createHandleForLib") != StringRef::npos) {
          auto FunctionUses = F.uses();
          for (auto FI = FunctionUses.begin(); FI != FunctionUses.end();) {
            auto &FunctionUse = *FI++;
            auto FunctionUser = FunctionUse.getUser();
            auto instruction = cast<Instruction>(FunctionUser);
            Value *resourceLoad = instruction->getOperand(
                DXIL::OperandIndex::kCreateHandleForLibResOpIdx);
            if (auto *load = cast<LoadInst>(resourceLoad)) {
              auto *resOrGep = load->getOperand(0);
              if (auto *gep = dyn_cast<GetElementPtrInst>(resOrGep)) {
                if (!Visitor(DxilMDHelper::IsMarkedNonUniform(gep), load,
                             gep->getOperand(2))) {
                  return;
                }
              }
            }
          }
        }
      }
    }
  }

  auto CreateHandleFn =
      HlslOP->GetOpFunc(DXIL::OpCode::CreateHandle, Type::getVoidTy(Ctx));
  auto CreateHandleFromBindingFn = HlslOP->GetOpFunc(
      DXIL::OpCode::CreateHandleFromBinding, Type::getVoidTy(Ctx));
  auto CreateHandleFromHeapFn = HlslOP->GetOpFunc(
      DXIL::OpCode::CreateHandleFromHeap, Type::getVoidTy(Ctx));

  struct UnusedDeclarationCleanup {
    hlsl::DxilModule &DM;
    llvm::Function *CreateHandleFn;
    llvm::Function *CreateHandleFromBindingFn;
    llvm::Function *CreateHandleFromHeapFn;
    ~UnusedDeclarationCleanup() {
      EraseIfUnused(DM, CreateHandleFn);
      EraseIfUnused(DM, CreateHandleFromBindingFn);
      EraseIfUnused(DM, CreateHandleFromHeapFn);
    }
  } cleanup{DM, CreateHandleFn, CreateHandleFromBindingFn,
            CreateHandleFromHeapFn};

  for (auto FI = CreateHandleFn->user_begin();
       FI != CreateHandleFn->user_end();) {
    auto *FunctionUser = *FI++;
    auto instruction = cast<Instruction>(FunctionUser);
    Value *index =
        instruction->getOperand(DXIL::OperandIndex::kCreateHandleResIndexOpIdx);
    if (!isa<Constant>(index)) {
      const DxilInst_CreateHandle createHandle(instruction);
      if (!Visitor(createHandle.get_nonUniformIndex_val(), instruction,
                   index)) {
        return;
      }
    }
  }

  for (auto FI = CreateHandleFromBindingFn->user_begin();
       FI != CreateHandleFromBindingFn->user_end();) {
    auto *FunctionUser = *FI++;
    auto instruction = cast<Instruction>(FunctionUser);
    Value *index = instruction->getOperand(
        DXIL::OperandIndex::kCreateHandleFromBindingResIndexOpIdx);
    if (!isa<Constant>(index)) {
      const DxilInst_CreateHandleFromBinding createHandle(instruction);
      if (!Visitor(createHandle.get_nonUniformIndex_val(), instruction,
                   index)) {
        return;
      }
    }
  }

  for (auto FI = CreateHandleFromHeapFn->user_begin();
       FI != CreateHandleFromHeapFn->user_end();) {
    auto *FunctionUser = *FI++;
    auto instruction = cast<Instruction>(FunctionUser);
    Value *index = instruction->getOperand(
        DXIL::OperandIndex::kCreateHandleFromHeapHeapIndexOpIdx);
    if (!isa<Constant>(index)) {
      const DxilInst_CreateHandleFromHeap createHandle(instruction);
      if (!Visitor(createHandle.get_nonUniformIndex_val(), instruction,
                   index)) {
        return;
      }
    }
  }
}

#ifdef PIX_DEBUG_DUMP_HELPER

static int g_logIndent = 0;
void IncreaseLogIndent() { g_logIndent++; }
void DecreaseLogIndent() { --g_logIndent; }

void Log(const char *format, ...) {
  va_list argumentPointer;
  va_start(argumentPointer, format);
  char buffer[512];
  vsnprintf(buffer, _countof(buffer), format, argumentPointer);
  va_end(argumentPointer);
  for (int i = 0; i < g_logIndent; ++i) {
    OutputDebugFormatA("    ");
  }
  OutputDebugFormatA(buffer);
  OutputDebugFormatA("\n");
}

void LogPartialLine(const char *format, ...) {
  va_list argumentPointer;
  va_start(argumentPointer, format);
  char buffer[512];
  vsnprintf(buffer, _countof(buffer), format, argumentPointer);
  va_end(argumentPointer);
  for (int i = 0; i < g_logIndent; ++i) {
    OutputDebugFormatA("    ");
  }
  OutputDebugFormatA(buffer);
}

static llvm::DIType const *DITypePeelTypeAlias(llvm::DIType const *Ty) {
  if (auto *DerivedTy = llvm::dyn_cast<llvm::DIDerivedType>(Ty)) {
    const llvm::DITypeIdentifierMap EmptyMap;
    switch (DerivedTy->getTag()) {
    case llvm::dwarf::DW_TAG_restrict_type:
    case llvm::dwarf::DW_TAG_reference_type:
    case llvm::dwarf::DW_TAG_const_type:
    case llvm::dwarf::DW_TAG_typedef:
    case llvm::dwarf::DW_TAG_pointer_type:
    case llvm::dwarf::DW_TAG_member:
      return DITypePeelTypeAlias(DerivedTy->getBaseType().resolve(EmptyMap));
    }
  }

  return Ty;
}

void DumpArrayType(llvm::DICompositeType const *Ty);
void DumpStructType(llvm::DICompositeType const *Ty);

void DumpFullType(llvm::DIType const *type) {
  auto *Ty = DITypePeelTypeAlias(type);

  const llvm::DITypeIdentifierMap EmptyMap;
  if (auto *DerivedTy = llvm::dyn_cast<llvm::DIDerivedType>(Ty)) {
    switch (DerivedTy->getTag()) {
    default:
      assert(!"Unhandled DIDerivedType");
      std::abort();
      return;
    case llvm::dwarf::DW_TAG_arg_variable: // "this" pointer
    case llvm::dwarf::DW_TAG_pointer_type: // "this" pointer
    case llvm::dwarf::DW_TAG_restrict_type:
    case llvm::dwarf::DW_TAG_reference_type:
    case llvm::dwarf::DW_TAG_const_type:
    case llvm::dwarf::DW_TAG_typedef:
    case llvm::dwarf::DW_TAG_inheritance:
      DumpFullType(DerivedTy->getBaseType().resolve(EmptyMap));
      return;
    case llvm::dwarf::DW_TAG_member: {
      Log("Member variable");
      ScopedIndenter indent;
      DumpFullType(DerivedTy->getBaseType().resolve(EmptyMap));
    }
      return;
    case llvm::dwarf::DW_TAG_subroutine_type:
      std::abort();
      return;
    }
  } else if (auto *CompositeTy = llvm::dyn_cast<llvm::DICompositeType>(Ty)) {
    switch (CompositeTy->getTag()) {
    default:
      assert(!"Unhandled DICompositeType");
      std::abort();
      return;
    case llvm::dwarf::DW_TAG_array_type:
      DumpArrayType(CompositeTy);
      return;
    case llvm::dwarf::DW_TAG_structure_type:
    case llvm::dwarf::DW_TAG_class_type:
      DumpStructType(CompositeTy);
      return;
    case llvm::dwarf::DW_TAG_enumeration_type:
      // enum base type is int:
      std::abort();
      return;
    }
  } else if (auto *BasicTy = llvm::dyn_cast<llvm::DIBasicType>(Ty)) {
    Log("%d: %s", BasicTy->getOffsetInBits(), BasicTy->getName().str().c_str());
    return;
  } else {
    std::abort();
  }
}

static unsigned NumArrayElements(llvm::DICompositeType const *Array) {
  if (Array->getElements().size() == 0) {
    return 0;
  }

  unsigned NumElements = 1;
  for (llvm::DINode *N : Array->getElements()) {
    if (auto *Subrange = llvm::dyn_cast<llvm::DISubrange>(N)) {
      NumElements *= Subrange->getCount();
    } else {
      assert(!"Unhandled array element");
      return 0;
    }
  }
  return NumElements;
}

void DumpArrayType(llvm::DICompositeType const *Ty) {
  unsigned NumElements = NumArrayElements(Ty);
  Log("Array %s: size: %d", Ty->getName().str().c_str(), NumElements);
  if (NumElements == 0) {
    std::abort();
    return;
  }

  const llvm::DITypeIdentifierMap EmptyMap;
  llvm::DIType *ElementTy = Ty->getBaseType().resolve(EmptyMap);
  ScopedIndenter indent;
  DumpFullType(ElementTy);
}

void DumpStructType(llvm::DICompositeType const *Ty) {
  Log("Struct %s", Ty->getName().str().c_str());
  ScopedIndenter indent;
  auto Elements = Ty->getElements();
  if (Elements.begin() == Elements.end()) {
    Log("Resource member: size %d", Ty->getSizeInBits());
    return;
  }
  for (auto *Element : Elements) {
    switch (Element->getTag()) {
    case llvm::dwarf::DW_TAG_member: {
      if (auto *Member = llvm::dyn_cast<llvm::DIDerivedType>(Element)) {
        DumpFullType(Member);
        break;
      }
      assert(!"member is not a Member");
      std::abort();
      return;
    }
    case llvm::dwarf::DW_TAG_subprogram: {
      if (auto *SubProgram = llvm::dyn_cast<llvm::DISubprogram>(Element)) {
        Log("Member function %s", SubProgram->getName().str().c_str());
        continue;
      }
      assert(!"DISubprogram not understood");
      std::abort();
      return;
    }
    case llvm::dwarf::DW_TAG_inheritance: {
      if (auto *Member = llvm::dyn_cast<llvm::DIDerivedType>(Element)) {
        DumpFullType(Member);
      } else {
        std::abort();
      }
      continue;
    }
    default:
      assert(!"Unhandled field type in DIStructType");
      std::abort();
    }
  }
}
#endif
} // namespace PIXPassHelpers
