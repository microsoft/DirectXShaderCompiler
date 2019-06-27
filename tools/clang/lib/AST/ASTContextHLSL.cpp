//===--- ASTContextHLSL.cpp - HLSL support for AST nodes and operations ---===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// ASTContextHLSL.cpp                                                        //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
//  This file implements the ASTContext interface for HLSL.                  //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "clang/AST/ASTContext.h"
#include "clang/AST/Attr.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/ExternalASTSource.h"
#include "clang/AST/TypeLoc.h"
#include "clang/Sema/SemaDiagnostic.h"
#include "clang/Sema/Sema.h"
#include "clang/Sema/Overload.h"
#include "dxc/Support/Global.h"
#include "dxc/HLSL/HLOperations.h"
#include "dxc/DXIL/DxilSemantic.h"

using namespace clang;
using namespace hlsl;

static const int FirstTemplateDepth = 0;
static const int FirstParamPosition = 0;
static const bool ForConstFalse = false;          // a construct is targeting a const type
static const bool ForConstTrue = true;            // a construct is targeting a non-const type
static const bool ParameterPackFalse = false;     // template parameter is not an ellipsis.
static const bool TypenameFalse = false;          // 'typename' specified rather than 'class' for a template argument.
static const bool DelayTypeCreationTrue = true;   // delay type creation for a declaration
static const SourceLocation NoLoc;                // no source location attribution available
static const bool InlineFalse = false;            // namespace is not an inline namespace
static const bool InlineSpecifiedFalse = false;   // function was not specified as inline
static const bool ExplicitFalse = false;          // constructor was not specified as explicit
static const bool IsConstexprFalse = false;       // function is not constexpr
static const bool VirtualFalse = false;           // whether the base class is declares 'virtual'
static const bool BaseClassFalse = false;         // whether the base class is declared as 'class' (vs. 'struct')

/// <summary>Names of HLSLScalarType enumeration values, in matching order to HLSLScalarType.</summary>
const char* HLSLScalarTypeNames[] = {
  "<unknown>",
  "bool",
  "int",
  "uint",
  "dword",
  "half",
  "float",
  "double",
  "min10float",
  "min16float",
  "min12int",
  "min16int",
  "min16uint",
  "literal float",
  "literal int",
  "int16_t",
  "int32_t",
  "int64_t",
  "uint16_t",
  "uint32_t",
  "uint64_t",
  "float16_t",
  "float32_t",
  "float64_t"
};

static_assert(HLSLScalarTypeCount == _countof(HLSLScalarTypeNames), "otherwise scalar constants are not aligned");

static HLSLScalarType FindScalarTypeByName(const char *typeName, const size_t typeLen, const LangOptions& langOptions) {
  // skipped HLSLScalarType: unknown, literal int, literal float

  switch (typeLen) {
    case 3: // int
      if (typeName[0] == 'i') {
        if (strncmp(typeName, "int", 3))
          break;
        return HLSLScalarType_int;
      }
      break;
    case 4: // bool, uint, half
      if (typeName[0] == 'b') {
        if (strncmp(typeName, "bool", 4))
          break;
        return HLSLScalarType_bool;
      }
      else if (typeName[0] == 'u') {
        if (strncmp(typeName, "uint", 4))
          break;
        return HLSLScalarType_uint;
      }
      else if (typeName[0] == 'h') {
        if (strncmp(typeName, "half", 4))
          break;
        return HLSLScalarType_half;
      }
      break;
    case 5: // dword, float
      if (typeName[0] == 'd') {
        if (strncmp(typeName, "dword", 5))
          break;
        return HLSLScalarType_dword;
      }
      else if (typeName[0] == 'f') {
        if (strncmp(typeName, "float", 5))
          break;
        return HLSLScalarType_float;
      }
      break;
    case 6: // double
      if (typeName[0] == 'd') {
        if (strncmp(typeName, "double", 6))
          break;
        return HLSLScalarType_double;
      }
      break;
    case 7: // int64_t
      if (typeName[0] == 'i' && typeName[1] == 'n') {
        if (typeName[3] == '6') {
          if (strncmp(typeName, "int64_t", 7))
            break;
          return HLSLScalarType_int64;
        }
      }
    case 8: // min12int, min16int, uint64_t
      if (typeName[0] == 'm' && typeName[1] == 'i') {
        if (typeName[4] == '2') {
          if (strncmp(typeName, "min12int", 8))
            break;
          return HLSLScalarType_int_min12;
        }
        else if (typeName[4] == '6') {
          if (strncmp(typeName, "min16int", 8))
            break;
          return HLSLScalarType_int_min16;
        }
      }
      else if (typeName[0] == 'u' && typeName[1] == 'i') {
        if (typeName[4] == '6') {
          if (strncmp(typeName, "uint64_t", 8))
            break;
          return HLSLScalarType_uint64;
        }
      }
      break;
    case 9: // min16uint
      if (typeName[0] == 'm' && typeName[1] == 'i') {
        if (strncmp(typeName, "min16uint", 9))
          break;
        return HLSLScalarType_uint_min16;
      }
      break;
    case 10: // min10float, min16float
      if (typeName[0] == 'm' && typeName[1] == 'i') {
        if (typeName[4] == '0') {
          if (strncmp(typeName, "min10float", 10))
            break;
          return HLSLScalarType_float_min10;
        }
        if (typeName[4] == '6') {
          if (strncmp(typeName, "min16float", 10))
            break;
          return HLSLScalarType_float_min16;
        }
      }
      break;
    default:
      break;
  }
  // fixed width types (int16_t, uint16_t, int32_t, uint32_t, float16_t, float32_t, float64_t)
  // are only supported in HLSL 2018
  if (langOptions.HLSLVersion >= 2018) {
    switch (typeLen) {
    case 7: // int16_t, int32_t
      if (typeName[0] == 'i' && typeName[1] == 'n') {
        if (!langOptions.UseMinPrecision) {
          if (typeName[3] == '1') {
            if (strncmp(typeName, "int16_t", 7))
              break;
            return HLSLScalarType_int16;
          }
        }
        if (typeName[3] == '3') {
          if (strncmp(typeName, "int32_t", 7))
            break;
          return HLSLScalarType_int32;
        }
      }
    case 8: // uint16_t, uint32_t
      if (!langOptions.UseMinPrecision) {
        if (typeName[0] == 'u' && typeName[1] == 'i') {
          if (typeName[4] == '1') {
            if (strncmp(typeName, "uint16_t", 8))
              break;
            return HLSLScalarType_uint16;
          }
        }
      }
      if (typeName[4] == '3') {
        if (strncmp(typeName, "uint32_t", 8))
          break;
        return HLSLScalarType_uint32;
      }
    case 9: // float16_t, float32_t, float64_t
      if (typeName[0] == 'f' && typeName[1] == 'l') {
        if (!langOptions.UseMinPrecision) {
          if (typeName[5] == '1') {
            if (strncmp(typeName, "float16_t", 9))
              break;
            return HLSLScalarType_float16;
          }
        }
        if (typeName[5] == '3') {
          if (strncmp(typeName, "float32_t", 9))
            break;
          return HLSLScalarType_float32;
        }
        else if (typeName[5] == '6') {
          if (strncmp(typeName, "float64_t", 9))
            break;
          return HLSLScalarType_float64;
        }
      }
    }
  }
  return HLSLScalarType_unknown;
}

/// <summary>Provides the primitive type for lowering matrix types to IR.</summary>
static
CanQualType GetHLSLObjectHandleType(ASTContext& context)
{
  return context.IntTy;
}

/// <summary>Adds a handle field to the specified record.</summary>
static 
void AddHLSLHandleField(ASTContext& context, DeclContext* recordDecl, QualType handleQualType)
{
  IdentifierInfo& handleId = context.Idents.get(StringRef("h"), tok::TokenKind::identifier);
  TypeSourceInfo* fieldTypeSource = context.getTrivialTypeSourceInfo(handleQualType, NoLoc);
  const bool MutableFalse = false;
  const InClassInitStyle initStyle = InClassInitStyle::ICIS_NoInit;
  FieldDecl* handleDecl = FieldDecl::Create(
    context, recordDecl, NoLoc, NoLoc, &handleId, handleQualType, fieldTypeSource, nullptr, MutableFalse, initStyle);
  handleDecl->setAccess(AccessSpecifier::AS_private);
  handleDecl->setImplicit(true); 

  recordDecl->addDecl(handleDecl);
}

