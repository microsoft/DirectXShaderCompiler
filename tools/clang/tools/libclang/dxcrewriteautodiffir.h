///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxcrewriteautodiffir.h                                                    //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Defines the typed, function-local IR used by the auto-diff rewriter.      //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_CLANG_TOOLS_LIBCLANG_DXCREWRITEAUTODIFFIR_H
#define LLVM_CLANG_TOOLS_LIBCLANG_DXCREWRITEAUTODIFFIR_H

#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "llvm/ADT/SmallVector.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace hlsl {
namespace autodiff {

enum class ADActivity { Inactive, Active };

enum class ADValueCategory { RValue, LValue };

struct ADBinding;

struct ADValueInfo {
  clang::QualType PrimalType;
  ADActivity Activity = ADActivity::Inactive;
  ADValueCategory ValueCategory = ADValueCategory::RValue;
  const clang::ValueDecl *SourceDecl = nullptr;
};

struct ADExpr {
  enum class Kind {
    Literal,
    DeclRef,
    LocalRef,
    LoopStateRef,
    PrimalLocal,
    This,
    Member,
    Swizzle,
    Subscript,
    AggregateConstruct,
    Cast,
    Unary,
    Binary,
    Conditional,
    Call,
    RuntimeLoopResult,
  };

  ADExpr(Kind K, const clang::Expr *SourceExpr);

  Kind K;
  ADValueInfo Value;
  clang::SourceRange Range;
  const clang::Expr *SourceExpr;
  const clang::ValueDecl *SourceDecl = nullptr;
  const ADBinding *Binding = nullptr;
  const clang::FunctionDecl *Callee = nullptr;
  unsigned LoopStateIndex = 0;
  unsigned LoopStateVersion = 0;
  const ADExpr *Receiver = nullptr;
  clang::UnaryOperatorKind UnaryOpcode = clang::UO_Plus;
  clang::BinaryOperatorKind BinaryOpcode = clang::BO_Add;
  llvm::SmallVector<const ADExpr *, 4> Operands;
  llvm::SmallVector<unsigned, 4> Components;
};

enum class ADPullbackRule {
  Unsupported,
  Leaf,
  Swizzle,
  Subscript,
  AggregateConstruct,
  Cast,
  Positive,
  Negative,
  Add,
  Subtract,
  Multiply,
  Divide,
  Conditional,
  Sin,
  Cos,
  Exp,
  Log,
  Sqrt,
  ComposedCall,
  CustomCall,
};

struct ADPullbackRuleInfo {
  ADPullbackRule Rule = ADPullbackRule::Unsupported;
  uint64_t PrimalOperandMask = 0;
};

ADPullbackRuleInfo getADPullbackRule(const ADExpr *Expression);
bool isADInactiveParameter(const clang::ParmVarDecl *Parameter);
const clang::FunctionDecl *
getADBackwardDerivative(const clang::FunctionDecl *Primal);
const clang::FunctionDecl *
getADPrimalSubstitute(const clang::FunctionDecl *Primal);

struct ADBinding {
  const clang::VarDecl *SourceDecl = nullptr;
  unsigned Version = 0;
  const ADExpr *Value = nullptr;
};

struct ADLoopState {
  const clang::VarDecl *SourceDecl = nullptr;
  clang::QualType PrimalType;
  const ADExpr *InitialValue = nullptr;
  const ADExpr *Result = nullptr;
  bool NeedsPrimalTape = false;
  unsigned FinalVersion = 0;
};

struct ADLoopPullbackInput {
  unsigned StateIndex = 0;
  unsigned Version = 0;
  bool NeedsPrimal = false;
};

struct ADLoopPullback {
  const ADExpr *Value = nullptr;
  llvm::SmallVector<ADLoopPullbackInput, 4> Inputs;
};

struct ADLoopTapeSlot {
  unsigned StateIndex = 0;
  unsigned Version = 0;
};

struct ADLoopUpdate {
  unsigned TargetStateIndex = 0;
  unsigned InputVersion = 0;
  unsigned ResultVersion = 0;
  clang::BinaryOperatorKind Opcode = clang::BO_Assign;
  const ADExpr *Value = nullptr;
  ADLoopPullback Pullback;
};

enum class ADLoopStorageKind { None, Static, Recompute, DynamicRequired };

struct ADLoopPlan {
  const clang::ForStmt *Source = nullptr;
  const clang::VarDecl *Counter = nullptr;
  const ADExpr *TripCount = nullptr;
  ADLoopStorageKind Storage = ADLoopStorageKind::None;
  unsigned TapeCapacity = 0;
  llvm::SmallVector<ADLoopState, 4> States;
  llvm::SmallVector<ADLoopUpdate, 4> Updates;
  llvm::SmallVector<ADLoopTapeSlot, 4> TapeSlots;
};

struct ADStmt {
  enum class Kind {
    Declare,
    Assign,
    Expression,
    PrimalLoop,
    ActiveLoop,
    Return
  };

  ADStmt() = default;
  ADStmt(Kind K, const ADBinding *Binding = nullptr,
         const ADExpr *Value = nullptr, const clang::Stmt *SourceStmt = nullptr)
      : K(K), Binding(Binding), Value(Value), SourceStmt(SourceStmt) {}

  Kind K = Kind::Declare;
  const ADBinding *Binding = nullptr;
  const ADExpr *Value = nullptr;
  const clang::Stmt *SourceStmt = nullptr;
  llvm::SmallVector<const ADExpr *, 4> Values;
  const ADLoopPlan *Loop = nullptr;
};

struct ADFunctionPlan {
  const clang::FunctionDecl *Source = nullptr;
  clang::QualType ResultType;
  std::vector<std::unique_ptr<ADExpr>> Expressions;
  std::vector<std::unique_ptr<ADBinding>> Bindings;
  std::vector<std::unique_ptr<ADLoopPlan>> Loops;
  std::vector<ADStmt> Statements;
  llvm::SmallVector<const clang::VarDecl *, 4> PrimalLocals;
};

// Build a typed plan for a straight-line function. Returns false when the
// function contains a construct not represented by this initial IR slice; the
// caller may then use the legacy emitter for the whole function.
bool BuildADFunctionPlan(const clang::FunctionDecl *FD, ADFunctionPlan &Plan,
                         std::string &Reason);

} // namespace autodiff
} // namespace hlsl

#endif // LLVM_CLANG_TOOLS_LIBCLANG_DXCREWRITEAUTODIFFIR_H
