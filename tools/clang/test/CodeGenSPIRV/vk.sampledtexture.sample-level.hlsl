// RUN: %dxc -T ps_6_0 -E main -fcgl %s -spirv | FileCheck %s

// CHECK: OpCapability SparseResidency

// CHECK: [[v2fc:%[0-9]+]] = OpConstantComposite %v2float %float_0_5 %float_0_25
// CHECK: [[v2ic:%[0-9]+]] = OpConstantComposite %v2int %int_2 %int_3

// CHECK: [[type_2d_image_1:%[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 0 0 0 1 Unknown
// CHECK: [[type_sampled_image_1:%[a-zA-Z0-9_]+]] = OpTypeSampledImage [[type_2d_image_1]]
// CHECK: [[ptr_type_1:%[a-zA-Z0-9_]+]] = OpTypePointer UniformConstant [[type_sampled_image_1]]

// CHECK: [[tex1:%[a-zA-Z0-9_]+]] = OpVariable [[ptr_type_1]] UniformConstant

vk::SampledTexture2D<float4> tex1 : register(t0);

float4 main() : SV_Target {
// CHECK: [[tex1_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_sampled_image_1]] [[tex1]]
// CHECK: [[sampled_result1:%[a-zA-Z0-9_]+]] = OpImageSampleExplicitLod %v4float [[tex1_load]] [[v2fc]] Lod|ConstOffset %float_0_5 [[v2ic]]
    float4 val1 = tex1.SampleLevel(float2(0.5, 0.25), 0.5f, int2(2, 3));

// CHECK: [[tex2_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_sampled_image_1]] [[tex1]]
// CHECK: [[sampled_result2:%[a-zA-Z0-9_]+]] = OpImageSampleExplicitLod %v4float [[tex2_load]] [[v2fc]] Lod|ConstOffset %float_0_5 [[v2ic]]
    float4 val2 = tex1.SampleLevel(float2(0.5, 0.25), 0.5f, int2(2, 3));

// CHECK: [[tex3_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_sampled_image_1]] [[tex1]]
// CHECK: [[sampled_result3:%[a-zA-Z0-9_]+]] = OpImageSparseSampleExplicitLod %SparseResidencyStruct [[tex3_load]] [[v2fc]] Lod|ConstOffset %float_0_5 [[v2ic]]
// CHECK: [[status_0:%[a-zA-Z0-9_]+]] = OpCompositeExtract %uint [[sampled_result3]] 0
// CHECK:                        OpStore %status [[status_0]]
    uint status;
    float4 val3 = tex1.SampleLevel(float2(0.5, 0.25), 0.5f, int2(2, 3), status);
    return 1.0;
}
