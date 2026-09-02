///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilAddPixelHitInstrumentation.cpp                                        //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides a pass to add instrumentation to determine pixel hit count and   //
// cost. Used by PIX.                                                        //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/DXIL/DxilOperations.h"

#include "dxc/DXIL/DxilInstructions.h"
#include "dxc/DXIL/DxilModule.h"
#include "dxc/DXIL/DxilUtil.h"
#include "dxc/DxilPIXPasses/DxilPIXPasses.h"
#include "dxc/HLSL/DxilGenerationPass.h"

#include "llvm/IR/PassManager.h"
#include "llvm/Transforms/Utils/Local.h"

#include "PixPassHelpers.h"

#include "dxc/Support/Global.h"
#include <winerror.h>

using namespace llvm;
using namespace hlsl;

class DxilAddPixelHitInstrumentation : public ModulePass {

  bool ForceEarlyZ = false;
  bool AddPixelCost = false;
  int RTWidth = 1024;
  int NumPixels = 128;

public:
  static char ID; // Pass identification, replacement for typeid
  explicit DxilAddPixelHitInstrumentation() : ModulePass(ID) {}
  StringRef getPassName() const override {
    return "DXIL Add Pixel Hit Instrumentation";
  }
  void applyOptions(PassOptions O) override;
  bool runOnModule(Module &M) override;
  unsigned m_upstreamSVPositionRow = PIXPassHelpers::kUnknownSVPositionRow;
  PIXPassHelpers::SVPositionRowAuthority m_svPositionRowAuthority =
      PIXPassHelpers::SVPositionRowAuthority::Hint;
};

void DxilAddPixelHitInstrumentation::applyOptions(PassOptions O) {
  GetPassOptionBool(O, "force-early-z", &ForceEarlyZ, false);
  GetPassOptionBool(O, "add-pixel-cost", &AddPixelCost, false);
  GetPassOptionInt(O, "rt-width", &RTWidth, 0);
  GetPassOptionInt(O, "num-pixels", &NumPixels, 0);

  // RTWidth and NumPixels size the counter UAV and convert SV_Position to a
  // byte offset into it. Reject a width or pixel count this pass cannot
  // represent -- zero, negative, or large enough that the pixel-cost half's
  // high water mark (NumPixels * 2 * 4 bytes) does not fit in 32 bits --
  // instead of emitting a shader whose offset arithmetic silently wraps.
  if (RTWidth <= 0 || NumPixels <= 0 ||
      static_cast<uint64_t>(NumPixels) * 2 * 4 > UINT32_MAX) {
    throw ::hlsl::Exception(
        E_FAIL, "PIX: the pixel-hit instrumentation was given a render "
                "target width or pixel count it cannot represent.");
  }

  // This option always sets a hint, never a required row: treating an
  // unverified row as required could evict a real interpolant based on a
  // guess.
  //
  // GetPassOptionUnsigned leaves the value untouched when the option is
  // present but unparseable, so seed the member before the call rather than
  // rely on the default argument.
  //
  // "upstream-sv-position-row" is the pre-rename spelling: old PIX versions
  // predate this rename and still send it, so it is kept as an accepted
  // alias indefinitely rather than only for a deprecation window. New
  // callers should prefer "preferred-sv-position-row"; if both are
  // supplied, the preferred spelling wins.
  m_upstreamSVPositionRow = PIXPassHelpers::kUnknownSVPositionRow;
  if (!GetPassOptionUnsigned(O, "preferred-sv-position-row",
                             &m_upstreamSVPositionRow,
                             PIXPassHelpers::kUnknownSVPositionRow)) {
    GetPassOptionUnsigned(O, "upstream-sv-position-row",
                          &m_upstreamSVPositionRow,
                          PIXPassHelpers::kUnknownSVPositionRow);
  }
  m_svPositionRowAuthority = PIXPassHelpers::SVPositionRowAuthority::Hint;

  unsigned RequiredRow = PIXPassHelpers::kUnknownSVPositionRow;
  GetPassOptionUnsigned(O, "required-sv-position-row", &RequiredRow,
                        PIXPassHelpers::kUnknownSVPositionRow);
  if (RequiredRow != PIXPassHelpers::kUnknownSVPositionRow) {
    m_upstreamSVPositionRow = RequiredRow;
    m_svPositionRowAuthority =
        PIXPassHelpers::SVPositionRowAuthority::Authoritative;
  }
}

