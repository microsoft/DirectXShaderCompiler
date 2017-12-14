///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilShaderAccessTracking.cpp                                        //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides a pass to add instrumentation to determine pixel hit count and   //
// cost. Used by PIX.                                                        //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/HLSL/DxilGenerationPass.h"
#include "dxc/HLSL/DxilOperations.h"
#include "dxc/HLSL/DxilSignatureElement.h"
#include "dxc/HLSL/DxilModule.h"
#include "dxc/Support/Global.h"
#include "dxc/HLSL/DxilTypeSystem.h"
#include "dxc/HLSL/DxilConstants.h"
#include "dxc/HLSL/DxilInstructions.h"
#include "dxc/HLSL/DxilSpanAllocator.h"

#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/Pass.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Transforms/Utils/Local.h"
#include <memory>
#include <map>
#include <sstream> //test-remove
#include <deque>

//#include "dxc/Support/WinIncludes.h"
#include <winerror.h>

using namespace llvm;
using namespace hlsl;


void ThrowIf(bool a)
{
  if (a) {
    throw ::hlsl::Exception(E_INVALIDARG);
  }
}

//---------------------------------------------------------------------------------------------------------------------------------
// These types are taken from PIX's ShaderAccessHelpers.h

enum class ShaderAccessFlags : uint32_t
{
  None = 0,
  Read = 1 << 0,
  Write = 1 << 1,

  // "Counter" access is only applicable to UAVs; it means the counter buffer attached to the UAV
  // was accessed, but not necessarily the UAV resource.
  Counter = 1 << 2
};

enum class RegisterType
{
  CBV,
  SRV,
  UAV,
  RTV,
  DSV,
  Sampler,
  SOV,
  Invalid,
  Terminator
};

RegisterType RegisterTypeFromResourceClass(DXIL::ResourceClass c)
{
  switch (c)
  {
  case DXIL::ResourceClass::SRV    : return RegisterType::SRV    ; break;
  case DXIL::ResourceClass::UAV    : return RegisterType::UAV    ; break;
  case DXIL::ResourceClass::CBuffer: return RegisterType::CBV    ; break;
  case DXIL::ResourceClass::Sampler: return RegisterType::Sampler; break;
  case DXIL::ResourceClass::Invalid: return RegisterType::Invalid; break;
  default:
    ThrowIf(true);
    return RegisterType::Invalid;
  }
}

struct RegisterTypeAndSpace
{
  bool operator < (const RegisterTypeAndSpace & o) const {
    return static_cast<int>(Type) < static_cast<int>(o.Type) ||
      (static_cast<int>(Type) == static_cast<int>(o.Type) && Space < o.Space);
  }
  RegisterType Type;
  unsigned     Space;
};

// Identifies a bind point as defined by the root signature
struct RSRegisterIdentifier
{
  RegisterType Type;
  unsigned     Space;
  unsigned     Index;
};

struct SlotRange
{
  unsigned startSlot;
  unsigned numSlots;

  // Number of slots needed if no descriptors from unbounded ranges are included
  unsigned numInvariableSlots;
};

//---------------------------------------------------------------------------------------------------------------------------------

class DxilShaderAccessTracking : public ModulePass {
public:
  static char ID; // Pass identification, replacement for typeid
  explicit DxilShaderAccessTracking() : ModulePass(ID) {}
  const char *getPassName() const override { return "DXIL shader access tracking"; }
  bool runOnModule(Module &M) override;
  void applyOptions(PassOptions O) override;

private:
  void EmitAccess(LLVMContext & Ctx, OP *HlslOP, IRBuilder<> &, Value *slot, ShaderAccessFlags access);

private:
  bool m_CheckForDynamicIndexing = false;
  std::vector<std::pair<RSRegisterIdentifier, ShaderAccessFlags>> m_limitedAccessOutputs;
  std::map<RegisterTypeAndSpace, SlotRange> m_slotAssignments;
  std::vector<unsigned> m_testOutput;
  CallInst *m_HandleForUAV;
};

static unsigned DeserializeInt(std::deque<wchar_t> & q)
{
  unsigned i = 0;

  while(!q.empty() && iswdigit(q.front()))
  {
    i *= 10;
    i += q.front() - L'0';
    q.pop_front();
  }
  return i;
}

