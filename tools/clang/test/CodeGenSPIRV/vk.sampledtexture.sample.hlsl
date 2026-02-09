// RUN: %dxc -T ps_6_7 -E main -fcgl %s -spirv | FileCheck %s

// CHECK: OpCapability MinLod
// CHECK: OpCapability SparseResidency

// CHECK: [[v2fc:%[0-9]+]] = OpConstantComposite %v2float %float_0_5 %float_0_25
// CHECK: [[v2ic:%[0-9]+]] = OpConstantComposite %v2int %int_2 %int_3

// CHECK: [[type_2d_image_1:%[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 0 0 0 1 Unknown
// CHECK: [[type_sampled_image_1:%[a-zA-Z0-9_]+]] = OpTypeSampledImage [[type_2d_image_1]]
// CHECK: [[ptr_type_1:%[a-zA-Z0-9_]+]] = OpTypePointer UniformConstant [[type_sampled_image_1]]

// CHECK: [[type_2d_image_2:%[a-zA-Z0-9_]+]] = OpTypeImage %uint 2D 0 0 0 1 Unknown
// CHECK: [[type_sampled_image_2:%[a-zA-Z0-9_]+]] = OpTypeSampledImage [[type_2d_image_2]]
// CHECK: [[ptr_type_2:%[a-zA-Z0-9_]+]] = OpTypePointer UniformConstant [[type_sampled_image_2]]

// CHECK: [[tex1:%[a-zA-Z0-9_]+]] = OpVariable [[ptr_type_1]] UniformConstant
// CHECK: [[tex2:%[a-zA-Z0-9_]+]] = OpVariable [[ptr_type_1]] UniformConstant
// CHECK: [[tex3:%[a-zA-Z0-9_]+]] = OpVariable [[ptr_type_2]] UniformConstant

vk::SampledTexture2D tex1 : register(t0);
vk::SampledTexture2D<float4> tex2 : register(t1);
vk::SampledTexture2D<uint> tex3 : register(t2);

float4 main() : SV_Target {
// CHECK: [[tex1_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_sampled_image_1]] [[tex1]]
// CHECK: [[sampled_result1:%[a-zA-Z0-9_]+]] = OpImageSampleImplicitLod %v4float [[tex1_load]] [[v2fc]] None
    float4 val1 = tex1.Sample(float2(0.5, 0.25));

// CHECK: [[tex2_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_sampled_image_1]] [[tex2]]
// CHECK: [[sampled_result2:%[a-zA-Z0-9_]+]] = OpImageSampleImplicitLod %v4float [[tex2_load]] [[v2fc]] ConstOffset [[v2ic]]
    float4 val2 = tex2.Sample(float2(0.5, 0.25), int2(2, 3));

// CHECK: [[tex3_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_sampled_image_1]] [[tex2]]
// CHECK: [[sampled_result3:%[a-zA-Z0-9_]+]] = OpImageSampleImplicitLod %v4float [[tex3_load]] [[v2fc]] ConstOffset|MinLod [[v2ic]] %float_1
    float4 val3 = tex2.Sample(float2(0.5, 0.25), int2(2, 3), 1.0f);

// CHECK: [[tex4_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_sampled_image_1]] [[tex2]]
// CHECK: [[sampled_result4:%[a-zA-Z0-9_]+]] = OpImageSparseSampleImplicitLod %SparseResidencyStruct [[tex4_load]] [[v2fc]] ConstOffset|MinLod [[v2ic]] %float_1
// CHECK: [[status_0:%[a-zA-Z0-9_]+]] = OpCompositeExtract %uint [[sampled_result4]] 0
// CHECK:                        OpStore %status [[status_0]]
    uint status;
    float4 val4 = tex2.Sample(float2(0.5, 0.25), int2(2, 3), 1.0f, status);

// CHECK: [[tex5_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_sampled_image_2]] [[tex3]]
// CHECK: [[sampled_result5:%[a-zA-Z0-9_]+]] = OpImageSampleImplicitLod %v4uint [[tex5_load]] [[v2fc]] None
// CHECK: [[val5:%[a-zA-Z0-9_]+]] = OpCompositeExtract %uint [[sampled_result5]] 0
    uint val5 = tex3.Sample(float2(0.5, 0.25));
    return 1.0;
}
