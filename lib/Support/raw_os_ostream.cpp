//===--- raw_os_ostream.cpp - Implement the raw_os_ostream class ----------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// raw_os_ostream.cpp                                                        //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
// This implements support adapting raw_ostream to std::ostream.             //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/Support/raw_os_ostream.h"
#include <ostream>
using namespace llvm;

//===----------------------------------------------------------------------===//
//  raw_os_ostream
//===----------------------------------------------------------------------===//

raw_os_ostream::~raw_os_ostream() {
  flush();
}

void raw_os_ostream::write_impl(const char *Ptr, size_t Size) {
  OS.write(Ptr, Size);
}

uint64_t raw_os_ostream::current_pos() const { return OS.tellp(); }