static
void AddSubscriptOperator(
  ASTContext& context, unsigned int templateDepth, TemplateTypeParmDecl *elementTemplateParamDecl,
  NonTypeTemplateParmDecl* colCountTemplateParamDecl, QualType intType, CXXRecordDecl* templateRecordDecl,
  ClassTemplateDecl* vectorTemplateDecl,
  bool forConst)
{
  QualType elementType = context.getTemplateTypeParmType(
    templateDepth, 0, ParameterPackFalse, elementTemplateParamDecl);
  Expr* sizeExpr = DeclRefExpr::Create(context, NestedNameSpecifierLoc(), NoLoc, colCountTemplateParamDecl, false,
    DeclarationNameInfo(colCountTemplateParamDecl->getDeclName(), NoLoc),
    intType, ExprValueKind::VK_RValue);

  CXXRecordDecl *vecTemplateRecordDecl = vectorTemplateDecl->getTemplatedDecl();
  const clang::Type *vecTy = vecTemplateRecordDecl->getTypeForDecl();

  TemplateArgument templateArgs[2] =
  {
    TemplateArgument(elementType),
    TemplateArgument(sizeExpr)
  };
  TemplateName canonName = context.getCanonicalTemplateName(TemplateName(vectorTemplateDecl));
  QualType vectorType = context.getTemplateSpecializationType(
      canonName, templateArgs, _countof(templateArgs), QualType(vecTy, 0));

  vectorType = context.getLValueReferenceType(vectorType);

  if (forConst)
    vectorType = context.getConstType(vectorType);

  QualType indexType = intType;
  CreateObjectFunctionDeclarationWithParams(
    context, templateRecordDecl, vectorType,
    ArrayRef<QualType>(indexType), ArrayRef<StringRef>(StringRef("index")),
    context.DeclarationNames.getCXXOperatorName(OO_Subscript), forConst);
}

/// <summary>Adds up-front support for HLSL matrix types (just the template declaration).</summary>
void hlsl::AddHLSLMatrixTemplate(ASTContext& context, ClassTemplateDecl* vectorTemplateDecl, ClassTemplateDecl** matrixTemplateDecl)
{
  DXASSERT_NOMSG(matrixTemplateDecl != nullptr);
  DXASSERT_NOMSG(vectorTemplateDecl != nullptr);

  DeclContext* currentDeclContext = context.getTranslationUnitDecl();

  // Create a matrix template declaration in translation unit scope.
  // template<typename element, int row_count, int col_count> matrix { ... }
  IdentifierInfo& elementTemplateParamId = context.Idents.get(StringRef("element"), tok::TokenKind::identifier);
  TemplateTypeParmDecl *elementTemplateParamDecl = TemplateTypeParmDecl::Create(
    context, currentDeclContext, NoLoc, NoLoc,
    FirstTemplateDepth, FirstParamPosition, &elementTemplateParamId, TypenameFalse, ParameterPackFalse);
  elementTemplateParamDecl->setDefaultArgument(context.getTrivialTypeSourceInfo(context.FloatTy));
  QualType intType = context.IntTy;
  Expr *literalIntFour = IntegerLiteral::Create(
      context, llvm::APInt(context.getIntWidth(intType), 4), intType, NoLoc);
  IdentifierInfo& rowCountParamId = context.Idents.get(StringRef("row_count"), tok::TokenKind::identifier);
  NonTypeTemplateParmDecl* rowCountTemplateParamDecl = NonTypeTemplateParmDecl::Create(
    context, currentDeclContext, NoLoc, NoLoc,
    FirstTemplateDepth, FirstParamPosition + 1, &rowCountParamId, intType, ParameterPackFalse, context.getTrivialTypeSourceInfo(intType));
  rowCountTemplateParamDecl->setDefaultArgument(literalIntFour);
  IdentifierInfo& colCountParamId = context.Idents.get(StringRef("col_count"), tok::TokenKind::identifier);
  NonTypeTemplateParmDecl* colCountTemplateParamDecl = NonTypeTemplateParmDecl::Create(
    context, currentDeclContext, NoLoc, NoLoc,
    FirstTemplateDepth, FirstParamPosition + 2, &colCountParamId, intType, ParameterPackFalse, context.getTrivialTypeSourceInfo(intType));
  colCountTemplateParamDecl->setDefaultArgument(literalIntFour);
  NamedDecl* templateParameters[] =
  {
    elementTemplateParamDecl, rowCountTemplateParamDecl, colCountTemplateParamDecl
  };
  TemplateParameterList* templateParameterList = TemplateParameterList::Create(
    context, NoLoc, NoLoc, templateParameters, _countof(templateParameters), NoLoc);

  IdentifierInfo& matrixId = context.Idents.get(StringRef("matrix"), tok::TokenKind::identifier);
  CXXRecordDecl* templateRecordDecl = CXXRecordDecl::Create(
    context, TagDecl::TagKind::TTK_Class, currentDeclContext, NoLoc, NoLoc, &matrixId,
    nullptr, DelayTypeCreationTrue);
  ClassTemplateDecl* classTemplateDecl = ClassTemplateDecl::Create(
    context, currentDeclContext, NoLoc, DeclarationName(&matrixId),
    templateParameterList, templateRecordDecl, nullptr);
  templateRecordDecl->setDescribedClassTemplate(classTemplateDecl);
  templateRecordDecl->addAttr(FinalAttr::CreateImplicit(context, FinalAttr::Keyword_final));

  // Requesting the class name specialization will fault in required types.
  QualType T = classTemplateDecl->getInjectedClassNameSpecialization();
  T = context.getInjectedClassNameType(templateRecordDecl, T);
  assert(T->isDependentType() && "Class template type is not dependent?");
  classTemplateDecl->setLexicalDeclContext(currentDeclContext);
  templateRecordDecl->setLexicalDeclContext(currentDeclContext);
  templateRecordDecl->startDefinition();

  // Add an 'h' field to hold the handle.
  // The type is vector<element, col>[row].
  QualType elementType = context.getTemplateTypeParmType(
      /*templateDepth*/ 0, 0, ParameterPackFalse, elementTemplateParamDecl);
  Expr *sizeExpr = DeclRefExpr::Create(
      context, NestedNameSpecifierLoc(), NoLoc, rowCountTemplateParamDecl,
      false,
      DeclarationNameInfo(rowCountTemplateParamDecl->getDeclName(), NoLoc),
      intType, ExprValueKind::VK_RValue);

  Expr *rowSizeExpr = DeclRefExpr::Create(
      context, NestedNameSpecifierLoc(), NoLoc, colCountTemplateParamDecl,
      false,
      DeclarationNameInfo(colCountTemplateParamDecl->getDeclName(), NoLoc),
      intType, ExprValueKind::VK_RValue);

  QualType vectorType = context.getDependentSizedExtVectorType(
      elementType, rowSizeExpr, SourceLocation());
  QualType vectorArrayType = context.getDependentSizedArrayType(
      vectorType, sizeExpr, ArrayType::Normal, 0, SourceRange());

  AddHLSLHandleField(context, templateRecordDecl, vectorArrayType);

  // Add an operator[]. The operator ranges from zero to rowcount-1, and returns a vector of colcount elements.
  const unsigned int templateDepth = 0;
  AddSubscriptOperator(context, templateDepth, elementTemplateParamDecl,
                       colCountTemplateParamDecl, context.UnsignedIntTy,
                       templateRecordDecl, vectorTemplateDecl, ForConstFalse);
  AddSubscriptOperator(context, templateDepth, elementTemplateParamDecl,
                       colCountTemplateParamDecl, context.UnsignedIntTy,
                       templateRecordDecl, vectorTemplateDecl, ForConstTrue);

  templateRecordDecl->completeDefinition();

  classTemplateDecl->setImplicit(true);
  templateRecordDecl->setImplicit(true);

  // Both declarations need to be present for correct handling.
  currentDeclContext->addDecl(classTemplateDecl);
  currentDeclContext->addDecl(templateRecordDecl);

#ifdef DBG
  // Verify that we can read the field member from the template record.
  DeclContext::lookup_result lookupResult = templateRecordDecl->lookup(
    DeclarationName(&context.Idents.get(StringRef("h"))));
  DXASSERT(!lookupResult.empty(), "otherwise matrix handle cannot be looked up");
#endif

  *matrixTemplateDecl = classTemplateDecl;
}

static void AddHLSLVectorSubscriptAttr(Decl *D, ASTContext &context) {
  StringRef group = GetHLOpcodeGroupName(HLOpcodeGroup::HLSubscript);
  D->addAttr(HLSLIntrinsicAttr::CreateImplicit(context, group, "", static_cast<unsigned>(HLSubscriptOpcode::VectorSubscript)));
}

