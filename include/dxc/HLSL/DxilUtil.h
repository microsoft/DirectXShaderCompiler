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

namespace llvm {
class Type;
class GlobalVariable;
class Function;
class Module;
class MemoryBuffer;
class LLVMContext;
class DiagnosticInfo;
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

  bool IsStaticGlobal(llvm::GlobalVariable *GV);
  bool IsSharedMemoryGlobal(llvm::GlobalVariable *GV);
  bool RemoveUnusedFunctions(llvm::Module &M, llvm::Function *EntryFunc,
                             llvm::Function *PatchConstantFunc, bool IsLib);
  void EmitResMappingError(llvm::Instruction *Res);
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
  std::unique_ptr<llvm::Module> LoadModuleFromBitcode(llvm::StringRef BC,
    llvm::LLVMContext &Ctx, std::string &DiagStr);
  std::unique_ptr<llvm::Module> LoadModuleFromBitcode(llvm::MemoryBuffer *MB,
    llvm::LLVMContext &Ctx, std::string &DiagStr);
  void PrintDiagnosticHandler(const llvm::DiagnosticInfo &DI, void *Context);
}

}