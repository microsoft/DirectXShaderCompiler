//===- FuzzerSanitizerOptions.cpp - default flags for sanitizers ----------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// FuzzerSanitizerOptions.cpp                                                //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// Set default options for sanitizers while running the fuzzer.              //
// Options reside in a separate file, so if we don't want to set the default //
// options we simply do not link this file in.                               //
// ASAN options:                                                             //
//   * don't dump the coverage to disk.                                      //
//   * enable coverage by default.                                           //
//   * enable handle_abort.                                                  //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

extern "C" const char *__asan_default_options() {
  return "coverage_pcs=0:coverage=1:handle_abort=1";
}
