//===- unittests/SPIRV/CodeGenSPIRVTest.cpp ---- Run CodeGenSPIRV tests ---===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "FileTestFixture.h"

namespace {
using clang::spirv::FileTest;

// === Partial output tests ===

// For types
TEST_F(FileTest, ScalarTypes) { runFileTest("type.scalar.hlsl"); }
TEST_F(FileTest, VectorTypes) { runFileTest("type.vector.hlsl"); }
TEST_F(FileTest, StructTypes) { runFileTest("type.struct.hlsl"); }
TEST_F(FileTest, StructTypeEmptyStructArrayStride) {
  runFileTest("type.struct.empty-struct.array-stride.hlsl");
}
TEST_F(FileTest, StructTypeUniqueness) {
  runFileTest("type.struct.uniqueness.hlsl");
}
TEST_F(FileTest, StringTypes) { runFileTest("type.string.hlsl"); }
TEST_F(FileTest, ClassTypes) { runFileTest("type.class.hlsl"); }
TEST_F(FileTest, ArrayTypes) { runFileTest("type.array.hlsl"); }
TEST_F(FileTest, RuntimeArrayTypes) { runFileTest("type.runtime-array.hlsl"); }
TEST_F(FileTest, TypedefTypes) { runFileTest("type.typedef.hlsl"); }
TEST_F(FileTest, SamplerTypes) { runFileTest("type.sampler.hlsl"); }
TEST_F(FileTest, TextureTypes) { runFileTest("type.texture.hlsl"); }
TEST_F(FileTest, RWTextureTypes) { runFileTest("type.rwtexture.hlsl"); }
TEST_F(FileTest, RWTextureTypesWithMinPrecisionScalarTypes) {
  runFileTest("type.rwtexture.with.min.precision.scalar.hlsl");
}
TEST_F(FileTest, RWTextureTypesWith64bitsScalarTypes) {
  runFileTest("type.rwtexture.with.64bit.scalar.hlsl");
}
TEST_F(FileTest, BufferType) { runFileTest("type.buffer.hlsl"); }

TEST_F(FileTest, RWBufferTypeHalfElementType) {
  runFileTest("type.rwbuffer.half.hlsl");
}
TEST_F(FileTest, CBufferType) { runFileTest("type.cbuffer.hlsl"); }
TEST_F(FileTest, TypeCBufferIncludingResource) {
  runFileTest("type.cbuffer.including.resource.hlsl");
}
TEST_F(FileTest, ConstantBufferType) {
  runFileTest("type.constant-buffer.hlsl");
}
TEST_F(FileTest, ConstantBufferTypeAssign) {
  runFileTest("type.constant-buffer.assign.hlsl");
}
TEST_F(FileTest, ConstantBufferTypeReturn) {
  runFileTest("type.constant-buffer.return.hlsl");
}
TEST_F(FileTest, ConstantBufferTypeMultiDimensionalArray) {
  runFileTest("type.constant-buffer.multiple-dimensions.hlsl");
}
TEST_F(FileTest, BindlessConstantBufferArrayType) {
  runFileTest("type.constant-buffer.bindless.array.hlsl");
}
TEST_F(FileTest, EnumType) { runFileTest("type.enum.hlsl"); }
TEST_F(FileTest, ClassEnumType) { runFileTest("class.enum.hlsl"); }
TEST_F(FileTest, TBufferType) { runFileTest("type.tbuffer.hlsl"); }
TEST_F(FileTest, TextureBufferType) { runFileTest("type.texture-buffer.hlsl"); }
TEST_F(FileTest, StructuredBufferType) {
  runFileTest("type.structured-buffer.hlsl");
}
TEST_F(FileTest, StructuredBufferTypeWithVector) {
  runFileTest("type.structured-buffer.vector.hlsl");
}
TEST_F(FileTest, RWStructuredBufferArrayNoCounter) {
  runFileTest("type.rwstructured-buffer.array.nocounter.hlsl");
}
TEST_F(FileTest, RWStructuredBufferArrayNoCounterFlattened) {
  runFileTest("type.rwstructured-buffer.array.nocounter.flatten.hlsl");
}
TEST_F(FileTest, RWStructuredBufferArrayCounter) {
  runFileTest("type.rwstructured-buffer.array.counter.hlsl");
}
TEST_F(FileTest, RWStructuredBufferUnboundedArrayCounter) {
  runFileTest("type.rwstructured-buffer.unbounded.array.counter.hlsl");
}
TEST_F(FileTest, RWStructuredBufferArrayCounterConstIndex) {
  runFileTest("type.rwstructured-buffer.array.counter.const.index.hlsl");
}
TEST_F(FileTest, RWStructuredBufferArrayCounterFlattened) {
  runFileTest("type.rwstructured-buffer.array.counter.flatten.hlsl");
}
TEST_F(FileTest, RWStructuredBufferArrayCounterIndirect) {
  runFileTest("type.rwstructured-buffer.array.counter.indirect.hlsl");
}
TEST_F(FileTest, RWStructuredBufferArrayCounterIndirect2) {
  runFileTest("type.rwstructured-buffer.array.counter.indirect2.hlsl");
}
TEST_F(FileTest, RWStructuredBufferArrayBindAttributes) {
  runFileTest("type.rwstructured-buffer.array.binding.attributes.hlsl");
}
TEST_F(FileTest, RWStructuredBufferUnboundedArray) {
  runFileTest("type.rwstructured-buffer.array.unbounded.counter.hlsl");
}
TEST_F(FileTest, AppendStructuredBufferArrayError) {
  runFileTest("type.append-structured-buffer.array.hlsl");
}
TEST_F(FileTest, ConsumeStructuredBufferArrayError) {
  runFileTest("type.consume-structured-buffer.array.hlsl");
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

TEST_F(FileTest, TemplateFunctionInstance) {
  runFileTest("type.template.function.template-instance.hlsl");
}
TEST_F(FileTest, TemplateStructInstance) {
  runFileTest("type.template.struct.template-instance.hlsl");
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
TEST_F(FileTest, VarInitArray) { runFileTest("var.init.array.hlsl"); }

TEST_F(FileTest, VarInitCrossStorageClass) {
  runFileTest("var.init.cross-storage-class.hlsl");
}
TEST_F(FileTest, VarInitVec1) { runFileTest("var.init.vec.size.1.hlsl"); }
TEST_F(FileTest, StaticVar) { runFileTest("var.static.hlsl"); }
TEST_F(FileTest, TemplateStaticVar) { runFileTest("template.static.var.hlsl"); }
TEST_F(FileTest, UninitStaticResourceVar) {
  runFileTest("var.static.resource.hlsl");
}
TEST_F(FileTest, ResourceArrayVar) { runFileTest("var.resource.array.hlsl"); }
TEST_F(FileTest, GlobalsCBuffer) { runFileTest("var.globals.hlsl"); }

TEST_F(FileTest, OperatorOverloadingCall) {
  runFileTest("operator.overloading.call.hlsl");
}
TEST_F(FileTest, OperatorOverloadingStar) {
  runFileTest("operator.overloading.star.hlsl");
}
TEST_F(FileTest, OperatorOverloadingMatrixMultiplication) {
  runFileTest("operator.overloading.mat.mul.hlsl");
}
TEST_F(FileTest, OperatorOverloadingCorrectnessOfResourceTypeCheck) {
  runFileTest("operator.overloading.resource.type.check.hlsl");
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

// For sizeof()
TEST_F(FileTest, UnaryOpSizeof) { runFileTest("unary-op.sizeof.hlsl"); }

// For cast of size 1 float vectors
TEST_F(FileTest, CastSize1Vectors) { runFileTest("cast.float1.half1.hlsl"); }

// For ternary operators
TEST_F(FileTest, TernaryOpConditionalOp) {
  runFileTest("ternary-op.cond-op.hlsl");
}

// For short-circuited ternary operators (HLSL 2021)
TEST_F(FileTest, TernaryOpShortCircuitedConditionalOp) {
  runFileTest("ternary-op.short-circuited-cond-op.hlsl");
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
TEST_F(FileTest, OpStructAccessBitfield) {
  runFileTest("op.struct.access.bitfield.hlsl");
}
TEST_F(FileTest, OpArrayAccess) { runFileTest("op.array.access.hlsl"); }

// For buffer accessing operator
TEST_F(FileTest, OpBufferAccess) { runFileTest("op.buffer.access.hlsl"); }
TEST_F(FileTest, OpBufferAccessBitfield) {
  runFileTest("op.buffer.access.bitfield.hlsl");
}
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
TEST_F(FileTest, OpStructuredBufferAccessBitfield) {
  runFileTest("op.structured-buffer.access.bitfield.hlsl");
}
TEST_F(FileTest, OpStructuredBufferReconstructBitfield) {
  runFileTest("op.structured-buffer.reconstruct.bitfield.hlsl");
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
TEST_F(FileTest, OpSizeOfSameForInitAndReturn) {
  runFileTest("op.sizeof.same.for.init.and.return.hlsl");
}

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
TEST_F(FileTest, CastFlatConversionDeclRef) {
  runFileTest("cast.flat-conversion.decl-ref.hlsl");
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
TEST_F(FileTest, CastImplicitVecToMat) {
  runFileTest("cast.vec-to-mat.implicit.hlsl");
}
TEST_F(FileTest, CastMatrixToVector) { runFileTest("cast.mat-to-vec.hlsl"); }
TEST_F(FileTest, CastStructToInt) { runFileTest("cast.struct-to-int.hlsl"); }
TEST_F(FileTest, CastBitwidth) { runFileTest("cast.bitwidth.hlsl"); }

TEST_F(FileTest, CastLiteralTypeForArraySubscript) {
  runFileTest("cast.literal-type.array-subscript.hlsl");
}

TEST_F(FileTest, CastLiteralTypeForTernary) {
  runFileTest("cast.literal-type.ternary.hlsl");
}

TEST_F(FileTest, SelectLongLit) { runFileTest("select.long.lit.hlsl"); }
TEST_F(FileTest, SelectShortLit) { runFileTest("select.short.lit.hlsl"); }
TEST_F(FileTest, SelectLongLit2021) {
  runFileTest("select.long.lit.hlsl2021.hlsl");
}
TEST_F(FileTest, SelectShortLit2021) {
  runFileTest("select.short.lit.hlsl2021.hlsl");
}

TEST_F(FileTest, CastLiteralTypeForTernary2021) {
  runFileTest("cast.literal-type.ternary.2021.hlsl");
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
TEST_F(FileTest, ForStmtShortCircuitedCond) {
  runFileTest("cf.for.short-circuited-cond.hlsl");
}

// For while statements
TEST_F(FileTest, WhileStmtPlain) { runFileTest("cf.while.plain.hlsl"); }
TEST_F(FileTest, WhileStmtNested) { runFileTest("cf.while.nested.hlsl"); }
TEST_F(FileTest, WhileStmtContinue) { runFileTest("cf.while.continue.hlsl"); }
TEST_F(FileTest, WhileStmtBreak) { runFileTest("cf.while.break.hlsl"); }
TEST_F(FileTest, WhileStmtShortCircuitedCond) {
  runFileTest("cf.while.short-circuited-cond.hlsl");
}

// For do statements
TEST_F(FileTest, DoStmtPlain) { runFileTest("cf.do.plain.hlsl"); }
TEST_F(FileTest, DoStmtNested) { runFileTest("cf.do.nested.hlsl"); }
TEST_F(FileTest, DoStmtContinue) { runFileTest("cf.do.continue.hlsl"); }
TEST_F(FileTest, DoStmtBreak) { runFileTest("cf.do.break.hlsl"); }

// For break statements (mix of breaks in loops and switch)
TEST_F(FileTest, BreakStmtMixed) { runFileTest("cf.break.mixed.hlsl"); }

// For discard statement
TEST_F(FileTest, Discard) { runFileTest("cf.discard.hlsl"); }
TEST_F(FileTest, DiscardToDemote) { runFileTest("cf.discard.to-demote.hlsl"); }

// For return statement
TEST_F(FileTest, EarlyReturn) { runFileTest("cf.return.early.hlsl"); }
TEST_F(FileTest, EarlyReturnFloat4) {
  runFileTest("cf.return.early.float4.hlsl");
}
TEST_F(FileTest, ReturnStruct) { runFileTest("cf.return.struct.hlsl"); }
TEST_F(FileTest, ReturnFromDifferentStorageClass) {
  runFileTest("cf.return.storage-class.hlsl");
}
TEST_F(FileTest, ReturnFromDifferentMemoryLayout) {
  runFileTest("cf.return.memory-layout.hlsl");
}
TEST_F(FileTest, VoidReturn) { runFileTest("cf.return.void.hlsl"); }

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
TEST_F(FileTest, FunctionInOutParamNoNeedToCopy) {
  // Tests that referencing function scope variables as a whole with out/inout
  // annotation does not create temporary variables
  runFileTest("fn.param.inout.no-copy.hlsl");
}
TEST_F(FileTest, FunctionParamUnsizedOpaqueArrayO3) {
  runFileTest("fn.param.unsized-opaque-array-o3.hlsl");
}
TEST_F(FileTest, FunctionInOutParamTypeMismatch) {
  // The type for the inout parameter doesn't match the argument type.
  runFileTest("fn.param.inout.type-mismatch.hlsl");
}
TEST_F(FileTest, FunctionFowardDeclaration) {
  runFileTest("fn.foward-declaration.hlsl");
}

TEST_F(FileTest, FunctionNoInline) { runFileTest("fn.noinline.hlsl"); }
TEST_F(FileTest, FunctionDefaultParam) { runFileTest("fn.param.default.hlsl"); }
TEST_F(FileTest, FunctionExport) { runFileTest("fn.export.hlsl"); }

TEST_F(FileTest, FixFunctionCall) {
  runFileTest("fn.fixfuncall-compute.hlsl");
  runFileTest("fn.fixfuncall-linkage.hlsl");
}

TEST_F(FileTest, FunctionForwardDecl) {
  runFileTest("fn.forward-declaration.hlsl");
}

// For OO features
TEST_F(FileTest, StructDerivedMethodsOverride) {
  runFileTest("oo.struct.derived.methods.override.hlsl");
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
TEST_F(FileTest, InheritanceLayoutDifferences) {
  runFileTest("oo.inheritance.layout-differences.hlsl");
}
TEST_F(FileTest, InheritanceLayoutEmptyStruct) {
  runFileTest("oo.inheritance.layout.empty-struct.hlsl");
}
TEST_F(FileTest, InheritanceCallMethodOfBase) {
  setBeforeHLSLLegalization();
  runFileTest("oo.inheritance.call.base.method.hlsl", Expect::Success);
}
TEST_F(FileTest, InheritanceBaseWithByteAddressBuffer) {
  runFileTest("oo.inheritance.base-with-byte-address-buffer.hlsl");
}
TEST_F(FileTest, InheritanceCallMethodWithSameBaseMethodName) {
  runFileTest("oo.call.method.with.same.base.method.name.hlsl");
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
TEST_F(FileTest, SemanticNonzeroBaseInstanceVS) {
  runFileTest("semantic.nonzero-base-instance.vs.hlsl");
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
TEST_F(FileTest, SemanticTessFactorSizeMismatchHS) {
  runFileTest("semantic.tess-factor.size-mismatch.hs.hlsl");
}
TEST_F(FileTest, SemanticInsideTessFactorHSArray1) {
  // Test that SV_InsideTessFactor is of type float[1]
  runFileTest("semantic.inside-tess-factor.hs.array1.hlsl");
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
TEST_F(FileTest, SemanticRenderTargetArrayIndexCoreVS) {
  runFileTest("semantic.render-target-array-index-core.vs.hlsl");
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
TEST_F(FileTest, SemanticViewportArrayIndexCoreVS) {
  runFileTest("semantic.viewport-array-index-core.vs.hlsl");
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
TEST_F(FileTest, TextureSampleOffsetWithLoopUnroll) {
  runFileTest("texture.sample-offset.with.loop-unroll.hlsl");
}
TEST_F(FileTest, TextureSampleConstOffsetAfterLegalization) {
  runFileTest("texture.sample.offset.needs.legalization.o0.hlsl");
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
TEST_F(FileTest, ByteAddressBufferLoadLayout) {
  runFileTest("method.byte-address-buffer.load.layout.hlsl");
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
  runFileTest("method.byte-address-buffer.templated-store.struct2.hlsl");
}
TEST_F(FileTest, ByteAddressBufferTemplatedStoreMatrix) {
  runFileTest("method.byte-address-buffer.templated-store.matrix.hlsl");
}
TEST_F(FileTest, ByteAddressBufferGetDimensions) {
  runFileTest("method.byte-address-buffer.get-dimensions.hlsl");
}
TEST_F(FileTest, RWByteAddressBufferAtomicMethods) {
  runFileTest("method.rw-byte-address-buffer.atomic.hlsl");
}

// For `-fspv-use-legacy-matrix-buffer-order`
TEST_F(FileTest, SpvUseLegacyMatrixBufferOrder) {
  runFileTest("spv.use-legacy-buffer-matrix-order.hlsl");
}

TEST_F(FileTest, SpvPreserveInterface) {
  runFileTest("spv.preserve-interface.hlsl");
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
TEST_F(FileTest, IntrinsicsAsfloat16) {
  runFileTest("intrinsics.asfloat16.hlsl");
}
TEST_F(FileTest, IntrinsicsAsint) { runFileTest("intrinsics.asint.hlsl"); }
TEST_F(FileTest, IntrinsicsAsint16) { runFileTest("intrinsics.asint16.hlsl"); }
TEST_F(FileTest, IntrinsicsAsuint) { runFileTest("intrinsics.asuint.hlsl"); }
TEST_F(FileTest, IntrinsicsAsuint16) {
  runFileTest("intrinsics.asuint16.hlsl");
}
TEST_F(FileTest, IntrinsicsAsuintArgumentMustBeRValue) {
  runFileTest("intrinsics.asuint.rvalue.hlsl");
}
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
TEST_F(FileTest, IntrinsicsPrintf) { runFileTest("intrinsics.printf.hlsl"); }
TEST_F(FileTest, IntrinsicsFloor) { runFileTest("intrinsics.floor.hlsl"); }
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
TEST_F(FileTest, IntrinsicsAnd) { runFileTest("intrinsics.and.hlsl"); }
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
TEST_F(FileTest, Intrinsics64BitInterlockedMethodsPS) {
  runFileTest("intrinsics.64bit-interlocked-methods.ps.hlsl");
}
TEST_F(FileTest, IntrinsicsInterlockedMethodsCS) {
  runFileTest("intrinsics.interlocked-methods.cs.hlsl");
}
TEST_F(FileTest, IntrinsicsInterlockedMethodsTextureSwizzling) {
  runFileTest("intrinsics.interlocked-methods.texture.swizzling.hlsl");
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
TEST_F(FileTest, IntrinsicsMinFiniteMathOnly) {
  runFileTest("intrinsics.min.finitemathonly.hlsl");
}
TEST_F(FileTest, IntrinsicsLit) { runFileTest("intrinsics.lit.hlsl"); }
TEST_F(FileTest, IntrinsicsModf) { runFileTest("intrinsics.modf.hlsl"); }
TEST_F(FileTest, IntrinsicsModfWithSwizzling) {
  runFileTest("intrinsics.modf.swizzle.hlsl");
}
TEST_F(FileTest, IntrinsicsMad) { runFileTest("intrinsics.mad.hlsl"); }
TEST_F(FileTest, IntrinsicsUMad) { runFileTest("intrinsics.umad.hlsl"); }
TEST_F(FileTest, IntrinsicsMax) { runFileTest("intrinsics.max.hlsl"); }
TEST_F(FileTest, IntrinsicsMaxFiniteMathOnly) {
  runFileTest("intrinsics.max.finitemathonly.hlsl");
}
TEST_F(FileTest, IntrinsicsMsad4) { runFileTest("intrinsics.msad4.hlsl"); }
TEST_F(FileTest, IntrinsicsNormalize) {
  runFileTest("intrinsics.normalize.hlsl");
}
TEST_F(FileTest, IntrinsicsOr) { runFileTest("intrinsics.or.hlsl"); }
TEST_F(FileTest, IntrinsicsPow) { runFileTest("intrinsics.pow.hlsl"); }
TEST_F(FileTest, IntrinsicsRsqrt) { runFileTest("intrinsics.rsqrt.hlsl"); }
TEST_F(FileTest, IntrinsicsFloatSign) {
  runFileTest("intrinsics.floatsign.hlsl");
}
TEST_F(FileTest, IntrinsicsIntSign) { runFileTest("intrinsics.intsign.hlsl"); }
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
TEST_F(FileTest, IntrinsicsSelect) { runFileTest("intrinsics.select.hlsl"); }
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
TEST_F(FileTest, IntrinsicsCheckAccessFullyMapped) {
  runFileTest("intrinsics.check-access-fully-mapped.hlsl");
}
TEST_F(FileTest, IntrinsicsCheckAccessFullyMappedWithoutSampler) {
  runFileTest("intrinsics.check-access-fully-mapped.without-sampler.hlsl");
}
TEST_F(FileTest, IntrinsicsNonUniformResourceIndex) {
  runFileTest("intrinsics.non-uniform-resource-index.hlsl");
}

// Vulkan-specific intrinsic functions
TEST_F(FileTest, IntrinsicsVkCrossDeviceScope) {
  runFileTest("intrinsics.vkcrossdevicescope.hlsl");
}
TEST_F(FileTest, IntrinsicsVkDeviceScope) {
  runFileTest("intrinsics.vkdevicescope.hlsl");
}
TEST_F(FileTest, IntrinsicsVkWorkgroupScope) {
  runFileTest("intrinsics.vkworkgroupscope.hlsl");
}
TEST_F(FileTest, IntrinsicsVkSubgroupScope) {
  runFileTest("intrinsics.vksubgroupscope.hlsl");
}
TEST_F(FileTest, IntrinsicsVkInvocationScope) {
  runFileTest("intrinsics.vkinvocationscope.hlsl");
}
TEST_F(FileTest, IntrinsicsVkQueueFamilyScope) {
  runFileTest("intrinsics.vkqueuefamilyscope.hlsl");
}
TEST_F(FileTest, IntrinsicsSpirv) {
  runFileTest("spv.intrinsicInstruction.hlsl");
  runFileTest("spv.intrinsic.result_id.hlsl");
  runFileTest("spv.intrinsicLiteral.hlsl");
  runFileTest("spv.intrinsicTypeInteger.hlsl");
}
TEST_F(FileTest, IntrinsicsVkReadClock) {
  runFileTest("intrinsics.vkreadclock.hlsl");
}
TEST_F(FileTest, IntrinsicsVkRawBufferLoad) {
  runFileTest("intrinsics.vkrawbufferload.hlsl");
}
TEST_F(FileTest, IntrinsicsVkRawBufferLoadBitfield) {
  runFileTest("intrinsics.vkrawbufferload.bitfield.hlsl");
}
TEST_F(FileTest, IntrinsicsVkRawBufferStore) {
  runFileTest("intrinsics.vkrawbufferstore.hlsl");
}
TEST_F(FileTest, IntrinsicsVkRawBufferStoreBitfields) {
  runFileTest("intrinsics.vkrawbufferstore.bitfields.hlsl");
}
// Intrinsics added in SM 6.6
TEST_F(FileTest, IntrinsicsSM66PackU8S8) {
  runFileTest("intrinsics.sm6_6.pack_s8u8.hlsl");
}
TEST_F(FileTest, IntrinsicsSM66PackClampU8S8) {
  runFileTest("intrinsics.sm6_6.pack_clamp_s8u8.hlsl");
}
TEST_F(FileTest, IntrinsicsSM66Unpack) {
  runFileTest("intrinsics.sm6_6.unpack.hlsl");
}
TEST_F(FileTest, IntrinsicsSM66IsHelperLane) {
  runFileTest("intrinsics.sm6_6.ishelperlane.hlsl");
}
TEST_F(FileTest, IntrinsicsSM66IsHelperLaneVk1p3) {
  runFileTest("intrinsics.sm6_6.ishelperlane.vk1p3.hlsl");
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
// Shader model 6.0 wave query
TEST_F(FileTest, SM6WaveIsFirstLane) {
  runFileTest("sm6.wave-is-first-lane.hlsl");
}
TEST_F(FileTest, SM6WaveGetLaneCount) {
  runFileTest("sm6.wave-get-lane-count.hlsl");
}
TEST_F(FileTest, SM6WaveGetLaneIndex) {
  runFileTest("sm6.wave-get-lane-index.hlsl");
}
TEST_F(FileTest, SM6WaveGetLaneCountPS) {
  runFileTest("sm6.wave-get-lane-count.ps.hlsl");
}
TEST_F(FileTest, SM6WaveGetLaneIndexPS) {
  runFileTest("sm6.wave-get-lane-index.ps.hlsl");
}
TEST_F(FileTest, SM6WaveBuiltInNoDuplicate) {
  runFileTest("sm6.wave.builtin.no-dup.hlsl");
}

// Shader model 6.0 wave vote
TEST_F(FileTest, SM6WaveActiveAnyTrue) {
  runFileTest("sm6.wave-active-any-true.hlsl");
}
TEST_F(FileTest, SM6WaveActiveAllTrue) {
  runFileTest("sm6.wave-active-all-true.hlsl");
}
TEST_F(FileTest, SM6WaveActiveBallot) {
  runFileTest("sm6.wave-active-ballot.hlsl");
}

TEST_F(FileTest, SM6WaveActiveAllEqualScalar) {
  runFileTest("sm6.wave-active-all-equal-scalar.hlsl");
}
TEST_F(FileTest, SM6WaveActiveAllEqualVector) {
  runFileTest("sm6.wave-active-all-equal-vector.hlsl");
}
TEST_F(FileTest, SM6WaveActiveAllEqualMatrix) {
  runFileTest("sm6.wave-active-all-equal-matrix.hlsl");
}
TEST_F(FileTest, SM6WaveActiveAllEqualMatrix1x1) {
  runFileTest("sm6.wave-active-all-equal-matrix1x1.hlsl");
}
TEST_F(FileTest, SM6WaveActiveSum) { runFileTest("sm6.wave-active-sum.hlsl"); }
TEST_F(FileTest, SM6WaveActiveProduct) {
  runFileTest("sm6.wave-active-product.hlsl");
}
TEST_F(FileTest, SM6WaveActiveMax) { runFileTest("sm6.wave-active-max.hlsl"); }
TEST_F(FileTest, SM6WaveActiveMin) { runFileTest("sm6.wave-active-min.hlsl"); }
TEST_F(FileTest, SM6WaveActiveBitAnd) {
  runFileTest("sm6.wave-active-bit-and.hlsl");
}
TEST_F(FileTest, SM6WaveActiveBitOr) {
  runFileTest("sm6.wave-active-bit-or.hlsl");
}
TEST_F(FileTest, SM6WaveActiveBitXor) {
  runFileTest("sm6.wave-active-bit-xor.hlsl");
}
TEST_F(FileTest, SM6WaveActiveCountBits) {
  runFileTest("sm6.wave-active-count-bits.hlsl");
}

// Shader model 6.0 wave scan/prefix
TEST_F(FileTest, SM6WavePrefixSum) { runFileTest("sm6.wave-prefix-sum.hlsl"); }
TEST_F(FileTest, SM6WavePrefixProduct) {
  runFileTest("sm6.wave-prefix-product.hlsl");
}
TEST_F(FileTest, SM6WavePrefixCountBits) {
  runFileTest("sm6.wave-prefix-count-bits.hlsl");
}

// Shader model 6.0 wave broadcast
TEST_F(FileTest, SM6WaveReadLaneAt) {
  runFileTest("sm6.wave-read-lane-at.hlsl");
}
TEST_F(FileTest, SM6WaveReadLaneFirst) {
  runFileTest("sm6.wave-read-lane-first.hlsl");
}

// Shader model 6.0 wave quad-wide shuffle
TEST_F(FileTest, SM6QuadReadAcrossX) {
  runFileTest("sm6.quad-read-across-x.hlsl");
}
TEST_F(FileTest, SM6QuadReadAcrossY) {
  runFileTest("sm6.quad-read-across-y.hlsl");
}
TEST_F(FileTest, SM6QuadReadAcrossDiagonal) {
  runFileTest("sm6.quad-read-across-diagonal.hlsl");
}
TEST_F(FileTest, SM6QuadReadLaneAt) {
  runFileTest("sm6.quad-read-lane-at.hlsl");
}

// SPIR-V specific
TEST_F(FileTest, SpirvStorageClass) { runFileTest("spirv.storage-class.hlsl"); }

TEST_F(FileTest, SpirvString) { runFileTest("spirv.string.hlsl"); }

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
TEST_F(FileTest, SpirvBuiltInHelperInvocationVk1p3) {
  runFileTest("spirv.builtin.helper-invocation.vk1p3.hlsl");
}
TEST_F(FileTest, SpirvBuiltInShaderDrawParameters) {
  runFileTest("spirv.builtin.shader-draw-parameters.hlsl");
}
TEST_F(FileTest, SpirvBuiltInDeviceIndex) {
  runFileTest("spirv.builtin.device-index.hlsl");
}

TEST_F(FileTest, SpirvExtensionCLAllow) {
  runFileTest("spirv.ext.cl.allow.hlsl");
}
TEST_F(FileTest, SpirvExtensionAllowAllKHR) {
  runFileTest("spirv.ext.allow-all-khr.hlsl");
}
// Test -Oconfig command line option.
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

TEST_F(FileTest, SpirvStageIOInterfaceVSArraySVClipDistance) {
  runFileTest("spirv.interface.vs.array.sv_clipdistance.hlsl");
}
TEST_F(FileTest, SpirvStageIOInterfacePSArraySVClipDistance) {
  runFileTest("spirv.interface.ps.array.sv_clipdistance.hlsl");
}
TEST_F(FileTest, SpirvStageIOInterfaceVSMultipleArraySVClipDistance) {
  runFileTest("spirv.interface.vs.multiple.array.sv_clipdistance.hlsl");
}
TEST_F(FileTest, SpirvStageIOInterfacePSMultipleArraySVClipDistance) {
  runFileTest("spirv.interface.ps.multiple.array.sv_clipdistance.hlsl");
}
TEST_F(FileTest, SpirvStageIOInterfacePSInheritanceSVClipDistance) {
  runFileTest("spirv.interface.ps.inheritance.sv_clipdistance.hlsl");
}

TEST_F(FileTest, SpirvStageIOAliasBuiltIn) {
  runFileTest("spirv.interface.alias-builtin.hlsl");
}

TEST_F(FileTest, SpirvInterfacesForMultipleEntryPointsSimple) {
  runFileTest("spirv.interface.multiple.entries.simple.hlsl");
}
TEST_F(FileTest, SpirvInterfacesForMultipleEntryPointsBuiltIn) {
  runFileTest("spirv.interface.multiple.entries.built-in.hlsl");
}
TEST_F(FileTest, SpirvInterfacesForMultipleEntryPointsBuiltInVulkan1p2) {
  runFileTest("spirv.interface.multiple.entries.built-in.vk.1p2.hlsl");
}
TEST_F(FileTest, SpirvInterfacesForMultipleEntryPoints) {
  runFileTest("spirv.interface.multiple.entries.hlsl");
}
TEST_F(FileTest, SpirvInterfacesForMultipleEntryPointsVulkan1p2) {
  runFileTest("spirv.interface.multiple.entries.vk.1p2.hlsl");
}

// Test shaders that require Vulkan1.1 support with
// -fspv-target-env=vulkan1.2 option to make sure that enabling
// Vulkan1.2 also enables Vulkan1.1.
TEST_F(FileTest, CompatibilityWithVk1p1) {
  runFileTest("sm6.quad-read-across-diagonal.vulkan1.2.hlsl");
  runFileTest("sm6.quad-read-across-x.vulkan1.2.hlsl");
  runFileTest("sm6.quad-read-across-y.vulkan1.2.hlsl");
  runFileTest("sm6.quad-read-lane-at.vulkan1.2.hlsl");
  runFileTest("sm6.wave-active-all-equal.vulkan1.2.hlsl");
  runFileTest("sm6.wave-active-all-true.vulkan1.2.hlsl");
  runFileTest("sm6.wave-active-any-true.vulkan1.2.hlsl");
  runFileTest("sm6.wave-active-ballot.vulkan1.2.hlsl");
  runFileTest("sm6.wave-active-bit-and.vulkan1.2.hlsl");
  runFileTest("sm6.wave-active-bit-or.vulkan1.2.hlsl");
  runFileTest("sm6.wave-active-bit-xor.vulkan1.2.hlsl");
  runFileTest("sm6.wave-active-count-bits.vulkan1.2.hlsl");
  runFileTest("sm6.wave-active-max.vulkan1.2.hlsl");
  runFileTest("sm6.wave-active-min.vulkan1.2.hlsl");
  runFileTest("sm6.wave-active-product.vulkan1.2.hlsl");
  runFileTest("sm6.wave-active-sum.vulkan1.2.hlsl");
  runFileTest("sm6.wave-get-lane-count.vulkan1.2.hlsl");
  runFileTest("sm6.wave-get-lane-index.vulkan1.2.hlsl");
  runFileTest("sm6.wave-is-first-lane.vulkan1.2.hlsl");
  runFileTest("sm6.wave-prefix-count-bits.vulkan1.2.hlsl");
  runFileTest("sm6.wave-prefix-product.vulkan1.2.hlsl");
  runFileTest("sm6.wave-prefix-sum.vulkan1.2.hlsl");
  runFileTest("sm6.wave-read-lane-at.vulkan1.2.hlsl");
  runFileTest("sm6.wave-read-lane-first.vulkan1.2.hlsl");
  runFileTest("sm6.wave.builtin.no-dup.vulkan1.2.hlsl");
}

TEST_F(FileTest, InlinedCodeTest) {
  const std::string command(R"(// RUN: %dxc -T ps_6_0 -E PSMain)");
  const std::string code = command + R"(
struct PSInput
{
        float4 color : COLOR;
};

// CHECK: OpFunctionCall %v4float %src_PSMain
float4 PSMain(PSInput input) : SV_TARGET
{
        return input.color;
})";
  runCodeTest(code);
}

TEST_F(FileTest, InlinedCodeWithErrorTest) {
  const std::string command(R"(// RUN: %dxc -T ps_6_0 -E PSMain)");
  const std::string code = command + R"(
struct PSInput
{
        float4 color : COLOR;
};

// CHECK: error: cannot initialize return object of type 'float4' with an lvalue of type 'PSInput'
float4 PSMain(PSInput input) : SV_TARGET
{
        return input;
})";
  runCodeTest(code, Expect::Failure);
}

std::string getVertexPositionTypeTestShader(const std::string &subType,
                                            const std::string &positionType,
                                            const std::string &check,
                                            bool use16bit) {
  const std::string code = std::string(R"(// RUN: %dxc -T vs_6_2 -E main)") +
                           (use16bit ? R"( -enable-16bit-types)" : R"()") + R"(
)" + subType + R"(
struct output {
)" + positionType + R"(
};

output main() : SV_Position
{
    output result;
    return result;
}
)" + check;
  return code;
}

const char *kInvalidPositionTypeForVSErrorMessage =
    "// CHECK: error: SV_Position must be a 4-component 32-bit float vector or "
    "a composite which recursively contains only such a vector";

TEST_F(FileTest, PositionInVSWithArrayType) {
  runCodeTest(
      getVertexPositionTypeTestShader(
          "", "float x[4];", kInvalidPositionTypeForVSErrorMessage, false),
      Expect::Failure);
}
TEST_F(FileTest, PositionInVSWithDoubleType) {
  runCodeTest(
      getVertexPositionTypeTestShader(
          "", "double4 x;", kInvalidPositionTypeForVSErrorMessage, false),
      Expect::Failure);
}
TEST_F(FileTest, PositionInVSWithIntType) {
  runCodeTest(getVertexPositionTypeTestShader(
                  "", "int4 x;", kInvalidPositionTypeForVSErrorMessage, false),
              Expect::Failure);
}
TEST_F(FileTest, PositionInVSWithMatrixType) {
  runCodeTest(
      getVertexPositionTypeTestShader(
          "", "float1x4 x;", kInvalidPositionTypeForVSErrorMessage, false),
      Expect::Failure);
}
TEST_F(FileTest, PositionInVSWithInvalidFloatVectorType) {
  runCodeTest(
      getVertexPositionTypeTestShader(
          "", "float3 x;", kInvalidPositionTypeForVSErrorMessage, false),
      Expect::Failure);
}
TEST_F(FileTest, PositionInVSWithInvalidInnerStructType) {
  runCodeTest(getVertexPositionTypeTestShader(
                  R"(
struct InvalidType {
  float3 x;
};)",
                  "InvalidType x;", kInvalidPositionTypeForVSErrorMessage,
                  false),
              Expect::Failure);
}
TEST_F(FileTest, PositionInVSWithValidInnerStructType) {
  runCodeTest(getVertexPositionTypeTestShader(R"(
struct validType {
  float4 x;
};)",
                                              "validType x;", R"(
// CHECK: %validType = OpTypeStruct %v4float
// CHECK:    %output = OpTypeStruct %validType
)",
                                              false));
}
TEST_F(FileTest, PositionInVSWithValidFloatType) {
  runCodeTest(getVertexPositionTypeTestShader("", "float4 x;", R"(
// CHECK:    %output = OpTypeStruct %v4float
)",
                                              false));
}
TEST_F(FileTest, PositionInVSWithValidMin10Float4Type) {
  runCodeTest(getVertexPositionTypeTestShader("", "min10float4 x;", R"(
// CHECK:    %output = OpTypeStruct %v4float
)",
                                              false));
}
TEST_F(FileTest, PositionInVSWithValidMin16Float4Type) {
  runCodeTest(getVertexPositionTypeTestShader("", "min16float4 x;", R"(
// CHECK:    %output = OpTypeStruct %v4float
)",
                                              false));
}
TEST_F(FileTest, PositionInVSWithValidHalf4Type) {
  runCodeTest(getVertexPositionTypeTestShader("", "half4 x;", R"(
// CHECK:    %output = OpTypeStruct %v4float
)",
                                              false));
}
TEST_F(FileTest, PositionInVSWithInvalidHalf4Type) {
  runCodeTest(getVertexPositionTypeTestShader(
                  "", "half4 x;", kInvalidPositionTypeForVSErrorMessage, true),
              Expect::Failure);
}
TEST_F(FileTest, PositionInVSWithInvalidMin10Float4Type) {
  runCodeTest(
      getVertexPositionTypeTestShader(
          "", "min10float4 x;", kInvalidPositionTypeForVSErrorMessage, true),
      Expect::Failure);
}
TEST_F(FileTest, SourceCodeWithoutFilePath) {
  const std::string command(R"(// RUN: %dxc -T ps_6_0 -E PSMain -Zi)");
  const std::string code = command + R"(
float4 PSMain(float4 color : COLOR) : SV_TARGET { return color; }
// CHECK: float4 PSMain(float4 color : COLOR) : SV_TARGET { return color; }
)";
  runCodeTest(code);
}

} // namespace