/// <summary>Adds up-front support for HLSL vector types (just the template declaration).</summary>
void hlsl::AddHLSLVectorTemplate(ASTContext& context, ClassTemplateDecl** vectorTemplateDecl)
{
  DXASSERT_NOMSG(vectorTemplateDecl != nullptr);

  DeclContext* currentDeclContext = context.getTranslationUnitDecl();

  // Create a vector template declaration in translation unit scope.
  // template<typename element, int element_count> vector { ... }
  IdentifierInfo& elementTemplateParamId = context.Idents.get(StringRef("element"), tok::TokenKind::identifier);
  TemplateTypeParmDecl *elementTemplateParamDecl = TemplateTypeParmDecl::Create(
    context, currentDeclContext, NoLoc, NoLoc,
    FirstTemplateDepth, FirstParamPosition, &elementTemplateParamId, TypenameFalse, ParameterPackFalse);
  elementTemplateParamDecl->setDefaultArgument(context.getTrivialTypeSourceInfo(context.FloatTy));
  QualType intType = context.IntTy;
  Expr *literalIntFour = IntegerLiteral::Create(
      context, llvm::APInt(context.getIntWidth(intType), 4), intType, NoLoc);
  IdentifierInfo& colCountParamId = context.Idents.get(StringRef("element_count"), tok::TokenKind::identifier);
  NonTypeTemplateParmDecl* colCountTemplateParamDecl = NonTypeTemplateParmDecl::Create(
    context, currentDeclContext, NoLoc, NoLoc,
    FirstTemplateDepth, FirstParamPosition + 1, &colCountParamId, intType, ParameterPackFalse, nullptr);
  colCountTemplateParamDecl->setDefaultArgument(literalIntFour);
  NamedDecl* templateParameters[] =
  {
    elementTemplateParamDecl, colCountTemplateParamDecl
  };
  TemplateParameterList* templateParameterList = TemplateParameterList::Create(
    context, NoLoc, NoLoc, templateParameters, _countof(templateParameters), NoLoc);

  IdentifierInfo& vectorId = context.Idents.get(StringRef("vector"), tok::TokenKind::identifier);
  CXXRecordDecl* templateRecordDecl = CXXRecordDecl::Create(
    context, TagDecl::TagKind::TTK_Class, currentDeclContext, NoLoc, NoLoc, &vectorId,
    nullptr, DelayTypeCreationTrue);
  ClassTemplateDecl* classTemplateDecl = ClassTemplateDecl::Create(
    context, currentDeclContext, NoLoc, DeclarationName(&vectorId),
    templateParameterList, templateRecordDecl, nullptr);
  templateRecordDecl->setDescribedClassTemplate(classTemplateDecl);
  templateRecordDecl->addAttr(FinalAttr::CreateImplicit(context, FinalAttr::Keyword_final));

  // Requesting the class name specialization will fault in required types.
  QualType T = classTemplateDecl->getInjectedClassNameSpecialization();
  T = context.getInjectedClassNameType(templateRecordDecl, T);
  assert(T->isDependentType() && "Class template type is not dependent?");
  classTemplateDecl->setLexicalDeclContext(currentDeclContext);
  templateRecordDecl->setLexicalDeclContext(currentDeclContext);
  templateRecordDecl->startDefinition();

  Expr *vecSizeExpr = DeclRefExpr::Create(
      context, NestedNameSpecifierLoc(), NoLoc, colCountTemplateParamDecl,
      false,
      DeclarationNameInfo(colCountTemplateParamDecl->getDeclName(), NoLoc),
      intType, ExprValueKind::VK_RValue);

  const unsigned int templateDepth = 0;
  QualType resultType = context.getTemplateTypeParmType(
    templateDepth, 0, ParameterPackFalse, elementTemplateParamDecl);
  QualType vectorType = context.getDependentSizedExtVectorType(
      resultType, vecSizeExpr, SourceLocation());
  // Add an 'h' field to hold the handle.
  AddHLSLHandleField(context, templateRecordDecl, vectorType);

  // Add an operator[]. The operator ranges from zero to colcount-1, and returns a scalar.

  // ForConstTrue:
  QualType refResultType = context.getConstType(context.getLValueReferenceType(resultType));
  CXXMethodDecl* functionDecl = CreateObjectFunctionDeclarationWithParams(
    context, templateRecordDecl, refResultType,
    ArrayRef<QualType>(context.UnsignedIntTy), ArrayRef<StringRef>(StringRef("index")),
    context.DeclarationNames.getCXXOperatorName(OO_Subscript), ForConstTrue);
  AddHLSLVectorSubscriptAttr(functionDecl, context);
  // ForConstFalse:
  resultType = context.getLValueReferenceType(resultType);
  functionDecl = CreateObjectFunctionDeclarationWithParams(
    context, templateRecordDecl, resultType,
    ArrayRef<QualType>(context.UnsignedIntTy), ArrayRef<StringRef>(StringRef("index")),
    context.DeclarationNames.getCXXOperatorName(OO_Subscript), ForConstFalse);
  AddHLSLVectorSubscriptAttr(functionDecl, context);

  templateRecordDecl->completeDefinition();

  classTemplateDecl->setImplicit(true);
  templateRecordDecl->setImplicit(true);

  // Both declarations need to be present for correct handling.
  currentDeclContext->addDecl(classTemplateDecl);
  currentDeclContext->addDecl(templateRecordDecl);

#ifdef DBG
  // Verify that we can read the field member from the template record.
  DeclContext::lookup_result lookupResult = templateRecordDecl->lookup(
    DeclarationName(&context.Idents.get(StringRef("h"))));
  DXASSERT(!lookupResult.empty(), "otherwise vector handle cannot be looked up");
#endif

  *vectorTemplateDecl = classTemplateDecl;
}

/// <summary>
/// Adds a new record type in the specified context with the given name. The record type will have a handle field.
/// </summary>
void hlsl::AddRecordTypeWithHandle(ASTContext& context, _Outptr_ CXXRecordDecl** typeDecl, _In_z_ const char* typeName)
{
  DXASSERT_NOMSG(typeDecl != nullptr);
  DXASSERT_NOMSG(typeName != nullptr);
  
  *typeDecl = nullptr;

  DeclContext* currentDeclContext = context.getTranslationUnitDecl();
  IdentifierInfo& newTypeId = context.Idents.get(StringRef(typeName), tok::TokenKind::identifier);
  CXXRecordDecl* newDecl = CXXRecordDecl::Create(
    context, TagDecl::TagKind::TTK_Struct, currentDeclContext, NoLoc, NoLoc, &newTypeId, nullptr);
  newDecl->setLexicalDeclContext(currentDeclContext);
  newDecl->setFreeStanding();
  newDecl->addAttr(FinalAttr::CreateImplicit(context, FinalAttr::Keyword_final));
  newDecl->startDefinition();
  AddHLSLHandleField(context, newDecl, QualType(GetHLSLObjectHandleType(context)));
  currentDeclContext->addDecl(newDecl);
  newDecl->completeDefinition();

  *typeDecl = newDecl;
}

// creates a global static constant unsigned integer with value.
// equivalent to: static const uint name = val;
static void AddConstUInt(clang::ASTContext& context, DeclContext *DC, StringRef name, unsigned val) {
  IdentifierInfo &Id = context.Idents.get(name, tok::TokenKind::identifier);
  QualType type = context.getConstType(context.UnsignedIntTy);
  VarDecl *varDecl = VarDecl::Create(context, DC, NoLoc, NoLoc, &Id, type,
                                context.getTrivialTypeSourceInfo(type),
                                clang::StorageClass::SC_Static);
  Expr *exprVal = IntegerLiteral::Create(
      context, llvm::APInt(context.getIntWidth(type), val), type, NoLoc);
  varDecl->setInit(exprVal);
  varDecl->setImplicit(true);
  DC->addDecl(varDecl);
}

