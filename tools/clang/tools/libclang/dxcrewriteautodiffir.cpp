///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// dxcrewriteautodiffir.cpp                                                  //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Builds the typed, function-local IR used by the auto-diff rewriter.       //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "dxcrewriteautodiffir.h"

#include "clang/AST/ASTContext.h"
#include "clang/AST/Attr.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/HlslTypes.h"
#include "clang/AST/Stmt.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallPtrSet.h"

#include <algorithm>
#include <cassert>

using namespace clang;
using namespace llvm;

namespace hlsl {
namespace autodiff {

bool isADInactiveParameter(const ParmVarDecl *Parameter) {
  return Parameter->hasAttr<HLSLNoDiffAttr>() ||
         hlsl::IsHLSLResourceCarrierType(Parameter->getType());
}

static bool isADBackwardDerivativeMatch(const FunctionDecl *Primal,
                                        const FunctionDecl *Derivative) {
  if (Primal->getCanonicalDecl() == Derivative->getCanonicalDecl())
    return false;
  const auto *PrimalMethod = dyn_cast<CXXMethodDecl>(Primal);
  const auto *DerivativeMethod = dyn_cast<CXXMethodDecl>(Derivative);
  if ((PrimalMethod != nullptr) != (DerivativeMethod != nullptr) ||
      (PrimalMethod &&
       (PrimalMethod->getParent()->getCanonicalDecl() !=
            DerivativeMethod->getParent()->getCanonicalDecl() ||
        PrimalMethod->isStatic() != DerivativeMethod->isStatic())) ||
      !Derivative->getReturnType()->isVoidType())
    return false;

  unsigned ActiveParameters = 0;
  for (const ParmVarDecl *Parameter : Primal->parameters())
    ActiveParameters += !isADInactiveParameter(Parameter);
  if (Derivative->getNumParams() !=
      Primal->getNumParams() + 1 + ActiveParameters)
    return false;

  ASTContext &Context = Primal->getASTContext();
  for (unsigned I = 0; I < Primal->getNumParams(); ++I) {
    const ParmVarDecl *Input = Derivative->getParamDecl(I);
    if (!Context.hasSameType(Primal->getParamDecl(I)->getType(),
                             Input->getType().getNonReferenceType()) ||
        Input->hasAttr<HLSLOutAttr>())
      return false;
  }
  unsigned SeedIndex = Primal->getNumParams();
  const ParmVarDecl *Seed = Derivative->getParamDecl(SeedIndex);
  if (!Context.hasSameType(Primal->getReturnType(),
                           Seed->getType().getNonReferenceType()) ||
      Seed->hasAttr<HLSLOutAttr>())
    return false;

  unsigned OutputIndex = SeedIndex + 1;
  for (const ParmVarDecl *Parameter : Primal->parameters()) {
    if (isADInactiveParameter(Parameter))
      continue;
    const ParmVarDecl *Output = Derivative->getParamDecl(OutputIndex++);
    if (!Context.hasSameType(Parameter->getType(),
                             Output->getType().getNonReferenceType()) ||
        !Output->hasAttr<HLSLOutAttr>() || Output->hasAttr<HLSLInAttr>())
      return false;
  }
  return true;
}

static unsigned getADParameterDirection(const ParmVarDecl *Parameter) {
  bool IsOut = Parameter->hasAttr<HLSLOutAttr>();
  bool IsIn = Parameter->hasAttr<HLSLInAttr>();
  if (IsOut && IsIn)
    return 2;
  return IsOut ? 1 : 0;
}

static bool isADPrimalSubstituteMatch(const FunctionDecl *Primal,
                                      const FunctionDecl *Substitute) {
  if (Primal->getCanonicalDecl() == Substitute->getCanonicalDecl())
    return false;
  const auto *PrimalMethod = dyn_cast<CXXMethodDecl>(Primal);
  const auto *SubstituteMethod = dyn_cast<CXXMethodDecl>(Substitute);
  if ((PrimalMethod != nullptr) != (SubstituteMethod != nullptr) ||
      (PrimalMethod &&
       (PrimalMethod->getParent()->getCanonicalDecl() !=
            SubstituteMethod->getParent()->getCanonicalDecl() ||
        PrimalMethod->isStatic() != SubstituteMethod->isStatic())))
    return false;

  ASTContext &Context = Primal->getASTContext();
  if (!Context.hasSameType(Primal->getReturnType(),
                           Substitute->getReturnType()) ||
      Primal->getNumParams() != Substitute->getNumParams())
    return false;
  for (unsigned I = 0; I < Primal->getNumParams(); ++I) {
    const ParmVarDecl *PrimalParameter = Primal->getParamDecl(I);
    const ParmVarDecl *SubstituteParameter = Substitute->getParamDecl(I);
    if (!Context.hasSameType(
            PrimalParameter->getType().getNonReferenceType(),
            SubstituteParameter->getType().getNonReferenceType()) ||
        getADParameterDirection(PrimalParameter) !=
            getADParameterDirection(SubstituteParameter) ||
        PrimalParameter->hasAttr<HLSLNoDiffAttr>() !=
            SubstituteParameter->hasAttr<HLSLNoDiffAttr>())
      return false;
  }
  return true;
}

ADExpr::ADExpr(Kind K, const Expr *SourceExpr)
    : K(K), Range(SourceExpr ? SourceExpr->getSourceRange() : SourceRange()),
      SourceExpr(SourceExpr) {
  if (!SourceExpr)
    return;
  Value.PrimalType = SourceExpr->getType();
  Value.ValueCategory = SourceExpr->isLValue() ? ADValueCategory::LValue
                                               : ADValueCategory::RValue;
}

const FunctionDecl *getADBackwardDerivative(const FunctionDecl *Primal) {
  if (!Primal)
    return nullptr;
  for (const FunctionDecl *Redecl : Primal->redecls()) {
    const auto *Attr = Redecl->getAttr<HLSLBackwardDerivativeAttr>();
    if (!Attr)
      continue;
    const FunctionDecl *Derivative = nullptr;
    SmallPtrSet<const FunctionDecl *, 4> SeenCandidates;
    for (const NamedDecl *Candidate : Redecl->getDeclContext()->lookup(
             DeclarationName(Attr->getDerivative())))
      if (const auto *Function = dyn_cast<FunctionDecl>(Candidate)) {
        if (!SeenCandidates.insert(Function->getCanonicalDecl()).second ||
            !isADBackwardDerivativeMatch(Redecl, Function))
          continue;
        if (Derivative)
          return nullptr;
        Derivative = Function;
      }
    if (Derivative)
      return Derivative->getCanonicalDecl();
  }
  return nullptr;
}

const FunctionDecl *getADPrimalSubstitute(const FunctionDecl *Primal) {
  if (!Primal)
    return nullptr;
  Primal = Primal->getCanonicalDecl();
  const FunctionDecl *Substitute = nullptr;
  SmallPtrSet<const FunctionDecl *, 4> SeenCandidates;
  for (const Decl *D : Primal->getDeclContext()->decls()) {
    const auto *Candidate = dyn_cast<FunctionDecl>(D);
    if (!Candidate ||
        !SeenCandidates.insert(Candidate->getCanonicalDecl()).second)
      continue;
    for (const FunctionDecl *Redecl : Candidate->redecls()) {
      const auto *Attr = Redecl->getAttr<HLSLPrimalSubstituteOfAttr>();
      if (!Attr || Attr->getPrimal() != Primal->getIdentifier() ||
          !isADPrimalSubstituteMatch(Primal, Redecl))
        continue;
      if (Substitute)
        return nullptr;
      Substitute = Redecl->getCanonicalDecl();
    }
  }
  return Substitute;
}

ADPullbackRuleInfo getADPullbackRule(const ADExpr *Expression) {
  switch (Expression->K) {
  case ADExpr::Kind::DeclRef:
  case ADExpr::Kind::LocalRef:
  case ADExpr::Kind::LoopStateRef:
    return {ADPullbackRule::Leaf, 0};
  case ADExpr::Kind::Swizzle:
    return {ADPullbackRule::Swizzle, 0};
  case ADExpr::Kind::Subscript:
    return {ADPullbackRule::Subscript, 0};
  case ADExpr::Kind::AggregateConstruct:
    return {ADPullbackRule::AggregateConstruct, 0};
  case ADExpr::Kind::Cast:
    return {ADPullbackRule::Cast, 0};
  case ADExpr::Kind::Unary:
    if (Expression->UnaryOpcode == UO_Plus)
      return {ADPullbackRule::Positive, 0};
    if (Expression->UnaryOpcode == UO_Minus)
      return {ADPullbackRule::Negative, 0};
    return {};
  case ADExpr::Kind::Binary:
    switch (Expression->BinaryOpcode) {
    case BO_Add:
      return {ADPullbackRule::Add, 0};
    case BO_Sub:
      return {ADPullbackRule::Subtract, 0};
    case BO_Mul:
      return {ADPullbackRule::Multiply,
              Expression->Operands[0]->Value.Activity == ADActivity::Active &&
                      Expression->Operands[1]->Value.Activity ==
                          ADActivity::Active
                  ? 3u
                  : 0u};
    case BO_Div:
      return {ADPullbackRule::Divide,
              Expression->Operands[1]->Value.Activity == ADActivity::Active
                  ? 3u
                  : 0u};
    default:
      return {};
    }
  case ADExpr::Kind::Conditional:
    return {ADPullbackRule::Conditional, 0};
  case ADExpr::Kind::Call: {
    if (!Expression->Callee)
      return {};
    if (!Expression->Receiver && Expression->Operands.size() == 1) {
      ADPullbackRule Intrinsic =
          StringSwitch<ADPullbackRule>(Expression->Callee->getName())
              .Case("sin", ADPullbackRule::Sin)
              .Case("cos", ADPullbackRule::Cos)
              .Case("exp", ADPullbackRule::Exp)
              .Case("log", ADPullbackRule::Log)
              .Case("sqrt", ADPullbackRule::Sqrt)
              .Default(ADPullbackRule::Unsupported);
      if (Intrinsic != ADPullbackRule::Unsupported)
        return {Intrinsic, 1};
    }
    if (Expression->Receiver &&
        Expression->Receiver->Value.Activity == ADActivity::Active)
      return {};
    if (getADBackwardDerivative(Expression->Callee)) {
      uint64_t PrimalOperandMask = 0;
      for (unsigned I = 0; I < Expression->Operands.size(); ++I)
        if (!isADInactiveParameter(Expression->Callee->getParamDecl(I)) &&
            Expression->Operands[I]->Value.Activity == ADActivity::Active)
          PrimalOperandMask |= uint64_t(1) << I;
      return {ADPullbackRule::CustomCall, PrimalOperandMask};
    }
    bool HasBackward = false;
    for (const FunctionDecl *Redecl : Expression->Callee->redecls())
      if (const auto *Attr = Redecl->getAttr<HLSLAutoDiffAttr>())
        HasBackward |= Attr->hasBackward();
    if (!HasBackward || Expression->Operands.size() > 64)
      return {};
    uint64_t PrimalOperandMask = 0;
    for (unsigned I = 0; I < Expression->Operands.size(); ++I)
      if (Expression->Operands[I]->Value.Activity == ADActivity::Active)
        PrimalOperandMask |= uint64_t(1) << I;
    return {ADPullbackRule::ComposedCall, PrimalOperandMask};
  }
  default:
    return {};
  }
}

namespace {

const ValueDecl *getCanonicalValueDecl(const ValueDecl *D) {
  return D ? cast<ValueDecl>(D->getCanonicalDecl()) : nullptr;
}

ADActivity combineActivity(ArrayRef<const ADExpr *> Operands) {
  for (const ADExpr *Operand : Operands)
    if (Operand->Value.Activity == ADActivity::Active)
      return ADActivity::Active;
  return ADActivity::Inactive;
}

unsigned getComponentCount(QualType Type) {
  if (hlsl::IsHLSLVecType(Type))
    return hlsl::GetHLSLVecSize(Type);
  if (const auto *Vector =
          dyn_cast<VectorType>(Type.getCanonicalType().getTypePtr()))
    return Vector->getNumElements();
  return 1;
}

QualType getElementType(QualType Type) {
  if (hlsl::IsHLSLVecType(Type))
    return hlsl::GetHLSLVecElementType(Type);
  if (const auto *Vector =
          dyn_cast<VectorType>(Type.getCanonicalType().getTypePtr()))
    return Vector->getElementType();
  return Type;
}

bool isDifferentiableFloatingCast(QualType Source, QualType Destination) {
  return getComponentCount(Source) == getComponentCount(Destination) &&
         getElementType(Source)->isFloatingType() &&
         getElementType(Destination)->isFloatingType();
}

class ADFunctionBuilder {
public:
  ADFunctionBuilder(const FunctionDecl *FD, ADFunctionPlan &Plan,
                    std::string &Reason)
      : FD(FD), Ctx(FD->getASTContext()), Plan(Plan), Reason(Reason) {}

