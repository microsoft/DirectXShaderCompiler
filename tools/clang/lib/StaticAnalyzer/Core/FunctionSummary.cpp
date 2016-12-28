//== FunctionSummary.cpp - Stores summaries of functions. ----------*- C++ -*-//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// FunctionSummary.cpp                                                       //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file defines a summary of a function gathered/used by static analysis.//
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "clang/StaticAnalyzer/Core/PathSensitive/FunctionSummary.h"
using namespace clang;
using namespace ento;

unsigned FunctionSummariesTy::getTotalNumBasicBlocks() {
  unsigned Total = 0;
  for (MapTy::iterator I = Map.begin(), E = Map.end(); I != E; ++I) {
    Total += I->second.TotalBasicBlocks;
  }
  return Total;
}

unsigned FunctionSummariesTy::getTotalNumVisitedBasicBlocks() {
  unsigned Total = 0;
  for (MapTy::iterator I = Map.begin(), E = Map.end(); I != E; ++I) {
    Total += I->second.VisitedBasicBlocks.count();
  }
  return Total;
}
