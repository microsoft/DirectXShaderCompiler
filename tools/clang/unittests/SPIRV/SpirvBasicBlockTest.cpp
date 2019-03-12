//===- unittests/SPIRV/SpirvBasicBlockTest.cpp ----- Basic Block Tests ----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/SPIRV/SpirvBasicBlock.h"
#include "clang/SPIRV/SpirvInstruction.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace clang::spirv;

namespace {

TEST(SpirvBasicBlockTest, CheckName) {
  SpirvBasicBlock bb("myBasicBlock");
  EXPECT_EQ(bb.getName(), "myBasicBlock");
}

TEST(SpirvBasicBlockTest, CheckResultId) {
  SpirvBasicBlock bb("myBasicBlock");
  bb.setResultId(5);
  EXPECT_EQ(bb.getResultId(), 5u);
}

TEST(SpirvBasicBlockTest, CheckMergeTarget) {
  SpirvBasicBlock bb1("bb1");
  SpirvBasicBlock bb2("bb2");
  bb1.setMergeTarget(&bb2);
  EXPECT_EQ(bb1.getMergeTarget(), &bb2);
}

TEST(SpirvBasicBlockTest, CheckContinueTarget) {
  SpirvBasicBlock bb1("bb1");
  SpirvBasicBlock bb2("bb2");
  bb1.setContinueTarget(&bb2);
  EXPECT_EQ(bb1.getContinueTarget(), &bb2);
}

TEST(SpirvBasicBlockTest, CheckSuccessors) {
  SpirvBasicBlock bb1("bb1");
  SpirvBasicBlock bb2("bb2");
  SpirvBasicBlock bb3("bb3");
  bb1.addSuccessor(&bb2);
  bb1.addSuccessor(&bb3);
  auto successors = bb1.getSuccessors();
  EXPECT_EQ(successors[0], &bb2);
  EXPECT_EQ(successors[1], &bb3);
}

TEST(SpirvBasicBlockTest, CheckTerminatedByKill) {
  SpirvBasicBlock bb("bb");
  SpirvKill kill({});
  bb.addInstruction(&kill);
  EXPECT_TRUE(bb.hasTerminator());
}

TEST(SpirvBasicBlockTest, CheckTerminatedByBranch) {
  SpirvBasicBlock bb("bb");
  SpirvBranch branch({}, nullptr);
  bb.addInstruction(&branch);
  EXPECT_TRUE(bb.hasTerminator());
}

TEST(SpirvBasicBlockTest, CheckTerminatedByBranchConditional) {
  SpirvBasicBlock bb("bb");
  SpirvBranchConditional branch({}, nullptr, nullptr, nullptr);
  bb.addInstruction(&branch);
  EXPECT_TRUE(bb.hasTerminator());
}

TEST(SpirvBasicBlockTest, CheckTerminatedByReturn) {
  SpirvBasicBlock bb("bb");
  SpirvReturn returnInstr({});
  bb.addInstruction(&returnInstr);
  EXPECT_TRUE(bb.hasTerminator());
}

TEST(SpirvBasicBlockTest, CheckTerminatedByUnreachable) {
  SpirvBasicBlock bb("bb");
  SpirvUnreachable unreachable({});
  bb.addInstruction(&unreachable);
  EXPECT_TRUE(bb.hasTerminator());
}

TEST(SpirvBasicBlockTest, CheckNotTerminated) {
  SpirvBasicBlock bb("bb");
  SpirvLoad load({}, {}, nullptr);
  bb.addInstruction(&load);
  EXPECT_FALSE(bb.hasTerminator());
}

} // anonymous namespace
