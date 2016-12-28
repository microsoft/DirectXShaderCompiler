//===- unittests/Rewrite/RewriteBufferTest.cpp - RewriteBuffer tests ------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// RewriteBufferTest.cpp                                                     //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "clang/Rewrite/Core/RewriteBuffer.h"
#include "gtest/gtest.h"

using namespace llvm;
using namespace clang;

namespace {

static void tagRange(unsigned Offset, unsigned Len, StringRef tagName,
                     RewriteBuffer &Buf) {
  std::string BeginTag;
  raw_string_ostream(BeginTag) << '<' << tagName << '>';
  std::string EndTag;
  raw_string_ostream(EndTag) << "</" << tagName << '>';

  Buf.InsertTextAfter(Offset, BeginTag);
  Buf.InsertTextBefore(Offset+Len, EndTag);
}

TEST(RewriteBuffer, TagRanges) {
  StringRef Input = "hello world";
  const char *Output = "<outer><inner>hello</inner></outer> ";

  RewriteBuffer Buf;
  Buf.Initialize(Input);
  StringRef RemoveStr = "world";
  size_t Pos = Input.find(RemoveStr);
  Buf.RemoveText(Pos, RemoveStr.size());

  StringRef TagStr = "hello";
  Pos = Input.find(TagStr);
  tagRange(Pos, TagStr.size(), "outer", Buf);
  tagRange(Pos, TagStr.size(), "inner", Buf);

  std::string Result;
  raw_string_ostream OS(Result);
  Buf.write(OS);
  OS.flush();
  EXPECT_EQ(Output, Result);
}

} // anonymous namespace
