//===--- ASTConsumer.cpp - Abstract interface for reading ASTs --*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// ASTConsumer.cpp                                                           //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
//  This file defines the ASTConsumer class.                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "clang/AST/ASTConsumer.h"
#include "llvm/Bitcode/BitstreamReader.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclGroup.h"
using namespace clang;

bool ASTConsumer::HandleTopLevelDecl(DeclGroupRef D) {
  return true;
}

void ASTConsumer::HandleInterestingDecl(DeclGroupRef D) {
  HandleTopLevelDecl(D);
}

void ASTConsumer::HandleTopLevelDeclInObjCContainer(DeclGroupRef D) {}

void ASTConsumer::HandleImplicitImportDecl(ImportDecl *D) {
  HandleTopLevelDecl(DeclGroupRef(D));
}
