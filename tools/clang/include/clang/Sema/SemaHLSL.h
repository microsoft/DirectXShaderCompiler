//===--- SemaHLSL.h - Semantic Analysis & AST Building for HLSL --*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// SemaHLSL.h                                                                //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
//  This file defines the semantic support for HLSL.                         //
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_CLANG_SEMA_SEMAHLSL_H
#define LLVM_CLANG_SEMA_SEMAHLSL_H

#include "clang/AST/ASTContext.h"
#include "clang/Sema/Sema.h"
#include "clang/Sema/Overload.h"
#include "clang/Sema/SemaDiagnostic.h"
#include "clang/Sema/Initialization.h"
#include "clang/Sema/Lookup.h"
#include "clang/Sema/Template.h"
#include "clang/Sema/TemplateDeduction.h"

// Forward declarations.
struct IDxcIntrinsicTable;
namespace clang {
  class Expr;
  class ExternalSemaSource;
  class ImplicitConversionSequence;
}

namespace hlsl {

void CheckBinOpForHLSL(
  clang::Sema& self,
  clang::SourceLocation OpLoc,
  clang::BinaryOperatorKind Opc,
  clang::ExprResult& LHS,
  clang::ExprResult& RHS,
  clang::QualType& ResultTy,
  clang::QualType& CompLHSTy,
  clang::QualType& CompResultTy);

bool CheckTemplateArgumentListForHLSL(
  clang::Sema& self, 
  clang::TemplateDecl*, 
  clang::SourceLocation, 
  clang::TemplateArgumentListInfo&);
  
clang::QualType CheckUnaryOpForHLSL(
  clang::Sema& self,
  clang::SourceLocation OpLoc,
  clang::UnaryOperatorKind Opc,
  clang::ExprResult& InputExpr,
  clang::ExprValueKind& VK,
  clang::ExprObjectKind& OK);

clang::Sema::TemplateDeductionResult DeduceTemplateArgumentsForHLSL(
  clang::Sema*, 
  clang::FunctionTemplateDecl *, 
  clang::TemplateArgumentListInfo *, 
  llvm::ArrayRef<clang::Expr *>, 
  clang::FunctionDecl *&, 
  clang::sema::TemplateDeductionInfo &);

void DiagnoseControlFlowConditionForHLSL(clang::Sema *self,
                                         clang::Expr *condExpr,
                                         llvm::StringRef StmtName);

void DiagnosePackingOffset(
  clang::Sema* self,
  clang::SourceLocation loc,
  clang::QualType type,
  int componentOffset);

void DiagnoseRegisterType(
  clang::Sema* self,
  clang::SourceLocation loc,
  clang::QualType type,
  char registerType);

void DiagnoseTranslationUnit(clang::Sema* self);

void DiagnoseUnusualAnnotationsForHLSL(
  clang::Sema& S,
  std::vector<hlsl::UnusualAnnotation *>& annotations);

/// <summary>Finds the best viable function on this overload set, if it exists.</summary>
clang::OverloadingResult GetBestViableFunction(
  clang::Sema &S,
  clang::SourceLocation Loc,
  clang::OverloadCandidateSet& set,
  clang::OverloadCandidateSet::iterator& Best);

/// <summary>Processes an attribute for a declaration.</summary>
/// <param name="S">Sema with context.</param>
/// <param name="D">Annotated declaration.</param>
/// <param name="A">Single parsed attribute to process.</param>
/// <param name="Handled">After execution, whether this was recognized and handled.</param>
void HandleDeclAttributeForHLSL(
  clang::Sema &S,
  _In_ clang::Decl *D,
  const clang::AttributeList &Attr,
  bool& Handled);

void InitializeInitSequenceForHLSL(
  clang::Sema* sema,
  const clang::InitializedEntity& Entity,
  const clang::InitializationKind& Kind,
  clang::MultiExprArg Args,
  bool TopLevelOfInitList,
  _Inout_ clang::InitializationSequence* initSequence);

unsigned CaculateInitListArraySizeForHLSL(
  _In_ clang::Sema* sema,
  _In_ const clang::InitListExpr *InitList,
  _In_ const clang::QualType EltTy);

bool IsConversionToLessOrEqualElements(
  _In_ clang::Sema* self,
  const clang::ExprResult& sourceExpr,
  const clang::QualType& targetType,
  bool explicitConversion);

bool LookupMatrixMemberExprForHLSL(
  clang::Sema* self,
  clang::Expr& BaseExpr,
  clang::DeclarationName MemberName,
  bool IsArrow,
  clang::SourceLocation OpLoc,
  clang::SourceLocation MemberLoc,
  _Inout_ clang::ExprResult* result);

bool LookupVectorMemberExprForHLSL(
  clang::Sema* self,
  clang::Expr& BaseExpr,
  clang::DeclarationName MemberName,
  bool IsArrow,
  clang::SourceLocation OpLoc,
  clang::SourceLocation MemberLoc,
  _Inout_ clang::ExprResult* result);

bool LookupArrayMemberExprForHLSL(
  clang::Sema* self,
  clang::Expr& BaseExpr,
  clang::DeclarationName MemberName,
  bool IsArrow,
  clang::SourceLocation OpLoc,
  clang::SourceLocation MemberLoc,
  _Inout_ clang::ExprResult* result);

clang::ExprResult MaybeConvertScalarToVector(
  _In_ clang::Sema* Self,
  _In_ clang::Expr* E);

/// <summary>Performs the HLSL-specific type conversion steps.</summary>
/// <param name="self">Sema with context.</param>
/// <param name="E">Expression to convert.</param>
/// <param name="targetType">Type to convert to.</param>
/// <param name="SCS">Standard conversion sequence from which Second and ComponentConversion will be used.</param>
/// <param name="CCK">Conversion kind.</param>
/// <returns>Expression result of conversion.</returns>
clang::ExprResult PerformHLSLConversion(
  _In_ clang::Sema* self,
  _In_ clang::Expr* E,
  _In_ clang::QualType targetType,
  _In_ const clang::StandardConversionSequence &SCS,
  _In_ clang::Sema::CheckedConversionKind CCK);

/// <summary>Processes an attribute for a statement.</summary>
/// <param name="S">Sema with context.</param>
/// <param name="St">Annotated statement.</param>
/// <param name="A">Single parsed attribute to process.</param>
/// <param name="Range">Range of all attribute lists (useful for FixIts to suggest inclusions).</param>
/// <param name="Handled">After execution, whether this was recognized and handled.</param>
/// <returns>An attribute instance if processed, nullptr if not recognized or an error was found.</returns>
clang::Attr *ProcessStmtAttributeForHLSL(
  clang::Sema &S,
  _In_ clang::Stmt *St,
  const clang::AttributeList &A,
  clang::SourceRange Range,
  bool& Handled);

bool TryStaticCastForHLSL(
  _In_ clang::Sema* Self,
  clang::ExprResult &SrcExpr,
  clang::QualType DestType,
  clang::Sema::CheckedConversionKind CCK,
  const clang::SourceRange &OpRange,
  unsigned &msg,
  clang::CastKind &Kind,
  clang::CXXCastPath &BasePath,
  bool ListInitialization,
  bool SuppressDiagnostics,
  _Inout_opt_ clang::StandardConversionSequence* standard);

clang::ImplicitConversionSequence TrySubscriptIndexInitialization(
  _In_ clang::Sema* Self,
  _In_ clang::Expr* SrcExpr,
  clang::QualType DestType);

bool IsHLSLAttr(clang::attr::Kind AttrKind);
void CustomPrintHLSLAttr(const clang::Attr *A, llvm::raw_ostream &Out, const clang::PrintingPolicy &Policy, unsigned int Indentation);
void PrintClipPlaneIfPresent(clang::Expr *ClipPlane, llvm::raw_ostream &Out, const clang::PrintingPolicy &Policy);
void Indent(unsigned int Indentation, llvm::raw_ostream &Out);
void GetHLSLAttributedTypes(
  _In_ clang::Sema* self,
  clang::QualType type, 
  _Inout_opt_ const clang::AttributedType** ppMatrixOrientation, 
  _Inout_opt_ const clang::AttributedType** ppNorm);

bool IsMatrixType(
  _In_ clang::Sema* self, 
  _In_ clang::QualType type);
bool IsVectorType(
  _In_ clang::Sema* self, 
  _In_ clang::QualType type);
clang::QualType GetOriginalMatrixOrVectorElementType(
  _In_ clang::QualType type);
clang::QualType GetOriginalElementType(
  _In_ clang::Sema* self, 
  _In_ clang::QualType type);

bool IsObjectType(
  _In_ clang::Sema* self,
  _In_ clang::QualType type,
  _Inout_opt_ bool *isDeprecatedEffectObject = nullptr);

bool CanConvert(
  _In_ clang::Sema* self, 
  clang::SourceLocation loc, 
  _In_ clang::Expr* sourceExpr, 
  clang::QualType target, 
  bool explicitConversion,
  _Inout_opt_ clang::StandardConversionSequence* standard);

// This function takes the external sema source rather than the sema object itself
// because the wire-up doesn't happen until parsing is initialized and we want
// to set this up earlier. If the HLSL constructs in the external sema move to
// Sema itself, this can be invoked on the Sema object directly.
void RegisterIntrinsicTable(_In_ clang::ExternalSemaSource* self, IDxcIntrinsicTable* table);

clang::QualType CheckVectorConditional(
  _In_ clang::Sema* self,
  _In_ clang::ExprResult &Cond,
  _In_ clang::ExprResult &LHS,
  _In_ clang::ExprResult &RHS,
  _In_ clang::SourceLocation QuestionLoc);

}

bool IsTypeNumeric(_In_ clang::Sema* self, _In_ clang::QualType &type);

// This function reads the given declaration TSS and returns the corresponding parsedType with the
// corresponding type. Replaces the given parsed type with the new type
clang::QualType ApplyTypeSpecSignToParsedType(
    _In_ clang::Sema* self,
    _In_ clang::QualType &type,
    _In_ clang::TypeSpecifierSign TSS,
    _In_ clang::SourceLocation Loc
);

#endif