/// <summary> Adds a const integers for ray flags </summary>
void hlsl::AddRayFlags(ASTContext& context) {
  DeclContext *curDC = context.getTranslationUnitDecl();
  // typedef uint RAY_FLAG;
  IdentifierInfo &rayFlagId = context.Idents.get(StringRef("RAY_FLAG"), tok::TokenKind::identifier);
  TypeSourceInfo *uintTypeSource = context.getTrivialTypeSourceInfo(context.UnsignedIntTy, NoLoc);
  TypedefDecl *rayFlagDecl = TypedefDecl::Create(context, curDC, NoLoc, NoLoc, &rayFlagId, uintTypeSource);
  curDC->addDecl(rayFlagDecl);
  rayFlagDecl->setImplicit(true);
  // static const uint RAY_FLAG_* = *;
  AddConstUInt(context, curDC, StringRef("RAY_FLAG_NONE"), (unsigned)DXIL::RayFlag::None);
  AddConstUInt(context, curDC, StringRef("RAY_FLAG_FORCE_OPAQUE"), (unsigned)DXIL::RayFlag::ForceOpaque);
  AddConstUInt(context, curDC, StringRef("RAY_FLAG_FORCE_NON_OPAQUE"), (unsigned)DXIL::RayFlag::ForceNonOpaque);
  AddConstUInt(context, curDC, StringRef("RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH"), (unsigned)DXIL::RayFlag::AcceptFirstHitAndEndSearch);
  AddConstUInt(context, curDC, StringRef("RAY_FLAG_SKIP_CLOSEST_HIT_SHADER"), (unsigned)DXIL::RayFlag::SkipClosestHitShader);
  AddConstUInt(context, curDC, StringRef("RAY_FLAG_CULL_BACK_FACING_TRIANGLES"), (unsigned)DXIL::RayFlag::CullBackFacingTriangles);
  AddConstUInt(context, curDC, StringRef("RAY_FLAG_CULL_FRONT_FACING_TRIANGLES"), (unsigned)DXIL::RayFlag::CullFrontFacingTriangles);
  AddConstUInt(context, curDC, StringRef("RAY_FLAG_CULL_OPAQUE"), (unsigned)DXIL::RayFlag::CullOpaque);
  AddConstUInt(context, curDC, StringRef("RAY_FLAG_CULL_NON_OPAQUE"), (unsigned)DXIL::RayFlag::CullNonOpaque);
}

/// <summary> Adds a constant integers for hit kinds </summary>
void hlsl::AddHitKinds(ASTContext& context) {
  DeclContext *curDC = context.getTranslationUnitDecl();
  // static const uint HIT_KIND_* = *;
  AddConstUInt(context, curDC, StringRef("HIT_KIND_NONE"), (unsigned)DXIL::HitKind::None);
  AddConstUInt(context, curDC, StringRef("HIT_KIND_TRIANGLE_FRONT_FACE"), (unsigned)DXIL::HitKind::TriangleFrontFace);
  AddConstUInt(context, curDC, StringRef("HIT_KIND_TRIANGLE_BACK_FACE"), (unsigned)DXIL::HitKind::TriangleBackFace);
}

/// <summary> Adds a constant integers for state object flags </summary>
void hlsl::AddStateObjectFlags(ASTContext& context) {
  DeclContext *curDC = context.getTranslationUnitDecl();
 
  AddConstUInt(context, curDC, StringRef("STATE_OBJECT_FLAGS_ALLOW_LOCAL_DEPENDENCIES_ON_EXTERNAL_DEFINITONS"), (unsigned)DXIL::StateObjectFlags::AllowLocalDependenciesOnExternalDefinitions);
  AddConstUInt(context, curDC, StringRef("STATE_OBJECT_FLAGS_ALLOW_EXTERNAL_DEPENDENCIES_ON_LOCAL_DEFINITIONS"), (unsigned)DXIL::StateObjectFlags::AllowExternalDependenciesOnLocalDefinitions);
}

static
Expr* IntConstantAsBoolExpr(clang::Sema& sema, uint64_t value)
{
  return sema.ImpCastExprToType(
    sema.ActOnIntegerConstant(NoLoc, value).get(), sema.getASTContext().BoolTy, CK_IntegralToBoolean).get();
}

static
CXXRecordDecl* CreateStdStructWithStaticBool(clang::ASTContext& context, NamespaceDecl* stdNamespace, IdentifierInfo& trueTypeId, IdentifierInfo& valueId, Expr* trueExpression)
{
  // struct true_type { static const bool value = true; }
  TypeSourceInfo* boolTypeSource = context.getTrivialTypeSourceInfo(context.BoolTy.withConst());
  CXXRecordDecl* trueTypeDecl = CXXRecordDecl::Create(context, TagTypeKind::TTK_Struct, stdNamespace, NoLoc, NoLoc, &trueTypeId, nullptr, DelayTypeCreationTrue);

  // static fields are variables in the AST
  VarDecl* trueValueDecl = VarDecl::Create(context, trueTypeDecl, NoLoc, NoLoc, &valueId,
    context.BoolTy.withConst(), boolTypeSource, SC_Static);

  trueValueDecl->setInit(trueExpression);
  trueValueDecl->setConstexpr(true);
  trueValueDecl->setAccess(AS_public);
  trueTypeDecl->setLexicalDeclContext(stdNamespace);
  trueTypeDecl->startDefinition();
  trueTypeDecl->addDecl(trueValueDecl);
  trueTypeDecl->completeDefinition();
  stdNamespace->addDecl(trueTypeDecl);

  return trueTypeDecl;
}

static
void DefineRecordWithBase(CXXRecordDecl* decl, DeclContext* lexicalContext, const CXXBaseSpecifier* base)
{
  decl->setLexicalDeclContext(lexicalContext);
  decl->startDefinition();
  decl->setBases(&base, 1);
  decl->completeDefinition();
  lexicalContext->addDecl(decl);
}

static
void SetPartialExplicitSpecialization(ClassTemplateDecl* templateDecl, ClassTemplatePartialSpecializationDecl* specializationDecl)
{
  specializationDecl->setSpecializationKind(TSK_ExplicitSpecialization);
  templateDecl->AddPartialSpecialization(specializationDecl, nullptr);
}

static
void CreateIsEqualSpecialization(ASTContext& context, ClassTemplateDecl* templateDecl, TemplateName& templateName,
  DeclContext* lexicalContext, const CXXBaseSpecifier* base, TemplateParameterList* templateParamList,
  TemplateArgument (&templateArgs)[2])
{
  QualType specializationCanonType = context.getTemplateSpecializationType(templateName, templateArgs, _countof(templateArgs));

  TemplateArgumentListInfo templateArgsListInfo = TemplateArgumentListInfo(NoLoc, NoLoc);
  templateArgsListInfo.addArgument(TemplateArgumentLoc(templateArgs[0], context.getTrivialTypeSourceInfo(templateArgs[0].getAsType())));
  templateArgsListInfo.addArgument(TemplateArgumentLoc(templateArgs[1], context.getTrivialTypeSourceInfo(templateArgs[1].getAsType())));

  ClassTemplatePartialSpecializationDecl* specializationDecl =
    ClassTemplatePartialSpecializationDecl::Create(context, TTK_Struct, lexicalContext, NoLoc, NoLoc,
    templateParamList, templateDecl, templateArgs, _countof(templateArgs),
    templateArgsListInfo, specializationCanonType, nullptr);
  context.getTagDeclType(specializationDecl); // Fault this in now.
  DefineRecordWithBase(specializationDecl, lexicalContext, base);
  SetPartialExplicitSpecialization(templateDecl, specializationDecl);
}

