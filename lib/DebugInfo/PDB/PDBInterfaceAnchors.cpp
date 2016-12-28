//===- PDBInterfaceAnchors.h - defines class anchor funcions ----*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// PDBInterfaceAnchors.cpp                                                   //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// Class anchors are necessary per the LLVM Coding style guide, to ensure that//
// the vtable is only generated in this object file, and not in every object //
// file that incldues the corresponding header.                              //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/DebugInfo/PDB/IPDBDataStream.h"
#include "llvm/DebugInfo/PDB/IPDBLineNumber.h"
#include "llvm/DebugInfo/PDB/IPDBRawSymbol.h"
#include "llvm/DebugInfo/PDB/IPDBSession.h"
#include "llvm/DebugInfo/PDB/IPDBRawSymbol.h"

using namespace llvm;

IPDBSession::~IPDBSession() {}

IPDBDataStream::~IPDBDataStream() {}

IPDBRawSymbol::~IPDBRawSymbol() {}

IPDBLineNumber::~IPDBLineNumber() {}
