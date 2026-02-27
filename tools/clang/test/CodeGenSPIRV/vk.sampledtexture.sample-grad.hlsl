// RUN: %dxc -T ps_6_0 -E main -fcgl %s -spirv | FileCheck %s

// CHECK: OpCapability MinLod
// CHECK: OpCapability SparseResidency

// CHECK: [[v2fc:%[0-9]+]] = OpConstantComposite %v2float %float_0_5 %float_0_25
// CHECK: [[v2f_1:%[0-9]+]] = OpConstantComposite %v2float %float_1 %float_1
// CHECK: [[v2f_2:%[0-9]+]] = OpConstantComposite %v2float %float_2 %float_2
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
// CHECK: [[sampled_result1:%[a-zA-Z0-9_]+]] = OpImageSampleExplicitLod %v4float [[tex1_load]] [[v2fc]] Grad [[v2f_1]] [[v2f_2]]
    float4 val1 = tex2d.SampleGrad(float2(0.5, 0.25), float2(1, 1), float2(2, 2));

// CHECK: [[tex2_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_2d_sampled_image]] %tex2d
// CHECK: [[sampled_result2:%[a-zA-Z0-9_]+]] = OpImageSampleExplicitLod %v4float [[tex2_load]] [[v2fc]] Grad|ConstOffset [[v2f_1]] [[v2f_2]] [[v2ic]]
    float4 val2 = tex2d.SampleGrad(float2(0.5, 0.25), float2(1, 1), float2(2, 2), int2(2,3));

// CHECK: [[tex3_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_2d_sampled_image]] %tex2d
// CHECK: [[sampled_result3:%[a-zA-Z0-9_]+]] = OpImageSampleExplicitLod %v4float [[tex3_load]] [[v2fc]] Grad|ConstOffset|MinLod [[v2f_1]] [[v2f_2]] [[v2ic]] %float_0_5
    float4 val3 = tex2d.SampleGrad(float2(0.5, 0.25), float2(1, 1), float2(2, 2), int2(2,3), 0.5);

// CHECK: [[tex4_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_2d_sampled_image]] %tex2d
// CHECK: [[sampled_result4:%[a-zA-Z0-9_]+]] = OpImageSparseSampleExplicitLod %SparseResidencyStruct [[tex4_load]] [[v2fc]] Grad|ConstOffset|MinLod [[v2f_1]] [[v2f_2]] [[v2ic]] %float_0_5
// CHECK: [[status_0:%[a-zA-Z0-9_]+]] = OpCompositeExtract %uint [[sampled_result4]] 0
// CHECK:                        OpStore %status [[status_0]]
    uint status;
    float4 val4 = tex2d.SampleGrad(float2(0.5, 0.25), float2(1, 1), float2(2, 2), int2(2,3), 0.5, status);

// CHECK: [[load_arr1:%[a-zA-Z0-9_]+]] = OpLoad [[type_2d_sampled_image_array]] %tex2dArray
// CHECK: [[sampled_arr1:%[a-zA-Z0-9_]+]] = OpImageSampleExplicitLod %v4float [[load_arr1]] [[v3fc]] Grad [[v2f_1]] [[v2f_2]]
    float4 val5 = tex2dArray.SampleGrad(float3(0.5, 0.25, 0), float2(1, 1), float2(2, 2));

// CHECK: [[tex1d_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_1d_sampled_image]] %tex1d
// CHECK: [[sampled_1d:%[a-zA-Z0-9_]+]] = OpImageSampleExplicitLod %v4float [[tex1d_load]] %float_0_5 Grad %float_1 %float_2
    float4 val6 = tex1d.SampleGrad(0.5, 1.0, 2.0);

// CHECK: [[tex1da_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_1d_sampled_image_array]] %tex1dArray
// CHECK: [[sampled_1da:%[a-zA-Z0-9_]+]] = OpImageSampleExplicitLod %v4float [[tex1da_load]] {{%[0-9]+}} Grad %float_1 %float_2
    float4 val7 = tex1dArray.SampleGrad(float2(0.5, 0), 1.0, 2.0);

// CHECK: [[tex3d_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_3d_sampled_image]] %tex3d
// CHECK: [[sampled_3d:%[a-zA-Z0-9_]+]] = OpImageSampleExplicitLod %v4float [[tex3d_load]] [[v3fc]] Grad [[v3fc]] [[v3fc]]
    float4 val8 = tex3d.SampleGrad(float3(0.5, 0.25, 0), float3(0.5, 0.25, 0), float3(0.5, 0.25, 0));

    return 1.0;
}
