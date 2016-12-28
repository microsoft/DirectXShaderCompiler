//===--- OperatorPrecedence.h - Operator precedence levels ------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// OperatorPrecedence.h                                                      //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///
/// \file                                                                    //
/// \brief Defines and computes precedence levels for binary/ternary operators.//
///
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_CLANG_BASIC_OPERATORPRECEDENCE_H
#define LLVM_CLANG_BASIC_OPERATORPRECEDENCE_H

#include "clang/Basic/TokenKinds.h"

namespace clang {

/// PrecedenceLevels - These are precedences for the binary/ternary
/// operators in the C99 grammar.  These have been named to relate
/// with the C99 grammar productions.  Low precedences numbers bind
/// more weakly than high numbers.
namespace prec {
  enum Level {
    Unknown         = 0,    // Not binary operator.
    Comma           = 1,    // ,
    Assignment      = 2,    // =, *=, /=, %=, +=, -=, <<=, >>=, &=, ^=, |=
    Conditional     = 3,    // ?
    LogicalOr       = 4,    // ||
    LogicalAnd      = 5,    // &&
    InclusiveOr     = 6,    // |
    ExclusiveOr     = 7,    // ^
    And             = 8,    // &
    Equality        = 9,    // ==, !=
    Relational      = 10,   //  >=, <=, >, <
    Shift           = 11,   // <<, >>
    Additive        = 12,   // -, +
    Multiplicative  = 13,   // *, /, %
    PointerToMember = 14    // .*, ->*
  };
}

/// \brief Return the precedence of the specified binary operator token.
prec::Level getBinOpPrecedence(tok::TokenKind Kind, bool GreaterThanIsOperator,
                               bool CPlusPlus11);

}  // end namespace clang

#endif  // LLVM_CLANG_OPERATOR_PRECEDENCE_H
