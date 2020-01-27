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

TEST_F(WholeFileTest, PassThruPixelShader) {
  runWholeFileTest("passthru-ps.hlsl2spv", /*generateHeader*/ true);
}

TEST_F(WholeFileTest, PassThruVertexShader) {
  runWholeFileTest("passthru-vs.hlsl2spv", /*generateHeader*/ true);
}

TEST_F(WholeFileTest, PassThruComputeShader) {
  runWholeFileTest("passthru-cs.hlsl2spv", /*generateHeader*/ true);
}

TEST_F(WholeFileTest, BezierHullShader) {
  runWholeFileTest("bezier.hull.hlsl2spv");
}

TEST_F(WholeFileTest, BezierDomainShader) {
  runWholeFileTest("bezier.domain.hlsl2spv");
}
TEST_F(WholeFileTest, EmptyStructInterfaceVS) {
  runWholeFileTest("empty-struct-interface.vs.hlsl2spv");
}

// === Partial output tests ===

// For types
TEST_F(FileTest, ScalarTypes) { runFileTest("type.scalar.hlsl"); }
TEST_F(FileTest, VectorTypes) { runFileTest("type.vector.hlsl"); }
TEST_F(FileTest, MatrixTypes) { runFileTest("type.matrix.hlsl"); }
TEST_F(FileTest, MatrixTypesMajornessZpr) {
  runFileTest("type.matrix.majorness.zpr.hlsl");
}
TEST_F(FileTest, MatrixTypesMajornessZpc) {
  runFileTest("type.matrix.majorness.zpc.hlsl");
}
TEST_F(FileTest, StructTypes) { runFileTest("type.struct.hlsl"); }
TEST_F(FileTest, StructTypeUniqueness) {
  runFileTest("type.struct.uniqueness.hlsl");
}
TEST_F(FileTest, ClassTypes) { runFileTest("type.class.hlsl"); }
TEST_F(FileTest, ArrayTypes) { runFileTest("type.array.hlsl"); }
TEST_F(FileTest, RuntimeArrayTypes) { runFileTest("type.runtime-array.hlsl"); }
TEST_F(FileTest, TypedefTypes) { runFileTest("type.typedef.hlsl"); }
TEST_F(FileTest, SamplerTypes) { runFileTest("type.sampler.hlsl"); }
TEST_F(FileTest, TextureTypes) { runFileTest("type.texture.hlsl"); }
TEST_F(FileTest, RWTextureTypes) { runFileTest("type.rwtexture.hlsl"); }
TEST_F(FileTest, BufferType) { runFileTest("type.buffer.hlsl"); }
TEST_F(FileTest, BufferTypeStructError1) {
  runFileTest("type.buffer.struct.error1.hlsl", Expect::Failure);
}
TEST_F(FileTest, BufferTypeStructError2) {
  runFileTest("type.buffer.struct.error2.hlsl", Expect::Failure);
}
TEST_F(FileTest, BufferTypeStructError3) {
  runFileTest("type.buffer.struct.error3.hlsl", Expect::Failure);
}
TEST_F(FileTest, RWBufferTypeHalfElementType) {
  runFileTest("type.rwbuffer.half.hlsl");
}
TEST_F(FileTest, RWBufferTypeStructError) {
  runFileTest("type.rwbuffer.struct.error.hlsl", Expect::Failure);
}
TEST_F(FileTest, CBufferType) { runFileTest("type.cbuffer.hlsl"); }
TEST_F(FileTest, ConstantBufferType) {
  runFileTest("type.constant-buffer.hlsl");
}
TEST_F(FileTest, EnumType) { runFileTest("type.enum.hlsl"); }
TEST_F(FileTest, TBufferType) { runFileTest("type.tbuffer.hlsl"); }
TEST_F(FileTest, TextureBufferType) { runFileTest("type.texture-buffer.hlsl"); }
TEST_F(FileTest, StructuredBufferType) {
  runFileTest("type.structured-buffer.hlsl");
}
TEST_F(FileTest, StructuredBufferTypeWithVector) {
  runFileTest("type.structured-buffer.vector.hlsl");
}
TEST_F(FileTest, StructuredBufferTypeWithVectorDX) {
  // structured buffers with fxc layout rules
  setDxLayout();
  runFileTest("type.structured-buffer.vector.dx.hlsl");
}
TEST_F(FileTest, StructuredBufferTypeWithVectorGL) {
  setGlLayout();
  runFileTest("type.structured-buffer.vector.gl.hlsl");
}
TEST_F(FileTest, StructuredBufferTypeWithVectorScalar) {
  // VK_EXT_scalar_block_layout
  setScalarLayout();
  runFileTest("type.structured-buffer.vector.scalar.hlsl");
}
TEST_F(FileTest, StructuredByteBufferArray) {
  setBeforeHLSLLegalization();
  runFileTest("type.structured-buffer.array.hlsl");
}
TEST_F(FileTest, StructuredByteBufferArrayError) {
  runFileTest("type.structured-buffer.array.error.hlsl", Expect::Failure);
}
TEST_F(FileTest, AppendConsumeStructuredBufferTypeCast) {
  runFileTest("type.append.consume-structured-buffer.cast.hlsl");
}
TEST_F(FileTest, AppendStructuredBufferType) {
  runFileTest("type.append-structured-buffer.hlsl");
}
TEST_F(FileTest, ConsumeStructuredBufferType) {
  runFileTest("type.consume-structured-buffer.hlsl");
}
TEST_F(FileTest, ByteAddressBufferTypes) {
  runFileTest("type.byte-address-buffer.hlsl");
}
TEST_F(FileTest, PointStreamTypes) { runFileTest("type.point-stream.hlsl"); }
TEST_F(FileTest, LineStreamTypes) { runFileTest("type.line-stream.hlsl"); }
TEST_F(FileTest, TriangleStreamTypes) {
  runFileTest("type.triangle-stream.hlsl");
}

// For constants
TEST_F(FileTest, ScalarConstants) { runFileTest("constant.scalar.hlsl"); }
TEST_F(FileTest, 16BitDisabledScalarConstants) {
  runFileTest("constant.scalar.16bit.disabled.hlsl");
}
TEST_F(FileTest, 16BitEnabledScalarConstants) {
  runFileTest("constant.scalar.16bit.enabled.hlsl");
}
TEST_F(FileTest, 16BitEnabledScalarConstantsHalfZero) {
  runFileTest("constant.scalar.16bit.enabled.half.zero.hlsl");
}
TEST_F(FileTest, 64BitScalarConstants) {
  runFileTest("constant.scalar.64bit.hlsl");
}
TEST_F(FileTest, VectorConstants) { runFileTest("constant.vector.hlsl"); }
TEST_F(FileTest, MatrixConstants) { runFileTest("constant.matrix.hlsl"); }
TEST_F(FileTest, StructConstants) { runFileTest("constant.struct.hlsl"); }
TEST_F(FileTest, ArrayConstants) { runFileTest("constant.array.hlsl"); }

// For literals
TEST_F(FileTest, UnusedLiterals) { runFileTest("literal.unused.hlsl"); }
TEST_F(FileTest, LiteralConstantComposite) {
  runFileTest("literal.constant-composite.hlsl");
}
TEST_F(FileTest, LiteralVecTimesScalar) {
  runFileTest("literal.vec-times-scalar.hlsl");
}

// For variables
TEST_F(FileTest, VarInitScalarVector) { runFileTest("var.init.hlsl"); }
TEST_F(FileTest, VarInitMatrixMxN) { runFileTest("var.init.matrix.mxn.hlsl"); }
TEST_F(FileTest, VarInitMatrixMx1) { runFileTest("var.init.matrix.mx1.hlsl"); }
TEST_F(FileTest, VarInitMatrix1xN) { runFileTest("var.init.matrix.1xn.hlsl"); }
TEST_F(FileTest, VarInitMatrix1x1) { runFileTest("var.init.matrix.1x1.hlsl"); }
TEST_F(FileTest, VarInitStruct) { runFileTest("var.init.struct.hlsl"); }
TEST_F(FileTest, VarInitArray) { runFileTest("var.init.array.hlsl"); }
TEST_F(FileTest, VarInitCbuffer) {
  runFileTest("var.init.cbuffer.hlsl", Expect::Warning);
}
TEST_F(FileTest, VarInitTbuffer) {
  runFileTest("var.init.tbuffer.hlsl", Expect::Warning);
}
TEST_F(FileTest, VarInitWarningIngored) {
  runFileTest("var.init.warning.ignored.hlsl", Expect::Warning);
}
TEST_F(FileTest, VarInitOpaque) {
  setBeforeHLSLLegalization();
  runFileTest("var.init.opaque.hlsl");
}
TEST_F(FileTest, VarInitCrossStorageClass) {
  runFileTest("var.init.cross-storage-class.hlsl");
}
TEST_F(FileTest, VarInitVec1) { runFileTest("var.init.vec.size.1.hlsl"); }
TEST_F(FileTest, StaticVar) { runFileTest("var.static.hlsl"); }
TEST_F(FileTest, UninitStaticResourceVar) {
  runFileTest("var.static.resource.hlsl");
}
TEST_F(FileTest, ResourceArrayVar) { runFileTest("var.resource.array.hlsl"); }
TEST_F(FileTest, GlobalsCBuffer) { runFileTest("var.globals.hlsl"); }
TEST_F(FileTest, GlobalsCBufferError) {
  runFileTest("var.globals.error.hlsl", Expect::Failure);
}

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
TEST_F(FileTest, BinaryOpAssignImage) {
  runFileTest("binary-op.assign.image.hlsl");
}
TEST_F(FileTest, BinaryOpAssignComposite) {
  runFileTest("binary-op.assign.composite.hlsl");
}
TEST_F(FileTest, BinaryOpAssignOpaqueArray) {
  // Test that for copying opaque arrays, we load each element via access chain
  // separately, create an composite, and then write out once
  setBeforeHLSLLegalization();
  runFileTest("binary-op.assign.opaque.array.hlsl");
}

// For comma binary operator
TEST_F(FileTest, BinaryOpComma) { runFileTest("binary-op.comma.hlsl"); }

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
TEST_F(FileTest, BinaryOpMixedFormArithAssign) {
  // Test mixing scalar/vector/matrix/etc.
  runFileTest("binary-op.arith-assign.mixed.form.hlsl");
}
TEST_F(FileTest, BinaryOpMixedTypeArithAssign) {
  // Test mixing float/int/uint/bool/etc.
  runFileTest("binary-op.arith-assign.mixed.type.hlsl");
}

// For bitwise binary operators
TEST_F(FileTest, BinaryOpScalarBitwise) {
  runFileTest("binary-op.bitwise.scalar.hlsl");
}
TEST_F(FileTest, BinaryOpVectorBitwise) {
  runFileTest("binary-op.bitwise.vector.hlsl");
}
TEST_F(FileTest, BinaryOpBitwiseShiftLeft) {
  runFileTest("binary-op.bitwise.shift-left.hlsl");
}
TEST_F(FileTest, BinaryOpBitwiseShiftRight) {
  runFileTest("binary-op.bitwise.shift-right.hlsl");
}

