//== ConstraintManager.cpp - Constraints on symbolic values -----*- C++ -*--==//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// ConstraintManager.cpp                                                     //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
//  This file defined the interface to manage constraints on symbolic values.//
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "clang/StaticAnalyzer/Core/PathSensitive/ProgramState.h"

using namespace clang;
using namespace ento;

ConstraintManager::~ConstraintManager() {}

static DefinedSVal getLocFromSymbol(const ProgramStateRef &State,
                                    SymbolRef Sym) {
  const MemRegion *R = State->getStateManager().getRegionManager()
                                               .getSymbolicRegion(Sym);
  return loc::MemRegionVal(R);
}

ConditionTruthVal ConstraintManager::checkNull(ProgramStateRef State,
                                               SymbolRef Sym) {  
  QualType Ty = Sym->getType();
  DefinedSVal V = Loc::isLocType(Ty) ? getLocFromSymbol(State, Sym)
                                     : nonloc::SymbolVal(Sym);
  const ProgramStatePair &P = assumeDual(State, V);
  if (P.first && !P.second)
    return ConditionTruthVal(false);
  if (!P.first && P.second)
    return ConditionTruthVal(true);
  return ConditionTruthVal();
}
