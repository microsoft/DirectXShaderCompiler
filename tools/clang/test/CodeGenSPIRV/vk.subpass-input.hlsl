// Run: %dxc -T ps_6_0 -E main

// CHECK: OpCapability InputAttachment

// CHECK:  [[v2i00:%\d+]] = OpConstantComposite %v2int %int_0 %int_0

// CHECK: %type_subpass_image = OpTypeImage %float SubpassData 2 0 0 2 Unknown
// CHECK: %_ptr_UniformConstant_type_subpass_image = OpTypePointer UniformConstant %type_subpass_image

// CHECK: %type_subpass_image_0 = OpTypeImage %int SubpassData 2 0 0 2 Unknown
// CHECK: %_ptr_UniformConstant_type_subpass_image_0 = OpTypePointer UniformConstant %type_subpass_image_0

// CHECK: %type_subpass_image_1 = OpTypeImage %uint SubpassData 2 0 0 2 Unknown
// CHECK: %_ptr_UniformConstant_type_subpass_image_1 = OpTypePointer UniformConstant %type_subpass_image_1

// CHECK: %type_subpass_image_2 = OpTypeImage %uint SubpassData 2 0 1 2 Unknown
// CHECK: %_ptr_UniformConstant_type_subpass_image_2 = OpTypePointer UniformConstant %type_subpass_image_2

// CHECK: %type_subpass_image_3 = OpTypeImage %float SubpassData 2 0 1 2 Unknown
// CHECK: %_ptr_UniformConstant_type_subpass_image_3 = OpTypePointer UniformConstant %type_subpass_image_3

// CHECK: %type_subpass_image_4 = OpTypeImage %int SubpassData 2 0 1 2 Unknown
// CHECK: %_ptr_UniformConstant_type_subpass_image_4 = OpTypePointer UniformConstant %type_subpass_image_4

// CHCK:   %SI_f4 = OpVariable %_ptr_UniformConstant_type_subpass_image UniformConstant
[[vk::input_attachment_index(0)]]  SubpassInput           SI_f4;
// CHCK:   %SI_i3 = OpVariable %_ptr_UniformConstant_type_subpass_image_0 UniformConstant
[[vk::input_attachment_index(1)]]  SubpassInput<int3>     SI_i3;
// CHCK:   %SI_u2 = OpVariable %_ptr_UniformConstant_type_subpass_image_1 UniformConstant
[[vk::input_attachment_index(2)]]  SubpassInput<uint2>    SI_u2;
// CHCK:   %SI_f1 = OpVariable %_ptr_UniformConstant_type_subpass_image UniformConstant
[[vk::input_attachment_index(3)]]  SubpassInput<float>    SI_f1;

// CHCK: %SIMS_u4 = OpVariable %_ptr_UniformConstant_type_subpass_image_2 UniformConstant
[[vk::input_attachment_index(10)]] SubpassInputMS<uint4>  SIMS_u4;
// CHCK: %SIMS_f3 = OpVariable %_ptr_UniformConstant_type_subpass_image_3 UniformConstant
[[vk::input_attachment_index(11)]] SubpassInputMS<float3> SIMS_f3;
// CHCK: %SIMS_i2 = OpVariable %_ptr_UniformConstant_type_subpass_image_4 UniformConstant
[[vk::input_attachment_index(12)]] SubpassInputMS<int2>   SIMS_i2;
// CHCK: %SIMS_u1 = OpVariable %_ptr_UniformConstant_type_subpass_image_2 UniformConstant
[[vk::input_attachment_index(13)]] SubpassInputMS<uint>   SIMS_u1;

float4 main() : SV_Target {
// CHECK:        [[img:%\d+]] = OpLoad %type_subpass_image %SI_f4
// CHECK-NEXT: [[texel:%\d+]] = OpImageRead %v4float [[img]] [[v2i00]] None
// CHECK-NEXT:                  OpStore %v0 [[texel]]
    float4 v0 = SI_f4.SubpassLoad();
// CHECK:        [[img:%\d+]] = OpLoad %type_subpass_image_0 %SI_i3
// CHECK-NEXT: [[texel:%\d+]] = OpImageRead %v4int [[img]] [[v2i00]] None
// CHECK-NEXT:   [[val:%\d+]] = OpVectorShuffle %v3int [[texel]] [[texel]] 0 1 2
// CHECK-NEXT:                  OpStore %v1 [[val]]
    int3   v1 = SI_i3.SubpassLoad();
// CHECK:        [[img:%\d+]] = OpLoad %type_subpass_image_1 %SI_u2
// CHECK-NEXT: [[texel:%\d+]] = OpImageRead %v4uint [[img]] [[v2i00]] None
// CHECK-NEXT:   [[val:%\d+]] = OpVectorShuffle %v2uint [[texel]] [[texel]] 0 1
// CHECK-NEXT:                  OpStore %v2 [[val]]
    uint2  v2 = SI_u2.SubpassLoad();
// CHECK:        [[img:%\d+]] = OpLoad %type_subpass_image %SI_f1
// CHECK-NEXT: [[texel:%\d+]] = OpImageRead %v4float [[img]] [[v2i00]] None
// CHECK-NEXT:   [[val:%\d+]] = OpCompositeExtract %float [[texel]] 0
// CHECK-NEXT:                  OpStore %v3 [[val]]
    float  v3 = SI_f1.SubpassLoad();

// CHECK:        [[img:%\d+]] = OpLoad %type_subpass_image_2 %SIMS_u4
// CHECK-NEXT: [[texel:%\d+]] = OpImageRead %v4uint [[img]] [[v2i00]] Sample %int_1
// CHECK-NEXT:                  OpStore %v10 [[texel]]
    uint4  v10 = SIMS_u4.SubpassLoad(1);
// CHECK:        [[img:%\d+]] = OpLoad %type_subpass_image_3 %SIMS_f3
// CHECK-NEXT: [[texel:%\d+]] = OpImageRead %v4float [[img]] [[v2i00]] Sample %int_2
// CHECK-NEXT:   [[val:%\d+]] = OpVectorShuffle %v3float [[texel]] [[texel]] 0 1 2
// CHECK-NEXT:                  OpStore %v11 [[val]]
    float3 v11 = SIMS_f3.SubpassLoad(2);
// CHECK:        [[img:%\d+]] = OpLoad %type_subpass_image_4 %SIMS_i2
// CHECK-NEXT: [[texel:%\d+]] = OpImageRead %v4int [[img]] [[v2i00]] Sample %int_3
// CHECK-NEXT:   [[val:%\d+]] = OpVectorShuffle %v2int [[texel]] [[texel]] 0 1
// CHECK-NEXT:                  OpStore %v12 [[val]]
    int2   v12 = SIMS_i2.SubpassLoad(3);
// CHECK:        [[img:%\d+]] = OpLoad %type_subpass_image_2 %SIMS_u1
// CHECK-NEXT: [[texel:%\d+]] = OpImageRead %v4uint [[img]] [[v2i00]] Sample %int_4
// CHECK-NEXT:   [[val:%\d+]] = OpCompositeExtract %uint [[texel]] 0
// CHECK-NEXT:                  OpStore %v13 [[val]]
    uint   v13 = SIMS_u1.SubpassLoad(4);

    return v0.x + v1.y + v2.x + v3 + v10.x + v11.y + v12.x + v13;
}
