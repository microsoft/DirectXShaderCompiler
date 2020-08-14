//===- unittests/SPIRV/SpirvDebugInstructionTest.cpp - test debug insts --===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===---------------------------------------------------------------------===//

#include "SpirvTestBase.h"
#include "clang/SPIRV/SpirvBuilder.h"
#include "clang/SPIRV/SpirvInstruction.h"
#include "clang/SPIRV/SpirvType.h"

using namespace clang::spirv;

namespace {

class SpirvDebugInstructionTest : public SpirvTestBase {
public:
  SpirvDebugInstructionTest() : spirvBuilder(nullptr) {}
  SpirvBuilder *GetSpirvBuilder();

private:
  SpirvBuilder *spirvBuilder;
};

SpirvBuilder *SpirvDebugInstructionTest::GetSpirvBuilder() {
  if (spirvBuilder != nullptr)
    return spirvBuilder;
  SpirvCodeGenOptions opt;
  spirvBuilder = new (getSpirvContext())
      SpirvBuilder(getAstContext(), getSpirvContext(), opt);
  return spirvBuilder;
}

TEST_F(SpirvDebugInstructionTest, DynamicTypeCheckDebugInfoNone) {
  SpirvInstruction *i = GetSpirvBuilder()->getOrCreateDebugInfoNone();
  EXPECT_TRUE(llvm::isa<SpirvDebugInfoNone>(i));
  EXPECT_TRUE(llvm::isa<SpirvDebugInstruction>(i));
  EXPECT_TRUE(llvm::isa<SpirvInstruction>(i));
}

TEST_F(SpirvDebugInstructionTest, DynamicTypeCheckDebugTypeTemplateParameter) {
  SpirvInstruction *i = getSpirvContext().createDebugTypeTemplateParameter(
      nullptr, "vtable check", nullptr, nullptr, nullptr, 0, 0);
  EXPECT_TRUE(llvm::isa<SpirvDebugTypeTemplateParameter>(i));
  EXPECT_TRUE(llvm::isa<SpirvDebugInstruction>(i));
  EXPECT_TRUE(llvm::isa<SpirvInstruction>(i));
}

} // anonymous namespace
