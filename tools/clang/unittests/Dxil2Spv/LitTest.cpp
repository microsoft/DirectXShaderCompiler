//===- utils/unittest/Dxil2Spv/LitTest.cpp ---- Run dxil2spv lit tests ----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "WholeFileTestFixture.h"

namespace {
using clang::dxil2spv::WholeFileTest;

TEST_F(WholeFileTest, PassThruPixelShader) {
  runWholeFileTest("passthru-ps.ll");
}

TEST_F(WholeFileTest, PassThruVertexShader) {
  runWholeFileTest("passthru-vs.ll");
}

TEST_F(WholeFileTest, PassThruComputeShader) {
  runWholeFileTest("passthru-cs.ll");
}

} // namespace
