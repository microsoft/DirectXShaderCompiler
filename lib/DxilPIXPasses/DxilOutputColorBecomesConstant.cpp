///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilOutputColorBecomesConstant.cpp                                        //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides a pass to stomp a pixel shader's output color to a given         //
// constant value                                                            //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxc/DXIL/DxilModule.h"
#include "dxc/DXIL/DxilOperations.h"
#include "dxc/DXIL/DxilTypeSystem.h"
#include "dxc/DxilPIXPasses/DxilPIXPasses.h"
#include "dxc/HLSL/DxilGenerationPass.h"
#include "dxc/HLSL/DxilSpanAllocator.h"

#include "llvm/IR/PassManager.h"
#include "llvm/Transforms/Utils/Local.h"
#include <array>

#include "PixPassHelpers.h"

using namespace llvm;
using namespace hlsl;

class DxilOutputColorBecomesConstant : public ModulePass {

  enum VisualizerInstrumentationMode {
    FromLiteralConstant,
    FromConstantBuffer
  };

  float Red = 1.f;
  float Green = 1.f;
  float Blue = 1.f;
  float Alpha = 1.f;
  VisualizerInstrumentationMode Mode = FromLiteralConstant;

  void visitOutputInstructionCallers(Function *OutputFunction,
                                     const hlsl::DxilSignature &OutputSignature,
                                     OP *HlslOP,
                                     std::function<void(CallInst *)> Visitor);

public:
  static char ID; // Pass identification, replacement for typeid
  explicit DxilOutputColorBecomesConstant() : ModulePass(ID) {}
  StringRef getPassName() const override { return "DXIL Constant Color Mod"; }
  void applyOptions(PassOptions O) override;
  bool runOnModule(Module &M) override;
};

void DxilOutputColorBecomesConstant::applyOptions(PassOptions O) {
  GetPassOptionFloat(O, "constant-red", &Red, 1.f);
  GetPassOptionFloat(O, "constant-green", &Green, 1.f);
  GetPassOptionFloat(O, "constant-blue", &Blue, 1.f);
  GetPassOptionFloat(O, "constant-alpha", &Alpha, 1.f);

  int mode = 0;
  GetPassOptionInt(O, "mod-mode", &mode, 0);
  Mode = static_cast<VisualizerInstrumentationMode>(mode);
}

void DxilOutputColorBecomesConstant::visitOutputInstructionCallers(
    Function *OutputFunction, const hlsl::DxilSignature &OutputSignature,
    OP *HlslOP, std::function<void(CallInst *)> Visitor) {

  auto OutputFunctionUses = OutputFunction->uses();

  for (Use &FunctionUse : OutputFunctionUses) {
    iterator_range<Value::user_iterator> FunctionUsers = FunctionUse->users();
    for (User *FunctionUser : FunctionUsers) {
      if (isa<Instruction>(FunctionUser)) {
        auto CallInstruction = cast<CallInst>(FunctionUser);

        // Check if the instruction writes to a render target (as opposed to a
        // system-value, such as RenderTargetArrayIndex)
        Value *OutputID = CallInstruction->getArgOperand(
            DXIL::OperandIndex::kStoreOutputIDOpIdx);
        unsigned SignatureElementIndex =
            cast<ConstantInt>(OutputID)->getLimitedValue();
        const DxilSignatureElement &SignatureElement =
            OutputSignature.GetElement(SignatureElementIndex);

        // We only modify the output color for RTV0
        if (SignatureElement.GetSemantic()->GetKind() ==
                DXIL::SemanticKind::Target &&
            SignatureElement.GetSemanticStartIndex() == 0) {

          // Replace the source operand with the appropriate constant value
          Visitor(CallInstruction);
        }
      }
    }
  }
}

