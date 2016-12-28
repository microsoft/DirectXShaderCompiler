//===--- AttrImpl.cpp - Classes for representing attributes -----*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// AttrImpl.cpp                                                              //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
//  This file contains out-of-line methods for Attr classes.                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "clang/AST/Attr.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Expr.h"
#include "clang/AST/Type.h"
#include "llvm/ADT/StringSwitch.h"
using namespace clang;

#include "clang/AST/AttrImpl.inc"
