//===--- HLSLOutParamBuilder.h - Helper for HLSLOutParamExpr ----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//
//===----------------------------------------------------------------------===//

#ifndef CLANG_SEMA_HLSLOUTPARAMBUILDER_H
#define CLANG_SEMA_HLSLOUTPARAMBUILDER_H

#include "clang/AST/ASTContext.h"
#include "clang/AST/Expr.h"
#include "clang/AST/StmtVisitor.h"
#include "clang/Sema/Sema.h"
#include "clang/Sema/SemaHLSL.h"
#include "llvm/ADT/DenseSet.h"

namespace clang {

class HLSLOutParamBuilder {
  llvm::DenseSet<VarDecl *> SeenVars;

  // not copyable
  HLSLOutParamBuilder(const HLSLOutParamBuilder &) = delete;
  HLSLOutParamBuilder &operator=(const HLSLOutParamBuilder &) = delete;

  class DeclFinder : public StmtVisitor<DeclFinder> {
  public:
    VarDecl *Decl = nullptr;

    // TODO: For correctness, when multiple decls are found all decls should be
    // added to the Seen list.
    bool MultipleFound = false;

    DeclFinder() = default;

    void VisitStmt(Stmt *S) {
      for (Stmt *Child : S->children())
        if (Child)
          Visit(Child);
    }

    void VisitDeclRefExpr(DeclRefExpr *DRE) {
      if (MultipleFound)
        return;
      if (Decl)
        MultipleFound = true;
      Decl = dyn_cast<VarDecl>(DRE->getFoundDecl());
      return;
    }
  };

  ExprResult CreateCasted(Sema &Sema, ParmVarDecl *P, Expr *Base, QualType Ty) {
    ASTContext &Ctx = Sema.getASTContext();
    ExprResult Res =
        Sema.PerformImplicitConversion(Base, Ty, Sema::AA_Passing);
    if (Res.isInvalid())
      return ExprError();
    // After the cast, drop the reference type when creating the exprs.
    Ty = Ty.getNonLValueExprType(Ctx);
    HLSLOutParamExpr *OutExpr =
        HLSLOutParamExpr::Create(Ctx, Ty, Res.get(),
                                 P->hasAttr<HLSLInOutAttr>());
    auto *OpV = new (Ctx) OpaqueValueExpr(P->getLocStart(), Ty, VK_LValue,
                                          OK_Ordinary, OutExpr);
    Res = Sema.PerformImplicitConversion(OpV, Base->getType(),
                                          Sema::AA_Passing);
    if (Res.isInvalid())
      return ExprError();
    OutExpr->setWriteback(Res.get());
    OutExpr->setSrcLV(Base);
    OutExpr->setOpaqueValue(OpV);
    OpV->setSourceIsParent();
    return ExprResult(OutExpr);
  }

public:
  HLSLOutParamBuilder() = default;

  ExprResult Create(Sema &Sema, ParmVarDecl *P, Expr *Base) {
    ASTContext &Ctx = Sema.getASTContext();

    QualType Ty = P->getType().getNonLValueExprType(Ctx);

    if(hlsl::IsHLSLVecMatType(Base->getType()) && Ty->isScalarType()) {
      Sema.Diag(Base->getLocStart(), diag::err_hlsl_unsupported_lvalue_cast_op);
      return ExprError();
    }

    bool PossibleFlatConv =
      hlsl::IsConversionToLessOrEqualElements(&Sema, Base, Ty, true);
    bool RequiresConversion = Ty.getUnqualifiedType() != Base->getType().getUnqualifiedType();

    // If the unqualified types mismatch we may have some casting. Since this
    // results in a copy we can ignore qualifiers.
    if (!PossibleFlatConv && RequiresConversion)
      return CreateCasted(Sema, P, Base, Ty);

    DeclFinder DF;
    DF.Visit(Base);

    // If the analysis returned multiple possible decls, or no decl, or we've
    // seen the decl before, generate a HLSLOutParamExpr that can't be elided.
    // Note: The DXIL IR passes are fragile to a bunch of cases when vectors are
    // retained as parameter types to user-defined function calls. To work
    // around that we always emit copies for vector arguments. This is not a
    // regression because the HLParameterLegalization pass used to do the same
    // thing.
    if (DF.MultipleFound || DF.Decl == nullptr ||
        DF.Decl->getType().getQualifiers().hasAddressSpace() ||
        hlsl::IsHLSLVecType(Ty) || // This is a hack, see note above.
        SeenVars.count(DF.Decl) > 0 || !DF.Decl->hasLocalStorage()) {
      if (RequiresConversion)
        return CreateCasted(Sema, P, Base, P->getType());
      return ExprResult(
          HLSLOutParamExpr::Create(Ctx, Ty, Base, P->hasAttr<HLSLInOutAttr>()));
    }

    if (RequiresConversion) {
      ExprResult Res =
        Sema.PerformImplicitConversion(Base, P->getType(), Sema::AA_Passing);
      if (Res.isInvalid())
        return ExprError();
      Base = Res.get();
    }
    // Add the decl to the seen list, and generate a HLSLOutParamExpr that can
    // be elided.
    SeenVars.insert(DF.Decl);
    return ExprResult(HLSLOutParamExpr::Create(
        Ctx, Ty, Base, P->hasAttr<HLSLInOutAttr>(), true));
  }
};

} // namespace clang

#endif // CLANG_SEMA_HLSLOUTPARAMBUILDER_H
