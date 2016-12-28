//===--- PPCallbacks.cpp - Callbacks for Preprocessor actions ---*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// PPCallbacks.cpp                                                           //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "clang/Lex/PPCallbacks.h"

using namespace clang;

void PPChainedCallbacks::anchor() { }
