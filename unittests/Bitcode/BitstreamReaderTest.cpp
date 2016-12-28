//===- BitstreamReaderTest.cpp - Tests for BitstreamReader ----------------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// BitstreamReaderTest.cpp                                                   //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/Bitcode/BitstreamReader.h"
#include "gtest/gtest.h"

using namespace llvm;

namespace {

TEST(BitstreamReaderTest, AtEndOfStream) {
  uint8_t Bytes[4] = {
    0x00, 0x01, 0x02, 0x03
  };
  BitstreamReader Reader(std::begin(Bytes), std::end(Bytes));
  BitstreamCursor Cursor(Reader);

  EXPECT_FALSE(Cursor.AtEndOfStream());
  (void)Cursor.Read(8);
  EXPECT_FALSE(Cursor.AtEndOfStream());
  (void)Cursor.Read(24);
  EXPECT_TRUE(Cursor.AtEndOfStream());

  Cursor.JumpToBit(0);
  EXPECT_FALSE(Cursor.AtEndOfStream());

  Cursor.JumpToBit(32);
  EXPECT_TRUE(Cursor.AtEndOfStream());
}

TEST(BitstreamReaderTest, AtEndOfStreamJump) {
  uint8_t Bytes[4] = {
    0x00, 0x01, 0x02, 0x03
  };
  BitstreamReader Reader(std::begin(Bytes), std::end(Bytes));
  BitstreamCursor Cursor(Reader);

  Cursor.JumpToBit(32);
  EXPECT_TRUE(Cursor.AtEndOfStream());
}

TEST(BitstreamReaderTest, AtEndOfStreamEmpty) {
  uint8_t Dummy = 0xFF;
  BitstreamReader Reader(&Dummy, &Dummy);
  BitstreamCursor Cursor(Reader);

  EXPECT_TRUE(Cursor.AtEndOfStream());
}

} // end anonymous namespace
