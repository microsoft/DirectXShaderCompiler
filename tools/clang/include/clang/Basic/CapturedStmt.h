//===--- CapturedStmt.h - Types for CapturedStmts ---------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// CapturedStmt.h                                                            //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////


#ifndef LLVM_CLANG_BASIC_CAPTUREDSTMT_H
#define LLVM_CLANG_BASIC_CAPTUREDSTMT_H

namespace clang {

/// \brief The different kinds of captured statement.
enum CapturedRegionKind {
  CR_Default,
  CR_OpenMP
};

} // end namespace clang

#endif // LLVM_CLANG_BASIC_CAPTUREDSTMT_H
