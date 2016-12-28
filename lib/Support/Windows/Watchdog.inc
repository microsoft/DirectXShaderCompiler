//===--- Windows/Watchdog.inc - Windows Watchdog Implementation -*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// Watchdog.inc                                                              //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file provides the generic Windows implementation of the Watchdog class.//
//                                                                           //
///////////////////////////////////////////////////////////////////////////////
// TODO: implement.                                                          //
// Currently this is only used by PrettyStackTrace which is also unimplemented//
// on Windows. Roughly, a Windows implementation would use CreateWaitableTimer//
// and a second thread to run the TimerAPCProc.                              //

namespace llvm {
  namespace sys {
    Watchdog::Watchdog(unsigned int seconds) {}
    Watchdog::~Watchdog() {}
  }
}