  bool build() {
    const auto *Body = dyn_cast_or_null<CompoundStmt>(FD->getBody());
    if (!Body)
      return fail("function has no compound body");

    Plan.Source = FD;
    Plan.ResultType = FD->getReturnType();
    for (const Stmt *S : Body->body())
      if (!buildStmt(S))
        return false;
    return true;
  }

private:
  const FunctionDecl *FD;
  ASTContext &Ctx;
  ADFunctionPlan &Plan;
  std::string &Reason;
  DenseMap<const ValueDecl *, const ADBinding *> CurrentBindings;

  bool fail(StringRef Message) {
    if (Reason.empty())
      Reason = Message.str();
    return false;
  }

  ADExpr *createExpr(ADExpr::Kind K, const Expr *Source) {
    std::unique_ptr<ADExpr> Node(new ADExpr(K, Source));
    ADExpr *Result = Node.get();
    Plan.Expressions.push_back(std::move(Node));
    return Result;
  }

  const ADBinding *createBinding(const VarDecl *VD, unsigned Version,
                                 const ADExpr *Value) {
    std::unique_ptr<ADBinding> Binding(new ADBinding());
    Binding->SourceDecl = VD;
    Binding->Version = Version;
    Binding->Value = Value;
    const ADBinding *Result = Binding.get();
    Plan.Bindings.push_back(std::move(Binding));
    return Result;
  }

  struct LoopPullbackAnalysis {
    enum class Rejection {
      None,
      ActiveValueOutsideLoopState,
      NonVectorSubscript,
      ActiveSubscriptIndex,
      ActiveCondition,
      UnsupportedCall,
      UnsupportedBinaryOperator,
      UnsupportedExpression,
    };

    Rejection RejectedBy = Rejection::None;
    const ADExpr *Expression = nullptr;

    explicit operator bool() const { return RejectedBy == Rejection::None; }
  };

  static LoopPullbackAnalysis rejectLoopPullback(
      LoopPullbackAnalysis::Rejection Rejection, const ADExpr *Expression) {
    return {Rejection, Expression};
  }

  std::string describeLoopPullbackRejection(
      const LoopPullbackAnalysis &Analysis) const {
    using Rejection = LoopPullbackAnalysis::Rejection;
    switch (Analysis.RejectedBy) {
    case Rejection::None:
      llvm_unreachable("supported loop pullback has no rejection");
    case Rejection::ActiveValueOutsideLoopState:
      return "active runtime loop pullback references a value outside loop "
             "state";
    case Rejection::NonVectorSubscript:
      return "active runtime loop pullback only supports vector subscripts";
    case Rejection::ActiveSubscriptIndex:
      return "active runtime loop pullback requires an inactive subscript "
             "index";
    case Rejection::ActiveCondition:
      return "active runtime loop pullback requires an inactive condition";
    case Rejection::UnsupportedCall: {
      StringRef Name = Analysis.Expression->Callee
                           ? Analysis.Expression->Callee->getName()
                           : StringRef("<indirect>");
      return "active runtime loop pullback does not support call '" +
             Name.str() + "'";
    }
    case Rejection::UnsupportedBinaryOperator:
      return "active runtime loop pullback does not support binary operator '" +
             BinaryOperator::getOpcodeStr(Analysis.Expression->BinaryOpcode)
                 .str() +
             "'";
    case Rejection::UnsupportedExpression:
      return "active runtime loop pullback does not support this active "
             "expression";
    }
    llvm_unreachable("unknown loop pullback rejection");
  }

  struct LoopBodyAnalysis {
    enum class Rejection {
      None,
      ControlFlow,
      SideEffectingStatement,
      UnsupportedStatement,
      UnsupportedUpdate,
      IndirectTarget,
      InactiveTarget,
      RepeatedTarget,
      IncompatibleStateTypes,
    };

    Rejection RejectedBy = Rejection::None;
    const Stmt *Statement = nullptr;
    const ParmVarDecl *Target = nullptr;

    explicit operator bool() const { return RejectedBy == Rejection::None; }
  };

  static LoopBodyAnalysis rejectLoopBody(
      LoopBodyAnalysis::Rejection Rejection, const Stmt *Statement,
      const ParmVarDecl *Target = nullptr) {
    return {Rejection, Statement, Target};
  }