bool DxilOutputColorBecomesConstant::runOnModule(Module &M) {
  // This pass finds all users of the "StoreOutput" function, and replaces their
  // source operands with a constant value.

  DxilModule &DM = M.GetOrCreateDxilModule();

  LLVMContext &Ctx = M.getContext();

  OP *HlslOP = DM.GetOP();

  const hlsl::DxilSignature &OutputSignature = DM.GetOutputSignature();

  // dx.op.storeOutput has four legal overloads: f16, f32, i16 and i32.
  // A min16float or min16int SV_Target lowers through the f16 or i16 form,
  // as does a native half or int16_t target under -enable-16bit-types.
  const std::array<llvm::Type *, 4> OverloadTypes{
      Type::getHalfTy(Ctx), Type::getFloatTy(Ctx), Type::getInt16Ty(Ctx),
      Type::getInt32Ty(Ctx)};

  std::array<Function *, 4> OutputFunctions{};
  size_t ActiveOverload = OverloadTypes.size();

  for (size_t OverloadIndex = 0; OverloadIndex < OverloadTypes.size();
       ++OverloadIndex) {
    OutputFunctions[OverloadIndex] = HlslOP->GetOpFunc(
        DXIL::OpCode::StoreOutput, OverloadTypes[OverloadIndex]);

    bool HasTargetZeroStores = false;
    visitOutputInstructionCallers(
        OutputFunctions[OverloadIndex], OutputSignature, HlslOP,
        [&HasTargetZeroStores](CallInst *) { HasTargetZeroStores = true; });

    if (HasTargetZeroStores) {
      // visitOutputInstructionCallers filters on SemanticKind::Target with
      // GetSemanticStartIndex() == 0, so at most one overload writes
      // SV_Target0.
      DXASSERT(ActiveOverload == OverloadTypes.size(),
               "Only one storeOutput overload can write SV_Target0");
      ActiveOverload = OverloadIndex;
    }
  }

  // GetOpFunc materialises each overload declaration on demand. Any
  // overload with no callers must be erased before the pass returns; the
  // validator rejects a module carrying an unused dx.op declaration.
  struct EraseUnusedOutputFunctionsOnExit {
    hlsl::DxilModule &DM;
    std::array<Function *, 4> &OutputFunctions;
    ~EraseUnusedOutputFunctionsOnExit() {
      for (Function *OutputFunction : OutputFunctions) {
        PIXPassHelpers::EraseIfUnused(DM, OutputFunction);
      }
    }
  } EraseUnusedOutputFunctions{DM, OutputFunctions};

  if (ActiveOverload == OverloadTypes.size()) {
    return false;
  }

  // Replacement values must match the store's own overload type.
  llvm::Type *const OutputValueType = OverloadTypes[ActiveOverload];
  const bool IsFloatOutput = OutputValueType->isFloatingPointTy();

  std::array<llvm::Value *, 4> ReplacementColors;

  switch (Mode) {
  case FromLiteralConstant: {
    const std::array<float, 4> Channels{Red, Green, Blue, Alpha};
    for (size_t ChannelIndex = 0; ChannelIndex < Channels.size();
         ++ChannelIndex) {
      ReplacementColors[ChannelIndex] =
          IsFloatOutput
              ? ConstantFP::get(OutputValueType, Channels[ChannelIndex])
              : ConstantInt::get(OutputValueType,
                                 static_cast<uint64_t>(static_cast<int64_t>(
                                     Channels[ChannelIndex])),
                                 /*isSigned*/ true);
    }
  } break;
  case FromConstantBuffer: {

    // A float4 constant buffer row is 16 bytes wide.
    constexpr unsigned int ConstantColorCBufferSizeInBytes = 4 * sizeof(float);

    // Setup a constant buffer with a single float4 in it:
    SmallVector<llvm::Type *, 4> Elements{
        Type::getFloatTy(Ctx), Type::getFloatTy(Ctx), Type::getFloatTy(Ctx),
        Type::getFloatTy(Ctx)};
    llvm::StructType *CBStructTy =
        llvm::StructType::create(Elements, "PIX_ConstantColorCB_Type");
    std::unique_ptr<DxilCBuffer> pCBuf = llvm::make_unique<DxilCBuffer>();
    pCBuf->SetGlobalName("PIX_ConstantColorCBName");
    // The global symbol and HLSL type must be pointers to the struct so
    // ValidateCBuffer can reach the annotation.
    pCBuf->SetGlobalSymbol(UndefValue::get(CBStructTy->getPointerTo()));
    pCBuf->SetHLSLType(CBStructTy->getPointerTo());
    pCBuf->SetID(static_cast<unsigned int>(DM.GetCBuffers().size()));
    pCBuf->SetSpaceID(
        (unsigned int)-2); // This is the reserved-for-tools register space
    pCBuf->SetLowerBound(0);
    pCBuf->SetRangeSize(1);
    pCBuf->SetSize(ConstantColorCBufferSizeInBytes);

    auto *StructAnnotation = DM.GetTypeSystem().GetStructAnnotation(CBStructTy);
    if (StructAnnotation == nullptr) {
      StructAnnotation = DM.GetTypeSystem().AddStructAnnotation(CBStructTy);
      StructAnnotation->SetCBufferSize(ConstantColorCBufferSizeInBytes);
      static const char *const ComponentNames[] = {"r", "g", "b", "a"};
      for (unsigned int ComponentIndex = 0; ComponentIndex < 4;
           ++ComponentIndex) {
        auto &FieldAnnotation =
            StructAnnotation->GetFieldAnnotation(ComponentIndex);
        FieldAnnotation.SetCBufferOffset(ComponentIndex * sizeof(float));
        FieldAnnotation.SetCompType(hlsl::DXIL::ComponentType::F32);
        FieldAnnotation.SetFieldName(ComponentNames[ComponentIndex]);
      }
    }

    Instruction *entryPointInstruction =
        &*(PIXPassHelpers::GetEntryFunction(DM)->begin()->begin());
    IRBuilder<> Builder(entryPointInstruction);

    // Create handle for the newly-added constant buffer (which is achieved via
    // a function call)
    auto ConstantBufferName = "PIX_Constant_Color_CB_Handle";

    CallInst *callCreateHandle = PIXPassHelpers::CreateHandleForResource(
        DM, Builder, pCBuf.get(), ConstantBufferName);

    DM.AddCBuffer(std::move(pCBuf));

    DM.ReEmitDxilResources();

#define PIX_CONSTANT_VALUE "PIX_Constant_Color_Value"

    // Insert the Buffer load instruction:
    // The tools constant buffer is always four 32-bit components; PIX
    // uploads that layout.
    llvm::Type *const CBufferComponentType =
        IsFloatOutput ? Type::getFloatTy(Ctx) : Type::getInt32Ty(Ctx);
    Function *CBLoad =
        HlslOP->GetOpFunc(OP::OpCode::CBufferLoadLegacy, CBufferComponentType);
    Constant *OpArg =
        HlslOP->GetU32Const((unsigned)OP::OpCode::CBufferLoadLegacy);
    Value *ResourceHandle = callCreateHandle;
    Constant *RowIndex = HlslOP->GetU32Const(0);
    CallInst *loadLegacy = Builder.CreateCall(
        CBLoad, {OpArg, ResourceHandle, RowIndex}, PIX_CONSTANT_VALUE);

    // Now extract four color values:
    ReplacementColors[0] =
        Builder.CreateExtractValue(loadLegacy, 0, PIX_CONSTANT_VALUE "0");
    ReplacementColors[1] =
        Builder.CreateExtractValue(loadLegacy, 1, PIX_CONSTANT_VALUE "1");
    ReplacementColors[2] =
        Builder.CreateExtractValue(loadLegacy, 2, PIX_CONSTANT_VALUE "2");
    ReplacementColors[3] =
        Builder.CreateExtractValue(loadLegacy, 3, PIX_CONSTANT_VALUE "3");

    // Narrow the loaded components to a 16-bit output overload.
    if (OutputValueType != CBufferComponentType) {
      static const char *const NarrowedNames[] = {
          PIX_CONSTANT_VALUE "Narrowed0", PIX_CONSTANT_VALUE "Narrowed1",
          PIX_CONSTANT_VALUE "Narrowed2", PIX_CONSTANT_VALUE "Narrowed3"};
      for (size_t ChannelIndex = 0; ChannelIndex < ReplacementColors.size();
           ++ChannelIndex) {
        ReplacementColors[ChannelIndex] =
            IsFloatOutput
                ? Builder.CreateFPTrunc(ReplacementColors[ChannelIndex],
                                        OutputValueType,
                                        NarrowedNames[ChannelIndex])
                : Builder.CreateTrunc(ReplacementColors[ChannelIndex],
                                      OutputValueType,
                                      NarrowedNames[ChannelIndex]);
      }
    }
  } break;
  default:
    assert(false);
    return false;
  }

  bool Modified = false;

  visitOutputInstructionCallers(
      OutputFunctions[ActiveOverload], OutputSignature, HlslOP,
      [&ReplacementColors, &Modified](CallInst *CallInstruction) {
        Modified = true;
        // The output column is the channel (red, green, blue or alpha) within
        // the output pixel
        Value *OutputColumnOperand = CallInstruction->getOperand(
            hlsl::DXIL::OperandIndex::kStoreOutputColOpIdx);
        ConstantInt *OutputColumnConstant =
            cast<ConstantInt>(OutputColumnOperand);
        APInt OutputColumn = OutputColumnConstant->getValue();
        CallInstruction->setOperand(
            hlsl::DXIL::OperandIndex::kStoreOutputValOpIdx,
            ReplacementColors[*OutputColumn.getRawData()]);
      });

  return Modified;
}

char DxilOutputColorBecomesConstant::ID = 0;

ModulePass *llvm::createDxilOutputColorBecomesConstantPass() {
  return new DxilOutputColorBecomesConstant();
}

INITIALIZE_PASS(DxilOutputColorBecomesConstant, "hlsl-dxil-constantColor",
                "DXIL Constant Color Mod", false, false)
