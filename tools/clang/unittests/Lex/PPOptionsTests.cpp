///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// PPOptionsTests.cpp                                                        //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Tests HLSL-specific preprocessor option defaults.                         //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "clang/Lex/PreprocessorOptions.h"
#include "gtest/gtest.h"

#include <cstring>
#include <new>

using namespace clang;

TEST(PPOptionsTests, ExpandTokPastingArgDefaultsToFalse) {
  alignas(
      PreprocessorOptions) unsigned char Storage[sizeof(PreprocessorOptions)];
  std::memset(Storage, 0xff, sizeof(Storage));

  auto *Options = new (Storage) PreprocessorOptions();
  EXPECT_FALSE(Options->ExpandTokPastingArg);
  Options->~PreprocessorOptions();
}