  std::string
  describeLoopBodyRejection(const LoopBodyAnalysis &Analysis) const {
    using Rejection = LoopBodyAnalysis::Rejection;
    switch (Analysis.RejectedBy) {
    case Rejection::None:
      llvm_unreachable("supported loop body has no rejection");
    case Rejection::ControlFlow:
      return "active runtime loop body does not support control flow";
    case Rejection::SideEffectingStatement:
      return "active runtime loop body does not support side-effecting "
             "statements";
    case Rejection::UnsupportedStatement:
      return "active runtime loop body contains an unsupported statement";
    case Rejection::UnsupportedUpdate:
      return "active runtime loop requires =, +=, -=, *=, or /= updates";
    case Rejection::IndirectTarget:
      return "active runtime loop update target must be a direct active "
             "parameter";
    case Rejection::InactiveTarget:
      return "active runtime loop cannot update an inactive parameter";
    case Rejection::RepeatedTarget:
      return "active runtime loop body updates parameter '" +
             Analysis.Target->getName().str() + "' more than once";
    case Rejection::IncompatibleStateTypes:
      return "active runtime loop state values must have the same type";
    }
    llvm_unreachable("unknown loop body rejection");
  }

  LoopBodyAnalysis analyzeActiveRuntimeLoopBody(
      const Stmt *Body,
      SmallVectorImpl<const BinaryOperator *> &Updates,
      SmallVectorImpl<const ParmVarDecl *> &Targets,
      SmallVectorImpl<const DeclRefExpr *> &TargetRefs,
      SmallPtrSetImpl<const ValueDecl *> &SeenTargets) const {
    auto AnalyzeStatement = [&](const Stmt *Child) -> LoopBodyAnalysis {
      if (isa<IfStmt>(Child) || isa<ForStmt>(Child) || isa<WhileStmt>(Child) ||
          isa<DoStmt>(Child) || isa<SwitchStmt>(Child))
        return rejectLoopBody(LoopBodyAnalysis::Rejection::ControlFlow, Child);

      const auto *Update = dyn_cast<BinaryOperator>(Child);
      if (!Update)
        return rejectLoopBody(
            isa<Expr>(Child)
                ? LoopBodyAnalysis::Rejection::SideEffectingStatement
                : LoopBodyAnalysis::Rejection::UnsupportedStatement,
            Child);
      if (Update->getOpcode() != BO_Assign &&
          Update->getOpcode() != BO_AddAssign &&
          Update->getOpcode() != BO_SubAssign &&
          Update->getOpcode() != BO_MulAssign &&
          Update->getOpcode() != BO_DivAssign)
        return rejectLoopBody(LoopBodyAnalysis::Rejection::UnsupportedUpdate,
                              Child);

      const auto *TargetRef =
          dyn_cast<DeclRefExpr>(Update->getLHS()->IgnoreParenImpCasts());
      const auto *Target =
          TargetRef ? dyn_cast<ParmVarDecl>(TargetRef->getDecl()) : nullptr;
      if (!Target)
        return rejectLoopBody(LoopBodyAnalysis::Rejection::IndirectTarget,
                              Child);
      if (Target->hasAttr<HLSLNoDiffAttr>())
        return rejectLoopBody(LoopBodyAnalysis::Rejection::InactiveTarget,
                              Child, Target);
      if (!SeenTargets.insert(getCanonicalValueDecl(Target)).second)
        return rejectLoopBody(LoopBodyAnalysis::Rejection::RepeatedTarget,
                              Child, Target);
      if (!Targets.empty() &&
          !Ctx.hasSameType(Targets.front()->getType(), Target->getType()))
        return rejectLoopBody(
            LoopBodyAnalysis::Rejection::IncompatibleStateTypes, Child,
            Target);

      Updates.push_back(Update);
      Targets.push_back(Target);
      TargetRefs.push_back(TargetRef);
      return {};
    };

    if (const auto *Compound = dyn_cast<CompoundStmt>(Body)) {
      for (const Stmt *Child : Compound->body()) {
        LoopBodyAnalysis Analysis = AnalyzeStatement(Child);
        if (!Analysis)
          return Analysis;
      }
      return {};
    }
    return AnalyzeStatement(Body);
  }

  const ADExpr *createLoopUpdateExpr(
      const ADExpr *Expression,
      const DenseMap<const ValueDecl *, unsigned> &StateIndices,
      ArrayRef<unsigned> Versions) {
    if ((Expression->K == ADExpr::Kind::DeclRef ||
         Expression->K == ADExpr::Kind::LocalRef) &&
        Expression->SourceDecl) {
      auto It =
          StateIndices.find(getCanonicalValueDecl(Expression->SourceDecl));
      if (It != StateIndices.end()) {
        ADExpr *StateRef =
            createExpr(ADExpr::Kind::LoopStateRef, Expression->SourceExpr);
        StateRef->Value = Expression->Value;
        StateRef->SourceDecl = Expression->SourceDecl;
        StateRef->LoopStateIndex = It->second;
        StateRef->LoopStateVersion = Versions[It->second];
        return StateRef;
      }
    }

    bool Changed = false;
    SmallVector<const ADExpr *, 4> Operands;
    for (const ADExpr *Operand : Expression->Operands) {
      const ADExpr *Rewritten =
          createLoopUpdateExpr(Operand, StateIndices, Versions);
      Operands.push_back(Rewritten);
      Changed |= Rewritten != Operand;
    }
    const ADExpr *Receiver = Expression->Receiver;
    if (Receiver) {
      const ADExpr *Rewritten =
          createLoopUpdateExpr(Receiver, StateIndices, Versions);
      Changed |= Rewritten != Receiver;
      Receiver = Rewritten;
    }
    if (!Changed)
      return Expression;

    ADExpr *Rewritten = createExpr(Expression->K, Expression->SourceExpr);
    *Rewritten = *Expression;
    Rewritten->Operands = Operands;
    Rewritten->Receiver = Receiver;
    return Rewritten;
  }

  LoopPullbackAnalysis analyzeGenericLoopPullback(
      const ADExpr *Expression,
      SmallVectorImpl<ADLoopPullbackInput> *Inputs = nullptr,
      const SmallPtrSetImpl<const ValueDecl *> *StateDecls = nullptr,
      bool NeedsPrimal = false) const {
    if (Expression->Value.Activity == ADActivity::Inactive)
      return {};
    ADPullbackRuleInfo Rule = getADPullbackRule(Expression);
    switch (Rule.Rule) {
    case ADPullbackRule::Leaf:
      if (Expression->K == ADExpr::Kind::LoopStateRef) {
        if (!Inputs)
          return {};
        for (ADLoopPullbackInput &Input : *Inputs)
          if (Input.StateIndex == Expression->LoopStateIndex &&
              Input.Version == Expression->LoopStateVersion) {
            Input.NeedsPrimal |= NeedsPrimal;
            return {};
          }
        Inputs->push_back({Expression->LoopStateIndex,
                           Expression->LoopStateVersion, NeedsPrimal});
        return {};
      }
      if (StateDecls && Expression->SourceDecl &&
          StateDecls->count(getCanonicalValueDecl(Expression->SourceDecl)))
        return {};
      return rejectLoopPullback(
          LoopPullbackAnalysis::Rejection::ActiveValueOutsideLoopState,
          Expression);
    case ADPullbackRule::AggregateConstruct:
      for (unsigned I = 0; I < Expression->Operands.size(); ++I) {
        LoopPullbackAnalysis Analysis = analyzeGenericLoopPullback(
            Expression->Operands[I], Inputs, StateDecls,
            NeedsPrimal || (Rule.PrimalOperandMask & (1u << I)));
        if (!Analysis)
          return Analysis;
      }
      return {};
    case ADPullbackRule::Subscript:
      if (!hlsl::IsHLSLVecType(Expression->Operands[0]->Value.PrimalType))
        return rejectLoopPullback(
            LoopPullbackAnalysis::Rejection::NonVectorSubscript, Expression);
      if (Expression->Operands[1]->Value.Activity == ADActivity::Active)
        return rejectLoopPullback(
            LoopPullbackAnalysis::Rejection::ActiveSubscriptIndex, Expression);
      return analyzeGenericLoopPullback(Expression->Operands[0], Inputs,
                                        StateDecls, NeedsPrimal);
    case ADPullbackRule::Conditional: {
      if (Expression->Operands[0]->Value.Activity == ADActivity::Active)
        return rejectLoopPullback(
            LoopPullbackAnalysis::Rejection::ActiveCondition, Expression);
      LoopPullbackAnalysis TrueAnalysis = analyzeGenericLoopPullback(
          Expression->Operands[1], Inputs, StateDecls, NeedsPrimal);
      if (!TrueAnalysis)
        return TrueAnalysis;
      return analyzeGenericLoopPullback(Expression->Operands[2], Inputs,
                                        StateDecls, NeedsPrimal);
    }
    case ADPullbackRule::Swizzle:
    case ADPullbackRule::Cast:
    case ADPullbackRule::Positive:
    case ADPullbackRule::Negative:
      return analyzeGenericLoopPullback(Expression->Operands.front(), Inputs,
                                        StateDecls, NeedsPrimal);
    case ADPullbackRule::Sin:
    case ADPullbackRule::Cos:
    case ADPullbackRule::Exp:
    case ADPullbackRule::Log:
    case ADPullbackRule::Sqrt:
      return analyzeGenericLoopPullback(Expression->Operands.front(), Inputs,
                                        StateDecls, true);
    case ADPullbackRule::ComposedCall:
    case ADPullbackRule::CustomCall:
      for (unsigned I = 0; I < Expression->Operands.size(); ++I) {
        LoopPullbackAnalysis Analysis = analyzeGenericLoopPullback(
            Expression->Operands[I], Inputs, StateDecls,
            NeedsPrimal || (Rule.PrimalOperandMask & (uint64_t(1) << I)));
        if (!Analysis)
          return Analysis;
      }
      return {};
    case ADPullbackRule::Add:
    case ADPullbackRule::Subtract:
    case ADPullbackRule::Multiply:
    case ADPullbackRule::Divide: {
      LoopPullbackAnalysis LeftAnalysis = analyzeGenericLoopPullback(
          Expression->Operands[0], Inputs, StateDecls,
          NeedsPrimal || (Rule.PrimalOperandMask & 1));
      if (!LeftAnalysis)
        return LeftAnalysis;
      return analyzeGenericLoopPullback(
          Expression->Operands[1], Inputs, StateDecls,
          NeedsPrimal || (Rule.PrimalOperandMask & 2));
    }
    case ADPullbackRule::Unsupported:
      if (Expression->K == ADExpr::Kind::Call)
        return rejectLoopPullback(
            LoopPullbackAnalysis::Rejection::UnsupportedCall, Expression);
      if (Expression->K == ADExpr::Kind::Binary)
        return rejectLoopPullback(
            LoopPullbackAnalysis::Rejection::UnsupportedBinaryOperator,
            Expression);
      return rejectLoopPullback(
          LoopPullbackAnalysis::Rejection::UnsupportedExpression, Expression);
    }
    llvm_unreachable("unknown typed pullback rule");
  }

