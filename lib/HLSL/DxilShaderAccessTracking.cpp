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
#include <unordered_set>
#include <array>
#include <sstream> //test-remove

using namespace llvm;
using namespace hlsl;

class DxilShaderAccessTracking : public ModulePass {
public:
  static char ID; // Pass identification, replacement for typeid
  explicit DxilShaderAccessTracking() : ModulePass(ID) {}
  const char *getPassName() const override { return "DXIL shader access tracking"; }
  bool runOnModule(Module &M) override;
};

bool DxilShaderAccessTracking::runOnModule(Module &M)
{
  // This pass adds instrumentation for shader access to resources

  DxilModule &DM = M.GetOrCreateDxilModule();
  LLVMContext & Ctx = M.getContext();
  OP *HlslOP = DM.GetOP();

  enum class ReadWrite { Read, Write };
  struct ResourceAccessFunction
  {
    DXIL::OpCode opcode;
    ReadWrite readWrite;
    std::vector<Type*> overloads;
  };

  std::vector<Type*> voidType = { Type::getVoidTy(Ctx) };
  std::vector<Type*> i32 = { Type::getInt32Ty(Ctx) };
  std::vector<Type*> f16f32 = { Type::getHalfTy(Ctx), Type::getFloatTy(Ctx) };
  std::vector<Type*> f32i32 = { Type::getFloatTy(Ctx), Type::getInt32Ty(Ctx) };
  std::vector<Type*> f32i32f64 = { Type::getFloatTy(Ctx), Type::getInt32Ty(Ctx), Type::getDoubleTy(Ctx) };
  std::vector<Type*> f16f32i16i32 = { Type::getHalfTy(Ctx), Type::getFloatTy(Ctx), Type::getInt16Ty(Ctx), Type::getInt32Ty(Ctx) };
  std::vector<Type*> f16f32f64i16i32i64 = { Type::getHalfTy(Ctx), Type::getFloatTy(Ctx), Type::getDoubleTy(Ctx), Type::getInt16Ty(Ctx), Type::getInt32Ty(Ctx), Type::getInt64Ty(Ctx) };


  ResourceAccessFunction raFunctions[] = {
    { DXIL::OpCode::CBufferLoadLegacy     , ReadWrite::Read , f32i32f64 },
    { DXIL::OpCode::CBufferLoad           , ReadWrite::Read , f16f32f64i16i32i64 },
    { DXIL::OpCode::Sample                , ReadWrite::Read , f16f32 },
    { DXIL::OpCode::SampleBias            , ReadWrite::Read , f16f32 },
    { DXIL::OpCode::SampleLevel           , ReadWrite::Read , f16f32 },
    { DXIL::OpCode::SampleGrad            , ReadWrite::Read , f16f32 },
    { DXIL::OpCode::SampleCmp             , ReadWrite::Read , f16f32 },
    { DXIL::OpCode::SampleCmpLevelZero    , ReadWrite::Read , f16f32 },
    { DXIL::OpCode::TextureLoad           , ReadWrite::Read , f16f32i16i32 },
    { DXIL::OpCode::TextureStore          , ReadWrite::Write, f16f32i16i32 },
    { DXIL::OpCode::TextureGather         , ReadWrite::Read , f32i32 }, // todo: SM6: f16f32i16i32 },
    { DXIL::OpCode::TextureGatherCmp      , ReadWrite::Read , f32i32 }, // todo: SM6: f16f32i16i32 },
    { DXIL::OpCode::BufferLoad            , ReadWrite::Read , f32i32 },
    { DXIL::OpCode::BufferStore           , ReadWrite::Write, f32i32 },
    { DXIL::OpCode::BufferUpdateCounter   , ReadWrite::Write, voidType },
    { DXIL::OpCode::AtomicBinOp           , ReadWrite::Write, i32 },
    { DXIL::OpCode::AtomicCompareExchange , ReadWrite::Write, i32 },
  };

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

        

        if (OSOverride != nullptr) {
          formatted_raw_ostream FOS(*OSOverride);
          FOS << "(" << res->GetSpaceID() << "," << res->GetID() << ")\n";
          std::ostringstream s;
          s << "(" << res->GetSpaceID() << "," << res->GetID() << ")\n";
          OutputDebugStringA(s.str().c_str());
        }
      }
    }
  }

  CallInst *HandleForUAV;
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
    HandleForUAV = Builder.CreateCall(CreateHandleOpFunc,
    { CreateHandleOpcodeArg, UAVArg, MetaDataArg, IndexArg, FalseArg }, "PIX_CountUAV_Handle");

    DM.ReEmitDxilResources();
  }
  // todo: is it a reasonable assumption that there will be a "Ret" in the entry block, and that these are the only
  // points from which the shader can exit (except for a pixel-kill?)
  //// Start adding instructions right before the Ret:
  //      IRBuilder<> Builder(ThisInstruction);
  //
  //      // ------------------------------------------------------------------------------------------------------------
  //      // Generate instructions to increment (by one) a UAV value corresponding to the pixel currently being rendered
  //      // ------------------------------------------------------------------------------------------------------------
  //
  //      // Useful constants
  //      Constant* Zero32Arg = HlslOP->GetU32Const(0);
  //      Constant* Zero8Arg = HlslOP->GetI8Const(0);
  //      Constant* One32Arg = HlslOP->GetU32Const(1);
  //      Constant* One8Arg = HlslOP->GetI8Const(1);
  //      UndefValue* UndefArg = UndefValue::get(Type::getInt32Ty(Ctx));
  //      Constant* NumPixelsByteOffsetArg = HlslOP->GetU32Const(NumPixels * 4);
  //
  //      // Step 1: Convert SV_POSITION to UINT          
  //      Value * XAsInt;
  //      Value * YAsInt;
  //      {
  //        auto LoadInputOpFunc = HlslOP->GetOpFunc(DXIL::OpCode::LoadInput, Type::getFloatTy(Ctx));
  //        Constant* LoadInputOpcode = HlslOP->GetU32Const((unsigned)DXIL::OpCode::LoadInput);
  //        Constant*  SV_Pos_ID = HlslOP->GetU32Const(SV_Position_ID);
  //        auto XPos = Builder.CreateCall(LoadInputOpFunc,
  //        { LoadInputOpcode, SV_Pos_ID, Zero32Arg /*row*/, Zero8Arg /*column*/, UndefArg }, "XPos");
  //        auto YPos = Builder.CreateCall(LoadInputOpFunc,
  //        { LoadInputOpcode, SV_Pos_ID, Zero32Arg /*row*/, One8Arg /*column*/, UndefArg }, "YPos");
  //
  //        XAsInt = Builder.CreateCast(Instruction::CastOps::FPToUI, XPos, Type::getInt32Ty(Ctx), "XIndex");
  //        YAsInt = Builder.CreateCast(Instruction::CastOps::FPToUI, YPos, Type::getInt32Ty(Ctx), "YIndex");
  //      }
  //
  //      // Step 2: Calculate pixel index
  //      Value * Index;
  //      {
  //        Constant* RTWidthArg = HlslOP->GetI32Const(RTWidth);
  //        auto YOffset = Builder.CreateMul(YAsInt, RTWidthArg, "YOffset");
  //        auto Elementoffset = Builder.CreateAdd(XAsInt, YOffset, "ElementOffset");
  //        Index = Builder.CreateMul(Elementoffset, HlslOP->GetU32Const(4), "ByteIndex");
  //      }
  //
  //      // Insert the UAV increment instruction:
  //      Function* AtomicOpFunc = HlslOP->GetOpFunc(OP::OpCode::AtomicBinOp, Type::getInt32Ty(Ctx));
  //      Constant* AtomicBinOpcode = HlslOP->GetU32Const((unsigned)OP::OpCode::AtomicBinOp);
  //      Constant* AtomicAdd = HlslOP->GetU32Const((unsigned)DXIL::AtomicBinOpCode::Add);
  //      {
  //        (void)Builder.CreateCall(AtomicOpFunc, {
  //          AtomicBinOpcode,// i32, ; opcode
  //          HandleForUAV,   // %dx.types.Handle, ; resource handle
  //          AtomicAdd,      // i32, ; binary operation code : EXCHANGE, IADD, AND, OR, XOR, IMIN, IMAX, UMIN, UMAX
  //          Index,          // i32, ; coordinate c0: byte offset
  //          UndefArg,       // i32, ; coordinate c1 (unused)
  //          UndefArg,       // i32, ; coordinate c2 (unused)
  //          One32Arg        // i32); increment value
  //        }, "UAVIncResult");
  //      }
  //

  bool Modified = false;

  return Modified;
}

char DxilShaderAccessTracking::ID = 0;

ModulePass *llvm::createDxilShaderAccessTrackingPass() {
  return new DxilShaderAccessTracking();
}

INITIALIZE_PASS(DxilShaderAccessTracking, "hlsl-dxil-shader-access-tracking", "HLSL DXIL shader access tracking for PIX", false, false)
