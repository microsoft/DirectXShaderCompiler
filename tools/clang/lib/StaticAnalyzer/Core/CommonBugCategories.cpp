//=--- CommonBugCategories.cpp - Provides common issue categories -*- C++ -*-=//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// CommonBugCategories.cpp                                                   //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "clang/StaticAnalyzer/Core/BugReporter/CommonBugCategories.h"

// Common strings used for the "category" of many static analyzer issues.
namespace clang { namespace ento { namespace categories {

const char * const CoreFoundationObjectiveC = "Core Foundation/Objective-C";
const char * const LogicError = "Logic error";
const char * const MemoryCoreFoundationObjectiveC =
  "Memory (Core Foundation/Objective-C)";
const char * const UnixAPI = "Unix API";
}}}