  bool canRecomputeLoopExpression(const ADExpr *Expression) const {
    if (Expression->K == ADExpr::Kind::Call) {
      ADPullbackRule Rule = getADPullbackRule(Expression).Rule;
      if (Rule != ADPullbackRule::Sin && Rule != ADPullbackRule::Cos &&
          Rule != ADPullbackRule::Exp && Rule != ADPullbackRule::Log &&
          Rule != ADPullbackRule::Sqrt)
        return false;
    }
    if (Expression->Receiver &&
        !canRecomputeLoopExpression(Expression->Receiver))
      return false;
    for (const ADExpr *Operand : Expression->Operands)
      if (!canRecomputeLoopExpression(Operand))
        return false;
    return true;
  }

  const ADLoopPlan *createLoopPlan(const ForStmt *FS, const VarDecl *Counter,
                                   const ADExpr *TripCount,
                                   ArrayRef<const ADExpr *> Results,
                                   unsigned TapeCapacity) {
    std::unique_ptr<ADLoopPlan> Loop(new ADLoopPlan());
    Loop->Source = FS;
    Loop->Counter = Counter;
    Loop->TripCount = TripCount;
    DenseMap<const ValueDecl *, unsigned> StateIndices;
    for (unsigned I = 0; I < Results.size(); ++I) {
      const ADExpr *Result = Results[I];
      ADLoopState State;
      State.SourceDecl = cast<VarDecl>(Result->SourceDecl);
      State.PrimalType = Result->Value.PrimalType;
      State.InitialValue = Result->Operands[0];
      State.Result = Result;
      Loop->States.push_back(State);
      StateIndices[getCanonicalValueDecl(State.SourceDecl)] = I;
    }

    SmallVector<unsigned, 4> Versions(Results.size(), 0);
    for (unsigned I = 0; I < Results.size(); ++I) {
      const ADExpr *Result = Results[I];
      ADLoopUpdate Update;
      Update.TargetStateIndex = I;
      Update.InputVersion = Versions[I];
      Update.Opcode = Result->BinaryOpcode;
      Update.Value =
          createLoopUpdateExpr(Result->Operands[2], StateIndices, Versions);
      Update.Pullback.Value = Update.Value;
      if (Update.Opcode != BO_Assign) {
        ADExpr *FullUpdate =
            createExpr(ADExpr::Kind::Binary, Result->SourceExpr);
        FullUpdate->BinaryOpcode = Update.Opcode;
        FullUpdate->Operands.push_back(
            createLoopUpdateExpr(Result->Operands[0], StateIndices, Versions));
        FullUpdate->Operands.push_back(Update.Value);
        FullUpdate->Value = Result->Value;
        Update.Pullback.Value = FullUpdate;
      }
      Update.ResultVersion = ++Versions[I];
      LoopPullbackAnalysis Analysis = analyzeGenericLoopPullback(
          Update.Pullback.Value, &Update.Pullback.Inputs);
      if (!Analysis) {
        fail(describeLoopPullbackRejection(Analysis));
        return nullptr;
      }
      for (const ADLoopPullbackInput &Input : Update.Pullback.Inputs) {
        if (!Input.NeedsPrimal)
          continue;
        if (Input.Version == 0) {
          Loop->States[Input.StateIndex].NeedsPrimalTape = true;
          continue;
        }
        bool HasSlot = false;
        for (const ADLoopTapeSlot &Slot : Loop->TapeSlots)
          HasSlot |= Slot.StateIndex == Input.StateIndex &&
                     Slot.Version == Input.Version;
        if (!HasSlot)
          Loop->TapeSlots.push_back({Input.StateIndex, Input.Version});
      }
      Loop->Updates.push_back(Update);
    }
    for (unsigned I = 0; I < Loop->States.size(); ++I)
      Loop->States[I].FinalVersion = Versions[I];
    bool NeedsTape = !Loop->TapeSlots.empty();
    for (const ADLoopState &State : Loop->States)
      NeedsTape |= State.NeedsPrimalTape;
    if (NeedsTape) {
      if (TapeCapacity >= 1 && TapeCapacity <= 1024) {
        Loop->Storage = ADLoopStorageKind::Static;
        Loop->TapeCapacity = TapeCapacity;
      } else if (llvm::all_of(Loop->Updates, [&](const ADLoopUpdate &Update) {
                   return canRecomputeLoopExpression(Update.Pullback.Value);
                 })) {
        Loop->Storage = ADLoopStorageKind::Recompute;
      } else {
        Loop->Storage = ADLoopStorageKind::DynamicRequired;
      }
    }
    const ADLoopPlan *Result = Loop.get();
    Plan.Loops.push_back(std::move(Loop));
    return Result;
  }

  const ADExpr *createLocalRef(const Expr *Source, const ADBinding *Binding,
                               bool ForceInactive) {
    ADExpr *Node = createExpr(ADExpr::Kind::LocalRef, Source);
    Node->Binding = Binding;
    Node->SourceDecl = getCanonicalValueDecl(Binding->SourceDecl);
    Node->Value.SourceDecl = Node->SourceDecl;
    Node->Value.Activity =
        ForceInactive ? ADActivity::Inactive : Binding->Value->Value.Activity;
    return Node;
  }

  const ADExpr *createPrimalLocal(const Expr *Source, const VarDecl *VD) {
    ADExpr *Node = createExpr(ADExpr::Kind::PrimalLocal, Source);
    Node->SourceDecl = getCanonicalValueDecl(VD);
    Node->Value.SourceDecl = Node->SourceDecl;
    Node->Value.Activity = ADActivity::Inactive;
    return Node;
  }

