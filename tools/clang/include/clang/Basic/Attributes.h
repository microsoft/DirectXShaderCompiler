//===--- Attributes.h - Attributes header -----------------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// Attributes.h                                                              //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_CLANG_BASIC_ATTRIBUTES_H
#define LLVM_CLANG_BASIC_ATTRIBUTES_H

#include "clang/Basic/LangOptions.h"
#include "llvm/ADT/Triple.h"

namespace clang {

class IdentifierInfo;

enum class AttrSyntax {
  /// Is the identifier known as a GNU-style attribute?
  GNU,
  /// Is the identifier known as a __declspec-style attribute?
  Declspec,
  // Is the identifier known as a C++-style attribute?
  CXX,
  // Is the identifier known as a pragma attribute?
  Pragma
};

/// \brief Return the version number associated with the attribute if we
/// recognize and implement the attribute specified by the given information.
int hasAttribute(AttrSyntax Syntax, const IdentifierInfo *Scope,
                 const IdentifierInfo *Attr, const llvm::Triple &T,
                 const LangOptions &LangOpts);

} // end namespace clang

#endif // LLVM_CLANG_BASIC_ATTRIBUTES_H
