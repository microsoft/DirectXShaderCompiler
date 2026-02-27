// RUN: %dxc -T ps_6_0 -E main -fcgl %s -spirv | FileCheck %s

// CHECK: OpCapability SparseResidency

// CHECK: [[v2fc:%[0-9]+]] = OpConstantComposite %v2float %float_0_5 %float_0_25
// CHECK: [[v2ic:%[0-9]+]] = OpConstantComposite %v2int %int_2 %int_3
// CHECK: [[v3fc:%[0-9]+]] = OpConstantComposite %v3float %float_0_5 %float_0_25 %float_0

// CHECK: [[type_1d_image:%[a-zA-Z0-9_]+]] = OpTypeImage %float 1D 0 0 0 1 Unknown
// CHECK: [[type_1d_sampled_image:%[a-zA-Z0-9_]+]] = OpTypeSampledImage [[type_1d_image]]
// CHECK: [[type_1d_image_array:%[a-zA-Z0-9_]+]] = OpTypeImage %float 1D 0 1 0 1 Unknown
// CHECK: [[type_1d_sampled_image_array:%[a-zA-Z0-9_]+]] = OpTypeSampledImage [[type_1d_image_array]]
// CHECK: [[type_2d_image:%[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 0 0 0 1 Unknown
// CHECK: [[type_2d_sampled_image:%[a-zA-Z0-9_]+]] = OpTypeSampledImage [[type_2d_image]]
// CHECK: [[type_2d_image_array:%[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 0 1 0 1 Unknown
// CHECK: [[type_2d_sampled_image_array:%[a-zA-Z0-9_]+]] = OpTypeSampledImage [[type_2d_image_array]]
// CHECK: [[type_3d_image:%[a-zA-Z0-9_]+]] = OpTypeImage %float 3D 0 0 0 1 Unknown
// CHECK: [[type_3d_sampled_image:%[a-zA-Z0-9_]+]] = OpTypeSampledImage [[type_3d_image]]

vk::SampledTexture1D<float4> tex1d;
vk::SampledTexture1DArray<float4> tex1dArray;
vk::SampledTexture2D<float4> tex2d;
vk::SampledTexture2DArray<float4> tex2dArray;
vk::SampledTexture3D<float4> tex3d;

float4 main() : SV_Target {
// CHECK: [[tex1_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_2d_sampled_image]] %tex2d
// CHECK: [[sampled_result1:%[a-zA-Z0-9_]+]] = OpImageSampleExplicitLod %v4float [[tex1_load]] [[v2fc]] Lod %float_0_5
    float4 val1 = tex2d.SampleLevel(float2(0.5, 0.25), 0.5f);

// CHECK: [[tex2_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_2d_sampled_image]] %tex2d
// CHECK: [[sampled_result2:%[a-zA-Z0-9_]+]] = OpImageSampleExplicitLod %v4float [[tex2_load]] [[v2fc]] Lod|ConstOffset %float_0_5 [[v2ic]]
    float4 val2 = tex2d.SampleLevel(float2(0.5, 0.25), 0.5f, int2(2, 3));

// CHECK: [[tex3_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_2d_sampled_image]] %tex2d
// CHECK: [[sampled_result3:%[a-zA-Z0-9_]+]] = OpImageSparseSampleExplicitLod %SparseResidencyStruct [[tex3_load]] [[v2fc]] Lod|ConstOffset %float_0_5 [[v2ic]]
// CHECK: [[status_0:%[a-zA-Z0-9_]+]] = OpCompositeExtract %uint [[sampled_result3]] 0
// CHECK:                        OpStore %status [[status_0]]
    uint status;
    float4 val3 = tex2d.SampleLevel(float2(0.5, 0.25), 0.5f, int2(2, 3), status);

// CHECK: [[load_arr1:%[a-zA-Z0-9_]+]] = OpLoad [[type_2d_sampled_image_array]] %tex2dArray
// CHECK: [[sampled_arr1:%[a-zA-Z0-9_]+]] = OpImageSampleExplicitLod %v4float [[load_arr1]] [[v3fc]] Lod %float_0_5
    float4 val4 = tex2dArray.SampleLevel(float3(0.5, 0.25, 0), 0.5f);

// CHECK: [[tex1d_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_1d_sampled_image]] %tex1d
// CHECK: [[sampled_1d:%[a-zA-Z0-9_]+]] = OpImageSampleExplicitLod %v4float [[tex1d_load]] %float_0_5 Lod %float_0_5
    float4 val5 = tex1d.SampleLevel(0.5, 0.5f);

// CHECK: [[tex1da_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_1d_sampled_image_array]] %tex1dArray
// CHECK: [[sampled_1da:%[a-zA-Z0-9_]+]] = OpImageSampleExplicitLod %v4float [[tex1da_load]] {{%[0-9]+}} Lod %float_0_5
    float4 val6 = tex1dArray.SampleLevel(float2(0.5, 0), 0.5f);

// CHECK: [[tex3d_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_3d_sampled_image]] %tex3d
// CHECK: [[sampled_3d:%[a-zA-Z0-9_]+]] = OpImageSampleExplicitLod %v4float [[tex3d_load]] [[v3fc]] Lod %float_0_5
    float4 val7 = tex3d.SampleLevel(float3(0.5, 0.25, 0), 0.5f);

    return 1.0;
}
