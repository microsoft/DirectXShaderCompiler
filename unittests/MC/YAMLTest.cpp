//===- llvm/unittest/Object/YAMLTest.cpp - Tests for Object YAML ----------===//
///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// YAMLTest.cpp                                                              //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// Licensed under the MIT license. See COPYRIGHT in the project root for     //
// full license information.                                                 //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/MC/YAML.h"
#include "llvm/Support/YAMLTraits.h"
#include "gtest/gtest.h"

using namespace llvm;

struct BinaryHolder {
  yaml::BinaryRef Binary;
};

namespace llvm {
namespace yaml {
template <>
struct MappingTraits<BinaryHolder> {
  static void mapping(IO &IO, BinaryHolder &BH) {
    IO.mapRequired("Binary", BH.Binary);
  }
};
} // end namespace yaml
} // end namespace llvm

TEST(ObjectYAML, BinaryRef) {
  BinaryHolder BH;
  SmallVector<char, 32> Buf;
  llvm::raw_svector_ostream OS(Buf);
  yaml::Output YOut(OS);
  YOut << BH;
  EXPECT_NE(OS.str().find("''"), StringRef::npos);
}
