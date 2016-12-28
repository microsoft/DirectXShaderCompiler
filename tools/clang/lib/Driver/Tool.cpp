//===--- Tool.cpp - Compilation Tools -------------------------------------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// Tool.cpp                                                                  //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "clang/Driver/Tool.h"

using namespace clang::driver;

Tool::Tool(const char *_Name, const char *_ShortName, const ToolChain &TC,
           ResponseFileSupport _ResponseSupport,
           llvm::sys::WindowsEncodingMethod _ResponseEncoding,
           const char *_ResponseFlag)
    : Name(_Name), ShortName(_ShortName), TheToolChain(TC),
      ResponseSupport(_ResponseSupport), ResponseEncoding(_ResponseEncoding),
      ResponseFlag(_ResponseFlag) {}

Tool::~Tool() {
}