/// <summary>Adds the implementation for std::is_equal.</summary>
void hlsl::AddStdIsEqualImplementation(clang::ASTContext& context, clang::Sema& sema)
{
  // The goal is to support std::is_same<T, T>::value for testing purposes, in a manner that can 
  // evolve into a compliant feature in the future.
  //
  // The definitions necessary are as follows (all in the std namespace).
  //  template <class T, T v>
  //  struct integral_constant {
  //    typedef T value_type;
  //    static const value_type value = v;
  //    operator value_type() { return value; }
  //  };
  //
  //  typedef integral_constant<bool, true> true_type;
  //  typedef integral_constant<bool, false> false_type;
  //
  //  template<typename T, typename U> struct is_same : public false_type {};
  //  template<typename T>             struct is_same<T, T> : public true_type{};
  //
  // We instead use these simpler definitions for true_type and false_type.
  //  struct false_type { static const bool value = false; };
  //  struct true_type { static const bool value = true; };
  DeclContext* tuContext = context.getTranslationUnitDecl();
  IdentifierInfo& stdId = context.Idents.get(StringRef("std"), tok::TokenKind::identifier);
  IdentifierInfo& trueTypeId = context.Idents.get(StringRef("true_type"), tok::TokenKind::identifier);
  IdentifierInfo& falseTypeId = context.Idents.get(StringRef("false_type"), tok::TokenKind::identifier);
  IdentifierInfo& valueId = context.Idents.get(StringRef("value"), tok::TokenKind::identifier);
  IdentifierInfo& isSameId = context.Idents.get(StringRef("is_same"), tok::TokenKind::identifier);
  IdentifierInfo& tId = context.Idents.get(StringRef("T"), tok::TokenKind::identifier);
  IdentifierInfo& vId = context.Idents.get(StringRef("V"), tok::TokenKind::identifier);

  Expr* trueExpression = IntConstantAsBoolExpr(sema, 1);
  Expr* falseExpression = IntConstantAsBoolExpr(sema, 0);

  // namespace std
  NamespaceDecl* stdNamespace = NamespaceDecl::Create(context, tuContext, InlineFalse, NoLoc, NoLoc, &stdId, nullptr);

  CXXRecordDecl* trueTypeDecl = CreateStdStructWithStaticBool(context, stdNamespace, trueTypeId, valueId, trueExpression);
  CXXRecordDecl* falseTypeDecl = CreateStdStructWithStaticBool(context, stdNamespace, falseTypeId, valueId, falseExpression);

  //  template<typename T, typename U> struct is_same : public false_type {};
  CXXRecordDecl* isSameFalseRecordDecl = CXXRecordDecl::Create(context, TagTypeKind::TTK_Struct, stdNamespace, NoLoc, NoLoc, &isSameId, nullptr, false);
  TemplateTypeParmDecl* tParam = TemplateTypeParmDecl::Create(context, stdNamespace, NoLoc, NoLoc, FirstTemplateDepth, FirstParamPosition, &tId, TypenameFalse, ParameterPackFalse);
  TemplateTypeParmDecl* uParam = TemplateTypeParmDecl::Create(context, stdNamespace, NoLoc, NoLoc, FirstTemplateDepth, FirstParamPosition + 1, &vId, TypenameFalse, ParameterPackFalse);
  NamedDecl* falseParams[] = { tParam, uParam };
  TemplateParameterList* falseParamList = TemplateParameterList::Create(context, NoLoc, NoLoc, falseParams, _countof(falseParams), NoLoc);
  ClassTemplateDecl* isSameFalseTemplateDecl = ClassTemplateDecl::Create(context, stdNamespace, NoLoc, DeclarationName(&isSameId), falseParamList, isSameFalseRecordDecl, nullptr);
  context.getTagDeclType(isSameFalseRecordDecl); // Fault this in now.
  CXXBaseSpecifier* falseBase = new (context)CXXBaseSpecifier(SourceRange(), VirtualFalse, BaseClassFalse, AS_public,
    context.getTrivialTypeSourceInfo(context.getTypeDeclType(falseTypeDecl)), NoLoc);
  isSameFalseRecordDecl->setDescribedClassTemplate(isSameFalseTemplateDecl);
  isSameFalseTemplateDecl->setLexicalDeclContext(stdNamespace);
  DefineRecordWithBase(isSameFalseRecordDecl, stdNamespace, falseBase);

  // is_same for 'true' is a specialization of is_same for 'false', taking a single T, where both T will match
  //  template<typename T> struct is_same<T, T> : public true_type{};
  TemplateName tn = TemplateName(isSameFalseTemplateDecl);
  NamedDecl* trueParams[] = { tParam };
  TemplateParameterList* trueParamList = TemplateParameterList::Create(context, NoLoc, NoLoc, trueParams, _countof(trueParams), NoLoc);
  CXXBaseSpecifier* trueBase = new (context)CXXBaseSpecifier(SourceRange(), VirtualFalse, BaseClassFalse, AS_public,
    context.getTrivialTypeSourceInfo(context.getTypeDeclType(trueTypeDecl)), NoLoc);

  TemplateArgument ta = TemplateArgument(context.getCanonicalType(context.getTypeDeclType(tParam)));
  TemplateArgument isSameTrueTemplateArgs[] = { ta, ta };
  CreateIsEqualSpecialization(context, isSameFalseTemplateDecl, tn, stdNamespace, trueBase, trueParamList, isSameTrueTemplateArgs);

  stdNamespace->addDecl(isSameFalseTemplateDecl);
  stdNamespace->setImplicit(true);
  tuContext->addDecl(stdNamespace);

  // This could be a parameter if ever needed.
  const bool SupportExtensions = true;

  // Consider right-hand const and right-hand ref to be true for is_same:
  // template<typename T> struct is_same<T, const T> : public true_type{};
  // template<typename T> struct is_same<T, T&>      : public true_type{};
  if (SupportExtensions)
  {
    TemplateArgument trueConstArg = TemplateArgument(context.getCanonicalType(context.getTypeDeclType(tParam)).withConst());
    TemplateArgument isSameTrueConstTemplateArgs[] = { ta, trueConstArg };
    CreateIsEqualSpecialization(context, isSameFalseTemplateDecl, tn, stdNamespace, trueBase, trueParamList, isSameTrueConstTemplateArgs);

    TemplateArgument trueRefArg = TemplateArgument(
      context.getLValueReferenceType(context.getCanonicalType(context.getTypeDeclType(tParam))));
    TemplateArgument isSameTrueRefTemplateArgs[] = { ta, trueRefArg };
    CreateIsEqualSpecialization(context, isSameFalseTemplateDecl, tn, stdNamespace, trueBase, trueParamList, isSameTrueRefTemplateArgs);
  }
}

/// <summary>
/// Adds a new template type in the specified context with the given name. The record type will have a handle field.
/// </summary>
/// <parm name="context">AST context to which template will be added.</param>
/// <parm name="typeDecl">After execution, template declaration.</param>
/// <parm name="recordDecl">After execution, record declaration for template.</param>
/// <parm name="typeName">Name of template to create.</param>
/// <parm name="templateArgCount">Number of template arguments (one or two).</param>
/// <parm name="defaultTypeArgValue">If assigned, the default argument for the element template.</param>
void hlsl::AddTemplateTypeWithHandle(
  ASTContext& context,
  _Outptr_ ClassTemplateDecl** typeDecl,
  _Outptr_ CXXRecordDecl** recordDecl,
  _In_z_ const char* typeName,
  uint8_t templateArgCount, 
  _In_opt_ TypeSourceInfo* defaultTypeArgValue
)
{
  DXASSERT_NOMSG(typeDecl != nullptr);
  DXASSERT_NOMSG(recordDecl != nullptr);
  DXASSERT_NOMSG(typeName != nullptr);

  DXASSERT(templateArgCount != 0, "otherwise caller should be creating a class or struct");
  DXASSERT(templateArgCount <= 2, "otherwise the function needs to be updated for a different template pattern");

  DeclContext* currentDeclContext = context.getTranslationUnitDecl();

  // Create an object template declaration in translation unit scope.
  // templateArgCount=1: template<typename element> typeName { ... }
  // templateArgCount=2: template<typename element, int count> typeName { ... }
  IdentifierInfo& elementTemplateParamId = context.Idents.get(StringRef("element"), tok::TokenKind::identifier);
  TemplateTypeParmDecl *elementTemplateParamDecl = TemplateTypeParmDecl::Create(
    context, currentDeclContext, NoLoc, NoLoc,
    FirstTemplateDepth, FirstParamPosition, &elementTemplateParamId, TypenameFalse, ParameterPackFalse);
  QualType intType = context.IntTy;

  if (defaultTypeArgValue != nullptr)
  {
    elementTemplateParamDecl->setDefaultArgument(defaultTypeArgValue);
  }

  NonTypeTemplateParmDecl* countTemplateParamDecl = nullptr;
  if (templateArgCount > 1) {
    IdentifierInfo& countParamId = context.Idents.get(StringRef("count"), tok::TokenKind::identifier);
    countTemplateParamDecl = NonTypeTemplateParmDecl::Create(
      context, currentDeclContext, NoLoc, NoLoc,
      FirstTemplateDepth, FirstParamPosition + 1, &countParamId, intType, ParameterPackFalse, nullptr);
    // Zero means default here. The count is decided by runtime.
    Expr *literalIntZero = IntegerLiteral::Create(
        context, llvm::APInt(context.getIntWidth(intType), 0), intType, NoLoc);
    countTemplateParamDecl->setDefaultArgument(literalIntZero);
  }
  NamedDecl* templateParameters[] =
  {
    elementTemplateParamDecl, countTemplateParamDecl
  };
  TemplateParameterList* templateParameterList = TemplateParameterList::Create(
    context, NoLoc, NoLoc, templateParameters, templateArgCount, NoLoc);

  IdentifierInfo& typeId = context.Idents.get(StringRef(typeName), tok::TokenKind::identifier);
  CXXRecordDecl* templateRecordDecl = CXXRecordDecl::Create(
    context, TagDecl::TagKind::TTK_Class, currentDeclContext, NoLoc, NoLoc, &typeId,
    nullptr, DelayTypeCreationTrue);
  ClassTemplateDecl* classTemplateDecl = ClassTemplateDecl::Create(
    context, currentDeclContext, NoLoc, DeclarationName(&typeId),
    templateParameterList, templateRecordDecl, nullptr);
  templateRecordDecl->setDescribedClassTemplate(classTemplateDecl);
  templateRecordDecl->addAttr(FinalAttr::CreateImplicit(context, FinalAttr::Keyword_final));
  
  // Requesting the class name specialization will fault in required types.
  QualType T = classTemplateDecl->getInjectedClassNameSpecialization();
  T = context.getInjectedClassNameType(templateRecordDecl, T);
  assert(T->isDependentType() && "Class template type is not dependent?");
  classTemplateDecl->setLexicalDeclContext(currentDeclContext);
  templateRecordDecl->setLexicalDeclContext(currentDeclContext);
  templateRecordDecl->startDefinition();
  // Many more things to come here, like constructors and the like....

  // Add an 'h' field to hold the handle.
  QualType elementType = context.getTemplateTypeParmType(
      /*templateDepth*/ 0, 0, ParameterPackFalse, elementTemplateParamDecl);

  if (templateArgCount > 1 &&
      // Only need array type for inputpatch and outputpatch.
      // Avoid Texture2DMS which may use 0 count.
      // TODO: use hlsl types to do the check.
      !typeId.getName().startswith("Texture")) {
    Expr *countExpr = DeclRefExpr::Create(
        context, NestedNameSpecifierLoc(), NoLoc, countTemplateParamDecl, false,
        DeclarationNameInfo(countTemplateParamDecl->getDeclName(), NoLoc),
        intType, ExprValueKind::VK_RValue);

    elementType = context.getDependentSizedArrayType(
        elementType, countExpr, ArrayType::ArraySizeModifier::Normal, 0,
        SourceRange());

    // InputPatch and OutputPatch also have a "Length" static const member for the number of control points
    IdentifierInfo& lengthId = context.Idents.get(StringRef("Length"), tok::TokenKind::identifier);
    TypeSourceInfo* lengthTypeSource = context.getTrivialTypeSourceInfo(intType.withConst());
    VarDecl* lengthValueDecl = VarDecl::Create(context, templateRecordDecl, NoLoc, NoLoc, &lengthId,
      intType.withConst(), lengthTypeSource, SC_Static);
    lengthValueDecl->setInit(countExpr);
    lengthValueDecl->setAccess(AS_public);
    templateRecordDecl->addDecl(lengthValueDecl);
  }

  AddHLSLHandleField(context, templateRecordDecl, elementType);

  templateRecordDecl->completeDefinition();

  // Both declarations need to be present for correct handling.
  currentDeclContext->addDecl(classTemplateDecl);
  currentDeclContext->addDecl(templateRecordDecl);

#ifdef DBG
  // Verify that we can read the field member from the template record.
  DeclContext::lookup_result lookupResult = templateRecordDecl->lookup(
    DeclarationName(&context.Idents.get(StringRef("h"))));
  DXASSERT(!lookupResult.empty(), "otherwise template object handle cannot be looked up");
#endif

  *typeDecl = classTemplateDecl;
  *recordDecl = templateRecordDecl;
}

