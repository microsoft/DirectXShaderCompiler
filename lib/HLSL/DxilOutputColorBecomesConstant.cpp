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

#include "dxc/HLSL/DxilGenerationPass.h"
#include "dxc/HLSL/DxilOperations.h"
#include "dxc/HLSL/DxilSignatureElement.h"
#include "dxc/HLSL/DxilModule.h"
#include "dxc/Support/Global.h"
#include "dxc/HLSL/DxilTypeSystem.h"
#include "dxc/HLSL/DxilInstructions.h"
#include "dxc/HLSL/DxilSpanAllocator.h"

#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/Utils/Local.h"
#include <memory>
#include <unordered_set>
#include <array>

using namespace llvm;
using namespace hlsl;

class DxilOutputColorBecomesConstant : public ModulePass {

  enum VisualizerInstrumentationMode
  {
    FromLiteralConstant,
    FromConstantBuffer
  };

  float Red = 1.f;
  float Green = 1.f;
  float Blue = 1.f;
  float Alpha = 1.f;
  VisualizerInstrumentationMode Mode = FromLiteralConstant;

  void visitOutputInstructionCallers(
    Function * OutputFunction, 
    const hlsl::DxilSignature &OutputSignature, 
    OP * HlslOP, 
    std::function<void(CallInst*)> Visitor);

public:
  static char ID; // Pass identification, replacement for typeid
  explicit DxilOutputColorBecomesConstant() : ModulePass(ID) {}
  const char *getPassName() const override { return "DXIL Constant Color Mod"; }
  void applyOptions(PassOptions O) override;
  bool runOnModule(Module &M) override;
};

void DxilOutputColorBecomesConstant::applyOptions(PassOptions O)
{
  for (const auto & option : O)
  {
    if (0 == option.first.compare("constant-red"))
    {
      Red = atof(option.second.data());
    }
    else if (0 == option.first.compare("constant-green"))
    {
      Green = atof(option.second.data());
    }
    else if (0 == option.first.compare("constant-blue"))
    {
      Blue = atof(option.second.data());
    }
    else if (0 == option.first.compare("constant-alpha"))
    {
      Alpha = atof(option.second.data());
    }
    else if (0 == option.first.compare("mod-mode"))
    {
      Mode = static_cast<VisualizerInstrumentationMode>(atoi(option.second.data()));
    }
  }
}

void DxilOutputColorBecomesConstant::visitOutputInstructionCallers(
  Function * OutputFunction,
  const hlsl::DxilSignature &OutputSignature,
  OP * HlslOP,
  std::function<void(CallInst*)> Visitor) {

  auto OutputFunctionUses = OutputFunction->uses();

  for (Use &FunctionUse : OutputFunctionUses) {
    iterator_range<Value::user_iterator> FunctionUsers = FunctionUse->users();
    for (User * FunctionUser : FunctionUsers) {
      if (isa<Instruction>(FunctionUser)) {
        auto CallInstruction = cast<CallInst>(FunctionUser);

        // Check if the instruction writes to a render target (as opposed to a system-value, such as RenderTargetArrayIndex)
        Value *OutputID = CallInstruction->getArgOperand(DXIL::OperandIndex::kStoreOutputIDOpIdx);
        unsigned SignatureElementIndex = cast<ConstantInt>(OutputID)->getLimitedValue();
        const DxilSignatureElement &SignatureElement = OutputSignature.GetElement(SignatureElementIndex);

        // We only modify the output color for RTV0
        if (SignatureElement.GetSemantic()->GetKind() == DXIL::SemanticKind::Target &&
            SignatureElement.GetSemanticStartIndex() == 0) {

          // Replace the source operand with the appropriate constant value
          Visitor(CallInstruction);
        }
      }
    }
  }
}