  const ADExpr *buildExpr(const Expr *Input, bool ForceInactive = false) {
    if (!Input) {
      fail("null expression");
      return nullptr;
    }

    const Expr *Original = Input;
    if (Ctx.isHLSLNoDiffExpr(Original) ||
        Ctx.isHLSLNoDiffExpr(Original->IgnoreParenImpCasts()))
      ForceInactive = true;

    const Expr *E = Original->IgnoreParenImpCasts();
    if (isa<FloatingLiteral>(E) || isa<IntegerLiteral>(E)) {
      ADExpr *Node = createExpr(ADExpr::Kind::Literal, E);
      Node->Value.Activity = ADActivity::Inactive;
      return Node;
    }

    if (const auto *DRE = dyn_cast<DeclRefExpr>(E)) {
      const ValueDecl *D = getCanonicalValueDecl(DRE->getDecl());
      auto It = CurrentBindings.find(D);
      if (It != CurrentBindings.end()) {
        if (!It->second->Value) {
          if (!ForceInactive) {
            fail("read of uninitialized local in typed auto-diff IR");
            return nullptr;
          }
          return createPrimalLocal(E, cast<VarDecl>(DRE->getDecl()));
        }
        return createLocalRef(E, It->second, ForceInactive);
      }

      ADExpr *Node = createExpr(ADExpr::Kind::DeclRef, E);
      Node->SourceDecl = D;
      Node->Value.SourceDecl = D;
      const auto *Parameter = dyn_cast<ParmVarDecl>(D);
      Node->Value.Activity =
          !ForceInactive && Parameter && !isADInactiveParameter(Parameter)
              ? ADActivity::Active
              : ADActivity::Inactive;
      return Node;
    }

    if (isa<CXXThisExpr>(E)) {
      ADExpr *Node = createExpr(ADExpr::Kind::This, E);
      Node->Value.Activity = ADActivity::Inactive;
      return Node;
    }

    if (const auto *VE = dyn_cast<HLSLVectorElementExpr>(E)) {
      ADExpr *Node = createExpr(ADExpr::Kind::Swizzle, E);
      const ADExpr *Base = buildExpr(VE->getBase(), ForceInactive);
      if (!Base)
        return nullptr;
      Node->Operands.push_back(Base);
      VE->getEncodedElementAccess(Node->Components);
      Node->Value.Activity =
          ForceInactive ? ADActivity::Inactive : Base->Value.Activity;
      return Node;
    }

    if (const auto *ASE = dyn_cast<ArraySubscriptExpr>(E)) {
      ADExpr *Node = createExpr(ADExpr::Kind::Subscript, E);
      const ADExpr *Base = buildExpr(ASE->getBase(), ForceInactive);
      const ADExpr *Index = buildExpr(ASE->getIdx(), ForceInactive);
      if (!Base || !Index)
        return nullptr;
      if (Index->Value.Activity == ADActivity::Active) {
        fail("active subscript index in typed auto-diff IR");
        return nullptr;
      }
      Node->Operands.push_back(Base);
      Node->Operands.push_back(Index);
      Node->Value.Activity =
          ForceInactive ? ADActivity::Inactive : Base->Value.Activity;
      return Node;
    }

    if (const auto *OCE = dyn_cast<CXXOperatorCallExpr>(E)) {
      if (OCE->getOperator() != OO_Subscript || OCE->getNumArgs() != 2) {
        fail("unsupported operator call in typed auto-diff IR");
        return nullptr;
      }
      ADExpr *Node = createExpr(ADExpr::Kind::Subscript, E);
      const ADExpr *Base = buildExpr(OCE->getArg(0), ForceInactive);
      const ADExpr *Index = buildExpr(OCE->getArg(1), ForceInactive);
      if (!Base || !Index)
        return nullptr;
      if (Index->Value.Activity == ADActivity::Active) {
        fail("active subscript index in typed auto-diff IR");
        return nullptr;
      }
      Node->Operands.push_back(Base);
      Node->Operands.push_back(Index);
      Node->Value.Activity =
          ForceInactive ? ADActivity::Inactive : Base->Value.Activity;
      return Node;
    }

    if (const auto *ME = dyn_cast<MemberExpr>(E)) {
      ADExpr *Node = createExpr(ADExpr::Kind::Member, E);
      Node->SourceDecl = getCanonicalValueDecl(ME->getMemberDecl());
      Node->Value.SourceDecl = Node->SourceDecl;
      const ADExpr *Base = buildExpr(ME->getBase(), ForceInactive);
      if (!Base)
        return nullptr;
      Node->Operands.push_back(Base);
      // Class state is currently treated as a primal constant. Parameter and
      // local activity still flows through explicit expression operands.
      Node->Value.Activity = ADActivity::Inactive;
      return Node;
    }

    if (const auto *ILE = dyn_cast<InitListExpr>(E)) {
      ADExpr *Node = createExpr(ADExpr::Kind::AggregateConstruct, E);
      for (unsigned I = 0; I < ILE->getNumInits(); ++I) {
        const ADExpr *Operand = buildExpr(ILE->getInit(I), ForceInactive);
        if (!Operand)
          return nullptr;
        Node->Operands.push_back(Operand);
      }
      Node->Value.Activity = ForceInactive ? ADActivity::Inactive
                                           : combineActivity(Node->Operands);
      return Node;
    }

    if (const auto *FCE = dyn_cast<CXXFunctionalCastExpr>(E)) {
      const auto *ILE = dyn_cast<InitListExpr>(FCE->getSubExpr());
      if (!ILE) {
        fail("functional cast without initializer list in typed auto-diff IR");
        return nullptr;
      }
      ADExpr *Node = createExpr(ADExpr::Kind::AggregateConstruct, E);
      for (unsigned I = 0; I < ILE->getNumInits(); ++I) {
        const ADExpr *Operand = buildExpr(ILE->getInit(I), ForceInactive);
        if (!Operand)
          return nullptr;
        Node->Operands.push_back(Operand);
      }
      Node->Value.Activity = ForceInactive ? ADActivity::Inactive
                                           : combineActivity(Node->Operands);
      return Node;
    }

    if (const auto *CE = dyn_cast<CastExpr>(E)) {
      ADExpr *Node = createExpr(ADExpr::Kind::Cast, E);
      const ADExpr *Operand = buildExpr(CE->getSubExpr(), ForceInactive);
      if (!Operand)
        return nullptr;
      Node->Operands.push_back(Operand);
      Node->Value.Activity =
          ForceInactive ? ADActivity::Inactive : Operand->Value.Activity;
      if (Node->Value.Activity == ADActivity::Active &&
          !isDifferentiableFloatingCast(Operand->Value.PrimalType,
                                        Node->Value.PrimalType)) {
        fail("active cast not represented in typed auto-diff IR");
        return nullptr;
      }
      return Node;
    }

    if (const auto *UO = dyn_cast<UnaryOperator>(E)) {
      if (UO->getOpcode() != UO_Plus && UO->getOpcode() != UO_Minus) {
        fail("unsupported unary operator in typed auto-diff IR");
        return nullptr;
      }
      ADExpr *Node = createExpr(ADExpr::Kind::Unary, E);
      Node->UnaryOpcode = UO->getOpcode();
      const ADExpr *Operand = buildExpr(UO->getSubExpr(), ForceInactive);
      if (!Operand)
        return nullptr;
      Node->Operands.push_back(Operand);
      Node->Value.Activity =
          ForceInactive ? ADActivity::Inactive : Operand->Value.Activity;
      return Node;
    }

    if (const auto *CO = dyn_cast<ConditionalOperator>(E)) {
      if (!ForceInactive && referencesActiveValue(CO->getCond())) {
        fail("the ternary ?: operator is not differentiable");
        return nullptr;
      }
      ADExpr *Node = createExpr(ADExpr::Kind::Conditional, E);
      const ADExpr *Condition = buildExpr(CO->getCond(), ForceInactive);
      const ADExpr *TrueValue = buildExpr(CO->getTrueExpr(), ForceInactive);
      const ADExpr *FalseValue = buildExpr(CO->getFalseExpr(), ForceInactive);
      if (!Condition || !TrueValue || !FalseValue)
        return nullptr;
      Node->Operands.push_back(Condition);
      Node->Operands.push_back(TrueValue);
      Node->Operands.push_back(FalseValue);
      Node->Value.Activity =
          ForceInactive
              ? ADActivity::Inactive
              : combineActivity(
                    ArrayRef<const ADExpr *>(Node->Operands).slice(1));
      return Node;
    }

    if (const auto *BO = dyn_cast<BinaryOperator>(E)) {
      bool IsComparison = BO->isComparisonOp();
      bool RequiresInactiveOperands = false;
      switch (BO->getOpcode()) {
      case BO_Add:
      case BO_Sub:
      case BO_Mul:
      case BO_Div:
      case BO_LT:
      case BO_GT:
      case BO_LE:
      case BO_GE:
      case BO_EQ:
      case BO_NE:
        break;
      case BO_Rem:
      case BO_Shl:
      case BO_Shr:
      case BO_And:
      case BO_Xor:
      case BO_Or:
      case BO_LAnd:
      case BO_LOr:
        RequiresInactiveOperands = true;
        break;
      default:
        fail("unsupported binary operator in typed auto-diff IR");
        return nullptr;
      }
      ADExpr *Node = createExpr(ADExpr::Kind::Binary, E);
      Node->BinaryOpcode = BO->getOpcode();
      const ADExpr *Left = buildExpr(BO->getLHS(), ForceInactive);
      const ADExpr *Right = buildExpr(BO->getRHS(), ForceInactive);
      if (!Left || !Right)
        return nullptr;
      Node->Operands.push_back(Left);
      Node->Operands.push_back(Right);
      Node->Value.Activity = ForceInactive ? ADActivity::Inactive
                                           : combineActivity(Node->Operands);
      if (IsComparison && Node->Value.Activity == ADActivity::Active) {
        fail("active comparison in typed auto-diff IR");
        return nullptr;
      }
      if (RequiresInactiveOperands &&
          Node->Value.Activity == ADActivity::Active) {
        fail("active discrete operator in typed auto-diff IR");
        return nullptr;
      }
      return Node;
    }

    if (const auto *CE = dyn_cast<CallExpr>(E)) {
      const FunctionDecl *Callee = CE->getDirectCallee();
      if (!Callee) {
        fail("indirect call in typed auto-diff IR");
        return nullptr;
      }
      ADExpr *Node = createExpr(ADExpr::Kind::Call, E);
      Callee = Callee->getCanonicalDecl();
      Node->Callee = getADPrimalSubstitute(Callee);
      if (!Node->Callee)
        Node->Callee = Callee;
      if (const auto *MemberCall = dyn_cast<CXXMemberCallExpr>(CE)) {
        Node->Receiver =
            buildExpr(MemberCall->getImplicitObjectArgument(), ForceInactive);
        if (!Node->Receiver)
          return nullptr;
      }
      for (const Expr *Argument : CE->arguments()) {
        const ADExpr *Operand = buildExpr(Argument, ForceInactive);
        if (!Operand)
          return nullptr;
        Node->Operands.push_back(Operand);
      }
      Node->Value.Activity = ForceInactive ? ADActivity::Inactive
                                           : combineActivity(Node->Operands);
      if (!ForceInactive && Node->Receiver &&
          Node->Receiver->Value.Activity == ADActivity::Active)
        Node->Value.Activity = ADActivity::Active;
      if (!ForceInactive && getADBackwardDerivative(Node->Callee))
        Node->Value.Activity = ADActivity::Active;
      if (!ForceInactive)
        for (const FunctionDecl *Redecl : Node->Callee->redecls())
          if (const auto *Attr = Redecl->getAttr<HLSLAutoDiffAttr>())
            if (Attr->hasBackward()) {
              Node->Value.Activity = ADActivity::Active;
              break;
            }
      return Node;
    }

    fail("unsupported expression in typed auto-diff IR");
    return nullptr;
  }

