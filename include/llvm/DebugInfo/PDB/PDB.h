//===- PDB.h - base header file for creating a PDB reader -------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// PDB.h                                                                     //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_DEBUGINFO_PDB_PDB_H
#define LLVM_DEBUGINFO_PDB_PDB_H

#include "PDBTypes.h"
#include <memory>

namespace llvm {
class StringRef;

PDB_ErrorCode loadDataForPDB(PDB_ReaderType Type, StringRef Path,
                             std::unique_ptr<IPDBSession> &Session);

PDB_ErrorCode loadDataForEXE(PDB_ReaderType Type, StringRef Path,
                             std::unique_ptr<IPDBSession> &Session);
}

#endif
