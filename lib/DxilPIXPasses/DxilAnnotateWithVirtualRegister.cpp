///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilAnnotateWithVirtualRegister.cpp                                       //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Annotates the llvm instructions with a virtual register number to be used //
// during PIX debugging.                                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include <memory>

#include "dxc/DXIL/DxilModule.h"
#include "dxc/DxilPIXPasses/DxilPIXPasses.h"
#include "dxc/DxilPIXPasses/DxilPIXVirtualRegisters.h"
#include "dxc/Support/Global.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/ModuleSlotTracker.h"
#include "llvm/IR/Type.h"
#include "llvm/Pass.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "dxil-annotate-with-virtual-regs"

namespace {
using namespace pix_dxil;

class DxilAnnotateWithVirtualRegister : public llvm::ModulePass {
public:
  static char ID;
  DxilAnnotateWithVirtualRegister() : llvm::ModulePass(ID) {}

  bool runOnModule(llvm::Module &M) override;

private:
  void AnnotateValues(llvm::Instruction *pI);
  void AnnotateStore(llvm::Instruction *pI);
  bool IsAllocaRegisterWrite(llvm::Value *V, llvm::AllocaInst **pAI,
                             llvm::Value **pIdx);
  void AnnotateAlloca(llvm::AllocaInst *pAlloca);
  void AnnotateGeneric(llvm::Instruction *pI);
  void AssignNewDxilRegister(llvm::Instruction *pI);
  void AssignNewAllocaRegister(llvm::AllocaInst *pAlloca, std::uint32_t C);