  bool referencesDecl(const Stmt *S, const ValueDecl *Decl) const {
    if (!S)
      return false;
    if (const auto *DRE = dyn_cast<DeclRefExpr>(S))
      if (getCanonicalValueDecl(DRE->getDecl()) == Decl)
        return true;
    for (const Stmt *Child : S->children())
      if (referencesDecl(Child, Decl))
        return true;
    return false;
  }

  bool containsDeclaration(const Stmt *S) const {
    if (!S)
      return false;
    if (isa<DeclStmt>(S))
      return true;
    for (const Stmt *Child : S->children())
      if (containsDeclaration(Child))
        return true;
    return false;
  }

  bool referencesActiveValue(const Stmt *S) const {
    if (!S)
      return false;
    if (const auto *DRE = dyn_cast<DeclRefExpr>(S)) {
      const ValueDecl *Decl = getCanonicalValueDecl(DRE->getDecl());
      if (const auto *Parameter = dyn_cast<ParmVarDecl>(Decl))
        return !isADInactiveParameter(Parameter);
      auto It = CurrentBindings.find(Decl);
      return It != CurrentBindings.end() && It->second->Value &&
             It->second->Value->Value.Activity == ADActivity::Active;
    }
    for (const Stmt *Child : S->children())
      if (referencesActiveValue(Child))
        return true;
    return false;
  }

  bool collectInactiveLoopAssignments(
      const Stmt *S, const VarDecl *Counter,
      SmallVectorImpl<std::pair<const ParmVarDecl *, const Expr *>> &Outputs) {
    if (!S)
      return true;
    if (const auto *BO = dyn_cast<BinaryOperator>(S)) {
      if (BO->isAssignmentOp()) {
        const auto *LHS =
            dyn_cast<DeclRefExpr>(BO->getLHS()->IgnoreParenImpCasts());
        if (!LHS)
          return false;
        const auto *Parameter = dyn_cast<ParmVarDecl>(LHS->getDecl());
        if (!Parameter || !isADInactiveParameter(Parameter))
          return false;
        Outputs.push_back({Parameter, LHS});
      }
    }
    if (const auto *UO = dyn_cast<UnaryOperator>(S)) {
      if (UO->isIncrementDecrementOp()) {
        const auto *Target =
            dyn_cast<DeclRefExpr>(UO->getSubExpr()->IgnoreParenImpCasts());
        if (!Target || Target->getDecl() != Counter)
          return false;
      }
    }
    for (const Stmt *Child : S->children())
      if (!collectInactiveLoopAssignments(Child, Counter, Outputs))
        return false;
    return true;
  }

  bool buildInactiveRuntimeFor(const ForStmt *FS) {
    const auto *Init = dyn_cast_or_null<DeclStmt>(FS->getInit());
    const auto *Counter = Init && Init->isSingleDecl()
                              ? dyn_cast<VarDecl>(Init->getSingleDecl())
                              : nullptr;
    if (!Counter || referencesActiveValue(FS->getCond()) ||
        referencesActiveValue(FS->getBody()))
      return fail("data-dependent control flow (for) is not differentiable");

    SmallVector<std::pair<const ParmVarDecl *, const Expr *>, 4> Outputs;
    if (!collectInactiveLoopAssignments(FS, Counter, Outputs))
      return fail("inactive runtime loop may only update inactive parameters");
    Plan.Statements.push_back({ADStmt::Kind::PrimalLoop, nullptr, nullptr, FS});
    for (const auto &Output : Outputs) {
      const ParmVarDecl *Parameter = Output.first;
      const ValueDecl *Target = getCanonicalValueDecl(Parameter);
      auto It = CurrentBindings.find(Target);
      unsigned Version =
          It == CurrentBindings.end() ? 0 : It->second->Version + 1;
      const ADExpr *Value = createPrimalLocal(Output.second, Parameter);
      CurrentBindings[Target] = createBinding(Parameter, Version, Value);
    }
    return true;
  }

  bool buildGenericVersionedRuntimeFor(const ForStmt *FS,
                                       const VarDecl *Counter,
                                       const ADExpr *Count,
                                       const Stmt *Body) {
    SmallVector<const BinaryOperator *, 4> Updates;
    SmallVector<const ParmVarDecl *, 4> Targets;
    SmallVector<const DeclRefExpr *, 4> TargetRefs;
    SmallPtrSet<const ValueDecl *, 4> SeenTargets;
    LoopBodyAnalysis BodyAnalysis = analyzeActiveRuntimeLoopBody(
        Body, Updates, Targets, TargetRefs, SeenTargets);
    if (!BodyAnalysis)
      return fail(describeLoopBodyRejection(BodyAnalysis));

    unsigned TapeSize = 0;
    const auto *Condition = dyn_cast<BinaryOperator>(FS->getCond());
    const auto *BoundCall =
        Condition
            ? dyn_cast<CallExpr>(Condition->getRHS()->IgnoreParenImpCasts())
            : nullptr;
    if (BoundCall && BoundCall->getDirectCallee() &&
        BoundCall->getDirectCallee()->getName() == "min")
      for (const Expr *Argument : BoundCall->arguments())
        if (const auto *Limit =
                dyn_cast<IntegerLiteral>(Argument->IgnoreParenImpCasts()))
          TapeSize = Limit->getValue().getLimitedValue(1025);

    SmallVector<const ADBinding *, 4> Before;
    for (unsigned I = 0; I < Targets.size(); ++I) {
      const ValueDecl *CanonicalTarget = getCanonicalValueDecl(Targets[I]);
      auto BindingIt = CurrentBindings.find(CanonicalTarget);
      if (BindingIt != CurrentBindings.end()) {
        if (!BindingIt->second->Value)
          return fail("generic runtime loop reads an uninitialized value");
        Before.push_back(BindingIt->second);
        continue;
      }
      ADExpr *Initial = createExpr(ADExpr::Kind::DeclRef, TargetRefs[I]);
      Initial->SourceDecl = CanonicalTarget;
      Initial->Value.SourceDecl = CanonicalTarget;
      Initial->Value.Activity = ADActivity::Active;
      Before.push_back(createBinding(Targets[I], 0, Initial));
    }

    SmallVector<const ADExpr *, 4> Factors;
    for (const BinaryOperator *Update : Updates) {
      const ADExpr *Factor = buildExpr(Update->getRHS());
      if (!Factor)
        return false;
      Factors.push_back(Factor);
    }

    SmallVector<const ADExpr *, 4> Results;
    for (unsigned I = 0; I < Targets.size(); ++I) {
      ADExpr *Result = createExpr(ADExpr::Kind::RuntimeLoopResult, Updates[I]);
      Result->SourceDecl = getCanonicalValueDecl(Targets[I]);
      switch (Updates[I]->getOpcode()) {
      case BO_Assign:
        Result->BinaryOpcode = BO_Assign;
        break;
      case BO_AddAssign:
        Result->BinaryOpcode = BO_Add;
        break;
      case BO_SubAssign:
        Result->BinaryOpcode = BO_Sub;
        break;
      case BO_MulAssign:
        Result->BinaryOpcode = BO_Mul;
        break;
      case BO_DivAssign:
        Result->BinaryOpcode = BO_Div;
        break;
      default:
        llvm_unreachable("validated generic runtime update");
      }
      Result->Operands.push_back(
          createLocalRef(TargetRefs[I], Before[I], false));
      Result->Operands.push_back(Count);
      Result->Operands.push_back(Factors[I]);
      Result->Value.Activity = ADActivity::Active;
      Result->Value.PrimalType = Targets[I]->getType();
      Results.push_back(Result);
    }
    for (unsigned I = 0; I < Results.size(); ++I) {
      const ADBinding *After =
          createBinding(Targets[I], Before[I]->Version + 1, Results[I]);
      CurrentBindings[getCanonicalValueDecl(Targets[I])] = After;
    }
    ADStmt Statement(ADStmt::Kind::ActiveLoop, nullptr, Results.front(), FS);
    Statement.Values = Results;
    Statement.Loop =
      createLoopPlan(FS, Counter, Count, Statement.Values, TapeSize);
    if (!Statement.Loop)
      return false;
    if (Statement.Loop->Storage == ADLoopStorageKind::DynamicRequired)
      return fail("active runtime loop pullback requires a min(count, N) "
                  "bound with N between 1 and 1024");
    Plan.Statements.push_back(std::move(Statement));
    return true;
  }