// For bitwise assignments
TEST_F(FileTest, BinaryOpScalarBitwiseAssign) {
  runFileTest("binary-op.bitwise-assign.scalar.hlsl");
}
TEST_F(FileTest, BinaryOpVectorBitwiseAssign) {
  runFileTest("binary-op.bitwise-assign.vector.hlsl");
}
TEST_F(FileTest, BinaryOpBitwiseAssignShiftLeft) {
  runFileTest("binary-op.bitwise-assign.shift-left.hlsl");
}
TEST_F(FileTest, BinaryOpBitwiseAssignShiftRight) {
  runFileTest("binary-op.bitwise-assign.shift-right.hlsl");
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
TEST_F(FileTest, OpVectorSwizzle1) {
  runFileTest("op.vector.swizzle.size1.hlsl");
}
TEST_F(FileTest, OpVectorSwizzleAfterBufferAccess) {
  runFileTest("op.vector.swizzle.buffer-access.hlsl");
}
TEST_F(FileTest, OpVectorSwizzleAfterTextureAccess) {
  runFileTest("op.vector.swizzle.texture-access.hlsl");
}
TEST_F(FileTest, OpVectorSwizzleConstScalar) {
  runFileTest("op.vector.swizzle.const-scalar.hlsl");
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

// For struct & array accessing operator
TEST_F(FileTest, OpStructAccess) { runFileTest("op.struct.access.hlsl"); }
TEST_F(FileTest, OpArrayAccess) { runFileTest("op.array.access.hlsl"); }

// For buffer accessing operator
TEST_F(FileTest, OpBufferAccess) { runFileTest("op.buffer.access.hlsl"); }
TEST_F(FileTest, OpRWBufferAccess) { runFileTest("op.rwbuffer.access.hlsl"); }
TEST_F(FileTest, OpCBufferAccess) { runFileTest("op.cbuffer.access.hlsl"); }
TEST_F(FileTest, OpCBufferAccessMajorness) {
  /// Tests that we correctly consider majorness when accessing matrices
  runFileTest("op.cbuffer.access.majorness.hlsl");
}
TEST_F(FileTest, OpConstantBufferAccess) {
  runFileTest("op.constant-buffer.access.hlsl");
}
TEST_F(FileTest, OpTBufferAccess) { runFileTest("op.tbuffer.access.hlsl"); }
TEST_F(FileTest, OpTextureBufferAccess) {
  runFileTest("op.texture-buffer.access.hlsl");
}
TEST_F(FileTest, OpStructuredBufferAccess) {
  runFileTest("op.structured-buffer.access.hlsl");
}
TEST_F(FileTest, OpRWStructuredBufferAccess) {
  runFileTest("op.rw-structured-buffer.access.hlsl");
}

// For Texture/RWTexture accessing operator (operator[])
TEST_F(FileTest, OpTextureAccess) { runFileTest("op.texture.access.hlsl"); }
TEST_F(FileTest, OpRWTextureAccessRead) {
  runFileTest("op.rwtexture.access.read.hlsl");
}
TEST_F(FileTest, OpRWTextureAccessWrite) {
  runFileTest("op.rwtexture.access.write.hlsl");
}

// For Texture.mips[][] operator
TEST_F(FileTest, OpTextureMipsAccess) {
  runFileTest("op.texture.mips-access.hlsl");
}
// For Texture2MD(Array).sample[][] operator
TEST_F(FileTest, OpTextureSampleAccess) {
  runFileTest("op.texture.sample-access.hlsl");
}
TEST_F(FileTest, OpSizeOf) { runFileTest("op.sizeof.hlsl"); }

// For casting
TEST_F(FileTest, CastNoOp) { runFileTest("cast.no-op.hlsl"); }
TEST_F(FileTest, CastNoOpMatrixFloatToInt) {
  runFileTest("cast.no-op.matrix.float-to-int.hlsl");
}
TEST_F(FileTest, CastImplicit2Bool) { runFileTest("cast.2bool.implicit.hlsl"); }
TEST_F(FileTest, CastExplicit2Bool) { runFileTest("cast.2bool.explicit.hlsl"); }
TEST_F(FileTest, CastImplicit2SInt) { runFileTest("cast.2sint.implicit.hlsl"); }
TEST_F(FileTest, CastExplicit2SInt) { runFileTest("cast.2sint.explicit.hlsl"); }
TEST_F(FileTest, CastImplicit2UInt) { runFileTest("cast.2uint.implicit.hlsl"); }
TEST_F(FileTest, CastExplicit2UInt) { runFileTest("cast.2uint.explicit.hlsl"); }
TEST_F(FileTest, CastImplicit2FP) { runFileTest("cast.2fp.implicit.hlsl"); }
TEST_F(FileTest, CastExplicit2FP) { runFileTest("cast.2fp.explicit.hlsl"); }
TEST_F(FileTest, CastImplicit2LiteralInt) {
  runFileTest("cast.2literal-int.implicit.hlsl");
}
TEST_F(FileTest, CastFlatConversionArrayToVector) {
  runFileTest("cast.flat-conversion.array-to-vector.hlsl");
}
TEST_F(FileTest, CastImplicitFlatConversion) {
  runFileTest("cast.flat-conversion.implicit.hlsl");
}
TEST_F(FileTest, CastFlatConversionStruct) {
  runFileTest("cast.flat-conversion.struct.hlsl");
}
TEST_F(FileTest, CastFlatConversionNoOp) {
  runFileTest("cast.flat-conversion.no-op.hlsl");
}
TEST_F(FileTest, CastFlatConversionStructToStruct) {
  runFileTest("cast.flat-conversion.struct-to-struct.hlsl");
}
TEST_F(FileTest, CastFlatConversionLiteralInitializer) {
  runFileTest("cast.flat-conversion.literal-initializer.hlsl");
}
TEST_F(FileTest, CastFlatConversionDecomposeVector) {
  runFileTest("cast.flat-conversion.vector.hlsl");
}
TEST_F(FileTest, CastExplicitVecToMat) {
  runFileTest("cast.vec-to-mat.explicit.hlsl");
}
TEST_F(FileTest, CastMatrixToVector) { runFileTest("cast.mat-to-vec.hlsl"); }
TEST_F(FileTest, CastBitwidth) { runFileTest("cast.bitwidth.hlsl"); }

TEST_F(FileTest, CastLiteralTypeForArraySubscript) {
  runFileTest("cast.literal-type.array-subscript.hlsl");
}

TEST_F(FileTest, CastLiteralTypeForTernary) {
  runFileTest("cast.literal-type.ternary.hlsl");
}

// For vector/matrix splatting and trunction
TEST_F(FileTest, CastTruncateVector) { runFileTest("cast.vector.trunc.hlsl"); }
TEST_F(FileTest, CastTruncateMatrix) { runFileTest("cast.matrix.trunc.hlsl"); }
TEST_F(FileTest, CastSplatVector) { runFileTest("cast.vector.splat.hlsl"); }
TEST_F(FileTest, CastSplatMatrix) { runFileTest("cast.matrix.splat.hlsl"); }

// For if statements
TEST_F(FileTest, IfStmtPlainAssign) { runFileTest("cf.if.plain.hlsl"); }
TEST_F(FileTest, IfStmtNestedIfStmt) { runFileTest("cf.if.nested.hlsl"); }
TEST_F(FileTest, IfStmtConstCondition) { runFileTest("cf.if.const-cond.hlsl"); }

// For switch statements
TEST_F(FileTest, SwitchStmtUsingOpSwitch) {
  runFileTest("cf.switch.opswitch.hlsl");
}
TEST_F(FileTest, SwitchStmtUsingIfStmt) {
  runFileTest("cf.switch.ifstmt.hlsl");
}

// For for statements
TEST_F(FileTest, ForStmtPlainAssign) { runFileTest("cf.for.plain.hlsl"); }
TEST_F(FileTest, ForStmtNestedForStmt) { runFileTest("cf.for.nested.hlsl"); }
TEST_F(FileTest, ForStmtContinue) { runFileTest("cf.for.continue.hlsl"); }
TEST_F(FileTest, ForStmtBreak) { runFileTest("cf.for.break.hlsl"); }

// For while statements
TEST_F(FileTest, WhileStmtPlain) { runFileTest("cf.while.plain.hlsl"); }
TEST_F(FileTest, WhileStmtNested) { runFileTest("cf.while.nested.hlsl"); }
TEST_F(FileTest, WhileStmtContinue) { runFileTest("cf.while.continue.hlsl"); }
TEST_F(FileTest, WhileStmtBreak) { runFileTest("cf.while.break.hlsl"); }

// For do statements
TEST_F(FileTest, DoStmtPlain) { runFileTest("cf.do.plain.hlsl"); }
TEST_F(FileTest, DoStmtNested) { runFileTest("cf.do.nested.hlsl"); }
TEST_F(FileTest, DoStmtContinue) { runFileTest("cf.do.continue.hlsl"); }
TEST_F(FileTest, DoStmtBreak) { runFileTest("cf.do.break.hlsl"); }

// For break statements (mix of breaks in loops and switch)
TEST_F(FileTest, BreakStmtMixed) { runFileTest("cf.break.mixed.hlsl"); }

// For discard statement
TEST_F(FileTest, Discard) { runFileTest("cf.discard.hlsl"); }

// For return statement
TEST_F(FileTest, EarlyReturn) { runFileTest("cf.return.early.hlsl"); }
TEST_F(FileTest, EarlyReturnFloat4) {
  runFileTest("cf.return.early.float4.hlsl");
}
TEST_F(FileTest, ReturnStruct) { runFileTest("cf.return.struct.hlsl"); }
TEST_F(FileTest, ReturnFromDifferentStorageClass) {
  runFileTest("cf.return.storage-class.hlsl");
}

// For control flows
TEST_F(FileTest, ControlFlowNestedIfForStmt) { runFileTest("cf.if.for.hlsl"); }
TEST_F(FileTest, ControlFlowLogicalAnd) { runFileTest("cf.logical-and.hlsl"); }
TEST_F(FileTest, ControlFlowLogicalOr) { runFileTest("cf.logical-or.hlsl"); }
TEST_F(FileTest, ControlFlowConditionalOp) { runFileTest("cf.cond-op.hlsl"); }

// For functions
TEST_F(FileTest, FunctionCall) { runFileTest("fn.call.hlsl"); }
TEST_F(FileTest, FunctionDefaultArg) { runFileTest("fn.default-arg.hlsl"); }
TEST_F(FileTest, FunctionInOutParam) {
  // Tests using uniform/in/out/inout annotations on function parameters
  runFileTest("fn.param.inout.hlsl");
}
TEST_F(FileTest, FunctionInOutParamVector) {
  setBeforeHLSLLegalization();
  runFileTest("fn.param.inout.vector.hlsl");
}
TEST_F(FileTest, FunctionInOutParamDiffStorageClass) {
  setBeforeHLSLLegalization();
  runFileTest("fn.param.inout.storage-class.hlsl");
}
TEST_F(FileTest, FunctionInOutParamIsomorphism) {
  setBeforeHLSLLegalization();
  runFileTest("fn.param.isomorphism.hlsl");
}
TEST_F(FileTest, FunctionInOutParamNoNeedToCopy) {
  // Tests that referencing function scope variables as a whole with out/inout
  // annotation does not create temporary variables
  runFileTest("fn.param.inout.no-copy.hlsl");
}
TEST_F(FileTest, FunctionParamUnsizedArray) {
  // Unsized ararys as function params are not supported.
  runFileTest("fn.param.unsized-array.hlsl", Expect::Failure);
}
TEST_F(FileTest, FunctionInOutParamTypeMismatch) {
  // The type for the inout parameter doesn't match the argument type.
  runFileTest("fn.param.inout.type-mismatch.hlsl");
}
TEST_F(FileTest, FunctionFowardDeclaration) {
  runFileTest("fn.foward-declaration.hlsl");
}
TEST_F(FileTest, FunctionInCTBuffer) {
  setBeforeHLSLLegalization();
  runFileTest("fn.ctbuffer.hlsl");
}

// For OO features
TEST_F(FileTest, StructMethodCall) {
  setBeforeHLSLLegalization();
  runFileTest("oo.struct.method.hlsl");
}
TEST_F(FileTest, StructThisAlias) {
  setBeforeHLSLLegalization();
  runFileTest("oo.struct.this.alias.hlsl");
}
TEST_F(FileTest, ClassMethodCall) {
  setBeforeHLSLLegalization();
  runFileTest("oo.class.method.hlsl");
}
TEST_F(FileTest, StructStaticMember) {
  runFileTest("oo.struct.static.member.hlsl");
}
TEST_F(FileTest, ClassStaticMember) {
  runFileTest("oo.struct.static.member.hlsl");
}
TEST_F(FileTest, StaticMemberInitializer) {
  runFileTest("oo.static.member.init.hlsl");
}
TEST_F(FileTest, MethodCallOnStaticVar) {
  setBeforeHLSLLegalization();
  runFileTest("oo.method.on-static-var.hlsl");
}
TEST_F(FileTest, Inheritance) { runFileTest("oo.inheritance.hlsl"); }
TEST_F(FileTest, InheritanceMemberFunction) {
  runFileTest("oo.inheritance.member.function.hlsl");
}
TEST_F(FileTest, InheritanceStageIOVS) {
  runFileTest("oo.inheritance.stage-io.vs.hlsl");
}
TEST_F(FileTest, InheritanceStageIOGS) {
  runFileTest("oo.inheritance.stage-io.gs.hlsl");
}

// For semantics
// SV_Position, SV_ClipDistance, and SV_CullDistance are covered in
// SpirvStageIOInterface* tests.
TEST_F(FileTest, SemanticVertexIDVS) {
  runFileTest("semantic.vertex-id.vs.hlsl");
}
TEST_F(FileTest, SemanticInstanceIDVS) {
  runFileTest("semantic.instance-id.vs.hlsl");
}
TEST_F(FileTest, SemanticInstanceIDHS) {
  runFileTest("semantic.instance-id.hs.hlsl");
}
TEST_F(FileTest, SemanticInstanceIDDS) {
  runFileTest("semantic.instance-id.ds.hlsl");
}
TEST_F(FileTest, SemanticInstanceIDGS) {
  runFileTest("semantic.instance-id.gs.hlsl");
}
TEST_F(FileTest, SemanticInstanceIDPS) {
  runFileTest("semantic.instance-id.ps.hlsl");
}
TEST_F(FileTest, SemanticTargetPS) { runFileTest("semantic.target.ps.hlsl"); }
TEST_F(FileTest, SemanticTargetDualBlend) {
  runFileTest("semantic.target.dual-blend.hlsl");
}
TEST_F(FileTest, SemanticTargetDualBlendError1) {
  runFileTest("semantic.target.dual-blend.error1.hlsl", Expect::Failure);
}
TEST_F(FileTest, SemanticTargetDualBlendError2) {
  runFileTest("semantic.target.dual-blend.error2.hlsl", Expect::Failure);
}
TEST_F(FileTest, SemanticDepthPS) { runFileTest("semantic.depth.ps.hlsl"); }
TEST_F(FileTest, SemanticDepthGreaterEqualPS) {
  runFileTest("semantic.depth-greater-equal.ps.hlsl");
}
TEST_F(FileTest, SemanticDepthLessEqualPS) {
  runFileTest("semantic.depth-less-equal.ps.hlsl");
}
TEST_F(FileTest, SemanticIsFrontFaceGS) {
  runFileTest("semantic.is-front-face.gs.hlsl");
}
TEST_F(FileTest, SemanticIsFrontFacePS) {
  runFileTest("semantic.is-front-face.ps.hlsl");
}
TEST_F(FileTest, SemanticDispatchThreadId) {
  runFileTest("semantic.dispatch-thread-id.cs.hlsl");
}
TEST_F(FileTest, SemanticDispatchThreadIdUint) {
  runFileTest("semantic.dispatch-thread-id.uint.cs.hlsl");
}
TEST_F(FileTest, SemanticDispatchThreadIdInt2) {
  runFileTest("semantic.dispatch-thread-id.int2.cs.hlsl");
}
TEST_F(FileTest, SemanticGroupID) { runFileTest("semantic.group-id.cs.hlsl"); }
TEST_F(FileTest, SemanticGroupIDUint) {
  runFileTest("semantic.group-id.uint.cs.hlsl");
}
TEST_F(FileTest, SemanticGroupIDInt2) {
  runFileTest("semantic.group-id.int2.cs.hlsl");
}
TEST_F(FileTest, SemanticGroupThreadID) {
  runFileTest("semantic.group-thread-id.cs.hlsl");
}
TEST_F(FileTest, SemanticGroupThreadIDUint) {
  runFileTest("semantic.group-thread-id.uint.cs.hlsl");
}
TEST_F(FileTest, SemanticGroupThreadIDInt2) {
  runFileTest("semantic.group-thread-id.int2.cs.hlsl");
}
TEST_F(FileTest, SemanticGroupIndex) {
  runFileTest("semantic.group-index.cs.hlsl");
}
TEST_F(FileTest, SemanticDomainLocationDS) {
  runFileTest("semantic.domain-location.ds.hlsl");
}
TEST_F(FileTest, SemanticTessFactorDS) {
  runFileTest("semantic.tess-factor.ds.hlsl");
}
TEST_F(FileTest, SemanticTessFactorSizeMismatchDS) {
  runFileTest("semantic.tess-factor.size-mismatch.ds.hlsl");
}
TEST_F(FileTest, SemanticInsideTessFactorDS) {
  runFileTest("semantic.inside-tess-factor.ds.hlsl");
}
TEST_F(FileTest, SemanticInsideTessFactorDSArray1) {
  // Test that SV_InsideTessFactor is of type float[1]
  runFileTest("semantic.inside-tess-factor.ds.array1.hlsl");
}
TEST_F(FileTest, SemanticTessFactorHS) {
  runFileTest("semantic.tess-factor.hs.hlsl");
}
TEST_F(FileTest, SemanticTessFactorSizeMismatchHS) {
  runFileTest("semantic.tess-factor.size-mismatch.hs.hlsl");
}
TEST_F(FileTest, SemanticInsideTessFactorHS) {
  runFileTest("semantic.inside-tess-factor.hs.hlsl");
}
TEST_F(FileTest, SemanticInsideTessFactorHSArray1) {
  // Test that SV_InsideTessFactor is of type float[1]
  runFileTest("semantic.inside-tess-factor.hs.array1.hlsl");
}
TEST_F(FileTest, SemanticPrimitiveIdHS) {
  runFileTest("semantic.primitive-id.hs.hlsl");
}
TEST_F(FileTest, SemanticPrimitiveIdDS) {
  runFileTest("semantic.primitive-id.ds.hlsl");
}
TEST_F(FileTest, SemanticPrimitiveIdGS) {
  runFileTest("semantic.primitive-id.gs.hlsl");
}
TEST_F(FileTest, SemanticPrimitiveIdPS) {
  runFileTest("semantic.primitive-id.ps.hlsl");
}
TEST_F(FileTest, SemanticOutputControlPointIdHS) {
  runFileTest("semantic.output-control-point-id.hs.hlsl");
}
TEST_F(FileTest, SemanticOutputControlPointIdHSError) {
  runFileTest("semantic.output-control-point-id.hs.error.hlsl",
              Expect::Failure);
}
TEST_F(FileTest, SemanticGSInstanceIDGS) {
  runFileTest("semantic.gs-instance-id.gs.hlsl");
}
TEST_F(FileTest, SemanticSampleIndexPS) {
  runFileTest("semantic.sample-index.ps.hlsl");
}
TEST_F(FileTest, SemanticStencilRefPS) {
  runFileTest("semantic.stencil-ref.ps.hlsl");
}
TEST_F(FileTest, SemanticRenderTargetArrayIndexVS) {
  runFileTest("semantic.render-target-array-index.vs.hlsl");
}
TEST_F(FileTest, SemanticRenderTargetArrayIndexHS) {
  runFileTest("semantic.render-target-array-index.hs.hlsl");
}
TEST_F(FileTest, SemanticRenderTargetArrayIndexDS) {
  runFileTest("semantic.render-target-array-index.ds.hlsl");
}
TEST_F(FileTest, SemanticRenderTargetArrayIndexGS) {
  runFileTest("semantic.render-target-array-index.gs.hlsl");
}
TEST_F(FileTest, SemanticRenderTargetArrayIndexPS) {
  runFileTest("semantic.render-target-array-index.ps.hlsl");
}
TEST_F(FileTest, SemanticViewportArrayIndexVS) {
  runFileTest("semantic.viewport-array-index.vs.hlsl");
}
TEST_F(FileTest, SemanticViewportArrayIndexHS) {
  runFileTest("semantic.viewport-array-index.hs.hlsl");
}
TEST_F(FileTest, SemanticViewportArrayIndexDS) {
  runFileTest("semantic.viewport-array-index.ds.hlsl");
}
TEST_F(FileTest, SemanticViewportArrayIndexGS) {
  runFileTest("semantic.viewport-array-index.gs.hlsl");
}
TEST_F(FileTest, SemanticViewportArrayIndexPS) {
  runFileTest("semantic.viewport-array-index.ps.hlsl");
}
TEST_F(FileTest, SemanticBarycentricsSmoothPS) {
  runFileTest("semantic.barycentrics.ps.s.hlsl");
}
TEST_F(FileTest, SemanticBarycentricsSmoothCentroidPS) {
  runFileTest("semantic.barycentrics.ps.s-c.hlsl");
}
TEST_F(FileTest, SemanticBarycentricsSmoothSamplePS) {
  runFileTest("semantic.barycentrics.ps.s-s.hlsl");
}
TEST_F(FileTest, SemanticBarycentricsNoPerspectivePS) {
  runFileTest("semantic.barycentrics.ps.np.hlsl");
}
TEST_F(FileTest, SemanticBarycentricsNoPerspectiveCentroidPS) {
  runFileTest("semantic.barycentrics.ps.np-c.hlsl");
}
TEST_F(FileTest, SemanticBarycentricsNoPerspectiveSamplePS) {
  runFileTest("semantic.barycentrics.ps.np-s.hlsl");
}

TEST_F(FileTest, SemanticCoveragePS) {
  runFileTest("semantic.coverage.ps.hlsl");
}
TEST_F(FileTest, SemanticCoverageTypeMismatchPS) {
  runFileTest("semantic.coverage.type-mismatch.ps.hlsl");
}
TEST_F(FileTest, SemanticInnerCoveragePS) {
  runFileTest("semantic.inner-coverage.ps.hlsl");
}
TEST_F(FileTest, SemanticViewIDVS) { runFileTest("semantic.view-id.vs.hlsl"); }
TEST_F(FileTest, SemanticViewIDHS) { runFileTest("semantic.view-id.hs.hlsl"); }
TEST_F(FileTest, SemanticViewIDDS) { runFileTest("semantic.view-id.ds.hlsl"); }
TEST_F(FileTest, SemanticViewIDGS) { runFileTest("semantic.view-id.gs.hlsl"); }
TEST_F(FileTest, SemanticViewIDPS) { runFileTest("semantic.view-id.ps.hlsl"); }

TEST_F(FileTest, SemanticArbitrary) { runFileTest("semantic.arbitrary.hlsl"); }
TEST_F(FileTest, SemanticArbitraryDeclLocation) {
  runFileTest("semantic.arbitrary.location.decl.hlsl");
}
TEST_F(FileTest, SemanticArbitraryAlphaLocation) {
  runFileTest("semantic.arbitrary.location.alpha.hlsl");
}
TEST_F(FileTest, SemanticDuplication) {
  runFileTest("semantic.duplication.hlsl", Expect::Failure);
}
TEST_F(FileTest, SemanticOnStruct) { runFileTest("semantic.on-struct.hlsl"); }

// For texture methods
TEST_F(FileTest, TextureSample) { runFileTest("texture.sample.hlsl"); }
TEST_F(FileTest, TextureArraySample) {
  runFileTest("texture.array.sample.hlsl");
}
TEST_F(FileTest, TextureLoad) { runFileTest("texture.load.hlsl"); }
TEST_F(FileTest, TextureArrayLoad) { runFileTest("texture.array.load.hlsl"); }
TEST_F(FileTest, TextureGetDimensions) {
  runFileTest("texture.get-dimensions.hlsl");
}
TEST_F(FileTest, TextureGetSamplePosition) {
  runFileTest("texture.get-sample-position.hlsl");
}
TEST_F(FileTest, TextureCalculateLevelOfDetail) {
  runFileTest("texture.calculate-lod.hlsl");
}
TEST_F(FileTest, TextureCalculateLevelOfDetailUnclamped) {
  runFileTest("texture.calculate-lod-unclamped.hlsl");
}
TEST_F(FileTest, TextureGather) { runFileTest("texture.gather.hlsl"); }
TEST_F(FileTest, TextureArrayGather) {
  runFileTest("texture.array.gather.hlsl");
}
TEST_F(FileTest, TextureGatherRed) { runFileTest("texture.gather-red.hlsl"); }
TEST_F(FileTest, TextureArrayGatherRed) {
  runFileTest("texture.array.gather-red.hlsl");
}
TEST_F(FileTest, TextureGatherGreen) {
  runFileTest("texture.gather-green.hlsl");
}
TEST_F(FileTest, TextureArrayGatherGreen) {
  runFileTest("texture.array.gather-green.hlsl");
}
TEST_F(FileTest, TextureGatherBlue) { runFileTest("texture.gather-blue.hlsl"); }
TEST_F(FileTest, TextureArrayGatherBlue) {
  runFileTest("texture.array.gather-blue.hlsl");
}
TEST_F(FileTest, TextureGatherAlpha) {
  runFileTest("texture.gather-alpha.hlsl");
}
TEST_F(FileTest, TextureArrayGatherAlpha) {
  runFileTest("texture.array.gather-alpha.hlsl");
}
TEST_F(FileTest, TextureGatherCmp) { runFileTest("texture.gather-cmp.hlsl"); }
TEST_F(FileTest, TextureArrayGatherCmp) {
  runFileTest("texture.array.gather-cmp.hlsl");
}
TEST_F(FileTest, TextureGatherCmpRed) {
  runFileTest("texture.gather-cmp-red.hlsl");
}
TEST_F(FileTest, TextureArrayGatherCmpRed) {
  runFileTest("texture.array.gather-cmp-red.hlsl");
}
TEST_F(FileTest, TextureArrayGatherCmpGreen) {
  runFileTest("texture.gather-cmp-green.hlsl", Expect::Failure);
}
TEST_F(FileTest, TextureArrayGatherCmpBlue) {
  runFileTest("texture.gather-cmp-blue.hlsl", Expect::Failure);
}
TEST_F(FileTest, TextureArrayGatherCmpAlpha) {
  runFileTest("texture.gather-cmp-alpha.hlsl", Expect::Failure);
}
TEST_F(FileTest, TextureSampleLevel) {
  runFileTest("texture.sample-level.hlsl");
}
TEST_F(FileTest, TextureArraySampleLevel) {
  runFileTest("texture.array.sample-level.hlsl");
}
TEST_F(FileTest, TextureSampleBias) { runFileTest("texture.sample-bias.hlsl"); }
TEST_F(FileTest, TextureArraySampleBias) {
  runFileTest("texture.array.sample-bias.hlsl");
}
TEST_F(FileTest, TextureSampleGrad) { runFileTest("texture.sample-grad.hlsl"); }
TEST_F(FileTest, TextureArraySampleGrad) {
  runFileTest("texture.array.sample-grad.hlsl");
}
TEST_F(FileTest, TextureSampleCmp) { runFileTest("texture.sample-cmp.hlsl"); }
TEST_F(FileTest, TextureArraySampleCmp) {
  runFileTest("texture.array.sample-cmp.hlsl");
}
TEST_F(FileTest, TextureSampleCmpLevelZero) {
  runFileTest("texture.sample-cmp-level-zero.hlsl");
}
TEST_F(FileTest, TextureArraySampleCmpLevelZero) {
  runFileTest("texture.array.sample-cmp-level-zero.hlsl");
}

// For structured buffer methods
TEST_F(FileTest, StructuredBufferLoad) {
  runFileTest("method.structured-buffer.load.hlsl");
}
TEST_F(FileTest, StructuredBufferGetDimensions) {
  runFileTest("method.structured-buffer.get-dimensions.hlsl");
}
TEST_F(FileTest, RWStructuredBufferIncDecCounter) {
  runFileTest("method.rw-structured-buffer.counter.hlsl");
}
TEST_F(FileTest, AppendStructuredBufferAppend) {
  runFileTest("method.append-structured-buffer.append.hlsl");
}
TEST_F(FileTest, AppendStructuredBufferGetDimensions) {
  runFileTest("method.append-structured-buffer.get-dimensions.hlsl");
}
TEST_F(FileTest, ConsumeStructuredBufferConsume) {
  runFileTest("method.consume-structured-buffer.consume.hlsl");
}
TEST_F(FileTest, ConsumeStructuredBufferGetDimensions) {
  runFileTest("method.consume-structured-buffer.get-dimensions.hlsl");
}

// For ByteAddressBuffer methods
TEST_F(FileTest, ByteAddressBufferLoad) {
  runFileTest("method.byte-address-buffer.load.hlsl");
}
TEST_F(FileTest, ByteAddressBufferTemplatedLoadScalar) {
  runFileTest("method.byte-address-buffer.templated-load.scalar.hlsl");
}
TEST_F(FileTest, ByteAddressBufferTemplatedLoadVector) {
  runFileTest("method.byte-address-buffer.templated-load.vector.hlsl");
}
TEST_F(FileTest, ByteAddressBufferTemplatedLoadMatrix) {
  runFileTest("method.byte-address-buffer.templated-load.matrix.hlsl");
}
TEST_F(FileTest, ByteAddressBufferTemplatedLoadStruct) {
  runFileTest("method.byte-address-buffer.templated-load.struct.hlsl");
}
TEST_F(FileTest, ByteAddressBufferTemplatedLoadStruct2) {
  runFileTest("method.byte-address-buffer.templated-load.struct2.hlsl");
}
TEST_F(FileTest, ByteAddressBufferTemplatedLoadStruct3) {
  runFileTest("method.byte-address-buffer.templated-load.struct3.hlsl");
}
TEST_F(FileTest, ByteAddressBufferStore) {
  runFileTest("method.byte-address-buffer.store.hlsl");
}
TEST_F(FileTest, ByteAddressBufferTemplatedStoreStruct) {
  runFileTest("method.byte-address-buffer.templated-store.struct.hlsl");
}
TEST_F(FileTest, ByteAddressBufferTemplatedStoreStruct2) {
  runFileTest("method.byte-address-buffer.templated-store.struct.hlsl");
}
TEST_F(FileTest, ByteAddressBufferGetDimensions) {
  runFileTest("method.byte-address-buffer.get-dimensions.hlsl");
}
TEST_F(FileTest, RWByteAddressBufferAtomicMethods) {
  runFileTest("method.rw-byte-address-buffer.atomic.hlsl");
}

// For Buffer/RWBuffer methods
TEST_F(FileTest, BufferLoad) { runFileTest("method.buffer.load.hlsl"); }
TEST_F(FileTest, BufferGetDimensions) {
  runFileTest("method.buffer.get-dimensions.hlsl");
}

// For RWTexture methods
TEST_F(FileTest, RWTextureLoad) { runFileTest("method.rwtexture.load.hlsl"); }
TEST_F(FileTest, RWTextureGetDimensions) {
  runFileTest("method.rwtexture.get-dimensions.hlsl");
}

// For InputPatch and OutputPatch methods
TEST_F(FileTest, InputOutputPatchAccess) {
  runFileTest("method.input-output-patch.access.hlsl");
}

// For intrinsic functions
TEST_F(FileTest, IntrinsicsCountBits) {
  runFileTest("intrinsics.countbits.hlsl");
}
TEST_F(FileTest, IntrinsicsDot) { runFileTest("intrinsics.dot.hlsl"); }
TEST_F(FileTest, IntrinsicsMul) { runFileTest("intrinsics.mul.hlsl"); }
TEST_F(FileTest, IntrinsicsAll) { runFileTest("intrinsics.all.hlsl"); }
TEST_F(FileTest, IntrinsicsAny) { runFileTest("intrinsics.any.hlsl"); }
TEST_F(FileTest, IntrinsicsAsDouble) {
  runFileTest("intrinsics.asdouble.hlsl");
}
TEST_F(FileTest, IntrinsicsAsfloat) { runFileTest("intrinsics.asfloat.hlsl"); }
TEST_F(FileTest, IntrinsicsAsint) { runFileTest("intrinsics.asint.hlsl"); }
TEST_F(FileTest, IntrinsicsAsuint) { runFileTest("intrinsics.asuint.hlsl"); }
TEST_F(FileTest, IntrinsicsRound) { runFileTest("intrinsics.round.hlsl"); }
TEST_F(FileTest, IntrinsicsAbs) { runFileTest("intrinsics.abs.hlsl"); }
TEST_F(FileTest, IntrinsicsCross) { runFileTest("intrinsics.cross.hlsl"); }
TEST_F(FileTest, IntrinsicsCeil) { runFileTest("intrinsics.ceil.hlsl"); }
TEST_F(FileTest, IntrinsicsClamp) { runFileTest("intrinsics.clamp.hlsl"); }
TEST_F(FileTest, IntrinsicsClip) { runFileTest("intrinsics.clip.hlsl"); }
TEST_F(FileTest, IntrinsicsD3DCOLORtoUBYTE4) {
  runFileTest("intrinsics.D3DCOLORtoUBYTE4.hlsl");
}
TEST_F(FileTest, IntrinsicsDegrees) { runFileTest("intrinsics.degrees.hlsl"); }
TEST_F(FileTest, IntrinsicsDistance) {
  runFileTest("intrinsics.distance.hlsl");
}
TEST_F(FileTest, IntrinsicsRadians) { runFileTest("intrinsics.radians.hlsl"); }
TEST_F(FileTest, IntrinsicsDdx) { runFileTest("intrinsics.ddx.hlsl"); }
TEST_F(FileTest, IntrinsicsDdy) { runFileTest("intrinsics.ddy.hlsl"); }
TEST_F(FileTest, IntrinsicsDdxCoarse) {
  runFileTest("intrinsics.ddx-coarse.hlsl");
}
TEST_F(FileTest, IntrinsicsDdyCoarse) {
  runFileTest("intrinsics.ddy-coarse.hlsl");
}
TEST_F(FileTest, IntrinsicsDdxFine) { runFileTest("intrinsics.ddx-fine.hlsl"); }
TEST_F(FileTest, IntrinsicsDdyFine) { runFileTest("intrinsics.ddy-fine.hlsl"); }
TEST_F(FileTest, IntrinsicsDeterminant) {
  runFileTest("intrinsics.determinant.hlsl");
}
TEST_F(FileTest, IntrinsicsDst) { runFileTest("intrinsics.dst.hlsl"); }
TEST_F(FileTest, IntrinsicsExp) { runFileTest("intrinsics.exp.hlsl"); }
TEST_F(FileTest, IntrinsicsExp2) { runFileTest("intrinsics.exp2.hlsl"); }
TEST_F(FileTest, IntrinsicsF16ToF32) {
  runFileTest("intrinsics.f16tof32.hlsl");
}
TEST_F(FileTest, IntrinsicsF32ToF16) {
  runFileTest("intrinsics.f32tof16.hlsl");
}
TEST_F(FileTest, IntrinsicsFaceForward) {
  runFileTest("intrinsics.faceforward.hlsl");
}
TEST_F(FileTest, IntrinsicsFirstBitHigh) {
  runFileTest("intrinsics.firstbithigh.hlsl");
}
TEST_F(FileTest, IntrinsicsFirstBitLow) {
  runFileTest("intrinsics.firstbitlow.hlsl");
}
TEST_F(FileTest, IntrinsicsFloor) { runFileTest("intrinsics.floor.hlsl"); }
TEST_F(FileTest, IntrinsicsFma) { runFileTest("intrinsics.fma.hlsl"); }
TEST_F(FileTest, IntrinsicsFmod) { runFileTest("intrinsics.fmod.hlsl"); }
TEST_F(FileTest, IntrinsicsFrac) { runFileTest("intrinsics.frac.hlsl"); }
TEST_F(FileTest, IntrinsicsFrexp) { runFileTest("intrinsics.frexp.hlsl"); }
TEST_F(FileTest, IntrinsicsFwidth) { runFileTest("intrinsics.fwidth.hlsl"); }
TEST_F(FileTest, IntrinsicsDeviceMemoryBarrier) {
  runFileTest("intrinsics.devicememorybarrier.hlsl");
}
TEST_F(FileTest, IntrinsicsAllMemoryBarrier) {
  runFileTest("intrinsics.allmemorybarrier.hlsl");
}
TEST_F(FileTest, IntrinsicsAllMemoryBarrierWithGroupSync) {
  runFileTest("intrinsics.allmemorybarrierwithgroupsync.hlsl");
}
TEST_F(FileTest, IntrinsicsDeviceMemoryBarrierWithGroupSync) {
  runFileTest("intrinsics.devicememorybarrierwithgroupsync.hlsl");
}
TEST_F(FileTest, IntrinsicsGroupMemoryBarrier) {
  runFileTest("intrinsics.groupmemorybarrier.hlsl");
}
TEST_F(FileTest, IntrinsicsGroupMemoryBarrierWithGroupSync) {
  runFileTest("intrinsics.groupmemorybarrierwithgroupsync.hlsl");
}
TEST_F(FileTest, IntrinsicsIsFinite) {
  runFileTest("intrinsics.isfinite.hlsl");
}
TEST_F(FileTest, IntrinsicsInterlockedMethodsPS) {
  runFileTest("intrinsics.interlocked-methods.ps.hlsl");
}
TEST_F(FileTest, IntrinsicsInterlockedMethodsCS) {
  runFileTest("intrinsics.interlocked-methods.cs.hlsl");
}
TEST_F(FileTest, IntrinsicsInterlockedMethodsError) {
  runFileTest("intrinsics.interlocked-methods.error.hlsl", Expect::Failure);
}
TEST_F(FileTest, IntrinsicsInterlockedMethodsStaticError) {
  runFileTest("intrinsics.interlocked-methods.static-error.hlsl",
              Expect::Failure);
}
TEST_F(FileTest, IntrinsicsInterlockedMethodsTextureSwizzling) {
  runFileTest("intrinsics.interlocked-methods.texture.swizzling.hlsl");
}
TEST_F(FileTest, IntrinsicsInterlockedMethodsTextureSwizzlingError) {
  runFileTest("intrinsics.interlocked-methods.texture.swizzling.error.hlsl",
              Expect::Failure);
}
TEST_F(FileTest, IntrinsicsIsInf) { runFileTest("intrinsics.isinf.hlsl"); }
TEST_F(FileTest, IntrinsicsIsNan) { runFileTest("intrinsics.isnan.hlsl"); }
TEST_F(FileTest, IntrinsicsLength) { runFileTest("intrinsics.length.hlsl"); }
TEST_F(FileTest, IntrinsicsLdexp) { runFileTest("intrinsics.ldexp.hlsl"); }
TEST_F(FileTest, IntrinsicsLerp) { runFileTest("intrinsics.lerp.hlsl"); }
TEST_F(FileTest, IntrinsicsLog) { runFileTest("intrinsics.log.hlsl"); }
TEST_F(FileTest, IntrinsicsLog10) { runFileTest("intrinsics.log10.hlsl"); }
TEST_F(FileTest, IntrinsicsLog2) { runFileTest("intrinsics.log2.hlsl"); }
TEST_F(FileTest, IntrinsicsMin) { runFileTest("intrinsics.min.hlsl"); }
TEST_F(FileTest, IntrinsicsLit) { runFileTest("intrinsics.lit.hlsl"); }
TEST_F(FileTest, IntrinsicsModf) { runFileTest("intrinsics.modf.hlsl"); }
TEST_F(FileTest, IntrinsicsModfWithSwizzling) {
  runFileTest("intrinsics.modf.swizzle.hlsl");
}
TEST_F(FileTest, IntrinsicsMad) { runFileTest("intrinsics.mad.hlsl"); }
TEST_F(FileTest, IntrinsicsUMad) { runFileTest("intrinsics.umad.hlsl"); }
TEST_F(FileTest, IntrinsicsMax) { runFileTest("intrinsics.max.hlsl"); }
TEST_F(FileTest, IntrinsicsMsad4) { runFileTest("intrinsics.msad4.hlsl"); }
TEST_F(FileTest, IntrinsicsNormalize) {
  runFileTest("intrinsics.normalize.hlsl");
}
TEST_F(FileTest, IntrinsicsPow) { runFileTest("intrinsics.pow.hlsl"); }
TEST_F(FileTest, IntrinsicsRsqrt) { runFileTest("intrinsics.rsqrt.hlsl"); }
TEST_F(FileTest, IntrinsicsFloatSign) {
  runFileTest("intrinsics.floatsign.hlsl");
}
TEST_F(FileTest, IntrinsicsIntSign) { runFileTest("intrinsics.intsign.hlsl"); }
TEST_F(FileTest, IntrinsicsRcp) { runFileTest("intrinsics.rcp.hlsl"); }
TEST_F(FileTest, IntrinsicsReflect) { runFileTest("intrinsics.reflect.hlsl"); }
TEST_F(FileTest, IntrinsicsRefract) { runFileTest("intrinsics.refract.hlsl"); }
TEST_F(FileTest, IntrinsicsReverseBits) {
  runFileTest("intrinsics.reversebits.hlsl");
}
TEST_F(FileTest, IntrinsicsSaturate) {
  runFileTest("intrinsics.saturate.hlsl");
}
TEST_F(FileTest, IntrinsicsSmoothStep) {
  runFileTest("intrinsics.smoothstep.hlsl");
}
TEST_F(FileTest, IntrinsicsStep) { runFileTest("intrinsics.step.hlsl"); }
TEST_F(FileTest, IntrinsicsSqrt) { runFileTest("intrinsics.sqrt.hlsl"); }
TEST_F(FileTest, IntrinsicsTranspose) {
  runFileTest("intrinsics.transpose.hlsl");
}
TEST_F(FileTest, IntrinsicsTrunc) { runFileTest("intrinsics.trunc.hlsl"); }

// For intrinsic trigonometric functions
TEST_F(FileTest, IntrinsicsSin) { runFileTest("intrinsics.sin.hlsl"); }
TEST_F(FileTest, IntrinsicsCos) { runFileTest("intrinsics.cos.hlsl"); }
TEST_F(FileTest, IntrinsicsSinCos) { runFileTest("intrinsics.sincos.hlsl"); }
TEST_F(FileTest, IntrinsicsTan) { runFileTest("intrinsics.tan.hlsl"); }
TEST_F(FileTest, IntrinsicsSinh) { runFileTest("intrinsics.sinh.hlsl"); }
TEST_F(FileTest, IntrinsicsCosh) { runFileTest("intrinsics.cosh.hlsl"); }
TEST_F(FileTest, IntrinsicsTanh) { runFileTest("intrinsics.tanh.hlsl"); }
TEST_F(FileTest, IntrinsicsAsin) { runFileTest("intrinsics.asin.hlsl"); }
TEST_F(FileTest, IntrinsicsAcos) { runFileTest("intrinsics.acos.hlsl"); }
TEST_F(FileTest, IntrinsicsAtan) { runFileTest("intrinsics.atan.hlsl"); }
TEST_F(FileTest, IntrinsicsAtan2) { runFileTest("intrinsics.atan2.hlsl"); }
TEST_F(FileTest, IntrinsicsAtanFp16) {
  // Float16 capability should be emitted for usage of fp16 in the extended
  // instruction set.
  runFileTest("intrinsics.atan.fp16.hlsl");
}

// Unspported intrinsic functions
TEST_F(FileTest, IntrinsicsAbort) {
  runFileTest("intrinsics.abort.hlsl", Expect::Failure);
}
TEST_F(FileTest, IntrinsicsCheckAccessFullyMapped) {
  runFileTest("intrinsics.check-access-fully-mapped.hlsl");
}
TEST_F(FileTest, IntrinsicsCheckAccessFullyMappedWithoutSampler) {
  runFileTest("intrinsics.check-access-fully-mapped.without-sampler.hlsl");
}
TEST_F(FileTest, IntrinsicsGetRenderTargetSampleCount) {
  runFileTest("intrinsics.get-render-target-sample-count.hlsl",
              Expect::Failure);
}
TEST_F(FileTest, IntrinsicsGetRenderTargetSamplePosition) {
  runFileTest("intrinsics.get-render-target-sample-position.hlsl",
              Expect::Failure);
}
TEST_F(FileTest, IntrinsicsNonUniformResourceIndex) {
  runFileTest("intrinsics.non-uniform-resource-index.hlsl");
}

// For attributes
TEST_F(FileTest, AttributeEarlyDepthStencil) {
  runFileTest("attribute.earlydepthstencil.ps.hlsl");
}
TEST_F(FileTest, AttributePostDepthCoverage) {
  runFileTest("attribute.postdepthcoverage.ps.hlsl");
}
TEST_F(FileTest, AttributeNumThreads) {
  runFileTest("attribute.numthreads.hlsl");
}
TEST_F(FileTest, AttributeMissingNumThreads) {
  runFileTest("attribute.numthreads.missing.hlsl");
}
TEST_F(FileTest, AttributeDomainTri) {
  runFileTest("attribute.domain.tri.hlsl");
}
TEST_F(FileTest, AttributeDomainQuad) {
  runFileTest("attribute.domain.quad.hlsl");
}
TEST_F(FileTest, AttributeDomainIsoline) {
  runFileTest("attribute.domain.isoline.hlsl");
}
TEST_F(FileTest, AttributePartitioningInteger) {
  runFileTest("attribute.partitioning.integer.hlsl");
}
TEST_F(FileTest, AttributePartitioningFractionalEven) {
  runFileTest("attribute.partitioning.fractional-even.hlsl");
}
TEST_F(FileTest, AttributePartitioningFractionalOdd) {
  runFileTest("attribute.partitioning.fractional-odd.hlsl");
}
TEST_F(FileTest, AttributeOutputTopologyPoint) {
  runFileTest("attribute.outputtopology.point.hlsl");
}
TEST_F(FileTest, AttributeOutputTopologyTriangleCw) {
  runFileTest("attribute.outputtopology.triangle-cw.hlsl");
}
TEST_F(FileTest, AttributeOutputTopologyTriangleCcw) {
  runFileTest("attribute.outputtopology.triangle-ccw.hlsl");
}
TEST_F(FileTest, AttributeOutputControlPoints) {
  runFileTest("attribute.outputcontrolpoints.hlsl");
}
TEST_F(FileTest, AttributeMaxVertexCount) {
  runFileTest("attribute.max-vertex-count.hlsl");
}
TEST_F(FileTest, AttributeInstanceGS) {
  runFileTest("attribute.instance.gs.hlsl");
}
TEST_F(FileTest, AttributeInstanceMissingGS) {
  runFileTest("attribute.instance.missing.gs.hlsl");
}

// For geometry shader primitive types
TEST_F(FileTest, PrimitivePointGS) { runFileTest("primitive.point.gs.hlsl"); }
TEST_F(FileTest, PrimitiveLineGS) { runFileTest("primitive.line.gs.hlsl"); }
TEST_F(FileTest, PrimitiveTriangleGS) {
  runFileTest("primitive.triangle.gs.hlsl");
}
TEST_F(FileTest, PrimitiveLineAdjGS) {
  runFileTest("primitive.lineadj.gs.hlsl");
}
TEST_F(FileTest, PrimitiveTriangleAdjGS) {
  runFileTest("primitive.triangleadj.gs.hlsl");
}
TEST_F(FileTest, PrimitiveErrorGS) {
  runFileTest("primitive.error.gs.hlsl", Expect::Failure);
}

// Shader model 6.0 wave query
TEST_F(FileTest, SM6WaveIsFirstLane) {
  useVulkan1p1();
  runFileTest("sm6.wave-is-first-lane.hlsl");
}
TEST_F(FileTest, SM6WaveGetLaneCount) {
  useVulkan1p1();
  runFileTest("sm6.wave-get-lane-count.hlsl");
}
TEST_F(FileTest, SM6WaveGetLaneIndex) {
  useVulkan1p1();
  runFileTest("sm6.wave-get-lane-index.hlsl");
}
TEST_F(FileTest, SM6WaveBuiltInNoDuplicate) {
  useVulkan1p1();
  runFileTest("sm6.wave.builtin.no-dup.hlsl");
}

// Shader model 6.0 wave vote
TEST_F(FileTest, SM6WaveActiveAnyTrue) {
  useVulkan1p1();
  runFileTest("sm6.wave-active-any-true.hlsl");
}
TEST_F(FileTest, SM6WaveActiveAllTrue) {
  useVulkan1p1();
  runFileTest("sm6.wave-active-all-true.hlsl");
}
TEST_F(FileTest, SM6WaveActiveBallot) {
  useVulkan1p1();
  runFileTest("sm6.wave-active-ballot.hlsl");
}

// Shader model 6.0 wave reduction
TEST_F(FileTest, SM6WaveActiveAllEqual) {
  useVulkan1p1();
  runFileTest("sm6.wave-active-all-equal.hlsl");
}
TEST_F(FileTest, SM6WaveActiveSum) {
  useVulkan1p1();
  runFileTest("sm6.wave-active-sum.hlsl");
}
TEST_F(FileTest, SM6WaveActiveProduct) {
  useVulkan1p1();
  runFileTest("sm6.wave-active-product.hlsl");
}
TEST_F(FileTest, SM6WaveActiveMax) {
  useVulkan1p1();
  runFileTest("sm6.wave-active-max.hlsl");
}
TEST_F(FileTest, SM6WaveActiveMin) {
  useVulkan1p1();
  runFileTest("sm6.wave-active-min.hlsl");
}
TEST_F(FileTest, SM6WaveActiveBitAnd) {
  useVulkan1p1();
  runFileTest("sm6.wave-active-bit-and.hlsl");
}
TEST_F(FileTest, SM6WaveActiveBitOr) {
  useVulkan1p1();
  runFileTest("sm6.wave-active-bit-or.hlsl");
}
TEST_F(FileTest, SM6WaveActiveBitXor) {
  useVulkan1p1();
  runFileTest("sm6.wave-active-bit-xor.hlsl");
}
TEST_F(FileTest, SM6WaveActiveCountBits) {
  useVulkan1p1();
  runFileTest("sm6.wave-active-count-bits.hlsl");
}

// Shader model 6.0 wave scan/prefix
TEST_F(FileTest, SM6WavePrefixSum) {
  useVulkan1p1();
  runFileTest("sm6.wave-prefix-sum.hlsl");
}
TEST_F(FileTest, SM6WavePrefixProduct) {
  useVulkan1p1();
  runFileTest("sm6.wave-prefix-product.hlsl");
}
TEST_F(FileTest, SM6WavePrefixCountBits) {
  useVulkan1p1();
  runFileTest("sm6.wave-prefix-count-bits.hlsl");
}

// Shader model 6.0 wave broadcast
TEST_F(FileTest, SM6WaveReadLaneAt) {
  useVulkan1p1();
  runFileTest("sm6.wave-read-lane-at.hlsl");
}
TEST_F(FileTest, SM6WaveReadLaneFirst) {
  useVulkan1p1();
  runFileTest("sm6.wave-read-lane-first.hlsl");
}

// Shader model 6.0 wave quad-wide shuffle
TEST_F(FileTest, SM6QuadReadAcrossX) {
  useVulkan1p1();
  runFileTest("sm6.quad-read-across-x.hlsl");
}
TEST_F(FileTest, SM6QuadReadAcrossY) {
  useVulkan1p1();
  runFileTest("sm6.quad-read-across-y.hlsl");
}
TEST_F(FileTest, SM6QuadReadAcrossDiagonal) {
  useVulkan1p1();
  runFileTest("sm6.quad-read-across-diagonal.hlsl");
}
TEST_F(FileTest, SM6QuadReadLaneAt) {
  useVulkan1p1();
  runFileTest("sm6.quad-read-lane-at.hlsl");
}

// Test error on using wave ops on Vulkan 1.0 target environment.
TEST_F(FileTest, WaveOpVulkan1Error) {
  runFileTest("sm6.wave-op.target-vulkan1.error.hlsl", Expect::Failure);
}
TEST_F(FileTest, WaveOpNoTargetEnvError) {
  runFileTest("sm6.wave-op.no-target-env.error.hlsl", Expect::Failure);
}

// SPIR-V specific
TEST_F(FileTest, SpirvStorageClass) { runFileTest("spirv.storage-class.hlsl"); }

TEST_F(FileTest, SpirvControlFlowMissingReturn) {
  runFileTest("spirv.cf.ret-missing.hlsl");
}

TEST_F(FileTest, SpirvEntryFunctionWrapper) {
  runFileTest("spirv.entry-function.wrapper.hlsl");
}
TEST_F(FileTest, SpirvEntryFunctionInOut) {
  runFileTest("spirv.entry-function.inout.hlsl");
}
TEST_F(FileTest, SpirvEntryFunctionUnusedParameter) {
  runFileTest("spirv.entry-function.unused-param.hlsl");
}

TEST_F(FileTest, SpirvBuiltInHelperInvocation) {
  runFileTest("spirv.builtin.helper-invocation.hlsl");
}
TEST_F(FileTest, SpirvBuiltInHelperInvocationInvalidUsage) {
  runFileTest("spirv.builtin.helper-invocation.invalid.hlsl", Expect::Failure);
}
TEST_F(FileTest, SpirvBuiltInPointSizeInvalidUsage) {
  runFileTest("spirv.builtin.point-size.invalid.hlsl", Expect::Failure);
}
TEST_F(FileTest, SpirvBuiltInShaderDrawParameters) {
  runFileTest("spirv.builtin.shader-draw-parameters.hlsl");
}
TEST_F(FileTest, SpirvBuiltInShaderDrawParametersInvalidUsage) {
  runFileTest("spirv.builtin.shader-draw-parameters.invalid.hlsl",
              Expect::Failure);
}
TEST_F(FileTest, SpirvBuiltInDeviceIndex) {
  runFileTest("spirv.builtin.device-index.hlsl");
}
TEST_F(FileTest, SpirvBuiltInDeviceIndexInvalidUsage) {
  runFileTest("spirv.builtin.device-index.invalid.hlsl", Expect::Failure);
}

TEST_F(FileTest, SpirvExtensionCLAllow) {
  runFileTest("spirv.ext.cl.allow.hlsl");
}
TEST_F(FileTest, SpirvExtensionCLForbid) {
  runFileTest("spirv.ext.cl.forbid.hlsl", Expect::Failure);
}
TEST_F(FileTest, SpirvExtensionCLUnknown) {
  runFileTest("spirv.ext.cl.unknown.hlsl", Expect::Failure);
}
TEST_F(FileTest, SpirvExtensionAllowAllKHR) {
  runFileTest("spirv.ext.allow-all-khr.hlsl");
}
// Test -Oconfig command line option.
TEST_F(FileTest, SpirvOptOconfigMultipleUses) {
  runFileTest("spirv.opt.multiple.cl.oconfig.hlsl", Expect::Failure);
}
TEST_F(FileTest, SpirvOptOconfigAndO0) {
  runFileTest("spirv.opt.with-O0.cl.oconfig.hlsl", Expect::Failure);
}
TEST_F(FileTest, SpirvOptOconfigAndO1) {
  runFileTest("spirv.opt.with-O1.cl.oconfig.hlsl", Expect::Failure);
}
TEST_F(FileTest, SpirvOptOconfigAndO2) {
  runFileTest("spirv.opt.with-O2.cl.oconfig.hlsl", Expect::Failure);
}
TEST_F(FileTest, SpirvOptOconfigAndO3) {
  runFileTest("spirv.opt.with-O3.cl.oconfig.hlsl", Expect::Failure);
}
TEST_F(FileTest, SpirvOptOconfigInvalidFlag) {
  runFileTest("spirv.opt.invalid-flag.cl.oconfig.hlsl", Expect::Failure);
}
TEST_F(FileTest, SpirvOptOconfig) { runFileTest("spirv.opt.cl.oconfig.hlsl"); }

// For shader stage input/output interface
// For semantic SV_Position, SV_ClipDistance, SV_CullDistance
TEST_F(FileTest, SpirvStageIOInterfaceVS) {
  runFileTest("spirv.interface.vs.hlsl");
}
TEST_F(FileTest, SpirvStageIOInterfaceHS) {
  runFileTest("spirv.interface.hs.hlsl");
}
TEST_F(FileTest, SpirvStageIOInterfaceDS) {
  runFileTest("spirv.interface.ds.hlsl");
}
TEST_F(FileTest, SpirvStageIOInterfaceGS) {
  runFileTest("spirv.interface.gs.hlsl");
}
TEST_F(FileTest, SpirvStageIOInterfacePS) {
  runFileTest("spirv.interface.ps.hlsl");
}

TEST_F(FileTest, SpirvStageIOAliasBuiltIn) {
  runFileTest("spirv.interface.alias-builtin.hlsl");
}

// For testing UserSemantic decoration
TEST_F(FileTest, SpirvUserSemanticVS) {
  runFileTest("spirv.user-semantic.vs.hlsl");
}

TEST_F(FileTest, SpirvStageIO16bitTypes) {
  runFileTest("spirv.stage-io.16bit.hlsl");
}

TEST_F(FileTest, SpirvInterpolationPS) {
  runFileTest("spirv.interpolation.ps.hlsl");
}
TEST_F(FileTest, SpirvInterpolationVS) {
  runFileTest("spirv.interpolation.vs.hlsl");
}
TEST_F(FileTest, SpirvInterpolationError) {
  runFileTest("spirv.interpolation.error.hlsl", Expect::Failure);
}

TEST_F(FileTest, SpirvLegalizationOpaqueStruct) {
  setBeforeHLSLLegalization();
  runFileTest("spirv.legal.opaque-struct.hlsl");
}
TEST_F(FileTest, SpirvLegalizationStructuredBufferUsage) {
  setBeforeHLSLLegalization();
  runFileTest("spirv.legal.sbuffer.usage.hlsl");
}
TEST_F(FileTest, SpirvLegalizationStructuredBufferMethods) {
  setBeforeHLSLLegalization();
  runFileTest("spirv.legal.sbuffer.methods.hlsl");
}
TEST_F(FileTest, SpirvLegalizationStructuredBufferCounter) {
  setBeforeHLSLLegalization();
  runFileTest("spirv.legal.sbuffer.counter.hlsl");
}
TEST_F(FileTest, SpirvLegalizationStructuredBufferCounterInStruct) {
  // Tests using struct/class having associated counters
  setBeforeHLSLLegalization();
  runFileTest("spirv.legal.sbuffer.counter.struct.hlsl");
}
TEST_F(FileTest, SpirvLegalizationStructuredBufferCounterInMethod) {
  // Tests using methods whose enclosing struct/class having associated counters
  setBeforeHLSLLegalization();
  runFileTest("spirv.legal.sbuffer.counter.method.hlsl");
}
TEST_F(FileTest,
       SpirvLegalizationCounterVarAssignAcrossDifferentNestedStructLevel) {
  setBeforeHLSLLegalization();
  runFileTest("spirv.legal.counter.nested-struct.hlsl");
}
TEST_F(FileTest, SpirvLegalizationStructuredBufferInStruct) {
  setBeforeHLSLLegalization();
  runFileTest("spirv.legal.sbuffer.struct.hlsl");
}
TEST_F(FileTest, SpirvLegalizationConstantBuffer) {
  runFileTest("spirv.legal.cbuffer.hlsl");
}
TEST_F(FileTest, SpirvLegalizationTextureBuffer) {
  runFileTest("spirv.legal.tbuffer.hlsl", Expect::Success,
              // TODO: fix the different type error for OpStore
              /*runValidation=*/false);
}

TEST_F(FileTest, SpirvDebugOpSource) {
  runFileTest("spirv.debug.opsource.hlsl");
}

TEST_F(FileTest, SpirvDebugOpLine) { runFileTest("spirv.debug.opline.hlsl"); }
TEST_F(FileTest, SpirvDebugOpLineBranch) {
  runFileTest("spirv.debug.opline.branch.hlsl");
}
TEST_F(FileTest, SpirvDebugOpLinePrecendence) {
  runFileTest("spirv.debug.opline.precedence.hlsl");
}
TEST_F(FileTest, SpirvDebugOpLineComposite) {
  runFileTest("spirv.debug.opline.composite.hlsl");
}
TEST_F(FileTest, SpirvDebugOpLineEntry) {
  runFileTest("spirv.debug.opline.entry.hlsl");
}
TEST_F(FileTest, SpirvDebugOpLineFunction) {
  setBeforeHLSLLegalization();
  runFileTest("spirv.debug.opline.function.hlsl");
}
TEST_F(FileTest, SpirvDebugOpLineIntrinsics) {
  runFileTest("spirv.debug.opline.intrinsic.hlsl");
}
TEST_F(FileTest, SpirvDebugOpLineIntrinsicsControlBarrier) {
  runFileTest("spirv.debug.opline.intrinsic.control.barrier.hlsl");
}
TEST_F(FileTest, SpirvDebugOpLineIntrinsicsVulkan1_1) {
  useVulkan1p1();
  runFileTest("spirv.debug.opline.intrinsic.vulkan1.1.hlsl");
}
TEST_F(FileTest, SpirvDebugOpLineOperators) {
  runFileTest("spirv.debug.opline.operators.hlsl");
}
TEST_F(FileTest, SpirvDebugOpLineVariables) {
  runFileTest("spirv.debug.opline.variables.hlsl");
}
TEST_F(FileTest, SpirvDebugOpLineInclude) {
  runFileTest("spirv.debug.opline.include.hlsl");
}

TEST_F(FileTest, SpirvDebugDxcCommitInfo) {
  useVulkan1p1();
  runFileTest("spirv.debug.commit.hlsl");
}

// Test that command line options are exposed using OpModuleProcessed.
TEST_F(FileTest, SpirvDebugClOption) {
  useVulkan1p1();
  runFileTest("spirv.debug.cl-option.hlsl");
}

TEST_F(FileTest, SpirvDebugO1Option) {
  runFileTest("spirv.debug.o1.option.hlsl");
}

TEST_F(FileTest, SpirvDebugO2Option) {
  runFileTest("spirv.debug.o2.option.hlsl");
}

TEST_F(FileTest, SpirvDebugO3Option) {
  useVulkan1p1();
  runFileTest("spirv.debug.o3.option.hlsl");
}

TEST_F(FileTest, SpirvDebugControlFile) {
  useVulkan1p1();
  runFileTest("spirv.debug.ctrl.file.hlsl");
}
TEST_F(FileTest, SpirvDebugControlSource) {
  useVulkan1p1();
  runFileTest("spirv.debug.ctrl.source.hlsl");
}
TEST_F(FileTest, SpirvDebugControlLine) {
  useVulkan1p1();
  runFileTest("spirv.debug.ctrl.line.hlsl");
}
TEST_F(FileTest, SpirvDebugControlTool) {
  useVulkan1p1();
  runFileTest("spirv.debug.ctrl.tool.hlsl");
}
TEST_F(FileTest, SpirvDebugControlUnknown) {
  useVulkan1p1();
  runFileTest("spirv.debug.ctrl.unknown.hlsl", Expect::Failure);
}

TEST_F(FileTest, VulkanAttributeErrors) {
  runFileTest("vk.attribute.error.hlsl", Expect::Failure);
}
TEST_F(FileTest, VulkanAttributeInvalidUsages) {
  runFileTest("vk.attribute.invalid.hlsl", Expect::Failure);
}

TEST_F(FileTest, VulkanCLOptionInvertYVS) {
  runFileTest("vk.cloption.invert-y.vs.hlsl");
}
TEST_F(FileTest, VulkanCLOptionInvertYDS) {
  runFileTest("vk.cloption.invert-y.ds.hlsl");
}
TEST_F(FileTest, VulkanCLOptionInvertYGS) {
  runFileTest("vk.cloption.invert-y.gs.hlsl");
}

TEST_F(FileTest, VulkanCLOptionInvertWDS) {
  runFileTest("vk.cloption.invert-w.ds.hlsl");
}
TEST_F(FileTest, VulkanCLOptionInvertWPS) {
  runFileTest("vk.cloption.invert-w.ps.hlsl");
}

// Vulkan specific
TEST_F(FileTest, VulkanLocation) { runFileTest("vk.location.hlsl"); }
TEST_F(FileTest, VulkanLocationInputExplicitOutputImplicit) {
  runFileTest("vk.location.exp-in.hlsl");
}
TEST_F(FileTest, VulkanLocationInputImplicitOutputExplicit) {
  runFileTest("vk.location.exp-out.hlsl");
}
TEST_F(FileTest, VulkanLocationCompositeTypes) {
  runFileTest("vk.location.composite.hlsl");
}
TEST_F(FileTest, VulkanLocationTooLarge) {
  runFileTest("vk.location.large.hlsl", Expect::Failure);
}
TEST_F(FileTest, VulkanLocationReassigned) {
  runFileTest("vk.location.reassign.hlsl", Expect::Failure);
}
TEST_F(FileTest, VulkanLocationPartiallyAssigned) {
  runFileTest("vk.location.mixed.hlsl", Expect::Failure);
}

TEST_F(FileTest, VulkanExplicitBinding) {
  // Resource binding from [[vk::binding()]]
  runFileTest("vk.binding.explicit.hlsl");
}
TEST_F(FileTest, VulkanImplicitBinding) {
  // Resource binding from neither [[vk::binding()]] or :register()
  runFileTest("vk.binding.implicit.hlsl");
}
TEST_F(FileTest, VulkanPrecedenceBinding) {
  // Bindings from vk::binding and :register competing for dominance
  runFileTest("vk.binding.precedence.hlsl");
}
TEST_F(FileTest, VulkanRegisterBinding) {
  // Resource binding from :register(xX, spaceY)
  runFileTest("vk.binding.register.hlsl");
}
TEST_F(FileTest, VulkanSpaceOnlyRegisterBinding) {
  // Resource binding from :register(spaceY)
  runFileTest("vk.binding.register.space-only.hlsl");
}
TEST_F(FileTest, VulkanRegisterBindingShift) {
  // Resource binding from :register() with shift specified via
  // command line option
  runFileTest("vk.binding.cl.shift.hlsl");
}
TEST_F(FileTest, VulkanRegisterBindingShiftAllSets) {
  // Resource binding from :register() with shift specified for all sets via
  // command line option
  runFileTest("vk.binding.cl.shift.all-sets.hlsl");
}
TEST_F(FileTest, VulkanRegisterBinding1to1Mapping) {
  runFileTest("vk.binding.cl.register.hlsl");
}
TEST_F(FileTest, VulkanGlobalsBinding) {
  // Binding the $Globals buffer to a specific set/binding via command line
  // option.
  runFileTest("vk.binding.cl.globals.hlsl");
}
TEST_F(FileTest, VulkanGlobalsBindingRegisterBinding) {
  // Using command line option for specifying both the 1-1 register mapping as
  // well as $Globals binding.
  runFileTest("vk.binding.cl.register-and-globals.hlsl");
}
TEST_F(FileTest, VulkanRegisterBinding1to1MappingInvalidSpaceNo) {
  runFileTest("vk.binding.cl.register.invalid-space.hlsl", Expect::Failure);
}
TEST_F(FileTest, VulkanRegisterBinding1to1MappingInvalidSetNo) {
  runFileTest("vk.binding.cl.register.invalid-set.hlsl", Expect::Failure);
}
TEST_F(FileTest, VulkanRegisterBinding1to1MappingInvalidBindNo) {
  runFileTest("vk.binding.cl.register.invalid-bind.hlsl", Expect::Failure);
}
TEST_F(FileTest, VulkanRegisterBinding1to1MappingMissingAttr) {
  runFileTest("vk.binding.cl.register.missing-attr.hlsl", Expect::Failure);
}
TEST_F(FileTest, VulkanRegisterBinding1to1MappingMissingCLOption) {
  runFileTest("vk.binding.cl.register.missing-cl.hlsl", Expect::Failure);
}
TEST_F(FileTest, VulkanRegisterBinding1to1MappingAssociatedCounter) {
  runFileTest("vk.binding.cl.register.counter.hlsl", Expect::Failure);
}

// For flattening array of resources
TEST_F(FileTest, FlattenResourceArrayBindings1) {
  runFileTest("vk.binding.cl.flatten-arrays.example1.hlsl");
}
TEST_F(FileTest, FlattenResourceArrayBindings1Optimized) {
  runFileTest("vk.binding.cl.flatten-arrays.example1-optimized.hlsl");
}
TEST_F(FileTest, FlattenResourceArrayBindings2) {
  runFileTest("vk.binding.cl.flatten-arrays.example2.hlsl");
}
TEST_F(FileTest, FlattenResourceArrayBindings2Optimized) {
  runFileTest("vk.binding.cl.flatten-arrays.example2-optimized.hlsl");
}
TEST_F(FileTest, FlattenResourceArrayBindings3) {
  runFileTest("vk.binding.cl.flatten-arrays.example3.hlsl");
}

// For testing the "-auto-binding-space" command line option which specifies the
// "default space" for resources.
TEST_F(FileTest, VulkanRegisterBindingDefaultSpaceImplicit) {
  runFileTest("vk.binding.default-space.implicit.hlsl");
}
TEST_F(FileTest, VulkanRegisterBindingDefaultSpaceExplicit) {
  runFileTest("vk.binding.default-space.explicit.hlsl");
}
// Testing combinations of "-auto-binding-space" and other options.
TEST_F(FileTest, VulkanRegisterBindingDefaultSpaceWithShift) {
  runFileTest("vk.binding.default-space.with-shift.hlsl");
}
TEST_F(FileTest, VulkanRegisterBindingDefaultSpaceWithShiftAll) {
  runFileTest("vk.binding.default-space.with-shift-all.hlsl");
}

TEST_F(FileTest, VulkanStructuredBufferCounter) {
  // [[vk::counter_binding()]] for RWStructuredBuffer, AppendStructuredBuffer,
  // and ConsumeStructuredBuffer
  runFileTest("vk.binding.counter.hlsl");
}

TEST_F(FileTest, VulkanPushConstant) { runFileTest("vk.push-constant.hlsl"); }
TEST_F(FileTest, VulkanPushConstantOffset) {
  // Checks the behavior of [[vk::offset]] with [[vk::push_constant]]
  runFileTest("vk.push-constant.offset.hlsl");
}
TEST_F(FileTest, VulkanPushConstantAnonymousStruct) {
  runFileTest("vk.push-constant.anon-struct.hlsl");
}
TEST_F(FileTest, VulkanMultiplePushConstant) {
  runFileTest("vk.push-constant.multiple.hlsl", Expect::Failure);
}

TEST_F(FileTest, VulkanSpecConstantInit) {
  runFileTest("vk.spec-constant.init.hlsl");
}
TEST_F(FileTest, VulkanSpecConstantUsage) {
  runFileTest("vk.spec-constant.usage.hlsl");
}
TEST_F(FileTest, VulkanSpecConstantError) {
  runFileTest("vk.spec-constant.error.hlsl", Expect::Failure);
}

TEST_F(FileTest, VulkanLayoutCBufferMatrixZpr) {
  runFileTest("vk.layout.cbuffer.zpr.hlsl");
}
TEST_F(FileTest, VulkanLayoutCBufferMatrixZpc) {
  runFileTest("vk.layout.cbuffer.zpc.hlsl");
}
TEST_F(FileTest, VulkanLayoutCBufferStd140) {
  setGlLayout();
  runFileTest("vk.layout.cbuffer.std140.hlsl");
}
TEST_F(FileTest, VulkanLayoutCBufferNestedStd140) {
  setGlLayout();
  runFileTest("vk.layout.cbuffer.nested.std140.hlsl");
}
TEST_F(FileTest, VulkanLayoutCBufferNestedEmptyStd140) {
  setGlLayout();
  runFileTest("vk.layout.cbuffer.nested.empty.std140.hlsl");
}
TEST_F(FileTest, VulkanLayoutCBufferBoolean) {
  runFileTest("vk.layout.cbuffer.boolean.hlsl");
}
TEST_F(FileTest, VulkanLayoutCBufferDerivedStruct) {
  runFileTest("vk.layout.cbuffer.derived-struct.hlsl");
}
TEST_F(FileTest, VulkanLayoutRWStructuredBufferBoolean) {
  runFileTest("vk.layout.rwstructuredbuffer.boolean.hlsl");
}
TEST_F(FileTest, VulkanLayoutSBufferStd430) {
  setGlLayout();
  runFileTest("vk.layout.sbuffer.std430.hlsl");
}
TEST_F(FileTest, VulkanLayoutSBufferNestedStd430) {
  setGlLayout();
  runFileTest("vk.layout.sbuffer.nested.std430.hlsl");
}
TEST_F(FileTest, VulkanLayoutAppendSBufferStd430) {
  setGlLayout();
  runFileTest("vk.layout.asbuffer.std430.hlsl");
}
TEST_F(FileTest, VulkanLayoutConsumeSBufferStd430) {
  setGlLayout();
  runFileTest("vk.layout.csbuffer.std430.hlsl");
}
TEST_F(FileTest, VulkanLayoutTBufferStd430) {
  setGlLayout();
  runFileTest("vk.layout.tbuffer.std430.hlsl");
}
TEST_F(FileTest, VulkanLayoutTextureBufferStd430) {
  setGlLayout();
  runFileTest("vk.layout.texture-buffer.std430.hlsl");
}
TEST_F(FileTest, VulkanLayout64BitTypesStd430) {
  setGlLayout();
  runFileTest("vk.layout.64bit-types.std430.hlsl");
}
TEST_F(FileTest, VulkanLayout64BitTypesStd140) {
  setGlLayout();
  runFileTest("vk.layout.64bit-types.std140.hlsl");
}
TEST_F(FileTest, VulkanLayout16BitTypesPushConstant) {
  runFileTest("vk.layout.16bit-types.pc.hlsl");
}
TEST_F(FileTest, VulkanLayout16BitTypesCBuffer) {
  runFileTest("vk.layout.16bit-types.cbuffer.hlsl");
}
TEST_F(FileTest, VulkanLayout16BitTypesTBuffer) {
  runFileTest("vk.layout.16bit-types.tbuffer.hlsl");
}
TEST_F(FileTest, VulkanLayout16BitTypesStructuredBuffer) {
  runFileTest("vk.layout.16bit-types.sbuffer.hlsl");
}
TEST_F(FileTest, VulkanLayoutVectorRelaxedLayout) {
  // Allows vectors to be aligned according to their element types, if not
  // causing improper straddle
  runFileTest("vk.layout.vector.relaxed.hlsl");
}
TEST_F(FileTest, VulkanLayoutStructRelaxedLayout) {
  // Checks VK_KHR_relaxed_block_layout on struct types
  runFileTest("vk.layout.struct.relaxed.hlsl");
}

TEST_F(FileTest, VulkanLayoutVkOffsetAttr) {
  // Checks the behavior of [[vk::offset]]
  setDxLayout();
  runFileTest("vk.layout.attr.offset.hlsl");
}

TEST_F(FileTest, VulkanLayoutPushConstantStd430) {
  setGlLayout();
  runFileTest("vk.layout.push-constant.std430.hlsl");
}

TEST_F(FileTest, VulkanLayoutCBufferPackOffset) {
  runFileTest("vk.layout.cbuffer.packoffset.hlsl");
}
TEST_F(FileTest, VulkanLayoutCBufferPackOffsetError) {
  runFileTest("vk.layout.cbuffer.packoffset.error.hlsl", Expect::Failure);
}

TEST_F(FileTest, VulkanLayoutFxcRulesSBuffer) {
  // structured buffers with fxc layout rules
  setDxLayout();
  runFileTest("vk.layout.sbuffer.fxc.hlsl");
}
TEST_F(FileTest, VulkanLayoutFxcRulesCBuffer) {
  // cbuffer/tbuffer/ConstantBuffer/TextureBuffer with fxc layout rules
  setDxLayout();
  runFileTest("vk.layout.cbuffer.fxc.hlsl");
}

TEST_F(FileTest, VulkanLayoutFxcRulesCBuffer1) {
  // cbuffer/tbuffer/ConstantBuffer/TextureBuffer with fxc layout rules
  setDxLayout();
  runFileTest("vk.layout.cbuffer.fxc.1.hlsl");
}

TEST_F(FileTest, VulkanLayoutCBufferScalar) {
  // VK_EXT_scalar_block_layout
  setScalarLayout();
  runFileTest("vk.layout.cbuffer.scalar.hlsl");
}

TEST_F(FileTest, VulkanLayoutRegisterCAll) {
  // :register(c#) used on all global variables.
  setDxLayout();
  runFileTest("vk.layout.register-c.all.hlsl");
}

TEST_F(FileTest, VulkanLayoutRegisterCMixed) {
  // :register(c#) used only on some global variables.
  setDxLayout();
  runFileTest("vk.layout.register-c.mixed.hlsl");
}

TEST_F(FileTest, VulkanLayoutRegisterCError) {
  // :register(c#) causing offset overlap for global variables.
  setDxLayout();
  runFileTest("vk.layout.register-c.error.hlsl", Expect::Failure);
}

TEST_F(FileTest, VulkanSubpassInput) { runFileTest("vk.subpass-input.hlsl"); }
TEST_F(FileTest, VulkanSubpassInputBinding) {
  runFileTest("vk.subpass-input.binding.hlsl");
}
TEST_F(FileTest, VulkanSubpassInputError) {
  runFileTest("vk.subpass-input.error.hlsl", Expect::Failure);
}

TEST_F(FileTest, NonFpColMajorError) {
  runFileTest("vk.layout.non-fp-matrix.error.hlsl", Expect::Failure);
}
TEST_F(FileTest, NonFpColMajorErrorArrayStruct) {
  runFileTest("vk.layout.non-fp-matrix.array.struct.error.hlsl", Expect::Failure);
}

TEST_F(FileTest, NamespaceFunctions) {
  runFileTest("namespace.functions.hlsl");
}
TEST_F(FileTest, NamespaceGlobals) { runFileTest("namespace.globals.hlsl"); }
TEST_F(FileTest, NamespaceResources) {
  runFileTest("namespace.resources.hlsl");
}

// HS: for different Patch Constant Functions
TEST_F(FileTest, HullShaderPCFVoid) { runFileTest("hs.pcf.void.hlsl"); }
TEST_F(FileTest, HullShaderPCFTakesInputPatch) {
  runFileTest("hs.pcf.input-patch.hlsl");
}
TEST_F(FileTest, HullShaderPCFTakesOutputPatch) {
  runFileTest("hs.pcf.output-patch.hlsl");
}
TEST_F(FileTest, HullShaderPCFTakesPrimitiveId) {
  runFileTest("hs.pcf.primitive-id.1.hlsl");
}
TEST_F(FileTest, HullShaderPCFTakesPrimitiveIdButMainDoesnt) {
  runFileTest("hs.pcf.primitive-id.2.hlsl");
}
TEST_F(FileTest, HullShaderPCFTakesViewId) {
  runFileTest("hs.pcf.view-id.1.hlsl");
}
TEST_F(FileTest, HullShaderPCFTakesViewIdButMainDoesnt) {
  runFileTest("hs.pcf.view-id.2.hlsl");
}
// HS: for the structure of hull shaders
TEST_F(FileTest, HullShaderStructure) { runFileTest("hs.structure.hlsl"); }

// GS: emit vertex and emit primitive
TEST_F(FileTest, GeometryShaderEmit) { runFileTest("gs.emit.hlsl"); }

// CS: groupshared
TEST_F(FileTest, ComputeShaderGroupShared) {
  runFileTest("cs.groupshared.hlsl");
}
TEST_F(FileTest, ComputeShaderGroupSharedNotInGlobals) {
  runFileTest("cs.groupshared.not-in-globals.hlsl");
}
TEST_F(FileTest, ComputeShaderGroupSharedFunctionParam) {
  setBeforeHLSLLegalization();
  runFileTest("cs.groupshared.function-param.hlsl");
}
TEST_F(FileTest, ComputeShaderGroupSharedFunctionParamOut) {
  setBeforeHLSLLegalization();
  runFileTest("cs.groupshared.function-param.out.hlsl");
}
TEST_F(FileTest, ComputeShaderGroupSharedStructFunction) {
  setBeforeHLSLLegalization();
  runFileTest("cs.groupshared.struct-function.hlsl");
}

// === Legalization examples ===

TEST_F(FileTest, LegalizationExample0) {
  runFileTest("legal-examples/00-copy-sbuf-ok.hlsl");
}
TEST_F(FileTest, LegalizationExample1) {
  runFileTest("legal-examples/01-copy-global-static-ok.hlsl");
}
TEST_F(FileTest, LegalizationExample2) {
  runFileTest("legal-examples/02-write-global-static-ok.hlsl");
}
TEST_F(FileTest, LegalizationExample3) {
  runFileTest("legal-examples/03-copy-local-struct-ok.hlsl");
}
TEST_F(FileTest, LegalizationExample4) {
  runFileTest("legal-examples/04-copy-local-nested-struct-ok.hlsl");
}
TEST_F(FileTest, LegalizationExample5) {
  runFileTest("legal-examples/05-func-param-sbuf-ok.hlsl");
}
TEST_F(FileTest, LegalizationExample6) {
  runFileTest("legal-examples/06-func-param-rwsbuf-ok.hlsl");
}
TEST_F(FileTest, LegalizationExample7) {
  runFileTest("legal-examples/07-func-ret-tmp-var-ok.hlsl");
}
TEST_F(FileTest, LegalizationExample8) {
  runFileTest("legal-examples/08-func-ret-direct-ok.hlsl");
}
TEST_F(FileTest, LegalizationExample9) {
  runFileTest("legal-examples/09-if-stmt-select-fail.hlsl", Expect::ValFailure);
}
TEST_F(FileTest, LegalizationExample10) {
  runFileTest("legal-examples/10-if-stmt-select-ok.hlsl");
}
TEST_F(FileTest, LegalizationExample11) {
  runFileTest("legal-examples/11-if-stmt-const-ok.hlsl");
}
TEST_F(FileTest, LegalizationExample12) {
  runFileTest("legal-examples/12-switch-stmt-select-fail.hlsl",
              Expect::ValFailure);
}
TEST_F(FileTest, LegalizationExample13) {
  runFileTest("legal-examples/13-switch-stmt-const-ok.hlsl");
}
TEST_F(FileTest, LegalizationExample14) {
  runFileTest("legal-examples/14-loop-var-fail.hlsl", Expect::ValFailure);
}
TEST_F(FileTest, LegalizationExample15) {
  runFileTest("legal-examples/15-loop-var-unroll-ok.hlsl");
}
TEST_F(FileTest, LegalizationExample16) {
  runFileTest("legal-examples/16-loop-var-range-fail.hlsl", Expect::ValFailure);
}
TEST_F(FileTest, LegalizationExample17) {
  runFileTest("legal-examples/17-loop-var-float-fail.hlsl", Expect::ValFailure);
}
TEST_F(FileTest, LegalizationExample18) {
  runFileTest("legal-examples/18-multi-func-call-ok.hlsl");
}
TEST_F(FileTest, LegalizationExample19) {
  runFileTest("legal-examples/19-multi-func-ret-fail.hlsl", Expect::ValFailure);
}
TEST_F(FileTest, LegalizationExample20) {
  runFileTest("legal-examples/20-multi-func-ret-const-ok.hlsl");
}
TEST_F(FileTest, LegalizationExample21) {
  runFileTest("legal-examples/21-combined-ok.hlsl");
}

TEST_F(FileTest, PreprocessorError) {
  // Tests that preprocessor error is surfaced
  runFileTest("preprocess.error.hlsl", Expect::Failure);
}

// === Raytracing NV examples ===
TEST_F(FileTest, RayTracingNVRaygen) {
  runFileTest("raytracing.nv.raygen.hlsl");
}
TEST_F(FileTest, RayTracingNVEnum) { runFileTest("raytracing.nv.enum.hlsl"); }
TEST_F(FileTest, RayTracingNVIntersection) {
  runFileTest("raytracing.nv.intersection.hlsl");
}
TEST_F(FileTest, RayTracingNVAnyHit) {
  runFileTest("raytracing.nv.anyhit.hlsl");
}
TEST_F(FileTest, RayTracingNVClosestHit) {
  runFileTest("raytracing.nv.closesthit.hlsl");
}
TEST_F(FileTest, RayTracingNVMiss) { runFileTest("raytracing.nv.miss.hlsl"); }
TEST_F(FileTest, RayTracingNVCallable) {
  runFileTest("raytracing.nv.callable.hlsl");
}
TEST_F(FileTest, RayTracingNVLibrary) {
  runFileTest("raytracing.nv.library.hlsl");
}

// For decoration uniqueness
TEST_F(FileTest, DecorationUnique) { runFileTest("decoration.unique.hlsl"); }

// For capability uniqueness
TEST_F(FileTest, CapabilityUnique) { runFileTest("capability.unique.hlsl"); }

// For extension uniqueness
TEST_F(FileTest, ExtensionUnique) { runFileTest("extension.unique.hlsl"); }

// For RelaxedPrecision decorations
TEST_F(FileTest, DecorationRelaxedPrecisionBasic) {
  runFileTest("decoration.relaxed-precision.basic.hlsl");
}
TEST_F(FileTest, DecorationRelaxedPrecisionStruct) {
  runFileTest("decoration.relaxed-precision.struct.hlsl");
}
TEST_F(FileTest, DecorationRelaxedPrecisionImage) {
  runFileTest("decoration.relaxed-precision.image.hlsl");
}

// For NoContraction decorations
TEST_F(FileTest, DecorationNoContraction) {
  setBeforeHLSLLegalization();
  runFileTest("decoration.no-contraction.hlsl");
}
TEST_F(FileTest, DecorationNoContractionVariableReuse) {
  runFileTest("decoration.no-contraction.variable-reuse.hlsl");
}
TEST_F(FileTest, DecorationNoContractionStruct) {
  runFileTest("decoration.no-contraction.struct.hlsl");
}
TEST_F(FileTest, DecorationNoContractionStageVars) {
  runFileTest("decoration.no-contraction.stage-vars.hlsl");
}

// For UserTypeGOOGLE decorations
TEST_F(FileTest, DecorationUserTypeGOOGLE) {
  runFileTest("decoration.user-type.hlsl");
}

// For pragmas
TEST_F(FileTest, PragmaPackMatrix) { runFileTest("pragma.pack_matrix.hlsl"); }

// Tests for [[vk::shader_record_nv]]
TEST_F(FileTest, VulkanShaderRecordBufferNV) {
  runFileTest("vk.shader-record-nv.hlsl");
}
TEST_F(FileTest, VulkanLayoutShaderRecordBufferNVStd430) {
  setGlLayout();
  runFileTest("vk.layout.shader-record-nv.std430.hlsl");
}
TEST_F(FileTest, VulkanShaderRecordBufferNVOffset) {
  // Checks the behavior of [[vk::offset]] with [[vk::shader_record_nv]]
  runFileTest("vk.shader-record-nv.offset.hlsl");
}
TEST_F(FileTest, VulkanShadingRate) { runFileTest("vk.shading-rate.hlsl"); }
TEST_F(FileTest, VulkanShadingRateError) {
  runFileTest("vk.shading-rate.vs-error.hlsl", Expect::Failure);
}

// === MeshShading NV examples ===
TEST_F(FileTest, MeshShadingNVMeshTriangle) {
  runFileTest("meshshading.nv.triangle.mesh.hlsl");
}
TEST_F(FileTest, MeshShadingNVMeshLine) {
  runFileTest("meshshading.nv.line.mesh.hlsl");
}
TEST_F(FileTest, MeshShadingNVMeshPoint) {
  runFileTest("meshshading.nv.point.mesh.hlsl");
}
TEST_F(FileTest, MeshShadingNVMeshBuffer) {
  runFileTest("meshshading.nv.buffer.mesh.hlsl");
}
TEST_F(FileTest, MeshShadingNVMeshError1) {
  runFileTest("meshshading.nv.error1.mesh.hlsl", Expect::Failure);
}
TEST_F(FileTest, MeshShadingNVMeshError2) {
  runFileTest("meshshading.nv.error2.mesh.hlsl", Expect::Failure);
}
TEST_F(FileTest, MeshShadingNVMeshError3) {
  runFileTest("meshshading.nv.error3.mesh.hlsl", Expect::Failure);
}
TEST_F(FileTest, MeshShadingNVMeshError4) {
  runFileTest("meshshading.nv.error4.mesh.hlsl", Expect::Failure);
}
TEST_F(FileTest, MeshShadingNVMeshError5) {
  runFileTest("meshshading.nv.error5.mesh.hlsl", Expect::Failure);
}
TEST_F(FileTest, MeshShadingNVMeshError6) {
  runFileTest("meshshading.nv.error6.mesh.hlsl", Expect::Failure);
}
TEST_F(FileTest, MeshShadingNVMeshError7) {
  runFileTest("meshshading.nv.error7.mesh.hlsl", Expect::Failure);
}
TEST_F(FileTest, MeshShadingNVMeshError8) {
  runFileTest("meshshading.nv.error8.mesh.hlsl", Expect::Failure);
}
TEST_F(FileTest, MeshShadingNVMeshError9) {
  runFileTest("meshshading.nv.error9.mesh.hlsl", Expect::Failure);
}
TEST_F(FileTest, MeshShadingNVMeshError10) {
  runFileTest("meshshading.nv.error10.mesh.hlsl", Expect::Failure);
}
TEST_F(FileTest, MeshShadingNVMeshError11) {
  runFileTest("meshshading.nv.error11.mesh.hlsl", Expect::Failure);
}
TEST_F(FileTest, MeshShadingNVMeshError12) {
  runFileTest("meshshading.nv.error12.mesh.hlsl", Expect::Failure);
}
TEST_F(FileTest, MeshShadingNVMeshError13) {
  runFileTest("meshshading.nv.error13.mesh.hlsl", Expect::Failure);
}
TEST_F(FileTest, MeshShadingNVMeshError14) {
  runFileTest("meshshading.nv.error14.mesh.hlsl", Expect::Failure);
}
TEST_F(FileTest, MeshShadingNVAmplification) {
  runFileTest("meshshading.nv.amplification.hlsl");
}
TEST_F(FileTest, MeshShadingNVAmplificationFunCall) {
  useVulkan1p1();
  runFileTest("meshshading.nv.fncall.amplification.hlsl");
}
TEST_F(FileTest, MeshShadingNVAmplificationError1) {
  runFileTest("meshshading.nv.error1.amplification.hlsl", Expect::Failure);
}
TEST_F(FileTest, MeshShadingNVAmplificationError2) {
  runFileTest("meshshading.nv.error2.amplification.hlsl", Expect::Failure);
}
TEST_F(FileTest, MeshShadingNVAmplificationError3) {
  runFileTest("meshshading.nv.error4.amplification.hlsl", Expect::Failure);
}
TEST_F(FileTest, MeshShadingNVAmplificationError4) {
  runFileTest("meshshading.nv.error3.amplification.hlsl", Expect::Failure);
}

} // namespace
