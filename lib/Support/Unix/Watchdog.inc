//===--- Unix/Watchdog.inc - Unix Watchdog Implementation -------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// Watchdog.inc                                                              //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file provides the generic Unix implementation of the Watchdog class. //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

namespace llvm {
  namespace sys {
    Watchdog::Watchdog(unsigned int seconds) {
#ifdef HAVE_UNISTD_H
      alarm(seconds);
#endif
    }

    Watchdog::~Watchdog() {
#ifdef HAVE_UNISTD_H
      alarm(0);
#endif
    }
  }
}