bool DxilAddPixelHitInstrumentation::runOnModule(Module &M) {
  // This pass adds instrumentation for pixel hit counting and pixel cost.

  DxilModule &DM = M.GetOrCreateDxilModule();
  LLVMContext &Ctx = M.getContext();
  OP *HlslOP = DM.GetOP();

  // ForceEarlyZ is incompatible with the discard function (the Z has to be
  // tested/written, and may be written before the shader even runs)
  if (ForceEarlyZ) {
    DM.m_ShaderFlags.SetForceEarlyDepthStencil(true);
  }

  auto SV_Position_ID = PIXPassHelpers::FindOrAddSV_Position(
      DM, m_upstreamSVPositionRow, m_svPositionRowAuthority);

  auto EntryPointFunction = PIXPassHelpers::GetEntryFunction(DM);

  CallInst *HandleForUAV;
  {
    IRBuilder<> Builder(dxilutil::FirstNonAllocaInsertionPt(
        PIXPassHelpers::GetEntryFunction(DM)));

    HandleForUAV = PIXPassHelpers::CreateUAVOnceForModule(
        DM, Builder, 0, "PIX_CountUAV_Handle");

    DM.ReEmitDxilResources();
  }
  // Every point where the shader completes must bump the counter. A
  // straight-line shader keeps its Ret in the entry block, but a shader with
  // a loop or branch ends the entry block early, so every basic block is
  // scanned for a Ret.
  llvm::SmallVector<llvm::Instruction *, 4> ReturnInstructions;
  bool FunctionHasWork = false;
  for (auto &ThisBlock : EntryPointFunction->getBasicBlockList()) {
    for (auto &ThisInstruction : ThisBlock) {
      LlvmInst_Ret Ret(&ThisInstruction);
      if (Ret) {
        ReturnInstructions.push_back(&ThisInstruction);
      } else if (!llvm::isa<llvm::TerminatorInst>(&ThisInstruction)) {
        FunctionHasWork = true;
      }
    }
  }

  bool Modified = false;

  for (auto ThisInstruction : ReturnInstructions) {
    LlvmInst_Ret Ret(ThisInstruction);
    if (Ret) {
      // A function that contains nothing but terminators has no pixel work
      // worth counting.
      if (FunctionHasWork) {
        Modified = true;

        // Start adding instructions right before the Ret:
        IRBuilder<> Builder(ThisInstruction);

        // ------------------------------------------------------------------------------------------------------------
        // Generate instructions to increment (by one) a UAV value corresponding
        // to the pixel currently being rendered
        // ------------------------------------------------------------------------------------------------------------

        // Useful constants
        Constant *Zero32Arg = HlslOP->GetU32Const(0);
        Constant *Zero8Arg = HlslOP->GetI8Const(0);
        Constant *One32Arg = HlslOP->GetU32Const(1);
        Constant *One8Arg = HlslOP->GetI8Const(1);
        UndefValue *UndefArg = UndefValue::get(Type::getInt32Ty(Ctx));
        // Compute as uint32_t, not NumPixels' own int: applyOptions
        // guarantees NumPixels * 2 * 4 fits in 32 bits only for unsigned
        // arithmetic. The signed multiply would overflow int32 for a
        // NumPixels this pass accepts, which is undefined behavior on the
        // host, not just a wrapped value in the shader.
        Constant *NumPixelsByteOffsetArg =
            HlslOP->GetU32Const(static_cast<uint32_t>(NumPixels) * 4u);

        // Step 1: Convert SV_POSITION to UINT
        Value *XAsInt;
        Value *YAsInt;
        {
          auto LoadInputOpFunc =
              HlslOP->GetOpFunc(DXIL::OpCode::LoadInput, Type::getFloatTy(Ctx));
          Constant *LoadInputOpcode =
              HlslOP->GetU32Const((unsigned)DXIL::OpCode::LoadInput);
          Constant *SV_Pos_ID = HlslOP->GetU32Const(SV_Position_ID);
          auto XPos =
              Builder.CreateCall(LoadInputOpFunc,
                                 {LoadInputOpcode, SV_Pos_ID, Zero32Arg /*row*/,
                                  Zero8Arg /*column*/, UndefArg},
                                 "XPos");
          auto YPos =
              Builder.CreateCall(LoadInputOpFunc,
                                 {LoadInputOpcode, SV_Pos_ID, Zero32Arg /*row*/,
                                  One8Arg /*column*/, UndefArg},
                                 "YPos");

          XAsInt = Builder.CreateCast(Instruction::CastOps::FPToUI, XPos,
                                      Type::getInt32Ty(Ctx), "XIndex");
          YAsInt = Builder.CreateCast(Instruction::CastOps::FPToUI, YPos,
                                      Type::getInt32Ty(Ctx), "YIndex");
        }

        // Step 2: Calculate pixel index
        Value *Index;
        {
          Constant *RTWidthArg =
              HlslOP->GetU32Const(static_cast<uint32_t>(RTWidth));
          auto YOffset = Builder.CreateMul(YAsInt, RTWidthArg, "YOffset");
          auto Elementoffset =
              Builder.CreateAdd(XAsInt, YOffset, "ElementOffset");

          // The viewport can be offset from the render target's origin, or
          // smaller than the counter buffer PIX sized for it, so
          // SV_Position's X and Y can land ElementOffset past the last valid
          // element. Clamp the element count before scaling to a byte
          // offset: applyOptions guarantees (NumPixels-1)*4 fits in uint32,
          // so the clamped multiply cannot wrap. Clamping after scaling
          // would let an oversized ElementOffset overflow the multiply
          // first.
          Function *UMinOpFunc =
              HlslOP->GetOpFunc(OP::OpCode::UMin, Type::getInt32Ty(Ctx));
          Constant *UMinOpcode =
              HlslOP->GetU32Const((unsigned)OP::OpCode::UMin);
          Constant *LastElementArg =
              HlslOP->GetU32Const(static_cast<uint32_t>(NumPixels) - 1);
          auto ClampedElementOffset = Builder.CreateCall(
              UMinOpFunc, {UMinOpcode, Elementoffset, LastElementArg},
              "ClampedElementOffset");
          Index = Builder.CreateMul(ClampedElementOffset,
                                    HlslOP->GetU32Const(4), "ByteIndex");
        }

        // Insert the UAV increment instruction:
        Function *AtomicOpFunc =
            HlslOP->GetOpFunc(OP::OpCode::AtomicBinOp, Type::getInt32Ty(Ctx));
        Constant *AtomicBinOpcode =
            HlslOP->GetU32Const((unsigned)OP::OpCode::AtomicBinOp);
        Constant *AtomicAdd =
            HlslOP->GetU32Const((unsigned)DXIL::AtomicBinOpCode::Add);
        {
          (void)Builder.CreateCall(
              AtomicOpFunc,
              {
                  AtomicBinOpcode, // i32, ; opcode
                  HandleForUAV,    // %dx.types.Handle, ; resource handle
                  AtomicAdd, // i32, ; binary operation code : EXCHANGE, IADD,
                             // AND, OR, XOR, IMIN, IMAX, UMIN, UMAX
                  Index,     // i32, ; coordinate c0: byte offset
                  UndefArg,  // i32, ; coordinate c1 (unused)
                  UndefArg,  // i32, ; coordinate c2 (unused)
                  One32Arg   // i32); increment value
              },
              "UAVIncResult");
        }

        if (AddPixelCost) {
          // ------------------------------------------------------------------------------------------------------------
          // Generate instructions to increment a value corresponding to the
          // current pixel in the second half of the UAV, by an amount
          // proportional to the estimated average cost of each pixel in the
          // current draw call.
          // ------------------------------------------------------------------------------------------------------------

          // Step 1: Retrieve weight value from UAV; it will be placed after the
          // range we're writing to
          Value *Weight;
          {
            Function *LoadWeight = HlslOP->GetOpFunc(OP::OpCode::BufferLoad,
                                                     Type::getInt32Ty(Ctx));
            Constant *LoadWeightOpcode =
                HlslOP->GetU32Const((unsigned)DXIL::OpCode::BufferLoad);
            Constant *OffsetIntoUAV =
                HlslOP->GetU32Const(static_cast<uint32_t>(NumPixels) * 2u * 4u);
            auto WeightStruct = Builder.CreateCall(
                LoadWeight,
                {
                    LoadWeightOpcode, // i32 opcode
                    HandleForUAV,     // %dx.types.Handle, ; resource handle
                    OffsetIntoUAV,    // i32 c0: byte offset
                    UndefArg          // i32 c1: unused
                },
                "WeightStruct");
            Weight = Builder.CreateExtractValue(
                WeightStruct, static_cast<uint64_t>(0LL), "Weight");
          }

          // Step 2: Update write position ("Index") to second half of the UAV.
          // Index is already clamped to the first half, so this can only land
          // in the second half without a clamp of its own.
          auto OffsetIndex = Builder.CreateAdd(Index, NumPixelsByteOffsetArg,
                                               "OffsetByteIndex");

          // Step 3: Increment UAV value by the weight
          (void)Builder.CreateCall(
              AtomicOpFunc,
              {
                  AtomicBinOpcode, // i32, ; opcode
                  HandleForUAV,    // %dx.types.Handle, ; resource handle
                  AtomicAdd,   // i32, ; binary operation code : EXCHANGE, IADD,
                               // AND, OR, XOR, IMIN, IMAX, UMIN, UMAX
                  OffsetIndex, // i32, ; coordinate c0: byte offset
                  UndefArg,    // i32, ; coordinate c1 (unused)
                  UndefArg,    // i32, ; coordinate c2 (unused)
                  Weight       // i32); increment value
              },
              "UAVIncResult2");
        }
      }
    }
  }

  return Modified;
}

char DxilAddPixelHitInstrumentation::ID = 0;

ModulePass *llvm::createDxilAddPixelHitInstrumentationPass() {
  return new DxilAddPixelHitInstrumentation();
}

INITIALIZE_PASS(DxilAddPixelHitInstrumentation,
                "hlsl-dxil-add-pixel-hit-instrmentation",
                "DXIL Count completed PS invocations and costs", false, false)
