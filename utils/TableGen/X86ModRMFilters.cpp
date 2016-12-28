//===- X86ModRMFilters.cpp - Disassembler ModR/M filterss -------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// X86ModRMFilters.cpp                                                       //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "X86ModRMFilters.h"

using namespace llvm::X86Disassembler;

void ModRMFilter::anchor() { }

void DumbFilter::anchor() { }

void ModFilter::anchor() { }

void ExtendedFilter::anchor() { }

void ExactFilter::anchor() { }
