// RUN: %dxc -T ps_6_7 -E main -fcgl %s -spirv | FileCheck %s

// CHECK: OpCapability MinLod
// CHECK: OpCapability SparseResidency

// CHECK: [[v2fc:%[0-9]+]] = OpConstantComposite %v2float %float_0_5 %float_0_25
// CHECK: [[v2ic:%[0-9]+]] = OpConstantComposite %v2int %int_2 %int_3
// CHECK: [[v3fc:%[0-9]+]] = OpConstantComposite %v3float %float_0_5 %float_0_25 %float_0
// CHECK: [[v4fc:%[0-9]+]] = OpConstantComposite %v4float %float_0_5 %float_0_25 %float_0 %float_0

// CHECK: [[type_1d_image:%[a-zA-Z0-9_]+]] = OpTypeImage %float 1D 0 0 0 1 Unknown
// CHECK: [[type_1d_sampled_image:%[a-zA-Z0-9_]+]] = OpTypeSampledImage [[type_1d_image]]
// CHECK: [[type_1d_image_array:%[a-zA-Z0-9_]+]] = OpTypeImage %float 1D 0 1 0 1 Unknown
// CHECK: [[type_1d_sampled_image_array:%[a-zA-Z0-9_]+]] = OpTypeSampledImage [[type_1d_image_array]]
// CHECK: [[type_2d_image:%[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 0 0 0 1 Unknown
// CHECK: [[type_2d_sampled_image:%[a-zA-Z0-9_]+]] = OpTypeSampledImage [[type_2d_image]]
// CHECK: [[type_2d_image_uint:%[a-zA-Z0-9_]+]] = OpTypeImage %uint 2D 0 0 0 1 Unknown
// CHECK: [[type_2d_sampled_image_uint:%[a-zA-Z0-9_]+]] = OpTypeSampledImage [[type_2d_image_uint]]
// CHECK: [[type_2d_image_array:%[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 0 1 0 1 Unknown
// CHECK: [[type_2d_sampled_image_array:%[a-zA-Z0-9_]+]] = OpTypeSampledImage [[type_2d_image_array]]
// CHECK: [[type_cube_image:%[a-zA-Z0-9_]+]] = OpTypeImage %float Cube 0 0 0 1 Unknown
// CHECK: [[type_cube_sampled_image:%[a-zA-Z0-9_]+]] = OpTypeSampledImage [[type_cube_image]]
// CHECK: [[type_cube_image_array:%[a-zA-Z0-9_]+]] = OpTypeImage %float Cube 0 1 0 1 Unknown
// CHECK: [[type_cube_sampled_image_array:%[a-zA-Z0-9_]+]] = OpTypeSampledImage [[type_cube_image_array]]

vk::SampledTexture1D<float4> tex1d;
vk::SampledTexture1DArray<float4> tex1dArray;
vk::SampledTexture2D<float4> tex2df4;
vk::SampledTexture2D<uint> tex2duint;
vk::SampledTexture2DArray<float4> tex2dArray;
vk::SampledTextureCUBE<float4> texCube;
vk::SampledTextureCUBEArray<float4> texCubeArray;
vk::SampledTexture2D tex2d_default;

float4 main() : SV_Target {
// CHECK: [[tex1_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_2d_sampled_image]] %tex2df4
// CHECK: [[sampled_result1:%[a-zA-Z0-9_]+]] = OpImageSampleImplicitLod %v4float [[tex1_load]] [[v2fc]] None
    float4 val1 = tex2df4.Sample(float2(0.5, 0.25));

// CHECK: [[tex2_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_2d_sampled_image]] %tex2d_default
// CHECK: [[sampled_result2:%[a-zA-Z0-9_]+]] = OpImageSampleImplicitLod %v4float [[tex2_load]] [[v2fc]] ConstOffset [[v2ic]]
    float4 val2 = tex2d_default.Sample(float2(0.5, 0.25), int2(2, 3));

// CHECK: [[tex3_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_2d_sampled_image]] %tex2df4
// CHECK: [[sampled_result3:%[a-zA-Z0-9_]+]] = OpImageSampleImplicitLod %v4float [[tex3_load]] [[v2fc]] ConstOffset|MinLod [[v2ic]] %float_1
    float4 val3 = tex2df4.Sample(float2(0.5, 0.25), int2(2, 3), 1.0f);

// CHECK: [[tex4_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_2d_sampled_image]] %tex2df4
// CHECK: [[sampled_result4:%[a-zA-Z0-9_]+]] = OpImageSparseSampleImplicitLod %SparseResidencyStruct [[tex4_load]] [[v2fc]] ConstOffset|MinLod [[v2ic]] %float_1
// CHECK: [[status_0:%[a-zA-Z0-9_]+]] = OpCompositeExtract %uint [[sampled_result4]] 0
// CHECK:                        OpStore %status [[status_0]]
    uint status;
    float4 val4 = tex2df4.Sample(float2(0.5, 0.25), int2(2, 3), 1.0f, status);

// CHECK: [[tex5_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_2d_sampled_image_uint]] %tex2duint
// CHECK: [[sampled_result5:%[a-zA-Z0-9_]+]] = OpImageSampleImplicitLod %v4uint [[tex5_load]] [[v2fc]] None
// CHECK: [[val5:%[a-zA-Z0-9_]+]] = OpCompositeExtract %uint [[sampled_result5]] 0
    uint val5 = tex2duint.Sample(float2(0.5, 0.25));

// CHECK: [[texArr_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_2d_sampled_image_array]] %tex2dArray
// CHECK: [[sampled_arr1:%[a-zA-Z0-9_]+]] = OpImageSampleImplicitLod %v4float [[texArr_load]] [[v3fc]] None
    float4 val6 = tex2dArray.Sample(float3(0.5, 0.25, 0));

// CHECK: [[tex1d_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_1d_sampled_image]] %tex1d
// CHECK: [[sampled_result_1d:%[a-zA-Z0-9_]+]] = OpImageSampleImplicitLod %v4float [[tex1d_load]] %float_0_5 None
    float4 val7 = tex1d.Sample(0.5);

// CHECK: [[tex1dArr_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_1d_sampled_image_array]] %tex1dArray
// CHECK: [[sampled_result_1d_arr:%[a-zA-Z0-9]+]] = OpImageSampleImplicitLod %v4float [[tex1dArr_load]] [[v2fc]] None
    float4 val8 = tex1dArray.Sample(float2(0.5, 0.25));

// CHECK: [[cube_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_cube_sampled_image]] %texCube
// CHECK: [[cube_sample:%[a-zA-Z0-9_]+]] = OpImageSampleImplicitLod %v4float [[cube_load]] {{%[0-9]+}} None
    float4 val9 = texCube.Sample(float3(0.5, 0.25, 0));

// CHECK: [[cube_arr_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_cube_sampled_image_array]] %texCubeArray
// CHECK: [[cube_arr_sample:%[a-zA-Z0-9_]+]] = OpImageSampleImplicitLod %v4float [[cube_arr_load]] [[v4fc]] None
    float4 val10 = texCubeArray.Sample(float4(0.5, 0.25, 0, 0));

    return 1.0;
}
