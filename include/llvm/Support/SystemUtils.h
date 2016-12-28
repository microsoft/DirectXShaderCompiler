//===- SystemUtils.h - Utilities to do low-level system stuff ---*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// SystemUtils.h                                                             //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file contains functions used to do a variety of low-level, often     //
// system-specific, tasks.                                                   //
//
///////////////////////////////////////////////////////////////////////////////

#ifndef LLVM_SUPPORT_SYSTEMUTILS_H
#define LLVM_SUPPORT_SYSTEMUTILS_H

namespace llvm {
  class raw_ostream;

/// Determine if the raw_ostream provided is connected to a terminal. If so,
/// generate a warning message to errs() advising against display of bitcode
/// and return true. Otherwise just return false.
/// @brief Check for output written to a console
bool CheckBitcodeOutputToConsole(
  raw_ostream &stream_to_check, ///< The stream to be checked
  bool print_warning = true     ///< Control whether warnings are printed
);

} // End llvm namespace

#endif
