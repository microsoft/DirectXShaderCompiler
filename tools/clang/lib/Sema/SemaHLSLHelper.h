///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// SemaHLSLHelper.h                                                          //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
//  This file contains helper functions for HLSL semantic support.           //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#pragma once
namespace clang {
class CXXMethodDecl;
class Sema;
} // namespace clang

namespace hlsl {
enum class IntrinsicOp;
bool isIllegalIntrinsic(const clang::CXXMethodDecl *MD, IntrinsicOp opCode,
                        clang::Sema *sema);
} // namespace hlsl
