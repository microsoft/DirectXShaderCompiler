//===----- CGHLSLRuntime.cpp - Interface to HLSL Runtime ----------------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// CGHLSLRuntime.cpp                                                         //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
//  This provides a class for HLSL code generation.                          //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "clang/AST/ASTContext.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/HlslTypes.h"
#include "clang/AST/Type.h"

#include "CGHLSLRuntime.h"

using namespace clang;
using namespace CodeGen;

namespace hlsl {
/// <summary>Try to convert HLSL template vector/matrix type to
/// ExtVectorType.</summary>
const clang::ExtVectorType *
ConvertHLSLVecMatTypeToExtVectorType(const clang::ASTContext &context,
                                     clang::QualType type) {
  return CodeGen::CGHLSLRuntime::ConvertHLSLVecMatTypeToExtVectorType(context,
                                                                      type);
}

bool IsHLSLVecMatType(clang::QualType type) {
  return CodeGen::CGHLSLRuntime::IsHLSLVecMatType(type);
}

bool IsHLSLMatType(clang::QualType type) {
  const clang::Type *Ty = type.getCanonicalType().getTypePtr();
  if (const RecordType *RT = dyn_cast<RecordType>(Ty)) {
    if (const ClassTemplateSpecializationDecl *templateDecl =
            dyn_cast<ClassTemplateSpecializationDecl>(RT->getDecl())) {
      if (templateDecl->getName() == "matrix") {
        return true;
      }
    }
  }
  return false;
}

bool IsHLSLVecType(clang::QualType type) {
  const clang::Type *Ty = type.getCanonicalType().getTypePtr();
  if (const RecordType *RT = dyn_cast<RecordType>(Ty)) {
    if (const ClassTemplateSpecializationDecl *templateDecl =
            dyn_cast<ClassTemplateSpecializationDecl>(RT->getDecl())) {
      if (templateDecl->getName() == "vector") {
        return true;
      }
    }
  }
  return false;
}

}

CGHLSLRuntime::~CGHLSLRuntime() {}

bool CGHLSLRuntime::IsHLSLVecMatType(clang::QualType &type) {
  const Type *Ty = type.getCanonicalType().getTypePtr();
  if (const RecordType *RT = dyn_cast<RecordType>(Ty)) {
    if (const ClassTemplateSpecializationDecl *templateDecl =
            dyn_cast<ClassTemplateSpecializationDecl>(RT->getDecl())) {
      if (templateDecl->getName() == "vector") {
        return true;
      } else if (templateDecl->getName() == "matrix") {
        return true;
      }
    }
  }
  return false;
}

const clang::ExtVectorType *CGHLSLRuntime::ConvertHLSLVecMatTypeToExtVectorType(
    const clang::ASTContext &context, clang::QualType &type) {
  const Type *Ty = type.getCanonicalType().getTypePtr();

  if (const RecordType *RT = dyn_cast<RecordType>(Ty)) {
    if (const ClassTemplateSpecializationDecl *templateDecl =
            dyn_cast<ClassTemplateSpecializationDecl>(RT->getDecl())) {
      // TODO: check pointer instead of name
      if (templateDecl->getName() == "vector") {
        const TemplateArgumentList &argList = templateDecl->getTemplateArgs();
        const TemplateArgument &arg0 = argList[0];
        const TemplateArgument &arg1 = argList[1];
        QualType elemTy = arg0.getAsType();
        llvm::APSInt elmSize = arg1.getAsIntegral();
        return context.getExtVectorType(elemTy, elmSize.getLimitedValue())
            ->getAs<ExtVectorType>();
      }
    }
  }
  return nullptr;
}

QualType CGHLSLRuntime::GetHLSLVecMatElementType(QualType type) {
  const Type *Ty = type.getCanonicalType().getTypePtr();
  // TODO: check isVecMatrix
  const RecordType *RT = cast<RecordType>(Ty);
  const ClassTemplateSpecializationDecl *templateDecl =
            cast<ClassTemplateSpecializationDecl>(RT->getDecl());
  const TemplateArgumentList &argList = templateDecl->getTemplateArgs();
  const TemplateArgument &arg0 = argList[0];
  return arg0.getAsType();
}
