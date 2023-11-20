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
