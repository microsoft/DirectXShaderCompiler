// RUN: %dxc -T ps_6_0 -E main -fcgl %s -spirv | FileCheck %s

// CHECK: OpCapability MinLod
// CHECK: OpCapability SparseResidency

// CHECK: [[v2fc:%[0-9]+]] = OpConstantComposite %v2float %float_0_5 %float_0_25
// CHECK: [[v2f_1:%[0-9]+]] = OpConstantComposite %v2float %float_1 %float_1
// CHECK: [[v2f_2:%[0-9]+]] = OpConstantComposite %v2float %float_2 %float_2
// CHECK: [[v2ic:%[0-9]+]] = OpConstantComposite %v2int %int_2 %int_3

// CHECK: [[type_2d_image_1:%[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 0 0 0 1 Unknown
// CHECK: [[type_sampled_image_1:%[a-zA-Z0-9_]+]] = OpTypeSampledImage [[type_2d_image_1]]
// CHECK: [[ptr_type_1:%[a-zA-Z0-9_]+]] = OpTypePointer UniformConstant [[type_sampled_image_1]]

// CHECK: [[tex1:%[a-zA-Z0-9_]+]] = OpVariable [[ptr_type_1]] UniformConstant

vk::SampledTexture2D<float4> tex1 : register(t0);

float4 main() : SV_Target {
// CHECK: [[tex1_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_sampled_image_1]] [[tex1]]
// CHECK: [[sampled_result1:%[a-zA-Z0-9_]+]] = OpImageSampleDrefExplicitLod %float [[tex1_load]] [[v2fc]] %float_1 Grad [[v2f_1]] [[v2f_2]]
    float val1 = tex1.SampleCmpGrad(float2(0.5, 0.25), 1.0f, float2(1, 1), float2(2, 2));

// CHECK: [[tex2_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_sampled_image_1]] [[tex1]]
// CHECK: [[sampled_result2:%[a-zA-Z0-9_]+]] = OpImageSampleDrefExplicitLod %float [[tex2_load]] [[v2fc]] %float_1 Grad|ConstOffset [[v2f_1]] [[v2f_2]] [[v2ic]]
    float val2 = tex1.SampleCmpGrad(float2(0.5, 0.25), 1.0f, float2(1, 1), float2(2, 2), int2(2,3));

// CHECK: [[tex3_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_sampled_image_1]] [[tex1]]
// CHECK: [[sampled_result3:%[a-zA-Z0-9_]+]] = OpImageSampleDrefExplicitLod %float [[tex3_load]] [[v2fc]] %float_1 Grad|ConstOffset|MinLod [[v2f_1]] [[v2f_2]] [[v2ic]] %float_0_5
    float val3 = tex1.SampleCmpGrad(float2(0.5, 0.25), 1.0f, float2(1, 1), float2(2, 2), int2(2,3), 0.5);

// CHECK: [[tex4_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_sampled_image_1]] [[tex1]]
// CHECK: [[sampled_result4:%[a-zA-Z0-9_]+]] = OpImageSparseSampleDrefExplicitLod %SparseResidencyStruct [[tex4_load]] [[v2fc]] %float_1 Grad|ConstOffset|MinLod [[v2f_1]] [[v2f_2]] [[v2ic]] %float_0_5
// CHECK: [[status_0:%[a-zA-Z0-9_]+]] = OpCompositeExtract %uint [[sampled_result4]] 0
// CHECK:                        OpStore %status [[status_0]]
    uint status;
    float val4 = tex1.SampleCmpGrad(float2(0.5, 0.25), 1.0f, float2(1, 1), float2(2, 2), int2(2,3), 0.5, status);
    return 1.0;
}
