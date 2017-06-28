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

// For types
TEST_F(FileTest, ScalarTypes) { runFileTest("type.scalar.hlsl"); }
TEST_F(FileTest, VectorTypes) { runFileTest("type.vector.hlsl"); }
TEST_F(FileTest, MatrixTypes) { runFileTest("type.matrix.hlsl"); }

// For constants
TEST_F(FileTest, ScalarConstants) { runFileTest("constant.scalar.hlsl"); }
TEST_F(FileTest, VectorConstants) { runFileTest("constant.vector.hlsl"); }

// For variables
TEST_F(FileTest, VariableInitializer) { runFileTest("var.init.hlsl"); }

// For prefix/postfix increment/decrement
TEST_F(FileTest, UnaryOpPrefixIncrement) {
  runFileTest("unary-op.prefix-inc.hlsl");
}
TEST_F(FileTest, UnaryOpPrefixIncrementMatrix) {
  runFileTest("unary-op.prefix-inc.matrix.hlsl");
}
TEST_F(FileTest, UnaryOpPrefixDecrement) {
  runFileTest("unary-op.prefix-dec.hlsl");
}
TEST_F(FileTest, UnaryOpPrefixDecrementMatrix) {
  runFileTest("unary-op.prefix-dec.matrix.hlsl");
}
TEST_F(FileTest, UnaryOpPostfixIncrement) {
  runFileTest("unary-op.postfix-inc.hlsl");
}
TEST_F(FileTest, UnaryOpPostfixIncrementMatrix) {
  runFileTest("unary-op.postfix-inc.matrix.hlsl");
}
TEST_F(FileTest, UnaryOpPostfixDecrement) {
  runFileTest("unary-op.postfix-dec.hlsl");
}
TEST_F(FileTest, UnaryOpPostfixDecrementMatrix) {
  runFileTest("unary-op.postfix-dec.matrix.hlsl");
}

// For unary operators
TEST_F(FileTest, UnaryOpPlus) { runFileTest("unary-op.plus.hlsl"); }
TEST_F(FileTest, UnaryOpMinus) { runFileTest("unary-op.minus.hlsl"); }
TEST_F(FileTest, UnaryOpLogicalNot) {
  runFileTest("unary-op.logical-not.hlsl");
}

// For assignments
TEST_F(FileTest, BinaryOpAssign) { runFileTest("binary-op.assign.hlsl"); }

// For arithmetic binary operators
TEST_F(FileTest, BinaryOpScalarArithmetic) {
  runFileTest("binary-op.arithmetic.scalar.hlsl");
}
TEST_F(FileTest, BinaryOpVectorArithmetic) {
  runFileTest("binary-op.arithmetic.vector.hlsl");
}
TEST_F(FileTest, BinaryOpMatrixArithmetic) {
  runFileTest("binary-op.arithmetic.matrix.hlsl");
}
TEST_F(FileTest, BinaryOpMixedArithmetic) {
  runFileTest("binary-op.arithmetic.mixed.hlsl");
}

// For arithmetic assignments
TEST_F(FileTest, BinaryOpScalarArithAssign) {
  runFileTest("binary-op.arith-assign.scalar.hlsl");
}
TEST_F(FileTest, BinaryOpVectorArithAssign) {
  runFileTest("binary-op.arith-assign.vector.hlsl");
}
TEST_F(FileTest, BinaryOpMatrixArithAssign) {
  runFileTest("binary-op.arith-assign.matrix.hlsl");
}
TEST_F(FileTest, BinaryOpMixedArithAssign) {
  runFileTest("binary-op.arith-assign.mixed.hlsl");
}

// For bitwise binary operators
TEST_F(FileTest, BinaryOpScalarBitwise) {
  runFileTest("binary-op.bitwise.scalar.hlsl");
}
TEST_F(FileTest, BinaryOpVectorBitwise) {
  runFileTest("binary-op.bitwise.vector.hlsl");
}

// For bitwise assignments
TEST_F(FileTest, BinaryOpScalarBitwiseAssign) {
  runFileTest("binary-op.bitwise-assign.scalar.hlsl");
}
TEST_F(FileTest, BinaryOpVectorBitwiseAssign) {
  runFileTest("binary-op.bitwise-assign.vector.hlsl");
}

// For comparison operators
TEST_F(FileTest, BinaryOpScalarComparison) {
  runFileTest("binary-op.comparison.scalar.hlsl");
}
TEST_F(FileTest, BinaryOpVectorComparison) {
  runFileTest("binary-op.comparison.vector.hlsl");
}

// For logical binary operators
TEST_F(FileTest, BinaryOpLogicalAnd) {
  runFileTest("binary-op.logical-and.hlsl");
}
TEST_F(FileTest, BinaryOpLogicalOr) {
  runFileTest("binary-op.logical-or.hlsl");
}

