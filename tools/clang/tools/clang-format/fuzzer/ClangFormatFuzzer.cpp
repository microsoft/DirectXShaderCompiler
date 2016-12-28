//===-- ClangFormatFuzzer.cpp - Fuzz the Clang format tool ----------------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// ClangFormatFuzzer.cpp                                                     //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///
/// \file                                                                    //
/// \brief This file implements a function that runs Clang format on a single//
///  input. This function is then linked into the Fuzzer library.            //
///
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "clang/Format/Format.h"

extern "C" void LLVMFuzzerTestOneInput(uint8_t *data, size_t size) {
  // FIXME: fuzz more things: different styles, different style features.
  std::string s((const char *)data, size);
  auto Style = getGoogleStyle(clang::format::FormatStyle::LK_Cpp);
  Style.ColumnLimit = 60;
  applyAllReplacements(s, clang::format::reformat(
                              Style, s, {clang::tooling::Range(0, s.size())}));
}