bool DxilOutputColorBecomesConstant::runOnModule(Module &M)
{
  // This pass finds all users of the "StoreOutput" function, and replaces their source operands with a constant
  // value. 

  DxilModule &DM = M.GetOrCreateDxilModule();

  LLVMContext & Ctx = M.getContext();

  OP *HlslOP = DM.GetOP();

  const hlsl::DxilSignature & OutputSignature = DM.GetOutputSignature();

  Function * FloatOutputFunction = HlslOP->GetOpFunc(DXIL::OpCode::StoreOutput, Type::getFloatTy(Ctx));
  Function * IntOutputFunction = HlslOP->GetOpFunc(DXIL::OpCode::StoreOutput, Type::getInt32Ty(Ctx));

  bool hasFloatOutputs = false;
  bool hasIntOutputs = false;

  visitOutputInstructionCallers(FloatOutputFunction, OutputSignature, HlslOP, [&hasFloatOutputs](CallInst *) {
    hasFloatOutputs = true;
  });

  visitOutputInstructionCallers(IntOutputFunction, OutputSignature, HlslOP, [&hasIntOutputs](CallInst *) {
    hasIntOutputs = true;
  });

    if (!hasFloatOutputs && !hasIntOutputs)
  {
    return false;
  }

  // Otherwise, we assume the shader outputs only one or the other (because the 0th RTV can't have a mixed type)
  assert(hasFloatOutputs || hasIntOutputs);

  std::array<llvm::Value *, 4> ReplacementColors;

  switch (Mode)
  {
    case FromLiteralConstant: {
      if (hasFloatOutputs) {
        ReplacementColors[0] = HlslOP->GetFloatConst(Red);
        ReplacementColors[1] = HlslOP->GetFloatConst(Green);
        ReplacementColors[2] = HlslOP->GetFloatConst(Blue);
        ReplacementColors[3] = HlslOP->GetFloatConst(Alpha);
      }
      if (hasIntOutputs) {
        ReplacementColors[0] = HlslOP->GetI32Const(static_cast<int>(Red));
        ReplacementColors[1] = HlslOP->GetI32Const(static_cast<int>(Green));
        ReplacementColors[2] = HlslOP->GetI32Const(static_cast<int>(Blue));
        ReplacementColors[3] = HlslOP->GetI32Const(static_cast<int>(Alpha));
      }
    }
    break;
    case FromConstantBuffer: {
      Instruction * entryPointInstruction = &*(DM.GetEntryFunction()->begin()->begin());
      IRBuilder<> Builder(entryPointInstruction);

      // Create handle for the newly-added constant buffer (which is achieved via a function call)
      auto ConstantBufferName = "PIX_Constant_Color_CB_Handle";
      Function *createHandle = HlslOP->GetOpFunc(DXIL::OpCode::CreateHandle, Type::getVoidTy(Ctx));
      Constant *CreateHandleOpcodeArg = HlslOP->GetU32Const((unsigned)DXIL::OpCode::CreateHandle);
      Constant *CBVArg = HlslOP->GetI8Const(2); // "2" implies CB (todo: must be a named value for this somewhere)
      Constant *MetaDataArg = HlslOP->GetU32Const(0); // position of the metadata record in the corresponding metadata list
      Constant *IndexArg = HlslOP->GetU32Const(0); // 
      Constant *FalseArg = HlslOP->GetI1Const(0); // non-uniform resource index: false
      CallInst *callCreateHandle = Builder.CreateCall(createHandle, { CreateHandleOpcodeArg, CBVArg, MetaDataArg, IndexArg, FalseArg }, ConstantBufferName);

      // Insert the Buffer load instruction:
      auto ConstantValueName = "PIX_Constant_Color_Value";
      Function *CBLoad = HlslOP->GetOpFunc(OP::OpCode::CBufferLoadLegacy, hasFloatOutputs ? Type::getFloatTy(Ctx) : Type::getInt32Ty(Ctx));
      Constant *OpArg = HlslOP->GetU32Const((unsigned)OP::OpCode::CBufferLoadLegacy);
      Value * ResourceHandle = callCreateHandle;
      Constant *RowIndex = HlslOP->GetU32Const(0);
      CallInst *loadLegacy = Builder.CreateCall(CBLoad, { OpArg, ResourceHandle, RowIndex }, ConstantValueName);

      // Now extract four color values:
  #define PIX_CONSTANT_VALUE "PIX_Constant_Color_Value"
      ReplacementColors[0] = Builder.CreateExtractValue(loadLegacy, 0, PIX_CONSTANT_VALUE "0");
      ReplacementColors[1] = Builder.CreateExtractValue(loadLegacy, 1, PIX_CONSTANT_VALUE "1");
      ReplacementColors[2] = Builder.CreateExtractValue(loadLegacy, 2, PIX_CONSTANT_VALUE "2");
      ReplacementColors[3] = Builder.CreateExtractValue(loadLegacy, 3, PIX_CONSTANT_VALUE "3");
    }
    break;
    default:
      assert(false);
      return 0;
  }

  bool Modified = false;

  // The StoreOutput function can store either a float or an integer, depending on the intended output
  // render-target resource view.
  if (!FloatOutputFunction->user_empty()) {
    visitOutputInstructionCallers(FloatOutputFunction, OutputSignature, HlslOP, 
      [&ReplacementColors, &Modified](CallInst * CallInstruction) {
      Modified = true;
      // The output column is the channel (red, green, blue or alpha) within the output pixel
      Value * OutputColumnOperand = CallInstruction->getOperand(hlsl::DXIL::OperandIndex::kStoreOutputColOpIdx);
      ConstantInt * OutputColumnConstant = cast<ConstantInt>(OutputColumnOperand);
      APInt OutputColumn = OutputColumnConstant->getValue();
      CallInstruction->setOperand(hlsl::DXIL::OperandIndex::kStoreOutputValOpIdx, ReplacementColors[*OutputColumn.getRawData()]);
    });
  }

  if (!IntOutputFunction->user_empty()) {
    visitOutputInstructionCallers(IntOutputFunction, OutputSignature, HlslOP, 
    [&ReplacementColors, &Modified](CallInst * CallInstruction) {
      Modified = true;
      // The output column is the channel (red, green, blue or alpha) within the output pixel
      Value * OutputColumnOperand = CallInstruction->getOperand(hlsl::DXIL::OperandIndex::kStoreOutputColOpIdx);
      ConstantInt * OutputColumnConstant = cast<ConstantInt>(OutputColumnOperand);
      APInt OutputColumn = OutputColumnConstant->getValue();
      CallInstruction->setOperand(hlsl::DXIL::OperandIndex::kStoreOutputValOpIdx, ReplacementColors[*OutputColumn.getRawData()]);
    });
  }

  return Modified;
}

char DxilOutputColorBecomesConstant::ID = 0;

ModulePass *llvm::createDxilOutputColorBecomesConstantPass() {
  return new DxilOutputColorBecomesConstant();
}

INITIALIZE_PASS(DxilOutputColorBecomesConstant, "hlsl-dxil-constantColor", "DXIL Constant Color Mod", false, false)
