//===- FuzzerMain.cpp - main() function and flags -------------------------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// FuzzerMain.cpp                                                            //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// main() and flags.                                                         //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "FuzzerInterface.h"
#include "FuzzerInternal.h"

// This function should be defined by the user.
extern "C" void LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size);

int main(int argc, char **argv) {
  return fuzzer::FuzzerDriver(argc, argv, LLVMFuzzerTestOneInput);
}
