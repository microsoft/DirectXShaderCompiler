///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilOperations.h                                                          //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Implementation of DXIL operation tables.                                  //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once

namespace llvm {
class LLVMContext;
class Module;
class Type;
class Function;
class Constant;
class Value;
class Instruction;
};
#include "llvm/IR/Attributes.h"

#include "DxilConstants.h"
#include <unordered_map>

namespace hlsl {

/// Use this utility class to interact with DXIL operations.
class OP {
public:
  using OpCode = DXIL::OpCode;
  using OpCodeClass = DXIL::OpCodeClass;

public:
  OP() = delete;
  OP(llvm::LLVMContext &Ctx, llvm::Module *pModule);

  llvm::Function *GetOpFunc(OpCode OpCode, llvm::Type *pOverloadType);
  llvm::ArrayRef<llvm::Function *> GetOpFuncList(OpCode OpCode) const;
  void RemoveFunction(llvm::Function *F);
  llvm::Type *GetOverloadType(OpCode OpCode, llvm::Function *F);
  llvm::LLVMContext &GetCtx() { return m_Ctx; }
  llvm::Type *GetHandleType() const;
  llvm::Type *GetDimensionsType() const;
  llvm::Type *GetSamplePosType() const;
  llvm::Type *GetBinaryWithCarryType() const;
  llvm::Type *GetBinaryWithTwoOutputsType() const;
  llvm::Type *GetSplitDoubleType() const;
  llvm::Type *GetInt4Type() const;

  llvm::Type *GetResRetType(llvm::Type *pOverloadType);
  llvm::Type *GetCBufferRetType(llvm::Type *pOverloadType);

  // LLVM helpers. Perhaps, move to a separate utility class.
  llvm::Constant *GetI1Const(bool v);
  llvm::Constant *GetI8Const(char v);
  llvm::Constant *GetU8Const(unsigned char v);
  llvm::Constant *GetI16Const(int v);
  llvm::Constant *GetU16Const(unsigned v);
  llvm::Constant *GetI32Const(int v);
  llvm::Constant *GetU32Const(unsigned v);
  llvm::Constant *GetU64Const(unsigned long long v);
  llvm::Constant *GetFloatConst(float v);
  llvm::Constant *GetDoubleConst(double v);

  static OpCode GetDxilOpFuncCallInst(const llvm::Instruction *I);
  static const char *GetOpCodeName(OpCode OpCode);
  static const char *GetAtomicOpName(DXIL::AtomicBinOpCode OpCode);
  static const OpCodeClass GetOpCodeClass(OpCode OpCode);
  static const char *GetOpCodeClassName(OpCode OpCode);
  static bool IsOverloadLegal(OpCode OpCode, llvm::Type *pType);
  static bool CheckOpCodeTable();
  static bool IsDxilOpFunc(const llvm::Function *F);
  static bool IsDxilOpFuncCallInst(const llvm::Instruction *I);
  static bool IsDxilOpFuncCallInst(const llvm::Instruction *I, OpCode opcode);
  static bool IsDxilOpWave(OpCode C);
  static bool IsDxilOpGradient(OpCode C);

private:
  // Per-module properties.
  llvm::LLVMContext &m_Ctx;
  llvm::Module *m_pModule;

  llvm::Type *m_pHandleType;
  llvm::Type *m_pDimensionsType;
  llvm::Type *m_pSamplePosType;
  llvm::Type *m_pBinaryWithCarryType;
  llvm::Type *m_pBinaryWithTwoOutputsType;
  llvm::Type *m_pSplitDoubleType;
  llvm::Type *m_pInt4Type;

  static const unsigned kNumTypeOverloads = 9;

  llvm::Type *m_pResRetType[kNumTypeOverloads];
  llvm::Type *m_pCBufferRetType[kNumTypeOverloads];

  struct OpCodeCacheItem {
    llvm::Function *pOverloads[kNumTypeOverloads];
  };
  OpCodeCacheItem m_OpCodeClassCache[(unsigned)OpCodeClass::NumOpClasses];
  std::unordered_map<llvm::Function *, OpCodeClass> m_FunctionToOpClass;

private:
  // Static properties.
  struct OpCodeProperty {
    OpCode OpCode;
    const char *pOpCodeName;
    OpCodeClass OpCodeClass;
    const char *pOpCodeClassName;
    bool bAllowOverload[kNumTypeOverloads];   // void, h,f,d, i1, i8,i16,i32,i64
    llvm::Attribute::AttrKind FuncAttr;
  };
  static const OpCodeProperty m_OpCodeProps[(unsigned)OpCode::NumOpCodes];

  static const char *m_OverloadTypeName[kNumTypeOverloads];
  static const char *m_NamePrefix;
  static unsigned GetTypeSlot(llvm::Type *pType);
  static const char *GetOverloadTypeName(unsigned TypeSlot);
};

} // namespace hlsl
