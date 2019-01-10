///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilPIXVirtualRegisters.cpp                                               //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Defines functions for dealing with the virtual register annotations in    //
// DXIL instructions.                                                        //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "DxilPIXVirtualRegisters.h"

#include "dxc/Support/Global.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Type.h"

static llvm::Metadata *MetadataForValue(llvm::Value *V) {
  if (auto *C = llvm::dyn_cast<llvm::Constant>(V)) {
    return llvm::ConstantAsMetadata::get(C);
  }
  return llvm::ValueAsMetadata::get(V);
}

void pix_dxil::PixDxilReg::AddMD(llvm::LLVMContext &Ctx, llvm::Instruction *pI, std::uint32_t RegNum) {
  llvm::IRBuilder<> B(Ctx);
  pI->setMetadata(
      llvm::StringRef(MDName),
      llvm::MDNode::get(Ctx, { llvm::ConstantAsMetadata::get(B.getInt32(ID)),
                               llvm::ConstantAsMetadata::get(B.getInt32(RegNum)) }));
}

bool pix_dxil::PixDxilReg::FromInst(llvm::Instruction *pI, std::uint32_t *pRegNum) {
  *pRegNum = 0;

  auto *mdNodes = pI->getMetadata(MDName);

  if (mdNodes == nullptr) {
    return false;
  }

  if (mdNodes->getNumOperands() != 2) {
    return false;
  }

  auto *mdID = llvm::mdconst::dyn_extract<llvm::ConstantInt>(mdNodes->getOperand(0));
  if (mdID == nullptr || mdID->getLimitedValue() != ID) {
    return false;
  }

  auto *mdRegNum = llvm::mdconst::dyn_extract<llvm::ConstantInt>(mdNodes->getOperand(1));
  if (mdRegNum == nullptr) {
    return false;
  }

  *pRegNum = mdRegNum->getLimitedValue();
  return true;
}

static bool ParsePixAllocaReg(llvm::MDNode *MD, std::uint32_t *RegNum, std::uint32_t *Count) {
  if (MD->getNumOperands() != 3) {
    return false;
  }

  auto *mdID = llvm::mdconst::dyn_extract<llvm::ConstantInt>(MD->getOperand(0));
  if (mdID == nullptr || mdID->getLimitedValue() != pix_dxil::PixAllocaReg::ID) {
    return false;
  }

  auto *mdRegNum = llvm::mdconst::dyn_extract<llvm::ConstantInt>(MD->getOperand(1));
  auto *mdCount = llvm::mdconst::dyn_extract<llvm::ConstantInt>(MD->getOperand(2));

  if (mdRegNum == nullptr || mdCount == nullptr) {
    return false;
  }

  *RegNum = mdRegNum->getLimitedValue();
  *Count = mdCount->getLimitedValue();
  return true;
}

void pix_dxil::PixAllocaReg::AddMD(llvm::LLVMContext &Ctx, llvm::AllocaInst *pAlloca, std::uint32_t RegNum, std::uint32_t Count) {
  llvm::IRBuilder<> B(Ctx);
  pAlloca->setMetadata(
      llvm::StringRef(MDName),
      llvm::MDNode::get(Ctx, { llvm::ConstantAsMetadata::get(B.getInt32(ID)),
                               llvm::ConstantAsMetadata::get(B.getInt32(RegNum)),
                               llvm::ConstantAsMetadata::get(B.getInt32(Count)) }));
}

void pix_dxil::PixAllocaRegWrite::AddMD(llvm::LLVMContext &Ctx, llvm::StoreInst *pSt, llvm::MDNode *pAllocaReg, llvm::Value *Index) {
  llvm::IRBuilder<> B(Ctx);
  pSt->setMetadata(
      llvm::StringRef(MDName),
      llvm::MDNode::get(Ctx, { llvm::ConstantAsMetadata::get(B.getInt32(ID)),
                               pAllocaReg,
                               MetadataForValue(Index) }));
}

bool pix_dxil::PixAllocaRegWrite::FromInst(llvm::StoreInst *pI, std::uint32_t *pRegBase, std::uint32_t *pRegSize, llvm::Value **pIndex) {
  *pRegBase = 0;
  *pRegSize = 0;
  *pIndex = nullptr;

  auto *mdNodes = pI->getMetadata(MDName);
  if (mdNodes == nullptr || mdNodes->getNumOperands() != 3) {
    return false;
  }

  auto *mdID = llvm::mdconst::dyn_extract<llvm::ConstantInt>(mdNodes->getOperand(0));
  if (mdID == nullptr || mdID->getLimitedValue() != ID) {
    return false;
  }

  auto *mdAllocaReg = llvm::dyn_cast<llvm::MDNode>(mdNodes->getOperand(1));
  if (mdAllocaReg == nullptr || !ParsePixAllocaReg(mdAllocaReg, pRegBase, pRegSize)) {
    return false;
  }

  auto *mdIndex = llvm::dyn_cast<llvm::ValueAsMetadata>(mdNodes->getOperand(2));
  if (mdIndex == nullptr) {
    return false;
  }
  *pIndex = mdIndex->getValue();

  return true;
}
