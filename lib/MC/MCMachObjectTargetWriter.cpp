//===-- MCMachObjectTargetWriter.cpp - Mach-O Target Writer Subclass ------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// MCMachObjectTargetWriter.cpp                                              //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/MC/MCMachObjectWriter.h"

using namespace llvm;

MCMachObjectTargetWriter::MCMachObjectTargetWriter(bool Is64Bit_,
                                                   uint32_t CPUType_,
                                                   uint32_t CPUSubtype_)
    : Is64Bit(Is64Bit_), CPUType(CPUType_), CPUSubtype(CPUSubtype_) {}

MCMachObjectTargetWriter::~MCMachObjectTargetWriter() {}
