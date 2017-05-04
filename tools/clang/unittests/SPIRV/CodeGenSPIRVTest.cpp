//===- unittests/SPIRV/CodeGenSPIRVTest.cpp ---- Run CodeGenSPIRV tests ---===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "FileTestFixture.h"
#include "WholeFileCheck.h"

namespace {
using clang::spirv::FileTest;
using clang::spirv::WholeFileTest;

// === Whole output tests ===

TEST_F(WholeFileTest, EmptyVoidMain) {
  runWholeFileTest("empty-void-main.hlsl2spv", /*generateHeader*/ true);
}

TEST_F(WholeFileTest, PassThruPixelShader) {
  runWholeFileTest("passthru-ps.hlsl2spv", /*generateHeader*/ true);
}

TEST_F(WholeFileTest, PassThruVertexShader) {
  runWholeFileTest("passthru-vs.hlsl2spv", /*generateHeader*/ true);
}

TEST_F(WholeFileTest, ConstantPixelShader) {
  runWholeFileTest("constant-ps.hlsl2spv", /*generateHeader*/ true);
}

// === Partial output tests ===

TEST_F(FileTest, ScalarTypes) { runFileTest("type.scalar.hlsl"); }

} // namespace
