///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilUtil.h                                                                //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// DXIL helper functions.                                                    //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once
#include <unordered_set>

namespace llvm {
class Type;
class GlobalVariable;
class Function;
class Module;
class MemoryBuffer;
class LLVMContext;
class DiagnosticInfo;
class Value;
class Instruction;
class BasicBlock;
class StringRef;
}

namespace hlsl {

class DxilFieldAnnotation;
class DxilTypeSystem;

namespace dxilutil {
  unsigned
  GetLegacyCBufferFieldElementSize(DxilFieldAnnotation &fieldAnnotation,
                                   llvm::Type *Ty, DxilTypeSystem &typeSys);
  llvm::Type *GetArrayEltTy(llvm::Type *Ty);
  bool HasDynamicIndexing(llvm::Value *V);

  // Find alloca insertion point, given instruction
  llvm::Instruction *FindAllocaInsertionPt(llvm::Instruction* I);
  llvm::Instruction *FindAllocaInsertionPt(llvm::Function* F);
  llvm::Instruction *SkipAllocas(llvm::Instruction *I);
  // Get first non-alloca insertion point, to avoid inserting non-allocas before alloca
  llvm::Instruction *FirstNonAllocaInsertionPt(llvm::Instruction* I);
  llvm::Instruction *FirstNonAllocaInsertionPt(llvm::BasicBlock* BB);
  llvm::Instruction *FirstNonAllocaInsertionPt(llvm::Function* F);

  bool IsStaticGlobal(llvm::GlobalVariable *GV);
  bool IsSharedMemoryGlobal(llvm::GlobalVariable *GV);
  bool RemoveUnusedFunctions(llvm::Module &M, llvm::Function *EntryFunc,
                             llvm::Function *PatchConstantFunc, bool IsLib);
  void EmitResMappingError(llvm::Instruction *Res);
  // Simple demangle just support case "\01?name@" pattern.
  llvm::StringRef DemangleFunctionName(llvm::StringRef name);
  // Change select/phi on operands into select/phi on operation.
  // phi0 = phi a0, b0, c0
  // phi1 = phi a1, b1, c1
  // Inst = Add(phi0, phi1);
  // into
  // A = Add(a0, a1);
  // B = Add(b0, b1);
  // C = Add(c0, c1);
  // NewInst = phi A, B, C
  // Only support 1 operand now, other oerands should be Constant.
  llvm::Value * SelectOnOperation(llvm::Instruction *Inst, unsigned operandIdx);
  // Collect all select operand used by Inst.
  void CollectSelect(llvm::Instruction *Inst,
                   std::unordered_set<llvm::Instruction *> &selectSet);
  // If all operands are the same for a select inst, replace it with the operand.
  bool MergeSelectOnSameValue(llvm::Instruction *SelInst, unsigned startOpIdx,
                            unsigned numOperands);
  std::unique_ptr<llvm::Module> LoadModuleFromBitcode(llvm::StringRef BC,
    llvm::LLVMContext &Ctx, std::string &DiagStr);
  std::unique_ptr<llvm::Module> LoadModuleFromBitcode(llvm::MemoryBuffer *MB,
    llvm::LLVMContext &Ctx, std::string &DiagStr);
  void PrintDiagnosticHandler(const llvm::DiagnosticInfo &DI, void *Context);
}

}