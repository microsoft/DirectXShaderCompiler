//===- llvm/Support/Unix/COM.inc - Unix COM Implementation -----*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// COM.inc                                                                   //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file implements the Unix portion of COM support.                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////
//=== WARNING: Implementation here must contain only generic UNIX code that  //
//===          is guaranteed to work on *all* UNIX variants.                 //

namespace llvm {
namespace sys {

InitializeCOMRAII::InitializeCOMRAII(COMThreadingMode Threading,
                                     bool SpeedOverMemory) {}

InitializeCOMRAII::~InitializeCOMRAII() {}
}
}
