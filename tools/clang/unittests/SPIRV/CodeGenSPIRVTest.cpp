//===- unittests/SPIRV/CodeGenSPIRVTest.cpp ---- Run CodeGenSPIRV tests ---===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "FileTestFixture.h"
#include "WholeFileTestFixture.h"

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

TEST_F(FileTest, ScalarConstants) { runFileTest("constant.scalar.hlsl"); }

TEST_F(FileTest, UnaryOpPrefixIncrement) {
  runFileTest("unary-op.prefix-inc.hlsl");
}

TEST_F(FileTest, BinaryOpAssign) { runFileTest("binary-op.assign.hlsl"); }

TEST_F(FileTest, BinaryOpScalarArithmetic) {
  runFileTest("binary-op.arithmetic.scalar.hlsl");
}

TEST_F(FileTest, BinaryOpScalarComparison) {
  runFileTest("binary-op.comparison.scalar.hlsl");
}

TEST_F(FileTest, IfStmtPlainAssign) { runFileTest("if-stmt.plain.hlsl"); }

TEST_F(FileTest, IfStmtNestedIfStmt) { runFileTest("if-stmt.nested.hlsl"); }

TEST_F(FileTest, ForStmtPlainAssign) { runFileTest("for-stmt.plain.hlsl"); }

TEST_F(FileTest, ForStmtNestedForStmt) { runFileTest("for-stmt.nested.hlsl"); }

TEST_F(FileTest, ControlFlowNestedIfForStmt) { runFileTest("cf.if.for.hlsl"); }

TEST_F(FileTest, FunctionCall) { runFileTest("fn.call.hlsl"); }

} // namespace