  bool buildActiveRuntimeFor(const ForStmt *FS) {
    const auto *Init = dyn_cast_or_null<DeclStmt>(FS->getInit());
    const auto *Counter = Init && Init->isSingleDecl()
                              ? dyn_cast<VarDecl>(Init->getSingleDecl())
                              : nullptr;
    const auto *Initial = Counter && Counter->getInit()
                              ? dyn_cast<IntegerLiteral>(
                                    Counter->getInit()->IgnoreParenImpCasts())
                              : nullptr;
    const auto *Condition = dyn_cast_or_null<BinaryOperator>(FS->getCond());
    const auto *ConditionCounter =
        Condition
            ? dyn_cast<DeclRefExpr>(Condition->getLHS()->IgnoreParenImpCasts())
            : nullptr;
    const auto *Increment = dyn_cast_or_null<UnaryOperator>(FS->getInc());
    const auto *IncrementCounter =
        Increment ? dyn_cast<DeclRefExpr>(
                        Increment->getSubExpr()->IgnoreParenImpCasts())
                  : nullptr;
    if (!Counter || !Initial || Initial->getValue() != 0 || !Condition ||
        Condition->getOpcode() != BO_LT || !ConditionCounter ||
        ConditionCounter->getDecl() != Counter || !Increment ||
        (Increment->getOpcode() != UO_PreInc &&
         Increment->getOpcode() != UO_PostInc) ||
        !IncrementCounter || IncrementCounter->getDecl() != Counter)
      return fail("active runtime loop is not canonical");

    const ADExpr *Count = buildExpr(Condition->getRHS());
    if (!Count)
      return false;
    if (Count->Value.Activity == ADActivity::Active)
      return fail("active runtime loop count must be inactive");
    return buildGenericVersionedRuntimeFor(FS, Counter, Count, FS->getBody());
  }

  bool buildStaticFor(const ForStmt *FS, bool ForceInactive) {
    const auto *Init = dyn_cast_or_null<DeclStmt>(FS->getInit());
    if (!Init || !Init->isSingleDecl())
      return fail("for loop requires one induction variable");
    const auto *Counter = dyn_cast<VarDecl>(Init->getSingleDecl());
    const auto *Initial = Counter && Counter->getInit()
                              ? dyn_cast<IntegerLiteral>(
                                    Counter->getInit()->IgnoreParenImpCasts())
                              : nullptr;
    const auto *Condition = dyn_cast_or_null<BinaryOperator>(FS->getCond());
    const auto *ConditionCounter =
        Condition
            ? dyn_cast<DeclRefExpr>(Condition->getLHS()->IgnoreParenImpCasts())
            : nullptr;
    const auto *Bound = Condition
                            ? dyn_cast<IntegerLiteral>(
                                  Condition->getRHS()->IgnoreParenImpCasts())
                            : nullptr;
    const auto *Increment = dyn_cast_or_null<UnaryOperator>(FS->getInc());
    const auto *IncrementCounter =
        Increment ? dyn_cast<DeclRefExpr>(
                        Increment->getSubExpr()->IgnoreParenImpCasts())
                  : nullptr;
    const ValueDecl *CanonicalCounter = getCanonicalValueDecl(Counter);
    if (!Initial || !Condition || Condition->getOpcode() != BO_LT || !Bound ||
        !Increment ||
        (Increment->getOpcode() != UO_PreInc &&
         Increment->getOpcode() != UO_PostInc) ||
        !ConditionCounter || !IncrementCounter ||
        getCanonicalValueDecl(ConditionCounter->getDecl()) !=
            CanonicalCounter ||
        getCanonicalValueDecl(IncrementCounter->getDecl()) != CanonicalCounter)
      return fail("for loop is not a canonical constant-bound loop");

    uint64_t InitialValue = Initial->getValue().getLimitedValue(65);
    uint64_t BoundValue = Bound->getValue().getLimitedValue(65);
    if (InitialValue > BoundValue || BoundValue - InitialValue > 64)
      return fail("for loop exceeds the static unroll limit");
    if (referencesDecl(FS->getBody(), CanonicalCounter))
      return fail("statically unrolled loop body references its induction "
                  "variable");
    if (containsDeclaration(FS->getBody()))
      return fail("statically unrolled loop body contains a declaration");

    for (uint64_t Iteration = InitialValue; Iteration < BoundValue; ++Iteration)
      if (!buildStmt(FS->getBody(), ForceInactive))
        return false;
    return true;
  }

