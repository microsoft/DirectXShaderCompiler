//===- llvm/Support/Unix/Host.inc -------------------------------*- C++ -*-===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// Host.inc                                                                  //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This file implements the UNIX Host support.                               //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////
//=== WARNING: Implementation here must contain only generic UNIX code that  //
//===          is guaranteed to work on *all* UNIX variants.                 //

#include "Unix.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Config/config.h"
#include <cctype>
#include <string>
#include <sys/utsname.h>

using namespace llvm;

static std::string getOSVersion() {
  struct utsname info;

  if (uname(&info))
    return "";

  return info.release;
}

std::string sys::getDefaultTargetTriple() {
  std::string TargetTripleString(LLVM_DEFAULT_TARGET_TRIPLE);

  // On darwin, we want to update the version to match that of the
  // target.
  std::string::size_type DarwinDashIdx = TargetTripleString.find("-darwin");
  if (DarwinDashIdx != std::string::npos) {
    TargetTripleString.resize(DarwinDashIdx + strlen("-darwin"));
    TargetTripleString += getOSVersion();
  }

  return Triple::normalize(TargetTripleString);
}
