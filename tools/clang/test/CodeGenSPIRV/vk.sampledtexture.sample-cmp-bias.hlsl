// RUN: %dxc -T ps_6_0 -E main -fcgl %s -spirv | FileCheck %s

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
// CHECK: [[type_2d_image_array:%[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 0 1 0 1 Unknown
// CHECK: [[type_2d_sampled_image_array:%[a-zA-Z0-9_]+]] = OpTypeSampledImage [[type_2d_image_array]]
// CHECK: [[type_cube_image:%[a-zA-Z0-9_]+]] = OpTypeImage %float Cube 0 0 0 1 Unknown
// CHECK: [[type_cube_sampled_image:%[a-zA-Z0-9_]+]] = OpTypeSampledImage [[type_cube_image]]
// CHECK: [[type_cube_image_array:%[a-zA-Z0-9_]+]] = OpTypeImage %float Cube 0 1 0 1 Unknown
// CHECK: [[type_cube_sampled_image_array:%[a-zA-Z0-9_]+]] = OpTypeSampledImage [[type_cube_image_array]]

vk::SampledTexture1D<float4> tex1d;
vk::SampledTexture1DArray<float4> tex1dArray;
vk::SampledTexture2D<float4> tex2d;
vk::SampledTexture2DArray<float4> tex2dArray;
vk::SampledTextureCUBE<float4> texCube;
vk::SampledTextureCUBEArray<float4> texCubeArray;

float4 main() : SV_Target {
// CHECK: [[tex1_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_2d_sampled_image]] %tex2d
// CHECK: [[sampled_result1:%[a-zA-Z0-9_]+]] = OpImageSampleDrefImplicitLod %float [[tex1_load]] [[v2fc]] %float_1 Bias|ConstOffset %float_0_5 [[v2ic]]
    float val1 = tex2d.SampleCmpBias(float2(0.5, 0.25), 1.0f, 0.5f, int2(2, 3));

// CHECK: [[tex2_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_2d_sampled_image]] %tex2d
// CHECK: [[sampled_result2:%[a-zA-Z0-9_]+]] = OpImageSampleDrefImplicitLod %float [[tex2_load]] [[v2fc]] %float_1 Bias|ConstOffset|MinLod %float_0_5 [[v2ic]] %float_2_5
    float val2 = tex2d.SampleCmpBias(float2(0.5, 0.25), 1.0f, 0.5f, int2(2, 3), 2.5f);

// CHECK: [[tex3_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_2d_sampled_image]] %tex2d
// CHECK: [[sampled_result3:%[a-zA-Z0-9_]+]] = OpImageSparseSampleDrefImplicitLod %SparseResidencyStruct [[tex3_load]] [[v2fc]] %float_1 Bias|ConstOffset|MinLod %float_0_5 [[v2ic]] %float_2_5
// CHECK: [[status_0:%[a-zA-Z0-9_]+]] = OpCompositeExtract %uint [[sampled_result3]] 0
// CHECK:                        OpStore %status [[status_0]]
    uint status;
    float val3 = tex2d.SampleCmpBias(float2(0.5, 0.25), 1.0f, 0.5f, int2(2, 3), 2.5f, status);

// CHECK: [[load_arr1:%[a-zA-Z0-9_]+]] = OpLoad {{.*}} %tex2dArray
// CHECK: [[sampled_arr1:%[a-zA-Z0-9_]+]] = OpImageSampleDrefImplicitLod %float [[load_arr1]] [[v3fc]] %float_1 Bias|ConstOffset %float_0_5 [[v2ic]]
    float val4 = tex2dArray.SampleCmpBias(float3(0.5, 0.25, 0), 1.0f, 0.5f, int2(2, 3));

// CHECK: [[tex1d_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_1d_sampled_image]] %tex1d
// CHECK: [[sampled_1d:%[a-zA-Z0-9_]+]] = OpImageSampleDrefImplicitLod %float [[tex1d_load]] %float_0_5 %float_1 Bias %float_0_5
    float val5 = tex1d.SampleCmpBias(0.5, 1.0f, 0.5f);

// CHECK: [[tex1da_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_1d_sampled_image_array]] %tex1dArray
// CHECK: [[sampled_1da:%[a-zA-Z0-9_]+]] = OpImageSampleDrefImplicitLod %float [[tex1da_load]] {{%[0-9]+}} %float_1 Bias %float_0_5
    float val6 = tex1dArray.SampleCmpBias(float2(0.5, 0), 1.0f, 0.5f);

// CHECK: [[cube_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_cube_sampled_image]] %texCube
// CHECK: [[sampled_cube:%[a-zA-Z0-9_]+]] = OpImageSampleDrefImplicitLod %float [[cube_load]] {{%[0-9]+}} %float_1 Bias %float_0_5
    float val7 = texCube.SampleCmpBias(float3(0.5, 0.25, 0), 1.0f, 0.5f);

// CHECK: [[cube_arr_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_cube_sampled_image_array]] %texCubeArray
// CHECK: [[sampled_cube_arr:%[a-zA-Z0-9_]+]] = OpImageSampleDrefImplicitLod %float [[cube_arr_load]] [[v4fc]] %float_1 Bias %float_0_5
    float val8 = texCubeArray.SampleCmpBias(float4(0.5, 0.25, 0, 0), 1.0f, 0.5f);

    return 1.0;
}
