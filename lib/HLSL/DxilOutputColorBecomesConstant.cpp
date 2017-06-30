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
#include "llvm/Support/FormattedStream.h"

using namespace llvm;
using namespace hlsl;

class DxilOutputColorBecomesConstant : public ModulePass {

  enum VisualizerInstrumentationMode
  {
    PRESERVE_ORIGINAL_INSTRUCTIONS,
    REMOVE_DISCARDS_AND_OPTIONALLY_OTHER_INSTRUCTIONS
  };

  float Red = 1.f;
  float Green = 1.f;
  float Blue = 1.f;
  float Alpha = 1.f;
  VisualizerInstrumentationMode Mode;
  bool ContainsDepthStencilWrites = false;

  bool convertTarget0ToConstantValue(Function * OutputFunction, const hlsl::DxilSignature &OutputSignature, OP * HlslOP, float * color);

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

bool DxilOutputColorBecomesConstant::convertTarget0ToConstantValue(
  Function * OutputFunction, 
  const hlsl::DxilSignature &OutputSignature, 
  OP * HlslOP, 
  float * color) {

  bool Modified = false;
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

        if (SignatureElement.GetSemantic()->GetKind() == DXIL::SemanticKind::Depth ||
          SignatureElement.GetSemantic()->GetKind() == DXIL::SemanticKind::DepthLessEqual ||
          SignatureElement.GetSemantic()->GetKind() == DXIL::SemanticKind::DepthGreaterEqual ||
          SignatureElement.GetSemantic()->GetKind() == DXIL::SemanticKind::StencilRef) {

          ContainsDepthStencilWrites = true;
        }

      // We only modify the output color for RTV0
        if (SignatureElement.GetSemantic()->GetKind() == DXIL::SemanticKind::Target &&
            SignatureElement.GetSemanticStartIndex() == 0) {

          // The output column is the channel (red, green, blue or alpha) within the output pixel
          Value * OutputColumnOperand = CallInstruction->getOperand(hlsl::DXIL::OperandIndex::kStoreOutputColOpIdx);
          ConstantInt * OutputColumnConstant = cast<ConstantInt>(OutputColumnOperand);
          APInt OutputColumn = OutputColumnConstant->getValue();

          Value * OutputValueOperand = CallInstruction->getOperand(hlsl::DXIL::OperandIndex::kStoreOutputValOpIdx);

          // Replace the source operand with the appropriate constant literal value
          if (OutputValueOperand->getType()->isFloatingPointTy()){
            Modified = true;
            Constant * FloatConstant = HlslOP->GetFloatConst(color[*OutputColumn.getRawData()]);
            CallInstruction->setOperand(hlsl::DXIL::OperandIndex::kStoreOutputValOpIdx, FloatConstant);
          }
          else if (OutputValueOperand->getType()->isIntegerTy()){
            Modified = true;
            Constant * pIntegerConstant = HlslOP->GetI32Const(static_cast<int>(color[*OutputColumn.getRawData()]));
            CallInstruction->setOperand(hlsl::DXIL::OperandIndex::kStoreOutputValOpIdx, pIntegerConstant);
          }
        }
      }
    }
  }
  return Modified;
}

bool DxilOutputColorBecomesConstant::runOnModule(Module &M)
{
  // This pass finds all users of the "StoreOutput" function, and replaces their source operands with a constant
  // value. 

  float color[4] = { Red, Green, Blue, Alpha };

  DxilModule &DM = M.GetOrCreateDxilModule();

  LLVMContext & Ctx = M.getContext();

  OP *HlslOP = DM.GetOP();

  const hlsl::DxilSignature & OutputSignature = DM.GetOutputSignature();

  bool Modified = false;

  // The StoreOutput function can store either a float or an integer, depending on the intended output
  // render-target resource view.
  Function * FloatOutputFunction = HlslOP->GetOpFunc(DXIL::OpCode::StoreOutput, Type::getFloatTy(Ctx));
  if (FloatOutputFunction->getNumUses() != 0) {
    Modified = convertTarget0ToConstantValue(FloatOutputFunction, OutputSignature, HlslOP, color);
  }

  Function * IntOutputFunction = HlslOP->GetOpFunc(DXIL::OpCode::StoreOutput, Type::getInt32Ty(Ctx));
  if (IntOutputFunction->getNumUses() != 0) {
    Modified = convertTarget0ToConstantValue(IntOutputFunction, OutputSignature, HlslOP, color);
  }

  if (OSOverride != nullptr) {
    if (ContainsDepthStencilWrites) {
      formatted_raw_ostream FOS(*OSOverride);
      FOS << "DxilOutputColorBecomesConstant:DepthOrStencilWritesPresent\n";
      M.print(FOS, nullptr);
    }
  }

  return Modified;
}

char DxilOutputColorBecomesConstant::ID = 0;

ModulePass *llvm::createDxilOutputColorBecomesConstantPass() {
  return new DxilOutputColorBecomesConstant();
}

INITIALIZE_PASS(DxilOutputColorBecomesConstant, "hlsl-dxil-constantColor", "DXIL Constant Color Mod", false, false)
