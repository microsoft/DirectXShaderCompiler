//===- ExpressionTraits.h - C++ Expression Traits Support Enums -*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// ExpressionTraits.h                                                        //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///
/// \file                                                                    //
/// \brief Defines enumerations for expression traits intrinsics.            //
///
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_CLANG_BASIC_EXPRESSIONTRAITS_H
#define LLVM_CLANG_BASIC_EXPRESSIONTRAITS_H

namespace clang {

  enum ExpressionTrait {
    ET_IsLValueExpr,
    ET_IsRValueExpr
  };
}

#endif