  bool buildStmt(const Stmt *S, bool ForceInactive = false) {
    if (const auto *AS = dyn_cast<AttributedStmt>(S)) {
      bool IsNoDiff = false;
      for (const Attr *A : AS->getAttrs())
        IsNoDiff |= isa<HLSLNoDiffAttr>(A);
      return buildStmt(AS->getSubStmt(), ForceInactive || IsNoDiff);
    }

    if (const auto *DS = dyn_cast<DeclStmt>(S)) {
      for (const Decl *D : DS->decls()) {
        const auto *VD = dyn_cast<VarDecl>(D);
        if (!VD)
          return fail("unsupported declaration in typed auto-diff IR");
        const ADExpr *Value = nullptr;
        if (VD->hasInit()) {
          Value = buildExpr(VD->getInit(), ForceInactive);
          if (!Value)
            return false;
        }
        const ADBinding *Binding = createBinding(VD, 0, Value);
        CurrentBindings[getCanonicalValueDecl(VD)] = Binding;
        Plan.Statements.push_back({ADStmt::Kind::Declare, Binding, Value});
      }
      return true;
    }

    if (const auto *RS = dyn_cast<ReturnStmt>(S)) {
      const ADExpr *Value = buildExpr(RS->getRetValue(), ForceInactive);
      if (!Value)
        return false;
      Plan.Statements.push_back({ADStmt::Kind::Return, nullptr, Value});
      return true;
    }

    if (const auto *FS = dyn_cast<ForStmt>(S)) {
      const auto *Condition = dyn_cast_or_null<BinaryOperator>(FS->getCond());
      const bool HasConstantBound =
          Condition &&
          isa<IntegerLiteral>(Condition->getRHS()->IgnoreParenImpCasts());
      if (HasConstantBound)
        return buildStaticFor(FS, ForceInactive);
      return referencesActiveValue(FS->getBody()) ? buildActiveRuntimeFor(FS)
                                                  : buildInactiveRuntimeFor(FS);
    }

    if (const auto *IS = dyn_cast<IfStmt>(S)) {
      if (IS->getConditionVariable())
        return fail(
            "if condition variable not represented in typed auto-diff IR");
      const ADExpr *Condition = buildExpr(IS->getCond(), ForceInactive);
      if (!Condition) {
        if (Reason == "active comparison in typed auto-diff IR")
          Reason = "data-dependent control flow (if) is not differentiable; "
                   "use [[dxc::NoDiff]] or branchless math";
        return false;
      }
      if (Condition->Value.Activity == ADActivity::Active)
        return fail("active if condition in typed auto-diff IR");

      DenseMap<const ValueDecl *, const ADBinding *> Before = CurrentBindings;
      size_t StatementCount = Plan.Statements.size();

      CurrentBindings = Before;
      if (!buildStmt(IS->getThen(), ForceInactive))
        return false;
      DenseMap<const ValueDecl *, const ADBinding *> Then = CurrentBindings;
      Plan.Statements.resize(StatementCount);

      CurrentBindings = Before;
      if (IS->getElse() && !buildStmt(IS->getElse(), ForceInactive))
        return false;
      DenseMap<const ValueDecl *, const ADBinding *> Else = CurrentBindings;
      Plan.Statements.resize(StatementCount);
      CurrentBindings = Before;

      for (const auto &Entry : Before) {
        const ValueDecl *Decl = Entry.first;
        const ADBinding *OldBinding = Entry.second;
        const ADBinding *ThenBinding = Then.lookup(Decl);
        const ADBinding *ElseBinding = Else.lookup(Decl);
        ThenBinding = ThenBinding ? ThenBinding : OldBinding;
        ElseBinding = ElseBinding ? ElseBinding : OldBinding;
        if (ThenBinding == OldBinding && ElseBinding == OldBinding)
          continue;
        if (!ThenBinding->Value || !ElseBinding->Value)
          return fail("inactive if leaves a local uninitialized");

        ADExpr *Merged = createExpr(ADExpr::Kind::Conditional, IS->getCond());
        Merged->Value.PrimalType = OldBinding->Value->Value.PrimalType;
        Merged->Operands.push_back(Condition);
        Merged->Operands.push_back(ThenBinding->Value);
        Merged->Operands.push_back(ElseBinding->Value);
        Merged->Value.Activity = combineActivity(
            ArrayRef<const ADExpr *>(Merged->Operands).slice(1));

        unsigned Version =
            std::max(ThenBinding->Version, ElseBinding->Version) + 1;
        const ADBinding *Binding =
            createBinding(cast<VarDecl>(Decl), Version, Merged);
        CurrentBindings[Decl] = Binding;
        Plan.Statements.push_back({ADStmt::Kind::Assign, Binding, Merged});
      }
      return true;
    }

    if (const auto *CS = dyn_cast<CompoundStmt>(S)) {
      for (const Stmt *Child : CS->body())
        if (!buildStmt(Child, ForceInactive))
          return false;
      return true;
    }

    if (const auto *Expression = dyn_cast<Expr>(S);
        Expression && !(isa<BinaryOperator>(Expression) &&
                        cast<BinaryOperator>(Expression)->isAssignmentOp())) {
      if (!ForceInactive)
        return fail("active expression statement in typed auto-diff IR");
      const ADExpr *Value = buildExpr(Expression, /*ForceInactive=*/true);
      if (!Value)
        return false;
      const auto *Call = dyn_cast<CallExpr>(Expression->IgnoreParenImpCasts());
      const FunctionDecl *Callee = Call ? Call->getDirectCallee() : nullptr;
      if (Call && Callee) {
        for (unsigned I = 0;
             I < Call->getNumArgs() && I < Callee->getNumParams(); ++I) {
          const auto *Output =
              dyn_cast<DeclRefExpr>(Call->getArg(I)->IgnoreParenImpCasts());
          if (!Output)
            continue;
          const auto *VD = dyn_cast<VarDecl>(Output->getDecl());
          if (!VD)
            continue;
          const ValueDecl *Target = getCanonicalValueDecl(VD);
          auto It = CurrentBindings.find(Target);
          bool IsExplicitOutput =
              Callee->getParamDecl(I)->hasAttr<HLSLOutAttr>() ||
              Callee->getParamDecl(I)->hasAttr<HLSLInOutAttr>();
          bool IsUninitializedLocal =
              It != CurrentBindings.end() && !It->second->Value;
          if (!IsExplicitOutput && !IsUninitializedLocal)
            continue;
          if (It == CurrentBindings.end())
            return fail("inactive out argument has no local binding");
          const ADExpr *OutputValue = createPrimalLocal(Output, VD);
          const ADBinding *Binding =
              createBinding(VD, It->second->Version + 1, OutputValue);
          CurrentBindings[Target] = Binding;
          if (std::find(Plan.PrimalLocals.begin(), Plan.PrimalLocals.end(),
                        VD) == Plan.PrimalLocals.end())
            Plan.PrimalLocals.push_back(VD);
        }
      }
      Plan.Statements.push_back({ADStmt::Kind::Expression, nullptr, Value});
      return true;
    }

    const auto *BO = dyn_cast<BinaryOperator>(S);
    if (!BO || !BO->isAssignmentOp())
      return fail("unsupported statement in typed auto-diff IR");

    const auto *LHS =
        dyn_cast<DeclRefExpr>(BO->getLHS()->IgnoreParenImpCasts());
    if (!LHS)
      return fail("assignment target is not a local variable");
    const ValueDecl *Target = getCanonicalValueDecl(LHS->getDecl());
    auto It = CurrentBindings.find(Target);
    if (It == CurrentBindings.end()) {
      const auto *Parameter = dyn_cast<ParmVarDecl>(LHS->getDecl());
      if (!Parameter)
        return fail("assignment target has no local binding");
      ADExpr *InitialValue = createExpr(ADExpr::Kind::DeclRef, LHS);
      InitialValue->SourceDecl = Target;
      InitialValue->Value.SourceDecl = Target;
      InitialValue->Value.Activity =
          !ForceInactive && !isADInactiveParameter(Parameter)
              ? ADActivity::Active
              : ADActivity::Inactive;
      CurrentBindings[Target] = createBinding(Parameter, 0, InitialValue);
      It = CurrentBindings.find(Target);
    }

    const ADExpr *Value = buildExpr(BO->getRHS(), ForceInactive);
    if (!Value)
      return false;

    BinaryOperatorKind ValueOpcode = BO_Add;
    bool IsCompound = true;
    bool RequiresInactiveOperands = false;
    switch (BO->getOpcode()) {
    case BO_Assign:
      IsCompound = false;
      break;
    case BO_AddAssign:
      ValueOpcode = BO_Add;
      break;
    case BO_SubAssign:
      ValueOpcode = BO_Sub;
      break;
    case BO_MulAssign:
      ValueOpcode = BO_Mul;
      break;
    case BO_DivAssign:
      ValueOpcode = BO_Div;
      break;
    case BO_RemAssign:
      ValueOpcode = BO_Rem;
      RequiresInactiveOperands = true;
      break;
    case BO_ShlAssign:
      ValueOpcode = BO_Shl;
      RequiresInactiveOperands = true;
      break;
    case BO_ShrAssign:
      ValueOpcode = BO_Shr;
      RequiresInactiveOperands = true;
      break;
    case BO_AndAssign:
      ValueOpcode = BO_And;
      RequiresInactiveOperands = true;
      break;
    case BO_XorAssign:
      ValueOpcode = BO_Xor;
      RequiresInactiveOperands = true;
      break;
    case BO_OrAssign:
      ValueOpcode = BO_Or;
      RequiresInactiveOperands = true;
      break;
    default:
      return fail("unsupported assignment in typed auto-diff IR");
    }

    if (IsCompound) {
      if (!It->second->Value)
        return fail("compound assignment reads an uninitialized local");
      ADExpr *Combined = createExpr(ADExpr::Kind::Binary, BO);
      Combined->BinaryOpcode = ValueOpcode;
      Combined->Operands.push_back(createLocalRef(BO->getLHS(), It->second,
                                                  /*ForceInactive=*/false));
      Combined->Operands.push_back(Value);
      Combined->Value.Activity = combineActivity(Combined->Operands);
      if (RequiresInactiveOperands &&
          Combined->Value.Activity == ADActivity::Active)
        return fail("active discrete assignment in typed auto-diff IR");
      Value = Combined;
    }

    unsigned NewVersion = It->second->Version + 1;
    assert(NewVersion > It->second->Version && "AD binding version overflow");
    const ADBinding *Binding =
        createBinding(cast<VarDecl>(LHS->getDecl()), NewVersion, Value);
    CurrentBindings[Target] = Binding;
    Plan.Statements.push_back({ADStmt::Kind::Assign, Binding, Value});
    return true;
  }
};

} // namespace

bool BuildADFunctionPlan(const FunctionDecl *FD, ADFunctionPlan &Plan,
                         std::string &Reason) {
  Reason.clear();
  ADFunctionBuilder Builder(FD, Plan, Reason);
  return Builder.build();
}

} // namespace autodiff
} // namespace hlsl
