// RUN: %dxc -T ps_6_0 -E main -fcgl %s -spirv | FileCheck %s

// CHECK: OpCapability SparseResidency

// CHECK: [[v2fc:%[0-9]+]] = OpConstantComposite %v2float %float_0_5 %float_0_25
// CHECK: [[v2ic:%[0-9]+]] = OpConstantComposite %v2int %int_2 %int_3

// CHECK: [[type_2d_image_1:%[a-zA-Z0-9_]+]] = OpTypeImage %float 2D 0 0 0 1 Unknown
// CHECK: [[type_sampled_image_1:%[a-zA-Z0-9_]+]] = OpTypeSampledImage [[type_2d_image_1]]
// CHECK: [[ptr_type_1:%[a-zA-Z0-9_]+]] = OpTypePointer UniformConstant [[type_sampled_image_1]]

// CHECK: [[type_struct_result:%[a-zA-Z0-9_]+]] = OpTypeStruct %uint %v4float

// CHECK: [[tex1:%[a-zA-Z0-9_]+]] = OpVariable [[ptr_type_1]] UniformConstant

vk::SampledTexture2D<float4> tex1 : register(t1);

float4 main() : SV_Target {
    uint status;
    float4 val = 0;

// CHECK: [[tex1_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_sampled_image_1]] [[tex1]]
// CHECK: [[val_green:%[a-zA-Z0-9_]+]] = OpImageGather %v4float [[tex1_load]] [[v2fc]] %int_1
// CHECK: OpStore %val [[val_green]]
    val = tex1.GatherGreen(float2(0.5, 0.25));

// CHECK: [[tex1_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_sampled_image_1]] [[tex1]]
// CHECK: [[val_green_o:%[a-zA-Z0-9_]+]] = OpImageGather %v4float [[tex1_load]] [[v2fc]] %int_1 ConstOffset [[v2ic]]
// CHECK: OpStore %val [[val_green_o]]
    val = tex1.GatherGreen(float2(0.5, 0.25), int2(2, 3));

// CHECK: [[tex1_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_sampled_image_1]] [[tex1]]
// CHECK: [[val_green_o4:%[a-zA-Z0-9_]+]] = OpImageGather %v4float [[tex1_load]] [[v2fc]] %int_1 ConstOffsets [[const_offsets:%[a-zA-Z0-9_]+]]
// CHECK: OpStore %val [[val_green_o4]]
    val = tex1.GatherGreen(float2(0.5, 0.25), int2(1, 2), int2(3, 4), int2(5, 6), int2(7, 8));

// CHECK: [[tex1_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_sampled_image_1]] [[tex1]]
// CHECK: [[val_green_s:%[a-zA-Z0-9_]+]] = OpImageSparseGather [[type_struct_result]] [[tex1_load]] [[v2fc]] %int_1 ConstOffset [[v2ic]]
// CHECK: [[status_green_s:%[a-zA-Z0-9_]+]] = OpCompositeExtract %uint [[val_green_s]] 0
// CHECK: OpStore %status [[status_green_s]]
// CHECK: [[res_green_s:%[a-zA-Z0-9_]+]] = OpCompositeExtract %v4float [[val_green_s]] 1
// CHECK: OpStore %val [[res_green_s]]
    val = tex1.GatherGreen(float2(0.5, 0.25), int2(2, 3), status);

// CHECK: [[tex1_load:%[a-zA-Z0-9_]+]] = OpLoad [[type_sampled_image_1]] [[tex1]]
// CHECK: [[val_green_o4_s:%[a-zA-Z0-9_]+]] = OpImageSparseGather [[type_struct_result]] [[tex1_load]] [[v2fc]] %int_1 ConstOffsets [[const_offsets]]
// CHECK: [[status_green_o4_s:%[a-zA-Z0-9_]+]] = OpCompositeExtract %uint [[val_green_o4_s]] 0
// CHECK: OpStore %status [[status_green_o4_s]]
// CHECK: [[res_green_o4_s:%[a-zA-Z0-9_]+]] = OpCompositeExtract %v4float [[val_green_o4_s]] 1
// CHECK: OpStore %val [[res_green_o4_s]]
    val = tex1.GatherGreen(float2(0.5, 0.25), int2(1, 2), int2(3, 4), int2(5, 6), int2(7, 8), status);

    return val;
}