  hlsl::DxilModule *m_DM;
  std::uint32_t m_uVReg;
  std::unique_ptr<llvm::ModuleSlotTracker> m_MST;
  void Init(llvm::Module &M) {
    m_DM = &M.GetOrCreateDxilModule();
    m_uVReg = 0;
    m_MST.reset(new llvm::ModuleSlotTracker(&M));
    m_MST->incorporateFunction(*m_DM->GetEntryFunction());
  }
};

char DxilAnnotateWithVirtualRegister::ID = 0;

bool DxilAnnotateWithVirtualRegister::runOnModule(llvm::Module &M) {
  Init(M);
  if (m_DM == nullptr) {
    return false;
  }

  std::uint32_t InstNum = 0;
  for (llvm::Instruction &I : llvm::inst_range(m_DM->GetEntryFunction())) {
    pix_dxil::PixDxilInstNum::AddMD(M.getContext(), &I, InstNum++);
  }

  if (OSOverride != nullptr) {
    *OSOverride << "\nInstructionCount:" << InstNum << "\n";
  }

  if (OSOverride != nullptr) {
    *OSOverride << "\nEnd - instruction ID to line\n";
  }

  if (OSOverride != nullptr) {
    *OSOverride << "\nBegin - dxil values to virtual register mapping\n";
  }

  for (llvm::Instruction &I : llvm::inst_range(m_DM->GetEntryFunction())) {
    AnnotateValues(&I);
  }

  for (llvm::Instruction &I : llvm::inst_range(m_DM->GetEntryFunction())) {
    AnnotateStore(&I);
  }

  if (OSOverride != nullptr) {
    *OSOverride << "\nEnd - dxil values to virtual register mapping\n";
  }

  m_DM = nullptr;
  return m_uVReg > 0;
}

void DxilAnnotateWithVirtualRegister::AnnotateValues(llvm::Instruction *pI) {
  if (auto *pAlloca = llvm::dyn_cast<llvm::AllocaInst>(pI)) {
    AnnotateAlloca(pAlloca);
  } else if (!pI->getType()->isVoidTy()) {
    AnnotateGeneric(pI);
  }
}

void DxilAnnotateWithVirtualRegister::AnnotateStore(llvm::Instruction *pI) {
  auto *pSt = llvm::dyn_cast<llvm::StoreInst>(pI);
  if (pSt == nullptr) {
    return;
  }

  llvm::AllocaInst *Alloca;
  llvm::Value *Index;
  if (!IsAllocaRegisterWrite(pSt->getPointerOperand(), &Alloca, &Index)) {
    return;
  }

  llvm::MDNode *AllocaReg = Alloca->getMetadata(PixAllocaReg::MDName);
  if (AllocaReg == nullptr) {
    return;
  }

  PixAllocaRegWrite::AddMD(m_DM->GetCtx(), pSt, AllocaReg, Index);
}

bool DxilAnnotateWithVirtualRegister::IsAllocaRegisterWrite(
    llvm::Value *V, llvm::AllocaInst **pAI, llvm::Value **pIdx) {
  llvm::IRBuilder<> B(m_DM->GetCtx());

  *pAI = nullptr;
  *pIdx = nullptr;

  if (auto *pGEP = llvm::dyn_cast<llvm::GetElementPtrInst>(V)) {
    auto *Alloca = llvm::dyn_cast<llvm::AllocaInst>(pGEP->getPointerOperand());
    if (Alloca == nullptr) {
      return false;
    }

    llvm::SmallVector<llvm::Value *, 2> Indices(pGEP->idx_begin(),
                                                pGEP->idx_end());
    if (Indices.size() != 2) {
      return false;
    }
    auto *pIdx0 = llvm::dyn_cast<llvm::ConstantInt>(Indices[0]);

    if (pIdx0 == nullptr || pIdx0->getLimitedValue() != 0) {
      return false;
    }

    *pAI = Alloca;
    *pIdx = Indices[1];
    return true;
  }

  if (auto *pAlloca = llvm::dyn_cast<llvm::AllocaInst>(V)) {
    llvm::Type *pAllocaTy = pAlloca->getType()->getElementType();
    if (!pAllocaTy->isFloatTy() && !pAllocaTy->isIntegerTy()) {
      return false;
    }

    *pAI = pAlloca;
    *pIdx = B.getInt32(0);
    return true;
  }

  return false;
}

void DxilAnnotateWithVirtualRegister::AnnotateAlloca(
    llvm::AllocaInst *pAlloca) {
  llvm::Type *pAllocaTy = pAlloca->getType()->getElementType();
  if (pAllocaTy->isFloatTy() || pAllocaTy->isIntegerTy() ||
      pAllocaTy->isHalfTy() || pAllocaTy->isIntegerTy(16)) {
    AssignNewAllocaRegister(pAlloca, 1);
  } else if (auto *AT = llvm::dyn_cast<llvm::ArrayType>(pAllocaTy)) {
    AssignNewAllocaRegister(pAlloca, AT->getNumElements());
  } else {
    DXASSERT_ARGS(false, "Unhandled alloca kind: %d", pAllocaTy->getTypeID());
  }
}

void DxilAnnotateWithVirtualRegister::AnnotateGeneric(llvm::Instruction *pI) {
  if (!pI->getType()->isFloatTy() && !pI->getType()->isIntegerTy()) {
    return;
  }
  AssignNewDxilRegister(pI);
}

void DxilAnnotateWithVirtualRegister::AssignNewDxilRegister(
    llvm::Instruction *pI) {
  PixDxilReg::AddMD(m_DM->GetCtx(), pI, m_uVReg);
  if (OSOverride != nullptr) {
    static constexpr bool DontPrintType = false;
    pI->printAsOperand(*OSOverride, DontPrintType, *m_MST.get());
    *OSOverride << " dxil " << m_uVReg << "\n";
  }
  m_uVReg++;
}

void DxilAnnotateWithVirtualRegister::AssignNewAllocaRegister(
    llvm::AllocaInst *pAlloca, std::uint32_t C) {
  PixAllocaReg::AddMD(m_DM->GetCtx(), pAlloca, m_uVReg, C);
  if (OSOverride != nullptr) {
    static constexpr bool DontPrintType = false;
    pAlloca->printAsOperand(*OSOverride, DontPrintType, *m_MST.get());
    *OSOverride << " alloca " << m_uVReg << " " << C << "\n";
  }
  m_uVReg += C;
}
} // namespace

using namespace llvm;

INITIALIZE_PASS(DxilAnnotateWithVirtualRegister, DEBUG_TYPE,
                "Annotates each instruction in the DXIL module with a virtual "
                "register number",
                false, false)

ModulePass *llvm::createDxilAnnotateWithVirtualRegisterPass() {
  return new DxilAnnotateWithVirtualRegister();
}
