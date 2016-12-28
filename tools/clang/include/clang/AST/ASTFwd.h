//===--- ASTFwd.h ----------------------------------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// ASTFwd.h                                                                  //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// \file
// \brief Forward declaration of all AST node types.
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_CLANG_AST_ASTFWD_H
#define LLVM_CLANG_AST_ASTFWD_H

namespace clang {

class Decl;
#define DECL(DERIVED, BASE) class DERIVED##Decl;
#include "clang/AST/DeclNodes.inc"
class Stmt;
#define STMT(DERIVED, BASE) class DERIVED;
#include "clang/AST/StmtNodes.inc"
class Type;
#define TYPE(DERIVED, BASE) class DERIVED##Type;
#include "clang/AST/TypeNodes.def"
class CXXCtorInitializer;

} // end namespace clang

#endif