FunctionTemplateDecl* hlsl::CreateFunctionTemplateDecl(
  ASTContext& context,
  _In_ CXXRecordDecl* recordDecl,
  _In_ CXXMethodDecl* functionDecl,
  _In_count_(templateParamNamedDeclsCount) NamedDecl** templateParamNamedDecls,
  size_t templateParamNamedDeclsCount)
{
  DXASSERT_NOMSG(recordDecl != nullptr);
  DXASSERT_NOMSG(templateParamNamedDecls != nullptr);
  DXASSERT(templateParamNamedDeclsCount > 0, "otherwise caller shouldn't invoke this function");

  TemplateParameterList* templateParams = TemplateParameterList::Create(
    context, NoLoc, NoLoc, &templateParamNamedDecls[0], templateParamNamedDeclsCount, NoLoc);
  FunctionTemplateDecl* functionTemplate =
    FunctionTemplateDecl::Create(context, recordDecl, NoLoc, functionDecl->getDeclName(), templateParams, functionDecl);
  functionTemplate->setAccess(AccessSpecifier::AS_public);
  functionTemplate->setLexicalDeclContext(recordDecl);
  functionDecl->setDescribedFunctionTemplate(functionTemplate);
  recordDecl->addDecl(functionTemplate);

  return functionTemplate;
}

static
void AssociateParametersToFunctionPrototype(
  _In_ TypeSourceInfo* tinfo,
  _In_count_(numParams) ParmVarDecl** paramVarDecls,
  unsigned int numParams)
{
  FunctionProtoTypeLoc protoLoc = tinfo->getTypeLoc().getAs<FunctionProtoTypeLoc>();
  DXASSERT(protoLoc.getNumParams() == numParams, "otherwise unexpected number of parameters available");
  for (unsigned i = 0; i < numParams; i++) {
    DXASSERT(protoLoc.getParam(i) == nullptr, "otherwise prototype parameters were already initialized");
    protoLoc.setParam(i, paramVarDecls[i]);
  }
}

static void CreateConstructorDeclaration(
  ASTContext &context, _In_ CXXRecordDecl *recordDecl, QualType resultType,
  ArrayRef<QualType> args, DeclarationName declarationName, bool isConst,
  _Out_ CXXConstructorDecl **constructorDecl, _Out_ TypeSourceInfo **tinfo) {
  DXASSERT_NOMSG(recordDecl != nullptr);
  DXASSERT_NOMSG(constructorDecl != nullptr);

  FunctionProtoType::ExtProtoInfo functionExtInfo;
  functionExtInfo.TypeQuals = isConst ? Qualifiers::Const : 0;
  QualType functionQT = context.getFunctionType(
    resultType, args, functionExtInfo, ArrayRef<ParameterModifier>());
  DeclarationNameInfo declNameInfo(declarationName, NoLoc);
  *tinfo = context.getTrivialTypeSourceInfo(functionQT, NoLoc);
  DXASSERT_NOMSG(*tinfo != nullptr);
  *constructorDecl = CXXConstructorDecl::Create(
    context, recordDecl, NoLoc, declNameInfo, functionQT, *tinfo,
    StorageClass::SC_None, ExplicitFalse, InlineSpecifiedFalse, IsConstexprFalse);
  DXASSERT_NOMSG(*constructorDecl != nullptr);
  (*constructorDecl)->setLexicalDeclContext(recordDecl);
  (*constructorDecl)->setAccess(AccessSpecifier::AS_public);
}

static void CreateObjectFunctionDeclaration(
    ASTContext &context, _In_ CXXRecordDecl *recordDecl, QualType resultType,
    ArrayRef<QualType> args, DeclarationName declarationName, bool isConst,
    _Out_ CXXMethodDecl **functionDecl, _Out_ TypeSourceInfo **tinfo) {
  DXASSERT_NOMSG(recordDecl != nullptr);
  DXASSERT_NOMSG(functionDecl != nullptr);

  FunctionProtoType::ExtProtoInfo functionExtInfo;
  functionExtInfo.TypeQuals = isConst ? Qualifiers::Const : 0;
  QualType functionQT = context.getFunctionType(
      resultType, args, functionExtInfo, ArrayRef<ParameterModifier>());
  DeclarationNameInfo declNameInfo(declarationName, NoLoc);
  *tinfo = context.getTrivialTypeSourceInfo(functionQT, NoLoc);
  DXASSERT_NOMSG(*tinfo != nullptr);
  *functionDecl = CXXMethodDecl::Create(
      context, recordDecl, NoLoc, declNameInfo, functionQT, *tinfo,
      StorageClass::SC_None, InlineSpecifiedFalse, IsConstexprFalse, NoLoc);
  DXASSERT_NOMSG(*functionDecl != nullptr);
  (*functionDecl)->setLexicalDeclContext(recordDecl);
  (*functionDecl)->setAccess(AccessSpecifier::AS_public);
}

