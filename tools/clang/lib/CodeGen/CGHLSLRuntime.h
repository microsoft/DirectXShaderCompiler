//===----- CGMSHLSLRuntime.h - Interface to HLSL Runtime ----------------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// CGHLSLRuntime.h                                                           //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
//  This provides a class for HLSL code generation.                          //
//
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <functional>

namespace llvm {
class Function;
template <typename T, unsigned N> class SmallVector;
class Value;
class Constant;
class TerminatorInst;
class GlobalVariable;
class Type;
template <typename T> class ArrayRef;
}

namespace clang {
class Decl;
class QualType;
class ExtVectorType;
class ASTContext;
class FunctionDecl;
class CallExpr;
class InitListExpr;
class Expr;
class Stmt;
class Attr;
class VarDecl;
class HLSLRootSignatureAttr;

namespace CodeGen {
class CodeGenModule;
class ReturnValueSlot;
class CodeGenFunction;

class RValue;
class LValue;

class CGHLSLRuntime {
protected:
  CodeGenModule &CGM;

public:
  CGHLSLRuntime(CodeGenModule &CGM) : CGM(CGM) {}
  virtual ~CGHLSLRuntime();

  virtual void addResource(Decl *D) = 0;
  virtual void addSubobject(Decl *D) = 0;
  virtual void FinishCodeGen() = 0;
  virtual RValue EmitHLSLBuiltinCallExpr(CodeGenFunction &CGF,
                                         const FunctionDecl *FD,
                                         const CallExpr *E,
                                         ReturnValueSlot ReturnValue) = 0;
  // Is E is a c++ init list not a hlsl init list which only match size.
  virtual bool IsTrivalInitListExpr(CodeGenFunction &CGF, InitListExpr *E) = 0;
  virtual llvm::Value *EmitHLSLInitListExpr(CodeGenFunction &CGF, InitListExpr *E,
      // The destPtr when emiting aggregate init, for normal case, it will be null.
      llvm::Value *DestPtr) = 0;
  virtual llvm::Constant *EmitHLSLConstInitListExpr(CodeGenModule &CGM, InitListExpr *E) = 0;

  virtual void EmitHLSLOutParamConversionInit(
      CodeGenFunction &CGF, const FunctionDecl *FD, const CallExpr *E,
      llvm::SmallVector<LValue, 8> &castArgList,
      llvm::SmallVector<const Stmt *, 8> &argList,
      const std::function<void(const VarDecl *, llvm::Value *)> &TmpArgMap) = 0;
  virtual void EmitHLSLOutParamConversionCopyBack(
      CodeGenFunction &CGF, llvm::SmallVector<LValue, 8> &castArgList) = 0;
  virtual llvm::Value *EmitHLSLMatrixOperationCall(CodeGenFunction &CGF, const clang::Expr *E, llvm::Type *RetType,
      llvm::ArrayRef<llvm::Value*> paramList) = 0;
  virtual void EmitHLSLDiscard(CodeGenFunction &CGF) = 0;

  // For [] on matrix
  virtual llvm::Value *EmitHLSLMatrixSubscript(CodeGenFunction &CGF,
                                          llvm::Type *RetType,
                                          llvm::Value *Ptr, llvm::Value *Idx,
                                          clang::QualType Ty) = 0;
  // For ._m on matrix
  virtual llvm::Value *EmitHLSLMatrixElement(CodeGenFunction &CGF,
                                          llvm::Type *RetType,
                                          llvm::ArrayRef<llvm::Value*> paramList,
                                          clang::QualType Ty) = 0;

  virtual llvm::Value *EmitHLSLMatrixLoad(CodeGenFunction &CGF,
                                          llvm::Value *Ptr,
                                          clang::QualType Ty) = 0;
  virtual void EmitHLSLMatrixStore(CodeGenFunction &CGF, llvm::Value *Val,
                                   llvm::Value *DestPtr,
                                   clang::QualType Ty) = 0;
  virtual void EmitHLSLAggregateCopy(CodeGenFunction &CGF, llvm::Value *SrcPtr,
                                   llvm::Value *DestPtr,
                                   clang::QualType Ty) = 0;
  virtual void EmitHLSLAggregateStore(CodeGenFunction &CGF, llvm::Value *Val,
                                   llvm::Value *DestPtr,
                                   clang::QualType Ty) = 0;
  virtual void EmitHLSLFlatConversionToAggregate(CodeGenFunction &CGF, llvm::Value *Val,
                                   llvm::Value *DestPtr,
                                   clang::QualType Ty, clang::QualType SrcTy) = 0;
  virtual void EmitHLSLFlatConversionAggregateCopy(CodeGenFunction &CGF, llvm::Value *SrcPtr,
                                   clang::QualType SrcTy,
                                   llvm::Value *DestPtr,
                                   clang::QualType DestTy) = 0;
  virtual void EmitHLSLRootSignature(CodeGenFunction &CGF,
                                     clang::HLSLRootSignatureAttr *RSA,
                                     llvm::Function *Fn) = 0;
  virtual llvm::Value *EmitHLSLLiteralCast(CodeGenFunction &CGF, llvm::Value *Src, clang::QualType SrcType,
                                               clang::QualType DstType) = 0;

  virtual void AddHLSLFunctionInfo(llvm::Function *, const FunctionDecl *FD) = 0;
  virtual void EmitHLSLFunctionProlog(llvm::Function *, const FunctionDecl *FD) = 0;

  
  virtual void AddControlFlowHint(CodeGenFunction &CGF, const Stmt &S, llvm::TerminatorInst *TI, llvm::ArrayRef<const Attr *> Attrs) = 0;

  virtual void FinishAutoVar(CodeGenFunction &CGF, const VarDecl &D, llvm::Value *V) = 0;
  static const clang::ExtVectorType *
  ConvertHLSLVecMatTypeToExtVectorType(const clang::ASTContext &context,
                                       clang::QualType &type);
  static bool IsHLSLVecMatType(clang::QualType &type);
  static clang::QualType GetHLSLVecMatElementType(clang::QualType type);
};

/// Create an instance of a HLSL runtime class.
CGHLSLRuntime *CreateMSHLSLRuntime(CodeGenModule &CGM);
}
}
