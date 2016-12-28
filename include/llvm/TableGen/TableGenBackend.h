//===- llvm/TableGen/TableGenBackend.h - Backend utilities ------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// TableGenBackend.h                                                         //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// Useful utilities for TableGen backends.                                   //
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_TABLEGEN_TABLEGENBACKEND_H
#define LLVM_TABLEGEN_TABLEGENBACKEND_H

namespace llvm {

class StringRef;
class raw_ostream;

/// emitSourceFileHeader - Output an LLVM style file header to the specified
/// raw_ostream.
void emitSourceFileHeader(StringRef Desc, raw_ostream &OS);

} // End llvm namespace

#endif
