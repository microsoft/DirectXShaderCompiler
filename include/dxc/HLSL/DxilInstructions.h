///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilInstructions.h                                                        //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file provides a library of instruction helper classes.               //
// MUCH WORK YET TO BE DONE - EXPECT THIS WILL CHANGE - GENERATED FILE       //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "llvm/IR/Constants.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"

// TODO: add correct include directives
// TODO: add accessors with values
// TODO: add validation support code, including calling into right fn
// TODO: add type hierarchy
namespace hlsl {
/* <py>
import hctdb_instrhelp
</py> */
/* <py::lines('INSTR-HELPER')>hctdb_instrhelp.get_instrhelper()</py>*/
// INSTR-HELPER:BEGIN
/// This instruction returns a value (possibly void), from a function.
struct LlvmInst_Ret {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_Ret(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::Ret;
  }
  // Validation support
  bool isAllowed() const { return true; }
};

/// This instruction branches (conditional or unconditional)
struct LlvmInst_Br {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_Br(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::Br;
  }
  // Validation support
  bool isAllowed() const { return true; }
};

/// This instruction performs a multiway switch
struct LlvmInst_Switch {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_Switch(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::Switch;
  }
  // Validation support
  bool isAllowed() const { return true; }
};

/// This instruction branches indirectly
struct LlvmInst_IndirectBr {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_IndirectBr(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::IndirectBr;
  }
  // Validation support
  bool isAllowed() const { return false; }
};

/// This instruction invokes function with normal and exceptional returns
struct LlvmInst_Invoke {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_Invoke(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::Invoke;
  }
  // Validation support
  bool isAllowed() const { return false; }
};

/// This instruction resumes the propagation of an exception
struct LlvmInst_Resume {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_Resume(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::Resume;
  }
  // Validation support
  bool isAllowed() const { return false; }
};

/// This instruction is unreachable
struct LlvmInst_Unreachable {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_Unreachable(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::Unreachable;
  }
  // Validation support
  bool isAllowed() const { return false; }
};

/// This instruction returns the sum of its two operands
struct LlvmInst_Add {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_Add(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::Add;
  }
  // Validation support
  bool isAllowed() const { return true; }
};

/// This instruction returns the sum of its two operands
struct LlvmInst_FAdd {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_FAdd(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::FAdd;
  }
  // Validation support
  bool isAllowed() const { return true; }
};

/// This instruction returns the difference of its two operands
struct LlvmInst_Sub {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_Sub(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::Sub;
  }
  // Validation support
  bool isAllowed() const { return true; }
};

/// This instruction returns the difference of its two operands
struct LlvmInst_FSub {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_FSub(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::FSub;
  }
  // Validation support
  bool isAllowed() const { return true; }
};

/// This instruction returns the product of its two operands
struct LlvmInst_Mul {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_Mul(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::Mul;
  }
  // Validation support
  bool isAllowed() const { return true; }
};

/// This instruction returns the product of its two operands
struct LlvmInst_FMul {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_FMul(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::FMul;
  }
  // Validation support
  bool isAllowed() const { return true; }
};

/// This instruction returns the quotient of its two unsigned operands
struct LlvmInst_UDiv {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_UDiv(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::UDiv;
  }
  // Validation support
  bool isAllowed() const { return true; }
};

/// This instruction returns the quotient of its two signed operands
struct LlvmInst_SDiv {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_SDiv(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::SDiv;
  }
  // Validation support
  bool isAllowed() const { return true; }
};

/// This instruction returns the quotient of its two operands
struct LlvmInst_FDiv {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_FDiv(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::FDiv;
  }
  // Validation support
  bool isAllowed() const { return true; }
};

/// This instruction returns the remainder from the unsigned division of its two operands
struct LlvmInst_URem {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_URem(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::URem;
  }
  // Validation support
  bool isAllowed() const { return true; }
};

/// This instruction returns the remainder from the signed division of its two operands
struct LlvmInst_SRem {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_SRem(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::SRem;
  }
  // Validation support
  bool isAllowed() const { return true; }
};

/// This instruction returns the remainder from the division of its two operands
struct LlvmInst_FRem {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_FRem(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::FRem;
  }
  // Validation support
  bool isAllowed() const { return true; }
};

/// This instruction shifts left (logical)
struct LlvmInst_Shl {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_Shl(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::Shl;
  }
  // Validation support
  bool isAllowed() const { return true; }
};

/// This instruction shifts right (logical), with zero bit fill
struct LlvmInst_LShr {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_LShr(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::LShr;
  }
  // Validation support
  bool isAllowed() const { return true; }
};

/// This instruction shifts right (arithmetic), with 'a' operand sign bit fill
struct LlvmInst_AShr {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_AShr(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::AShr;
  }
  // Validation support
  bool isAllowed() const { return true; }
};

/// This instruction returns a  bitwise logical and of its two operands
struct LlvmInst_And {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_And(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::And;
  }
  // Validation support
  bool isAllowed() const { return true; }
};

/// This instruction returns a bitwise logical or of its two operands
struct LlvmInst_Or {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_Or(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::Or;
  }
  // Validation support
  bool isAllowed() const { return true; }
};

/// This instruction returns a bitwise logical xor of its two operands
struct LlvmInst_Xor {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_Xor(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::Xor;
  }
  // Validation support
  bool isAllowed() const { return true; }
};

/// This instruction allocates memory on the stack frame of the currently executing function
struct LlvmInst_Alloca {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_Alloca(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::Alloca;
  }
  // Validation support
  bool isAllowed() const { return true; }
};

/// This instruction reads from memory
struct LlvmInst_Load {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_Load(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::Load;
  }
  // Validation support
  bool isAllowed() const { return true; }
};

/// This instruction writes to memory
struct LlvmInst_Store {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_Store(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::Store;
  }
  // Validation support
  bool isAllowed() const { return true; }
};

/// This instruction gets the address of a subelement of an aggregate value
struct LlvmInst_GetElementPtr {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_GetElementPtr(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::GetElementPtr;
  }
  // Validation support
  bool isAllowed() const { return true; }
};

/// This instruction introduces happens-before edges between operations
struct LlvmInst_Fence {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_Fence(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::Fence;
  }
  // Validation support
  bool isAllowed() const { return false; }
};

/// This instruction atomically modifies memory
struct LlvmInst_AtomicCmpXchg {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_AtomicCmpXchg(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::AtomicCmpXchg;
  }
  // Validation support
  bool isAllowed() const { return true; }
};

/// This instruction atomically modifies memory
struct LlvmInst_AtomicRMW {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_AtomicRMW(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::AtomicRMW;
  }
  // Validation support
  bool isAllowed() const { return true; }
};

/// This instruction truncates an integer
struct LlvmInst_Trunc {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_Trunc(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::Trunc;
  }
  // Validation support
  bool isAllowed() const { return true; }
};

/// This instruction zero extends an integer
struct LlvmInst_ZExt {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_ZExt(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::ZExt;
  }
  // Validation support
  bool isAllowed() const { return true; }
};

/// This instruction sign extends an integer
struct LlvmInst_SExt {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_SExt(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::SExt;
  }
  // Validation support
  bool isAllowed() const { return true; }
};

/// This instruction converts a floating point to UInt
struct LlvmInst_FPToUI {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_FPToUI(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::FPToUI;
  }
  // Validation support
  bool isAllowed() const { return true; }
};

/// This instruction converts a floating point to SInt
struct LlvmInst_FPToSI {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_FPToSI(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::FPToSI;
  }
  // Validation support
  bool isAllowed() const { return true; }
};

/// This instruction converts a UInt to floating point
struct LlvmInst_UIToFP {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_UIToFP(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::UIToFP;
  }
  // Validation support
  bool isAllowed() const { return true; }
};

/// This instruction converts a SInt to floating point
struct LlvmInst_SIToFP {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_SIToFP(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::SIToFP;
  }
  // Validation support
  bool isAllowed() const { return true; }
};

/// This instruction truncates a floating point
struct LlvmInst_FPTrunc {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_FPTrunc(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::FPTrunc;
  }
  // Validation support
  bool isAllowed() const { return true; }
};

/// This instruction extends a floating point
struct LlvmInst_FPExt {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_FPExt(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::FPExt;
  }
  // Validation support
  bool isAllowed() const { return true; }
};

/// This instruction converts a pointer to integer
struct LlvmInst_PtrToInt {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_PtrToInt(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::PtrToInt;
  }
  // Validation support
  bool isAllowed() const { return false; }
};

/// This instruction converts an integer to Pointer
struct LlvmInst_IntToPtr {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_IntToPtr(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::IntToPtr;
  }
  // Validation support
  bool isAllowed() const { return false; }
};

/// This instruction performs a bit-preserving type cast
struct LlvmInst_BitCast {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_BitCast(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::BitCast;
  }
  // Validation support
  bool isAllowed() const { return true; }
};

/// This instruction casts a value addrspace
struct LlvmInst_AddrSpaceCast {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_AddrSpaceCast(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::AddrSpaceCast;
  }
  // Validation support
  bool isAllowed() const { return true; }
};

/// This instruction compares integers
struct LlvmInst_ICmp {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_ICmp(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::ICmp;
  }
  // Validation support
  bool isAllowed() const { return true; }
};

/// This instruction compares floating points
struct LlvmInst_FCmp {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_FCmp(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::FCmp;
  }
  // Validation support
  bool isAllowed() const { return true; }
};

/// This instruction is a PHI node instruction
struct LlvmInst_PHI {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_PHI(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::PHI;
  }
  // Validation support
  bool isAllowed() const { return true; }
};

/// This instruction calls a function
struct LlvmInst_Call {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_Call(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::Call;
  }
  // Validation support
  bool isAllowed() const { return true; }
};

/// This instruction selects an instruction
struct LlvmInst_Select {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_Select(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::Select;
  }
  // Validation support
  bool isAllowed() const { return true; }
};

/// This instruction may be used internally in a pass
struct LlvmInst_UserOp1 {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_UserOp1(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::UserOp1;
  }
  // Validation support
  bool isAllowed() const { return false; }
};

/// This instruction internal to passes only
struct LlvmInst_UserOp2 {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_UserOp2(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::UserOp2;
  }
  // Validation support
  bool isAllowed() const { return false; }
};

/// This instruction vaarg instruction
struct LlvmInst_VAArg {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_VAArg(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::VAArg;
  }
  // Validation support
  bool isAllowed() const { return false; }
};

/// This instruction extracts from aggregate
struct LlvmInst_ExtractValue {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_ExtractValue(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::ExtractValue;
  }
  // Validation support
  bool isAllowed() const { return true; }
};

/// This instruction represents a landing pad
struct LlvmInst_LandingPad {
  const llvm::Instruction *Instr;
  // Construction and identification
  LlvmInst_LandingPad(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return Instr->getOpcode() == llvm::Instruction::LandingPad;
  }
  // Validation support
  bool isAllowed() const { return false; }
};

/// This instruction helper load operation
struct DxilInst_TempRegLoad {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_TempRegLoad(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::TempRegLoad);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_index() const { return Instr->getOperand(1); }
};

/// This instruction helper store operation
struct DxilInst_TempRegStore {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_TempRegStore(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::TempRegStore);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (3 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_index() const { return Instr->getOperand(1); }
  llvm::Value *get_value() const { return Instr->getOperand(2); }
};

/// This instruction helper load operation for minprecision
struct DxilInst_MinPrecXRegLoad {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_MinPrecXRegLoad(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::MinPrecXRegLoad);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (4 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_regIndex() const { return Instr->getOperand(1); }
  llvm::Value *get_index() const { return Instr->getOperand(2); }
  llvm::Value *get_component() const { return Instr->getOperand(3); }
};

/// This instruction helper store operation for minprecision
struct DxilInst_MinPrecXRegStore {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_MinPrecXRegStore(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::MinPrecXRegStore);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (5 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_regIndex() const { return Instr->getOperand(1); }
  llvm::Value *get_index() const { return Instr->getOperand(2); }
  llvm::Value *get_component() const { return Instr->getOperand(3); }
  llvm::Value *get_value() const { return Instr->getOperand(4); }
};

/// This instruction loads the value from shader input
struct DxilInst_LoadInput {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_LoadInput(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::LoadInput);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (5 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_inputSigId() const { return Instr->getOperand(1); }
  llvm::Value *get_rowIndex() const { return Instr->getOperand(2); }
  llvm::Value *get_colIndex() const { return Instr->getOperand(3); }
  llvm::Value *get_gsVertexAxis() const { return Instr->getOperand(4); }
};

/// This instruction stores the value to shader output
struct DxilInst_StoreOutput {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_StoreOutput(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::StoreOutput);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (5 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_outputtSigId() const { return Instr->getOperand(1); }
  llvm::Value *get_rowIndex() const { return Instr->getOperand(2); }
  llvm::Value *get_colIndex() const { return Instr->getOperand(3); }
  llvm::Value *get_value() const { return Instr->getOperand(4); }
};

/// This instruction returns the absolute value of the input value.
struct DxilInst_FAbs {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_FAbs(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::FAbs);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
};

/// This instruction clamps the result of a single or double precision floating point value to [0.0f...1.0f]
struct DxilInst_Saturate {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_Saturate(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::Saturate);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
};

/// This instruction returns the IsNaN
struct DxilInst_IsNaN {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_IsNaN(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::IsNaN);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
};

/// This instruction returns the IsInf
struct DxilInst_IsInf {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_IsInf(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::IsInf);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
};

/// This instruction returns the IsFinite
struct DxilInst_IsFinite {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_IsFinite(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::IsFinite);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
};

/// This instruction returns the IsNormal
struct DxilInst_IsNormal {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_IsNormal(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::IsNormal);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
};

/// This instruction returns cosine(theta) for theta in radians.
struct DxilInst_Cos {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_Cos(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::Cos);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
};

/// This instruction returns the Sin
struct DxilInst_Sin {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_Sin(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::Sin);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
};

/// This instruction returns the Tan
struct DxilInst_Tan {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_Tan(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::Tan);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
};

/// This instruction returns the Acos
struct DxilInst_Acos {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_Acos(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::Acos);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
};

/// This instruction returns the Asin
struct DxilInst_Asin {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_Asin(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::Asin);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
};

/// This instruction returns the Atan
struct DxilInst_Atan {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_Atan(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::Atan);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
};

/// This instruction returns the Hcos
struct DxilInst_Hcos {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_Hcos(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::Hcos);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
};

/// This instruction returns the Hsin
struct DxilInst_Hsin {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_Hsin(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::Hsin);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
};

/// This instruction returns the Exp
struct DxilInst_Exp {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_Exp(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::Exp);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
};

/// This instruction returns the Frc
struct DxilInst_Frc {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_Frc(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::Frc);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
};

/// This instruction returns the Log
struct DxilInst_Log {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_Log(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::Log);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
};

/// This instruction returns the Sqrt
struct DxilInst_Sqrt {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_Sqrt(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::Sqrt);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
};

/// This instruction returns the Rsqrt
struct DxilInst_Rsqrt {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_Rsqrt(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::Rsqrt);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
};

/// This instruction returns the Round_ne
struct DxilInst_Round_ne {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_Round_ne(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::Round_ne);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
};

/// This instruction returns the Round_ni
struct DxilInst_Round_ni {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_Round_ni(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::Round_ni);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
};

/// This instruction returns the Round_pi
struct DxilInst_Round_pi {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_Round_pi(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::Round_pi);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
};

/// This instruction returns the Round_z
struct DxilInst_Round_z {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_Round_z(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::Round_z);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
};

/// This instruction returns the reverse bit pattern of the input value
struct DxilInst_Bfrev {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_Bfrev(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::Bfrev);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
};

/// This instruction returns the Countbits
struct DxilInst_Countbits {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_Countbits(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::Countbits);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
};

/// This instruction returns the FirstbitLo
struct DxilInst_FirstbitLo {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_FirstbitLo(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::FirstbitLo);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
};

/// This instruction returns src != 0? (BitWidth-1 - FirstbitHi) : -1
struct DxilInst_FirstbitHi {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_FirstbitHi(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::FirstbitHi);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
};

/// This instruction returns src != 0? (BitWidth-1 - FirstbitSHi) : -1
struct DxilInst_FirstbitSHi {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_FirstbitSHi(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::FirstbitSHi);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
};

/// This instruction returns the FMax of the input values
struct DxilInst_FMax {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_FMax(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::FMax);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (3 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_a() const { return Instr->getOperand(1); }
  llvm::Value *get_b() const { return Instr->getOperand(2); }
};

/// This instruction returns the FMin of the input values
struct DxilInst_FMin {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_FMin(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::FMin);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (3 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_a() const { return Instr->getOperand(1); }
  llvm::Value *get_b() const { return Instr->getOperand(2); }
};

/// This instruction returns the IMax of the input values
struct DxilInst_IMax {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_IMax(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::IMax);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (3 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_a() const { return Instr->getOperand(1); }
  llvm::Value *get_b() const { return Instr->getOperand(2); }
};

/// This instruction returns the IMin of the input values
struct DxilInst_IMin {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_IMin(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::IMin);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (3 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_a() const { return Instr->getOperand(1); }
  llvm::Value *get_b() const { return Instr->getOperand(2); }
};

/// This instruction returns the UMax of the input values
struct DxilInst_UMax {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_UMax(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::UMax);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (3 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_a() const { return Instr->getOperand(1); }
  llvm::Value *get_b() const { return Instr->getOperand(2); }
};

/// This instruction returns the UMin of the input values
struct DxilInst_UMin {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_UMin(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::UMin);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (3 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_a() const { return Instr->getOperand(1); }
  llvm::Value *get_b() const { return Instr->getOperand(2); }
};

/// This instruction returns the IMul of the input values
struct DxilInst_IMul {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_IMul(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::IMul);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (3 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_a() const { return Instr->getOperand(1); }
  llvm::Value *get_b() const { return Instr->getOperand(2); }
};

/// This instruction returns the UMul of the input values
struct DxilInst_UMul {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_UMul(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::UMul);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (3 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_a() const { return Instr->getOperand(1); }
  llvm::Value *get_b() const { return Instr->getOperand(2); }
};

/// This instruction returns the UDiv of the input values
struct DxilInst_UDiv {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_UDiv(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::UDiv);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (3 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_a() const { return Instr->getOperand(1); }
  llvm::Value *get_b() const { return Instr->getOperand(2); }
};

/// This instruction returns the IAddc of the input values
struct DxilInst_IAddc {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_IAddc(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::IAddc);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (3 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_a() const { return Instr->getOperand(1); }
  llvm::Value *get_b() const { return Instr->getOperand(2); }
};

/// This instruction returns the UAddc of the input values
struct DxilInst_UAddc {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_UAddc(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::UAddc);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (3 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_a() const { return Instr->getOperand(1); }
  llvm::Value *get_b() const { return Instr->getOperand(2); }
};

/// This instruction returns the ISubc of the input values
struct DxilInst_ISubc {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_ISubc(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::ISubc);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (3 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_a() const { return Instr->getOperand(1); }
  llvm::Value *get_b() const { return Instr->getOperand(2); }
};

/// This instruction returns the USubc of the input values
struct DxilInst_USubc {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_USubc(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::USubc);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (3 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_a() const { return Instr->getOperand(1); }
  llvm::Value *get_b() const { return Instr->getOperand(2); }
};

/// This instruction performs a fused multiply add (FMA) of the form a * b + c
struct DxilInst_FMad {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_FMad(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::FMad);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (4 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_a() const { return Instr->getOperand(1); }
  llvm::Value *get_b() const { return Instr->getOperand(2); }
  llvm::Value *get_c() const { return Instr->getOperand(3); }
};

/// This instruction performs a fused multiply add (FMA) of the form a * b + c
struct DxilInst_Fma {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_Fma(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::Fma);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (4 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_a() const { return Instr->getOperand(1); }
  llvm::Value *get_b() const { return Instr->getOperand(2); }
  llvm::Value *get_c() const { return Instr->getOperand(3); }
};

/// This instruction performs an integral IMad
struct DxilInst_IMad {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_IMad(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::IMad);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (4 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_a() const { return Instr->getOperand(1); }
  llvm::Value *get_b() const { return Instr->getOperand(2); }
  llvm::Value *get_c() const { return Instr->getOperand(3); }
};

/// This instruction performs an integral UMad
struct DxilInst_UMad {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_UMad(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::UMad);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (4 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_a() const { return Instr->getOperand(1); }
  llvm::Value *get_b() const { return Instr->getOperand(2); }
  llvm::Value *get_c() const { return Instr->getOperand(3); }
};

/// This instruction performs an integral Msad
struct DxilInst_Msad {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_Msad(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::Msad);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (4 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_a() const { return Instr->getOperand(1); }
  llvm::Value *get_b() const { return Instr->getOperand(2); }
  llvm::Value *get_c() const { return Instr->getOperand(3); }
};

/// This instruction performs an integral Ibfe
struct DxilInst_Ibfe {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_Ibfe(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::Ibfe);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (4 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_a() const { return Instr->getOperand(1); }
  llvm::Value *get_b() const { return Instr->getOperand(2); }
  llvm::Value *get_c() const { return Instr->getOperand(3); }
};

/// This instruction performs an integral Ubfe
struct DxilInst_Ubfe {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_Ubfe(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::Ubfe);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (4 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_a() const { return Instr->getOperand(1); }
  llvm::Value *get_b() const { return Instr->getOperand(2); }
  llvm::Value *get_c() const { return Instr->getOperand(3); }
};

/// This instruction given a bit range from the LSB of a number, places that number of bits in another number at any offset
struct DxilInst_Bfi {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_Bfi(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::Bfi);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (5 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_width() const { return Instr->getOperand(1); }
  llvm::Value *get_offset() const { return Instr->getOperand(2); }
  llvm::Value *get_value() const { return Instr->getOperand(3); }
  llvm::Value *get_replaceCount() const { return Instr->getOperand(4); }
};

/// This instruction two-dimensional vector dot-product
struct DxilInst_Dot2 {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_Dot2(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::Dot2);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (5 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_ax() const { return Instr->getOperand(1); }
  llvm::Value *get_ay() const { return Instr->getOperand(2); }
  llvm::Value *get_bx() const { return Instr->getOperand(3); }
  llvm::Value *get_by() const { return Instr->getOperand(4); }
};

/// This instruction three-dimensional vector dot-product
struct DxilInst_Dot3 {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_Dot3(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::Dot3);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (7 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_ax() const { return Instr->getOperand(1); }
  llvm::Value *get_ay() const { return Instr->getOperand(2); }
  llvm::Value *get_az() const { return Instr->getOperand(3); }
  llvm::Value *get_bx() const { return Instr->getOperand(4); }
  llvm::Value *get_by() const { return Instr->getOperand(5); }
  llvm::Value *get_bz() const { return Instr->getOperand(6); }
};

/// This instruction four-dimensional vector dot-product
struct DxilInst_Dot4 {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_Dot4(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::Dot4);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (9 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_ax() const { return Instr->getOperand(1); }
  llvm::Value *get_ay() const { return Instr->getOperand(2); }
  llvm::Value *get_az() const { return Instr->getOperand(3); }
  llvm::Value *get_aw() const { return Instr->getOperand(4); }
  llvm::Value *get_bx() const { return Instr->getOperand(5); }
  llvm::Value *get_by() const { return Instr->getOperand(6); }
  llvm::Value *get_bz() const { return Instr->getOperand(7); }
  llvm::Value *get_bw() const { return Instr->getOperand(8); }
};

/// This instruction creates the handle to a resource
struct DxilInst_CreateHandle {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_CreateHandle(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::CreateHandle);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (5 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_resourceClass() const { return Instr->getOperand(1); }
  int8_t get_resourceClass_val() const { return (int8_t)(llvm::dyn_cast<llvm::ConstantInt>(Instr->getOperand(1))->getZExtValue()); }
  llvm::Value *get_rangeId() const { return Instr->getOperand(2); }
  llvm::Value *get_index() const { return Instr->getOperand(3); }
  llvm::Value *get_nonUniformIndex() const { return Instr->getOperand(4); }
  bool get_nonUniformIndex_val() const { return (bool)(llvm::dyn_cast<llvm::ConstantInt>(Instr->getOperand(4))->getZExtValue()); }
};

/// This instruction loads a value from a constant buffer resource
struct DxilInst_CBufferLoad {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_CBufferLoad(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::CBufferLoad);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (4 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_handle() const { return Instr->getOperand(1); }
  llvm::Value *get_byteOffset() const { return Instr->getOperand(2); }
  llvm::Value *get_alignment() const { return Instr->getOperand(3); }
  uint32_t get_alignment_val() const { return (uint32_t)(llvm::dyn_cast<llvm::ConstantInt>(Instr->getOperand(3))->getZExtValue()); }
};

/// This instruction loads a value from a constant buffer resource
struct DxilInst_CBufferLoadLegacy {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_CBufferLoadLegacy(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::CBufferLoadLegacy);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (3 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_handle() const { return Instr->getOperand(1); }
  llvm::Value *get_regIndex() const { return Instr->getOperand(2); }
};

/// This instruction samples a texture
struct DxilInst_Sample {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_Sample(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::Sample);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (11 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_srv() const { return Instr->getOperand(1); }
  llvm::Value *get_sampler() const { return Instr->getOperand(2); }
  llvm::Value *get_coord0() const { return Instr->getOperand(3); }
  llvm::Value *get_coord1() const { return Instr->getOperand(4); }
  llvm::Value *get_coord2() const { return Instr->getOperand(5); }
  llvm::Value *get_coord3() const { return Instr->getOperand(6); }
  llvm::Value *get_offset0() const { return Instr->getOperand(7); }
  llvm::Value *get_offset1() const { return Instr->getOperand(8); }
  llvm::Value *get_offset2() const { return Instr->getOperand(9); }
  llvm::Value *get_clamp() const { return Instr->getOperand(10); }
};

/// This instruction samples a texture after applying the input bias to the mipmap level
struct DxilInst_SampleBias {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_SampleBias(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::SampleBias);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (12 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_srv() const { return Instr->getOperand(1); }
  llvm::Value *get_sampler() const { return Instr->getOperand(2); }
  llvm::Value *get_coord0() const { return Instr->getOperand(3); }
  llvm::Value *get_coord1() const { return Instr->getOperand(4); }
  llvm::Value *get_coord2() const { return Instr->getOperand(5); }
  llvm::Value *get_coord3() const { return Instr->getOperand(6); }
  llvm::Value *get_offset0() const { return Instr->getOperand(7); }
  llvm::Value *get_offset1() const { return Instr->getOperand(8); }
  llvm::Value *get_offset2() const { return Instr->getOperand(9); }
  llvm::Value *get_bias() const { return Instr->getOperand(10); }
  llvm::Value *get_clamp() const { return Instr->getOperand(11); }
};

/// This instruction samples a texture using a mipmap-level offset
struct DxilInst_SampleLevel {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_SampleLevel(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::SampleLevel);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (11 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_srv() const { return Instr->getOperand(1); }
  llvm::Value *get_sampler() const { return Instr->getOperand(2); }
  llvm::Value *get_coord0() const { return Instr->getOperand(3); }
  llvm::Value *get_coord1() const { return Instr->getOperand(4); }
  llvm::Value *get_coord2() const { return Instr->getOperand(5); }
  llvm::Value *get_coord3() const { return Instr->getOperand(6); }
  llvm::Value *get_offset0() const { return Instr->getOperand(7); }
  llvm::Value *get_offset1() const { return Instr->getOperand(8); }
  llvm::Value *get_offset2() const { return Instr->getOperand(9); }
  llvm::Value *get_LOD() const { return Instr->getOperand(10); }
};

/// This instruction samples a texture using a gradient to influence the way the sample location is calculated
struct DxilInst_SampleGrad {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_SampleGrad(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::SampleGrad);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (17 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_srv() const { return Instr->getOperand(1); }
  llvm::Value *get_sampler() const { return Instr->getOperand(2); }
  llvm::Value *get_coord0() const { return Instr->getOperand(3); }
  llvm::Value *get_coord1() const { return Instr->getOperand(4); }
  llvm::Value *get_coord2() const { return Instr->getOperand(5); }
  llvm::Value *get_coord3() const { return Instr->getOperand(6); }
  llvm::Value *get_offset0() const { return Instr->getOperand(7); }
  llvm::Value *get_offset1() const { return Instr->getOperand(8); }
  llvm::Value *get_offset2() const { return Instr->getOperand(9); }
  llvm::Value *get_ddx0() const { return Instr->getOperand(10); }
  llvm::Value *get_ddx1() const { return Instr->getOperand(11); }
  llvm::Value *get_ddx2() const { return Instr->getOperand(12); }
  llvm::Value *get_ddy0() const { return Instr->getOperand(13); }
  llvm::Value *get_ddy1() const { return Instr->getOperand(14); }
  llvm::Value *get_ddy2() const { return Instr->getOperand(15); }
  llvm::Value *get_clamp() const { return Instr->getOperand(16); }
};

/// This instruction samples a texture and compares a single component against the specified comparison value
struct DxilInst_SampleCmp {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_SampleCmp(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::SampleCmp);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (12 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_srv() const { return Instr->getOperand(1); }
  llvm::Value *get_sampler() const { return Instr->getOperand(2); }
  llvm::Value *get_coord0() const { return Instr->getOperand(3); }
  llvm::Value *get_coord1() const { return Instr->getOperand(4); }
  llvm::Value *get_coord2() const { return Instr->getOperand(5); }
  llvm::Value *get_coord3() const { return Instr->getOperand(6); }
  llvm::Value *get_offset0() const { return Instr->getOperand(7); }
  llvm::Value *get_offset1() const { return Instr->getOperand(8); }
  llvm::Value *get_offset2() const { return Instr->getOperand(9); }
  llvm::Value *get_compareValue() const { return Instr->getOperand(10); }
  llvm::Value *get_clamp() const { return Instr->getOperand(11); }
};

/// This instruction samples a texture and compares a single component against the specified comparison value
struct DxilInst_SampleCmpLevelZero {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_SampleCmpLevelZero(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::SampleCmpLevelZero);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (11 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_srv() const { return Instr->getOperand(1); }
  llvm::Value *get_sampler() const { return Instr->getOperand(2); }
  llvm::Value *get_coord0() const { return Instr->getOperand(3); }
  llvm::Value *get_coord1() const { return Instr->getOperand(4); }
  llvm::Value *get_coord2() const { return Instr->getOperand(5); }
  llvm::Value *get_coord3() const { return Instr->getOperand(6); }
  llvm::Value *get_offset0() const { return Instr->getOperand(7); }
  llvm::Value *get_offset1() const { return Instr->getOperand(8); }
  llvm::Value *get_offset2() const { return Instr->getOperand(9); }
  llvm::Value *get_compareValue() const { return Instr->getOperand(10); }
};

/// This instruction reads texel data without any filtering or sampling
struct DxilInst_TextureLoad {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_TextureLoad(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::TextureLoad);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (9 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_srv() const { return Instr->getOperand(1); }
  llvm::Value *get_mipLevelOrSampleCount() const { return Instr->getOperand(2); }
  llvm::Value *get_coord0() const { return Instr->getOperand(3); }
  llvm::Value *get_coord1() const { return Instr->getOperand(4); }
  llvm::Value *get_coord2() const { return Instr->getOperand(5); }
  llvm::Value *get_offset0() const { return Instr->getOperand(6); }
  llvm::Value *get_offset1() const { return Instr->getOperand(7); }
  llvm::Value *get_offset2() const { return Instr->getOperand(8); }
};

/// This instruction reads texel data without any filtering or sampling
struct DxilInst_TextureStore {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_TextureStore(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::TextureStore);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (10 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_srv() const { return Instr->getOperand(1); }
  llvm::Value *get_coord0() const { return Instr->getOperand(2); }
  llvm::Value *get_coord1() const { return Instr->getOperand(3); }
  llvm::Value *get_coord2() const { return Instr->getOperand(4); }
  llvm::Value *get_value0() const { return Instr->getOperand(5); }
  llvm::Value *get_value1() const { return Instr->getOperand(6); }
  llvm::Value *get_value2() const { return Instr->getOperand(7); }
  llvm::Value *get_value3() const { return Instr->getOperand(8); }
  llvm::Value *get_mask() const { return Instr->getOperand(9); }
};

/// This instruction reads from a TypedBuffer
struct DxilInst_BufferLoad {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_BufferLoad(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::BufferLoad);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (4 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_srv() const { return Instr->getOperand(1); }
  llvm::Value *get_index() const { return Instr->getOperand(2); }
  llvm::Value *get_wot() const { return Instr->getOperand(3); }
};

/// This instruction writes to a RWTypedBuffer
struct DxilInst_BufferStore {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_BufferStore(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::BufferStore);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (9 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_uav() const { return Instr->getOperand(1); }
  llvm::Value *get_coord0() const { return Instr->getOperand(2); }
  llvm::Value *get_coord1() const { return Instr->getOperand(3); }
  llvm::Value *get_value0() const { return Instr->getOperand(4); }
  llvm::Value *get_value1() const { return Instr->getOperand(5); }
  llvm::Value *get_value2() const { return Instr->getOperand(6); }
  llvm::Value *get_value3() const { return Instr->getOperand(7); }
  llvm::Value *get_mask() const { return Instr->getOperand(8); }
};

/// This instruction atomically increments/decrements the hidden 32-bit counter stored with a Count or Append UAV
struct DxilInst_BufferUpdateCounter {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_BufferUpdateCounter(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::BufferUpdateCounter);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (3 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_uav() const { return Instr->getOperand(1); }
  llvm::Value *get_inc() const { return Instr->getOperand(2); }
};

/// This instruction determines whether all values from a Sample, Gather, or Load operation accessed mapped tiles in a tiled resource
struct DxilInst_CheckAccessFullyMapped {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_CheckAccessFullyMapped(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::CheckAccessFullyMapped);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_status() const { return Instr->getOperand(1); }
};

/// This instruction gets texture size information
struct DxilInst_GetDimensions {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_GetDimensions(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::GetDimensions);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (3 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_handle() const { return Instr->getOperand(1); }
  llvm::Value *get_mipLevel() const { return Instr->getOperand(2); }
};

/// This instruction gathers the four texels that would be used in a bi-linear filtering operation
struct DxilInst_TextureGather {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_TextureGather(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::TextureGather);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (10 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_srv() const { return Instr->getOperand(1); }
  llvm::Value *get_sampler() const { return Instr->getOperand(2); }
  llvm::Value *get_coord0() const { return Instr->getOperand(3); }
  llvm::Value *get_coord1() const { return Instr->getOperand(4); }
  llvm::Value *get_coord2() const { return Instr->getOperand(5); }
  llvm::Value *get_coord3() const { return Instr->getOperand(6); }
  llvm::Value *get_offset0() const { return Instr->getOperand(7); }
  llvm::Value *get_offset1() const { return Instr->getOperand(8); }
  llvm::Value *get_channel() const { return Instr->getOperand(9); }
};

/// This instruction same as TextureGather, except this instrution performs comparison on texels, similar to SampleCmp
struct DxilInst_TextureGatherCmp {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_TextureGatherCmp(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::TextureGatherCmp);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (11 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_srv() const { return Instr->getOperand(1); }
  llvm::Value *get_sampler() const { return Instr->getOperand(2); }
  llvm::Value *get_coord0() const { return Instr->getOperand(3); }
  llvm::Value *get_coord1() const { return Instr->getOperand(4); }
  llvm::Value *get_coord2() const { return Instr->getOperand(5); }
  llvm::Value *get_coord3() const { return Instr->getOperand(6); }
  llvm::Value *get_offset0() const { return Instr->getOperand(7); }
  llvm::Value *get_offset1() const { return Instr->getOperand(8); }
  llvm::Value *get_channel() const { return Instr->getOperand(9); }
  llvm::Value *get_compareVale() const { return Instr->getOperand(10); }
};

/// This instruction gets the position of the specified sample
struct DxilInst_Texture2DMSGetSamplePosition {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_Texture2DMSGetSamplePosition(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::Texture2DMSGetSamplePosition);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (3 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_srv() const { return Instr->getOperand(1); }
  llvm::Value *get_index() const { return Instr->getOperand(2); }
};

/// This instruction gets the position of the specified sample
struct DxilInst_RenderTargetGetSamplePosition {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_RenderTargetGetSamplePosition(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::RenderTargetGetSamplePosition);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_index() const { return Instr->getOperand(1); }
};

/// This instruction gets the number of samples for a render target
struct DxilInst_RenderTargetGetSampleCount {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_RenderTargetGetSampleCount(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::RenderTargetGetSampleCount);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (1 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
};

/// This instruction performs an atomic operation on two operands
struct DxilInst_AtomicBinOp {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_AtomicBinOp(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::AtomicBinOp);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (7 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_handle() const { return Instr->getOperand(1); }
  llvm::Value *get_atomicOp() const { return Instr->getOperand(2); }
  llvm::Value *get_offset0() const { return Instr->getOperand(3); }
  llvm::Value *get_offset1() const { return Instr->getOperand(4); }
  llvm::Value *get_offset2() const { return Instr->getOperand(5); }
  llvm::Value *get_newValue() const { return Instr->getOperand(6); }
};

/// This instruction atomic compare and exchange to memory
struct DxilInst_AtomicCompareExchange {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_AtomicCompareExchange(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::AtomicCompareExchange);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (7 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_handle() const { return Instr->getOperand(1); }
  llvm::Value *get_offset0() const { return Instr->getOperand(2); }
  llvm::Value *get_offset1() const { return Instr->getOperand(3); }
  llvm::Value *get_offset2() const { return Instr->getOperand(4); }
  llvm::Value *get_compareValue() const { return Instr->getOperand(5); }
  llvm::Value *get_newValue() const { return Instr->getOperand(6); }
};

/// This instruction inserts a memory barrier in the shader
struct DxilInst_Barrier {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_Barrier(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::Barrier);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_barrierMode() const { return Instr->getOperand(1); }
  int32_t get_barrierMode_val() const { return (int32_t)(llvm::dyn_cast<llvm::ConstantInt>(Instr->getOperand(1))->getZExtValue()); }
};

/// This instruction calculates the level of detail
struct DxilInst_CalculateLOD {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_CalculateLOD(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::CalculateLOD);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (7 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_handle() const { return Instr->getOperand(1); }
  llvm::Value *get_sampler() const { return Instr->getOperand(2); }
  llvm::Value *get_coord0() const { return Instr->getOperand(3); }
  llvm::Value *get_coord1() const { return Instr->getOperand(4); }
  llvm::Value *get_coord2() const { return Instr->getOperand(5); }
  llvm::Value *get_clamped() const { return Instr->getOperand(6); }
};

/// This instruction discard the current pixel
struct DxilInst_Discard {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_Discard(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::Discard);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_condition() const { return Instr->getOperand(1); }
};

/// This instruction computes the rate of change of components per stamp
struct DxilInst_DerivCoarseX {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_DerivCoarseX(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::DerivCoarseX);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
};

/// This instruction computes the rate of change of components per stamp
struct DxilInst_DerivCoarseY {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_DerivCoarseY(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::DerivCoarseY);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
};

/// This instruction computes the rate of change of components per pixel
struct DxilInst_DerivFineX {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_DerivFineX(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::DerivFineX);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
};

/// This instruction computes the rate of change of components per pixel
struct DxilInst_DerivFineY {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_DerivFineY(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::DerivFineY);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
};

/// This instruction evaluates an input attribute at pixel center with an offset
struct DxilInst_EvalSnapped {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_EvalSnapped(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::EvalSnapped);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (6 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_inputSigId() const { return Instr->getOperand(1); }
  llvm::Value *get_inputRowIndex() const { return Instr->getOperand(2); }
  llvm::Value *get_inputColIndex() const { return Instr->getOperand(3); }
  llvm::Value *get_offsetX() const { return Instr->getOperand(4); }
  llvm::Value *get_offsetY() const { return Instr->getOperand(5); }
};

/// This instruction evaluates an input attribute at a sample location
struct DxilInst_EvalSampleIndex {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_EvalSampleIndex(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::EvalSampleIndex);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (5 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_inputSigId() const { return Instr->getOperand(1); }
  llvm::Value *get_inputRowIndex() const { return Instr->getOperand(2); }
  llvm::Value *get_inputColIndex() const { return Instr->getOperand(3); }
  llvm::Value *get_sampleIndex() const { return Instr->getOperand(4); }
};

/// This instruction evaluates an input attribute at pixel center
struct DxilInst_EvalCentroid {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_EvalCentroid(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::EvalCentroid);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (4 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_inputSigId() const { return Instr->getOperand(1); }
  llvm::Value *get_inputRowIndex() const { return Instr->getOperand(2); }
  llvm::Value *get_inputColIndex() const { return Instr->getOperand(3); }
};

/// This instruction reads the thread ID
struct DxilInst_ThreadId {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_ThreadId(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::ThreadId);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_component() const { return Instr->getOperand(1); }
};

/// This instruction reads the group ID (SV_GroupID)
struct DxilInst_GroupId {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_GroupId(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::GroupId);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_component() const { return Instr->getOperand(1); }
};

/// This instruction reads the thread ID within the group (SV_GroupThreadID)
struct DxilInst_ThreadIdInGroup {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_ThreadIdInGroup(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::ThreadIdInGroup);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_component() const { return Instr->getOperand(1); }
};

/// This instruction provides a flattened index for a given thread within a given group (SV_GroupIndex)
struct DxilInst_FlattenedThreadIdInGroup {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_FlattenedThreadIdInGroup(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::FlattenedThreadIdInGroup);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (1 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
};

/// This instruction emits a vertex to a given stream
struct DxilInst_EmitStream {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_EmitStream(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::EmitStream);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_streamId() const { return Instr->getOperand(1); }
};

/// This instruction completes the current primitive topology at the specified stream
struct DxilInst_CutStream {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_CutStream(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::CutStream);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_streamId() const { return Instr->getOperand(1); }
};

/// This instruction equivalent to an EmitStream followed by a CutStream
struct DxilInst_EmitThenCutStream {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_EmitThenCutStream(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::EmitThenCutStream);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_streamId() const { return Instr->getOperand(1); }
};

/// This instruction creates a double value
struct DxilInst_MakeDouble {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_MakeDouble(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::MakeDouble);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (3 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_lo() const { return Instr->getOperand(1); }
  llvm::Value *get_hi() const { return Instr->getOperand(2); }
};

/// This instruction splits a double into low and high parts
struct DxilInst_SplitDouble {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_SplitDouble(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::SplitDouble);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
};

/// This instruction LoadOutputControlPoint
struct DxilInst_LoadOutputControlPoint {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_LoadOutputControlPoint(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::LoadOutputControlPoint);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (5 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_inputSigId() const { return Instr->getOperand(1); }
  llvm::Value *get_row() const { return Instr->getOperand(2); }
  llvm::Value *get_col() const { return Instr->getOperand(3); }
  llvm::Value *get_index() const { return Instr->getOperand(4); }
};

/// This instruction LoadPatchConstant
struct DxilInst_LoadPatchConstant {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_LoadPatchConstant(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::LoadPatchConstant);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (4 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_inputSigId() const { return Instr->getOperand(1); }
  llvm::Value *get_row() const { return Instr->getOperand(2); }
  llvm::Value *get_col() const { return Instr->getOperand(3); }
};

/// This instruction DomainLocation
struct DxilInst_DomainLocation {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_DomainLocation(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::DomainLocation);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_component() const { return Instr->getOperand(1); }
  int8_t get_component_val() const { return (int8_t)(llvm::dyn_cast<llvm::ConstantInt>(Instr->getOperand(1))->getZExtValue()); }
};

/// This instruction StorePatchConstant
struct DxilInst_StorePatchConstant {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_StorePatchConstant(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::StorePatchConstant);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (5 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_outputSigID() const { return Instr->getOperand(1); }
  llvm::Value *get_row() const { return Instr->getOperand(2); }
  llvm::Value *get_col() const { return Instr->getOperand(3); }
  llvm::Value *get_value() const { return Instr->getOperand(4); }
};

/// This instruction OutputControlPointID
struct DxilInst_OutputControlPointID {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_OutputControlPointID(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::OutputControlPointID);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (1 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
};

/// This instruction PrimitiveID
struct DxilInst_PrimitiveID {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_PrimitiveID(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::PrimitiveID);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (1 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
};

/// This instruction CycleCounterLegacy
struct DxilInst_CycleCounterLegacy {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_CycleCounterLegacy(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::CycleCounterLegacy);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (1 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
};

/// This instruction returns the hyperbolic tangent of the specified value
struct DxilInst_Htan {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_Htan(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::Htan);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
};

/// This instruction returns 1 for the first lane in the wave
struct DxilInst_WaveIsFirstLane {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_WaveIsFirstLane(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::WaveIsFirstLane);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (1 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
};

/// This instruction returns the index of the current lane in the wave
struct DxilInst_WaveGetLaneIndex {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_WaveGetLaneIndex(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::WaveGetLaneIndex);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (1 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
};

/// This instruction returns the number of lanes in the wave
struct DxilInst_WaveGetLaneCount {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_WaveGetLaneCount(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::WaveGetLaneCount);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (1 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
};

/// This instruction returns 1 if any of the lane evaluates the value to true
struct DxilInst_WaveAnyTrue {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_WaveAnyTrue(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::WaveAnyTrue);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_cond() const { return Instr->getOperand(1); }
};

/// This instruction returns 1 if all the lanes evaluate the value to true
struct DxilInst_WaveAllTrue {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_WaveAllTrue(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::WaveAllTrue);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_cond() const { return Instr->getOperand(1); }
};

/// This instruction returns 1 if all the lanes have the same value
struct DxilInst_WaveActiveAllEqual {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_WaveActiveAllEqual(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::WaveActiveAllEqual);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
};

/// This instruction returns a struct with a bit set for each lane where the condition is true
struct DxilInst_WaveActiveBallot {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_WaveActiveBallot(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::WaveActiveBallot);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_cond() const { return Instr->getOperand(1); }
};

/// This instruction returns the value from the specified lane
struct DxilInst_WaveReadLaneAt {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_WaveReadLaneAt(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::WaveReadLaneAt);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (3 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
  llvm::Value *get_lane() const { return Instr->getOperand(2); }
};

/// This instruction returns the value from the first lane
struct DxilInst_WaveReadLaneFirst {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_WaveReadLaneFirst(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::WaveReadLaneFirst);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
};

/// This instruction returns the result the operation across waves
struct DxilInst_WaveActiveOp {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_WaveActiveOp(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::WaveActiveOp);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (4 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
  llvm::Value *get_op() const { return Instr->getOperand(2); }
  int8_t get_op_val() const { return (int8_t)(llvm::dyn_cast<llvm::ConstantInt>(Instr->getOperand(2))->getZExtValue()); }
  llvm::Value *get_sop() const { return Instr->getOperand(3); }
  int8_t get_sop_val() const { return (int8_t)(llvm::dyn_cast<llvm::ConstantInt>(Instr->getOperand(3))->getZExtValue()); }
};

/// This instruction returns the result of the operation across all lanes
struct DxilInst_WaveActiveBit {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_WaveActiveBit(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::WaveActiveBit);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (3 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
  llvm::Value *get_op() const { return Instr->getOperand(2); }
  int8_t get_op_val() const { return (int8_t)(llvm::dyn_cast<llvm::ConstantInt>(Instr->getOperand(2))->getZExtValue()); }
};

/// This instruction returns the result of the operation on prior lanes
struct DxilInst_WavePrefixOp {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_WavePrefixOp(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::WavePrefixOp);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (4 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
  llvm::Value *get_op() const { return Instr->getOperand(2); }
  int8_t get_op_val() const { return (int8_t)(llvm::dyn_cast<llvm::ConstantInt>(Instr->getOperand(2))->getZExtValue()); }
  llvm::Value *get_sop() const { return Instr->getOperand(3); }
  int8_t get_sop_val() const { return (int8_t)(llvm::dyn_cast<llvm::ConstantInt>(Instr->getOperand(3))->getZExtValue()); }
};

/// This instruction reads from a lane in the quad
struct DxilInst_QuadReadLaneAt {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_QuadReadLaneAt(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::QuadReadLaneAt);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (3 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
  llvm::Value *get_quadLane() const { return Instr->getOperand(2); }
  uint32_t get_quadLane_val() const { return (uint32_t)(llvm::dyn_cast<llvm::ConstantInt>(Instr->getOperand(2))->getZExtValue()); }
};

/// This instruction returns the result of a quad-level operation
struct DxilInst_QuadOp {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_QuadOp(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::QuadOp);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (3 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
  llvm::Value *get_op() const { return Instr->getOperand(2); }
  int8_t get_op_val() const { return (int8_t)(llvm::dyn_cast<llvm::ConstantInt>(Instr->getOperand(2))->getZExtValue()); }
};

/// This instruction bitcast between different sizes
struct DxilInst_BitcastI16toF16 {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_BitcastI16toF16(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::BitcastI16toF16);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
};

/// This instruction bitcast between different sizes
struct DxilInst_BitcastF16toI16 {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_BitcastF16toI16(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::BitcastF16toI16);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
};

/// This instruction bitcast between different sizes
struct DxilInst_BitcastI32toF32 {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_BitcastI32toF32(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::BitcastI32toF32);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
};

/// This instruction bitcast between different sizes
struct DxilInst_BitcastF32toI32 {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_BitcastF32toI32(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::BitcastF32toI32);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
};

/// This instruction bitcast between different sizes
struct DxilInst_BitcastI64toF64 {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_BitcastI64toF64(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::BitcastI64toF64);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
};

/// This instruction bitcast between different sizes
struct DxilInst_BitcastF64toI64 {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_BitcastF64toI64(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::BitcastF64toI64);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
};

/// This instruction GSInstanceID
struct DxilInst_GSInstanceID {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_GSInstanceID(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::GSInstanceID);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (1 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
};

/// This instruction legacy fuction to convert float (f32) to half (f16) (this is not related to min-precision)
struct DxilInst_LegacyF32ToF16 {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_LegacyF32ToF16(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::LegacyF32ToF16);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
};

/// This instruction legacy fuction to convert half (f16) to float (f32) (this is not related to min-precision)
struct DxilInst_LegacyF16ToF32 {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_LegacyF16ToF32(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::LegacyF16ToF32);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
};

/// This instruction legacy fuction to convert double to float
struct DxilInst_LegacyDoubleToFloat {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_LegacyDoubleToFloat(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::LegacyDoubleToFloat);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
};

/// This instruction legacy fuction to convert double to int32
struct DxilInst_LegacyDoubleToSInt32 {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_LegacyDoubleToSInt32(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::LegacyDoubleToSInt32);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
};

/// This instruction legacy fuction to convert double to uint32
struct DxilInst_LegacyDoubleToUInt32 {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_LegacyDoubleToUInt32(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::LegacyDoubleToUInt32);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
};

/// This instruction returns the count of bits set to 1 across the wave
struct DxilInst_WaveAllBitCount {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_WaveAllBitCount(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::WaveAllBitCount);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
};

/// This instruction returns the count of bits set to 1 on prior lanes
struct DxilInst_WavePrefixBitCount {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_WavePrefixBitCount(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::WavePrefixBitCount);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (2 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
  // Accessors
  llvm::Value *get_value() const { return Instr->getOperand(1); }
};

/// This instruction returns the sample index in a sample-frequency pixel shader
struct DxilInst_SampleIndex {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_SampleIndex(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::SampleIndex);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (1 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
};

/// This instruction returns the coverage mask input in a pixel shader
struct DxilInst_Coverage {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_Coverage(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::Coverage);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (1 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
};

/// This instruction returns underestimated coverage input from conservative rasterization in a pixel shader
struct DxilInst_InnerCoverage {
  const llvm::Instruction *Instr;
  // Construction and identification
  DxilInst_InnerCoverage(llvm::Instruction *pInstr) : Instr(pInstr) {}
  operator bool() const {
    return hlsl::OP::IsDxilOpFuncCallInst(Instr, hlsl::OP::OpCode::InnerCoverage);
  }
  // Validation support
  bool isAllowed() const { return true; }
  bool isArgumentListValid() const {
    if (1 != llvm::dyn_cast<llvm::CallInst>(Instr)->getNumArgOperands()) return false;
    return true;
  }
};
// INSTR-HELPER:END
} // namespace hlsl
