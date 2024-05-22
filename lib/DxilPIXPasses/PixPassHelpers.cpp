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
#include "dxc/DXIL/DxilModule.h"
#include "dxc/DXIL/DxilOperations.h"
#include "dxc/DXIL/DxilResourceBinding.h"
#include "dxc/DXIL/DxilResourceProperties.h"
#include "dxc/DxilRootSignature/DxilRootSignature.h"
#include "dxc/HLSL/DxilSpanAllocator.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"

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

static unsigned int
GetNextRegisterIdForClass(hlsl::DxilModule &DM,
                          DXIL::ResourceClass resourceClass) {
  switch (resourceClass) {
  case DXIL::ResourceClass::CBuffer:
    return static_cast<unsigned int>(DM.GetCBuffers().size());
  case DXIL::ResourceClass::UAV:
    return static_cast<unsigned int>(DM.GetUAVs().size());
  default:
    DXASSERT(false, "Unexpected resource class");
    return 0;
  }
}

static bool IsDynamicResourceShaderModel(DxilModule &DM) {
  return DM.GetShaderModel()->IsSMAtLeast(6, 6);
}

static bool ShaderModelRequiresAnnotateHandle(DxilModule &DM) {
  return DM.GetShaderModel()->IsSMAtLeast(6, 6);
}

llvm::CallInst *CreateHandleForResource(hlsl::DxilModule &DM,
                                        llvm::IRBuilder<> &Builder,
                                        hlsl::DxilResourceBase *resource,
                                        const char *name) {

  OP *HlslOP = DM.GetOP();
  LLVMContext &Ctx = DM.GetModule()->getContext();

  DXIL::ResourceClass resourceClass = resource->GetClass();

  unsigned int resourceMetaDataId =
      GetNextRegisterIdForClass(DM, resourceClass);

  auto const *shaderModel = DM.GetShaderModel();
  if (shaderModel->IsLib()) {
    llvm::Constant *object = resource->GetGlobalSymbol();
    auto *load = Builder.CreateLoad(object);
    Function *CreateHandleForLibOpFunc =
        HlslOP->GetOpFunc(DXIL::OpCode::CreateHandleForLib,
                          resource->GetHLSLType()->getVectorElementType());
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
    Constant *MetaDataArg = HlslOP->GetU32Const(
        resourceMetaDataId); // position of the metadata record in the
                             // corresponding metadata list
    Constant *IndexArg = HlslOP->GetU32Const(0); //
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
  auto const *serializedData = reinterpret_cast<const uint8_t *>(
      serializedRootSignature->GetBufferPointer());
  ret.assign(serializedData,
             serializedData + serializedRootSignature->GetBufferSize());

  return ret;
}

constexpr uint32_t toolsRegisterSpace = static_cast<uint32_t>(-2);
constexpr uint32_t toolsUAVRegister = 0;

template <typename RootSigDesc, typename RootParameterDesc>
void ExtendRootSig(RootSigDesc &rootSigDesc) {
  auto *existingParams = rootSigDesc.pParameters;
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

static std::vector<uint8_t> AddUAVParamterToRootSignature(const void *Data,
                                                          uint32_t Size) {
  DxilVersionedRootSignature rootSignature;
  DeserializeRootSignature(Data, Size, rootSignature.get_address_of());
  auto *rs = rootSignature.get_mutable();
  switch (rootSignature->Version) {
  case DxilRootSignatureVersion::Version_1_0:
    ExtendRootSig<DxilRootSignatureDesc, DxilRootParameter>(rs->Desc_1_0);
    break;
  case DxilRootSignatureVersion::Version_1_1:
    ExtendRootSig<DxilRootSignatureDesc1, DxilRootParameter1>(rs->Desc_1_1);
    rs->Desc_1_1.pParameters[rs->Desc_1_1.NumParameters - 1].Descriptor.Flags =
        hlsl::DxilRootDescriptorFlags::None;
    break;
  }
  return SerializeRootSignatureToVector(rs);
}

static void AddUAVToShaderAttributeRootSignature(DxilModule &DM) {
  auto rs = DM.GetSerializedRootSignature();
  if (!rs.empty()) {
    std::vector<uint8_t> asVector = AddUAVParamterToRootSignature(
        rs.data(), static_cast<uint32_t>(rs.size()));
    DM.ResetSerializedRootSignature(asVector);
  }
}

static void AddUAVToDxilDefinedGlobalRootSignatures(DxilModule &DM) {
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
          auto extendedRootSig = AddUAVParamterToRootSignature(Data, Size);
          auto rootSignatureSubObjectName = subObject.first;
          subObjects->RemoveSubobject(rootSignatureSubObjectName);
          subObjects->CreateRootSignature(
              rootSignatureSubObjectName, notALocalRS, extendedRootSig.data(),
              static_cast<uint32_t>(extendedRootSig.size()));
          break;
        }
      }
    }
  }
}

