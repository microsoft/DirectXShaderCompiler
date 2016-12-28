//=- Dominators.cpp - Implementation of dominators tree for Clang CFG C++ -*-=//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// Dominators.cpp                                                            //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "clang/Analysis/Analyses/Dominators.h"

using namespace clang;

void DominatorTree::anchor() { }