static wchar_t DequeFront(std::deque<wchar_t> & q) {
  ThrowIf(q.empty());
  auto c = q.front();
  q.pop_front();
  return c;
}

RegisterType ParseRegisterType(std::deque<wchar_t> & q) {
  switch (DequeFront(q))
  {
  case L'C': return RegisterType::CBV;
  case L'S': return RegisterType::SRV;
  case L'U': return RegisterType::UAV;
  case L'R': return RegisterType::RTV;
  case L'D': return RegisterType::DSV;
  case L'M': return RegisterType::Sampler;
  case L'O': return RegisterType::SOV;
  case L'I': return RegisterType::Invalid;
  default: return RegisterType::Terminator;
  }
}

void ValidateDelimiter(std::deque<wchar_t> & q, wchar_t d) {
  ThrowIf(q.front() != d);
  q.pop_front();
}

void DxilShaderAccessTracking::applyOptions(PassOptions O) {
  int checkForDynamic;
  GetPassOptionInt(O, "checkForDynamicIndexing", &checkForDynamic, 0);
  m_CheckForDynamicIndexing = checkForDynamic != 0;

  StringRef configOption;
  if (GetPassOption(O, "config", &configOption)) {
    std::deque<wchar_t> config;
    config.assign(configOption.begin(), configOption.end());

    // Parse slot assignments. Compare with PIX's ShaderAccessHelpers.cpp (TrackingConfiguration::SerializedRepresentation)
    RegisterType rt = ParseRegisterType(config);
    while (rt != RegisterType::Terminator)
    {
      RegisterTypeAndSpace rst;
      rst.Type = rt;

      rst.Space = DeserializeInt(config);
      ValidateDelimiter(config, L':');

      SlotRange sr;
      sr.startSlot = DeserializeInt(config);
      ValidateDelimiter(config, L':');

      sr.numSlots = DeserializeInt(config);
      ValidateDelimiter(config, L'i');

      sr.numInvariableSlots = DeserializeInt(config);
      ValidateDelimiter(config, L';');

      m_slotAssignments[rst] = sr;

      rt = ParseRegisterType(config);
    }

    // Parse limited access outputs
    rt = ParseRegisterType(config);
    while (rt != RegisterType::Terminator)
    {
      RSRegisterIdentifier rid;
      rid.Type = rt;

      rid.Space = DeserializeInt(config);
      ValidateDelimiter(config, L':');

      rid.Index = DeserializeInt(config);
      ValidateDelimiter(config, L':');

      unsigned AccessFlags = DeserializeInt(config);
      ValidateDelimiter(config, L';');

      m_limitedAccessOutputs.emplace_back(rid, static_cast<ShaderAccessFlags>(AccessFlags));

      rt = ParseRegisterType(config);
    }
  }
}

void DxilShaderAccessTracking::EmitAccess(LLVMContext & Ctx, OP *HlslOP, IRBuilder<> & Builder, Value * slot, ShaderAccessFlags access)
{
  // Slots are four bytes each:
  auto ByteIndex = Builder.CreateMul(slot, HlslOP->GetU32Const(4));

  // Insert the UAV increment instruction:

  Function* AtomicOpFunc = HlslOP->GetOpFunc(OP::OpCode::AtomicBinOp, Type::getInt32Ty(Ctx));
  Constant* AtomicBinOpcode = HlslOP->GetU32Const((unsigned)OP::OpCode::AtomicBinOp);
  Constant* AtomicAdd = HlslOP->GetU32Const((unsigned)DXIL::AtomicBinOpCode::Or);

  Constant* AccessValue = HlslOP->GetU32Const(static_cast<unsigned>(access));
  UndefValue* UndefArg = UndefValue::get(Type::getInt32Ty(Ctx));

  (void)Builder.CreateCall(AtomicOpFunc, {
      AtomicBinOpcode,// i32, ; opcode
      m_HandleForUAV, // %dx.types.Handle, ; resource handle
      AtomicAdd,      // i32, ; binary operation code : EXCHANGE, IADD, AND, OR, XOR, IMIN, IMAX, UMIN, UMAX
      ByteIndex,      // i32, ; coordinate c0: byte offset
      UndefArg,       // i32, ; coordinate c1 (unused)
      UndefArg,       // i32, ; coordinate c2 (unused)
      AccessValue     // i32); increment value
  }, "UAVOrResult");
}