// For ternary operators
TEST_F(FileTest, TernaryOpConditionalOp) {
  runFileTest("ternary-op.cond-op.hlsl");
}

// For vector accessing/swizzling operators
TEST_F(FileTest, OpVectorSwizzle) { runFileTest("op.vector.swizzle.hlsl"); }
TEST_F(FileTest, OpVectorSize1Swizzle) {
  runFileTest("op.vector.swizzle.size1.hlsl");
}
TEST_F(FileTest, OpVectorAccess) { runFileTest("op.vector.access.hlsl"); }

// For matrix accessing/swizzling operators
TEST_F(FileTest, OpMatrixAccessMxN) {
  runFileTest("op.matrix.access.mxn.hlsl");
}
TEST_F(FileTest, OpMatrixAccessMx1) {
  runFileTest("op.matrix.access.mx1.hlsl");
}
TEST_F(FileTest, OpMatrixAccess1xN) {
  runFileTest("op.matrix.access.1xn.hlsl");
}
TEST_F(FileTest, OpMatrixAccess1x1) {
  runFileTest("op.matrix.access.1x1.hlsl");
}

// For casting
TEST_F(FileTest, CastNoOp) { runFileTest("cast.no-op.hlsl"); }
TEST_F(FileTest, CastImplicit2Bool) { runFileTest("cast.2bool.implicit.hlsl"); }
TEST_F(FileTest, CastExplicit2Bool) { runFileTest("cast.2bool.explicit.hlsl"); }
TEST_F(FileTest, CastImplicit2SInt) { runFileTest("cast.2sint.implicit.hlsl"); }
TEST_F(FileTest, CastExplicit2SInt) { runFileTest("cast.2sint.explicit.hlsl"); }
TEST_F(FileTest, CastImplicit2UInt) { runFileTest("cast.2uint.implicit.hlsl"); }
TEST_F(FileTest, CastExplicit2UInt) { runFileTest("cast.2uint.explicit.hlsl"); }
TEST_F(FileTest, CastImplicit2FP) { runFileTest("cast.2fp.implicit.hlsl"); }
TEST_F(FileTest, CastExplicit2FP) { runFileTest("cast.2fp.explicit.hlsl"); }

// For vector/matrix splatting and trunction
TEST_F(FileTest, CastTruncateVector) { runFileTest("cast.vector.trunc.hlsl"); }
TEST_F(FileTest, CastSplatVector) { runFileTest("cast.vector.splat.hlsl"); }
TEST_F(FileTest, CastSplatMatrix) { runFileTest("cast.matrix.splat.hlsl"); }

// For if statements
TEST_F(FileTest, IfStmtPlainAssign) { runFileTest("if-stmt.plain.hlsl"); }
TEST_F(FileTest, IfStmtNestedIfStmt) { runFileTest("if-stmt.nested.hlsl"); }

// For switch statements
TEST_F(FileTest, SwitchStmtUsingOpSwitch) {
  runFileTest("switch-stmt.opswitch.hlsl");
}
TEST_F(FileTest, SwitchStmtUsingIfStmt) {
  runFileTest("switch-stmt.ifstmt.hlsl");
}

// For for statements
TEST_F(FileTest, ForStmtPlainAssign) { runFileTest("for-stmt.plain.hlsl"); }
TEST_F(FileTest, ForStmtNestedForStmt) { runFileTest("for-stmt.nested.hlsl"); }

// For while statements
TEST_F(FileTest, WhileStmtPlain) { runFileTest("while-stmt.plain.hlsl"); }
TEST_F(FileTest, WhileStmtNested) { runFileTest("while-stmt.nested.hlsl"); }

// For control flows
TEST_F(FileTest, ControlFlowNestedIfForStmt) { runFileTest("cf.if.for.hlsl"); }
TEST_F(FileTest, ControlFlowLogicalAnd) { runFileTest("cf.logical-and.hlsl"); }
TEST_F(FileTest, ControlFlowLogicalOr) { runFileTest("cf.logical-or.hlsl"); }
TEST_F(FileTest, ControlFlowConditionalOp) { runFileTest("cf.cond-op.hlsl"); }

// For function calls
TEST_F(FileTest, FunctionCall) { runFileTest("fn.call.hlsl"); }

// For intrinsic functions
TEST_F(FileTest, IntrinsicsDot) { runFileTest("intrinsics.dot.hlsl"); }
TEST_F(FileTest, IntrinsicsAll) { runFileTest("intrinsics.all.hlsl"); }
TEST_F(FileTest, IntrinsicsAny) { runFileTest("intrinsics.any.hlsl"); }
TEST_F(FileTest, IntrinsicsAsfloat) { runFileTest("intrinsics.asfloat.hlsl"); }
TEST_F(FileTest, IntrinsicsAsint) { runFileTest("intrinsics.asint.hlsl"); }
TEST_F(FileTest, IntrinsicsAsuint) { runFileTest("intrinsics.asuint.hlsl"); }

} // namespace
