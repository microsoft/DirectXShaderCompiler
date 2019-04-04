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
#include <string>
#include <memory>
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Twine.h"
#include "llvm/IR/Constants.h"

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
class raw_ostream;
class ModulePass;
class PassRegistry;
class DebugLoc;

ModulePass *createDxilLoadMetadataPass();
void initializeDxilLoadMetadataPass(llvm::PassRegistry&);
}

namespace hlsl {

class DxilFieldAnnotation;
class DxilTypeSystem;

namespace dxilutil {
  extern const char ManglingPrefix[];
  extern const char EntryPrefix[];
  extern const llvm::StringRef kResourceMapErrorMsg;

  unsigned
  GetLegacyCBufferFieldElementSize(DxilFieldAnnotation &fieldAnnotation,
                                   llvm::Type *Ty, DxilTypeSystem &typeSys);
  llvm::Type *GetArrayEltTy(llvm::Type *Ty);
  bool HasDynamicIndexing(llvm::Value *V);

  // Find alloca insertion point, given instruction
  llvm::Instruction *FindAllocaInsertionPt(llvm::Instruction* I); // Considers entire parent function
  llvm::Instruction *FindAllocaInsertionPt(llvm::BasicBlock* BB); // Only considers provided block
  llvm::Instruction *FindAllocaInsertionPt(llvm::Function* F);
  llvm::Instruction *SkipAllocas(llvm::Instruction *I);
  // Get first non-alloca insertion point, to avoid inserting non-allocas before alloca
  llvm::Instruction *FirstNonAllocaInsertionPt(llvm::Instruction* I); // Considers entire parent function
  llvm::Instruction *FirstNonAllocaInsertionPt(llvm::BasicBlock* BB); // Only considers provided block
  llvm::Instruction *FirstNonAllocaInsertionPt(llvm::Function* F);

  bool IsStaticGlobal(llvm::GlobalVariable *GV);
  bool IsSharedMemoryGlobal(llvm::GlobalVariable *GV);
  bool RemoveUnusedFunctions(llvm::Module &M, llvm::Function *EntryFunc,
                             llvm::Function *PatchConstantFunc, bool IsLib);
  void EmitErrorOnInstruction(llvm::Instruction *I, llvm::StringRef Msg);
  void EmitResMappingError(llvm::Instruction *Res);
  std::string FormatMessageAtLocation(const llvm::DebugLoc &DL, const llvm::Twine& Msg);
  llvm::Twine FormatMessageWithoutLocation(const llvm::Twine& Msg);
  // Simple demangle just support case "\01?name@" pattern.
  llvm::StringRef DemangleFunctionName(llvm::StringRef name);
  // ReplaceFunctionName replaces the undecorated portion of originalName with undecorated newName
  std::string ReplaceFunctionName(llvm::StringRef originalName, llvm::StringRef newName);
  void PrintEscapedString(llvm::StringRef Name, llvm::raw_ostream &Out);
  void PrintUnescapedString(llvm::StringRef Name, llvm::raw_ostream &Out);
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
  // Returns replacement value if successful
  llvm::Value *MergeSelectOnSameValue(llvm::Instruction *SelInst,
                                      unsigned startOpIdx,
                                      unsigned numOperands);
  bool SimplifyTrivialPHIs(llvm::BasicBlock *BB);
  void MigrateDebugValue(llvm::Value *Old, llvm::Value *New);
  void TryScatterDebugValueToVectorElements(llvm::Value *Val);
  std::unique_ptr<llvm::Module> LoadModuleFromBitcode(llvm::StringRef BC,
    llvm::LLVMContext &Ctx, std::string &DiagStr);
  std::unique_ptr<llvm::Module> LoadModuleFromBitcode(llvm::MemoryBuffer *MB,
    llvm::LLVMContext &Ctx, std::string &DiagStr);
  void PrintDiagnosticHandler(const llvm::DiagnosticInfo &DI, void *Context);
  bool IsIntegerOrFloatingPointType(llvm::Type *Ty);
  // Returns true if type contains HLSL Object type (resource)
  bool ContainsHLSLObjectType(llvm::Type *Ty);
  bool IsHLSLResourceType(llvm::Type *Ty);
  bool IsHLSLObjectType(llvm::Type *Ty);
  bool IsSplat(llvm::ConstantDataVector *cdv);
}

}
