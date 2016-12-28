//== TaintTag.h - Path-sensitive "State" for tracking values -*- C++ -*--=//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// TaintTag.h                                                                //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// Defines a set of taint tags. Several tags are used to differentiate kinds //
// of taint.                                                                 //
//
///////////////////////////////////////////////////////////////////////////////
#ifndef LLVM_CLANG_STATICANALYZER_CORE_PATHSENSITIVE_TAINTTAG_H
#define LLVM_CLANG_STATICANALYZER_CORE_PATHSENSITIVE_TAINTTAG_H

namespace clang {
namespace ento {

/// The type of taint, which helps to differentiate between different types of
/// taint.
typedef unsigned TaintTagType;
static const TaintTagType TaintTagGeneric = 0;

}}

#endif
