//===-- Comdat.cpp - Implement Metadata classes --------------------------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// Comdat.cpp                                                                //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file implements the Comdat class.                                    //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/IR/Comdat.h"
#include "llvm/ADT/StringMap.h"
using namespace llvm;

Comdat::Comdat(SelectionKind SK, StringMapEntry<Comdat> *Name)
    : Name(Name), SK(SK) {}

Comdat::Comdat(Comdat &&C) : Name(C.Name), SK(C.SK) {}

Comdat::Comdat() : Name(nullptr), SK(Comdat::Any) {}

StringRef Comdat::getName() const { return Name->first(); }