bool DxilShaderAccessTracking::runOnModule(Module &M)
{
  // This pass adds instrumentation for shader access to resources

  DxilModule &DM = M.GetOrCreateDxilModule();
  LLVMContext & Ctx = M.getContext();
  OP *HlslOP = DM.GetOP();

  bool Modified = false;

  if (m_CheckForDynamicIndexing) {

    bool FoundDynamicIndexing = false;

    auto CreateHandleFn = HlslOP->GetOpFunc(DXIL::OpCode::CreateHandle, Type::getVoidTy(Ctx));
    auto CreateHandleUses = CreateHandleFn->uses();
    for (auto FI = CreateHandleUses.begin(); FI != CreateHandleUses.end(); ) {
      auto & FunctionUse = *FI++;
      auto FunctionUser = FunctionUse.getUser();
      auto instruction = cast<Instruction>(FunctionUser);
      Value * index = instruction->getOperand(3);
      if (!isa<Constant>(index)) {
        FoundDynamicIndexing = true;
        break;
      }
    }

    if (FoundDynamicIndexing) {
      if (OSOverride != nullptr) {
        formatted_raw_ostream FOS(*OSOverride);
        FOS << "true";
      }
    }
  }
  else {
    {
        IRBuilder<> Builder(DM.GetEntryFunction()->getEntryBlock().getFirstInsertionPt());

        unsigned int UAVResourceHandle = static_cast<unsigned int>(DM.GetUAVs().size());

        // Set up a UAV with structure of a single int
        SmallVector<llvm::Type*, 1> Elements{ Type::getInt32Ty(Ctx) };
        llvm::StructType *UAVStructTy = llvm::StructType::create(Elements, "class.RWStructuredBuffer");
        std::unique_ptr<DxilResource> pUAV = llvm::make_unique<DxilResource>();
        pUAV->SetGlobalName("PIX_CountUAVName");
        pUAV->SetGlobalSymbol(UndefValue::get(UAVStructTy->getPointerTo()));
        pUAV->SetID(UAVResourceHandle);
        pUAV->SetSpaceID((unsigned int)-2); // This is the reserved-for-tools register space
        pUAV->SetSampleCount(1);
        pUAV->SetGloballyCoherent(false);
        pUAV->SetHasCounter(false);
        pUAV->SetCompType(CompType::getI32());
        pUAV->SetLowerBound(0);
        pUAV->SetRangeSize(1);
        pUAV->SetKind(DXIL::ResourceKind::RawBuffer);

        auto pAnnotation = DM.GetTypeSystem().GetStructAnnotation(UAVStructTy);
        if (pAnnotation == nullptr)
        {
            pAnnotation = DM.GetTypeSystem().AddStructAnnotation(UAVStructTy);
            pAnnotation->GetFieldAnnotation(0).SetCBufferOffset(0);
            pAnnotation->GetFieldAnnotation(0).SetCompType(hlsl::DXIL::ComponentType::I32);
            pAnnotation->GetFieldAnnotation(0).SetFieldName("count");
        }

        ID = DM.AddUAV(std::move(pUAV));

        assert(ID == UAVResourceHandle);

        // Create handle for the newly-added UAV
        Function* CreateHandleOpFunc = HlslOP->GetOpFunc(DXIL::OpCode::CreateHandle, Type::getVoidTy(Ctx));
        Constant* CreateHandleOpcodeArg = HlslOP->GetU32Const((unsigned)DXIL::OpCode::CreateHandle);
        Constant* UAVArg = HlslOP->GetI8Const(static_cast<std::underlying_type<DxilResourceBase::Class>::type>(DXIL::ResourceClass::UAV));
        Constant* MetaDataArg = HlslOP->GetU32Const(ID); // position of the metadata record in the corresponding metadata list
        Constant* IndexArg = HlslOP->GetU32Const(0); // 
        Constant* FalseArg = HlslOP->GetI1Const(0); // non-uniform resource index: false
        m_HandleForUAV = Builder.CreateCall(CreateHandleOpFunc,
        { CreateHandleOpcodeArg, UAVArg, MetaDataArg, IndexArg, FalseArg }, "PIX_CountUAV_Handle");

        DM.ReEmitDxilResources();
    }

    struct ResourceAccessFunction
    {
      DXIL::OpCode opcode;
      ShaderAccessFlags readWrite;
      std::vector<Type*> overloads;
    };

    std::vector<Type*> voidType = { Type::getVoidTy(Ctx) };
    std::vector<Type*> i32 = { Type::getInt32Ty(Ctx) };
    std::vector<Type*> f16f32 = { Type::getHalfTy(Ctx), Type::getFloatTy(Ctx) };
    std::vector<Type*> f32i32 = { Type::getFloatTy(Ctx), Type::getInt32Ty(Ctx) };
    std::vector<Type*> f32i32f64 = { Type::getFloatTy(Ctx), Type::getInt32Ty(Ctx), Type::getDoubleTy(Ctx) };
    std::vector<Type*> f16f32i16i32 = { Type::getHalfTy(Ctx), Type::getFloatTy(Ctx), Type::getInt16Ty(Ctx), Type::getInt32Ty(Ctx) };
    std::vector<Type*> f16f32f64i16i32i64 = { Type::getHalfTy(Ctx), Type::getFloatTy(Ctx), Type::getDoubleTy(Ctx), Type::getInt16Ty(Ctx), Type::getInt32Ty(Ctx), Type::getInt64Ty(Ctx) };


    // todo: should "GetDimensions" mean a resource access?
    ResourceAccessFunction raFunctions[] = {
      { DXIL::OpCode::CBufferLoadLegacy     , ShaderAccessFlags::Read , f32i32f64 },
      { DXIL::OpCode::CBufferLoad           , ShaderAccessFlags::Read , f16f32f64i16i32i64 },
      { DXIL::OpCode::Sample                , ShaderAccessFlags::Read , f16f32 },
      { DXIL::OpCode::SampleBias            , ShaderAccessFlags::Read , f16f32 },
      { DXIL::OpCode::SampleLevel           , ShaderAccessFlags::Read , f16f32 },
      { DXIL::OpCode::SampleGrad            , ShaderAccessFlags::Read , f16f32 },
      { DXIL::OpCode::SampleCmp             , ShaderAccessFlags::Read , f16f32 },
      { DXIL::OpCode::SampleCmpLevelZero    , ShaderAccessFlags::Read , f16f32 },
      { DXIL::OpCode::TextureLoad           , ShaderAccessFlags::Read , f16f32i16i32 },
      { DXIL::OpCode::TextureStore          , ShaderAccessFlags::Write, f16f32i16i32 },
      { DXIL::OpCode::TextureGather         , ShaderAccessFlags::Read , f32i32 }, // todo: SM6: f16f32i16i32 },
      { DXIL::OpCode::TextureGatherCmp      , ShaderAccessFlags::Read , f32i32 }, // todo: SM6: f16f32i16i32 },
      { DXIL::OpCode::BufferLoad            , ShaderAccessFlags::Read , f32i32 },
      { DXIL::OpCode::RawBufferLoad         , ShaderAccessFlags::Read , f32i32 },
      { DXIL::OpCode::BufferStore           , ShaderAccessFlags::Write, f32i32 },
      { DXIL::OpCode::BufferUpdateCounter   , ShaderAccessFlags::Counter, voidType },
      { DXIL::OpCode::AtomicBinOp           , ShaderAccessFlags::Write, i32 },
      { DXIL::OpCode::AtomicCompareExchange , ShaderAccessFlags::Write, i32 },
    };

    //todo: don't forget StoreOutput

    for (const auto & raFunction : raFunctions) {
      for (const auto & Overload : raFunction.overloads) {
        Function * TheFunction = HlslOP->GetOpFunc(raFunction.opcode, Overload);
        auto TexLoadFunctionUses = TheFunction->uses();
        for (auto FI = TexLoadFunctionUses.begin(); FI != TexLoadFunctionUses.end(); ) {
          auto & FunctionUse = *FI++;
          auto FunctionUser = FunctionUse.getUser();
          auto instruction = cast<Instruction>(FunctionUser);

          auto * resHandle = instruction->getOperand(1);
          CallInst *handle = cast<CallInst>(resHandle);
          DxilInst_CreateHandle createHandle(handle);
          DXIL::ResourceClass ResClass =
            static_cast<DXIL::ResourceClass>(createHandle.get_resourceClass_val());
          // Dynamic rangeId is not supported - skip and let validation report the
          // error.
          if (!isa<ConstantInt>(createHandle.get_rangeId()))
            return false;

          unsigned rangeId =
            cast<ConstantInt>(createHandle.get_rangeId())->getLimitedValue();

          DxilResourceBase *res = nullptr;
          switch (ResClass) {
          case DXIL::ResourceClass::SRV:
            res = &DM.GetSRV(rangeId);
            break;
          case DXIL::ResourceClass::UAV:
            res = &DM.GetUAV(rangeId);
            break;
          case DXIL::ResourceClass::CBuffer:
            res = &DM.GetCBuffer(rangeId);
            break;
          case DXIL::ResourceClass::Sampler:
            res = &DM.GetSampler(rangeId);
            break;
          default:
            DXASSERT(0, "invalid res class");
            return false;
          }

          // Don't instrument the accesses to the UAV that we just added
          if (res->GetSpaceID() == (unsigned)-2) {
            continue;
          }

          auto slot = m_slotAssignments.find({ RegisterTypeFromResourceClass(ResClass), res->GetSpaceID() });
          // If the assignment isn't found, we assume 
          if(slot != m_slotAssignments.end()) {

            IRBuilder<> Builder(instruction);
            Value * slotIndex;

            if (isa<ConstantInt>(createHandle.get_index())) {
              unsigned index = cast<ConstantInt>(createHandle.get_index())->getLimitedValue();
              if (index > slot->second.numSlots) {
                // out-of-range accesses are written to slot zero:
                slotIndex = HlslOP->GetU32Const(0);
              }
              else {
                slotIndex = HlslOP->GetU32Const(slot->second.startSlot + index);
              }
            }
            else {
              // CompareWithSlotLimit will contain 1 if the access is out-of-bounds (both over- and and under-flow 
              // via the unsigned >= with slot count)
              auto CompareWithSlotLimit = Builder.CreateICmpUGE(createHandle.get_index(), HlslOP->GetU32Const(slot->second.numSlots), "CompareWithSlotLimit");
              auto CompareWithSlotLimitAsUint = Builder.CreateCast(Instruction::CastOps::ZExt, CompareWithSlotLimit, Type::getInt32Ty(Ctx), "CompareWithSlotLimitAsUint");

              // IsInBounds will therefore contain 0 if the access is out-of-bounds, and 1 otherwise.
              auto IsInBounds = Builder.CreateSub(HlslOP->GetU32Const(1), CompareWithSlotLimitAsUint, "IsInBounds");

              auto SlotOffset = Builder.CreateAdd(createHandle.get_index(), HlslOP->GetU32Const(slot->second.startSlot), "SlotOffset");

              // This will drive an out-of-bounds access slot down to 0
              slotIndex = Builder.CreateMul(SlotOffset, IsInBounds, "slotIndex");
            }

            EmitAccess(Ctx, HlslOP, Builder, slotIndex, raFunction.readWrite);
          }
        }
      }
    }

    std::ostringstream s;
    for (auto a : m_testOutput)
    {
      s << a << " ";
    }
    s << "\n";
    OutputDebugStringA(s.str().c_str());

    Modified = true;
  }

  return Modified;
}

char DxilShaderAccessTracking::ID = 0;

ModulePass *llvm::createDxilShaderAccessTrackingPass() {
  return new DxilShaderAccessTracking();
}

INITIALIZE_PASS(DxilShaderAccessTracking, "hlsl-dxil-pix-shader-access-instrumentation", "HLSL DXIL shader access tracking for PIX", false, false)
