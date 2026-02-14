// RUN: %dxc -T ps_6_7 -E main -fcgl  %s -spirv | FileCheck %s

// CHECK: OpCapability SparseResidency

// CHECK: [[v2fc:%[0-9]+]] = OpConstantComposite %v2float %float_0_5 %float_0_25
// CHECK: [[v2ic:%[0-9]+]] = OpConstantComposite %v2int %int_2 %int_3

// CHECK: [[type_2d_image_1:%[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 0 0 0 1 Unknown
// CHECK: [[type_sampled_image_1:%[a-zA-Z0-9_]+]] = OpTypeSampledImage [[type_2d_image_1]]
// CHECK: [[ptr_type_1:%[a-zA-Z0-9_]+]] = OpTypePointer UniformConstant [[type_sampled_image_1]]

// CHECK: [[type_2d_image_2:%[a-zA-Z0-9_]+]] = OpTypeImage %uint 2D 0 0 0 1 Unknown
// CHECK: [[type_sampled_image_2:%[a-zA-Z0-9_]+]] = OpTypeSampledImage [[type_2d_image_2]]
// CHECK: [[ptr_type_2:%[a-zA-Z0-9_]+]] = OpTypePointer UniformConstant [[type_sampled_image_2]]

// CHECK: %SparseResidencyStruct = OpTypeStruct %uint %v4float

// CHECK: [[tex1:%[a-zA-Z0-9_]+]] = OpVariable [[ptr_type_1]] UniformConstant
// CHECK: [[tex2:%[a-zA-Z0-9_]+]] = OpVariable [[ptr_type_2]] UniformConstant

vk::SampledTexture2D<float4> tex1 : register(t1);
vk::SampledTexture2D<uint> tex2 : register(t2);

float4 main() : SV_Target {

// CHECK: [[tex1_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_sampled_image_1]] [[tex1]]
// CHECK:      [[val1:%[a-zA-Z0-9_]+]] = OpImageGather %v4float [[tex1_load]] [[v2fc]] %int_0 None
    float4 val1 = tex1.Gather(float2(0.5, 0.25));

// CHECK: [[tex2_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_sampled_image_2]] [[tex2]]
// CHECK:      [[val2:%[a-zA-Z0-9_]+]] = OpImageGather %v4uint [[tex2_load]] [[v2fc]] %int_0 ConstOffset [[v2ic]]
    uint4 val2 = tex2.Gather(float2(0.5, 0.25), int2(2, 3));

// CHECK: [[tex3_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_sampled_image_1]] [[tex1]]
// CHECK:      [[val3:%[a-zA-Z0-9_]+]] = OpImageSparseGather %SparseResidencyStruct [[tex3_load]] [[v2fc]] %int_0 ConstOffset [[v2ic]]
// CHECK:  [[status_0:%[a-zA-Z0-9_]+]] = OpCompositeExtract %uint [[val3]] 0
// CHECK:                                OpStore %status [[status_0]]
    uint status;
    float4 val3 = tex1.Gather(float2(0.5, 0.25), int2(2, 3), status);

    return 1.0;
}
