///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilPIXVirtualRegisters.cpp                                               //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Declares functions for dealing with the virtual register annotations in   //
// DXIL instructions.                                                        //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include <cstdint>

namespace llvm {
class AllocaInst;
class Instruction;
class LLVMContext;
class MDNode;
class StoreInst;
class Value;
}  // namespace llvm

namespace pix_dxil {
namespace PixDxilReg {
static constexpr char MDName[] = "pix-dxil-reg";
static constexpr uint32_t ID = 0;

void AddMD(llvm::LLVMContext &Ctx, llvm::Instruction *pI, std::uint32_t RegNum);
bool FromInst(llvm::Instruction *pI, std::uint32_t *pRegNum);
}  // namespace PixDxilReg

namespace PixAllocaReg {
static constexpr char MDName[] = "pix-alloca-reg";
static constexpr uint32_t ID = 1;

void AddMD(llvm::LLVMContext &Ctx, llvm::AllocaInst *pAlloca, std::uint32_t RegNum, std::uint32_t Count);
}  // namespace PixAllocaReg

namespace PixAllocaRegWrite {
static constexpr char MDName[] = "pix-alloca-reg-write";
static constexpr uint32_t ID = 2;
void AddMD(llvm::LLVMContext &Ctx, llvm::StoreInst *pSt, llvm::MDNode *pAllocaReg, llvm::Value *Index);
bool FromInst(llvm::StoreInst *pI, std::uint32_t *pRegBase, std::uint32_t *pRegSize, llvm::Value **pIndex);
}  // namespace PixAllocaRegWrite
}  // namespace pix_dxil