CXXMethodDecl* hlsl::CreateObjectFunctionDeclarationWithParams(
  ASTContext& context,
  _In_ CXXRecordDecl* recordDecl,
  QualType resultType,
  ArrayRef<QualType> paramTypes,
  ArrayRef<StringRef> paramNames,
  DeclarationName declarationName,
  bool isConst)
{
  DXASSERT_NOMSG(recordDecl != nullptr);
  DXASSERT_NOMSG(!resultType.isNull());
  DXASSERT_NOMSG(paramTypes.size() == paramNames.size());

  TypeSourceInfo* tinfo;
  CXXMethodDecl* functionDecl;
  CreateObjectFunctionDeclaration(context, recordDecl, resultType, paramTypes,
                                  declarationName, isConst, &functionDecl,
                                  &tinfo);

  // Create and associate parameters to method.
  SmallVector<ParmVarDecl *, 2> parmVarDecls;
  if (!paramTypes.empty()) {
    for (unsigned int i = 0; i < paramTypes.size(); ++i) {
      IdentifierInfo *argIi = &context.Idents.get(paramNames[i]);
      ParmVarDecl *parmVarDecl = ParmVarDecl::Create(
          context, functionDecl, NoLoc, NoLoc, argIi, paramTypes[i],
          context.getTrivialTypeSourceInfo(paramTypes[i], NoLoc),
          StorageClass::SC_None, nullptr);
      parmVarDecl->setScopeInfo(0, i);
      DXASSERT(parmVarDecl->getFunctionScopeIndex() == i,
               "otherwise failed to set correct index");
      parmVarDecls.push_back(parmVarDecl);
    }
    functionDecl->setParams(ArrayRef<ParmVarDecl *>(parmVarDecls));
    AssociateParametersToFunctionPrototype(tinfo, &parmVarDecls.front(),
                                           parmVarDecls.size());
  }

  recordDecl->addDecl(functionDecl);

  return functionDecl;
}

void hlsl::AddRayQueryTemplate(
  ASTContext& context,
  _Outptr_ ClassTemplateDecl** typeDecl,
  _Outptr_ CXXRecordDecl** recordDecl
)
{
  DXASSERT_NOMSG(typeDecl != nullptr);
  DXASSERT_NOMSG(recordDecl != nullptr);

  DeclContext* currentDeclContext = context.getTranslationUnitDecl();

  // Create a RayQuery template declaration in translation unit scope.
  // template<uint flags> RayQuery { ... }
  QualType uintType = context.UnsignedIntTy;

  NonTypeTemplateParmDecl* flagsTemplateParamDecl = nullptr;
  IdentifierInfo& countParamId = context.Idents.get(StringRef("flags"), tok::TokenKind::identifier);
  flagsTemplateParamDecl = NonTypeTemplateParmDecl::Create(
    context, currentDeclContext, NoLoc, NoLoc,
    FirstTemplateDepth, FirstParamPosition, &countParamId, uintType, ParameterPackFalse, nullptr);

  // Should flags default to zero?
  Expr *literalIntZero = IntegerLiteral::Create(
    context, llvm::APInt(context.getIntWidth(uintType), 0), uintType, NoLoc);
  flagsTemplateParamDecl->setDefaultArgument(literalIntZero);

  NamedDecl* templateParameters[] =
  {
    flagsTemplateParamDecl
  };
  TemplateParameterList* templateParameterList = TemplateParameterList::Create(
    context, NoLoc, NoLoc, templateParameters, 1, NoLoc);

  IdentifierInfo& typeId = context.Idents.get(StringRef("RayQuery"), tok::TokenKind::identifier);
  CXXRecordDecl* templateRecordDecl = CXXRecordDecl::Create(
    context, TagDecl::TagKind::TTK_Class, currentDeclContext, NoLoc, NoLoc, &typeId,
    nullptr, DelayTypeCreationTrue);
  ClassTemplateDecl* classTemplateDecl = ClassTemplateDecl::Create(
    context, currentDeclContext, NoLoc, DeclarationName(&typeId),
    templateParameterList, templateRecordDecl, nullptr);
  templateRecordDecl->setDescribedClassTemplate(classTemplateDecl);
  templateRecordDecl->addAttr(FinalAttr::CreateImplicit(context, FinalAttr::Keyword_final));

  // Requesting the class name specialization will fault in required types.
  QualType T = classTemplateDecl->getInjectedClassNameSpecialization();
  T = context.getInjectedClassNameType(templateRecordDecl, T);
  assert(T->isDependentType() && "Class template type is not dependent?");
  classTemplateDecl->setLexicalDeclContext(currentDeclContext);
  templateRecordDecl->setLexicalDeclContext(currentDeclContext);
  templateRecordDecl->startDefinition();

  // TODO: Add constructor that will be lowered to the intrinsic that produces
  // the RayQuery handle for this object.
  CanQualType canQualType = templateRecordDecl->getTypeForDecl()->getCanonicalTypeUnqualified();
  CXXConstructorDecl *pConstructorDecl = nullptr;
  TypeSourceInfo *pTypeSourceInfo = nullptr;
  CreateConstructorDeclaration(context, templateRecordDecl, context.VoidTy, {}, context.DeclarationNames.getCXXConstructorName(canQualType), false, &pConstructorDecl, &pTypeSourceInfo);
  templateRecordDecl->addDecl(pConstructorDecl);

  // Add an 'h' field to hold the handle.
  AddHLSLHandleField(context, templateRecordDecl, uintType);

  templateRecordDecl->completeDefinition();

  // Both declarations need to be present for correct handling.
  currentDeclContext->addDecl(classTemplateDecl);
  currentDeclContext->addDecl(templateRecordDecl);

#ifdef DBG
  // Verify that we can read the field member from the template record.
  DeclContext::lookup_result lookupResult = templateRecordDecl->lookup(
    DeclarationName(&context.Idents.get(StringRef("h"))));
  DXASSERT(!lookupResult.empty(), "otherwise template object handle cannot be looked up");
#endif

  *typeDecl = classTemplateDecl;
  *recordDecl = templateRecordDecl;
}

bool hlsl::IsIntrinsicOp(const clang::FunctionDecl *FD) {
  return FD != nullptr && FD->hasAttr<HLSLIntrinsicAttr>();
}

bool hlsl::GetIntrinsicOp(const clang::FunctionDecl *FD, unsigned &opcode,
                    llvm::StringRef &group) {
  if (FD == nullptr || !FD->hasAttr<HLSLIntrinsicAttr>()) {
    return false;
  }

  HLSLIntrinsicAttr *A = FD->getAttr<HLSLIntrinsicAttr>();
  opcode = A->getOpcode();
  group = A->getGroup();
  return true;
}

bool hlsl::GetIntrinsicLowering(const clang::FunctionDecl *FD, llvm::StringRef &S) {
  if (FD == nullptr || !FD->hasAttr<HLSLIntrinsicAttr>()) {
    return false;
  }

  HLSLIntrinsicAttr *A = FD->getAttr<HLSLIntrinsicAttr>();
  S = A->getLowering();
  return true;
}

/// <summary>Parses a column or row digit.</summary>
static
bool TryParseColOrRowChar(const char digit, _Out_ int* count) {
  if ('1' <= digit && digit <= '4') {
    *count = digit - '0';
    return true;
  }

  *count = 0;
  return false;
}

/// <summary>Parses a matrix shorthand identifier (eg, float3x2).</summary>
_Use_decl_annotations_
bool hlsl::TryParseMatrixShorthand(
  const char* typeName,
  size_t typeNameLen,
  HLSLScalarType* parsedType,
  int* rowCount,
  int* colCount,
  const clang::LangOptions& langOptions
)
{
  //
  // Matrix shorthand format is PrimitiveTypeRxC, where R is the row count and C is the column count.
  // R and C should be between 1 and 4 inclusive.
  // x is a literal 'x' character.
  // PrimitiveType is one of the HLSLScalarTypeNames values.
  //
  if (TryParseMatrixOrVectorDimension(typeName, typeNameLen, rowCount, colCount, langOptions) &&
    *rowCount != 0 && *colCount != 0) {
    // compare scalar component
    HLSLScalarType type = FindScalarTypeByName(typeName, typeNameLen-3, langOptions);
    if (type!= HLSLScalarType_unknown) {
      *parsedType = type;
      return true;
    }
  }
  // Unable to parse.
  return false;
}

/// <summary>Parses a vector shorthand identifier (eg, float3).</summary>
_Use_decl_annotations_
bool hlsl::TryParseVectorShorthand(
  const char* typeName,
  size_t typeNameLen,
  HLSLScalarType* parsedType,
  int* elementCount,
  const clang::LangOptions& langOptions
  )
{
  // At least *something*N characters necessary, where something is at least 'int'
  if (TryParseColOrRowChar(typeName[typeNameLen - 1], elementCount)) {
    // compare scalar component
    HLSLScalarType type = FindScalarTypeByName(typeName, typeNameLen-1, langOptions);
    if (type!= HLSLScalarType_unknown) {
      *parsedType = type;
      return true;
    }
  }
  // Unable to parse.
  return false;
}

