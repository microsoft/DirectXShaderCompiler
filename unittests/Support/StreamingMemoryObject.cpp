//===- llvm/unittest/Support/StreamingMemoryObject.cpp - unit tests -------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// StreamingMemoryObject.cpp                                                 //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/StreamingMemoryObject.h"
#include "gtest/gtest.h"
#include <string.h>

using namespace llvm;

namespace {
class NullDataStreamer : public DataStreamer {
  size_t GetBytes(unsigned char *buf, size_t len) override {
    memset(buf, 0, len);
    return len;
  }
};
}

TEST(StreamingMemoryObject, Test) {
  auto DS = make_unique<NullDataStreamer>();
  StreamingMemoryObject O(std::move(DS));
  EXPECT_TRUE(O.isValidAddress(32 * 1024));
}

TEST(StreamingMemoryObject, TestSetKnownObjectSize) {
  auto DS = make_unique<NullDataStreamer>();
  StreamingMemoryObject O(std::move(DS));
  uint8_t Buf[32];
  EXPECT_EQ((uint64_t) 16, O.readBytes(Buf, 16, 0));
  O.setKnownObjectSize(24);
  EXPECT_EQ((uint64_t) 8, O.readBytes(Buf, 16, 16));
}
