//===- llvm/unittest/IR/AttributesTest.cpp - Attributes unit tests --------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// AttributesTest.cpp                                                        //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/IR/Attributes.h"
#include "llvm/IR/LLVMContext.h"
#include "gtest/gtest.h"
using namespace llvm;

namespace {

TEST(Attributes, Uniquing) {
  LLVMContext C;

  Attribute AttrA = Attribute::get(C, Attribute::AlwaysInline);
  Attribute AttrB = Attribute::get(C, Attribute::AlwaysInline);
  EXPECT_EQ(AttrA, AttrB);

  AttributeSet ASs[] = {
    AttributeSet::get(C, 1, Attribute::ZExt),
    AttributeSet::get(C, 2, Attribute::SExt)
  };

  AttributeSet SetA = AttributeSet::get(C, ASs);
  AttributeSet SetB = AttributeSet::get(C, ASs);
  EXPECT_EQ(SetA, SetB);
}

TEST(Attributes, Ordering) {
  LLVMContext C;

  AttributeSet ASs[] = {
    AttributeSet::get(C, 2, Attribute::ZExt),
    AttributeSet::get(C, 1, Attribute::SExt)
  };

  AttributeSet SetA = AttributeSet::get(C, ASs);
  AttributeSet SetB = SetA.removeAttributes(C, 1, ASs[1]);
  EXPECT_NE(SetA, SetB);
}

} // end anonymous namespace