/// <summary>Parses a hlsl scalar type (e.g min16float, uint3x4) </summary>
_Use_decl_annotations_
bool hlsl::TryParseScalar(
  _In_count_(typenameLen)
            const char* typeName,
            size_t typeNameLen,
  _Out_     HLSLScalarType *parsedType,
  _In_      const clang::LangOptions& langOptions) {
  HLSLScalarType type = FindScalarTypeByName(typeName, typeNameLen, langOptions);
  if (type!= HLSLScalarType_unknown) {
    *parsedType = type;
    return true;
  }
  return false; // unable to parse
}

/// <summary>Parse any (scalar, vector, matrix) hlsl types (e.g float, int3x4, uint2) </summary>
_Use_decl_annotations_
bool hlsl::TryParseAny(
  _In_count_(typenameLen)
  const char* typeName,
  size_t typeNameLen,
  _Out_ HLSLScalarType *parsedType,
  int *rowCount,
  int *colCount,
  _In_ const clang::LangOptions& langOptions) {
  // at least 'int'
  const size_t MinValidLen = 3;
  if (typeNameLen >= MinValidLen) {
    TryParseMatrixOrVectorDimension(typeName, typeNameLen, rowCount, colCount, langOptions);
    int suffixLen = *colCount == 0 ? 0 :
                    *rowCount == 0 ? 1 : 3;
    HLSLScalarType type = FindScalarTypeByName(typeName, typeNameLen-suffixLen, langOptions);
    if (type!= HLSLScalarType_unknown) {
      *parsedType = type;
      return true;
    }
  }
  return false;
}

/// <summary>Parse string hlsl type</summary>
_Use_decl_annotations_
bool hlsl::TryParseString(
  _In_count_(typenameLen)
  const char* typeName,
  size_t typeNameLen,
  _In_ const clang::LangOptions& langOptions) {

  if (typeNameLen == 6 && typeName[0] == 's' && strncmp(typeName, "string", 6) == 0) {
    return true;
  }
  return false;
}

/// <summary>Parse any kind of dimension for vector or matrix (e.g 4,3 in int4x3).
/// If it's a matrix type, rowCount and colCount will be nonzero. If it's a vector type, colCount is 0.
/// Otherwise both rowCount and colCount is 0. Returns true if either matrix or vector dimensions detected. </summary>
_Use_decl_annotations_
bool hlsl::TryParseMatrixOrVectorDimension(
    _In_count_(typeNameLen)
    const char *typeName,
    size_t typeNameLen,
    _Out_opt_ int *rowCount,
    _Out_opt_ int *colCount,
  _In_      const clang::LangOptions& langOptions) {
  *rowCount = 0;
  *colCount = 0;
  size_t MinValidLen = 3; // at least int
  if (typeNameLen > MinValidLen) {
    if (TryParseColOrRowChar(typeName[typeNameLen - 1], colCount)) {
      // Try parse matrix
      if (typeName[typeNameLen - 2] == 'x')
        TryParseColOrRowChar(typeName[typeNameLen - 3], rowCount);
      return true;
    }
  }
  return false;
}

/// <summary>Creates a typedef for a matrix shorthand (eg, float3x2).</summary>
TypedefDecl* hlsl::CreateMatrixSpecializationShorthand(
  ASTContext& context,
  QualType matrixSpecialization,
  HLSLScalarType scalarType,
  size_t rowCount,
  size_t colCount)
{
  DXASSERT(rowCount <= 4, "else caller didn't validate rowCount");
  DXASSERT(colCount <= 4, "else caller didn't validate colCount");
  char typeName[64];
  sprintf_s(typeName, _countof(typeName), "%s%ux%u",
    HLSLScalarTypeNames[scalarType], (unsigned)rowCount, (unsigned)colCount);
  IdentifierInfo& typedefId = context.Idents.get(StringRef(typeName), tok::TokenKind::identifier);
  DeclContext* currentDeclContext = context.getTranslationUnitDecl();
  TypedefDecl* decl = TypedefDecl::Create(context, currentDeclContext, NoLoc, NoLoc, &typedefId,
    context.getTrivialTypeSourceInfo(matrixSpecialization, NoLoc));
  decl->setImplicit(true);
  currentDeclContext->addDecl(decl);
  return decl;
}

/// <summary>Creates a typedef for a vector shorthand (eg, float3).</summary>
TypedefDecl* hlsl::CreateVectorSpecializationShorthand(
  ASTContext& context,
  QualType vectorSpecialization,
  HLSLScalarType scalarType,
  size_t colCount)
{
  DXASSERT(colCount <= 4, "else caller didn't validate colCount");
  char typeName[64];
  sprintf_s(typeName, _countof(typeName), "%s%u",
    HLSLScalarTypeNames[scalarType], (unsigned)colCount);
  IdentifierInfo& typedefId = context.Idents.get(StringRef(typeName), tok::TokenKind::identifier);
  DeclContext* currentDeclContext = context.getTranslationUnitDecl();
  TypedefDecl* decl = TypedefDecl::Create(context, currentDeclContext, NoLoc, NoLoc, &typedefId,
    context.getTrivialTypeSourceInfo(vectorSpecialization, NoLoc));
  decl->setImplicit(true);
  currentDeclContext->addDecl(decl);
  return decl;
}

llvm::ArrayRef<hlsl::UnusualAnnotation*>
hlsl::UnusualAnnotation::CopyToASTContextArray(
  clang::ASTContext& Context, hlsl::UnusualAnnotation** begin, size_t count) {
  if (count == 0) {
    return llvm::ArrayRef<hlsl::UnusualAnnotation*>();
  }

  UnusualAnnotation** arr = ::new (Context) UnusualAnnotation*[count];
  for (size_t i = 0; i < count; ++i) {
    arr[i] = begin[i]->CopyToASTContext(Context);
  }

  return llvm::ArrayRef<hlsl::UnusualAnnotation*>(arr, count);
}

UnusualAnnotation* hlsl::UnusualAnnotation::CopyToASTContext(ASTContext& Context) {
  // All UnusualAnnotation instances can be blitted.
  size_t instanceSize;
  switch (Kind) {
  case UA_RegisterAssignment:
    instanceSize = sizeof(hlsl::RegisterAssignment);
    break;
  case UA_ConstantPacking:
    instanceSize = sizeof(hlsl::ConstantPacking);
    break;
  default:
    DXASSERT(Kind == UA_SemanticDecl, "Kind == UA_SemanticDecl -- otherwise switch is incomplete");
    instanceSize = sizeof(hlsl::SemanticDecl);
    break;
  }

  void* result = Context.Allocate(instanceSize);
  memcpy(result, this, instanceSize);
  return (UnusualAnnotation*)result;
}

static bool HasTessFactorSemantic(const ValueDecl *decl) {
  for (const UnusualAnnotation *it : decl->getUnusualAnnotations()) {
    if (it->getKind() == UnusualAnnotation::UA_SemanticDecl) {
      const SemanticDecl *sd = cast<SemanticDecl>(it);
      const Semantic *pSemantic = Semantic::GetByName(sd->SemanticName);
      if (pSemantic && pSemantic->GetKind() == Semantic::Kind::TessFactor)
        return true;
    }
  }
  return false;
}

static bool HasTessFactorSemanticRecurse(const ValueDecl *decl, QualType Ty) {
  if (Ty->isBuiltinType() || hlsl::IsHLSLVecMatType(Ty))
    return false;

  if (const RecordType *RT = Ty->getAsStructureType()) {
    RecordDecl *RD = RT->getDecl();
    for (FieldDecl *fieldDecl : RD->fields()) {
      if (HasTessFactorSemanticRecurse(fieldDecl, fieldDecl->getType()))
        return true;
    }
    return false;
  }

  if (Ty->getAsArrayTypeUnsafe())
    return HasTessFactorSemantic(decl);

  return false;
}

bool ASTContext::IsPatchConstantFunctionDecl(const FunctionDecl *FD) const {
  // This checks whether the function is structurally capable of being a patch
  // constant function, not whether it is in fact the patch constant function
  // for the entry point of a compiled hull shader (which may not have been
  // seen yet). So the answer is conservative.
  if (!FD->getReturnType()->isVoidType()) {
    // Try to find TessFactor in return type.
    if (HasTessFactorSemanticRecurse(FD, FD->getReturnType()))
      return true;
  }
  // Try to find TessFactor in out param.
  for (const ParmVarDecl *param : FD->params()) {
    if (param->hasAttr<HLSLOutAttr>()) {
      if (HasTessFactorSemanticRecurse(param, param->getType()))
        return true;
    }
  }
  return false;
}
