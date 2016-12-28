//===--- DeclGroup.cpp - Classes for representing groups of Decls -*- C++ -*-==//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DeclGroup.cpp                                                             //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
//  This file defines the DeclGroup and DeclGroupRef classes.                //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "clang/AST/DeclGroup.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "llvm/Support/Allocator.h"
using namespace clang;

DeclGroup* DeclGroup::Create(ASTContext &C, Decl **Decls, unsigned NumDecls) {
  static_assert(sizeof(DeclGroup) % llvm::AlignOf<void *>::Alignment == 0,
                "Trailing data is unaligned!");
  assert(NumDecls > 1 && "Invalid DeclGroup");
  unsigned Size = sizeof(DeclGroup) + sizeof(Decl*) * NumDecls;
  void* Mem = C.Allocate(Size, llvm::AlignOf<DeclGroup>::Alignment);
  new (Mem) DeclGroup(NumDecls, Decls);
  return static_cast<DeclGroup*>(Mem);
}

DeclGroup::DeclGroup(unsigned numdecls, Decl** decls) : NumDecls(numdecls) {
  assert(numdecls > 0);
  assert(decls);
  memcpy(this+1, decls, numdecls * sizeof(*decls));
}
