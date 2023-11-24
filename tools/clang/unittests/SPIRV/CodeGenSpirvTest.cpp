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