// Set up a UAV with structure of a single int
llvm::CallInst *CreateUAV(DxilModule &DM, IRBuilder<> &Builder,
                          unsigned int registerId, const char *name) {
  LLVMContext &Ctx = DM.GetModule()->getContext();

  const char *PIXStructTypeName = "struct.RWByteAddressBuffer";
  llvm::StructType *UAVStructTy =
      DM.GetModule()->getTypeByName(PIXStructTypeName);
  if (UAVStructTy == nullptr) {
    SmallVector<llvm::Type *, 1> Elements{Type::getInt32Ty(Ctx)};
    UAVStructTy = llvm::StructType::create(Elements, PIXStructTypeName);

    // Since we only have to do this once per module, we can do it now when
    // we're adding the singular UAV structure type to the module:
    AddUAVToDxilDefinedGlobalRootSignatures(DM);
    AddUAVToShaderAttributeRootSignature(DM);
  }

  std::unique_ptr<DxilResource> pUAV = llvm::make_unique<DxilResource>();

  auto const *shaderModel = DM.GetShaderModel();
  if (shaderModel->IsLib()) {
    auto *Global = DM.GetModule()->getOrInsertGlobal(
        ("PIXUAV" + std::to_string(registerId)).c_str(), UAVStructTy);
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
  pUAV->SetID(GetNextRegisterIdForClass(DM, DXIL::ResourceClass::UAV));
  pUAV->SetRW(true); // sets UAV class
  pUAV->SetSpaceID(
      (unsigned int)-2); // This is the reserved-for-tools register space
  pUAV->SetSampleCount(1);
  pUAV->SetGloballyCoherent(false);
  pUAV->SetHasCounter(false);
  pUAV->SetCompType(CompType::getI32());
  pUAV->SetLowerBound(registerId);
  pUAV->SetRangeSize(1);
  pUAV->SetKind(DXIL::ResourceKind::RawBuffer);

  auto pAnnotation = DM.GetTypeSystem().GetStructAnnotation(UAVStructTy);
  if (pAnnotation == nullptr) {

    pAnnotation = DM.GetTypeSystem().AddStructAnnotation(UAVStructTy);
    pAnnotation->GetFieldAnnotation(0).SetCBufferOffset(0);
    pAnnotation->GetFieldAnnotation(0).SetCompType(
        hlsl::DXIL::ComponentType::I32);
    pAnnotation->GetFieldAnnotation(0).SetFieldName("count");
  }

  auto *handle = CreateHandleForResource(DM, Builder, pUAV.get(), name);

  DM.AddUAV(std::move(pUAV));

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

std::vector<llvm::BasicBlock *> GetAllBlocks(hlsl::DxilModule &DM) {
  std::vector<llvm::BasicBlock *> ret;
  auto entryPoints = DM.GetExportedFunctions();
  for (auto &fn : entryPoints) {
    auto &blocks = fn->getBasicBlockList();
    for (auto &block : blocks) {
      ret.push_back(&block);
    }
  }
  return ret;
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

unsigned int FindOrAddSV_Position(hlsl::DxilModule &DM,
                                  unsigned UpStreamSVPosRow) {
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
  // If not present, we add it.
  if (Existing_SV_Position == InputElements.end()) {
    unsigned int StartColumn = 0;
    unsigned int RowCount = 1;
    unsigned int ColumnCount = 4;
    auto Added_SV_Position =
        llvm::make_unique<DxilSignatureElement>(DXIL::SigPointKind::PSIn);
    Added_SV_Position->Initialize("Position", hlsl::CompType::getF32(),
                                  hlsl::DXIL::InterpolationMode::Linear,
                                  RowCount, ColumnCount, UpStreamSVPosRow,
                                  StartColumn);
    Added_SV_Position->AppendSemanticIndex(0);
    Added_SV_Position->SetKind(hlsl::DXIL::SemanticKind::Position);
    // AppendElement sets the element's ID by default
    auto index = InputSignature.AppendElement(std::move(Added_SV_Position));
    return InputElements[index]->GetID();
  } else {
    return Existing_SV_Position->get()->GetID();
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
